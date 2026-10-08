#include "STDInclude.hpp"

#include "ConfigStrings.hpp"
#include "Command.hpp"
#include "Flags.hpp"
#include "Gamepad.hpp"
#include "Logger.hpp"
#include "ModelCache.hpp"
#include "Weapon.hpp"

namespace Components
{
	struct ReallocatedGameState
	{
		int stringOffsets[ConfigStrings::MAX_CONFIGSTRINGS];
		char stringData[131072];
		int dataCount;
	};

	static_assert(sizeof(ReallocatedGameState::stringOffsets) == 0x6BF0);
	static_assert(offsetof(ReallocatedGameState, dataCount) == 0x26BF0);
	static_assert(sizeof(ReallocatedGameState) == 0x26BF4);

	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr std::uintptr_t CL_ParseGamestate = 0x1400FFC70;
	constexpr std::uintptr_t CL_ParseGamestate_OffsetsBase = 0x140C5CDA0;

	enum class SiteKind
	{
		OffsetsAddress,
		DataAddress,
		CountAddress,
		OffsetsFromImage,
		OffsetsFromParseBase,
		GameStateSize,
		OffsetsSize,
		ConfigStringCount,
		LastConfigString,
	};

	struct GameStateSite
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		SiteKind kind;
		std::uint8_t bytes[10];
	};

	static const GameStateSite gameStateSites[] =
	{
		{ 0x1400F3FD7, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x8D, 0x15, 0x02, 0x9B, 0xBA, 0x00 } },
		{ 0x1400F3FF3, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x8D, 0x0D, 0xE6, 0x9A, 0xBA, 0x00 } },
		{ 0x1400F47C3, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x8D, 0x0D, 0x16, 0x93, 0xBA, 0x00 } },
		{ 0x1400F4C61, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x63, 0x0D, 0x78, 0x8E, 0xBA, 0x00 } },
		{ 0x1400FC51A, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x8D, 0x0D, 0xBF, 0x15, 0xBA, 0x00 } },
		{ 0x1400FD008, 7, 3, SiteKind::OffsetsAddress, { 0x4C, 0x8D, 0x25, 0xD1, 0x0A, 0xBA, 0x00 } },
		{ 0x1400FFD4E, 7, 3, SiteKind::OffsetsAddress, { 0x48, 0x8D, 0x0D, 0x8B, 0xDD, 0xB9, 0x00 } },

		{ 0x1400F3F88, 7, 3, SiteKind::DataAddress, { 0x48, 0x8D, 0x0D, 0x1D, 0xDF, 0xBA, 0x00 } },
		{ 0x1400F4017, 7, 3, SiteKind::DataAddress, { 0x4C, 0x8D, 0x3D, 0x8E, 0xDE, 0xBA, 0x00 } },
		{ 0x1400F47CE, 7, 3, SiteKind::DataAddress, { 0x48, 0x8D, 0x0D, 0xD7, 0xD6, 0xBA, 0x00 } },
		{ 0x1400F4C68, 7, 3, SiteKind::DataAddress, { 0x48, 0x8D, 0x05, 0x3D, 0xD2, 0xBA, 0x00 } },
		{ 0x1400FFDA4, 7, 3, SiteKind::DataAddress, { 0x4C, 0x8D, 0x25, 0x01, 0x21, 0xBA, 0x00 } },
		{ 0x1400FFDFD, 7, 3, SiteKind::DataAddress, { 0x4C, 0x8D, 0x2D, 0xA8, 0x20, 0xBA, 0x00 } },
		{ 0x14010006B, 7, 3, SiteKind::DataAddress, { 0x4C, 0x8D, 0x25, 0x3A, 0x1E, 0xBA, 0x00 } },

		{ 0x1400F4011, 6, 2, SiteKind::CountAddress, { 0x89, 0x0D, 0x95, 0xDE, 0xBC, 0x00 } },
		{ 0x1400F406B, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0x3B, 0xDE, 0xBC, 0x00 } },
		{ 0x1400F407C, 7, 3, SiteKind::CountAddress, { 0x48, 0x63, 0x0D, 0x29, 0xDE, 0xBC, 0x00 } },
		{ 0x1400F4091, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0x15, 0xDE, 0xBC, 0x00 } },
		{ 0x1400F409B, 6, 2, SiteKind::CountAddress, { 0x89, 0x0D, 0x0B, 0xDE, 0xBC, 0x00 } },
		{ 0x1400FFD85, 10, 2, SiteKind::CountAddress, { 0xC7, 0x05, 0x1D, 0x21, 0xBC, 0x00, 0x01, 0x00, 0x00, 0x00 } },
		{ 0x1400FFF60, 6, 2, SiteKind::CountAddress, { 0x8B, 0x05, 0x46, 0x1F, 0xBC, 0x00 } },
		{ 0x1400FFF73, 7, 3, SiteKind::CountAddress, { 0x48, 0x63, 0x0D, 0x32, 0x1F, 0xBC, 0x00 } },
		{ 0x1400FFF85, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0x21, 0x1F, 0xBC, 0x00 } },
		{ 0x1400FFF92, 6, 2, SiteKind::CountAddress, { 0x89, 0x0D, 0x14, 0x1F, 0xBC, 0x00 } },
		{ 0x1400FFFE9, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0xBD, 0x1E, 0xBC, 0x00 } },
		{ 0x14010000C, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0x9A, 0x1E, 0xBC, 0x00 } },
		{ 0x140100026, 7, 3, SiteKind::CountAddress, { 0x48, 0x63, 0x0D, 0x7F, 0x1E, 0xBC, 0x00 } },
		{ 0x14010003B, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0x6B, 0x1E, 0xBC, 0x00 } },
		{ 0x14010004C, 6, 2, SiteKind::CountAddress, { 0x89, 0x0D, 0x5A, 0x1E, 0xBC, 0x00 } },
		{ 0x14010009B, 6, 2, SiteKind::CountAddress, { 0x8B, 0x05, 0x0B, 0x1E, 0xBC, 0x00 } },
		{ 0x1401000AE, 7, 3, SiteKind::CountAddress, { 0x48, 0x63, 0x0D, 0xF7, 0x1D, 0xBC, 0x00 } },
		{ 0x1401000C0, 6, 2, SiteKind::CountAddress, { 0x8B, 0x0D, 0xE6, 0x1D, 0xBC, 0x00 } },
		{ 0x1401000CD, 6, 2, SiteKind::CountAddress, { 0x89, 0x0D, 0xD9, 0x1D, 0xBC, 0x00 } },

		{ 0x1400F3F80, 8, 4, SiteKind::OffsetsFromImage, { 0x4A, 0x63, 0x84, 0xBB, 0xE0, 0xDA, 0xC9, 0x00 } },
		{ 0x1400F4071, 8, 4, SiteKind::OffsetsFromImage, { 0x41, 0x89, 0x8C, 0xB5, 0xE0, 0xDA, 0xC9, 0x00 } },
		{ 0x1400FD101, 8, 3, SiteKind::OffsetsFromImage, { 0x83, 0xBC, 0xB7, 0xE0, 0xDA, 0xC9, 0x00, 0x00 } },
		{ 0x1400FD144, 8, 3, SiteKind::OffsetsFromImage, { 0x83, 0xBC, 0xB7, 0xE0, 0xDA, 0xC9, 0x00, 0x00 } },

		{ 0x1400FFF69, 7, 3, SiteKind::OffsetsFromParseBase, { 0x89, 0x84, 0x8E, 0x40, 0x0D, 0x04, 0x00 } },
		{ 0x14010001C, 7, 3, SiteKind::OffsetsFromParseBase, { 0x89, 0x8C, 0x82, 0x40, 0x0D, 0x04, 0x00 } },
		{ 0x1401000A4, 7, 3, SiteKind::OffsetsFromParseBase, { 0x89, 0x84, 0x8E, 0x40, 0x0D, 0x04, 0x00 } },

		{ 0x1400F3EF5, 5, 1, SiteKind::GameStateSize, { 0xBA, 0xD0, 0x43, 0x02, 0x00 } },
		{ 0x1400F3FDE, 6, 2, SiteKind::GameStateSize, { 0x41, 0xB8, 0xD0, 0x43, 0x02, 0x00 } },
		{ 0x1400FC521, 6, 2, SiteKind::GameStateSize, { 0x41, 0xB8, 0xD0, 0x43, 0x02, 0x00 } },

		{ 0x1400F3FFA, 6, 2, SiteKind::OffsetsSize, { 0x41, 0xB8, 0xCC, 0x43, 0x00, 0x00 } },
		{ 0x1400F402F, 7, 3, SiteKind::OffsetsSize, { 0x48, 0x8D, 0xBD, 0xCC, 0x43, 0x00, 0x00 } },
		{ 0x1400FFD67, 6, 2, SiteKind::OffsetsSize, { 0x41, 0xB8, 0xCC, 0x43, 0x00, 0x00 } },

		{ 0x1400F40A4, 7, 3, SiteKind::ConfigStringCount, { 0x48, 0x81, 0xFE, 0xF3, 0x10, 0x00, 0x00 } },
		{ 0x1400FD07A, 6, 2, SiteKind::ConfigStringCount, { 0x81, 0xFE, 0xF3, 0x10, 0x00, 0x00 } },
		{ 0x1400FD198, 6, 2, SiteKind::ConfigStringCount, { 0x81, 0xFB, 0xF3, 0x10, 0x00, 0x00 } },
		{ 0x1400FFDD7, 7, 3, SiteKind::ConfigStringCount, { 0x41, 0x81, 0xFE, 0xF3, 0x10, 0x00, 0x00 } },
		{ 0x140100077, 7, 3, SiteKind::ConfigStringCount, { 0x41, 0x81, 0xFF, 0xF3, 0x10, 0x00, 0x00 } },

		{ 0x1400F3F49, 7, 3, SiteKind::LastConfigString, { 0x41, 0x81, 0xFF, 0xF2, 0x10, 0x00, 0x00 } },
		{ 0x1400FFF1A, 6, 2, SiteKind::LastConfigString, { 0x81, 0xFD, 0xF2, 0x10, 0x00, 0x00 } },
	};

	static bool TryEncodeOperand(const GameStateSite& site, const ReallocatedGameState* gameState, std::int32_t& operand)
	{
		const auto nextInstruction = static_cast<std::int64_t>(Utils::Hook::Rebase(site.address) + site.length);
		const auto offsets = reinterpret_cast<std::int64_t>(gameState->stringOffsets);
		std::int64_t encoded = 0;

		switch (site.kind)
		{
		case SiteKind::OffsetsAddress:
			encoded = offsets - nextInstruction;
			break;
		case SiteKind::DataAddress:
			encoded = reinterpret_cast<std::int64_t>(gameState->stringData) - nextInstruction;
			break;
		case SiteKind::CountAddress:
			encoded = reinterpret_cast<std::int64_t>(&gameState->dataCount) - nextInstruction;
			break;
		case SiteKind::OffsetsFromImage:
			encoded = offsets - static_cast<std::int64_t>(Utils::Hook::Rebase(imageBase));
			break;
		case SiteKind::OffsetsFromParseBase:
			encoded = offsets - static_cast<std::int64_t>(Utils::Hook::Rebase(CL_ParseGamestate_OffsetsBase));
			break;
		case SiteKind::GameStateSize:
			encoded = sizeof(ReallocatedGameState);
			break;
		case SiteKind::OffsetsSize:
			encoded = sizeof(ReallocatedGameState::stringOffsets);
			break;
		case SiteKind::ConfigStringCount:
			encoded = ConfigStrings::MAX_CONFIGSTRINGS;
			break;
		case SiteKind::LastConfigString:
			encoded = ConfigStrings::MAX_CONFIGSTRINGS - 1;
			break;
		}

		if (encoded < INT32_MIN || encoded > INT32_MAX)
		{
			return false;
		}

		operand = static_cast<std::int32_t>(encoded);
		return true;
	}

	static bool isClientTableRaised = false;

	void ConfigStrings::PatchConfigStrings()
	{
		for (const auto& site : gameStateSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, site.bytes, site.length))
			{
				Logger::Error("configstrings: 0x{:X} does not read as expected, nothing moved, so a 1.2.211 server's gamestate cannot be parsed\n", site.address);
				return;
			}
		}

		auto* const gameState = static_cast<ReallocatedGameState*>(Utils::Hook::AllocateDataNear(CL_ParseGamestate, sizeof(ReallocatedGameState)));

		if (!gameState)
		{
			Logger::Error("configstrings: no memory free within reach of the image, nothing moved, so a 1.2.211 server's gamestate cannot be parsed\n");
			return;
		}

		std::int32_t operands[std::size(gameStateSites)]{};

		for (std::size_t i = 0; i < std::size(gameStateSites); ++i)
		{
			if (!TryEncodeOperand(gameStateSites[i], gameState, operands[i]))
			{
				VirtualFree(gameState, 0, MEM_RELEASE);
				Logger::Error("configstrings: the new block is out of reach of 0x{:X}, nothing moved, so a 1.2.211 server's gamestate cannot be parsed\n", gameStateSites[i].address);
				return;
			}
		}

		for (std::size_t i = 0; i < std::size(gameStateSites); ++i)
		{
			const GameStateSite& site = gameStateSites[i];
			Utils::Hook::Set<std::int32_t>(reinterpret_cast<void*>(Utils::Hook::Rebase(site.address) + site.operandOffset), operands[i]);
		}

		isClientTableRaised = true;
	}

	constexpr int BASEGAME_MAX_CONFIGSTRINGS = 4139;
	constexpr int CS_MODELS_LAST = 0x664;
	constexpr int EXTRA_WEAPONS_LAST = BASEGAME_MAX_CONFIGSTRINGS + 1200 - 1;
	constexpr int EXTRA_MODELCACHE_FIRST = EXTRA_WEAPONS_LAST + 1;
	constexpr int EXTRA_MODELCACHE_LAST = EXTRA_MODELCACHE_FIRST + ModelCache::ADDITIONAL_GMODELS;

	static_assert(EXTRA_MODELCACHE_FIRST == 5339);
	static_assert(EXTRA_MODELCACHE_LAST == 5851);

	constexpr int RUMBLE_FIRST = EXTRA_MODELCACHE_LAST + 1;
	constexpr int RUMBLE_LAST = RUMBLE_FIRST + Gamepad::RUMBLE_CONFIGSTRINGS_COUNT - 2;

	static_assert(RUMBLE_FIRST == 5852);
	static_assert(RUMBLE_LAST == 5882);

	constexpr int DEV_DVAR_CONFIGSTRINGS_FIRST = RUMBLE_FIRST + Gamepad::RUMBLE_CONFIGSTRINGS_COUNT;

	static_assert(DEV_DVAR_CONFIGSTRINGS_FIRST == 5884);
	static_assert(DEV_DVAR_CONFIGSTRINGS_FIRST + ConfigStrings::DEV_DVAR_CONFIGSTRINGS_CAPACITY * 2 == ConfigStrings::MAX_CONFIGSTRINGS);

	constexpr std::uintptr_t modelNameCalls[] = { 0x1400E9BB4, 0x1400E9C4A, 0x1400EA1FA };

	static const std::uint8_t modelNameCallBytes[][5] =
	{
		{ 0xE8, 0x07, 0xAC, 0x00, 0x00 },
		{ 0xE8, 0x71, 0xAB, 0x00, 0x00 },
		{ 0xE8, 0xC1, 0xA5, 0x00, 0x00 },
	};

	constexpr std::uintptr_t CG_ServerCommand_ConfigStringModifiedCall = 0x1400E65D7;

	static const std::uint8_t configStringModifiedCall[] = { 0xE8, 0x54, 0xF8, 0xFF, 0xFF };

	static Utils::Hook modelNameHooks[3];
	static Utils::Hook configStringModifiedHook;

	static int ModelConfigStringIndex(int index)
	{
		if (index > CS_MODELS_LAST)
		{
			return EXTRA_MODELCACHE_FIRST + (index - (CS_MODELS_LAST + 1));
		}

		return index;
	}

	const char* ConfigStrings::CL_GetCachedModelConfigString(int index)
	{
		index = ModelConfigStringIndex(index);

		if (index > EXTRA_MODELCACHE_LAST)
		{
			return "";
		}

		return Game::CL_GetConfigString(index);
	}

	constexpr int CS_WEAPONFILES_EXTRA = 2939;
	constexpr int X64_LAST_CONFIGSTRING = 4338;

	void ConfigStrings::CG_ConfigStringModified_Hook(int localClientNum)
	{
		const Command::ClientParams params;

		if (params.Size() > 1 && Weapon::IsLimitRaised())
		{
			const int index = std::atoi(params.Get(1));

			if (index > X64_LAST_CONFIGSTRING && index <= EXTRA_WEAPONS_LAST)
			{
				Game::CG_SetupWeaponDef(localClientNum, static_cast<unsigned int>(index - CS_WEAPONFILES_EXTRA));
				return;
			}
		}

		if (params.Size() > 1 && ModelCache::gameModelsReallocated)
		{
			const int index = std::atoi(params.Get(1));

			if (index >= EXTRA_MODELCACHE_FIRST && index <= EXTRA_MODELCACHE_LAST)
			{
				const int model = index - EXTRA_MODELCACHE_FIRST + ModelCache::BASE_GMODEL_COUNT;
				ModelCache::gameModelsReallocated[model] = Game::R_RegisterModel(Game::CL_GetConfigString(index));
				return;
			}
		}

		reinterpret_cast<void(*)(int)>(configStringModifiedHook.GetOriginal())(localClientNum);
	}

	void ConfigStrings::PatchModelConfigStrings()
	{
		for (std::size_t i = 0; i < std::size(modelNameCalls); ++i)
		{
			if (!Utils::Hook::MatchesBytes(modelNameCalls[i], modelNameCallBytes[i], sizeof(modelNameCallBytes[i])))
			{
				Logger::Error("configstrings: a model name call does not read as expected, left alone, so models past 511 get the wrong names\n");
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(CG_ServerCommand_ConfigStringModifiedCall, configStringModifiedCall, sizeof(configStringModifiedCall)))
		{
			Logger::Error("configstrings: the 'd' call does not read as expected, left alone, so models past 511 get the wrong names\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(modelNameCalls); ++i)
		{
			isSeated = modelNameHooks[i].Initialize(modelNameCalls[i], reinterpret_cast<void*>(CL_GetCachedModelConfigString), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		isSeated = configStringModifiedHook.Initialize(CG_ServerCommand_ConfigStringModifiedCall, reinterpret_cast<void*>(CG_ConfigStringModified_Hook), HOOK_CALL)
			->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : modelNameHooks)
			{
				hook.Uninstall();
			}

			configStringModifiedHook.Uninstall();

			Logger::Error("configstrings: could not seat the model string hooks, so models past 511 get the wrong names\n");
			return;
		}

		for (auto& hook : modelNameHooks)
		{
			hook.Quick();
		}

		configStringModifiedHook.Quick();
	}

	enum class ServerSiteKind
	{
		TableAddress,
		TableEnd,
		TableFromServer,
		Count,
		LastIndex,
	};

	struct ServerSite
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		ServerSiteKind kind;
		std::int32_t expected;
	};

	constexpr std::uintptr_t sv_configstrings = 0x1464FEF22;
	constexpr std::uintptr_t svState = 0x1464FDF00;
	constexpr int BASEGAME_SERVER_CONFIGSTRINGS = 4339;

	static const ServerSite serverSites[] =
	{
		{ 0x140238E52, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x140238EFE, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023A13A, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023A1C0, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023A1F3, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023ACE4, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023B53E, 7, 3, ServerSiteKind::TableAddress, 0 },
		{ 0x14023B6CA, 7, 3, ServerSiteKind::TableEnd, 0 },
		{ 0x14023AABD, 7, 3, ServerSiteKind::TableFromServer, 0x1022 },
		{ 0x14023AB1A, 7, 3, ServerSiteKind::TableFromServer, 0x1022 },
		{ 0x14023AB6D, 7, 3, ServerSiteKind::TableFromServer, 0x1022 },
		{ 0x14023ABCA, 7, 3, ServerSiteKind::TableFromServer, 0x1022 },
		{ 0x140238ED9, 7, 3, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x140239037, 6, 2, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x14023A141, 5, 1, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x14023B548, 5, 1, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x1401A1E30, 6, 2, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x1401A2AE0, 6, 2, ServerSiteKind::Count, BASEGAME_SERVER_CONFIGSTRINGS },
		{ 0x14023AAF6, 7, 3, ServerSiteKind::LastIndex, BASEGAME_SERVER_CONFIGSTRINGS - 1 },
		{ 0x14023ACC8, 6, 2, ServerSiteKind::LastIndex, BASEGAME_SERVER_CONFIGSTRINGS - 1 },
	};

	constexpr std::uintptr_t Com_Memset = 0x140280620;
	constexpr std::uintptr_t SV_ClearServer_MemsetCalls[] = { 0x14023A186, 0x14023B5A0 };

	static Utils::Hook serverClearHooks[std::size(SV_ClearServer_MemsetCalls)];
	static std::uint16_t* serverConfigStrings = nullptr;

	static std::int64_t EncodeServerSite(const ServerSite& site, std::uintptr_t table, std::uintptr_t siteAddress, std::uintptr_t server, int count)
	{
		switch (site.kind)
		{
		case ServerSiteKind::TableAddress:
			return static_cast<std::int64_t>(table) - static_cast<std::int64_t>(siteAddress + site.length);
		case ServerSiteKind::TableEnd:
			return static_cast<std::int64_t>(table + count * sizeof(std::uint16_t)) - static_cast<std::int64_t>(siteAddress + site.length);
		case ServerSiteKind::TableFromServer:
			return static_cast<std::int64_t>(table) - static_cast<std::int64_t>(server);
		case ServerSiteKind::Count:
			return count;
		case ServerSiteKind::LastIndex:
			return count - 1;
		}

		return 0;
	}

	void* ConfigStrings::SV_ClearServer_Memset_Hook(void* dest, int value, std::size_t size)
	{
		void* const result = reinterpret_cast<void*(*)(void*, int, std::size_t)>(Utils::Hook::Rebase(Com_Memset))(dest, value, size);
		std::memset(serverConfigStrings, value, MAX_CONFIGSTRINGS * sizeof(std::uint16_t));
		return result;
	}

	void ConfigStrings::PatchServerConfigStrings()
	{
		for (const ServerSite& site : serverSites)
		{
			const std::int64_t expected = EncodeServerSite(site, sv_configstrings, site.address, svState, BASEGAME_SERVER_CONFIGSTRINGS);

			if (Utils::Hook::Get<std::int32_t>(site.address + site.operandOffset) != expected)
			{
				Logger::Error("configstrings: 0x{:X} does not read as expected, the server keeps 4339 configstrings\n", site.address);
				return;
			}
		}

		for (const std::uintptr_t call : SV_ClearServer_MemsetCalls)
		{
			if (!Utils::Hook::BranchesTo(call, Com_Memset, HOOK_CALL))
			{
				Logger::Error("configstrings: SV_ClearServer does not clear sv as expected, the server keeps 4339 configstrings\n");
				return;
			}
		}

		auto* const table = static_cast<std::uint16_t*>(Utils::Hook::AllocateDataNear(SV_ClearServer_MemsetCalls[0], MAX_CONFIGSTRINGS * sizeof(std::uint16_t)));

		if (!table)
		{
			Logger::Error("configstrings: no memory free within reach of the image, the server keeps 4339 configstrings\n");
			return;
		}

		const auto tableAddress = reinterpret_cast<std::uintptr_t>(table);
		const std::uintptr_t liveServer = Utils::Hook::Rebase(svState);
		std::int32_t operands[std::size(serverSites)]{};

		for (std::size_t i = 0; i < std::size(serverSites); ++i)
		{
			const std::int64_t encoded = EncodeServerSite(serverSites[i], tableAddress, Utils::Hook::Rebase(serverSites[i].address), liveServer, MAX_CONFIGSTRINGS);

			if (encoded < INT32_MIN || encoded > INT32_MAX)
			{
				VirtualFree(table, 0, MEM_RELEASE);
				Logger::Error("configstrings: the server table is out of reach of 0x{:X}, the server keeps 4339 configstrings\n", serverSites[i].address);
				return;
			}

			operands[i] = static_cast<std::int32_t>(encoded);
		}

		serverConfigStrings = table;

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(SV_ClearServer_MemsetCalls); ++i)
		{
			isSeated = serverClearHooks[i].Initialize(SV_ClearServer_MemsetCalls[i], reinterpret_cast<void*>(SV_ClearServer_Memset_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (Utils::Hook& hook : serverClearHooks)
			{
				hook.Uninstall();
			}

			serverConfigStrings = nullptr;
			VirtualFree(table, 0, MEM_RELEASE);
			Logger::Error("configstrings: could not hook SV_ClearServer's memsets, the server keeps 4339 configstrings\n");
			return;
		}

		std::memcpy(table, reinterpret_cast<const void*>(Utils::Hook::Rebase(sv_configstrings)), BASEGAME_SERVER_CONFIGSTRINGS * sizeof(std::uint16_t));

		for (Utils::Hook& hook : serverClearHooks)
		{
			hook.Quick();
		}

		for (std::size_t i = 0; i < std::size(serverSites); ++i)
		{
			Utils::Hook::Set<std::int32_t>(serverSites[i].address + serverSites[i].operandOffset, operands[i]);
		}
	}

	constexpr std::uintptr_t SV_GetConfigstringConst = 0x14023A1F0;
	constexpr std::uintptr_t SV_SetConfigstring = 0x14023ACB0;
	constexpr std::uintptr_t G_ModelIndex_SetConfigstringCall = 0x1401AC0BA;

	struct ModelStringRead
	{
		std::uintptr_t address;
		bool isJump;
	};

	static const ModelStringRead modelStringReads[] =
	{
		{ 0x1401AC056, false },
		{ 0x1401AC0E6, true },
		{ 0x1401AC0FF, false },
		{ 0x1401AB0F2, false },
		{ 0x1401AAFF2, false },
		{ 0x14019B4CB, false },
		{ 0x14019B708, false },
	};

	static Utils::Hook modelStringReadHooks[std::size(modelStringReads)];
	static Utils::Hook modelStringWriteHook;
	static bool hasServerModelStrings = false;

	unsigned int ConfigStrings::SV_GetCachedModelConfigStringConst(int index)
	{
		return Game::SV_GetConfigstringConst(ModelConfigStringIndex(index));
	}

	void ConfigStrings::SV_SetCachedModelConfigString(int index, const char* data)
	{
		Game::SV_SetConfigstring(ModelConfigStringIndex(index), data);
	}

	void ConfigStrings::PatchServerModelConfigStrings()
	{
		if (!serverConfigStrings)
		{
			return;
		}

		bool isExpected = Utils::Hook::BranchesTo(G_ModelIndex_SetConfigstringCall, SV_SetConfigstring, HOOK_CALL);

		for (const ModelStringRead& site : modelStringReads)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(site.address, SV_GetConfigstringConst, site.isJump);
		}

		if (!isExpected)
		{
			Logger::Error("configstrings: a server model string call does not read as expected, left alone, so the server keeps 512 models\n");
			return;
		}

		bool isSeated = modelStringWriteHook.Initialize(G_ModelIndex_SetConfigstringCall, reinterpret_cast<void*>(SV_SetCachedModelConfigString), HOOK_CALL)
			->Install()->IsInstalled();

		for (std::size_t i = 0; i < std::size(modelStringReads); ++i)
		{
			isSeated = modelStringReadHooks[i].Initialize(modelStringReads[i].address, reinterpret_cast<void*>(SV_GetCachedModelConfigStringConst), modelStringReads[i].isJump)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			modelStringWriteHook.Uninstall();

			for (Utils::Hook& hook : modelStringReadHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("configstrings: could not seat the server model string hooks, so the server keeps 512 models\n");
			return;
		}

		modelStringWriteHook.Quick();

		for (Utils::Hook& hook : modelStringReadHooks)
		{
			hook.Quick();
		}

		hasServerModelStrings = true;
	}

	constexpr std::uintptr_t SV_SetConfig = 0x1401FCC80;
	constexpr std::uintptr_t SV_SetConfigCalls[] = { 0x14023A9E3, 0x14023BA24, 0x14023D043 };
	constexpr int BASE_DVAR_CONFIGSTRINGS_FIRST = 24;
	constexpr int BASE_DVAR_CONFIGSTRINGS_CAPACITY = 200;

	static Utils::Hook setConfigHooks[std::size(SV_SetConfigCalls)];

	void ConfigStrings::SV_SetConfig_Hook(int start, int max, const int bit)
	{
		if (start == BASE_DVAR_CONFIGSTRINGS_FIRST && max == BASE_DVAR_CONFIGSTRINGS_CAPACITY)
		{
			start = DEV_DVAR_CONFIGSTRINGS_FIRST;
			max = DEV_DVAR_CONFIGSTRINGS_CAPACITY;
		}

		reinterpret_cast<void(*)(int, int, int)>(Utils::Hook::Rebase(SV_SetConfig))(start, max, bit);
	}

	void ConfigStrings::PatchDevDvarConfigStrings()
	{
		if (!Flags::HasFlag("dev"))
		{
			return;
		}

		if (!HasRaisedTables())
		{
			Logger::Error("configstrings: the tables were not raised, so -dev dvars stay at 24\n");
			return;
		}

		for (const std::uintptr_t site : SV_SetConfigCalls)
		{
			if (!Utils::Hook::BranchesTo(site, SV_SetConfig, HOOK_CALL))
			{
				Logger::Error("configstrings: 0x{:X} is not a call to SV_SetConfig, so -dev dvars stay at 24\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(SV_SetConfigCalls); ++i)
		{
			isSeated = setConfigHooks[i].Initialize(SV_SetConfigCalls[i], reinterpret_cast<void*>(SV_SetConfig_Hook), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (Utils::Hook& hook : setConfigHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("configstrings: could not seat the SV_SetConfig hooks, so -dev dvars stay at 24\n");
			return;
		}

		for (Utils::Hook& hook : setConfigHooks)
		{
			hook.Quick();
		}
	}

	bool ConfigStrings::HasRaisedTables()
	{
		return isClientTableRaised && serverConfigStrings != nullptr;
	}

	bool ConfigStrings::HasServerModelStrings()
	{
		return hasServerModelStrings;
	}

	const char* ConfigStrings::CL_GetRumbleConfigString(int index)
	{
		if (index < 0 || index > RUMBLE_LAST - RUMBLE_FIRST)
		{
			return "";
		}

		return Game::CL_GetConfigString(RUMBLE_FIRST + index);
	}

	unsigned int ConfigStrings::SV_GetRumbleConfigStringConst(int index)
	{
		return Game::SV_GetConfigstringConst(RUMBLE_FIRST + index);
	}

	void ConfigStrings::SV_SetRumbleConfigString(int index, const char* data)
	{
		Game::SV_SetConfigstring(RUMBLE_FIRST + index, data);
	}

	ConfigStrings::ConfigStrings()
	{
		PatchConfigStrings();
		PatchModelConfigStrings();
		PatchServerConfigStrings();
		PatchServerModelConfigStrings();
		PatchDevDvarConfigStrings();
	}
}
