#include "STDInclude.hpp"

#include "ClientCommand.hpp"
#include "Events.hpp"
#include "Logger.hpp"

#include "GSC/Script.hpp"

using namespace Utils::String;

namespace Components
{
	std::unordered_map<std::string, std::function<void(Game::gentity_s*, const Command::ServerParams*)>> ClientCommand::handlersSV;

	bool ClientCommand::cheatsEnabled;
	Dvar::Var ClientCommand::sv_cheats;

	constexpr std::uintptr_t SV_ExecuteClientCommand_ClientCommandCall = 0x140237142;
	constexpr std::uintptr_t ClientCommand_Engine = 0x1401990A0;

	constexpr std::uintptr_t isCheatOverride = 0x140427D98;
	constexpr std::uintptr_t Dvar_ForEach = 0x140285100;

	constexpr std::uintptr_t Dvar_SetVariant_CheatTest = 0x140287BA8;
	static const std::uint8_t cheatTest[] = { 0x83, 0xFE, 0x01, 0x75, 0x11, 0xA8, 0x04, 0x74, 0x0D, 0x80, 0x3D, 0xE0, 0x01, 0x1A, 0x00, 0x00, 0x0F, 0x84, 0xE2, 0x01, 0x00, 0x00 };

	constexpr std::uintptr_t Dvar_SetVariant_Com_LogFileOpenCall = 0x140287B2D;
	constexpr std::uintptr_t Com_LogFileOpen = 0x1401F5BA0;

	constexpr std::uintptr_t Cmd_Exec_f_ExecuteBufferCall = 0x1401E766B;
	constexpr std::uintptr_t Cmd_ExecuteBuffer = 0x1401E70B0;

	constexpr std::uintptr_t Playlist_RunRules_ExecuteCall = 0x14025C017;
	constexpr std::uintptr_t Cmd_ExecuteSingleCommand = 0x1401E7680;
	constexpr std::uintptr_t Playlist_RunRules_PlaylistNameJump = 0x14025C09D;
	constexpr std::uintptr_t Dvar_SetStringByName = 0x140287A70;

	constexpr std::uintptr_t CL_ParseGamestate_SystemInfoCall = 0x1401001BE;
	constexpr std::uintptr_t CL_GetConfigString = 0x1400F47C0;

	constexpr std::uintptr_t SV_InitGameVM_G_InitGameCall = 0x1402335BC;
	constexpr std::uintptr_t G_InitGame = 0x14019CF90;

	constexpr std::uintptr_t SV_SpawnServer_dvar_allowedModifiedFlags = 0x14023B284;
	static const std::uint8_t allowedModifiedFlagsReset[] = { 0xBA, 0x24, 0x00, 0x00, 0x00, 0x4A, 0x8B, 0x0C, 0x20, 0xC7, 0x04, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF };
	constexpr std::uintptr_t SV_SpawnServer_LSP_ForceSendPacketCall = 0x14023B294;
	constexpr std::uintptr_t LSP_ForceSendPacket = 0x1401B0910;

	constexpr std::uintptr_t Dvar_Reregister = 0x140286AA0;
	constexpr std::uintptr_t Dvar_SetLatchedValue = 0x1402878A0;

	constexpr std::uintptr_t Dvar_ReregisterCalls[] =
	{
		0x1402858C8,
		0x140285D02,
		0x140285EDD,
		0x140285FFE,
		0x140286127,
		0x140286215,
		0x1402865AB,
		0x1402866B1,
		0x1402867E1,
		0x14028690B,
		0x140286A40,
		0x1402877E3,
	};

	static Utils::Hook clientCommandHook;
	static Utils::Hook logFileOpenHook;
	static Utils::Hook rawFileExecuteHook;
	static Utils::Hook playlistExecuteHook;
	static Utils::Hook playlistNameHook;
	static Utils::Hook systemInfoHook;
	static Utils::Hook initGameHook;
	static Utils::Hook spawnServerHook;
	static Utils::Hook reregisterHooks[std::size(Dvar_ReregisterCalls)];

