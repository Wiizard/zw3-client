#include "STDInclude.hpp"

#include "Components/Modules/ClientCommand.hpp"
#include "Components/Modules/ClientSlots.hpp"
#include "Components/Modules/Command.hpp"
#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Dvar.hpp"
#include "Components/Modules/Logger.hpp"
#include "Components/Modules/Network.hpp"
#include "Components/Modules/TextRenderer.hpp"
#include "Components/Modules/ZoneBuilder.hpp"

namespace Game
{
	SV_AddTestClient_t SV_AddTestClient = nullptr;
	SV_IsTestClient_t SV_IsTestClient = nullptr;
	SV_GameSendServerCommand_t SV_GameSendServerCommand = nullptr;
	SV_Cmd_TokenizeString_t SV_Cmd_TokenizeString = nullptr;
	SV_Cmd_EndTokenizedString_t SV_Cmd_EndTokenizedString = nullptr;
	SV_SetConfigstring_t SV_SetConfigstring = nullptr;
	SV_GetConfigstringConst_t SV_GetConfigstringConst = nullptr;
	SV_DirectConnect_t SV_DirectConnect = nullptr;
	SV_ClientThink_t SV_ClientThink = nullptr;
	SV_DropClient_t SV_DropClient = nullptr;
	SV_FindClientByAddress_t SV_FindClientByAddress = nullptr;
	SV_GameDropClient_t SV_GameDropClient = nullptr;

	int* svs_time = nullptr;
	int* svs_clientCount = nullptr;
	client_s* svs_clients = nullptr;

	volatile long* sv_thread_owns_game = nullptr;

	playerState_s* SV_GetPlayerstateForClientNum(int clientNum)
	{
		return reinterpret_cast<playerState_s*(*)(int)>(Utils::Hook::Rebase(0x140233470))(clientNum);
	}

	int SV_GetServerThreadOwnsGame()
	{
		return *sv_thread_owns_game;
	}

	void SV_DropAllBots()
	{
		for (auto i = 0; i < *svs_clientCount; ++i)
		{
			if (svs_clients[i].header.state != CS_FREE
				&& svs_clients[i].header.netchan.remoteAddress.type == NA_BOT)
			{
				SV_GameDropClient(i, "GAME_GET_TO_COVER");
			}
		}
	}

	int SV_GetClientStat(int clientNum, int index)
	{
		if (index < 2000)
		{
			return svs_clients[clientNum].stats.binary[index];
		}

		if (index < 3498)
		{
			return svs_clients[clientNum].stats.data[index - 2000];
		}

		return 0;
	}

