#include <Utils/InfoString.hpp>

#include "Friends.hpp"
#include "Events.hpp"
#include "Gamepad.hpp"
#include "Party.hpp"
#include "ServerInfo.hpp"
#include "ServerList.hpp"
#include "UIFeeder.hpp"
#include "Voice.hpp"
#include "Bots.hpp"
#include "GSC/Field.hpp"

#include <version.hpp>

#include <set>
#include <array>
#include <unordered_map>
#include <algorithm>

#define SCOREBOARD_FEEDER 71

namespace Components
{
	ServerInfo::Container ServerInfo::PlayerContainer;
	static int ListenServerHostClientNum = -1;
	static std::array<int, Game::MAX_CLIENTS> ClientDownStateSuppressedUntil{};

	static void ResetClientTransientScoreboardState(const int clientNum, const int suppressMilliseconds = 1000)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		Dvar::Var(Utils::String::VA("zw3_sb_down_%d", clientNum)).set(0);
		Dvar::Var(Utils::String::VA("zw3_sb_down_progress_%d", clientNum)).set(0.0f);
		ClientDownStateSuppressedUntil[clientNum] = Game::Sys_Milliseconds() + suppressMilliseconds;
	}

	unsigned int ServerInfo::GetPlayerCount()
	{
		return PlayerContainer.playerList.size();
	}

	const char* ServerInfo::GetPlayerText(unsigned int index, int column)
	{
		if (index >= PlayerContainer.playerList.size())
		{
			return "";
		}

		const auto& player = PlayerContainer.playerList[index];

		switch (column)
		{
		case 0:
			return player.name.data();

		case 1:
			return Utils::String::VA("%d", player.score);

		case 2:
			return Utils::String::VA("%d", player.kills);

		case 3:
			return Utils::String::VA("%d", player.downs);

		case 4:
			return Utils::String::VA("%d", player.revives);

		case 5:
			return Utils::String::VA("%d", player.deaths);

		case 6:
			return Utils::String::VA("%d", player.ping);

		default:
			break;
		}

		return "";
	}

	void ServerInfo::SelectPlayer(unsigned int index)
	{
		PlayerContainer.currentPlayer = index;
	}

	void ServerInfo::ServerStatus([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		PlayerContainer.currentPlayer = 0;
		PlayerContainer.playerList.clear();

		auto* serverInfo = ServerList::GetCurrentServer();

		if (info && serverInfo)
		{
			Dvar::Var("uiSi_ServerName").set(serverInfo->hostname);
			Dvar::Var("uiSi_MaxClients").set(serverInfo->clients);
			Dvar::Var("uiSi_Version").set(serverInfo->version);
			Dvar::Var("uiSi_SecurityLevel").set(serverInfo->securityLevel);
			Dvar::Var("uiSi_isPrivate").set(serverInfo->password ? "@MENU_YES" : "@MENU_NO");
			Dvar::Var("uiSi_Hardcore").set(serverInfo->hardcore ? "@MENU_ENABLED" : "@MENU_DISABLED");
			Dvar::Var("uiSi_KillCam").set("@MENU_NO");
			Dvar::Var("uiSi_ffType").set("@MENU_DISABLED");
			Dvar::Var("uiSi_MapName").set(serverInfo->mapname);
			Dvar::Var("uiSi_MapNameLoc").set(Localization::LocalizeMapName(serverInfo->mapname.data()));
			Dvar::Var("uiSi_GameType").set(Game::UI_LocalizeGameType(serverInfo->gametype.data()));
			Dvar::Var("uiSi_ModName").set("");
			Dvar::Var("uiSi_aimAssist").set(serverInfo->aimassist ? "@MENU_YES" : "@MENU_NO");
			Dvar::Var("uiSi_voiceChat").set(serverInfo->voice ? "@MENU_YES" : "@MENU_NO");

			if (serverInfo->mod.size() > 5)
			{
				Dvar::Var("uiSi_ModName").set(serverInfo->mod.data() + 5);
			}

			PlayerContainer.target = serverInfo->addr;
			Network::SendCommand(PlayerContainer.target, "getstatus");
		}
	}

	void ServerInfo::DrawScoreboardInfo(int localClientNum)
	{
		Game::Font_s* font = Game::R_RegisterFont("fonts/bigfont", 0);
		const auto* cxt = Game::ScrPlace_GetActivePlacement(localClientNum);

		auto addressText = Network::Address(*Game::connectedHost).getString();
		if (addressText == "0.0.0.0:0"s || addressText == "loopback"s)
		{
			addressText = "Listen Server"s;
		}

		auto y = (480.0f - (*Game::cg_scoreboardHeight)->current.value) * 0.5f;
		y += (*Game::cg_scoreboardHeight)->current.value + 6.0f;

		const auto x = 320.0f - (*Game::cg_scoreboardWidth)->current.value * 0.5f;
		const auto x2 = 320.0f + (*Game::cg_scoreboardWidth)->current.value * 0.5f;

		if (!Friends::UIStreamFriendly.get<bool>())
		{
			constexpr auto fontSize = 0.35f;
			Game::UI_DrawText(cxt, reinterpret_cast<const char*>(0x7ED3F8), std::numeric_limits<int>::max(), font, x, y, 0, 0, fontSize, reinterpret_cast<float*>(0x747F34), 3);
			Game::UI_DrawText(cxt, addressText.data(), std::numeric_limits<int>::max(), font, x2 - Game::UI_TextWidth(addressText.data(), 0, font, fontSize), y, 0, 0, fontSize, reinterpret_cast<float*>(0x747F34), 3);
		}
	}

	__declspec(naked) void ServerInfo::DrawScoreboardStub()
	{
		__asm
		{
			pushad
			push eax
			call DrawScoreboardInfo
			pop eax
			popad

			push 591B70h
			retn
		}
	}

	Utils::InfoString ServerInfo::GetHostInfo()
	{
		Utils::InfoString info;

		info.set("admin", Dvar::Var("_Admin").get<std::string>());
		info.set("website", Dvar::Var("_Website").get<std::string>());
		info.set("email", Dvar::Var("_Email").get<std::string>());
		info.set("location", Dvar::Var("_Location").get<std::string>());

		return info;
	}

	Utils::InfoString ServerInfo::GetInfo()
	{
		auto maxClientCount = *Game::svs_clientCount;
		const auto* password = *Game::g_password ? (*Game::g_password)->current.string : "";

		if (!maxClientCount)
		{
			maxClientCount = *Game::party_maxplayers ? (*Game::party_maxplayers)->current.integer : 18;
		}

		Utils::InfoString info(Game::Dvar_InfoString_Big(Game::DVAR_SERVERINFO));
		info.set("gamename", "IW4");
		info.set("sv_maxclients", std::to_string(maxClientCount));
		info.set("protocol", std::to_string(PROTOCOL));
		info.set("version", REVISION_STR);
		info.set("version", (*Game::version)->current.string);
		info.set("mapname", (*Game::sv_mapname)->current.string);
		info.set("isPrivate", *password ? "1" : "0");
		info.set("checksum", Utils::String::VA("%X", Utils::Cryptography::JenkinsOneAtATime::Compute(std::to_string(Game::Sys_Milliseconds()))));
		info.set("aimAssist", (Gamepad::sv_allowAimAssist.get<bool>() ? "1" : "0"));
		info.set("voiceChat", (Voice::SV_VoiceEnabled() ? "1" : "0"));

		if (info.get("mapname").empty())
		{
			info.set("mapname", (*Game::ui_mapname)->current.string);
		}

		if (Party::IsEnabled() && Dvar::Var("party_host").get<bool>())
		{
			info.set("matchtype", "1");
		}
		else if (Dedicated::IsRunning())
		{
			info.set("matchtype", "2");
		}
		else
		{
			info.set("matchtype", "0");
		}

		return info;
	}

	static std::string GetScoreboardBaseName(std::string name)
	{
		Utils::String::Replace(name, "^1[DEAD] ^7", "");
		Utils::String::Replace(name, "^3[DOWN] ^7", "");
		Utils::String::Replace(name, "[DEAD] ", "");
		Utils::String::Replace(name, "[DOWN] ", "");

		return name;
	}

	static std::string GetScoreboardStatusName(const bool dead, const bool down)
	{
		if (dead)
		{
			return "DEAD";
		}

		if (down)
		{
			return "DOWN";
		}

		return "";
	}

	static std::string NormalizeIdentityName(const std::string& name)
	{
		std::string result;
		result.reserve(name.size());

		for (std::size_t i = 0; i < name.size(); ++i)
		{
			if (name[i] == '^' && i + 1 < name.size())
			{
				++i;
				continue;
			}

			result.push_back(name[i]);
		}

		const auto first = result.find_first_not_of(" 	");
		if (first == std::string::npos)
		{
			return "";
		}

		const auto last = result.find_last_not_of(" 	");
		return result.substr(first, last - first + 1);
	}


	static void UpdateListenServerHostClientNum()
	{
		if (Dedicated::IsRunning() || !Dvar::Var("party_host").get<bool>())
		{
			ListenServerHostClientNum = -1;
			return;
		}

		if (ListenServerHostClientNum >= 0 && ListenServerHostClientNum < Game::MAX_CLIENTS)
		{
			const auto& cached = Game::svs_clients[ListenServerHostClientNum];
			if (cached.header.state >= Game::CS_CONNECTED && !cached.bIsTestClient)
			{
				return;
			}
		}

		ListenServerHostClientNum = -1;

		std::vector<int> realClientNums;
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state >= Game::CS_CONNECTED && !client.bIsTestClient)
			{
				realClientNums.push_back(clientNum);
			}
		}

		if (realClientNums.empty())
		{
			return;
		}

		if (realClientNums.size() == 1)
		{
			ListenServerHostClientNum = realClientNums.front();
			return;
		}

		const auto localHostName = NormalizeIdentityName(Dvar::Var("name").get<std::string>());
		int bestClientNum = -1;

		for (const auto clientNum : realClientNums)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (_stricmp(NormalizeIdentityName(client.name).c_str(), localHostName.c_str()) != 0)
			{
				continue;
			}

			if (bestClientNum == -1)
			{
				bestClientNum = clientNum;
				continue;
			}

			const auto& best = Game::svs_clients[bestClientNum];
			if (client.header.state > best.header.state ||
				(client.header.state == best.header.state && client.ping < best.ping) ||
				(client.header.state == best.header.state && client.ping == best.ping && clientNum < bestClientNum))
			{
				bestClientNum = clientNum;
			}
		}

		if (bestClientNum == -1)
		{
			bestClientNum = *std::min_element(realClientNums.begin(), realClientNums.end(),
				[](const int a, const int b)
				{
					const auto& clientA = Game::svs_clients[a];
					const auto& clientB = Game::svs_clients[b];

					if (clientA.header.state != clientB.header.state)
					{
						return clientA.header.state > clientB.header.state;
					}

					if (clientA.ping != clientB.ping)
					{
						return clientA.ping < clientB.ping;
					}

					return a < b;
				});
		}

		ListenServerHostClientNum = bestClientNum;
	}


	static bool IsBotDisplayName(const std::string& name)
	{
		const auto normalized = NormalizeIdentityName(name);
		return normalized.size() >= 6 && _strnicmp(normalized.c_str(), "[BOT] ", 6) == 0;
	}

	static std::string CanonicalCharacterName(const std::string& value)
	{
		const auto normalized = NormalizeIdentityName(value);

		if (_stricmp(normalized.c_str(), "Richtofen") == 0)
		{
			return "Richtofen";
		}

		if (_stricmp(normalized.c_str(), "Dempsey") == 0)
		{
			return "Dempsey";
		}

		if (_stricmp(normalized.c_str(), "Nikolai") == 0)
		{
			return "Nikolai";
		}

		if (_stricmp(normalized.c_str(), "Takeo") == 0)
		{
			return "Takeo";
		}

		return {};
	}

	static std::string GetCharacterForClient(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return {};
		}

		const auto* characterDvar = Game::Dvar_FindVar(Utils::String::VA("zw3_character_client_%d", clientNum));
		if (!characterDvar || !characterDvar->current.string || !*characterDvar->current.string)
		{
			return {};
		}

		return CanonicalCharacterName(characterDvar->current.string);
	}

	static int GetDvarIntStringSafe(const char* name)
	{
		const auto value = Dvar::Var(name).get<std::string>();
		return std::strtol(value.data(), nullptr, 10);
	}

	static float GetDvarFloatStringSafe(const char* name)
	{
		const auto value = Dvar::Var(name).get<std::string>();
		return static_cast<float>(std::atof(value.data()));
	}

	void ServerInfo::NormalisePlayerDownState(Container::Player& player)
	{
		const auto isDead = player.status == "DEAD";

		if (isDead)
		{
			player.down = 0;
			player.downProgress = 0.0f;
			return;
		}

		if (player.status == "CONNECTING")
		{
			player.down = 0;
			player.downProgress = 0.0f;
			return;
		}

		if (player.down == 1 || player.status == "DOWN")
		{
			player.down = 1;
			player.downProgress = std::clamp(player.downProgress, 0.0f, 1.0f);

			if (player.downProgress <= 0.0f)
			{
				player.downProgress = 1.0f;
			}

			player.status = "DOWN";
			return;
		}

		player.down = 0;
		player.downProgress = 0.0f;
		player.status.clear();
	}

	static void ClearScoreboardRowDvars()
	{
		for (int i = 0; i < 4; ++i)
		{
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_name", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_score", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_kills", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_downs", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_revives", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_deaths", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_ping", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_icon", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_status", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_survival_time", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_down", i)).set(0);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_down_progress", i)).set(0.0f);
		}
	}

	void ServerInfo::WriteScoreboardRowDvars()
	{
		Dvar::Var("zw3_ui_sb_player_count").set(static_cast<int>(ServerInfo::PlayerContainer.playerList.size()));

		ClearScoreboardRowDvars();

		for (std::size_t i = 0; i < ServerInfo::PlayerContainer.playerList.size() && i < 4; ++i)
		{
			const auto& player = ServerInfo::PlayerContainer.playerList[i];

			const auto isDead = player.status == "DEAD";
			const auto isDown = !isDead && player.down == 1;
			const auto downProgress = isDown ? std::clamp(player.downProgress, 0.0f, 1.0f) : 0.0f;

			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_name", static_cast<int>(i))).set(player.name);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_score", static_cast<int>(i))).set(player.score);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_kills", static_cast<int>(i))).set(player.kills);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_downs", static_cast<int>(i))).set(player.downs);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_revives", static_cast<int>(i))).set(player.revives);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_deaths", static_cast<int>(i))).set(player.deaths);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_ping", static_cast<int>(i))).set(player.ping);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_icon", static_cast<int>(i))).set(player.icon);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_status", static_cast<int>(i))).set(player.status);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_survival_time", static_cast<int>(i))).set(player.survivalTime);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_down", static_cast<int>(i))).set(isDown ? 1 : 0);
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_down_progress", static_cast<int>(i))).set(downProgress);
		}
	}

	void ServerInfo::ApplyScoreboardSnapshot(const std::string& data)
	{
		ServerInfo::PlayerContainer.currentPlayer = 0;
		ServerInfo::PlayerContainer.playerList.clear();

		const auto lines = Utils::String::Split(data, '\n');
		std::string snapshotSurvivalTime;

		for (const auto& line : lines)
		{
			if (line.empty())
				continue;

			Utils::InfoString info(line);

			Container::Player player;
			player.name = info.get("name");
			player.score = std::strtol(info.get("score").data(), nullptr, 10);
			player.kills = std::strtol(info.get("kills").data(), nullptr, 10);
			player.downs = std::strtol(info.get("downs").data(), nullptr, 10);
			player.revives = std::strtol(info.get("revives").data(), nullptr, 10);
			player.deaths = std::strtol(info.get("deaths").data(), nullptr, 10);
			player.ping = std::strtol(info.get("ping").data(), nullptr, 10);
			player.icon = info.get("icon");
			player.status = info.get("status");
			player.down = std::strtol(info.get("down").data(), nullptr, 10);
			player.downProgress = static_cast<float>(std::atof(info.get("downProgress").data()));
			player.survivalTime = info.get("survivalTime");

			if (snapshotSurvivalTime.empty() && !player.survivalTime.empty())
			{
				snapshotSurvivalTime = player.survivalTime;
			}

			NormalisePlayerDownState(player);

			ServerInfo::PlayerContainer.playerList.push_back(player);
		}

		const auto currentSurvivalTime =
			Dvar::Var("zw3_ui_sb_survived_time").get<std::string>();
		if (currentSurvivalTime != snapshotSurvivalTime)
		{
			Dvar::Var("zw3_ui_sb_survived_time").set(snapshotSurvivalTime);
		}

		WriteScoreboardRowDvars();
	}

	ServerInfo::ServerInfo()
	{
		PlayerContainer.currentPlayer = 0;

		Utils::Hook::Nop(0x4FC6EA, 5);

		//Utils::Hook(0x4FC6EA, DrawScoreboardStub, HOOK_CALL).install()->quick();

		Utils::Hook::Nop(0x62654E, 6);

		UIScript::Add("ServerStatus", ServerStatus);
		UIScript::Add("RefreshScoreboard", RefreshScoreboard);

		UIFeeder::Add(13.0f, GetPlayerCount, GetPlayerText, SelectPlayer);

		Events::OnClientDisconnect([](const int clientNum)
			{
				GSC::Field::ResetClientScoreboardStats(clientNum);
				ResetClientTransientScoreboardState(clientNum, 1500);

				if (auto* characterDvar = Game::Dvar_FindVar(
					Utils::String::VA("zw3_character_client_%d", clientNum)))
				{
					Game::Dvar_SetString(characterDvar, "None");
				}

				if (ListenServerHostClientNum == clientNum)
				{
					ListenServerHostClientNum = -1;
				}
			});

		Components::Scheduler::Loop([]()
			{
				static std::array<int, Game::MAX_CLIENTS> previousStates{};
				static std::array<bool, Game::MAX_CLIENTS> previousBotFlags{};
				static std::array<std::string, Game::MAX_CLIENTS> previousNames{};
				static bool initialized = false;

				for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
				{
					const auto state = Game::svs_clients[clientNum].header.state;
					const bool hasOccupant = state >= Game::CS_CONNECTED;
					const bool hadOccupant = initialized && previousStates[clientNum] >= Game::CS_CONNECTED;
					const bool isBot = hasOccupant && Game::svs_clients[clientNum].bIsTestClient;
					const std::string currentName = hasOccupant ? Game::svs_clients[clientNum].name : "";

					const bool becameConnected = hasOccupant && !hadOccupant;
					const bool becameActive = initialized && state >= Game::CS_ACTIVE &&
						previousStates[clientNum] < Game::CS_ACTIVE;
					const bool occupantTypeChanged = hasOccupant && hadOccupant &&
						previousBotFlags[clientNum] != isBot;
					const bool occupantNameChanged = hasOccupant && hadOccupant &&
						_stricmp(NormalizeIdentityName(previousNames[clientNum]).c_str(),
							NormalizeIdentityName(currentName).c_str()) != 0;
					const bool departed = !hasOccupant && hadOccupant;

					if (departed || becameConnected || becameActive ||
						occupantTypeChanged || occupantNameChanged)
					{
						ResetClientTransientScoreboardState(clientNum, 1500);
					}

					if (departed || (!isBot &&
						(becameConnected || occupantTypeChanged || occupantNameChanged)))
					{
						if (auto* characterDvar = Game::Dvar_FindVar(
							Utils::String::VA("zw3_character_client_%d", clientNum)))
						{
							Game::Dvar_SetString(characterDvar, "None");
						}
					}

					if (hasOccupant && !isBot && state < Game::CS_ACTIVE)
					{
						ResetClientTransientScoreboardState(clientNum, 1500);
					}

					previousStates[clientNum] = state;
					previousBotFlags[clientNum] = isBot;
					previousNames[clientNum] = currentName;
				}

				initialized = true;
			}, Components::Scheduler::Pipeline::MAIN);

		Components::Scheduler::Loop([]()
			{
				static int lastRefresh = 0;

				UpdateListenServerHostClientNum();

				auto* scoreboard = Game::Menus_FindByName(Game::uiContext, "scoreboard");
				if (!scoreboard || !Game::Menu_IsVisible(Game::uiContext, scoreboard))
				{
					return;
				}

				const auto now = Game::Sys_Milliseconds();
				if (now - lastRefresh < 250)
				{
					return;
				}

				lastRefresh = now;

				ServerInfo::RefreshScoreboard(UIScript::Token(), nullptr);
			}, Components::Scheduler::Pipeline::MAIN);

		Network::OnClientPacket("getStatus", [](const Network::Address& address, [[maybe_unused]] const std::string& data)
			{
				std::string playerList;

				Utils::InfoString info = GetInfo();
				info.set("challenge", Utils::ParseChallenge(data));

				for (std::size_t i = 0; i < Game::MAX_CLIENTS; ++i)
				{
					auto score = 0;
					auto ping = 0;
					std::string name;

					if (Dedicated::IsRunning())
					{
						if (Game::svs_clients[i].header.state < Game::CS_ACTIVE) continue;
						if (!Game::svs_clients[i].gentity || !Game::svs_clients[i].gentity->client) continue;

						if (Game::svs_clients[i].bIsTestClient)
						{
							continue;
						}

						const auto clientNum = static_cast<int>(i);
						score = Game::SV_GameClientNum_Score(clientNum);
						ping = Game::svs_clients[i].ping;
						name = Game::svs_clients[i].name;
					}
					else
					{
						const auto* namePtr = Game::PartyHost_GetMemberName(reinterpret_cast<Game::PartyData*>(0x1081C00), i);
						if (!namePtr || !*namePtr) continue;

						name = namePtr;
					}

					playerList.append(std::format("{} {} \"{}\"\n", score, ping, name));
				}

				Network::SendCommand(address, "statusResponse", info.build() + "\n"s + playerList + "\n"s);
			});

		Network::OnClientPacket("statusResponse", [](const Network::Address& address, [[maybe_unused]] const std::string& data)
			{
				if (PlayerContainer.target != address)
				{
					return;
				}

				const auto pos = data.find_first_of('\n');
				if (pos == std::string::npos)
				{
					return;
				}

				const Utils::InfoString info(data.substr(0, pos));

				Dvar::Var("uiSi_ServerName").set(info.get("sv_hostname"));
				Dvar::Var("uiSi_MaxClients").set(info.get("sv_maxclients"));
				Dvar::Var("uiSi_Version").set(info.get("version"));
				Dvar::Var("uiSi_SecurityLevel").set(info.get("sv_securityLevel"));
				Dvar::Var("uiSi_isPrivate").set(info.get("isPrivate") == "0" ? "@MENU_NO" : "@MENU_YES");
				Dvar::Var("uiSi_Hardcore").set(info.get("g_hardcore") == "0" ? "@MENU_DISABLED" : "@MENU_ENABLED");
				Dvar::Var("uiSi_KillCam").set(info.get("scr_game_allowkillcam") == "0" ? "@MENU_NO" : "@MENU_YES");
				Dvar::Var("uiSi_MapName").set(info.get("mapname"));
				Dvar::Var("uiSi_MapNameLoc").set(Localization::LocalizeMapName(info.get("mapname").data()));
				Dvar::Var("uiSi_GameType").set(Game::UI_LocalizeGameType(info.get("g_gametype").data()));
				Dvar::Var("uiSi_ModName").set("");
				Dvar::Var("uiSi_aimAssist").set(info.get("aimAssist") == "0" ? "@MENU_DISABLED" : "@MENU_ENABLED");
				Dvar::Var("uiSi_voiceChat").set(info.get("voiceChat") == "0" ? "@MENU_DISABLED" : "@MENU_ENABLED");

				switch (std::strtol(info.get("scr_team_fftype").data(), nullptr, 10))
				{
				default:
					Dvar::Var("uiSi_ffType").set("@MENU_DISABLED");
					break;
				case 1:
					Dvar::Var("uiSi_ffType").set("@MENU_ENABLED");
					break;
				case 2:
					Dvar::Var("uiSi_ffType").set("@MPUI_RULES_REFLECT");
					break;
				case 3:
					Dvar::Var("uiSi_ffType").set("@MPUI_RULES_SHARED");
					break;
				}

				if (Utils::String::StartsWith(info.get("fs_game"), "mods/"))
				{
					const auto mod = info.get("fs_game");
					Dvar::Var("uiSi_ModName").set(mod.substr(5));
				}

				const auto lines = Utils::String::Split(data, '\n');

				if (lines.size() <= 1) return;

				for (std::size_t i = 1; i < lines.size(); ++i)
				{
					Container::Player player;

					std::string currentData = lines[i];

					if (currentData.size() < 3) continue;

					player.score = std::strtol(currentData.substr(0, currentData.find_first_of(' ')).data(), nullptr, 10);
					currentData = currentData.substr(currentData.find_first_of(' ') + 1);

					player.ping = std::strtol(currentData.substr(0, currentData.find_first_of(' ')).data(), nullptr, 10);
					currentData = currentData.substr(currentData.find_first_of(' ') + 1);

					if (currentData[0] == '\"')
					{
						currentData = currentData.substr(1);
					}

					if (currentData.back() == '\"')
					{
						currentData.pop_back();
					}

					player.name = currentData;

					PlayerContainer.playerList.push_back(player);
				}
			});

		Network::OnClientPacket("getZW3Scoreboard", [](const Network::Address& address, [[maybe_unused]] const std::string& data)
			{
				if (!Dedicated::IsRunning() && !Dvar::Var("party_host").get<bool>())
					return;

				ServerInfo::RefreshScoreboard(UIScript::Token(), nullptr);

				std::string response;

				const auto survivedTime = Dvar::Var("zw3_ui_sb_survived_time").get<std::string>();

				for (const auto& player : PlayerContainer.playerList)
				{
					const auto isDead = player.status == "DEAD";
					const auto isDown = !isDead && player.down == 1;
					const auto downProgress = isDown ? std::clamp(player.downProgress, 0.0f, 1.0f) : 0.0f;

					Utils::InfoString info;
					info.set("name", player.name);
					info.set("score", std::to_string(player.score));
					info.set("kills", std::to_string(player.kills));
					info.set("downs", std::to_string(player.downs));
					info.set("revives", std::to_string(player.revives));
					info.set("deaths", std::to_string(player.deaths));
					info.set("ping", std::to_string(player.ping));
					info.set("icon", player.icon);
					info.set("status", player.status);
					info.set("down", std::to_string(isDown ? 1 : 0));
					info.set("downProgress", std::to_string(downProgress));
					info.set("survivalTime", survivedTime);

					response.append(info.build());
					response.append("\n");
				}

				Network::SendCommand(address, "zw3ScoreboardResponse", response);
			});

		Network::OnClientPacket("zw3ScoreboardResponse", []([[maybe_unused]] const Network::Address& address, const std::string& data)
			{
				if (Dvar::Var("party_host").get<bool>())
					return;

				ApplyScoreboardSnapshot(data);
			});
	}

	void ServerInfo::RefreshScoreboard([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		if (!Dedicated::IsRunning() && !Dvar::Var("party_host").get<bool>())
		{
			static int lastScoreboardRequest = 0;
			const auto now = Game::Sys_Milliseconds();

			if (now - lastScoreboardRequest >= 250)
			{
				lastScoreboardRequest = now;
				Network::SendCommand(Party::Target(), "getZW3Scoreboard");
			}

			return;
		}

		PlayerContainer.currentPlayer = 0;
		PlayerContainer.playerList.clear();
		UpdateListenServerHostClientNum();

		struct PublishedCharacterSlot
		{
			int index = -1;
			std::string character;
			std::string owner;
			bool bot = false;
		};

		std::vector<PublishedCharacterSlot> publishedSlots;
		publishedSlots.reserve(4);

		for (int slot = 1; slot <= 4; ++slot)
		{
			const auto character = CanonicalCharacterName(
				Dvar::Var(Utils::String::VA("character_%d", slot)).get<std::string>());
			const auto owner = Dvar::Var(
				Utils::String::VA("character_%d_player", slot)).get<std::string>();

			if (character.empty() || owner.empty() || owner == "None")
			{
				continue;
			}

			publishedSlots.push_back({ slot - 1, character, owner, IsBotDisplayName(owner) });
		}

		auto characterFromBotName = [](const std::string& value) -> std::string
			{
				auto normalized = NormalizeIdentityName(GetScoreboardBaseName(value));
				if (normalized.size() >= 6 && _strnicmp(normalized.c_str(), "[BOT] ", 6) == 0)
				{
					normalized.erase(0, 6);
				}

				return CanonicalCharacterName(normalized);
			};

		auto slotIndexForCharacter = [&](const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				for (const auto& slot : publishedSlots)
				{
					if (_stricmp(slot.character.c_str(), character.c_str()) == 0)
					{
						return slot.index;
					}
				}

				return std::numeric_limits<int>::max();
			};

		std::vector<int> realClientOrder;
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state >= Game::CS_CONNECTED && !client.bIsTestClient)
			{
				realClientOrder.push_back(clientNum);
			}
		}

		std::stable_sort(realClientOrder.begin(), realClientOrder.end(), [](const int a, const int b)
			{
				const bool aHost = a == ListenServerHostClientNum;
				const bool bHost = b == ListenServerHostClientNum;
				if (aHost != bHost)
				{
					return aHost;
				}
				return a < b;
			});

		std::vector<std::string> publishedRealCharacters;
		for (const auto& slot : publishedSlots)
		{
			if (!slot.bot)
			{
				publishedRealCharacters.push_back(slot.character);
			}
		}

		std::unordered_map<int, std::string> realFallbackCharacter;
		for (std::size_t i = 0; i < realClientOrder.size() && i < publishedRealCharacters.size(); ++i)
		{
			realFallbackCharacter[realClientOrder[i]] = publishedRealCharacters[i];
		}

		std::vector<std::string> publishedBotCharacters;
		for (const auto& slot : publishedSlots)
		{
			if (!slot.bot)
			{
				continue;
			}

			bool duplicate = false;
			for (const auto& existing : publishedBotCharacters)
			{
				if (_stricmp(existing.c_str(), slot.character.c_str()) == 0)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
			{
				publishedBotCharacters.push_back(slot.character);
			}
		}

		std::vector<int> liveBotClients;
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state >= Game::CS_CONNECTED && client.bIsTestClient)
			{
				liveBotClients.push_back(clientNum);
			}
		}

		std::unordered_map<int, std::string> resolvedBotCharacters;
		std::vector<std::string> claimedBotCharacters;

		auto tryClaimBotCharacter = [&](const int clientNum, const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				if (character.empty())
				{
					return false;
				}

				bool published = false;
				for (const auto& candidate : publishedBotCharacters)
				{
					if (_stricmp(candidate.c_str(), character.c_str()) == 0)
					{
						published = true;
						break;
					}
				}

				if (!published)
				{
					return false;
				}

				for (const auto& claimed : claimedBotCharacters)
				{
					if (_stricmp(claimed.c_str(), character.c_str()) == 0)
					{
						return false;
					}
				}

				resolvedBotCharacters[clientNum] = character;
				claimedBotCharacters.push_back(character);
				return true;
			};

		for (const auto clientNum : liveBotClients)
		{
			tryClaimBotCharacter(clientNum, GetCharacterForClient(clientNum));
		}

		for (const auto clientNum : liveBotClients)
		{
			if (resolvedBotCharacters.contains(clientNum))
			{
				continue;
			}

			auto character = characterFromBotName(Bots::GetBotDisplayName(clientNum));
			if (character.empty())
			{
				character = characterFromBotName(Game::svs_clients[clientNum].name);
			}

			tryClaimBotCharacter(clientNum, character);
		}

		for (const auto clientNum : liveBotClients)
		{
			if (resolvedBotCharacters.contains(clientNum))
			{
				continue;
			}

			for (const auto& character : publishedBotCharacters)
			{
				if (tryClaimBotCharacter(clientNum, character))
				{
					break;
				}
			}
		}

		const auto now = Game::Sys_Milliseconds();

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto state = Game::svs_clients[clientNum].header.state;
			const bool isBot = Game::svs_clients[clientNum].bIsTestClient;

			if (state < Game::CS_CONNECTED)
			{
				GSC::Field::ResetClientScoreboardStats(clientNum);
				ResetClientTransientScoreboardState(clientNum, 0);
				continue;
			}

			if (isBot && state < Game::CS_ACTIVE)
			{
				continue;
			}

			Container::Player player;
			player.clientNum = clientNum;
			player.name = Game::svs_clients[clientNum].name;
			player.ping = Game::svs_clients[clientNum].ping;
			player.survivalTime = Dvar::Var("zw3_ui_sb_survived_time").get<std::string>();

			if (!isBot && state < Game::CS_ACTIVE)
			{
				player.score = 0;
				player.kills = 0;
				player.downs = 0;
				player.revives = 0;
				player.deaths = 0;
				player.down = 0;
				player.downProgress = 0.0f;
				player.status = "CONNECTING";
				ResetClientTransientScoreboardState(clientNum, 1500);
			}
			else
			{
				if (!Game::svs_clients[clientNum].gentity ||
					!Game::svs_clients[clientNum].gentity->client)
				{
					continue;
				}

				const auto* client = Game::svs_clients[clientNum].gentity->client;
				player.score = Game::SV_GameClientNum_Score(clientNum);
				player.kills = client->sess.kills;
				player.downs = GSC::Field::GetClientDowns(clientNum);
				player.revives = GSC::Field::GetClientRevives(clientNum);
				player.deaths = client->sess.deaths;

				const bool dead = client->sess.cs.team == Game::TEAM_SPECTATOR;
				const bool suppressDown = now < ClientDownStateSuppressedUntil[clientNum];
				const auto rawDown = suppressDown ? 0 :
					GetDvarIntStringSafe(Utils::String::VA("zw3_sb_down_%d", clientNum));
				const auto rawDownProgress = suppressDown ? 0.0f :
					GetDvarFloatStringSafe(Utils::String::VA("zw3_sb_down_progress_%d", clientNum));

				player.down = (!dead && rawDown == 1) ? 1 : 0;
				player.downProgress = player.down
					? std::clamp(rawDownProgress, 0.0f, 1.0f)
					: 0.0f;
				player.status = GetScoreboardStatusName(dead, player.down == 1);
				NormalisePlayerDownState(player);
			}

			std::string character;
			if (isBot)
			{
				const auto resolved = resolvedBotCharacters.find(clientNum);
				if (resolved == resolvedBotCharacters.end())
				{
					continue;
				}

				character = resolved->second;
			}
			else
			{
				character = GetCharacterForClient(clientNum);
				if (character.empty())
				{
					const auto fallback = realFallbackCharacter.find(clientNum);
					if (fallback != realFallbackCharacter.end())
					{
						character = fallback->second;
					}
				}
			}

			if (!character.empty())
			{
				player.icon = character;
				if (isBot)
				{
					player.name = Utils::String::VA("[BOT] %s", character.c_str());
				}
				else
				{
					player.name = GetScoreboardBaseName(player.name);
				}
			}
			else
			{
				player.name = GetScoreboardBaseName(player.name);
				player.icon = "hud_status_connecting";
			}

			PlayerContainer.playerList.push_back(player);
		}

		if (PlayerContainer.playerList.empty() && !Dedicated::IsRunning())
		{
			Container::Player host;
			host.clientNum = ListenServerHostClientNum >= 0 ? ListenServerHostClientNum : 0;
			host.name = Dvar::Var("name").get<std::string>();
			host.icon = GetCharacterForClient(host.clientNum);
			if (host.icon.empty() && !publishedRealCharacters.empty())
			{
				host.icon = publishedRealCharacters.front();
			}
			PlayerContainer.playerList.push_back(host);
		}

		std::stable_sort(PlayerContainer.playerList.begin(), PlayerContainer.playerList.end(),
			[&](const Container::Player& a, const Container::Player& b)
			{
				auto group = [&](const Container::Player& player)
					{
						if (!Dedicated::IsRunning() && Dvar::Var("party_host").get<bool>() &&
							player.clientNum == ListenServerHostClientNum)
						{
							return 0;
						}

						const bool bot = player.clientNum >= 0 && player.clientNum < Game::MAX_CLIENTS &&
							Game::svs_clients[player.clientNum].bIsTestClient;
						return bot ? 2 : 1;
					};

				const int aGroup = group(a);
				const int bGroup = group(b);
				if (aGroup != bGroup)
				{
					return aGroup < bGroup;
				}

				const int aSlot = slotIndexForCharacter(a.icon);
				const int bSlot = slotIndexForCharacter(b.icon);

				if (aSlot != bSlot)
				{
					return aSlot < bSlot;
				}

				return a.clientNum < b.clientNum;
			});

		WriteScoreboardRowDvars();
	}

	ServerInfo::~ServerInfo()
	{
		PlayerContainer.playerList.clear();
	}
}
