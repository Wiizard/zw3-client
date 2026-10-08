#include "STDInclude.hpp"

#include "Renderer.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "RawMouse.hpp"
#include "Scheduler.hpp"

extern "C"
{
	void BackendFrameStub();

	std::uintptr_t Renderer_SwapChainIndex = 0;

	void Renderer_BackendFrameHandler()
	{
		Components::Renderer::BackendFrameHandler();
	}
}

namespace Components
{
	std::vector<std::function<Renderer::BackendCallback>> Renderer::backendFrameSignal;
	std::vector<std::function<Renderer::BackendCallback>> Renderer::singleBackendFrameSignal;

	std::vector<std::function<Renderer::Callback>> Renderer::endRecoverDeviceSignal;
	std::vector<std::function<Renderer::Callback>> Renderer::beginRecoverDeviceSignal;

	constexpr std::uintptr_t vidConfig_displaySize = 0x148CCC908;

	constexpr std::uintptr_t RB_EndFrame_SwapBuffers = 0x14004FD20;
	constexpr std::uintptr_t swapChainIndex = 0x148CCA144;
	static const std::uint8_t swapChainIndexLoad[] = { 0x48, 0x63, 0x05, 0x1D, 0xA4, 0xC7, 0x08 };

	constexpr std::uintptr_t R_RecoverLostDevice_BeginCall = 0x1400336DB;
	constexpr std::uintptr_t R_RecoverLostDevice_EndCall = 0x1400339B7;
	constexpr std::uintptr_t DB_BeginRecoverLostDevice = 0x14012CF30;
	constexpr std::uintptr_t DB_EndRecoverLostDevice = 0x14012D1B0;

	constexpr std::uintptr_t CL_Vid_Restart_f_ShutdownCall = 0x1400FDCCC;
	constexpr std::uintptr_t CL_Vid_Restart_f_InitRendererCall = 0x1400FDE77;
	constexpr std::uintptr_t R_Shutdown = 0x140032C10;
	constexpr std::uintptr_t CL_InitRenderer = 0x1400FBD60;

	constexpr std::uintptr_t gfxDrawMethod_baseTechType = 0x14913B404;

	constexpr std::uintptr_t gfxCmdBufSourceState = 0x148F0DA20;
	constexpr std::size_t codeImages = 0x12C0;
	constexpr std::size_t codeImageSamplerStates = 0x1398;
	constexpr std::size_t codeImageCount = 27;
	constexpr std::size_t imageName = 0x20;

	struct BudgetSite
	{
		std::uintptr_t address;
		std::uint8_t immediateOffset;
		std::uint32_t stock;
	};

	constexpr std::uint32_t budgetScale = 4;

	static const BudgetSite budgetSites[] =
	{
		{ 0x140038D2A, 3, 0x480000 },
		{ 0x140038D46, 1, 0x480000 },
		{ 0x140038D78, 1, 0x480000 },
		{ 0x140038DA7, 1, 0x480000 },
		{ 0x140065331, 1, 0x480000 },
		{ 0x140065471, 1, 0x480000 },
		{ 0x140038DCF, 6, 0x100000 },
		{ 0x140038ECC, 3, 0x100000 },
		{ 0x140038E07, 1, 0x200000 },
		{ 0x140038E90, 1, 0x200000 },
		{ 0x140038EE4, 1, 0x200000 },
		{ 0x140038F83, 1, 0x200000 },
	};

	static bool RaiseRenderBudgets()
	{
		for (const BudgetSite& site : budgetSites)
		{
			if (Utils::Hook::Get<std::uint32_t>(site.address + site.immediateOffset) != site.stock)
			{
				return false;
			}
		}

		for (const BudgetSite& site : budgetSites)
		{
			Utils::Hook::Set<std::uint32_t>(site.address + site.immediateOffset, site.stock * budgetScale);
		}

		return true;
	}

	Dvar::Var Renderer::r_forceTechnique;
	Dvar::Var Renderer::r_listSamplers;