	static bool isCheatProtectionSeated = false;
	static bool isOverridingCheatProtection = false;
	static bool* cheatOverride = nullptr;
	static std::optional<bool> spawnCheats;

	static bool IsCheatProtected(unsigned int flags)
	{
		if (!(flags & Game::DVAR_CHEAT))
		{
			return false;
		}

		const Game::dvar_t* const cheats = ClientCommand::sv_cheats.Get();

		if (!cheats)
		{
			return false;
		}

		return !cheats->current.enabled && !isOverridingCheatProtection;
	}

	static void CollectCheatDvar(Game::dvar_t* dvar, void* cheatDvars)
	{
		if (dvar->flags & Game::DVAR_CHEAT)
		{
			static_cast<std::vector<Game::dvar_t*>*>(cheatDvars)->push_back(dvar);
		}
	}

	static void ResetCheatDvars()
	{
		std::vector<Game::dvar_t*> cheatDvars;

		reinterpret_cast<void(*)(void(*)(Game::dvar_t*, void*), void*)>(Utils::Hook::Rebase(Dvar_ForEach))(CollectCheatDvar, &cheatDvars);

		for (auto* const dvar : cheatDvars)
		{
			Game::Dvar_SetVariant(dvar, dvar->reset, Game::DVAR_SOURCE_INTERNAL);
		}
	}

	static bool Dvar_SetVariant_Com_LogFileOpen_Hk()
	{
		const bool isLogFileOpen = reinterpret_cast<bool(*)()>(logFileOpenHook.GetOriginal())();

		*cheatOverride = !IsCheatProtected(Game::DVAR_CHEAT);

		return isLogFileOpen;
	}

	static void Cmd_Exec_f_ExecuteBuffer_Hk(int localClientNum, int controllerIndex, const char* text, void(*execute)(int, int, const char*))
	{
		isOverridingCheatProtection = true;

		reinterpret_cast<void(*)(int, int, const char*, void(*)(int, int, const char*))>(rawFileExecuteHook.GetOriginal())(
			localClientNum, controllerIndex, text, execute);

		isOverridingCheatProtection = false;
	}

	static void Playlist_RunRules_Execute_Hk(int localClientNum, int controllerIndex, const char* text)
	{
		reinterpret_cast<void(*)(int, int, const char*)>(playlistExecuteHook.GetOriginal())(localClientNum, controllerIndex, text);

		isOverridingCheatProtection = true;
	}

	static void Playlist_RunRules_PlaylistName_Hk(const char* dvarName, const char* value)
	{
		isOverridingCheatProtection = false;

		reinterpret_cast<void(*)(const char*, const char*)>(playlistNameHook.GetOriginal())(dvarName, value);
	}

	static const char* CL_ParseGamestate_SystemInfo_Hk(int index)
	{
		const char* const systemInfo = reinterpret_cast<const char*(*)(int)>(systemInfoHook.GetOriginal())(index);

		if ((*Game::com_sv_running)->current.enabled || Game::CL_GetLocalClientConnectionState(0) >= Game::CA_ACTIVE)
		{
			return systemInfo;
		}

		if (std::atoi(Game::Info_ValueForKey(systemInfo, "sv_cheats")) == 0)
		{
			ResetCheatDvars();
		}

		return systemInfo;
	}

	static void SV_InitGameVM_G_InitGame_Hk(int levelTime, unsigned int randomSeed, int restart, int registerDvars, int savePersist, int loadGameArg)
	{
		const Game::dvar_t* const cheats = ClientCommand::sv_cheats.Get();

		if (cheats && !cheats->current.enabled && !restart)
		{
			ResetCheatDvars();
		}

		reinterpret_cast<void(*)(int, unsigned int, int, int, int, int)>(initGameHook.GetOriginal())(
			levelTime, randomSeed, restart, registerDvars, savePersist, loadGameArg);
	}

	static void SV_SpawnServer_SetCheats_Hk()
	{
		reinterpret_cast<void(*)()>(spawnServerHook.GetOriginal())();

		if (!spawnCheats.has_value())
		{
			return;
		}

		const bool isEnabled = *spawnCheats;
		spawnCheats.reset();

		if (ClientCommand::sv_cheats.IsValid())
		{
			Game::Dvar_SetBool(ClientCommand::sv_cheats.Get(), isEnabled);
		}
	}

