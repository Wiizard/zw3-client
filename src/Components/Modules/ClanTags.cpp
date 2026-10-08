#include "STDInclude.hpp"

#include "ClanTags.hpp"
#include "ClientSlots.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "PlayerName.hpp"
#include "ServerCommands.hpp"

namespace Components
{
	extern "C"
	{
		void ClientUserinfoChangedStub();
		void ClientConnectUserinfoStub();
		void ScoreboardNameStub();
		std::uintptr_t ClanTags_ScoreboardNameNext = 0;

		const char* ClanTags_UserinfoChanged(const char* s, const char* key, int clientNum)
		{
			ClanTags::ClientUserinfoChanged(s, clientNum);
			return Game::Info_ValueForKey(s, key);
		}

		const char* ClanTags_GetClanTagWithName(int clientNum, const char* playerName)
		{
			return ClanTags::GetClanTagWithName(clientNum, playerName);
		}
	}

	const Game::dvar_t* ClanTags::clanName;

	char ClanTags::clientState[Game::MAX_CLIENTS][MAX_CLAN_NAME_LENGTH];

	constexpr std::uintptr_t Dvar_InfoString_NameCall = 0x1401FC411;
	constexpr std::uintptr_t Info_SetValueForKey = 0x14028C5E0;

	constexpr std::uintptr_t ClientUserinfoChanged_NameCall = 0x1401969CA;
	constexpr std::uintptr_t ClientConnect_NameCall = 0x140196088;
	constexpr std::uintptr_t Info_ValueForKey = 0x14028CA30;

	constexpr std::uintptr_t CG_DrawClientScore_NameColumn = 0x1400E3D82;
	constexpr std::uintptr_t CG_DrawClientScore_ColumnTail = 0x1400E3DE1;
	static const std::uint8_t nameColumn[] = { 0x48, 0x8D, 0x53, 0x0C, 0xEB, 0x59 };

	constexpr std::uintptr_t PartyClient_Frame_StrcmpCall = 0x140106468;
	constexpr std::uintptr_t PartyClient_Frame_UpdateClanNameCall = 0x14010648B;
	constexpr std::uintptr_t I_strcmp = 0x14028C0A0;
	constexpr std::uintptr_t Party_UpdateClanName = 0x14010C300;

	constexpr std::uintptr_t PlayerCards_SetCachedPlayerData_NameCall = 0x1401E6C0C;
	constexpr std::uintptr_t PlayerCards_SetCachedPlayerData_ClearClan = 0x1401E6C11;
	static const std::uint8_t clearClan[] = { 0xC6, 0x43, 0x3C, 0x00 };
	constexpr std::uintptr_t I_strncpyz = 0x14028C390;
	constexpr std::uintptr_t firstClientInfoNameOffset = 0xC;
	constexpr std::uintptr_t clientInfoSize = 0x548;

	constexpr std::uintptr_t GetPlayerCardClientData_ForClientCall = 0x140251ADD;
	constexpr std::uintptr_t GetPlayerCardClientData_ForControllerCall = 0x140251AE9;
	constexpr std::uintptr_t PlayerCards_GetLiveProfileDataForClient = 0x1401E6770;
	constexpr std::uintptr_t PlayerCards_GetLiveProfileDataForController = 0x1401E6850;

	constexpr std::uintptr_t CG_Obituary_ClientNameCalls[] = { 0x1400AF0F4, 0x1400AF14C };
	constexpr std::uintptr_t CL_GetClientName = 0x140101D80;

	static Utils::Hook hooks[10];
	static Utils::Hook scoreboardNameHook;

	const char* ClanTags::GetClanTagWithName(int clientNum, const char* playerName)
	{
		AssertIn(clientNum, Game::MAX_CLIENTS);

		if (clientState[clientNum][0] == '\0')
		{
			return playerName;
		}

		return Utils::String::VA("[%s^7]%s", clientState[clientNum], playerName);
	}