	static Utils::Hook hooks[5];

	void Renderer::BackendFrameHandler()
	{
		IDirect3DDevice9* device = *Game::dx_device;

		if (device)
		{
			device->AddRef();

			for (const auto& callback : backendFrameSignal)
			{
				callback(device);
			}

			const auto copy = std::move(singleBackendFrameSignal);
			singleBackendFrameSignal.clear();

			for (const auto& callback : copy)
			{
				callback(device);
			}

			device->Release();
		}
	}

	void Renderer::OnNextBackendFrame(const std::function<BackendCallback>& callback)
	{
		singleBackendFrameSignal.push_back(callback);
	}

	void Renderer::OnBackendFrame(const std::function<BackendCallback>& callback)
	{
		backendFrameSignal.push_back(callback);
	}

	void Renderer::OnDeviceRecoveryEnd(const std::function<Callback>& callback)
	{
		endRecoverDeviceSignal.push_back(callback);
	}

	void Renderer::OnDeviceRecoveryBegin(const std::function<Callback>& callback)
	{
		beginRecoverDeviceSignal.push_back(callback);
	}

	int Renderer::Width()
	{
		return reinterpret_cast<LPPOINT>(Utils::Hook::Rebase(vidConfig_displaySize))->x;
	}

	int Renderer::Height()
	{
		return reinterpret_cast<LPPOINT>(Utils::Hook::Rebase(vidConfig_displaySize))->y;
	}

	void Renderer::ForceTechnique()
	{
		const auto forceTechnique = r_forceTechnique.Get<int>();

		if (forceTechnique > 0)
		{
			*reinterpret_cast<int*>(Utils::Hook::Rebase(gfxDrawMethod_baseTechType)) = forceTechnique;
		}
	}

	void Renderer::ListSamplers()
	{
		if (!r_listSamplers.Get<bool>())
		{
			return;
		}

		const auto source = Utils::Hook::Rebase(gfxCmdBufSourceState);

		auto* font = Game::R_RegisterFont("fonts/smallFont", 0);
		const auto height = Game::R_TextHeight(font);
		const auto scale = 1.0f;
		float color[] = { 0.0f, 1.0f, 0.0f, 1.0f };

		for (std::size_t i = 0; i < codeImageCount; ++i)
		{
			const auto* image = *reinterpret_cast<const std::uint8_t* const*>(source + codeImages + i * sizeof(void*));
			const auto samplerState = *reinterpret_cast<const std::uint8_t*>(source + codeImageSamplerStates + i);

			const char* name = "---";

			if (image == nullptr)
			{
				color[0] = 1.f;
			}
			else
			{
				color[0] = 0.f;
				name = *reinterpret_cast<const char* const*>(image + imageName);
			}

			const auto* str = Utils::String::Format("{}/{:#X} => {} {}", i, i, name, std::to_string(samplerState));

			Game::R_AddCmdDrawText(str, std::numeric_limits<int>::max(), font, 15.0f, (height * scale + 1) * (i + 1) + 14.0f, scale, scale, 0.0f, color, 0);
		}
	}

	static std::atomic_bool isDeviceRecoveryActive = false;
	static std::atomic_bool isDeviceRecoveryComplete = true;

	bool Renderer::IsDeviceRecoveryActive()
	{
		return isDeviceRecoveryActive.load(std::memory_order_acquire);
	}

	void Renderer::FinishLoading()
	{
		if (!isDeviceRecoveryComplete.load(std::memory_order_acquire))
		{
			return;
		}

		isDeviceRecoveryActive.store(false, std::memory_order_release);
	}

	void Renderer::PreVidRestart()
	{
		isDeviceRecoveryComplete.store(false, std::memory_order_release);
		isDeviceRecoveryActive.store(true, std::memory_order_release);
		RawMouse::SuspendMouseInput();

		for (const auto& callback : beginRecoverDeviceSignal)
		{
			callback();
		}
	}

