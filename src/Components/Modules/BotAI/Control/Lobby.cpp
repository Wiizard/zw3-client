#include "STDInclude.hpp"

#include "Components/Modules/BotAI/Control/Lobby.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include "Components/Modules/Party.hpp"
#include "Components/Modules/UIFeeder.hpp"

namespace Components::BotAI
{
	static constexpr float listFeeder = 48.0f;
	static constexpr float nameFeeder = 18.0f;
	static constexpr float rankFeeder = 39.0f;
	static constexpr float scoreFeeder = 35.0f;
	static constexpr float voiceFeeder = 19.0f;

	static constexpr int lobbyLookup = 3;

	static constexpr int rankDisplayColumn = 14;

	static constexpr std::uintptr_t GetPlayerCardClientData_PartyMemberCall = 0x140251B0A;
	static constexpr std::uintptr_t PlayerCards_GetPartyMemberData = 0x1401E6910;

	static constexpr int partyMemberSlots = 18;
	static constexpr std::size_t partyMemberStride = 224;
	static constexpr std::size_t partyMemberStatus = 0x110;
	static constexpr unsigned char memberPresent = 3;

	static constexpr int firstRowStaleMs = 500;

	static constexpr std::uintptr_t Party_SetUIPlayerCount_ConversionCall = 0x140116C92;
	static constexpr std::uintptr_t UI_ReplaceConversionInts = 0x140271A70;

	static Identity roster[maxClients];
	static int rosterCount = 0;
	static int handedOutCount = 0;
	static int countedRosterSize = 0;

	static int firstRow = 0;
	static int firstRowTime = 0;

	static Game::PlayerCardData botCard;

	static Utils::Hook cardHook;
	static Utils::Hook playerCountHook;

	static int SyncRoster()
	{
		if (!Party::IsHostingParty())
		{
			return 0;
		}

		const Game::dvar_t* const maxClientsDvar = *Game::sv_maxclients;

		if (!maxClientsDvar)
		{
			return 0;
		}

		int planned = PlannedBotCount(Game::GetLobbyMemberCount(), maxClientsDvar->current.integer);

		if (planned > maxClients)
		{
			planned = maxClients;
		}

		const char* taken[maxClients + partyMemberSlots] = {};
		int memberNameCount = 0;
		const auto* const party = reinterpret_cast<const std::uint8_t*>(Game::g_lobbyData);

		for (int slot = 0; slot < partyMemberSlots; ++slot)
		{
			if (party[partyMemberStatus + slot * partyMemberStride] >= memberPresent)
			{
				taken[memberNameCount] = Game::PartyHost_GetMemberName(Game::g_lobbyData, slot);
				++memberNameCount;
			}
		}

		while (rosterCount < planned)
		{
			for (int i = 0; i < rosterCount; ++i)
			{
				taken[memberNameCount + i] = roster[i].name;
			}

			roster[rosterCount] = RollIdentity(taken, memberNameCount + rosterCount);
			++rosterCount;
		}

		rosterCount = planned;

		if (rosterCount != countedRosterSize)
		{
			countedRosterSize = rosterCount;
			Game::Party_SetUIPlayerCount(Game::g_lobbyData);
		}

		return rosterCount;
	}

	static int BotRowCount()
	{
		handedOutCount = 0;
		return SyncRoster();
	}

	static const char* LobbyPlayerCount(const char* text, int count, int* values)
	{
		const int botRows = SyncRoster();

		if (botRows == 0 || count != 2)
		{
			return Game::UI_ReplaceConversionInts(text, count, values);
		}

		int withBots[2] = { Game::GetLobbyMemberCount() + botRows, (*Game::sv_maxclients)->current.integer };
		return Game::UI_ReplaceConversionInts(text, count, withBots);
	}

	static int LobbyCount(int localClientNum, float feeder)
	{
		const int botRows = BotRowCount();

		if (botRows == 0)
		{
			return Game::UI_FeederCount(localClientNum, feeder);
		}

		return Game::GetLobbyMemberCount() + botRows;
	}

	static void TrackCursorRow(int localClientNum, Game::itemDef_s* item, int index)
	{
		const Game::listBoxDef_s* const listBox = Game::Item_GetListBoxDef(item);

		if (!listBox)
		{
			return;
		}

		firstRow = listBox->startPos[localClientNum];
		firstRowTime = Game::Sys_Milliseconds();

		if (item->cursorPos[localClientNum] != index)
		{
			return;
		}

		std::uint64_t xuid = 0;

		if (index < Game::GetLobbyMemberCount())
		{
			xuid = Game::GetLobbyMemberXuid(localClientNum, index);
			*Game::selectedPlayerXuid = xuid;
		}

		Game::UpdatePartyDvars(localClientNum, index - firstRow, Game::g_lobbyData, xuid);
	}