	void ClanTags::SendClanTagsToClients()
	{
		std::string list;

		for (std::size_t i = 0; i < ClientSlots::SentClientCount(); ++i)
		{
			list.append(std::format("\\{}\\{}", i, clientState[i]));
		}

		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, Utils::String::Format("{:c} clanNames \"{}\"", 22, list));
	}

	void ClanTags::ParseClanTags(const char* infoString)
	{
		for (std::size_t i = 0; i < Game::MAX_CLIENTS; ++i)
		{
			const auto index = std::to_string(i);
			const auto* clanTag = Game::Info_ValueForKey(infoString, index.data());

			if (clanTag[0] == '\0')
			{
				clientState[i][0] = '\0';
			}
			else
			{
				Game::I_strncpyz(clientState[i], clanTag, sizeof(clientState[0]) / sizeof(char));
			}
		}
	}

	int ClanTags::CL_FilterChar(unsigned char input)
	{
		if (input == '^')
		{
			return ' ';
		}

		if (input < ' ')
		{
			return -1;
		}

		if (input == 188 || input == 189)
		{
			return -1;
		}

		return input;
	}

	void ClanTags::CL_SanitizeClanName()
	{
		char saneNameBuf[MAX_CLAN_NAME_LENGTH]{};
		auto* saneName = saneNameBuf;

		assert(clanName);
		const auto* currentName = clanName->current.string;

		if (currentName)
		{
			const auto nameLen = std::strlen(currentName);

			for (std::size_t i = 0; (i < nameLen) && (i < sizeof(saneNameBuf)); ++i)
			{
				const auto curChar = CL_FilterChar(static_cast<unsigned char>(currentName[i]));

				if (curChar > 0)
				{
					*saneName++ = curChar & 0xFF;
				}
			}

			saneNameBuf[sizeof(saneNameBuf) - 1] = '\0';
			Game::Dvar_SetString(clanName, saneNameBuf);
		}
	}

	char* ClanTags::GamerProfile_GetClanName(int controllerIndex)
	{
		AssertIn(controllerIndex, Game::MAX_LOCAL_CLIENTS);
		assert(clanName);

		CL_SanitizeClanName();
		Game::I_strncpyz(Game::gamerSettings[controllerIndex].exeConfig.clanPrefix, clanName->current.string, sizeof(Game::GamerSettingExeConfig::clanPrefix));

		return Game::gamerSettings[controllerIndex].exeConfig.clanPrefix;
	}

	void ClanTags::Dvar_InfoString_Stub(char* s, const char* key, const char* value)
	{
		const auto setValueForKey = reinterpret_cast<void(*)(char*, const char*, const char*)>(Utils::Hook::Rebase(Info_SetValueForKey));

		setValueForKey(s, key, value);

		setValueForKey(s, "clanAbbrev", GamerProfile_GetClanName(0));
	}

	void ClanTags::ClientUserinfoChanged(const char* s, int clientNum)
	{
		AssertIn(clientNum, Game::MAX_CLIENTS);

		const auto* clanAbbrev = Game::Info_ValueForKey(s, "clanAbbrev");

		if (clanAbbrev[0] == '\0')
		{
			clientState[clientNum][0] = '\0';
		}
		else
		{
			Game::I_strncpyz(clientState[clientNum], clanAbbrev, sizeof(clientState[0]) / sizeof(char));
		}
	}

	int ClanTags::PartyClient_Frame_Stub(const char* s0, [[maybe_unused]] const char* s1)
	{
		return reinterpret_cast<int(*)(const char*, const char*)>(Utils::Hook::Rebase(I_strcmp))(s0, GamerProfile_GetClanName(0));
	}

	void ClanTags::Party_UpdateClanName_Stub(Game::PartyData* party, [[maybe_unused]] const char* clanAbbrev)
	{
		reinterpret_cast<void(*)(Game::PartyData*, const char*)>(Utils::Hook::Rebase(Party_UpdateClanName))(party, GamerProfile_GetClanName(0));
	}

	void ClanTags::PlayerCards_SetCachedPlayerData(Game::PlayerCardData* data, const int clientNum)
	{
		Game::I_strncpyz(data->clanAbbrev, clientState[clientNum], sizeof(Game::PlayerCardData::clanAbbrev));
	}

	void ClanTags::PlayerCards_SetCachedPlayerData_Stub(char* name, const char* source, int size)
	{
		Game::I_strncpyz(name, source, size);

		auto* data = reinterpret_cast<Game::PlayerCardData*>(name - offsetof(Game::PlayerCardData, name));

		const auto firstName = reinterpret_cast<std::uintptr_t>(ClientSlots::CgameClientInfo(0)) + firstClientInfoNameOffset;
		const auto sourceAt = reinterpret_cast<std::uintptr_t>(source);

		if (sourceAt < firstName)
		{
			data->clanAbbrev[0] = '\0';
			return;
		}

		const std::size_t clientNum = (sourceAt - firstName) / clientInfoSize;

		if (clientNum >= Game::MAX_CLIENTS)
		{
			data->clanAbbrev[0] = '\0';
			return;
		}

		PlayerCards_SetCachedPlayerData(data, static_cast<int>(clientNum));
	}

	Game::PlayerCardData* ClanTags::PlayerCards_GetLiveProfileDataForClient_Stub(const unsigned int clientIndex)
	{
		auto* result = reinterpret_cast<Game::PlayerCardData*(*)(unsigned int)>(Utils::Hook::Rebase(PlayerCards_GetLiveProfileDataForClient))(clientIndex);
		Game::I_strncpyz(result->clanAbbrev, GamerProfile_GetClanName(static_cast<int>(clientIndex)), sizeof(Game::PlayerCardData::clanAbbrev));

		return result;
	}

	Game::PlayerCardData* ClanTags::PlayerCards_GetLiveProfileDataForController_Stub(const unsigned int controllerIndex)
	{
		auto* result = reinterpret_cast<Game::PlayerCardData*(*)(unsigned int)>(Utils::Hook::Rebase(PlayerCards_GetLiveProfileDataForController))(controllerIndex);
		AssertIn(controllerIndex, Game::MAX_LOCAL_CLIENTS);
		Game::I_strncpyz(result->clanAbbrev, GamerProfile_GetClanName(static_cast<int>(controllerIndex)), sizeof(Game::PlayerCardData::clanAbbrev));

		return result;
	}

	ClanTags::ClanTags()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
		};

		const HookSite sites[] =
		{
			{ Dvar_InfoString_NameCall, Info_SetValueForKey, reinterpret_cast<void*>(Dvar_InfoString_Stub) },
			{ ClientUserinfoChanged_NameCall, Info_ValueForKey, reinterpret_cast<void*>(ClientUserinfoChangedStub) },
			{ ClientConnect_NameCall, Info_ValueForKey, reinterpret_cast<void*>(ClientConnectUserinfoStub) },
			{ PartyClient_Frame_StrcmpCall, I_strcmp, reinterpret_cast<void*>(PartyClient_Frame_Stub) },
			{ PartyClient_Frame_UpdateClanNameCall, Party_UpdateClanName, reinterpret_cast<void*>(Party_UpdateClanName_Stub) },
			{ PlayerCards_SetCachedPlayerData_NameCall, I_strncpyz, reinterpret_cast<void*>(PlayerCards_SetCachedPlayerData_Stub) },
			{ GetPlayerCardClientData_ForClientCall, PlayerCards_GetLiveProfileDataForClient, reinterpret_cast<void*>(PlayerCards_GetLiveProfileDataForClient_Stub) },
			{ GetPlayerCardClientData_ForControllerCall, PlayerCards_GetLiveProfileDataForController, reinterpret_cast<void*>(PlayerCards_GetLiveProfileDataForController_Stub) },
			{ CG_Obituary_ClientNameCalls[0], CL_GetClientName, reinterpret_cast<void*>(PlayerName::GetClientName) },
			{ CG_Obituary_ClientNameCalls[1], CL_GetClientName, reinterpret_cast<void*>(PlayerName::GetClientName) },
		};

		static_assert(std::size(sites) == std::size(hooks));

		Events::OnDvarInit([]
		{
			clanName = Game::Dvar_RegisterString("clanName", "", Game::DVAR_ARCHIVE, "Your clan abbreviation");
		});

		std::memset(&clientState, 0, sizeof(char[Game::MAX_CLIENTS][MAX_CLAN_NAME_LENGTH]));

		ServerCommands::OnCommand(22, [](const Command::Params* params)
		{
			if (std::strcmp(params->Get(1), "clanNames") == 0)
			{
				if (params->Size() == 3)
				{
					ParseClanTags(params->Get(2));
					return true;
				}
			}

			return false;
		});

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, false))
			{
				Logger::Error("clantags: 0x{:X} no longer reaches 0x{:X}, no clan tags\n", hookSite.site, hookSite.target);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(CG_DrawClientScore_NameColumn, nameColumn, sizeof(nameColumn))
			|| !Utils::Hook::MatchesBytes(PlayerCards_SetCachedPlayerData_ClearClan, clearClan, sizeof(clearClan)))
		{
			Logger::Error("clantags: the scoreboard or the player card does not read as expected, no clan tags\n");
			return;
		}

		ClanTags_ScoreboardNameNext = Utils::Hook::Rebase(CG_DrawClientScore_ColumnTail);

		bool isSeated = scoreboardNameHook.Initialize(CG_DrawClientScore_NameColumn, ScoreboardNameStub, HOOK_CALL)->Install()->IsInstalled();

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			scoreboardNameHook.Uninstall();

			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("clantags: could not seat every hook, no clan tags\n");
			return;
		}

		scoreboardNameHook.Quick();

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		Utils::Hook::Nop(PlayerCards_SetCachedPlayerData_ClearClan, sizeof(clearClan));
	}
}
