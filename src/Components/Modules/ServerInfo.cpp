#include <Utils/InfoString.hpp>
#include <Utils/IO.hpp>

#include "Friends.hpp"
#include "Events.hpp"
#include "Gamepad.hpp"
#include "Party.hpp"
#include "ServerInfo.hpp"
#include "CharacterAssignments.hpp"
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
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#define SCOREBOARD_FEEDER 71

namespace Components
{
	ServerInfo::Container ServerInfo::PlayerContainer;
	static int ListenServerHostClientNum = -1;
	static std::array<int, Game::MAX_CLIENTS> ClientDownStateSuppressedUntil{};
	static std::array<int, Game::MAX_CLIENTS> ClientConnectingLastPacketTime{};
	static std::array<int, Game::MAX_CLIENTS> ClientConnectingLastPacketAdvanceAt{};
	static std::array<bool, Game::MAX_CLIENTS> ClientConnectingStale{};
	static constexpr int ConnectingPacketTimeout = 10000;

	static constexpr int ZombieRankRefreshInterval = 1000;

	struct ZombieRankState
	{
		std::uint64_t identity = 0;
		int level = -1;
		int prestige = 0;
		int nextReadAt = 0;
	};

	static std::array<ZombieRankState, Game::MAX_CLIENTS> ClientZombieRanks{};

	static void ResetClientZombieRankState(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		ClientZombieRanks[clientNum] = {};
	}

	static bool TryParseZombieRankValue(const std::string& data, const std::string_view field, int& value)
	{
		const auto fieldPosition = data.find(field);
		if (fieldPosition == std::string::npos)
		{
			return false;
		}

		auto valuePosition = fieldPosition + field.size();
		while (valuePosition < data.size() &&
			(data[valuePosition] == ':' ||
				data[valuePosition] == ' ' ||
				data[valuePosition] == '\t'))
		{
			++valuePosition;
		}

		if (valuePosition >= data.size())
		{
			return false;
		}

		char* end = nullptr;
		const auto parsed = std::strtol(data.c_str() + valuePosition, &end, 10);
		if (end == data.c_str() + valuePosition)
		{
			return false;
		}

		value = static_cast<int>(parsed);
		return true;
	}

	static void AddZombieRankGuidCandidate(std::vector<std::string>& candidates, std::string candidate)
	{
		if (candidate.empty() ||
			std::find(candidates.begin(), candidates.end(), candidate) != candidates.end())
		{
			return;
		}

		candidates.push_back(std::move(candidate));
	}