	void SV_SetClientStat(int clientNum, int index, int value)
	{
		if (index < 2000)
		{
			if (svs_clients[clientNum].stats.binary[index] == value)
			{
				return;
			}

			svs_clients[clientNum].stats.binary[index] = static_cast<unsigned char>(value);
		}
		else if (index < 3498)
		{
			if (svs_clients[clientNum].stats.data[index - 2000] == value)
			{
				return;
			}

			svs_clients[clientNum].stats.data[index - 2000] = value;
		}
		else
		{
			return;
		}

		SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, Utils::String::VA("%c %i %i", setStatCommand, index, value));
	}

	void BindServer()
	{
		SV_AddTestClient = BindFunction<SV_AddTestClient_t>(0x140236D00);
		SV_IsTestClient = BindFunction<SV_IsTestClient_t>(0x1402389F0);
		SV_GameSendServerCommand = BindFunction<SV_GameSendServerCommand_t>(0x1402333E0);
		SV_Cmd_TokenizeString = BindFunction<SV_Cmd_TokenizeString_t>(0x1401E8370);
		SV_Cmd_EndTokenizedString = BindFunction<SV_Cmd_EndTokenizedString_t>(0x1401E8330);
		SV_SetConfigstring = BindFunction<SV_SetConfigstring_t>(0x14023ACB0);
		SV_GetConfigstringConst = BindFunction<SV_GetConfigstringConst_t>(0x14023A1F0);
		SV_DirectConnect = BindFunction<SV_DirectConnect_t>(0x1402374E0);
		SV_ClientThink = BindFunction<SV_ClientThink_t>(0x140237260);
		SV_DropClient = BindFunction<SV_DropClient_t>(0x140237EC0);
		SV_FindClientByAddress = BindFunction<SV_FindClientByAddress_t>(0x14023C450);
		SV_GameDropClient = BindFunction<SV_GameDropClient_t>(0x1402333B0);

		svs_time = reinterpret_cast<int*>(Utils::Hook::Rebase(0x14340EC80));
		svs_clientCount = reinterpret_cast<int*>(Utils::Hook::Rebase(0x14340EC88));
		svs_clients = reinterpret_cast<client_s*>(Utils::Hook::Rebase(0x14340EC90));

		sv_thread_owns_game = reinterpret_cast<volatile long*>(Utils::Hook::Rebase(0x1422CE6B0));
	}

	constexpr std::uintptr_t SV_SpawnServer = 0x14023B220;

	constexpr std::uintptr_t SV_MapRestart = 0x140236360;
	constexpr std::uintptr_t SV_FastRestart = 0x140236120;
	constexpr std::uintptr_t SND_FadeAllSounds = 0x140245900;
	constexpr std::uintptr_t sv_map_restart = 0x1422CE1E0;
	constexpr std::uintptr_t sv_loadScripts = 0x1422CE1E4;
	constexpr std::uintptr_t sv_migrate = 0x1422CE1E8;

	constexpr std::uintptr_t com_errorPrintsCount = 0x141BD9ADC;

	constexpr std::uintptr_t DB_FileExists = 0x14012D3C0;
	constexpr std::uintptr_t FS_ConvertPath = 0x140275700;

	constexpr std::uintptr_t Party_StopParty = 0x14010C000;
	constexpr std::size_t partyIsRunning = 6164;
	constexpr std::uintptr_t Com_Shutdown = 0x1401F69D0;
	constexpr std::uintptr_t LiveStorage_EndGame = 0x1401F75D0;
	constexpr std::uintptr_t onlinegameDvar = 0x140D05078;

	constexpr std::uintptr_t SV_MigrationStart = 0x14023E820;
	constexpr std::uintptr_t migrationFlag = 0x14650DC94;
	constexpr std::uintptr_t xblive_privatematchDvar = 0x146787050;

	constexpr std::uintptr_t sv_kickBanTimeDvar = 0x14650D8B8;
	constexpr std::size_t tempBanCount = 16;

	constexpr std::size_t clientQport = 60;
	constexpr std::size_t clientRate = 135912;

	constexpr std::uintptr_t Dvar_InfoString = 0x1401FC3C0;

	constexpr std::uintptr_t mapnameDvar = 0x14650D878;

	constexpr std::size_t mapNameSize = 64;

	struct TempBan
	{
		std::uint64_t xuid;
		int time;
	};

	static TempBan tempBans[tempBanCount];

	template <typename T>
	static T* Global(std::uintptr_t address)
	{
		return reinterpret_cast<T*>(Utils::Hook::Rebase(address));
	}

	static std::string MapBaseName(const char* name)
	{
		std::string_view base = name;

		if (base.size() >= 8 && _strnicmp(base.data(), "maps/mp/", 8) == 0)
		{
			base.remove_prefix(8);
		}

		if (base.size() >= 7 && _stricmp(base.data() + base.size() - 3, "bsp") == 0)
		{
			base.remove_suffix(7);
		}

		std::string result(base.substr(0, mapNameSize - 1));
		std::ranges::replace(result, '%', '_');
		return result;
	}

	static bool IsPartyRunning(PartyData* party)
	{
		return *reinterpret_cast<int*>(reinterpret_cast<std::uint8_t*>(party) + partyIsRunning) != 0;
	}

	bool IsMapOnDisk(const char* name)
	{
		std::string mapname = MapBaseName(name);
		std::ranges::transform(mapname, mapname.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		return reinterpret_cast<bool(*)(const char*, int)>(Utils::Hook::Rebase(DB_FileExists))(mapname.data(), 0);
	}

	static void Map(const Components::Command::Params* params, bool isRestart)
	{
		*Global<int>(com_errorPrintsCount) = 0;

		char mapname[mapNameSize];
		strncpy_s(mapname, MapBaseName(params->Get(1)).data(), _TRUNCATE);
		_strlwr_s(mapname);

		const int migrate = *Global<int>(sv_migrate);

		if (!reinterpret_cast<bool(*)(const char*, int)>(Utils::Hook::Rebase(DB_FileExists))(mapname, 0))
		{
			Components::Logger::Error("Can't find map \"{}\".\nA mod is required for custom maps\n", mapname);
			return;
		}

		const bool isDevmap = _stricmp(params->Get(0), "devmap") == 0;

		reinterpret_cast<void(*)(char*)>(Utils::Hook::Rebase(FS_ConvertPath))(mapname);

		Components::ClientCommand::SetCheatsForSpawn(isDevmap);

		reinterpret_cast<void(*)(const char*, int, int, int, int)>(Utils::Hook::Rebase(SV_SpawnServer))(
			mapname, isRestart, migrate != 0, 0, migrate);
	}

	static void MapCommand(const Components::Command::Params* params)
	{
		if (params->Size() <= 1 || !*params->Get(1))
		{
			return;
		}

		bool isRestart = false;

		if (params->Size() > 2)
		{
			isRestart = std::atol(params->Get(2)) != 0;
		}

		int migrate = 0;

		if (params->Size() > 3)
		{
			migrate = std::atol(params->Get(3));
		}

		*Global<int>(sv_migrate) = migrate;
		Map(params, isRestart);
	}

	static void DevmapCommand(const Components::Command::Params* params)
	{
		if (params->Size() <= 1 || !*params->Get(1))
		{
			return;
		}

		const auto stopParty = reinterpret_cast<void(*)(PartyData*)>(Utils::Hook::Rebase(Party_StopParty));

		if (IsPartyRunning(g_partyData))
		{
			stopParty(g_partyData);
		}

		if (IsPartyRunning(g_lobbyData))
		{
			stopParty(g_lobbyData);
		}

		reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Com_Shutdown))("EXE_SERVERKILLED");
		Dvar_SetBool(*Global<dvar_t*>(onlinegameDvar), false);
		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(LiveStorage_EndGame))(0);

		Map(params, false);
	}

	static void MapRestartCommand()
	{
		reinterpret_cast<void(*)(float, int)>(Utils::Hook::Rebase(SND_FadeAllSounds))(0.0f, 0);

		*Global<int>(sv_map_restart) = 1;
		*Global<int>(sv_loadScripts) = 1;
		*Global<int>(sv_migrate) = 0;

		if (!(*com_sv_running)->current.enabled)
		{
			reinterpret_cast<void(*)(int, int)>(Utils::Hook::Rebase(SV_MapRestart))(0, 1);
		}
	}

	static void FastRestartCommand()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SV_FastRestart))();
	}

	static std::size_t TempBanSlot()
	{
		std::size_t oldest = 0;

		for (std::size_t i = 0; i < tempBanCount; ++i)
		{
			if (!tempBans[i].xuid)
			{
				return i;
			}

			if (tempBans[i].time < tempBans[oldest].time)
			{
				oldest = i;
			}
		}

		return oldest;
	}

	static void AddTempBan(std::uint64_t xuid)
	{
		auto& ban = tempBans[TempBanSlot()];
		ban.xuid = xuid;
		ban.time = *svs_time;
	}

	static bool IsServerRunning()
	{
		return (*com_sv_running)->current.enabled;
	}

	static client_s* PlayerByName(const Components::Command::Params* params)
	{
		if (!IsServerRunning())
		{
			return nullptr;
		}

		if (params->Size() < 2)
		{
			Components::Logger::Print("No player specified.\n");
			return nullptr;
		}

		const char* name = params->Get(1);

		for (int i = 0; i < *svs_clientCount; ++i)
		{
			auto* client = &svs_clients[i];

			if (!client->header.state)
			{
				continue;
			}

			if (_stricmp(client->name, name) == 0)
			{
				return client;
			}

			if (_stricmp(Components::TextRenderer::StripColors(client->name).data(), name) == 0)
			{
				return client;
			}
		}

		Components::Logger::Print("Player {} is not on the server\n", name);
		return nullptr;
	}

	static client_s* PlayerByNum(const Components::Command::Params* params)
	{
		if (!IsServerRunning())
		{
			return nullptr;
		}

		if (params->Size() < 2)
		{
			Components::Logger::Print("No player specified.\n");
			return nullptr;
		}

		const char* text = params->Get(1);

		for (const char* digit = text; *digit; ++digit)
		{
			if (*digit < '0' || *digit > '9')
			{
				Components::Logger::Print("Bad slot number: {}\n", text);
				return nullptr;
			}
		}

		const int slot = std::atoi(text);

		if (slot < 0 || slot >= *svs_clientCount)
		{
			Components::Logger::Print("Bad client slot: {}\n", slot);
			return nullptr;
		}

		auto* client = &svs_clients[slot];

		if (!client->header.state)
		{
			Components::Logger::Print("Client {} is not active\n", slot);
			return nullptr;
		}

		return client;
	}

	static bool Kick(client_s* client, const char* reason, std::string* name, std::uint64_t* xuid)
	{
		if (client->header.netchan.remoteAddress.type == NA_LOOPBACK)
		{
			if ((*Global<dvar_t*>(xblive_privatematchDvar))->current.enabled || !(*Global<dvar_t*>(onlinegameDvar))->current.enabled)
			{
				SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, "e \"EXE_CANNOTKICKHOSTPLAYER\"");
				return false;
			}

			if (!*Global<bool>(migrationFlag))
			{
				reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(SV_MigrationStart))(reason);
			}

			return true;
		}

		if (name)
		{
			*name = Components::TextRenderer::StripColors(client->name);
		}

		if (xuid)
		{
			*xuid = client->steamID;
		}

		SV_DropClient(client, reason, true);
		client->lastPacketTime = *svs_time;
		return true;
	}

	static const char* KickReason(const Components::Command::Params* params)
	{
		if (params->Size() == 3)
		{
			return params->Get(2);
		}

		return "EXE_PLAYERKICKED";
	}

	static bool KickByName(const Components::Command::Params* params, std::string* name, std::uint64_t* xuid)
	{
		if (!IsServerRunning())
		{
			Components::Logger::Print("Server is not running.\n");
			return false;
		}

		if (params->Size() < 2)
		{
			Components::Logger::Print("Usage: {0} <player name> <optional reason>\n{0} all = kick everyone\n", params->Get(0));
			return false;
		}

		const char* reason = KickReason(params);
		auto* client = PlayerByName(params);

		if (client)
		{
			return Kick(client, reason, name, xuid);
		}

		if (_stricmp(params->Get(1), "all") == 0)
		{
			for (int i = 0; i < *svs_clientCount; ++i)
			{
				if (svs_clients[i].header.state)
				{
					Kick(&svs_clients[i], reason, nullptr, nullptr);
				}
			}
		}

		return false;
	}

	static bool KickByNum(const Components::Command::Params* params, std::string* name, std::uint64_t* xuid)
	{
		if (!IsServerRunning())
		{
			Components::Logger::Print("Server is not running.\n");
			return false;
		}

		if (params->Size() < 2)
		{
			Components::Logger::Print("Usage: {} <client number> <optional reason>\n", params->Get(0));
			return false;
		}

		const char* reason = KickReason(params);
		auto* client = PlayerByNum(params);

		if (!client)
		{
			return false;
		}

		return Kick(client, reason, name, xuid);
	}

	static void BanKicked(bool isKicked, const std::string& name, std::uint64_t xuid)
	{
		if (!isKicked || !xuid)
		{
			return;
		}

		Components::Logger::Print("{} (guid \"{:X}\") was kicked for cheating\n", name, xuid);
		AddTempBan(xuid);
	}

	bool IsTempBanned(std::uint64_t xuid)
	{
		if (!xuid)
		{
			return false;
		}

		const float banTime = (*Global<dvar_t*>(sv_kickBanTimeDvar))->current.value * 1000.0f;

		for (const auto& ban : tempBans)
		{
			if (ban.xuid == xuid && static_cast<float>(*svs_time - ban.time) <= banTime)
			{
				return true;
			}
		}

		return false;
	}

	template <typename T>
	static T ClientField(const client_s* client, std::size_t offset)
	{
		return *reinterpret_cast<const T*>(reinterpret_cast<const std::uint8_t*>(client) + offset);
	}

	static void InfoPrint(const char* info)
	{
		const char* at = info;

		if (*at == '\\')
		{
			++at;
		}

		std::string out;

		while (*at)
		{
			std::string key;

			while (*at && *at != '\\')
			{
				key.push_back(*at);
				++at;
			}

			if (key.size() < 20)
			{
				key.resize(20, ' ');
			}

			out.append(key);

			if (!*at)
			{
				out.append("MISSING VALUE\n");
				break;
			}

			++at;

			while (*at && *at != '\\')
			{
				out.push_back(*at);
				++at;
			}

			if (*at)
			{
				++at;
			}

			out.push_back('\n');
		}

		Components::Logger::Print("{}", out);
	}

	static void StatusCommand()
	{
		if (!IsServerRunning())
		{
			Components::Logger::Print("Server is not running.\n");
			return;
		}

		std::string out = std::format("map: {}\n", (*Global<dvar_t*>(mapnameDvar))->current.string);
		out.append("num score ping guid                             name            lastmsg address               qport rate\n");
		out.append("--- ----- ---- -------------------------------- --------------- ------- --------------------- ----- -----\n");

		for (int i = 0; i < *svs_clientCount; ++i)
		{
			const auto* client = &svs_clients[i];

			if (!client->header.state)
			{
				continue;
			}

			out.append(std::format("{:3} {:5} ", i, G_GetClientScore(i)));

			if (client->header.state == CS_CONNECTED)
			{
				out.append("CNCT ");
			}
			else if (client->header.state == CS_ZOMBIE)
			{
				out.append("ZMBI ");
			}
			else
			{
				out.append(std::format("{:4} ", std::min(client->ping, 9999)));
			}

			out.append(std::format("{:>32} ", std::format("{:X}", client->steamID)));

			out.append(client->name);
			out.append("^7");
			const auto nameLength = Components::TextRenderer::StripColors(client->name).size();

			if (nameLength < 16)
			{
				out.append(16 - nameLength, ' ');
			}

			out.append(std::format("{:7} ", *svs_time - client->lastPacketTime));

			const std::string address = Components::Network::AdrToString(client->header.netchan.remoteAddress);
			out.append(address);

			if (address.size() < 22)
			{
				out.append(22 - address.size(), ' ');
			}

			out.append(std::format("{:5} {:5}\n", ClientField<int>(client, clientQport), ClientField<int>(client, clientRate)));
		}

		out.push_back('\n');
		Components::Logger::Print("{}", out);
	}

	static void ServerInfoCommand()
	{
		Components::Logger::Print("Server info settings:\n");
		InfoPrint(reinterpret_cast<const char*(*)(int, int)>(Utils::Hook::Rebase(Dvar_InfoString))(0, DVAR_SERVERINFO));
	}

	static void DumpUserCommand(const Components::Command::Params* params)
	{
		if (!IsServerRunning())
		{
			Components::Logger::Print("Server is not running.\n");
			return;
		}

		if (params->Size() != 2)
		{
			Components::Logger::Print("Usage: info <userid>\n");
			return;
		}

		const auto* client = PlayerByName(params);

		if (!client)
		{
			return;
		}

		Components::Logger::Print("userinfo\n--------\n");
		InfoPrint(client->userinfo);
	}

	static void HeartbeatCommand()
	{
		if (!Components::Dedicated::IsEnabled() || Components::ZoneBuilder::IsEnabled())
		{
			return;
		}

		Components::Dedicated::Heartbeat();
	}

	static void DropClientIfInactiveCommand(const Components::Command::Params* params)
	{
		const int clientNum = std::atol(params->Get(1));

		if (clientNum < 0 || clientNum >= *svs_clientCount)
		{
			Components::Logger::Print("dropclientifinactive: client index {} out of range (max {})\n", clientNum, *svs_clientCount);
			return;
		}

		auto* client = &svs_clients[clientNum];

		if (client->header.state == CS_FREE)
		{
			Components::Logger::Print("Unregistering client {} from session because they are CS_FREE in SV_DropClientIfInactive.\n", clientNum);

			if (static_cast<std::size_t>(clientNum) < Components::ClientSlots::BASEGAME_CLIENT_LIMIT)
			{
				PartyHost_RemovePlayer(g_lobbyData, clientNum, false, "");
			}

			return;
		}

		const Components::Dvar::Var sv_rejoinTimeout("sv_rejoinTimeout");
		const int inactiveMs = *svs_time - client->lastPacketTime;

		if (inactiveMs < 1000 * sv_rejoinTimeout.Get<int>())
		{
			return;
		}

		SV_DropClient(client, "EXE_TIMEDOUT", false);
	}

	constexpr unsigned int perkCodeCount = 36;
	constexpr std::size_t playerStatePerks = 0x428;
	constexpr std::size_t clientStatePerks = 0x60;

	static void SetPerkCommand(const Components::Command::Params* params)
	{
		auto* client = PlayerByName(params);

		if (!client)
		{
			return;
		}

		const char* perkName = params->Get(2);
		const auto perkIndex = BG_GetPerkCodeIndexForName(perkName);

		if (perkIndex >= perkCodeCount)
		{
			Components::Logger::Print("Unknown perk: {}\n", perkName);
			return;
		}

		const auto clientNum = static_cast<int>(client - svs_clients);
		const auto word = perkIndex >> 5;
		const auto bit = 1u << (perkIndex & 31);

		auto* playerState = reinterpret_cast<std::uint8_t*>(SV_GetPlayerstateForClientNum(clientNum));
		reinterpret_cast<std::uint32_t*>(playerState + playerStatePerks)[word] |= bit;

		auto* clientState = reinterpret_cast<std::uint8_t*>(G_GetClientState(clientNum));
		reinterpret_cast<std::uint32_t*>(clientState + clientStatePerks)[word] |= bit;
	}

	static void ConnectStringCommand(const Components::Command::Params* params)
	{
		int clientNum = 0;

		if (params->Size() > 1)
		{
			clientNum = std::atol(params->Get(1));
		}

		Components::Logger::Print("{} \n", clientNum);
	}

	void AddOperatorCommands()
	{
		Components::Command::AddSV("status", [](const Components::Command::Params*)
		{
			StatusCommand();
		});

		Components::Command::AddSV("serverinfo", [](const Components::Command::Params*)
		{
			ServerInfoCommand();
		});

		Components::Command::AddSV("dumpuser", DumpUserCommand);

		Components::Command::AddSV("killserver", [](const Components::Command::Params*)
		{
			reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Com_Shutdown))("EXE_SERVERKILLED");
		});

		const auto kickAndBan = [](const Components::Command::Params* params)
		{
			std::string name;
			std::uint64_t xuid = 0;
			const bool isKicked = KickByName(params, &name, &xuid);
			BanKicked(isKicked, name, xuid);
		};

		Components::Command::AddSV("kick", kickAndBan);
		Components::Command::AddSV("tempBanUser", kickAndBan);

		Components::Command::AddSV("onlykick", [](const Components::Command::Params* params)
		{
			KickByName(params, nullptr, nullptr);
		});

		Components::Command::AddSV("clientkick", [](const Components::Command::Params* params)
		{
			KickByNum(params, nullptr, nullptr);
		});

		Components::Command::AddSV("tempBanClient", [](const Components::Command::Params* params)
		{
			std::string name;
			std::uint64_t xuid = 0;
			const bool isKicked = KickByNum(params, &name, &xuid);
			BanKicked(isKicked, name, xuid);
		});

		Components::Command::AddSV("map_restart", [](const Components::Command::Params*)
		{
			MapRestartCommand();
		});

		Components::Command::AddSV("fast_restart", [](const Components::Command::Params*)
		{
			FastRestartCommand();
		});

		Components::Command::AddSV("map", MapCommand);
		Components::Command::AddSV("devmap", DevmapCommand);

		Components::Command::AddSV("heartbeat", [](const Components::Command::Params*)
		{
			HeartbeatCommand();
		});

		Components::Command::AddSV("dropclientifinactive", DropClientIfInactiveCommand);

		Components::Command::AddSV("gameCompleteStatus", [](const Components::Command::Params*)
		{
		});

		Components::Command::AddSV("setPerk", SetPerkCommand);
		Components::Command::AddSV("connectString", ConnectStringCommand);
	}
}
