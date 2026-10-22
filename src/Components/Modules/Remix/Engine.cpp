#include "STDInclude.hpp"

#include "Engine.hpp"
#include "Core.hpp"
#include "Hook.hpp"

#include <mutex>
#include <vector>

#include "Read.hpp"

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/Dedicated.hpp"

namespace Components::Remix::Engine
{
	using SunPostEffects = void(__fastcall*)(int, int);

	static SunPostEffects originalSunPostEffects = nullptr;
	static PrePostCallback prePostCallback = nullptr;
	static bool hasAttemptedPrePost = false;
	static bool isPrePostInstalled = false;

	static std::mutex mainThreadMutex;
	static std::vector<std::function<void()>> mainThreadTasks;
	static DWORD mainThreadId = 0;
	static unsigned mainFrameNumber = 0;

	const Iw4::GameBuild& CurrentBuild()
	{
		return Iw4::multiplayer;
	}

	const DrawSurfLayout& CurrentDrawSurfLayout()
	{
		static constexpr DrawSurfLayout stock{ Iw4::drawSurfGroupMask, Iw4::drawSurfUsesGfxEntity, Iw4::drawSurfHasEntityBit, Iw4::smodelListCountShift };
		static constexpr DrawSurfLayout wide{ Iw4::drawSurfGroupMaskWide, Iw4::drawSurfUsesGfxEntityWide, Iw4::drawSurfHasEntityBitWide, Iw4::smodelListCountShiftWide };

		if (AssetHandler::IsDrawSurfKeyWide())
		{
			return wide;
		}

		return stock;
	}

	MaterialPool CurrentMaterialPool()
	{
		MaterialPool result{};
		result.pool = reinterpret_cast<std::uintptr_t>(Game::DB_XAssetPool[Game::ASSET_TYPE_MATERIAL]);
		result.count = Game::g_poolSize[Game::ASSET_TYPE_MATERIAL];
		result.stride = static_cast<std::size_t>(Game::DB_GetXAssetTypeSize(Game::ASSET_TYPE_MATERIAL));

		if (!result.pool)
		{
			result.pool = RebaseAddress(CurrentBuild().materialPool);
			result.count = Iw4::materialPoolCount;
			result.stride = Iw4::materialStride;
		}

		result.entries = result.pool + Iw4::assetPoolEntries;
		return result;
	}

	bool SetDvar(const char* name, const char* value)
	{
		if (!name || !value || !Game::Dvar_FindVar(name))
		{
			return false;
		}

		RunOnMainThread([name = std::string(name), value = std::string(value)]
		{
			Game::Dvar_SetFromStringByName(name.c_str(), value.c_str());
		});

		return true;
	}

	bool IsRendererThreaded()
	{
		const auto smpBackend = ReadAt<std::uintptr_t>(RebaseAddress(CurrentBuild().r_smp_backendDvar));
		return smpBackend && ReadAt<std::uint8_t>(smpBackend + Iw4::dvarValueOffset) != 0;
	}

	void RunOnMainThread(std::function<void()> task)
	{
		const bool isMainThread = mainThreadId != 0 && mainThreadId == GetCurrentThreadId();

		{
			const std::lock_guard lock(mainThreadMutex);

			if (!isMainThread || !mainThreadTasks.empty())
			{
				mainThreadTasks.push_back(std::move(task));
				return;
			}
		}

		task();
	}

	void RunMainThreadTasks()
	{
		mainThreadId = GetCurrentThreadId();
		++mainFrameNumber;

		std::vector<std::function<void()>> tasks;

		{
			const std::lock_guard lock(mainThreadMutex);
			tasks.swap(mainThreadTasks);
		}

		for (const std::function<void()>& task : tasks)
		{
			task();
		}
	}

	unsigned MainFrameNumber()
	{
		return mainFrameNumber;
	}

	bool EngineAddressable()
	{
		return !Dedicated::IsEnabled();
	}

	std::uintptr_t RebaseAddress(std::uintptr_t idbAddress)
	{
		if (idbAddress == 0)
		{
			return 0;
		}

		return Utils::Hook::Rebase(idbAddress);
	}

	static void __fastcall SunPostEffectsDetour(int localClient, int unused)
	{
		if (prePostCallback)
		{
			__try
			{
				prePostCallback();
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				prePostCallback = nullptr;
			}
		}

		if (Core::IsRemixMode())
		{
			return;
		}

		originalSunPostEffects(localClient, unused);
	}

	bool InstallPrePostHook(PrePostCallback callback)
	{
		if (isPrePostInstalled || hasAttemptedPrePost)
		{
			return isPrePostInstalled;
		}

		hasAttemptedPrePost = true;

		const Iw4::CallSite& call = CurrentBuild().rbDrawSunPostEffectsCall;
		const std::uintptr_t site = RebaseAddress(call.address);
		const std::uintptr_t original = Hook::ResolveCallTarget(site);

		if (!original)
		{
			LogError() << "remix: the RB_DrawSunPostEffects site at 0x" << std::hex << site << std::dec << " is not a call, refusing to patch";
			return false;
		}

		originalSunPostEffects = reinterpret_cast<SunPostEffects>(original);
		prePostCallback = callback;

		if (!Hook::PatchCall(site, call.bytes, sizeof(call.bytes), reinterpret_cast<const void*>(&SunPostEffectsDetour), nullptr))
		{
			originalSunPostEffects = nullptr;
			prePostCallback = nullptr;
			return false;
		}

		isPrePostInstalled = true;
		return true;
	}
}