	static void AddZombieRankGuidCandidates(std::vector<std::string>& candidates, const std::uint64_t guid)
	{
		if (guid == 0)
		{
			return;
		}

		AddZombieRankGuidCandidate(candidates, std::to_string(guid));
		AddZombieRankGuidCandidate(candidates,
			std::to_string(static_cast<std::int64_t>(guid)));

		std::string fullHex = Utils::String::VA("%llX",
			static_cast<unsigned long long>(guid));
		AddZombieRankGuidCandidate(candidates, fullHex);

		std::transform(fullHex.begin(), fullHex.end(), fullHex.begin(),
			[](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		AddZombieRankGuidCandidate(candidates, fullHex);

		const auto lowGuid = static_cast<std::uint32_t>(guid);
		AddZombieRankGuidCandidate(candidates, std::to_string(lowGuid));
		AddZombieRankGuidCandidate(candidates,
			std::to_string(static_cast<std::int32_t>(lowGuid)));

		std::string lowHex = Utils::String::VA("%X", lowGuid);
		AddZombieRankGuidCandidate(candidates, lowHex);

		std::transform(lowHex.begin(), lowHex.end(), lowHex.begin(),
			[](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		AddZombieRankGuidCandidate(candidates, lowHex);
	}

	static std::filesystem::path GetZombieRankPath(const std::string& guid)
	{
		return std::filesystem::path("zw3") /
			"core" /
			"scriptdata" /
			("rank_" + guid);
	}

	static bool TryReadZombieRank(const std::string& guid,
		int& level, int& prestige)
	{
		if (guid.empty())
		{
			return false;
		}

		std::string data;
		if (!Utils::IO::ReadFile(GetZombieRankPath(guid).string(), &data))
		{
			return false;
		}

		int parsedLevel = -1;
		int parsedPrestige = 0;
		if (!TryParseZombieRankValue(data, "level", parsedLevel) ||
			!TryParseZombieRankValue(data, "prestige", parsedPrestige))
		{
			return false;
		}

		level = std::clamp(parsedLevel, 0, 53);
		prestige = std::max(parsedPrestige, 0);
		return true;
	}


	static bool TryReadZombieRank(const std::uint64_t guid,
		int& level, int& prestige)
	{
		std::vector<std::string> guidCandidates;
		guidCandidates.reserve(8);
		AddZombieRankGuidCandidates(guidCandidates, guid);

		for (const auto& candidate : guidCandidates)
		{
			std::string data;
			if (!Utils::IO::ReadFile(
				GetZombieRankPath(candidate).string(), &data))
			{
				continue;
			}

			int parsedLevel = -1;
			int parsedPrestige = 0;
			if (!TryParseZombieRankValue(data, "level", parsedLevel) ||
				!TryParseZombieRankValue(
					data, "prestige", parsedPrestige))
			{
				continue;
			}

			level = std::clamp(parsedLevel, 0, 53);
			prestige = std::max(parsedPrestige, 0);
			return true;
		}

		return false;
	}

	static bool TryReadZombieRank(const Game::client_s& client,
		const std::uint64_t resolvedXuid, int& level, int& prestige)
	{
		std::vector<std::string> guidCandidates;
		guidCandidates.reserve(16);

		AddZombieRankGuidCandidates(guidCandidates, resolvedXuid);
		AddZombieRankGuidCandidates(guidCandidates,
			static_cast<std::uint64_t>(client.steamID));

		const Utils::InfoString userInfo(client.userinfo);
		AddZombieRankGuidCandidates(guidCandidates,
			std::strtoull(userInfo.get("realsteamId").c_str(), nullptr, 16));
		AddZombieRankGuidCandidates(guidCandidates,
			std::strtoull(userInfo.get("steamId").c_str(), nullptr, 16));
		AddZombieRankGuidCandidate(guidCandidates, userInfo.get("guid"));

		for (const auto& guid : guidCandidates)
		{
			std::string data;
			if (!Utils::IO::ReadFile(GetZombieRankPath(guid).string(), &data))
			{
				continue;
			}

			int parsedLevel = -1;
			int parsedPrestige = 0;
			if (!TryParseZombieRankValue(data, "level", parsedLevel) ||
				!TryParseZombieRankValue(data, "prestige", parsedPrestige))
			{
				continue;
			}

			level = std::clamp(parsedLevel, 0, 53);
			prestige = std::max(parsedPrestige, 0);
			return true;
		}

		return false;
	}

	static void UpdateClientZombieRankState(const int clientNum,
		const std::uint64_t resolvedXuid, const int now)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		const auto& client = Game::svs_clients[clientNum];
		const auto identity = resolvedXuid != 0
			? resolvedXuid
			: static_cast<std::uint64_t>(client.steamID);

		if (identity == 0 ||
			client.header.state < Game::CS_ACTIVE ||
			client.bIsTestClient)
		{
			ResetClientZombieRankState(clientNum);
			return;
		}

		auto& state = ClientZombieRanks[clientNum];
		if (state.identity != identity)
		{
			state = {};
			state.identity = identity;
		}

		if (now < state.nextReadAt)
		{
			return;
		}

		state.nextReadAt = now + ZombieRankRefreshInterval;

		int level = -1;
		int prestige = 0;
		if (TryReadZombieRank(client, resolvedXuid, level, prestige))
		{
			state.level = level;
			state.prestige = prestige;
		}
	}

	static std::string GetZombieRankIcon(const int prestige)
	{
		if (prestige < 0)
		{
			return {};
		}

		const auto iconLevel = prestige + 1;
		if (iconLevel > 8)
		{
			return "skullicon";
		}

		return Utils::String::VA("prestige_%d", iconLevel);
	}



	static constexpr int LobbyRankSlotCount = 4;

	static void SetLobbyRankSlot(const int slot,
		const std::string& icon, const std::string& level)
	{
		if (slot < 0 || slot >= LobbyRankSlotCount)
		{
			return;
		}

		const auto displaySlot = slot + 1;

		Dvar::Var(Utils::String::VA(
			"character_%d_rank_icon",
			displaySlot)).set(icon.c_str());

		Dvar::Var(Utils::String::VA(
			"character_%d_rank_level",
			displaySlot)).set(level.c_str());
	}

	static void ClearLobbyRankSlots()
	{
		for (int slot = 0; slot < LobbyRankSlotCount; ++slot)
		{
			SetLobbyRankSlot(slot, "", "");
		}
	}

	static void SetZWNetLobbyRankSlot(const int slot,
		const std::string& icon, const std::string& level)
	{
		if (slot < 0 || slot >= LobbyRankSlotCount)
		{
			return;
		}

		Dvar::Var(Utils::String::VA(
			"zwnet_lobby_member_%d_rank_icon",
			slot)).set(icon.c_str());

		Dvar::Var(Utils::String::VA(
			"zwnet_lobby_member_%d_rank_level",
			slot)).set(level.c_str());
	}

	static void ClearZWNetLobbyRankSlots()
	{
		for (int slot = 0; slot < LobbyRankSlotCount; ++slot)
		{
			SetZWNetLobbyRankSlot(slot, "", "");
		}
	}

	static std::uint64_t FindLobbyPlayerXuid(
		const std::string& playerName)
	{
		if (!Game::g_lobbyData ||
			playerName.empty() ||
			playerName == "None")
		{
			return 0;
		}

		for (int memberIndex = 0;
			memberIndex < LobbyRankSlotCount;
			++memberIndex)
		{
			const auto& member =
				Game::g_lobbyData->partyMembers[memberIndex];

			if (member.status == 0 ||
				!member.gamertag ||
				!member.gamertag[0])
			{
				continue;
			}

			if (_stricmp(
				member.gamertag,
				playerName.c_str()) == 0)
			{
				return static_cast<std::uint64_t>(
					member.player);
			}
		}

		return 0;
	}

	static void RefreshZWNetLobbyRanks()
	{
		for (int slot = 0; slot < LobbyRankSlotCount; ++slot)
		{
			const auto playerName = Dvar::Var(Utils::String::VA(
				"zwnet_lobby_member_%d_name",
				slot)).get<std::string>();

			std::string rankIcon;
			std::string rankLevel;

			if (!playerName.empty())
			{
				int level = 0;
				int prestige = 0;
				const auto isLocalPlayer = Dvar::Var(Utils::String::VA(
					"zwnet_lobby_member_%d_self",
					slot)).get<bool>();
				bool rankFound = false;

				if (isLocalPlayer)
				{
					rankFound = TryReadZombieRank(
						Dvar::Var("ui_zwnet_guid").get<std::string>(),
						level, prestige);

					if (!rankFound)
					{
						rankFound = TryReadZombieRank(
							Party::GetLocalPlayerXUID(), level, prestige);
					}
				}
				else
				{
					const auto sharedRankKnown = Dvar::Var(
						Utils::String::VA(
							"zwnet_lobby_member_%d_shared_rank_known",
							slot)).get<bool>();
					if (sharedRankKnown)
					{
						level = Dvar::Var(Utils::String::VA(
							"zwnet_lobby_member_%d_shared_rank_level",
							slot)).get<int>();
						prestige = Dvar::Var(Utils::String::VA(
							"zwnet_lobby_member_%d_shared_rank_prestige",
							slot)).get<int>();
						rankFound = true;
					}
				}

				if (rankFound)
				{
					rankIcon = GetZombieRankIcon(prestige);
					rankLevel = std::to_string(
						isLocalPlayer ? level + 1 : level);
				}
				else
				{
					rankIcon = GetZombieRankIcon(0);
					rankLevel = "1";
				}
			}

			SetZWNetLobbyRankSlot(slot, rankIcon, rankLevel);
		}
	}

	static std::string BuildLobbyRankSnapshot()
	{
		Utils::InfoString snapshot;

		const auto realPlayers = std::clamp(
			Dvar::Var("party_realPlayers").get<int>(),
			0,
			LobbyRankSlotCount);

		const auto botCount = std::clamp(
			Dvar::Var("addBots").get<int>(),
			0,
			LobbyRankSlotCount - realPlayers);

		const auto totalPlayers =
			std::min(LobbyRankSlotCount,
				realPlayers + botCount);

		for (int slot = 0; slot < LobbyRankSlotCount; ++slot)
		{
			std::string rankIcon;
			std::string rankLevel;

			const auto displaySlot = slot + 1;
			const auto playerName = Dvar::Var(
				Utils::String::VA(
					"character_%d_player",
					displaySlot)).get<std::string>();

			const bool configuredBotSlot =
				slot >= realPlayers &&
				slot < totalPlayers;

			const bool occupied =
				configuredBotSlot ||
				(slot < realPlayers &&
					!playerName.empty() &&
					playerName != "None");

			if (occupied)
			{
				const bool isBot =
					configuredBotSlot ||
					Utils::String::StartsWith(
						playerName,
						"[BOT]");

				int level = 0;
				int prestige = 0;

				if (!isBot)
				{
					const auto xuid =
						FindLobbyPlayerXuid(playerName);

					int savedLevel = 0;
					int savedPrestige = 0;
					if (xuid != 0 &&
						TryReadZombieRank(
							xuid,
							savedLevel,
							savedPrestige))
					{
						level = savedLevel;
						prestige = savedPrestige;
					}
				}

				rankIcon = GetZombieRankIcon(prestige);
				rankLevel = std::to_string(level + 1);
			}

			snapshot.set(
				Utils::String::VA(
					"rankIcon%d",
					displaySlot),
				rankIcon);

			snapshot.set(
				Utils::String::VA(
					"rankLevel%d",
					displaySlot),
				rankLevel);
		}

		return snapshot.build();
	}

	static void ApplyLobbyRankSnapshot(const std::string& data)
	{
		const Utils::InfoString snapshot(data);

		for (int slot = 0; slot < LobbyRankSlotCount; ++slot)
		{
			const auto displaySlot = slot + 1;

			SetLobbyRankSlot(
				slot,
				snapshot.get(Utils::String::VA(
					"rankIcon%d",
					displaySlot)),
				snapshot.get(Utils::String::VA(
					"rankLevel%d",
					displaySlot)));
		}
	}

	static bool IsMenuVisible(const char* menuName)
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, menuName);
		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	static void RefreshLobbyRanks()
	{
		if (Dvar::Var("party_host").get<bool>())
		{
			ApplyLobbyRankSnapshot(
				BuildLobbyRankSnapshot());
			return;
		}

		Network::SendCommand(
			Party::Target(),
			"getZW3LobbyRanks");
	}

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

	static void ResetClientConnectingState(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		ClientConnectingLastPacketTime[clientNum] = 0;
		ClientConnectingLastPacketAdvanceAt[clientNum] = 0;
		ClientConnectingStale[clientNum] = false;
	}

	static void UpdateClientConnectingState(const int clientNum, const int now)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		const auto& client = Game::svs_clients[clientNum];

		if (client.header.state < Game::CS_CONNECTED ||
			client.header.state >= Game::CS_ACTIVE ||
			client.bIsTestClient)
		{
			ResetClientConnectingState(clientNum);
			return;
		}

		if (ClientConnectingLastPacketAdvanceAt[clientNum] == 0 ||
			ClientConnectingLastPacketTime[clientNum] != client.lastPacketTime)
		{
			ClientConnectingLastPacketTime[clientNum] = client.lastPacketTime;
			ClientConnectingLastPacketAdvanceAt[clientNum] = now;
			ClientConnectingStale[clientNum] = false;
			return;
		}

		if (now - ClientConnectingLastPacketAdvanceAt[clientNum] >= ConnectingPacketTimeout)
		{
			ClientConnectingStale[clientNum] = true;
		}
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
			const bool staleConnecting =
				client.header.state < Game::CS_ACTIVE &&
				ClientConnectingStale[clientNum];

			if (client.header.state >= Game::CS_CONNECTED &&
				!client.bIsTestClient &&
				!staleConnecting)
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

	static std::string GetCharacterForClient(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return {};
		}

		const auto character = CharacterAssignments::ResolveClientCharacter(clientNum);
		return CharacterAssignments::IsValid(character)
			? CharacterAssignments::ToString(character)
			: std::string();
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
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_rank_icon", i)).set("");
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_rank_level", i)).set("");
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
			std::string rankIcon;
			std::string rankLevel;
			if (player.rank >= 0)
			{
				rankIcon = GetZombieRankIcon(player.prestige);
				rankLevel = std::to_string(player.rank + 1);
			}

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
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_rank_icon", static_cast<int>(i))).set(rankIcon.c_str());
			Dvar::Var(Utils::String::VA("zw3_ui_sb_p%d_rank_level", static_cast<int>(i))).set(rankLevel.c_str());
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

			const auto rankValue = info.get("rank");
			const auto prestigeValue = info.get("prestige");
			player.rank = rankValue.empty()
				? -1
				: std::strtol(rankValue.data(), nullptr, 10);
			player.prestige = prestigeValue.empty()
				? 0
				: std::strtol(prestigeValue.data(), nullptr, 10);

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
		UIScript::Add("RefreshLobbyRanks",
			[]([[maybe_unused]] const UIScript::Token& token,
				[[maybe_unused]] const Game::uiInfo_s* info)
			{
				RefreshLobbyRanks();
			});
		UIScript::Add("RefreshZWNetLobbyRanks",
			[]([[maybe_unused]] const UIScript::Token& token,
				[[maybe_unused]] const Game::uiInfo_s* info)
			{
				RefreshZWNetLobbyRanks();
			});

		UIFeeder::Add(13.0f, GetPlayerCount, GetPlayerText, SelectPlayer);

		Events::OnClientDisconnect([](const int clientNum)
			{
				GSC::Field::ResetClientScoreboardStats(clientNum);
				ResetClientTransientScoreboardState(clientNum, 1500);
				ResetClientConnectingState(clientNum);
				ResetClientZombieRankState(clientNum);

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
				const auto now = Game::Sys_Milliseconds();

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
						ResetClientConnectingState(clientNum);
						ResetClientZombieRankState(clientNum);
					}


					if (hasOccupant && !isBot && state < Game::CS_ACTIVE)
					{
						ResetClientTransientScoreboardState(clientNum, 1500);
					}

					UpdateClientConnectingState(clientNum, now);

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


		Components::Scheduler::Loop([]()
			{
				if (IsMenuVisible("menu_xboxlive_privatelobby"))
				{
					RefreshLobbyRanks();
				}
				else
				{
					ClearLobbyRankSlots();
				}

				if (IsMenuVisible("zwnet_matchmaking"))
				{
					RefreshZWNetLobbyRanks();
				}
				else
				{
					ClearZWNetLobbyRankSlots();
				}
			}, Components::Scheduler::Pipeline::MAIN, 250ms);

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


		Network::OnClientPacket("getZW3LobbyRanks",
			[](const Network::Address& address,
				[[maybe_unused]] const std::string& data)
			{
				if (!Dvar::Var(
					"party_host").get<bool>())
				{
					return;
				}

				const auto snapshot =
					BuildLobbyRankSnapshot();

				ApplyLobbyRankSnapshot(snapshot);

				Network::SendCommand(
					address,
					"zw3LobbyRankUpdate",
					snapshot);
			});

		Network::OnClientPacket("zw3LobbyRankUpdate",
			[]([[maybe_unused]] const Network::Address& address,
				const std::string& data)
			{
				if (Dvar::Var(
					"party_host").get<bool>())
				{
					return;
				}

				ApplyLobbyRankSnapshot(data);
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
					info.set("rank", std::to_string(player.rank));
					info.set("prestige", std::to_string(player.prestige));
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

		const auto now = Game::Sys_Milliseconds();
		std::unordered_set<std::uint64_t> publishedRealXuids;

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& serverClient = Game::svs_clients[clientNum];

			if (serverClient.header.state < Game::CS_CONNECTED)
			{
				GSC::Field::ResetClientScoreboardStats(clientNum);
				ResetClientTransientScoreboardState(clientNum, 0);
				ResetClientConnectingState(clientNum);
				ResetClientZombieRankState(clientNum);
				continue;
			}

			UpdateClientConnectingState(clientNum, now);

			const auto xuid =
				CharacterAssignments::GetClientXuid(serverClient);
			const bool isBot = serverClient.bIsTestClient && xuid == 0;

			const bool staleConnecting =
				!isBot &&
				serverClient.header.state < Game::CS_ACTIVE &&
				ClientConnectingStale[clientNum];

			if (staleConnecting)
			{
				continue;
			}

			if (!isBot && xuid != 0 &&
				!publishedRealXuids.insert(xuid).second)
			{
				continue;
			}

			const auto character = GetCharacterForClient(clientNum);
			if (character.empty())
			{
				continue;
			}

			const bool hasActiveGameClient =
				serverClient.header.state >= Game::CS_ACTIVE &&
				serverClient.gentity &&
				serverClient.gentity->client;

			if (!hasActiveGameClient)
			{
				if (isBot ||
					clientNum == ListenServerHostClientNum ||
					ClientConnectingStale[clientNum])
				{
					continue;
				}

				Container::Player player;
				player.clientNum = clientNum;
				player.name = GetScoreboardBaseName(serverClient.name);

				if (player.name.empty())
				{
					player.name = "Connecting...";
				}

				player.icon = character;
				player.ping = serverClient.ping;
				player.survivalTime =
					Dvar::Var("zw3_ui_sb_survived_time").get<std::string>();
				player.status = "CONNECTING";

				PlayerContainer.playerList.push_back(player);
				continue;
			}

			Container::Player player;
			player.clientNum = clientNum;
			player.name = isBot
				? Utils::String::VA("[BOT] %s", character.c_str())
				: GetScoreboardBaseName(serverClient.name);
			player.icon = character;
			player.ping = serverClient.ping;
			player.survivalTime =
				Dvar::Var("zw3_ui_sb_survived_time").get<std::string>();

			const auto* client = serverClient.gentity->client;
			player.score = Game::SV_GameClientNum_Score(clientNum);
			player.kills = client->sess.kills;
			player.downs = GSC::Field::GetClientDowns(clientNum);
			player.revives = GSC::Field::GetClientRevives(clientNum);
			player.deaths = client->sess.deaths;

			if (isBot)
			{
				player.rank = 0;
				player.prestige = 0;
			}
			else
			{
				UpdateClientZombieRankState(clientNum, xuid, now);
				player.rank = ClientZombieRanks[clientNum].level;
				player.prestige = ClientZombieRanks[clientNum].prestige;
			}

			const bool dead =
				client->sess.cs.team == Game::TEAM_SPECTATOR;
			const bool suppressDown =
				now < ClientDownStateSuppressedUntil[clientNum];

			const auto rawDown = suppressDown
				? 0
				: GetDvarIntStringSafe(
					Utils::String::VA("zw3_sb_down_%d", clientNum));

			const auto rawProgress = suppressDown
				? 0.0f
				: GetDvarFloatStringSafe(
					Utils::String::VA(
						"zw3_sb_down_progress_%d",
						clientNum));

			player.down = (!dead && rawDown == 1) ? 1 : 0;
			player.downProgress = player.down
				? std::clamp(rawProgress, 0.0f, 1.0f)
				: 0.0f;
			player.status =
				GetScoreboardStatusName(dead, player.down == 1);

			NormalisePlayerDownState(player);
			PlayerContainer.playerList.push_back(player);
		}

		std::stable_sort(
			PlayerContainer.playerList.begin(),
			PlayerContainer.playerList.end(),
			[](const Container::Player& a, const Container::Player& b)
			{
				auto group = [](const Container::Player& player)
					{
						if (!Dedicated::IsRunning() &&
							player.clientNum == ListenServerHostClientNum)
						{
							return 0;
						}

						return Game::svs_clients[player.clientNum].bIsTestClient
							? 2
							: 1;
					};

				const int aGroup = group(a);
				const int bGroup = group(b);

				if (aGroup != bGroup)
				{
					return aGroup < bGroup;
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
