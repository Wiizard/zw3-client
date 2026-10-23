#include "STDInclude.hpp"

#include "Remix.hpp"
#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "FixedFunction.hpp"
#include "Menu.hpp"
#include "PbrMaterials.hpp"
#include "Scene.hpp"
#include "TextureTags.hpp"

#include "Components/Modules/Command.hpp"
#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Events.hpp"
#include "Components/Modules/Flags.hpp"
#include "Components/Modules/Logger.hpp"
#include "Components/Modules/Menus.hpp"
#include "Components/Modules/Renderer.hpp"
#include "Components/Modules/Scheduler.hpp"
#include "Components/Modules/UIScript.hpp"
#include "Components/Modules/Window.hpp"

namespace Components::Remix
{
	Dvar::Var Remix::r_rtxRemix;

	using VkFunction = void(*)();
	using VkGetProcAddr = VkFunction(*)(void* owner, const char* name);
	using VkQueuePresent = std::int32_t(*)(void* queue, const void* presentInfo);

	static VkGetProcAddr originalGetInstanceProcAddr = nullptr;
	static VkGetProcAddr originalGetDeviceProcAddr = nullptr;
	static std::atomic<VkQueuePresent> originalQueuePresent = nullptr;
	static std::atomic<std::uint64_t> presentCount = 0;

	static std::int32_t QueuePresentDetour(void* queue, const void* presentInfo)
	{
		presentCount.fetch_add(1, std::memory_order_relaxed);
		return originalQueuePresent.load(std::memory_order_relaxed)(queue, presentInfo);
	}

	static VkFunction WrapPresent(VkFunction found)
	{
		if (!found)
		{
			return nullptr;
		}

		originalQueuePresent.store(reinterpret_cast<VkQueuePresent>(found), std::memory_order_relaxed);
		return reinterpret_cast<VkFunction>(&QueuePresentDetour);
	}

	static VkFunction GetDeviceProcAddrDetour(void* device, const char* name)
	{
		const VkFunction found = originalGetDeviceProcAddr(device, name);
		return name && std::strcmp(name, "vkQueuePresentKHR") == 0 ? WrapPresent(found) : found;
	}

	static VkFunction GetInstanceProcAddrDetour(void* instance, const char* name)
	{
		const VkFunction found = originalGetInstanceProcAddr(instance, name);

		if (!name || !found)
		{
			return found;
		}

		if (std::strcmp(name, "vkGetDeviceProcAddr") == 0)
		{
			originalGetDeviceProcAddr = reinterpret_cast<VkGetProcAddr>(found);
			return reinterpret_cast<VkFunction>(&GetDeviceProcAddrDetour);
		}

		return std::strcmp(name, "vkQueuePresentKHR") == 0 ? WrapPresent(found) : found;
	}

	static bool TryHookPresentCount(HMODULE runtime)
	{
		const auto base = reinterpret_cast<std::uintptr_t>(runtime);
		const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

		if (directory.VirtualAddress == 0)
		{
			return false;
		}

		for (auto* imports = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); imports->Name; ++imports)
		{
			if (_stricmp(reinterpret_cast<const char*>(base + imports->Name), "vulkan-1.dll") != 0 || !imports->OriginalFirstThunk)
			{
				continue;
			}

			const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
			auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);

			for (; names->u1.AddressOfData; ++names, ++slots)
			{
				if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
				{
					continue;
				}

				const auto* const byName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);

				if (std::strcmp(byName->Name, "vkGetInstanceProcAddr") != 0)
				{
					continue;
				}

				DWORD protection = 0;

