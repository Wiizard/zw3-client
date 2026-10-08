#include "STDInclude.hpp"

#include "ServerInfo.hpp"
#include "CharacterAssignments.hpp"
#include "ClientSlots.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Friends.hpp"
#include "Gamepad.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "ServerList.hpp"
#include "UIFeeder.hpp"
#include "Voice.hpp"
#include "GSC/Field.hpp"

namespace Components
{
	ServerInfo::Container ServerInfo::playerContainer;

	constexpr int iw4xProtocol = 153;

	constexpr std::uintptr_t CG_DrawScoreboard_BodyCall = 0x1400E410F;
	constexpr std::uintptr_t scoreboardBody = 0x1400E4820;

	constexpr float playerListFeeder = 13.0f;

	constexpr int connectingTimeoutMs = 10'000;
	constexpr int zombieRankRefreshMs = 1000;
	constexpr int lobbyRankSlotCount = 4;
	constexpr int scoreboardRowCount = 4;

	struct ZombieRankState
	{
		std::uint64_t identity = 0;
		int level = -1;
		int prestige = 0;
		int nextReadAt = 0;
	};

	static int listenServerHostClientNum = -1;
	static std::array<int, Game::MAX_CLIENTS> downStateSuppressedUntil{};
	static std::array<int, Game::MAX_CLIENTS> connectingLastPacketTime{};
	static std::array<int, Game::MAX_CLIENTS> connectingLastPacketAdvanceAt{};
	static std::array<bool, Game::MAX_CLIENTS> isConnectingStale{};
	static std::array<ZombieRankState, Game::MAX_CLIENTS> zombieRanks{};

	static void SetUIDvar(const char* name, const std::string& value)
	{
		Game::Dvar_SetFromStringByName(name, value.data());
	}

	static const char* Choose(bool isSet, const char* whenSet, const char* whenClear)
	{
		if (isSet)
		{
			return whenSet;
		}

		return whenClear;
	}

