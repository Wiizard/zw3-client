#include "STDInclude.hpp"

#include "Remix.hpp"
#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Menu.hpp"
#include "PbrMaterials.hpp"
#include "Scene.hpp"
#include "TextureTags.hpp"

#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Events.hpp"
#include "Components/Modules/Logger.hpp"
#include "Components/Modules/Renderer.hpp"
#include "Components/Modules/Window.hpp"

namespace Components::Remix
{
	Dvar::Var Remix::r_rtxRemix;

	// remix presents on vulkan, and with DLSS frame generation it presents frames the game never drew, so the frames on
	// screen are counted at vkQueuePresentKHR: the runtime imports vkGetInstanceProcAddr, and every present function
	// it looks up goes through the wrapped lookups below
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
		// decided once per launch, so toggling r_rtxRemix and then vid_restart never swaps runtimes mid-session
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

	// zw3 code that draws with its own shaders outside the game's 3D scene calls this first, so Remix rasterises
	// those draws instead of taking them for world geometry on frames where no scene ran
	void Remix::BeginUiDrawing(IDirect3DDevice9* device)
	{
		if (Core::IsRemixMode())
		{
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

	Remix::Remix()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

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
