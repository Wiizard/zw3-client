#include "STDInclude.hpp"

#include "Rumble.hpp"
#include "Command.hpp"
#include "ConfigStrings.hpp"
#include "Events.hpp"
#include "Gamepad.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "RawFiles.hpp"
#include "Scheduler.hpp"

#include "GSC/Script.hpp"

#include "Controller/Engine/Rumble.hpp"

namespace Components
{
	Dvar::Var Rumble::cl_debug_rumbles;
	Dvar::Var Rumble::cl_rumbleScale;

	extern "C"
	{
		void MeleeRumbleStub();
		void PlayNoteMappedSoundAliasesStub();

		std::uintptr_t Rumble_MeleeTargetIsClient = 0;
		std::uintptr_t Rumble_MeleeTargetIsNotClient = 0;
		std::uintptr_t Rumble_NoteSoundMapTest = 0;

		void Rumble_MeleeRumble(Game::gentity_s* targetEntity, const Game::WeaponDef* weaponDef)
		{
			Rumble::MeleeRumble_Hook(targetEntity, weaponDef);
		}

		void Rumble_PlayNoteMappedRumbleAliases(int localClientNum, const char* noteName, const Game::WeaponDef* weapDef)
		{
			Rumble::PlayNoteMappedRumbleAliases(localClientNum, noteName, weapDef);
		}
	}

	static Game::RumbleGlobals rumbleGlobArray[Game::MAX_GPAD_COUNT]{};

	static const char* const rumbleStrings[] =
	{
		"riotshield_impact",
		"damage_heavy",
		"defaultweapon_fire",
		"pistol_fire",
		"defaultweapon_melee",
		"viewmodel_small",
		"viewmodel_medium",
		"viewmodel_large",
		"silencer_fire",
		"smg_fire",
		"assault_fire",
		"shotgun_fire",
		"heavygun_fire",
		"sniper_fire",
		"artillery_rumble",
		"grenade_rumble",
		"ac130_25mm_fire",
		"ac130_40mm_fire",
		"ac130_105mm_fire",
		"minigun_rumble",
	};

	static_assert(std::size(rumbleStrings) < Gamepad::RUMBLE_CONFIGSTRINGS_COUNT - 1);

	static const Game::cspField_t rumbleFields[] =
	{
		{ "duration", offsetof(Game::RumbleInfo, duration), 7 },
		{ "range", offsetof(Game::RumbleInfo, range), 7 },
		{ "fadeWithDistance", offsetof(Game::RumbleInfo, fadeWithDistance), 5 },
		{ "broadcast", offsetof(Game::RumbleInfo, broadcast), 5 },
	};

	constexpr int MAX_RUMBLE_GRAPHS = 64;
	constexpr int MAX_RUMBLE_GRAPH_KNOTS = 16;

	constexpr int infoStringBufferSize = 0x2000;

	constexpr std::uintptr_t Weapon_Melee_TargetClientTest = 0x1401894DE;
	constexpr std::uintptr_t Weapon_Melee_TargetIsClient = 0x1401894E8;
	constexpr std::uintptr_t Weapon_Melee_TargetIsNotClient = 0x1401894FC;
	static const std::uint8_t targetClientTest[] = { 0x48, 0x83, 0xBB, 0x58, 0x01, 0x00, 0x00, 0x00, 0x74, 0x14 };

	constexpr std::uintptr_t CG_UpdateViewWeaponAnim_NoteName = 0x1400C7BDC;
	constexpr std::uintptr_t CG_UpdateViewWeaponAnim_SoundMapTest = 0x1400C7BE8;
	static const std::uint8_t noteNameLoad[] = { 0x48, 0x8B, 0x84, 0x24, 0xC0, 0x00, 0x00, 0x00, 0x49, 0x8B, 0x0C, 0x06 };

	constexpr std::uintptr_t CG_DrawActiveFrame_R_EndDObjSceneCall = 0x1400B8CD5;
	constexpr std::uintptr_t R_EndDObjScene = 0x14001F760;

	constexpr std::uintptr_t CG_DrawActiveFrame_CG_AddPacketEntitiesCall = 0x1400B849D;
	constexpr std::uintptr_t CG_AddPacketEntities = 0x1400D27B0;

	constexpr std::uintptr_t SV_SpawnServer_SV_InitGameProgsCall = 0x14023B7E7;
	constexpr std::uintptr_t SV_InitGameProgs = 0x140233500;

	constexpr std::uintptr_t CG_FireWeapon_FireSoundCall = 0x1400C35B0;
	constexpr std::uintptr_t CG_FireWeapon_FireSound = 0x1400C3680;

	constexpr std::uintptr_t CG_BulletHitClientShieldEvent_GetImpactEffectCall = 0x1400C15CF;
	constexpr std::uintptr_t CG_GetImpactEffectForWeapon = 0x1400C3900;

	constexpr std::uintptr_t CG_EntityEvent_ExplosiveImpactOnShieldCall = 0x1400ADECB;
	constexpr std::uintptr_t CG_ExplosiveImpactOnShieldEvent = 0x1400C3140;
	constexpr std::uintptr_t CG_EntityEvent_ExplosiveSplashOnShieldCall = 0x1400ADEE2;
	constexpr std::uintptr_t CG_ExplosiveSplashOnShieldEvent = 0x1400C3180;

	constexpr std::uintptr_t CG_ProcessEntity_UpdateBarrelSpinSoundCall = 0x1400D5385;
	constexpr std::uintptr_t CG_Turret_UpdateBarrelSpinSound = 0x14024FEA0;

	constexpr std::uintptr_t Com_Frame_SCR_UpdateRumbleCall = 0x1401F47F0;
	constexpr std::uintptr_t SCR_UpdateRumble_Folded = 0x140080E50;

	constexpr std::uintptr_t G_RegisterWeapon_BG_GetWeaponDefCall = 0x14016CD49;
	constexpr std::uintptr_t BG_GetWeaponDef = 0x14009C8A0;

	constexpr std::uintptr_t CG_Init_CG_RegisterGraphicsCall = 0x1400D6B9C;
	constexpr std::uintptr_t CG_RegisterGraphics = 0x1400D9830;