	static void Dvar_Reregister_Hk(Game::dvar_t* dvar, const char* name, std::uint8_t type, unsigned int flags, Game::DvarValue resetValue, Game::DvarLimits domain, const char* description)
	{
		reinterpret_cast<void(*)(Game::dvar_t*, const char*, std::uint8_t, unsigned int, Game::DvarValue, Game::DvarLimits, const char*)>(Utils::Hook::Rebase(Dvar_Reregister))(
			dvar, name, type, flags, resetValue, domain, description);

		if (!IsCheatProtected(dvar->flags))
		{
			return;
		}

		Game::Dvar_SetVariant(dvar, dvar->reset, Game::DVAR_SOURCE_INTERNAL);
		reinterpret_cast<void(*)(Game::dvar_t*, Game::DvarValue)>(Utils::Hook::Rebase(Dvar_SetLatchedValue))(dvar, dvar->reset);
	}

	static bool TrySeatCheatProtection()
	{
		bool isExpected = Utils::Hook::MatchesBytes(Dvar_SetVariant_CheatTest, cheatTest, sizeof(cheatTest))
			&& Utils::Hook::MatchesBytes(SV_SpawnServer_dvar_allowedModifiedFlags, allowedModifiedFlagsReset, sizeof(allowedModifiedFlagsReset))
			&& Utils::Hook::BranchesTo(Dvar_SetVariant_Com_LogFileOpenCall, Com_LogFileOpen, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Cmd_Exec_f_ExecuteBufferCall, Cmd_ExecuteBuffer, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Playlist_RunRules_ExecuteCall, Cmd_ExecuteSingleCommand, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Playlist_RunRules_PlaylistNameJump, Dvar_SetStringByName, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(CL_ParseGamestate_SystemInfoCall, CL_GetConfigString, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_InitGameVM_G_InitGameCall, G_InitGame, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_SpawnServer_LSP_ForceSendPacketCall, LSP_ForceSendPacket, HOOK_CALL);

		for (const auto call : Dvar_ReregisterCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, Dvar_Reregister, HOOK_CALL);
		}

		if (!isExpected)
		{
			Logger::Error("clientcommand: the dvar cheat checks do not read as expected, cheat dvars ignore sv_cheats\n");
			return false;
		}

		cheatOverride = reinterpret_cast<bool*>(Utils::Hook::Rebase(isCheatOverride));

		bool isSeated = logFileOpenHook.Initialize(Dvar_SetVariant_Com_LogFileOpenCall, reinterpret_cast<void*>(Dvar_SetVariant_Com_LogFileOpen_Hk), HOOK_CALL)->Install()->IsInstalled();
		isSeated = rawFileExecuteHook.Initialize(Cmd_Exec_f_ExecuteBufferCall, reinterpret_cast<void*>(Cmd_Exec_f_ExecuteBuffer_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = playlistExecuteHook.Initialize(Playlist_RunRules_ExecuteCall, reinterpret_cast<void*>(Playlist_RunRules_Execute_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = playlistNameHook.Initialize(Playlist_RunRules_PlaylistNameJump, reinterpret_cast<void*>(Playlist_RunRules_PlaylistName_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = systemInfoHook.Initialize(CL_ParseGamestate_SystemInfoCall, reinterpret_cast<void*>(CL_ParseGamestate_SystemInfo_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = initGameHook.Initialize(SV_InitGameVM_G_InitGameCall, reinterpret_cast<void*>(SV_InitGameVM_G_InitGame_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = spawnServerHook.Initialize(SV_SpawnServer_LSP_ForceSendPacketCall, reinterpret_cast<void*>(SV_SpawnServer_SetCheats_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(Dvar_ReregisterCalls); ++i)
		{
			isSeated = reregisterHooks[i].Initialize(Dvar_ReregisterCalls[i], reinterpret_cast<void*>(Dvar_Reregister_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			logFileOpenHook.Uninstall();
			rawFileExecuteHook.Uninstall();
			playlistExecuteHook.Uninstall();
			playlistNameHook.Uninstall();
			systemInfoHook.Uninstall();
			initGameHook.Uninstall();
			spawnServerHook.Uninstall();

			for (auto& hook : reregisterHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("clientcommand: could not seat the dvar cheat checks, cheat dvars ignore sv_cheats\n");
			return false;
		}

		logFileOpenHook.Quick();
		rawFileExecuteHook.Quick();
		playlistExecuteHook.Quick();
		playlistNameHook.Quick();
		systemInfoHook.Quick();
		initGameHook.Quick();
		spawnServerHook.Quick();

		for (auto& hook : reregisterHooks)
		{
			hook.Quick();
		}

		return true;
	}

	ClientCommand::CheatsScopedLock::CheatsScopedLock()
	{
		cheatsEnabled = true;
	}

	ClientCommand::CheatsScopedLock::~CheatsScopedLock()
	{
		cheatsEnabled = false;
	}

	bool ClientCommand::CheatsOk(const Game::gentity_s* ent)
	{
		const auto entNum = ent->s.number;

		if (!sv_cheats.Get<bool>() && !cheatsEnabled)
		{
			Logger::Debug("Cheats are disabled!");
			Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"GAME_CHEATSNOTENABLED\"", 0x65));
			return false;
		}

		if (ent->health < 1)
		{
			Logger::Debug("Entity {} must be alive to use this command!", entNum);
			Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"GAME_MUSTBEALIVECOMMAND\"", 0x65));
			return false;
		}

		return true;
	}

	void ClientCommand::SetCheatsForSpawn(bool isEnabled)
	{
		if (isCheatProtectionSeated)
		{
			spawnCheats = isEnabled;
			return;
		}

		if (sv_cheats.IsValid())
		{
			Game::Dvar_SetBool(sv_cheats.Get(), isEnabled);
		}
	}

	void ClientCommand::Add(const char* name, const std::function<void(Game::gentity_s*, const Command::ServerParams*)>& callback)
	{
		const auto command = Utils::String::ToLower(name);

		handlersSV[command] = callback;
	}

	void ClientCommand::ClientCommandStub(const int clientNum)
	{
		const auto ent = &Game::g_entities[clientNum];

		if (!ent->client)
		{
			Logger::Debug("ClientCommand: client {} is not fully connected", clientNum);
			return;
		}

		Command::ServerParams params;
		const auto command = Utils::String::ToLower(params.Get(0));

		if (const auto itr = handlersSV.find(command); itr != handlersSV.end())
		{
			itr->second(ent, &params);
			return;
		}

		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(ClientCommand_Engine))(clientNum);
	}

	void ClientCommand::AddCheatCommands()
	{
		Add("noclip", Cmd_Noclip_f);
		Add("ufo", Cmd_UFO_f);

		Add("god", [](Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			if (!CheatsOk(ent))
			{
				return;
			}

			ent->flags ^= Game::FL_GODMODE;

			const auto entNum = ent->s.number;
			Logger::Debug("God toggled for entity {}", entNum);

			const auto* message = "GAME_GODMODE_OFF";

			if (ent->flags & Game::FL_GODMODE)
			{
				message = "GAME_GODMODE_ON";
			}

			Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"%s\"", 0x65, message));
		});

		Add("demigod", [](Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			if (!CheatsOk(ent))
			{
				return;
			}

			ent->flags ^= Game::FL_DEMI_GODMODE;

			const auto entNum = ent->s.number;
			Logger::Debug("Demigod toggled for entity {}", entNum);

			const auto* message = "GAME_DEMI_GODMODE_OFF";

			if (ent->flags & Game::FL_DEMI_GODMODE)
			{
				message = "GAME_DEMI_GODMODE_ON";
			}

			Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"%s\"", 0x65, message));
		});

		Add("notarget", [](Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			if (!CheatsOk(ent))
			{
				return;
			}

			ent->flags ^= Game::FL_NOTARGET;

			const auto entNum = ent->s.number;
			Logger::Debug("Notarget toggled for entity {}", entNum);

			const auto* message = "GAME_NOTARGETOFF";

			if (ent->flags & Game::FL_NOTARGET)
			{
				message = "GAME_NOTARGETON";
			}

			Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"%s\"", 0x65, message));
		});

		Add("setviewpos", [](Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			assert(ent);

			if (!CheatsOk(ent))
			{
				return;
			}

			float origin[3];
			float angles[3]{ 0.f, 0.f, 0.f };

			if (params->Size() < 4 || params->Size() > 6)
			{
				Game::SV_GameSendServerCommand(ent->s.number, Game::SV_CMD_CAN_IGNORE, VA("%c \"GAME_USAGE\x15: setviewpos x y z [yaw] [pitch]\n\"", 0x65));
				return;
			}

			for (auto i = 0; i < 3; i++)
			{
				origin[i] = std::strtof(params->Get(i + 1), nullptr);
			}

			if (params->Size() >= 5)
			{
				angles[1] = std::strtof(params->Get(4), nullptr);
			}

			if (params->Size() == 6)
			{
				angles[0] = std::strtof(params->Get(5), nullptr);
			}

			Logger::Debug("Teleported entity {} to {:f} {:f} {:f}\nviewpos {:f} {:f}", ent->s.number,
				origin[0], origin[1], origin[2], angles[0], angles[2]);
			Game::TeleportPlayer(ent, origin, angles);
		});

		Add("give", [](Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			if (!CheatsOk(ent))
			{
				return;
			}

			if (params->Size() < 2)
			{
				Game::SV_GameSendServerCommand(ent->s.number, Game::SV_CMD_CAN_IGNORE, VA("%c \"GAME_USAGE\x15: give <weapon name>\"", 0x65));
				return;
			}

			Game::level->initializing = 1;
			const auto* weaponName = params->Get(1);
			Logger::Debug("Giving weapon {} to entity {}", weaponName, ent->s.number);
			const auto weaponIndex = Game::G_GetWeaponIndexForName(weaponName);

			if (weaponIndex == 0)
			{
				Game::level->initializing = 0;
				return;
			}

			if (Game::BG_GetWeaponDef(weaponIndex)->inventoryType == Game::WEAPINVENTORY_ALTMODE)
			{
				Logger::Error("You can't directly spawn the altfire weapon '{}'. Spawn a weapon that has this altmode instead.\n", weaponName);
				Game::level->initializing = 0;
				return;
			}

			auto* weapEnt = Game::G_Spawn();
			std::memcpy(weapEnt->r.currentOrigin, ent->r.currentOrigin, sizeof(float[3]));
			Game::G_GetItemClassname(static_cast<int>(weaponIndex), weapEnt);
			Game::G_SpawnItem(weapEnt, static_cast<int>(weaponIndex));

			weapEnt->active = 1;
			const auto offHandClass = static_cast<Game::OffhandClass>(Game::BG_GetWeaponDef(weaponIndex)->offhandClass);

			if (offHandClass != Game::OFFHAND_CLASS_NONE)
			{
				auto* client = ent->client;

				if ((client->ps.weapCommon.offhandPrimary != offHandClass) && (client->ps.weapCommon.offhandSecondary != offHandClass))
				{
					switch (offHandClass)
					{
					case Game::OFFHAND_CLASS_FRAG_GRENADE:
					case Game::OFFHAND_CLASS_THROWINGKNIFE:
					case Game::OFFHAND_CLASS_OTHER:
						Logger::Debug("Setting offhandPrimary");
						client->ps.weapCommon.offhandPrimary = offHandClass;
						break;
					default:
						Logger::Debug("Setting offhandSecondary");
						client->ps.weapCommon.offhandSecondary = offHandClass;
						break;
					}
				}
			}

			Game::Touch_Item(weapEnt, ent, 0);
			weapEnt->active = 0;

			if (weapEnt->r.isInUse)
			{
				Logger::Debug("Freeing up entity {}", weapEnt->s.number);
				Game::G_FreeEntity(weapEnt);
			}

			Game::level->initializing = 0;

			for (std::size_t i = 0; i < std::extent_v<decltype(Game::playerState_s::weaponsEquipped)>; ++i)
			{
				const auto index = ent->client->ps.weaponsEquipped[i];

				if (index)
				{
					Game::Add_Ammo(ent, index, 0, 998, 1);
				}
			}
		});

		Add("kill", []([[maybe_unused]] Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
		{
			assert(ent->client);
			assert(ent->client->sess.connected != Game::CON_DISCONNECTED);

			if (ent->client->sess.sessionState != Game::SESS_STATE_PLAYING || !CheatsOk(ent))
			{
				return;
			}

			auto** bgs = Game::Sys::GetTls<Game::bgs_t*>(Game::Sys::TLS_OFFSET::LEVEL_BGS);

			assert(*bgs == nullptr);

			*bgs = Game::level_bgs;

			ent->flags &= ~(Game::FL_GODMODE | Game::FL_DEMI_GODMODE);
			ent->health = 0;
			ent->client->ps.stats[0] = 0;
			Game::player_die(ent, ent, ent, 100000, Game::MOD_SUICIDE, 0, nullptr, Game::HITLOC_NONE, 0);

			assert(*bgs == Game::level_bgs);

			*bgs = nullptr;
		});
	}

	void ClientCommand::AddScriptFunctions()
	{
		GSC::Script::AddFunction("DropAllBots", []
		{
			Game::SV_DropAllBots();
		});
	}

	void ClientCommand::AddScriptMethods()
	{
		GSC::Script::AddMethod("Noclip", [](const Game::scr_entref_t entref)
		{
			auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			CheatsScopedLock cheatsLock;
			Cmd_Noclip_f(ent, nullptr);
		});

		GSC::Script::AddMethod("Ufo", [](const Game::scr_entref_t entref)
		{
			auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			CheatsScopedLock cheatsLock;
			Cmd_UFO_f(ent, nullptr);
		});
	}

	void ClientCommand::Cmd_Noclip_f(Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
	{
		if (!CheatsOk(ent))
		{
			return;
		}

		ent->client->flags ^= Game::CF_BIT_NOCLIP;

		const auto entNum = ent->s.number;
		Logger::Debug("Noclip toggled for entity {}", entNum);

		const auto* message = "GAME_NOCLIPOFF";

		if (ent->client->flags & Game::CF_BIT_NOCLIP)
		{
			message = "GAME_NOCLIPON";
		}

		Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"%s\"", 0x65, message));
	}

	void ClientCommand::Cmd_UFO_f(Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
	{
		if (!CheatsOk(ent))
		{
			return;
		}

		ent->client->flags ^= Game::CF_BIT_UFO;

		const auto entNum = ent->s.number;
		Logger::Debug("UFO toggled for entity {}", entNum);

		const auto* message = "GAME_UFOOFF";

		if (ent->client->flags & Game::CF_BIT_UFO)
		{
			message = "GAME_UFOON";
		}

		Game::SV_GameSendServerCommand(entNum, Game::SV_CMD_CAN_IGNORE, VA("%c \"%s\"", 0x65, message));
	}

	ClientCommand::ClientCommand()
	{
		AssertOffset(Game::playerState_s, stats, 0x150);

		isCheatProtectionSeated = TrySeatCheatProtection();

		Events::OnDvarInit([]
		{
			unsigned int flags = Game::DVAR_INIT | Game::DVAR_INTERNAL;

			if (isCheatProtectionSeated)
			{
				flags |= Game::DVAR_SYSTEMINFO;
			}

			sv_cheats = Dvar::Register("sv_cheats", false, flags, "Enable cheats on the server");
		});

		cheatsEnabled = false;

		AddScriptFunctions();
		AddScriptMethods();

		if (!Utils::Hook::BranchesTo(SV_ExecuteClientCommand_ClientCommandCall, ClientCommand_Engine, false)
			|| !clientCommandHook.Initialize(SV_ExecuteClientCommand_ClientCommandCall, reinterpret_cast<void*>(ClientCommandStub), HOOK_CALL)->Install()->IsInstalled())
		{
			clientCommandHook.Uninstall();
			Logger::Error("clientcommand: SV_ExecuteClientCommand does not read as expected, no custom client commands\n");
			return;
		}

		clientCommandHook.Quick();

		AddCheatCommands();
	}
}