				if (!VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &protection))
				{
					return false;
				}

				originalGetInstanceProcAddr = reinterpret_cast<VkGetProcAddr>(slots->u1.Function);
				slots->u1.Function = reinterpret_cast<ULONGLONG>(&GetInstanceProcAddrDetour);
				VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), protection, &protection);
				return true;
			}
		}

		return false;
	}

	int Remix::GetPresentedFps()
	{
		static std::uint64_t lastCount = 0;
		static ULONGLONG lastTime = 0;
		static int presentedFps = -1;

		if (!originalQueuePresent.load(std::memory_order_relaxed))
		{
			return -1;
		}

		const ULONGLONG now = GetTickCount64();
		const std::uint64_t count = presentCount.load(std::memory_order_relaxed);

		if (lastTime == 0)
		{
			lastTime = now;
			lastCount = count;
		}
		else if (now - lastTime >= 500)
		{
			presentedFps = static_cast<int>(std::lrint(static_cast<double>(count - lastCount) * 1000.0 / static_cast<double>(now - lastTime)));
			lastTime = now;
			lastCount = count;
		}

		return presentedFps;
	}

	bool Remix::IsActive()
	{
		return Core::IsRemixMode();
	}

	bool Remix::IsEnabled()
	{
		return r_rtxRemix.IsValid() && r_rtxRemix.Get<bool>();
	}

	void Remix::FailBackToStock(const std::string& reason)
	{
		Logger::Error("remix: {}, starting without RTX Remix and turning r_rtxRemix off\n", reason);

		if (r_rtxRemix.IsValid())
		{
			r_rtxRemix.Set(false);
		}
	}

	IDirect3D9* Remix::TryCreateDirect3D(const UINT sdkVersion)
	{
		static const bool shouldUseRemix = r_rtxRemix.IsValid() && r_rtxRemix.Get<bool>();

		if (!shouldUseRemix)
		{
			return nullptr;
		}

		if (HMODULE loaded = Core::ResolveForwardTarget())
		{
			const auto create = reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(loaded, "Direct3DCreate9"));
			return Device::WrapDirect3D(create(sdkVersion), OnDeviceCreated, OnDeviceReset);
		}

		static bool hasAttemptedLoad = false;

		if (hasAttemptedLoad)
		{
			return nullptr;
		}

		hasAttemptedLoad = true;

		Core::LoadConfig();

		EnsurePbrMod();
		SanitizeUserConf();

		const std::string& configured = Core::GetConfig().remixDll;
		const std::wstring path = Core::ModuleDirectory() + std::wstring(configured.begin(), configured.end());

		Logger::Print("remix: r_rtxRemix is on, loading the RTX Remix runtime from {}\n", configured);

		HMODULE runtime = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

		if (!runtime)
		{
			FailBackToStock(std::format("could not load {} (error {})", configured, GetLastError()));
			return nullptr;
		}

		const auto create = reinterpret_cast<Direct3DCreate9_t>(GetProcAddress(runtime, "Direct3DCreate9"));

		if (create && !TryHookPresentCount(runtime))
		{
			Logger::Warning("remix: could not find the runtime's vulkan import, cg_drawFPS shows no RTX FPS\n");
		}

		if (!create)
		{
			FreeLibrary(runtime);
			FailBackToStock(std::format("{} exports no Direct3DCreate9, it is not the Remix runtime", configured));
			return nullptr;
		}

		IDirect3D9* const direct3D = create(sdkVersion);

		if (!direct3D)
		{
			FreeLibrary(runtime);
			FailBackToStock(std::format("the Remix runtime would not create a d3d9 interface for sdk version {}", sdkVersion));
			return nullptr;
		}

		Core::SetRuntime(runtime);

		return Device::WrapDirect3D(direct3D, OnDeviceCreated, OnDeviceReset);
	}

	void Remix::BeginUiDrawing(IDirect3DDevice9* device)
	{
		if (Core::IsRemixMode())
		{
			Device::NoteFrameEvent("UI marker from zw3's own drawing (lobby scene)");
			MarkUiPhase(device);
		}
	}

	void Remix::OnDeviceCreated([[maybe_unused]] IDirect3DDevice9* device)
	{
		InstallHooks();
		Engine::InstallPrePostHook(&OnPostEffectsBegin);
	}

	void Remix::OnDeviceReset([[maybe_unused]] IDirect3DDevice9* device)
	{
		Menu::OnDeviceLost();
		OnDeviceLost();
	}

	static std::wstring QuoteRestartArgument(std::wstring_view argument)
	{
		std::wstring result = L"\"";
		std::size_t backslashes = 0;
		for (const wchar_t character : argument)
		{
			if (character == L'\\')
			{
				++backslashes;
				continue;
			}
			result.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
			result += character;
			backslashes = 0;
		}
		result.append(backslashes * 2, L'\\');
		result += L'"';
		return result;
	}

	void Remix::SetEnabledAndRestart(bool enabled)
	{
		static bool restarting = false;
		if (restarting || !r_rtxRemix.IsValid() || r_rtxRemix.Get<bool>() == enabled)
		{
			return;
		}

		std::wstring executable(32768, L'\0');
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
		if (!length || length >= executable.size())
		{
			Logger::Error("remix: could not resolve the executable for restart\n");
			return;
		}
		executable.resize(length);

		int count = 0;
		auto* const arguments = CommandLineToArgvW(Flags::GetOriginalCommandLine().c_str(), &count);
		if (!arguments)
		{
			Logger::Error("remix: could not read the launch arguments for restart\n");
			return;
		}

		std::wstring commandLine = QuoteRestartArgument(executable);
		for (int i = 1; i < count; ++i)
		{
			if (std::wstring_view(arguments[i]).starts_with(Flags::RestartParentFlag))
			{
				continue;
			}
			// Replace earlier startup overrides instead of accumulating conflicting values on every restart.
			if (i + 2 < count && (!_wcsicmp(arguments[i], L"+set") || !_wcsicmp(arguments[i], L"+seta"))
				&& !_wcsicmp(arguments[i + 1], L"r_rtxRemix"))
			{
				i += 2;
				continue;
			}
			commandLine += L" " + QuoteRestartArgument(arguments[i]);
		}
		LocalFree(arguments);
		commandLine += std::format(L" {}{} +set r_rtxRemix {}", Flags::RestartParentFlag, GetCurrentProcessId(), enabled ? 1 : 0);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo))
		{
			Logger::Error("remix: could not restart the game, error {}\n", GetLastError());
			MessageBoxW(Window::GetWindow(), L"The game could not be restarted. RTX Remix has not been changed.", L"RTX Remix", MB_OK | MB_ICONERROR);
			return;
		}
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		// The new process waits for this one to exit before loading its configuration and renderer.
		restarting = true;
		r_rtxRemix.Set(enabled);
		Command::Execute("writeconfig " CLIENT_CONFIG);
		Command::Execute("quit", false);
	}

	Remix::Remix()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Menus::Add("ui_mp/popup_rtx_remix.menu");
		UIScript::Add("SetRtxRemixAndRestart", [](const UIScript::Token& token)
		{
			if (token.IsValid() && (token.Get<int>() == 0 || token.Get<int>() == 1))
			{
				SetEnabledAndRestart(token.Get<int>() != 0);
			}
		});

		Scheduler::Loop([]
		{
			if (Core::IsRemixMode())
			{
				Engine::RunMainThreadTasks();
			}
		}, Scheduler::Pipeline::MAIN);

		Command::Add("dumpframe", []
		{
			if (Core::IsRemixMode())
			{
				RequestSceneDump();
			}

			WriteMapMaterialReport();
		});

		Events::OnDvarInit([]
		{
			r_rtxRemix = Dvar::Register("r_rtxRemix", false, Game::DVAR_ARCHIVE, "Path trace the game with NVIDIA RTX Remix (applies on the next launch)");
		});

		Renderer::OnBackendFrame([](IDirect3DDevice9* device)
		{
			if (Core::IsRemixMode())
			{
				OnPresent(device);
				Menu::Render(device, Window::GetWindow());
			}
		});

		Renderer::OnDeviceRecoveryBegin([]
		{
			if (Core::IsRemixMode())
			{
				Menu::OnDeviceLost();
				OnDeviceLost();
			}
		});
	}
}