	static const char* LobbyText(int localClientNum, Game::itemDef_s* item, float feeder,
		int index, int column, float* a6, float* a7, float* a8, float* a9, Game::Material** material)
	{
		if (BotRowCount() == 0)
		{
			return Game::UI_FeederItemText(localClientNum, item, feeder, index, column,
				a6, a7, a8, a9, material);
		}

		if (material)
		{
			*material = nullptr;
		}

		if (feeder == listFeeder)
		{
			TrackCursorRow(localClientNum, item, index);
			return "";
		}

		const int memberCount = Game::GetLobbyMemberCount();

		if (index < memberCount)
		{
			return Game::UI_FeederItemText(localClientNum, item, feeder, index, column,
				a6, a7, a8, a9, material);
		}

		const int bot = index - memberCount;

		if (bot >= rosterCount)
		{
			return "";
		}

		const Identity& identity = roster[bot];

		if (feeder == nameFeeder)
		{
			return identity.name;
		}

		if (feeder == rankFeeder)
		{
			if (column != 0)
			{
				return Game::CL_GetRankData(identity.rank, rankDisplayColumn);
			}

			if (material)
			{
				Game::CL_GetRankIcon(identity.rank, identity.prestige, material);
			}

			return "";
		}

		if (feeder == scoreFeeder)
		{
			if (column == 1)
			{
				return "|";
			}

			return "0";
		}

		return "";
	}

	static void LobbyColor(int localClientNum, Game::itemDef_s* item, float feeder,
		int index, int column, float* color)
	{
		if (BotRowCount() == 0 || index < Game::GetLobbyMemberCount())
		{
			Game::UI_FeederItemColor(localClientNum, item, feeder, index, column, color);
			return;
		}

		std::memcpy(color, item->window.foreColor, sizeof(item->window.foreColor));
	}

	static int LobbyDoubleClick(int localClientNum, float feeder, int index)
	{
		if (BotRowCount() > 0 && index >= Game::GetLobbyMemberCount())
		{
			return 1;
		}

		return Game::UI_FeederDoubleClick(localClientNum, feeder, index);
	}

	static Game::PlayerCardData* PartyMemberCard(int localClientNum, int lookupType, unsigned int index)
	{
		const bool isLobbyInView = Game::Sys_Milliseconds() - firstRowTime < firstRowStaleMs;

		if (lookupType != lobbyLookup || rosterCount == 0 || !isLobbyInView)
		{
			return Game::PlayerCards_GetPartyMemberData(localClientNum, lookupType, index);
		}

		const unsigned int row = index + static_cast<unsigned int>(firstRow);
		const unsigned int memberCount = static_cast<unsigned int>(Game::GetLobbyMemberCount());

		if (row < memberCount || row - memberCount >= static_cast<unsigned int>(rosterCount))
		{
			return Game::PlayerCards_GetPartyMemberData(localClientNum, lookupType, row);
		}

		const Identity& identity = roster[row - memberCount];

		botCard = {};
		botCard.lastUpdateTime = static_cast<unsigned int>(Game::PartyUI_GetSelectedPlayerListChangedTime());
		botCard.titleIndex = static_cast<unsigned int>(identity.cardTitle);
		botCard.iconIndex = static_cast<unsigned int>(identity.cardIcon);
		botCard.nameplateIndex = static_cast<unsigned int>(identity.cardNameplate);
		botCard.rank = identity.rank;
		botCard.prestige = identity.prestige;
		strncpy_s(botCard.name, identity.name, _TRUNCATE);

		return &botCard;
	}

	Identity TakeLobbyIdentity(const char* const* takenNames, int takenCount)
	{
		while (handedOutCount < rosterCount)
		{
			const Identity& entry = roster[handedOutCount];
			++handedOutCount;

			bool isTaken = false;

			for (int i = 0; i < takenCount; ++i)
			{
				if (takenNames[i] && std::strcmp(takenNames[i], entry.name) == 0)
				{
					isTaken = true;
					break;
				}
			}

			if (!isTaken)
			{
				return entry;
			}
		}

		return RollIdentity(takenNames, takenCount);
	}

	bool InstallLobby()
	{
		if (!Utils::Hook::BranchesTo(GetPlayerCardClientData_PartyMemberCall, PlayerCards_GetPartyMemberData, false)
			|| !Utils::Hook::BranchesTo(Party_SetUIPlayerCount_ConversionCall, UI_ReplaceConversionInts, false))
		{
			return false;
		}

		bool isSeated = cardHook.Initialize(GetPlayerCardClientData_PartyMemberCall, reinterpret_cast<void*>(PartyMemberCard), HOOK_CALL)->Install()->IsInstalled();
		isSeated = playerCountHook.Initialize(Party_SetUIPlayerCount_ConversionCall, reinterpret_cast<void*>(LobbyPlayerCount), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			cardHook.Uninstall();
			playerCountHook.Uninstall();
			return false;
		}

		cardHook.Quick();
		playerCountHook.Quick();

		const UIFeeder::EngineCallbacks callbacks = { LobbyCount, LobbyText, LobbyColor, LobbyDoubleClick };

		UIFeeder::Extend(listFeeder, callbacks);
		UIFeeder::Extend(nameFeeder, callbacks);
		UIFeeder::Extend(rankFeeder, callbacks);
		UIFeeder::Extend(scoreFeeder, callbacks);
		UIFeeder::Extend(voiceFeeder, callbacks);

		return true;
	}
}