	void Renderer::PostVidRestart()
	{
		isDeviceRecoveryComplete.store(true, std::memory_order_release);
		isDeviceRecoveryActive.store(false, std::memory_order_release);

		for (const auto& callback : endRecoverDeviceSignal)
		{
			callback();
		}
	}

	void Renderer::DB_BeginRecoverLostDevice_Hk()
	{
		isDeviceRecoveryComplete.store(false, std::memory_order_release);
		isDeviceRecoveryActive.store(true, std::memory_order_release);
		RawMouse::SuspendMouseInput();

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(DB_BeginRecoverLostDevice))();

		for (const auto& callback : beginRecoverDeviceSignal)
		{
			callback();
		}
	}

	void Renderer::DB_EndRecoverLostDevice_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(DB_EndRecoverLostDevice))();

		isDeviceRecoveryComplete.store(true, std::memory_order_release);
		isDeviceRecoveryActive.store(false, std::memory_order_release);

		for (const auto& callback : endRecoverDeviceSignal)
		{
			callback();
		}
	}

	void Renderer::R_Shutdown_Hk(int destroyWindow)
	{
		PreVidRestart();
		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(R_Shutdown))(destroyWindow);
	}

	void Renderer::CL_InitRenderer_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_InitRenderer))();
		PostVidRestart();
	}

	Renderer::Renderer()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		if (!RaiseRenderBudgets())
		{
			Logger::Error("renderer: the dynamic buffers do not read as expected, the skinned cache and index buffers stay stock\n");
		}

		const bool isExpected = Utils::Hook::MatchesBytes(RB_EndFrame_SwapBuffers, swapChainIndexLoad, sizeof(swapChainIndexLoad))
			&& Utils::Hook::BranchesTo(R_RecoverLostDevice_BeginCall, DB_BeginRecoverLostDevice, false)
			&& Utils::Hook::BranchesTo(R_RecoverLostDevice_EndCall, DB_EndRecoverLostDevice, false)
			&& Utils::Hook::BranchesTo(CL_Vid_Restart_f_ShutdownCall, R_Shutdown, false)
			&& Utils::Hook::BranchesTo(CL_Vid_Restart_f_InitRendererCall, CL_InitRenderer, false);

		if (!isExpected)
		{
			Logger::Error("renderer: the renderer does not read as expected, no renderer hooks\n");
			return;
		}

		Renderer_SwapChainIndex = Utils::Hook::Rebase(swapChainIndex);

		bool isSeated = hooks[0].Initialize(RB_EndFrame_SwapBuffers, BackendFrameStub, HOOK_CALL)->Install()->IsInstalled();
		isSeated = hooks[1].Initialize(R_RecoverLostDevice_BeginCall, reinterpret_cast<void*>(DB_BeginRecoverLostDevice_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[2].Initialize(R_RecoverLostDevice_EndCall, reinterpret_cast<void*>(DB_EndRecoverLostDevice_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[3].Initialize(CL_Vid_Restart_f_ShutdownCall, reinterpret_cast<void*>(R_Shutdown_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[4].Initialize(CL_Vid_Restart_f_InitRendererCall, reinterpret_cast<void*>(CL_InitRenderer_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("renderer: could not seat every renderer hook\n");
			return;
		}

		Utils::Hook::Nop(RB_EndFrame_SwapBuffers + 5, sizeof(swapChainIndexLoad) - 5);

		Scheduler::Loop([]
		{
			if (Game::CL_IsCgameInitialized(0))
			{
				ForceTechnique();
				ListSamplers();
			}
		}, Scheduler::Pipeline::RENDERER);

		Events::OnDvarInit([]
		{
			r_forceTechnique = Game::Dvar_RegisterInt("r_forceTechnique", 0, 0, 14, Game::DVAR_NONE, "Force a base technique on the renderer");
			r_listSamplers = Game::Dvar_RegisterBool("r_listSamplers", false, Game::DVAR_NONE, "List samplers & sampler states");
		});
	}
}
