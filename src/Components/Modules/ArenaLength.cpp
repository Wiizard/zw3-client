#include "STDInclude.hpp"

#include "ArenaLength.hpp"
#include "Logger.hpp"

namespace Components
{
	extern "C"
	{
		void MapNameCopyStub();
		void MapNameCompareStub();
		void FeederMapNameStub();
		void MapCommandStub();

		std::uintptr_t ArenaLength_I_strncpyz = 0;
		std::uintptr_t ArenaLength_I_stricmp = 0;
		std::uintptr_t ArenaLength_Dvar_SetStringByName = 0;
		std::uintptr_t ArenaLength_va = 0;
	}

	Game::newMapArena_t* ArenaLength::newArenas = nullptr;
	char** ArenaLength::newArenaInfos = nullptr;

	constexpr std::uintptr_t ui_arenaInfos = 0x1465C0740;
	constexpr std::uintptr_t UI_LoadArenas_MaxCount = 0x14026B512;
	static const std::uint8_t maxCount[] = { 0xBB, 0x40, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t sharedUiInfo = 0x1465D0A40;
	constexpr std::uintptr_t mapList = 0x1465D2854;

	constexpr std::uintptr_t UI_UpdateArenas = 0x14026B800;

	static constexpr std::array<std::uint8_t, 3> leaR8 = { 0x4C, 0x8D, 0x05 };
	static constexpr std::array<std::uint8_t, 3> leaRcx = { 0x48, 0x8D, 0x0D };
	static constexpr std::array<std::uint8_t, 3> leaRdx = { 0x48, 0x8D, 0x15 };
	static constexpr std::array<std::uint8_t, 3> leaRbx = { 0x48, 0x8D, 0x1D };
	static constexpr std::array<std::uint8_t, 3> leaRbp = { 0x48, 0x8D, 0x2D };
	static constexpr std::array<std::uint8_t, 3> leaRsi = { 0x48, 0x8D, 0x35 };
	static constexpr std::array<std::uint8_t, 3> leaRdi = { 0x48, 0x8D, 0x3D };
	static constexpr std::array<std::uint8_t, 3> leaR15 = { 0x4C, 0x8D, 0x3D };

	struct ListLea
	{
		Utils::Hook::LeaSite lea;
		std::size_t newOffset;
	};

	static const ListLea listLeas[] =
	{
		{ { 0x14026B469, leaRcx, mapList }, 0 },
		{ { 0x14026B726, leaR15, mapList }, 0 },
		{ { 0x14026B84C, leaRbx, mapList }, 0 },
		{ { 0x14026BB76, leaRbx, mapList }, 0 },
		{ { 0x14026EC9E, leaRsi, mapList }, 0 },
		{ { 0x14026F266, leaRbp, mapList }, 0 },
		{ { 0x14026B3F2, leaRbp, mapList + 0x20 }, offsetof(Game::newMapArena_t, mapName) },
		{ { 0x14026B864, leaRdi, mapList + 0x20 }, offsetof(Game::newMapArena_t, mapName) },
		{ { 0x14026BB6B, leaRdi, mapList + 0x20 }, offsetof(Game::newMapArena_t, mapName) },
		{ { 0x14026B8FF, leaRdx, mapList + 0x30 }, offsetof(Game::newMapArena_t, description) },
		{ { 0x14026B934, leaRdx, mapList + 0x50 }, offsetof(Game::newMapArena_t, mapimage) },
		{ { 0x14026EB3A, leaRdi, mapList + 0xA70 }, offsetof(Game::newMapArena_t, other) },
		{ { 0x14026FC3B, leaRcx, mapList + 0xAFC }, offsetof(Game::newMapArena_t, other) + 0x8C },
	};

	static const Utils::Hook::LeaSite infoLeas[] =
	{
		{ 0x14026B4FB, leaR8, ui_arenaInfos },
		{ 0x14026B858, leaRsi, ui_arenaInfos },
	};

	constexpr std::uintptr_t strideSites[] =
	{
		0x14026B406, 0x14026B470, 0x14026B733, 0x14026B768, 0x14026B8A9, 0x14026B8D7,
		0x14026B906, 0x14026B93B, 0x14026B97E, 0x14026B994, 0x14026B9FD, 0x14026BA41,
		0x14026BA65, 0x14026EB92, 0x14026EC21, 0x14026ECAF, 0x14026F276, 0x14026FC58,
	};

	constexpr std::uint32_t oldStride = 0xB00;
	constexpr std::size_t strideImmOffset = 3;

	struct SharedUiDisp
	{
		std::uintptr_t address;
		std::size_t dispOffset;
		std::int32_t oldDisp;
		std::size_t newOffset;
	};

	static const SharedUiDisp sharedUiDisps[] =
	{
		{ 0x14026B985, 4, 0x288C, offsetof(Game::newMapArena_t, other) + 8 },
		{ 0x14026BA04, 4, 0x288C, offsetof(Game::newMapArena_t, other) + 8 },
		{ 0x14026BA0F, 4, 0x288C, offsetof(Game::newMapArena_t, other) + 8 },
		{ 0x14026BA48, 4, 0x288C, offsetof(Game::newMapArena_t, other) + 8 },
		{ 0x14026BA5B, 3, 0x1E14, 0 },
		{ 0x14026EC17, 3, 0x1E34, offsetof(Game::newMapArena_t, mapName) },
		{ 0x14026EC5E, 3, 0x1E64, offsetof(Game::newMapArena_t, mapimage) },
	};

	constexpr std::uintptr_t UI_GetCurrentMapCustom_MapName = 0x14026EB48;
	constexpr std::int32_t oldNameFromOther = -0xA50;
	constexpr std::int32_t newNameFromOther = static_cast<std::int32_t>(offsetof(Game::newMapArena_t, mapName) - offsetof(Game::newMapArena_t, other));

	struct CallStub
	{
		std::uintptr_t address;
		std::uintptr_t target;
		void(*stub)();
	};

	constexpr std::uintptr_t I_strncpyz = 0x14028C390;
	constexpr std::uintptr_t I_stricmp = 0x14028C0F0;
	constexpr std::uintptr_t Dvar_SetStringByName = 0x140287A70;
	constexpr std::uintptr_t va = 0x14028D1E0;

	static const CallStub callStubs[] =
	{
		{ 0x14026B8B6, I_strncpyz, MapNameCopyStub },
		{ 0x14026ECC1, I_stricmp, MapNameCompareStub },
		{ 0x14026F284, I_stricmp, MapNameCompareStub },
		{ 0x14026DAFD, Dvar_SetStringByName, FeederMapNameStub },
		{ 0x140271E88, va, MapCommandStub },
	};

	static Utils::Hook hooks[std::size(callStubs)];

	static bool FitsRel32(std::int64_t value)
	{
		return value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max();
	}

	static std::int64_t SharedUiDistance(std::size_t newOffset)
	{
		const auto target = reinterpret_cast<std::uintptr_t>(ArenaLength::newArenas) + newOffset;
		return static_cast<std::int64_t>(target) - static_cast<std::int64_t>(Utils::Hook::Rebase(sharedUiInfo));
	}

	ArenaLength::ArenaLength()
	{
		bool isExpected = Utils::Hook::MatchesBytes(UI_LoadArenas_MaxCount, maxCount, sizeof(maxCount))
			&& Utils::Hook::Get<std::int32_t>(UI_GetCurrentMapCustom_MapName + 3) == oldNameFromOther;

		for (const auto& site : listLeas)
		{
			isExpected = isExpected && Utils::Hook::IsLeaIntact(site.lea);
		}

		for (const auto& lea : infoLeas)
		{
			isExpected = isExpected && Utils::Hook::IsLeaIntact(lea);
		}

		for (const auto site : strideSites)
		{
			isExpected = isExpected && Utils::Hook::Get<std::uint32_t>(site + strideImmOffset) == oldStride;
		}

		for (const auto& site : sharedUiDisps)
		{
			isExpected = isExpected && Utils::Hook::Get<std::int32_t>(site.address + site.dispOffset) == site.oldDisp;
		}

		for (const auto& site : callStubs)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(site.address, site.target, false);
		}

		if (!isExpected)
		{
			Logger::Error("arenalength: the arena code does not read as expected, 64 arenas and 16 character map names\n");
			return;
		}

		newArenas = static_cast<Game::newMapArena_t*>(Utils::Hook::AllocateDataNear(UI_UpdateArenas, sizeof(Game::newMapArena_t) * newArenaCount));
		newArenaInfos = static_cast<char**>(Utils::Hook::AllocateDataNear(UI_UpdateArenas, sizeof(char*) * newArenaCount));

		bool isReachable = newArenas && newArenaInfos;

		for (const auto& site : listLeas)
		{
			isReachable = isReachable && Utils::Hook::CanLeaReach(site.lea, reinterpret_cast<const char*>(newArenas) + site.newOffset);
		}

		for (const auto& lea : infoLeas)
		{
			isReachable = isReachable && Utils::Hook::CanLeaReach(lea, newArenaInfos);
		}

		for (const auto& site : sharedUiDisps)
		{
			isReachable = isReachable && FitsRel32(SharedUiDistance(site.newOffset));
		}

		if (!isReachable)
		{
			newArenas = nullptr;
			newArenaInfos = nullptr;

			Logger::Error("arenalength: no memory within reach of the arena code, 64 arenas and 16 character map names\n");
			return;
		}

		ArenaLength_I_strncpyz = Utils::Hook::Rebase(I_strncpyz);
		ArenaLength_I_stricmp = Utils::Hook::Rebase(I_stricmp);
		ArenaLength_Dvar_SetStringByName = Utils::Hook::Rebase(Dvar_SetStringByName);
		ArenaLength_va = Utils::Hook::Rebase(va);

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(callStubs); ++i)
		{
			isSeated = hooks[i].Initialize(callStubs[i].address, callStubs[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			newArenas = nullptr;
			newArenaInfos = nullptr;

			Logger::Error("arenalength: could not seat every hook, 64 arenas and 16 character map names\n");
			return;
		}

		for (const auto& lea : infoLeas)
		{
			Utils::Hook::PointLeaAt(lea, newArenaInfos);
		}

		Utils::Hook::Set<std::uint32_t>(UI_LoadArenas_MaxCount + 1, newArenaCount);

		for (const auto& site : listLeas)
		{
			Utils::Hook::PointLeaAt(site.lea, reinterpret_cast<const char*>(newArenas) + site.newOffset);
		}

		for (const auto& site : sharedUiDisps)
		{
			Utils::Hook::Set<std::int32_t>(site.address + site.dispOffset, static_cast<std::int32_t>(SharedUiDistance(site.newOffset)));
		}

		Utils::Hook::Set<std::int32_t>(UI_GetCurrentMapCustom_MapName + 3, newNameFromOther);

		for (const auto site : strideSites)
		{
			Utils::Hook::Set<std::uint32_t>(site + strideImmOffset, sizeof(Game::newMapArena_t));
		}
	}
}