	static bool HasRumbleFile(const char* rumbleName)
	{
		const std::string path = std::format("rumble/{}", rumbleName);

		Game::fileHandle_t file = 0;

		if (Game::FS_FOpenFileByMode(path.data(), &file, Game::FS_READ) >= 0)
		{
			Game::FS_FCloseFile(file);
			return true;
		}

		Game::DB_FindXAssetHeader(Game::ASSET_TYPE_RAWFILE, path.data());
		return !Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, path.data());
	}

	static Utils::Hook meleeHook;
	static Utils::Hook noteNameHook;
	static Utils::Hook endDObjSceneHook;
	static Utils::Hook addPacketEntitiesHook;
	static Utils::Hook initGameProgsHook;
	static Utils::Hook fireSoundHook;
	static Utils::Hook impactEffectHook;
	static Utils::Hook impactOnShieldHook;
	static Utils::Hook splashOnShieldHook;
	static Utils::Hook barrelSpinSoundHook;
	static Utils::Hook updateRumbleHook;
	static Utils::Hook registerWeaponHook;
	static Utils::Hook registerGraphicsHook;

	int Rumble::GetRumbleInfoIndexFromName(const char* rumbleName)
	{
		for (int i = 0; i < Gamepad::RUMBLE_CONFIGSTRINGS_COUNT - 1; ++i)
		{
			const char* configString = ConfigStrings::CL_GetRumbleConfigString(i);

			if (*configString && std::strcmp(configString, rumbleName) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	Game::ActiveRumble* Rumble::GetDuplicateRumbleIfExists(Game::ActiveRumble* arArray, const Game::RumbleInfo* info, bool loop, Game::RumbleSourceType type, int entityNum, const float* pos)
	{
		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			Game::ActiveRumble* duplicateRumble = &arArray[i];

			if (duplicateRumble->rumbleInfo != info || duplicateRumble->loop != loop || duplicateRumble->sourceType != type)
			{
				continue;
			}

			bool isSame = false;

			if (type == Game::RUMBLESOURCE_ENTITY)
			{
				isSame = duplicateRumble->source.entityNum == entityNum;
			}
			else
			{
				if (type != Game::RUMBLESOURCE_POS)
				{
					return duplicateRumble;
				}

				if (duplicateRumble->source.pos[0] != pos[0] || duplicateRumble->source.pos[1] != pos[1])
				{
					continue;
				}

				isSame = duplicateRumble->source.pos[2] == pos[2];
			}

			if (isSame)
			{
				return duplicateRumble;
			}
		}

		return nullptr;
	}

	int Rumble::FindClosestToDyingActiveRumble(const Game::cg_s* cgameGlob, const Game::ActiveRumble* activeRumbleArray)
	{
		float oldestRumbleAge = 0.0f;
		int oldestRumbleIndex = 0;

		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			const Game::ActiveRumble* ar = &activeRumbleArray[i];
			const bool isOwnRumble = ar->sourceType == Game::RUMBLESOURCE_ENTITY && ar->source.entityNum == cgameGlob->predictedPlayerState.clientNum;

			if (!ar->rumbleInfo || isOwnRumble)
			{
				continue;
			}

			const float timeLived01 = static_cast<float>(cgameGlob->time - ar->startTime) / ar->rumbleInfo->duration;

			if (timeLived01 > oldestRumbleAge)
			{
				oldestRumbleIndex = static_cast<int>(i);
				oldestRumbleAge = timeLived01;
			}
		}

		if (oldestRumbleAge == 0.0f)
		{
			Logger::Warning("FindClosestToDyingActiveRumble(): Couldn't find a suitable rumble to stop, defaulting to index zero.\n");
		}

		return oldestRumbleIndex;
	}

	Game::ActiveRumble* Rumble::NextAvailableRumble(const Game::cg_s* cgameGlob, Game::ActiveRumble* arArray)
	{
		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			Game::ActiveRumble* candidate = &arArray[i];

			if (!candidate->rumbleInfo)
			{
				return candidate;
			}

			if (candidate->sourceType == Game::RUMBLESOURCE_INVALID)
			{
				return candidate;
			}

			if (candidate->startTime + candidate->rumbleInfo->duration < cgameGlob->time)
			{
				return candidate;
			}
		}

		return &arArray[FindClosestToDyingActiveRumble(cgameGlob, arArray)];
	}

	static bool IsValidLocalClient(int localClientNum)
	{
		return localClientNum >= 0 && localClientNum < Game::MAX_GPAD_COUNT;
	}

	void Rumble::InvalidateActiveRumble(Game::ActiveRumble* ar)
	{
		if (ar->rumbleInfo != nullptr)
		{
			Gamepad::StopHapticEffect(static_cast<std::uint32_t>(ar->rumbleInfo->rumbleNameIndex + 1));
		}

		ar->sourceType = Game::RUMBLESOURCE_INVALID;
		ar->rumbleInfo = nullptr;
		ar->startTime = -1;
	}

	void Rumble::CalcActiveRumbles(int localClientNum, Game::ActiveRumble* activeRumbleArray, const float* rumbleReceiverPos)
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);

		float finalRumbleHigh = -1.0f;
		float finalRumbleLow = -1.0f;
		bool hasAnyRumble = false;

		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			const Game::ActiveRumble* activeRumble = &activeRumbleArray[i];
			const Game::RumbleInfo* rumbleInfo = activeRumble->rumbleInfo;

			if (!rumbleInfo)
			{
				continue;
			}

			float scale = 1.0f;

			if (rumbleInfo->broadcast)
			{
				if (activeRumble->sourceType == Game::RUMBLESOURCE_ENTITY && activeRumble->source.entityNum != cg->predictedPlayerState.clientNum)
				{
					continue;
				}
			}
			else
			{
				float distance = 0.0f;

				if (activeRumble->sourceType == Game::RUMBLESOURCE_ENTITY)
				{
					const auto* entity = Game::CG_GetEntity(localClientNum, activeRumble->source.entityNum);
					const auto* receiver = Game::CG_GetEntity(localClientNum, rumbleGlobArray[localClientNum].receiverEntNum);

					const float x = receiver->pose.origin[0] - entity->pose.origin[0];
					const float y = receiver->pose.origin[1] - entity->pose.origin[1];
					const float z = receiver->pose.origin[2] - entity->pose.origin[2];

					distance = std::sqrtf((x * x) + (y * y) + (z * z));
				}
				else
				{
					const float x = rumbleReceiverPos[0] - activeRumble->source.pos[0];
					const float y = rumbleReceiverPos[1] - activeRumble->source.pos[1];
					const float z = rumbleReceiverPos[2] - activeRumble->source.pos[2];

					distance = std::sqrtf((x * x) + (y * y) + (z * z));
				}

				if (distance > rumbleInfo->range)
				{
					continue;
				}

				if (rumbleInfo->fadeWithDistance)
				{
					scale = 1.0f - distance / rumbleInfo->range;
				}
			}

			scale *= activeRumble->scale / static_cast<float>(std::numeric_limits<std::uint8_t>::max());

			const float duration01 = static_cast<float>(cg->time - activeRumble->startTime) / rumbleInfo->duration;

			const Game::RumbleGraph* highGraph = rumbleInfo->highRumbleGraph;
			const float highValue = Game::GraphGetValueFromFraction(highGraph->knotCount, highGraph->knots, duration01);

			const Game::RumbleGraph* lowGraph = rumbleInfo->lowRumbleGraph;
			const float lowValue = Game::GraphGetValueFromFraction(lowGraph->knotCount, lowGraph->knots, duration01);

			finalRumbleHigh = std::max(finalRumbleHigh, highValue * scale);
			finalRumbleLow = std::max(finalRumbleLow, lowValue * scale);

			hasAnyRumble = true;
		}

		if (hasAnyRumble)
		{
			Gamepad::GPad_SetHighRumble(localClientNum, finalRumbleHigh);
			Gamepad::GPad_SetLowRumble(localClientNum, finalRumbleLow);
		}
		else
		{
			Gamepad::GPad_SetHighRumble(localClientNum, 0.0);
			Gamepad::GPad_SetLowRumble(localClientNum, 0.0);
		}
	}

	void Rumble::PlayRumbleInternal(int localClientNum, const char* rumbleName, bool loop, Game::RumbleSourceType type, int entityNum, const float* pos, double scale, bool updateDuplicates)
	{
		const auto logError = [](const std::string& message)
		{
			if ((*Game::com_sv_running)->current.enabled)
			{
				Logger::Fatal("{}", message);
			}
			else
			{
				Logger::Warning("{}", message);
			}
		};

		if (!IsValidLocalClient(localClientNum))
		{
			return;
		}

		const int rumbleIndex = GetRumbleInfoIndexFromName(rumbleName);

		if (rumbleIndex < 0)
		{
			logError(std::format("Could not play rumble {} because it was not registered!\n", rumbleName));
			return;
		}

		Game::RumbleInfo* rumbleInfo = &rumbleGlobArray[localClientNum].infos[rumbleIndex];

		if (rumbleInfo->rumbleNameIndex <= 0)
		{
			logError(std::format("Could not play rumble {} because it was not registered and loaded. Make sure to precache rumble before playing from script!\n", rumbleName));
			return;
		}

		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);
		auto* activeRumbles = rumbleGlobArray[localClientNum].activeRumbles;

		Game::ActiveRumble* activeRumble = GetDuplicateRumbleIfExists(activeRumbles, rumbleInfo, loop, type, entityNum, pos);
		const bool isDuplicate = activeRumble != nullptr;

		if (!isDuplicate)
		{
			activeRumble = NextAvailableRumble(cg, activeRumbles);
		}

		if (!isDuplicate || updateDuplicates)
		{
			if (type == Game::RUMBLESOURCE_ENTITY)
			{
				const auto* entity = Game::CG_GetEntity(localClientNum, entityNum);

				if (!rumbleInfo->broadcast)
				{
					if ((entity->nextValid & 1) == 0)
					{
						return;
					}

					if (entity->nextState.eType != Game::ET_PLAYER)
					{
						logError(std::format("Non-player entity #{} of type {} at ({}, {}, {}) is trying to play non-broadcasting rumble \"{}\" on themselves.\n",
							entityNum,
							entity->nextState.eType,
							entity->prevState.pos.trBase[0],
							entity->prevState.pos.trBase[1],
							entity->prevState.pos.trBase[2],
							rumbleName));
						return;
					}
				}

				activeRumble->source.entityNum = entityNum;
			}
			else if (type == Game::RUMBLESOURCE_POS)
			{
				std::memcpy(activeRumble->source.pos, pos, sizeof(activeRumble->source.pos));
			}
		}

		if (scale < 0.0 || scale > 1.0)
		{
			Logger::Warning("Rumble \"{}\" has invalid scale value of {}.\n", rumbleName, scale);
			scale = 1.0;
		}

		activeRumble->sourceType = type;
		activeRumble->startTime = cg->time;
		activeRumble->rumbleInfo = rumbleInfo;
		activeRumble->loop = loop;
		activeRumble->scale = static_cast<std::uint8_t>(scale * 255.0);

		if (!loop || !isDuplicate)
		{
			Controller::Haptic::Effect effect;

			if (Controller::Engine::TryEffectFromRumble(*rumbleInfo, static_cast<float>(scale), loop, effect))
			{
				Gamepad::PlayHapticEffect(effect);
			}
		}

		const bool isOwnLivingView = cg->predictedPlayerState.clientNum == cg->clientNum && cg->predictedPlayerState.pm_type != Game::PM_SPECTATOR;

		if (!cg->nextSnap || isOwnLivingView)
		{
			CalcActiveRumbles(localClientNum, activeRumbles, rumbleGlobArray[localClientNum].receiverPos);
		}
	}

	void Rumble::CG_PlayRumbleOnEntity(int localClientNum, const char* rumbleName, int entityIndex)
	{
		PlayRumbleInternal(localClientNum, rumbleName, false, Game::RUMBLESOURCE_ENTITY, entityIndex, nullptr, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::CG_PlayRumbleOnPosition(int localClientNum, const char* rumbleName, const float* pos)
	{
		PlayRumbleInternal(localClientNum, rumbleName, false, Game::RUMBLESOURCE_POS, 0, pos, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::CG_PlayRumbleLoopOnEntity(int localClientNum, const char* rumbleName, int entityIndex)
	{
		PlayRumbleInternal(localClientNum, rumbleName, true, Game::RUMBLESOURCE_ENTITY, entityIndex, nullptr, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::CG_PlayRumbleLoopOnPosition(int localClientNum, const char* rumbleName, const float* pos)
	{
		PlayRumbleInternal(localClientNum, rumbleName, true, Game::RUMBLESOURCE_POS, 0, pos, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::CG_PlayRumbleOnClient(int localClientNum, const char* rumbleName)
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);

		if (!cg->nextSnap)
		{
			return;
		}

		PlayRumbleInternal(localClientNum, rumbleName, false, Game::RUMBLESOURCE_ENTITY, cg->predictedPlayerState.clientNum, nullptr, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::CG_PlayRumbleOnClientSafe(int localClientNum, const char* rumbleName)
	{
		if (GetRumbleInfoIndexFromName(rumbleName) < 0)
		{
			Logger::Warning("Can't play rumble asset '{}' because it is not registered.\n", rumbleName);
			return;
		}

		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);
		PlayRumbleInternal(localClientNum, rumbleName, false, Game::RUMBLESOURCE_ENTITY, cg->predictedPlayerState.clientNum, nullptr, cl_rumbleScale.Get<float>(), false);
	}

	void Rumble::Rumble_Strcpy(void* member, const char* keyValue)
	{
		std::strcpy(static_cast<char*>(member), keyValue);
	}

	bool Rumble::ParseRumbleGraph(Game::RumbleGraph* graph, const char* buffer, const char* fileName)
	{
		const char* cursor = buffer;

		Game::Com_BeginParseSession(fileName);
		const int parsedKnotCount = std::atoi(Game::Com_Parse(&cursor));

		if (parsedKnotCount > MAX_RUMBLE_GRAPH_KNOTS)
		{
			Game::Com_EndParseSession();
			Logger::Fatal("Too many graph nots on {}", fileName);
		}

		if (parsedKnotCount < 0)
		{
			Game::Com_EndParseSession();
			Logger::Fatal("Negative graph nots on {}", fileName);
		}

		graph->knotCount = static_cast<unsigned short>(parsedKnotCount);

		for (int i = 0; i < graph->knotCount; ++i)
		{
			const char* parsedTime = Game::Com_Parse(&cursor);

			if (!*parsedTime || *parsedTime == '}')
			{
				break;
			}

			const float knotTime = static_cast<float>(std::atof(parsedTime));
			const char* parsedValue = Game::Com_Parse(&cursor);

			if (!*parsedValue || *parsedValue == '}')
			{
				break;
			}

			graph->knots[i][0] = knotTime;
			graph->knots[i][1] = static_cast<float>(std::atof(parsedValue));
		}

		Game::Com_EndParseSession();
		return true;
	}

	void Rumble::ReadRumbleGraph(Game::RumbleGraph* graph, const char* rumbleFileName)
	{
		char buffer[infoStringBufferSize]{};
		const std::string path = std::format("rumble/{}", rumbleFileName);

		strncpy_s(graph->graphName, rumbleFileName, _TRUNCATE);
		const char* infoString = RawFiles::Com_LoadInfoString_Hk(path.data(), "rumble graph file", "RUMBLEGRAPHFILE", buffer);

		graph->knotCount = 0;

		if (!ParseRumbleGraph(graph, infoString, rumbleFileName))
		{
			Logger::Fatal("Error in parsing rumble file {}", rumbleFileName);
		}
	}

	int Rumble::LoadRumbleGraph(Game::RumbleGraph* rumbleGraphArray, Game::RumbleInfo* info, const char* highRumbleFileName, const char* lowRumbleFileName)
	{
		info->highRumbleGraph = nullptr;
		info->lowRumbleGraph = nullptr;

		int i = 0;

		for (i = 0; i < MAX_RUMBLE_GRAPHS; ++i)
		{
			Game::RumbleGraph* rumbleGraph = &rumbleGraphArray[i];

			if (!rumbleGraph->knotCount)
			{
				break;
			}

			if (!_stricmp(rumbleGraph->graphName, highRumbleFileName))
			{
				info->highRumbleGraph = rumbleGraph;
			}

			if (!_stricmp(rumbleGraph->graphName, lowRumbleFileName))
			{
				info->lowRumbleGraph = rumbleGraph;
			}
		}

		while (!info->highRumbleGraph || !info->lowRumbleGraph)
		{
			if (i == MAX_RUMBLE_GRAPHS)
			{
				Logger::Fatal("No more room to allocate rumble graph");
			}

			Game::RumbleGraph* rumbleGraph = &rumbleGraphArray[i];

			if (!info->highRumbleGraph)
			{
				ReadRumbleGraph(rumbleGraph, highRumbleFileName);
				info->highRumbleGraph = rumbleGraph;
			}
			else
			{
				ReadRumbleGraph(rumbleGraph, lowRumbleFileName);
				info->lowRumbleGraph = rumbleGraph;
			}

			++i;
		}

		return 1;
	}

	int Rumble::CG_LoadRumble(Game::RumbleGraph* rumbleGraphArray, Game::RumbleInfo* info, const char* rumbleName, int rumbleNameIndex)
	{
		char buffer[infoStringBufferSize]{};
		const std::string path = std::format("rumble/{}", rumbleName);

		const char* infoString = RawFiles::Com_LoadInfoString_Hk(path.data(), "rumble info file", "RUMBLE", buffer);

		const std::string highRumbleFile = Game::Info_ValueForKey(infoString, "highRumbleFile");
		const std::string lowRumbleFile = Game::Info_ValueForKey(infoString, "lowRumbleFile");

		if (!Game::ParseConfigStringToStructCustomSize(info, rumbleFields, static_cast<int>(std::size(rumbleFields)), infoString, 0, nullptr, Rumble_Strcpy))
		{
			return 0;
		}

		if (info->broadcast && info->range == 0.0f)
		{
			Logger::Fatal("Rumble file {} cannot have broadcast because its range is zero\n", rumbleName);
		}

		if (!LoadRumbleGraph(rumbleGraphArray, info, highRumbleFile.data(), lowRumbleFile.data()))
		{
			return 0;
		}

		info->rumbleNameIndex = rumbleNameIndex;
		info->duration = info->duration * 1000.0f;

		return 1;
	}

	void Rumble::CG_RegisterRumbles(int localClientNum)
	{
		auto& rumbleGlobals = rumbleGlobArray[localClientNum];

		for (int i = 1; i < Gamepad::RUMBLE_CONFIGSTRINGS_COUNT; ++i)
		{
			const char* rumbleConf = ConfigStrings::CL_GetRumbleConfigString(i - 1);

			if (!*rumbleConf)
			{
				continue;
			}

			if (!HasRumbleFile(rumbleConf))
			{
				Logger::Warning("rumble: {} has no rumble file, not loaded\n", rumbleConf);
				rumbleGlobals.infos[i - 1] = {};
				continue;
			}

			CG_LoadRumble(rumbleGlobals.graphs, &rumbleGlobals.infos[i - 1], rumbleConf, i);
		}
	}

	void Rumble::CG_RegisterGraphics_Hk(int localClientNum, void* arg2)
	{
		reinterpret_cast<void(*)(int, void*)>(registerGraphicsHook.GetOriginal())(localClientNum, arg2);

		CG_RegisterRumbles(localClientNum);
	}

	int Rumble::G_RumbleIndex(const char* name)
	{
		if (!*name)
		{
			return 0;
		}

		const unsigned int rumbleToLookFor = Game::SL_FindLowercaseString(name);
		int i = 1;

		for (i = 1; i < Gamepad::RUMBLE_CONFIGSTRINGS_COUNT; ++i)
		{
			const unsigned int rumble = ConfigStrings::SV_GetRumbleConfigStringConst(i - 1);

			if (rumble == Game::scr_const->_)
			{
				break;
			}

			if (rumble == rumbleToLookFor)
			{
				return i;
			}
		}

		if (i >= Gamepad::RUMBLE_CONFIGSTRINGS_COUNT)
		{
			Logger::Print("WARNING: Rumble not registered, {}\n", name);
			return 0;
		}

		ConfigStrings::SV_SetRumbleConfigString(i - 1, name);
		return i;
	}

	void Rumble::RegisterWeaponRumbles(const Game::WeaponDef* weapDef)
	{
		const auto registerRumble = [](const char* rumbleName)
		{
			if (rumbleName && *rumbleName && HasRumbleFile(rumbleName))
			{
				G_RumbleIndex(rumbleName);
			}
		};

		registerRumble(weapDef->fireRumble);
		registerRumble(weapDef->meleeImpactRumble);
		registerRumble(weapDef->turretBarrelSpinRumble);

		if (!weapDef->notetrackRumbleMapKeys || !weapDef->notetrackRumbleMapValues)
		{
			return;
		}

		for (int i = 0; i < 16; ++i)
		{
			if (!weapDef->notetrackRumbleMapKeys[i])
			{
				break;
			}

			const unsigned short rumbleNameId = weapDef->notetrackRumbleMapValues[i];

			if (rumbleNameId)
			{
				registerRumble(Game::SL_ConvertToString(rumbleNameId));
			}
		}
	}

	void Rumble::CG_FireWeapon_Rumble(int localClientNum, const Game::entityState_s* ent, const Game::WeaponDef* weaponDef, bool isPlayerView)
	{
		const char* rumbleName = weaponDef->fireRumble;

		if (!rumbleName || !*rumbleName)
		{
			return;
		}

		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);

		const bool isLockedOnShooter = ent->eType == Game::ET_HELICOPTER
			&& (cg->predictedPlayerState.eFlags & Game::EF_VEHICLE_ACTIVE) != 0
			&& cg->predictedPlayerState.viewlocked_entNum == ent->number;

		if (isPlayerView || isLockedOnShooter)
		{
			CG_PlayRumbleOnClient(localClientNum, rumbleName);
		}
	}

	void Rumble::CG_FireWeapon_FireSound_Hk(int localClientNum, Game::centity_s* cent, unsigned int weaponIndex, unsigned short tagName, void* obj, const Game::WeaponDef* weaponDef, bool isPlayerView, int hand)
	{
		CG_FireWeapon_Rumble(localClientNum, &cent->nextState, weaponDef, isPlayerView);

		reinterpret_cast<void(*)(int, Game::centity_s*, unsigned int, unsigned short, void*, const Game::WeaponDef*, bool, int)>(fireSoundHook.GetOriginal())(
			localClientNum, cent, weaponIndex, tagName, obj, weaponDef, isPlayerView, hand);
	}

	Game::WeaponDef* Rumble::BG_GetWeaponDef_RegisterRumble_Hk(unsigned int weapIndex)
	{
		auto* weapDef = Game::BG_GetWeaponDef(weapIndex);

		RegisterWeaponRumbles(weapDef);

		return weapDef;
	}

	void Rumble::SCR_UpdateRumble()
	{
		constexpr int controllerIndex = 0;

		const bool isActive = Game::CL_GetLocalClientConnectionState(0) == Game::CA_ACTIVE;

		if (!isActive || (*Game::cl_paused)->current.integer != 0)
		{
			Gamepad::GPad_StopRumbles(controllerIndex);
		}
		else
		{
			Gamepad::GPad_UpdateFeedbacks();
		}
	}

	void Rumble::RemoveInactiveRumbles(int localClientNum, Game::ActiveRumble* activeRumbleArray)
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);

		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			Game::ActiveRumble* ar = &activeRumbleArray[i];

			if (!ar->rumbleInfo)
			{
				continue;
			}

			if (ar->rumbleInfo->duration < static_cast<float>(cg->time - ar->startTime))
			{
				InvalidateActiveRumble(ar);
				continue;
			}

			if (ar->sourceType == Game::RUMBLESOURCE_ENTITY)
			{
				const auto* entity = Game::CG_GetEntity(localClientNum, ar->source.entityNum);

				if (!entity->nextValid)
				{
					InvalidateActiveRumble(ar);
				}
			}
		}
	}

	void Rumble::CG_UpdateRumble(int localClientNum)
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);
		auto& rumbleGlobals = rumbleGlobArray[localClientNum];

		const bool isOtherView = cg->predictedPlayerState.clientNum != cg->clientNum || cg->predictedPlayerState.pm_type == Game::PM_SPECTATOR;

		if (cg->nextSnap && isOtherView)
		{
			for (auto& ar : rumbleGlobals.activeRumbles)
			{
				if (ar.startTime < 0)
				{
					break;
				}

				InvalidateActiveRumble(&ar);
			}

			Gamepad::GPad_SetLowRumble(localClientNum, 0.0);
			Gamepad::GPad_SetHighRumble(localClientNum, 0.0);
			return;
		}

		RemoveInactiveRumbles(localClientNum, rumbleGlobals.activeRumbles);
		CalcActiveRumbles(localClientNum, rumbleGlobals.activeRumbles, rumbleGlobals.receiverPos);
	}

	void Rumble::CG_SetRumbleReceiver()
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(0);
		auto& rumbleGlobals = rumbleGlobArray[0];

		rumbleGlobals.receiverEntNum = cg->predictedPlayerState.clientNum;
		std::memcpy(rumbleGlobals.receiverPos, cg->refdef.view.org, sizeof(rumbleGlobals.receiverPos));

		reinterpret_cast<void(*)()>(endDObjSceneHook.GetOriginal())();
	}

	int Rumble::CG_AddPacketEntities_Hk(int localClientNum)
	{
		CG_UpdateRumble(0);

		return reinterpret_cast<int(*)(int)>(addPacketEntitiesHook.GetOriginal())(localClientNum);
	}

	void Rumble::DebugRumbles()
	{
		auto* font = Game::R_RegisterFont("fonts/smallFont", 0);
		const auto height = static_cast<float>(Game::R_TextHeight(font));
		const float scale = 0.55f;

		const auto* cg = Game::CL_GetLocalClientGlobals(0);
		const auto* activeRumbles = rumbleGlobArray[0].activeRumbles;

		for (unsigned int i = 0; i < MAX_ACTIVE_RUMBLES; ++i)
		{
			float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			std::string line = std::format("{} => ", i);

			const Game::ActiveRumble* activeRumble = &activeRumbles[i];

			if (!activeRumble->rumbleInfo)
			{
				line += "INACTIVE";
			}
			else
			{
				const float duration01 = static_cast<float>(cg->time - activeRumble->startTime) / activeRumble->rumbleInfo->duration;

				const Game::RumbleGraph* highGraph = activeRumble->rumbleInfo->highRumbleGraph;
				const float highValue = Game::GraphGetValueFromFraction(highGraph->knotCount, highGraph->knots, duration01);

				const Game::RumbleGraph* lowGraph = activeRumble->rumbleInfo->lowRumbleGraph;
				const float lowValue = Game::GraphGetValueFromFraction(lowGraph->knotCount, lowGraph->knots, duration01);

				line += std::format("HIGH: {} / LOW: {} (Time left: {:.0f}%)", highValue * scale, lowValue * scale, duration01 * 100.0f);

				color[0] = 0.0f;
				color[2] = 1.0f;
			}

			const float y = (height * scale + 1.0f) * static_cast<float>(i + 1) + 4.0f;
			Game::R_AddCmdDrawText(line.data(), std::numeric_limits<int>::max(), font, 15.0f, y, scale, scale, 0.0f, color, Game::ITEM_TEXTSTYLE_NORMAL);
		}
	}

	void Rumble::LoadConstantRumbleConfigStrings()
	{
		for (std::size_t i = 0; i < std::size(rumbleStrings); ++i)
		{
			ConfigStrings::SV_SetRumbleConfigString(static_cast<int>(i), rumbleStrings[i]);
		}
	}

	void Rumble::SV_InitGameProgs_Hk(int savegame, int spawnServerFifthArg)
	{
		LoadConstantRumbleConfigStrings();

		reinterpret_cast<void(*)(int, int)>(initGameProgsHook.GetOriginal())(savegame, spawnServerFifthArg);
	}

	void Rumble::CG_GetImpactEffectForWeapon_Hk(int localClientNum, int sourceEntityNum, int weaponIndex, int surfType, int impactFlags, const Game::FxEffectDef** outFx, Game::snd_alias_list_t** outSnd)
	{
		CG_PlayRumbleOnClient(localClientNum, "riotshield_impact");

		reinterpret_cast<void(*)(int, int, int, int, int, const Game::FxEffectDef**, Game::snd_alias_list_t**)>(impactEffectHook.GetOriginal())(
			localClientNum, sourceEntityNum, weaponIndex, surfType, impactFlags, outFx, outSnd);
	}

	void Rumble::CG_ExplosiveImpactOnShieldEvent_Hk(int localClientNum)
	{
		CG_PlayRumbleOnClient(localClientNum, "riotshield_impact");

		reinterpret_cast<void(*)(int)>(impactOnShieldHook.GetOriginal())(localClientNum);
	}

	void Rumble::CG_ExplosiveSplashOnShieldEvent_Hk(int localClientNum, int weaponIndex)
	{
		CG_PlayRumbleOnClient(localClientNum, "riotshield_impact");

		reinterpret_cast<void(*)(int, int)>(splashOnShieldHook.GetOriginal())(localClientNum, weaponIndex);
	}

	void Rumble::PlayNoteMappedRumbleAliases(int localClientNum, const char* noteName, const Game::WeaponDef* weapDef)
	{
		if (!weapDef->notetrackRumbleMapKeys || !weapDef->notetrackRumbleMapValues || !*weapDef->notetrackRumbleMapKeys)
		{
			return;
		}

		const unsigned int stringId = Game::SL_FindLowercaseString(noteName);

		if (!stringId)
		{
			return;
		}

		for (int i = 0; i < 16; ++i)
		{
			const unsigned short key = weapDef->notetrackRumbleMapKeys[i];

			if (!key)
			{
				break;
			}

			const unsigned short value = weapDef->notetrackRumbleMapValues[i];

			if (!value || key != stringId)
			{
				continue;
			}

			const char* rumbleName = Game::SL_ConvertToString(value);

			if (rumbleName)
			{
				CG_PlayRumbleOnClientSafe(localClientNum, rumbleName);
			}
		}
	}

	void Rumble::CG_StopRumble(int localClientNum, int entityNum, const char* rumbleName)
	{
		for (auto& activeRumble : rumbleGlobArray[localClientNum].activeRumbles)
		{
			if (activeRumble.startTime <= 0 || activeRumble.sourceType != Game::RUMBLESOURCE_ENTITY)
			{
				continue;
			}

			if (activeRumble.source.entityNum != entityNum)
			{
				continue;
			}

			const char* otherRumbleName = ConfigStrings::CL_GetRumbleConfigString(activeRumble.rumbleInfo->rumbleNameIndex - 1);

			if (std::strcmp(otherRumbleName, rumbleName) == 0)
			{
				InvalidateActiveRumble(&activeRumble);
				return;
			}
		}
	}

	bool Rumble::CG_EntityEvents_Hk(const Game::centity_s* entity, int event)
	{
		const auto rumbleIndex = static_cast<int>(entity->nextState.eventParm);

		switch (event)
		{
		case EV_PLAY_RUMBLE_ON_ENT:
			CG_PlayRumbleOnEntity(0, ConfigStrings::CL_GetRumbleConfigString(rumbleIndex), entity->nextState.clientNum);
			return true;

		case EV_PLAY_RUMBLE_ON_POS:
			CG_PlayRumbleOnPosition(0, ConfigStrings::CL_GetRumbleConfigString(rumbleIndex), entity->pose.origin);
			return true;

		case EV_PLAY_RUMBLELOOP_ON_ENT:
			CG_PlayRumbleLoopOnEntity(0, ConfigStrings::CL_GetRumbleConfigString(rumbleIndex), entity->nextState.clientNum);
			return true;

		case EV_PLAY_RUMBLELOOP_ON_POS:
			CG_PlayRumbleLoopOnPosition(0, ConfigStrings::CL_GetRumbleConfigString(rumbleIndex), entity->pose.origin);
			return true;

		case EV_STOP_RUMBLE:
			CG_StopRumble(0, entity->nextState.clientNum, ConfigStrings::CL_GetRumbleConfigString(rumbleIndex));
			return true;

		case EV_STOP_ALL_RUMBLES:
			CG_StopAllRumbles();
			return true;

		default:
			return false;
		}
	}

	void Rumble::CG_StopAllRumbles()
	{
		for (auto& activeRumble : rumbleGlobArray[0].activeRumbles)
		{
			InvalidateActiveRumble(&activeRumble);
		}

		Gamepad::GPad_SetHighRumble(0, 0.0);
		Gamepad::GPad_SetLowRumble(0, 0.0);
		Gamepad::GPad_StopRumbles(0);
	}

	void Rumble::Scr_PlayRumbleOnEntity(Game::scr_entref_t entref)
	{
		Scr_PlayRumbleOnEntity_Internal(entref, EV_PLAY_RUMBLE_ON_ENT);
	}

	void Rumble::Scr_PlayRumbleLoopOnEntity(Game::scr_entref_t entref)
	{
		Scr_PlayRumbleOnEntity_Internal(entref, EV_PLAY_RUMBLELOOP_ON_ENT);
	}

	void Rumble::Scr_PlayRumbleOnEntity_Internal(Game::scr_entref_t entref, rumble_entity_event_t event)
	{
		auto* entity = Game::GetEntity(entref);
		const char* rumbleName = Game::Scr_GetString(0);
		const int index = G_RumbleIndex(rumbleName);

		if (!index)
		{
			Game::Com_Error(Game::ERR_SCRIPT, "unknown rumble name '%s'", rumbleName);
			return;
		}

		entity->r.svFlags = static_cast<char>(entity->r.svFlags & 0xFE);

		if (Game::Scr_GetNumParam() != 1)
		{
			GSC::Script::Scr_Error("Incorrect number of parameters.\n");
			return;
		}

		if (event == EV_PLAY_RUMBLELOOP_ON_ENT)
		{
			if (entity->client)
			{
				entity->client->ps.eFlags |= Game::EF_LOOP_RUMBLE;
			}
			else
			{
				entity->s.lerp.eFlags |= Game::EF_LOOP_RUMBLE;
			}
		}

		Game::G_AddEvent(entity, event, static_cast<unsigned int>(index - 1));
	}

	void Rumble::Scr_PlayRumbleOnPosition_Internal(rumble_entity_event_t event)
	{
		const char* rumbleName = Game::Scr_GetString(0);
		const int index = G_RumbleIndex(rumbleName);

		if (!index)
		{
			Game::Com_Error(Game::ERR_SCRIPT, "unknown rumble name '%s'", rumbleName);
			return;
		}

		float origin[3]{};
		Game::Scr_GetVector(1, origin);

		auto* entity = Game::G_TempEntity(origin, event);
		entity->s.eventParm = static_cast<unsigned int>(index - 1);
	}

	void Rumble::Scr_PlayRumbleLoopOnPosition()
	{
		if (Game::Scr_GetNumParam() != 2)
		{
			GSC::Script::Scr_ParamError(0, "PlayRumbleLoopOnPosition [rumble name] [pos]");
		}

		Scr_PlayRumbleOnPosition_Internal(EV_PLAY_RUMBLELOOP_ON_POS);
	}

	void Rumble::Scr_PlayRumbleOnPosition()
	{
		if (Game::Scr_GetNumParam() != 2)
		{
			GSC::Script::Scr_ParamError(0, "PlayRumbleOnPosition [rumble name] [pos]");
		}

		Scr_PlayRumbleOnPosition_Internal(EV_PLAY_RUMBLE_ON_POS);
	}

	void Rumble::CG_Turret_UpdateBarrelSpinRumble(int localClientNum, Game::centity_s* cent)
	{
		reinterpret_cast<void(*)(int, Game::centity_s*)>(barrelSpinSoundHook.GetOriginal())(localClientNum, cent);

		const auto* weapon = Game::BG_GetWeaponDef(cent->nextState.weapon);

		if (!weapon->turretBarrelSpinEnabled)
		{
			return;
		}

		const char* rumble = weapon->turretBarrelSpinRumble;

		if (!rumble || !*rumble || !cent->pose.turret.playerUsing)
		{
			return;
		}

		const auto* cg = Game::CL_GetLocalClientGlobals(localClientNum);
		const float spinRate = Game::BG_Turret_ComputeBarrelSpinRate(weapon, &cent->nextState.lerp.u.turret, cg->time);

		if (spinRate > 0.0f)
		{
			PlayRumbleInternal(localClientNum, rumble, false, Game::RUMBLESOURCE_ENTITY, cg->predictedPlayerState.clientNum, nullptr, spinRate * cl_rumbleScale.Get<float>(), true);
		}
	}

	void Rumble::MeleeRumble_Hook(Game::gentity_s* targetEntity, const Game::WeaponDef* weaponDef)
	{
		if (!targetEntity || !targetEntity->client)
		{
			return;
		}

		const char* rumbleName = weaponDef->meleeImpactRumble;

		if (!rumbleName || !*rumbleName)
		{
			return;
		}

		targetEntity->r.svFlags = static_cast<char>(targetEntity->r.svFlags & 0xFE);

		const int index = G_RumbleIndex(rumbleName);

		if (!index)
		{
			return;
		}

		Game::G_AddEvent(targetEntity, EV_PLAY_RUMBLE_ON_ENT, static_cast<unsigned int>(index - 1));
	}

	bool Rumble::TryInstallHooks()
	{
		struct CallSite
		{
			Utils::Hook* hook;
			std::uintptr_t site;
			std::uintptr_t engine;
			void* replacement;
		};

		const CallSite callSites[] =
		{
			{ &endDObjSceneHook, CG_DrawActiveFrame_R_EndDObjSceneCall, R_EndDObjScene, reinterpret_cast<void*>(CG_SetRumbleReceiver) },
			{ &addPacketEntitiesHook, CG_DrawActiveFrame_CG_AddPacketEntitiesCall, CG_AddPacketEntities, reinterpret_cast<void*>(CG_AddPacketEntities_Hk) },
			{ &initGameProgsHook, SV_SpawnServer_SV_InitGameProgsCall, SV_InitGameProgs, reinterpret_cast<void*>(SV_InitGameProgs_Hk) },
			{ &fireSoundHook, CG_FireWeapon_FireSoundCall, CG_FireWeapon_FireSound, reinterpret_cast<void*>(CG_FireWeapon_FireSound_Hk) },
			{ &impactEffectHook, CG_BulletHitClientShieldEvent_GetImpactEffectCall, CG_GetImpactEffectForWeapon, reinterpret_cast<void*>(CG_GetImpactEffectForWeapon_Hk) },
			{ &impactOnShieldHook, CG_EntityEvent_ExplosiveImpactOnShieldCall, CG_ExplosiveImpactOnShieldEvent, reinterpret_cast<void*>(CG_ExplosiveImpactOnShieldEvent_Hk) },
			{ &splashOnShieldHook, CG_EntityEvent_ExplosiveSplashOnShieldCall, CG_ExplosiveSplashOnShieldEvent, reinterpret_cast<void*>(CG_ExplosiveSplashOnShieldEvent_Hk) },
			{ &barrelSpinSoundHook, CG_ProcessEntity_UpdateBarrelSpinSoundCall, CG_Turret_UpdateBarrelSpinSound, reinterpret_cast<void*>(CG_Turret_UpdateBarrelSpinRumble) },
			{ &updateRumbleHook, Com_Frame_SCR_UpdateRumbleCall, SCR_UpdateRumble_Folded, reinterpret_cast<void*>(SCR_UpdateRumble) },
			{ &registerWeaponHook, G_RegisterWeapon_BG_GetWeaponDefCall, BG_GetWeaponDef, reinterpret_cast<void*>(BG_GetWeaponDef_RegisterRumble_Hk) },
			{ &registerGraphicsHook, CG_Init_CG_RegisterGraphicsCall, CG_RegisterGraphics, reinterpret_cast<void*>(CG_RegisterGraphics_Hk) },
		};

		for (const auto& callSite : callSites)
		{
			if (!Utils::Hook::BranchesTo(callSite.site, callSite.engine, HOOK_CALL))
			{
				Logger::Error("rumble: 0x{:X} is not the call it should be, no rumble\n", callSite.site);
				return false;
			}
		}

		if (!Utils::Hook::MatchesBytes(Weapon_Melee_TargetClientTest, targetClientTest, sizeof(targetClientTest))
			|| !Utils::Hook::MatchesBytes(CG_UpdateViewWeaponAnim_NoteName, noteNameLoad, sizeof(noteNameLoad)))
		{
			Logger::Error("rumble: the melee or notetrack site does not read as expected, no rumble\n");
			return false;
		}

		Rumble_MeleeTargetIsClient = Utils::Hook::Rebase(Weapon_Melee_TargetIsClient);
		Rumble_MeleeTargetIsNotClient = Utils::Hook::Rebase(Weapon_Melee_TargetIsNotClient);
		Rumble_NoteSoundMapTest = Utils::Hook::Rebase(CG_UpdateViewWeaponAnim_SoundMapTest);

		bool isSeated = meleeHook.Initialize(Weapon_Melee_TargetClientTest, reinterpret_cast<void*>(MeleeRumbleStub), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = noteNameHook.Initialize(CG_UpdateViewWeaponAnim_NoteName, reinterpret_cast<void*>(PlayNoteMappedSoundAliasesStub), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		for (const auto& callSite : callSites)
		{
			isSeated = callSite.hook->Initialize(callSite.site, callSite.replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			meleeHook.Uninstall();
			noteNameHook.Uninstall();

			for (const auto& callSite : callSites)
			{
				callSite.hook->Uninstall();
			}

			Logger::Error("rumble: could not seat every hook, no rumble\n");
			return false;
		}

		meleeHook.Quick();
		noteNameHook.Quick();

		for (const auto& callSite : callSites)
		{
			callSite.hook->Quick();
		}

		return true;
	}

	void Rumble::InitDvars()
	{
		cl_debug_rumbles = Dvar::Register("cl_debug_rumbles", false, Game::DVAR_NONE, "Debug rumbles on the screen");
		cl_rumbleScale = Dvar::Register("cl_rumbleScale", 0.6f, 0.0f, 1.0f, Game::DVAR_ARCHIVE, "Rumble multiplier for the controller");
	}

	Rumble::Rumble()
	{
		if (!ConfigStrings::HasRaisedTables())
		{
			Logger::Error("rumble: the configstring tables were not raised, no rumble\n");
			return;
		}

		if (!TryInstallHooks())
		{
			return;
		}

		Network::OnEntityEvent([](int, Game::centity_s* cent, int event)
		{
			return CG_EntityEvents_Hk(cent, event);
		});

		Events::OnDvarInit([]
		{
			InitDvars();
		});

		Command::Add("playrumble", [](const Command::Params* params)
		{
			if (!Game::CL_GetLocalClientGlobals(0)->nextSnap)
			{
				return;
			}

			if (params->Size() != 2)
			{
				Logger::Print("USAGE: playrumble <rumblename>\n");
				return;
			}

			CG_PlayRumbleOnClient(0, params->Get(1));
		});

		GSC::Script::AddFunction("PlayRumbleOnPosition", Scr_PlayRumbleOnPosition, false, true);
		GSC::Script::AddFunction("PlayRumbleLoopOnPosition", Scr_PlayRumbleLoopOnPosition, false, true);

		GSC::Script::AddMethod("PlayRumbleOnEntity", Scr_PlayRumbleOnEntity, false, true);
		GSC::Script::AddMethod("PlayRumbleLoopOnEntity", Scr_PlayRumbleLoopOnEntity, false, true);

		Scheduler::Loop([]
		{
			if (cl_debug_rumbles.Get<bool>())
			{
				DebugRumbles();
			}
		}, Scheduler::Pipeline::RENDERER);
	}
}