	static void ResetZombieRankState(int clientNum)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return;
		}

		zombieRanks[clientNum] = {};
	}

	static bool TryParseRankValue(const std::string& data, std::string_view field, int& value)
	{
		const auto fieldPosition = data.find(field);

		if (fieldPosition == std::string::npos)
		{
			return false;
		}

		auto valuePosition = fieldPosition + field.size();

		while (valuePosition < data.size() && (data[valuePosition] == ':' || data[valuePosition] == ' ' || data[valuePosition] == '\t'))
		{
			++valuePosition;
		}

		if (valuePosition >= data.size())
		{
			return false;
		}

		char* end = nullptr;
		const auto parsed = std::strtol(data.data() + valuePosition, &end, 10);

		if (end == data.data() + valuePosition)
		{
			return false;
		}

		value = static_cast<int>(parsed);
		return true;
	}

	static void AddGuidCandidate(std::vector<std::string>& candidates, const std::string& candidate)
	{
		if (candidate.empty() || std::ranges::find(candidates, candidate) != candidates.end())
		{
			return;
		}

		candidates.push_back(candidate);
	}

	static void AddGuidCandidates(std::vector<std::string>& candidates, std::uint64_t guid)
	{
		if (guid == 0)
		{
			return;
		}

		const auto lowGuid = static_cast<std::uint32_t>(guid);

		AddGuidCandidate(candidates, std::to_string(guid));
		AddGuidCandidate(candidates, std::to_string(static_cast<std::int64_t>(guid)));
		AddGuidCandidate(candidates, std::format("{:X}", guid));
		AddGuidCandidate(candidates, std::format("{:x}", guid));
		AddGuidCandidate(candidates, std::to_string(lowGuid));
		AddGuidCandidate(candidates, std::to_string(static_cast<std::int32_t>(lowGuid)));
		AddGuidCandidate(candidates, std::format("{:X}", lowGuid));
		AddGuidCandidate(candidates, std::format("{:x}", lowGuid));
	}

	static std::filesystem::path GetZombieRankPath(const std::string& guid)
	{
		return std::filesystem::path("zw3") / "core" / "scriptdata" / ("rank_" + guid);
	}

	static bool TryReadZombieRank(const std::vector<std::string>& guids, int& level, int& prestige)
	{
		for (const auto& guid : guids)
		{
			std::string data;

			if (!Utils::IO::ReadFile(GetZombieRankPath(guid).string(), &data))
			{
				continue;
			}

			int parsedLevel = -1;
			int parsedPrestige = 0;

			if (!TryParseRankValue(data, "level", parsedLevel) || !TryParseRankValue(data, "prestige", parsedPrestige))
			{
				continue;
			}

			level = std::clamp(parsedLevel, 0, 53);
			prestige = std::max(parsedPrestige, 0);
			return true;
		}

		return false;
	}

	static bool TryReadClientZombieRank(const Game::client_s& client, std::uint64_t xuid, int& level, int& prestige)
	{
		std::vector<std::string> guids;
		const Utils::InfoString userInfo(client.userinfo);

		AddGuidCandidates(guids, xuid);
		AddGuidCandidates(guids, static_cast<std::uint64_t>(client.steamID));
		AddGuidCandidates(guids, std::strtoull(userInfo.Get("realsteamId").data(), nullptr, 16));
		AddGuidCandidates(guids, std::strtoull(userInfo.Get("steamId").data(), nullptr, 16));
		AddGuidCandidate(guids, userInfo.Get("guid"));

		return TryReadZombieRank(guids, level, prestige);
	}

	static void UpdateZombieRankState(int clientNum, std::uint64_t xuid, int now)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return;
		}

		const auto& client = Game::svs_clients[clientNum];
		auto identity = xuid;

		if (identity == 0)
		{
			identity = static_cast<std::uint64_t>(client.steamID);
		}

		if (identity == 0 || client.header.state < Game::CS_ACTIVE || client.bIsTestClient)
		{
			ResetZombieRankState(clientNum);
			return;
		}

		auto& state = zombieRanks[clientNum];

		if (state.identity != identity)
		{
			state = {};
			state.identity = identity;
		}

		if (now < state.nextReadAt)
		{
			return;
		}

		state.nextReadAt = now + zombieRankRefreshMs;

		int level = -1;
		int prestige = 0;

		if (TryReadClientZombieRank(client, xuid, level, prestige))
		{
			state.level = level;
			state.prestige = prestige;
		}
	}

	static std::string GetZombieRankIcon(int prestige)
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

		return std::format("prestige_{}", iconLevel);
	}

	static void SetLobbyRankSlot(int slot, const std::string& icon, const std::string& level)
	{
		SetUIDvar(Utils::String::VA("character_%d_rank_icon", slot + 1), icon);
		SetUIDvar(Utils::String::VA("character_%d_rank_level", slot + 1), level);
	}

	static void ClearLobbyRankSlots()
	{
		for (int slot = 0; slot < lobbyRankSlotCount; ++slot)
		{
			SetLobbyRankSlot(slot, "", "");
		}
	}

	static void SetZWNetLobbyRankSlot(int slot, const std::string& icon, const std::string& level)
	{
		SetUIDvar(Utils::String::VA("zwnet_lobby_member_%d_rank_icon", slot), icon);
		SetUIDvar(Utils::String::VA("zwnet_lobby_member_%d_rank_level", slot), level);
	}

	static void ClearZWNetLobbyRankSlots()
	{
		for (int slot = 0; slot < lobbyRankSlotCount; ++slot)
		{
			SetZWNetLobbyRankSlot(slot, "", "");
		}
	}

	static std::uint64_t FindLobbyPlayerXuid(const std::string& playerName)
	{
		if (playerName.empty() || playerName == "None")
		{
			return 0;
		}

		for (int memberIndex = 0; memberIndex < lobbyRankSlotCount; ++memberIndex)
		{
			const auto& member = Game::g_lobbyData->partyMembers[memberIndex];
			const std::string gamertag(member.gamertag, strnlen(member.gamertag, sizeof(member.gamertag)));

			if (member.status == 0 || gamertag.empty())
			{
				continue;
			}

			if (_stricmp(gamertag.data(), playerName.data()) == 0)
			{
				return member.player;
			}
		}

		return 0;
	}

	static void RefreshZWNetLobbyRanks()
	{
		for (int slot = 0; slot < lobbyRankSlotCount; ++slot)
		{
			const auto playerName = Dvar::Var(Utils::String::VA("zwnet_lobby_member_%d_name", slot)).Get<std::string>();

			std::string rankIcon;
			std::string rankLevel;

			if (!playerName.empty())
			{
				int level = 0;
				int prestige = 0;
				bool isRankFound = false;
				const bool isLocalPlayer = Dvar::Var(Utils::String::VA("zwnet_lobby_member_%d_self", slot)).Get<bool>();

				if (isLocalPlayer)
				{
					std::vector<std::string> guids;
					AddGuidCandidate(guids, Dvar::Var("ui_zwnet_guid").Get<std::string>());
					isRankFound = TryReadZombieRank(guids, level, prestige);

					if (!isRankFound)
					{
						guids.clear();
						AddGuidCandidates(guids, Party::GetLocalPlayerXuid());
						isRankFound = TryReadZombieRank(guids, level, prestige);
					}
				}
				else if (Dvar::Var(Utils::String::VA("zwnet_lobby_member_%d_shared_rank_known", slot)).Get<bool>())
				{
					level = Dvar::Var(Utils::String::VA("zwnet_lobby_member_%d_shared_rank_level", slot)).Get<int>();
					prestige = Dvar::Var(Utils::String::VA("zwnet_lobby_member_%d_shared_rank_prestige", slot)).Get<int>();
					isRankFound = true;
				}

				if (isRankFound)
				{
					rankIcon = GetZombieRankIcon(prestige);
					rankLevel = std::to_string(level);

					if (isLocalPlayer)
					{
						rankLevel = std::to_string(level + 1);
					}
				}
			}

			SetZWNetLobbyRankSlot(slot, rankIcon, rankLevel);
		}
	}

	static std::string BuildLobbyRankSnapshot()
	{
		Utils::InfoString snapshot;

		const auto realPlayers = std::clamp(Dvar::Var("party_realPlayers").Get<int>(), 0, lobbyRankSlotCount);
		const auto botCount = std::clamp(Dvar::Var("addBots").Get<int>(), 0, lobbyRankSlotCount - realPlayers);
		const auto totalPlayers = std::min(lobbyRankSlotCount, realPlayers + botCount);

		for (int slot = 0; slot < lobbyRankSlotCount; ++slot)
		{
			std::string rankIcon;
			std::string rankLevel;

			const auto playerName = Dvar::Var(Utils::String::VA("character_%d_player", slot + 1)).Get<std::string>();
			const bool isBotSlot = slot >= realPlayers && slot < totalPlayers;
			const bool isOccupied = isBotSlot || (slot < realPlayers && !playerName.empty() && playerName != "None");

			if (isOccupied)
			{
				const bool isBot = isBotSlot || Utils::String::StartsWith(playerName, "[BOT]");
				int level = 0;
				int prestige = 0;

				if (!isBot)
				{
					std::vector<std::string> guids;
					AddGuidCandidates(guids, FindLobbyPlayerXuid(playerName));

					int savedLevel = 0;
					int savedPrestige = 0;

					if (TryReadZombieRank(guids, savedLevel, savedPrestige))
					{
						level = savedLevel;
						prestige = savedPrestige;
					}
				}

				rankIcon = GetZombieRankIcon(prestige);
				rankLevel = std::to_string(level + 1);
			}

			snapshot.Set(Utils::String::VA("rankIcon%d", slot + 1), rankIcon);
			snapshot.Set(Utils::String::VA("rankLevel%d", slot + 1), rankLevel);
		}

		return snapshot.Build();
	}

	static void ApplyLobbyRankSnapshot(const std::string& data)
	{
		const Utils::InfoString snapshot(data);

		for (int slot = 0; slot < lobbyRankSlotCount; ++slot)
		{
			SetLobbyRankSlot(slot, snapshot.Get(Utils::String::VA("rankIcon%d", slot + 1)), snapshot.Get(Utils::String::VA("rankLevel%d", slot + 1)));
		}
	}

	static bool IsMenuVisible(const char* menuName)
	{
		auto* const menu = Game::Menus_FindByName(Game::uiContext, menuName);

		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	static void RefreshLobbyRanks()
	{
		if (Party::IsHostingParty())
		{
			ApplyLobbyRankSnapshot(BuildLobbyRankSnapshot());
			return;
		}

		Network::SendCommand(Party::Target(), "getZW3LobbyRanks");
	}

	static void ResetTransientScoreboardState(int clientNum, int suppressMs)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return;
		}

		SetUIDvar(Utils::String::VA("zw3_sb_down_%d", clientNum), "0");
		SetUIDvar(Utils::String::VA("zw3_sb_down_progress_%d", clientNum), "0");
		downStateSuppressedUntil[clientNum] = Game::Sys_Milliseconds() + suppressMs;
	}

	static void ResetConnectingState(int clientNum)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return;
		}

		connectingLastPacketTime[clientNum] = 0;
		connectingLastPacketAdvanceAt[clientNum] = 0;
		isConnectingStale[clientNum] = false;
	}

	static void UpdateConnectingState(int clientNum, int now)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return;
		}

		const auto& client = Game::svs_clients[clientNum];

		if (client.header.state < Game::CS_CONNECTED || client.header.state >= Game::CS_ACTIVE || client.bIsTestClient)
		{
			ResetConnectingState(clientNum);
			return;
		}

		if (connectingLastPacketAdvanceAt[clientNum] == 0 || connectingLastPacketTime[clientNum] != client.lastPacketTime)
		{
			connectingLastPacketTime[clientNum] = client.lastPacketTime;
			connectingLastPacketAdvanceAt[clientNum] = now;
			isConnectingStale[clientNum] = false;
			return;
		}

		if (now - connectingLastPacketAdvanceAt[clientNum] >= connectingTimeoutMs)
		{
			isConnectingStale[clientNum] = true;
		}
	}

	static std::string GetScoreboardBaseName(std::string name)
	{
		Utils::String::Replace(name, "^1[DEAD] ^7", "");
		Utils::String::Replace(name, "^3[DOWN] ^7", "");
		Utils::String::Replace(name, "[DEAD] ", "");
		Utils::String::Replace(name, "[DOWN] ", "");

		return name;
	}

	static std::string GetScoreboardStatusName(bool isDead, bool isDown)
	{
		if (isDead)
		{
			return "DEAD";
		}

		if (isDown)
		{
			return "DOWN";
		}

		return {};
	}

	static bool IsConnectingStale(int clientNum)
	{
		return Game::svs_clients[clientNum].header.state < Game::CS_ACTIVE && isConnectingStale[clientNum];
	}

	static bool IsBetterHostCandidate(int a, int b)
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
	}

	static void UpdateListenServerHostClientNum()
	{
		if (Dedicated::IsRunning() || !Party::IsHostingParty())
		{
			listenServerHostClientNum = -1;
			return;
		}

		if (CharacterAssignments::IsClientNum(listenServerHostClientNum))
		{
			const auto& cached = Game::svs_clients[listenServerHostClientNum];

			if (cached.header.state >= Game::CS_CONNECTED && !cached.bIsTestClient)
			{
				return;
			}
		}

		listenServerHostClientNum = -1;

		std::vector<int> realClientNums;

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];

			if (client.header.state >= Game::CS_CONNECTED && !client.bIsTestClient && !IsConnectingStale(clientNum))
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
			listenServerHostClientNum = realClientNums.front();
			return;
		}

		const auto localHostName = CharacterAssignments::Normalize(Dvar::Var("name").Get<std::string>());
		int bestClientNum = -1;

		for (const auto clientNum : realClientNums)
		{
			if (CharacterAssignments::Normalize(Game::svs_clients[clientNum].name) != localHostName)
			{
				continue;
			}

			if (bestClientNum == -1 || IsBetterHostCandidate(clientNum, bestClientNum))
			{
				bestClientNum = clientNum;
			}
		}

		if (bestClientNum == -1)
		{
			bestClientNum = *std::ranges::min_element(realClientNums, IsBetterHostCandidate);
		}

		listenServerHostClientNum = bestClientNum;
	}

	static std::string GetCharacterForClient(int clientNum)
	{
		const auto character = CharacterAssignments::ResolveClientCharacter(clientNum);

		if (!CharacterAssignments::IsValid(character))
		{
			return {};
		}

		return CharacterAssignments::ToString(character);
	}

	unsigned int ServerInfo::GetPlayerCount()
	{
		return static_cast<unsigned int>(playerContainer.playerList.size());
	}

	const char* ServerInfo::GetPlayerText(unsigned int index, int column)
	{
		if (index >= playerContainer.playerList.size())
		{
			return "";
		}

		const auto& player = playerContainer.playerList[index];

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
		playerContainer.currentPlayer = index;
	}

	void ServerInfo::ServerStatus([[maybe_unused]] const UIScript::Token& token)
	{
		playerContainer.currentPlayer = 0;
		playerContainer.playerList.clear();

		const auto* const server = ServerList::GetCurrentServer();

		if (!server)
		{
			return;
		}

		SetUIDvar("uiSi_ServerName", server->hostname);
		SetUIDvar("uiSi_MaxClients", std::to_string(server->clients));
		SetUIDvar("uiSi_Version", server->version);
		SetUIDvar("uiSi_SecurityLevel", std::to_string(server->securityLevel));
		SetUIDvar("uiSi_isPrivate", Choose(server->password, "@MENU_YES", "@MENU_NO"));
		SetUIDvar("uiSi_Hardcore", Choose(server->hardcore, "@MENU_ENABLED", "@MENU_DISABLED"));
		SetUIDvar("uiSi_KillCam", "@MENU_NO");
		SetUIDvar("uiSi_ffType", "@MENU_DISABLED");
		SetUIDvar("uiSi_MapName", server->mapname);
		SetUIDvar("uiSi_MapNameLoc", Localization::LocalizeMapName(server->mapname.data()));
		SetUIDvar("uiSi_GameType", Game::UI_GetGameTypeDisplayName(server->gametype.data()));
		SetUIDvar("uiSi_ModName", "");
		SetUIDvar("uiSi_aimAssist", Choose(server->aimassist, "@MENU_YES", "@MENU_NO"));
		SetUIDvar("uiSi_voiceChat", Choose(server->voice, "@MENU_YES", "@MENU_NO"));

		if (server->mod.size() > 5)
		{
			SetUIDvar("uiSi_ModName", server->mod.substr(5));
		}

		playerContainer.target = server->addr;
		Network::SendCommand(playerContainer.target, "getstatus");
	}

	void ServerInfo::NormalisePlayerDownState(Container::Player& player)
	{
		if (player.status == "DEAD" || player.status == "CONNECTING")
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

	void ServerInfo::WriteScoreboardRowDvars()
	{
		SetUIDvar("zw3_ui_sb_player_count", std::to_string(playerContainer.playerList.size()));

		for (int row = 0; row < scoreboardRowCount; ++row)
		{
			Container::Player player{};
			const bool hasPlayer = row < static_cast<int>(playerContainer.playerList.size());

			if (hasPlayer)
			{
				player = playerContainer.playerList[row];
			}

			const bool isDown = player.status != "DEAD" && player.down == 1;
			float downProgress = 0.0f;
			std::string rankIcon;
			std::string rankLevel;

			if (isDown)
			{
				downProgress = std::clamp(player.downProgress, 0.0f, 1.0f);
			}

			if (hasPlayer && player.rank >= 0)
			{
				rankIcon = GetZombieRankIcon(player.prestige);
				rankLevel = std::to_string(player.rank + 1);
			}

			const auto number = [hasPlayer](int value)
			{
				if (!hasPlayer)
				{
					return std::string();
				}

				return std::to_string(value);
			};

			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_name", row), player.name);
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_score", row), number(player.score));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_kills", row), number(player.kills));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_downs", row), number(player.downs));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_revives", row), number(player.revives));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_deaths", row), number(player.deaths));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_ping", row), number(player.ping));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_icon", row), player.icon);
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_status", row), player.status);
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_survival_time", row), player.survivalTime);
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_down", row), Choose(isDown, "1", "0"));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_down_progress", row), Utils::String::VA("%g", downProgress));
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_rank_icon", row), rankIcon);
			SetUIDvar(Utils::String::VA("zw3_ui_sb_p%d_rank_level", row), rankLevel);
		}
	}

	void ServerInfo::ApplyScoreboardSnapshot(const std::string& data)
	{
		playerContainer.currentPlayer = 0;
		playerContainer.playerList.clear();

		std::string survivalTime;

		for (const auto& line : Utils::String::Split(data, '\n'))
		{
			if (line.empty())
			{
				continue;
			}

			const Utils::InfoString info(line);
			Container::Player player;

			player.name = info.Get("name");
			player.score = std::strtol(info.Get("score").data(), nullptr, 10);
			player.kills = std::strtol(info.Get("kills").data(), nullptr, 10);
			player.downs = std::strtol(info.Get("downs").data(), nullptr, 10);
			player.revives = std::strtol(info.Get("revives").data(), nullptr, 10);
			player.deaths = std::strtol(info.Get("deaths").data(), nullptr, 10);
			player.ping = std::strtol(info.Get("ping").data(), nullptr, 10);
			player.icon = info.Get("icon");
			player.status = info.Get("status");
			player.down = std::strtol(info.Get("down").data(), nullptr, 10);
			player.downProgress = static_cast<float>(std::atof(info.Get("downProgress").data()));
			player.survivalTime = info.Get("survivalTime");

			const auto rank = info.Get("rank");
			const auto prestige = info.Get("prestige");

			if (!rank.empty())
			{
				player.rank = std::strtol(rank.data(), nullptr, 10);
			}

			if (!prestige.empty())
			{
				player.prestige = std::strtol(prestige.data(), nullptr, 10);
			}

			if (survivalTime.empty())
			{
				survivalTime = player.survivalTime;
			}

			NormalisePlayerDownState(player);
			playerContainer.playerList.push_back(player);
		}

		if (Dvar::Var("zw3_ui_sb_survived_time").Get<std::string>() != survivalTime)
		{
			SetUIDvar("zw3_ui_sb_survived_time", survivalTime);
		}

		WriteScoreboardRowDvars();
	}

	void ServerInfo::RefreshScoreboard([[maybe_unused]] const UIScript::Token& token)
	{
		if (!Dedicated::IsRunning() && !Party::IsHostingParty())
		{
			static int lastRequest = 0;
			const auto now = Game::Sys_Milliseconds();

			if (now - lastRequest >= 250)
			{
				lastRequest = now;
				Network::SendCommand(Party::Target(), "getZW3Scoreboard");
			}

			return;
		}

		playerContainer.currentPlayer = 0;
		playerContainer.playerList.clear();
		UpdateListenServerHostClientNum();

		const auto now = Game::Sys_Milliseconds();
		const auto survivalTime = Dvar::Var("zw3_ui_sb_survived_time").Get<std::string>();
		std::unordered_set<std::uint64_t> publishedXuids;

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			const auto& serverClient = Game::svs_clients[clientNum];

			if (serverClient.header.state < Game::CS_CONNECTED)
			{
				GSC::Field::ResetClientScoreboardStats(clientNum);
				ResetTransientScoreboardState(clientNum, 0);
				ResetConnectingState(clientNum);
				ResetZombieRankState(clientNum);
				continue;
			}

			UpdateConnectingState(clientNum, now);

			const auto xuid = CharacterAssignments::GetClientXuid(serverClient);
			const bool isBot = serverClient.bIsTestClient && xuid == 0;

			if (!isBot && IsConnectingStale(clientNum))
			{
				continue;
			}

			if (!isBot && xuid != 0 && !publishedXuids.insert(xuid).second)
			{
				continue;
			}

			const auto character = GetCharacterForClient(clientNum);

			if (character.empty())
			{
				continue;
			}

			Container::Player player;
			player.clientNum = clientNum;
			player.icon = character;
			player.ping = serverClient.ping;
			player.survivalTime = survivalTime;

			const bool hasGameClient = serverClient.header.state >= Game::CS_ACTIVE && serverClient.gentity && serverClient.gentity->client;

			if (!hasGameClient)
			{
				if (isBot || clientNum == listenServerHostClientNum || isConnectingStale[clientNum])
				{
					continue;
				}

				player.name = GetScoreboardBaseName(serverClient.name);

				if (player.name.empty())
				{
					player.name = "Connecting...";
				}

				player.status = "CONNECTING";
				playerContainer.playerList.push_back(player);
				continue;
			}

			player.name = GetScoreboardBaseName(serverClient.name);

			if (isBot)
			{
				player.name = std::format("[BOT] {}", character);
			}

			const auto* const client = serverClient.gentity->client;
			player.score = Game::G_GetClientScore(clientNum);
			player.kills = client->sess.kills;
			player.downs = GSC::Field::GetClientDowns(clientNum);
			player.revives = GSC::Field::GetClientRevives(clientNum);
			player.deaths = client->sess.deaths;
			player.rank = 0;
			player.prestige = 0;

			if (!isBot)
			{
				UpdateZombieRankState(clientNum, xuid, now);
				player.rank = zombieRanks[clientNum].level;
				player.prestige = zombieRanks[clientNum].prestige;
			}

			const bool isDead = client->sess.cs.team == Game::TEAM_SPECTATOR;
			const bool isDownSuppressed = now < downStateSuppressedUntil[clientNum];

			if (!isDead && !isDownSuppressed)
			{
				const auto down = Dvar::Var(Utils::String::VA("zw3_sb_down_%d", clientNum)).Get<std::string>();
				const auto progress = Dvar::Var(Utils::String::VA("zw3_sb_down_progress_%d", clientNum)).Get<std::string>();

				if (std::strtol(down.data(), nullptr, 10) == 1)
				{
					player.down = 1;
					player.downProgress = std::clamp(static_cast<float>(std::atof(progress.data())), 0.0f, 1.0f);
				}
			}

			player.status = GetScoreboardStatusName(isDead, player.down == 1);

			NormalisePlayerDownState(player);
			playerContainer.playerList.push_back(player);
		}

		const auto group = [](const Container::Player& player)
		{
			if (!Dedicated::IsRunning() && player.clientNum == listenServerHostClientNum)
			{
				return 0;
			}

			if (Game::svs_clients[player.clientNum].bIsTestClient)
			{
				return 2;
			}

			return 1;
		};

		std::ranges::stable_sort(playerContainer.playerList, [&group](const Container::Player& a, const Container::Player& b)
		{
			const int groupA = group(a);
			const int groupB = group(b);

			if (groupA != groupB)
			{
				return groupA < groupB;
			}

			return a.clientNum < b.clientNum;
		});

		WriteScoreboardRowDvars();
	}

	Utils::InfoString ServerInfo::GetHostInfo()
	{
		Utils::InfoString info;

		info.Set("admin", Dvar::Var("_Admin").Get<std::string>());
		info.Set("website", Dvar::Var("_Website").Get<std::string>());
		info.Set("email", Dvar::Var("_Email").Get<std::string>());
		info.Set("location", Dvar::Var("_Location").Get<std::string>());

		return info;
	}

	Utils::InfoString ServerInfo::GetInfo()
	{
		int maxClientCount = *Game::svs_clientCount;
		const auto password = Dvar::Var("g_password").Get<std::string>();

		if (!maxClientCount)
		{
			const Dvar::Var partyMaxPlayers("party_maxplayers");
			maxClientCount = 18;

			if (partyMaxPlayers.IsValid())
			{
				maxClientCount = partyMaxPlayers.Get<int>();
			}
		}

		Utils::InfoString info(Game::Dvar_InfoString_Big(Game::DVAR_SERVERINFO));
		info.Set("gamename", "IW4");
		info.Set("sv_maxclients", std::to_string(maxClientCount));
		info.Set("protocol", std::to_string(iw4xProtocol));
		info.Set("version", Dvar::Var("version").Get<std::string>());
		info.Set("mapname", Dvar::Var("mapname").Get<std::string>());
		info.Set("isPrivate", Choose(!password.empty(), "1", "0"));
		info.Set("checksum", Utils::String::VA("%X", Utils::Cryptography::JenkinsOneAtATime::Compute(std::to_string(Game::Sys_Milliseconds()))));
		info.Set("aimAssist", Choose(Gamepad::sv_allowAimAssist.Get<bool>(), "1", "0"));
		info.Set("voiceChat", Choose(Voice::SV_VoiceEnabled(), "1", "0"));

		if (Dedicated::IsRunning())
		{
			info.Set("zwnet_selected_map", Dvar::Var("zwnet_selected_map").Get<std::string>());
			info.Set("g_gametype", Dvar::Var("g_gametype").Get<std::string>());
			info.Set("zwnet_round", Dvar::Var("round").Get<std::string>());
		}

		if (info.Get("mapname").empty())
		{
			info.Set("mapname", Dvar::Var("ui_mapname").Get<std::string>());
		}

		std::string matchType = "0";

		if (Party::IsEnabled() && Party::IsHostingParty())
		{
			matchType = "1";
		}
		else if (Dedicated::IsRunning())
		{
			matchType = "2";
		}

		info.Set("matchtype", matchType);

		return info;
	}

	void ServerInfo::HandleGetStatus(Network::Address& address, const std::string& data)
	{
		std::string playerList;

		auto info = GetInfo();
		info.Set("challenge", ParseChallenge(data));

		for (std::size_t i = 0; i < ClientSlots::SentClientCount(); ++i)
		{
			int score = 0;
			int ping = 0;
			std::string name;

			const auto& client = Game::svs_clients[i];

			if (Dedicated::IsRunning())
			{
				const bool isInGame = client.header.state >= Game::CS_ACTIVE && client.gentity && client.gentity->client;

				if (!isInGame || client.bIsTestClient)
				{
					continue;
				}

				score = Game::G_GetClientScore(static_cast<int>(i));
				ping = client.ping;
				name = client.name;
			}
			else
			{
				const auto* const memberName = Game::PartyHost_GetMemberName(Game::g_lobbyData, static_cast<int>(i));

				if (!memberName || !*memberName)
				{
					continue;
				}

				name = memberName;
			}

			playerList.append(std::format("{} {} \"{}\"\n", score, ping, name));
		}

		Network::SendCommand(address, "statusResponse", info.Build() + "\n" + playerList + "\n");
	}

	void ServerInfo::HandleStatusResponse(Network::Address& address, const std::string& data)
	{
		if (playerContainer.target != address)
		{
			return;
		}

		const auto end = data.find_first_of('\n');

		if (end == std::string::npos)
		{
			return;
		}

		const Utils::InfoString info(data.substr(0, end));

		SetUIDvar("uiSi_ServerName", info.Get("sv_hostname"));
		SetUIDvar("uiSi_MaxClients", info.Get("sv_maxclients"));
		SetUIDvar("uiSi_Version", info.Get("version"));
		SetUIDvar("uiSi_SecurityLevel", info.Get("sv_securityLevel"));
		SetUIDvar("uiSi_isPrivate", Choose(info.Get("isPrivate") == "0", "@MENU_NO", "@MENU_YES"));
		SetUIDvar("uiSi_Hardcore", Choose(info.Get("g_hardcore") == "0", "@MENU_DISABLED", "@MENU_ENABLED"));
		SetUIDvar("uiSi_KillCam", Choose(info.Get("scr_game_allowkillcam") == "0", "@MENU_NO", "@MENU_YES"));
		SetUIDvar("uiSi_MapName", info.Get("mapname"));
		SetUIDvar("uiSi_MapNameLoc", Localization::LocalizeMapName(info.Get("mapname").data()));
		SetUIDvar("uiSi_GameType", Game::UI_GetGameTypeDisplayName(info.Get("g_gametype").data()));
		SetUIDvar("uiSi_ModName", "");
		SetUIDvar("uiSi_aimAssist", Choose(info.Get("aimAssist") == "0", "@MENU_DISABLED", "@MENU_ENABLED"));
		SetUIDvar("uiSi_voiceChat", Choose(info.Get("voiceChat") == "0", "@MENU_DISABLED", "@MENU_ENABLED"));

		switch (std::strtol(info.Get("scr_team_fftype").data(), nullptr, 10))
		{
		case 1:
			SetUIDvar("uiSi_ffType", "@MENU_ENABLED");
			break;
		case 2:
			SetUIDvar("uiSi_ffType", "@MPUI_RULES_REFLECT");
			break;
		case 3:
			SetUIDvar("uiSi_ffType", "@MPUI_RULES_SHARED");
			break;
		default:
			SetUIDvar("uiSi_ffType", "@MENU_DISABLED");
			break;
		}

		const auto mod = info.Get("fs_game");

		if (Utils::String::StartsWith(mod, "mods/"))
		{
			SetUIDvar("uiSi_ModName", mod.substr(5));
		}

		const auto lines = Utils::String::Split(data, '\n');

		for (std::size_t i = 1; i < lines.size(); ++i)
		{
			std::string line = lines[i];

			if (line.size() < 3)
			{
				continue;
			}

			Container::Player player{};

			player.score = std::strtol(line.substr(0, line.find_first_of(' ')).data(), nullptr, 10);
			line = line.substr(line.find_first_of(' ') + 1);

			player.ping = std::strtol(line.substr(0, line.find_first_of(' ')).data(), nullptr, 10);
			line = line.substr(line.find_first_of(' ') + 1);

			if (!line.empty() && line.front() == '"')
			{
				line = line.substr(1);
			}

			if (!line.empty() && line.back() == '"')
			{
				line.pop_back();
			}

			player.name = line;

			playerContainer.playerList.push_back(player);
		}
	}

	std::string ServerInfo::ParseChallenge(const std::string& data)
	{
		std::string challenge = data;

		Utils::String::Trim(challenge);

		const auto space = challenge.find_first_of(" \t\r\n");

		if (space != std::string::npos)
		{
			challenge.erase(space);
		}

		return challenge;
	}

	int ServerInfo::GetProtocol()
	{
		return iw4xProtocol;
	}

	ServerInfo::ServerInfo()
	{
		playerContainer.currentPlayer = 0;

		UIScript::Add("ServerStatus", ServerStatus);
		UIScript::Add("RefreshScoreboard", RefreshScoreboard);

		UIScript::Add("RefreshLobbyRanks", [](const UIScript::Token&)
		{
			RefreshLobbyRanks();
		});

		UIScript::Add("RefreshZWNetLobbyRanks", [](const UIScript::Token&)
		{
			RefreshZWNetLobbyRanks();
		});

		UIFeeder::Add(playerListFeeder, GetPlayerCount, GetPlayerText, SelectPlayer);

		Network::OnPacket("getStatus", HandleGetStatus);
		Network::OnPacket("statusResponse", HandleStatusResponse);

		Network::OnPacket("getZW3LobbyRanks", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			if (!Party::IsHostingParty())
			{
				return;
			}

			const auto snapshot = BuildLobbyRankSnapshot();
			ApplyLobbyRankSnapshot(snapshot);

			Network::SendCommand(address, "zw3LobbyRankUpdate", snapshot);
		});

		Network::OnPacket("zw3LobbyRankUpdate", [](Network::Address&, const std::string& data)
		{
			if (Party::IsHostingParty())
			{
				return;
			}

			ApplyLobbyRankSnapshot(data);
		});

		Network::OnPacket("getZW3Scoreboard", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			if (!Dedicated::IsRunning() && !Party::IsHostingParty())
			{
				return;
			}

			RefreshScoreboard(UIScript::Token());

			std::string response;
			const auto survivalTime = Dvar::Var("zw3_ui_sb_survived_time").Get<std::string>();

			for (const auto& player : playerContainer.playerList)
			{
				const bool isDown = player.status != "DEAD" && player.down == 1;
				float downProgress = 0.0f;

				if (isDown)
				{
					downProgress = std::clamp(player.downProgress, 0.0f, 1.0f);
				}

				Utils::InfoString info;
				info.Set("name", player.name);
				info.Set("score", std::to_string(player.score));
				info.Set("kills", std::to_string(player.kills));
				info.Set("downs", std::to_string(player.downs));
				info.Set("revives", std::to_string(player.revives));
				info.Set("deaths", std::to_string(player.deaths));
				info.Set("ping", std::to_string(player.ping));
				info.Set("rank", std::to_string(player.rank));
				info.Set("prestige", std::to_string(player.prestige));
				info.Set("icon", player.icon);
				info.Set("status", player.status);
				info.Set("down", Choose(isDown, "1", "0"));
				info.Set("downProgress", std::to_string(downProgress));
				info.Set("survivalTime", survivalTime);

				response.append(info.Build());
				response.append("\n");
			}

			Network::SendCommand(address, "zw3ScoreboardResponse", response);
		});

		Network::OnPacket("zw3ScoreboardResponse", [](Network::Address&, const std::string& data)
		{
			if (Party::IsHostingParty())
			{
				return;
			}

			ApplyScoreboardSnapshot(data);
		});

		Events::OnClientDisconnect([](int clientNum)
		{
			GSC::Field::ResetClientScoreboardStats(clientNum);
			ResetTransientScoreboardState(clientNum, 1500);
			ResetConnectingState(clientNum);
			ResetZombieRankState(clientNum);

			if (listenServerHostClientNum == clientNum)
			{
				listenServerHostClientNum = -1;
			}
		});

		Scheduler::Loop([]
		{
			static std::array<int, Game::MAX_CLIENTS> previousStates{};
			static std::array<bool, Game::MAX_CLIENTS> previousBotFlags{};
			static std::array<std::string, Game::MAX_CLIENTS> previousNames{};
			static bool isInitialized = false;

			const auto now = Game::Sys_Milliseconds();

			for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
			{
				const auto& client = Game::svs_clients[clientNum];
				const auto state = client.header.state;
				const bool hasOccupant = state >= Game::CS_CONNECTED;
				const bool hadOccupant = isInitialized && previousStates[clientNum] >= Game::CS_CONNECTED;
				const bool isBot = hasOccupant && client.bIsTestClient;

				std::string currentName;

				if (hasOccupant)
				{
					currentName = client.name;
				}

				const bool didConnect = hasOccupant && !hadOccupant;
				const bool didBecomeActive = isInitialized && state >= Game::CS_ACTIVE && previousStates[clientNum] < Game::CS_ACTIVE;
				const bool didChangeKind = hasOccupant && hadOccupant && previousBotFlags[clientNum] != isBot;
				const bool didChangeName = hasOccupant && hadOccupant && CharacterAssignments::Normalize(previousNames[clientNum]) != CharacterAssignments::Normalize(currentName);
				const bool didLeave = !hasOccupant && hadOccupant;

				if (didLeave || didConnect || didBecomeActive || didChangeKind || didChangeName)
				{
					ResetTransientScoreboardState(clientNum, 1500);
					ResetConnectingState(clientNum);
					ResetZombieRankState(clientNum);
				}

				if (hasOccupant && !isBot && state < Game::CS_ACTIVE)
				{
					ResetTransientScoreboardState(clientNum, 1500);
				}

				UpdateConnectingState(clientNum, now);

				previousStates[clientNum] = state;
				previousBotFlags[clientNum] = isBot;
				previousNames[clientNum] = currentName;
			}

			isInitialized = true;
		}, Scheduler::Pipeline::MAIN);

		Scheduler::Loop([]
		{
			static int lastRefresh = 0;

			UpdateListenServerHostClientNum();

			if (!IsMenuVisible("scoreboard"))
			{
				return;
			}

			const auto now = Game::Sys_Milliseconds();

			if (now - lastRefresh < 250)
			{
				return;
			}

			lastRefresh = now;

			RefreshScoreboard(UIScript::Token());
		}, Scheduler::Pipeline::MAIN);

		Scheduler::Loop([]
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
		}, Scheduler::Pipeline::MAIN, 250ms);

		if (!Utils::Hook::BranchesTo(CG_DrawScoreboard_BodyCall, scoreboardBody, HOOK_CALL))
		{
			Logger::Error("serverinfo: 0x{:X} no longer calls the scoreboard body, the engine's board stays\n", CG_DrawScoreboard_BodyCall);
			return;
		}

		Utils::Hook::Nop(CG_DrawScoreboard_BodyCall, 5);
	}
}
