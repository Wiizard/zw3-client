#include "STDInclude.hpp"

#include "Vote.hpp"
#include "ArenaLength.hpp"
#include "ClientCommand.hpp"
#include "ClientSlots.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "MapRotation.hpp"
#include "Menus.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"
#include "UIScript.hpp"

namespace Components
{
	constexpr std::uintptr_t level_maxclients = 0x1418673E0;
	constexpr std::uintptr_t level_time = 0x1418673E8;
	constexpr std::uintptr_t level_startTime = 0x1418673F4;
	constexpr std::uintptr_t level_numConnectedClients = 0x14186741C;

	constexpr std::uintptr_t sv_serverid_value = 0x1422CE688;

	constexpr std::uintptr_t SV_SetConfigstring = 0x14023ACB0;

	constexpr std::uintptr_t I_CleanStr = 0x14028BFC0;

	constexpr std::uintptr_t Scr_IsValidGameType = 0x1401A75A0;
	constexpr std::uintptr_t Scr_UpdateGameTypeList = 0x1401A7DF0;
	constexpr std::uintptr_t gameTypeCount = 0x1417C6700;
	constexpr std::uintptr_t gameTypeList = 0x1417C6704;
	constexpr std::size_t gameTypeSize = 132;
	constexpr std::size_t gameTypeDisplayNameOffset = 64;

	constexpr std::uintptr_t scr_const_call_vote = 0x1417CC4B4;
	constexpr std::uintptr_t scr_const_vote = 0x1417CC4CA;

	constexpr std::uintptr_t G_RegisterDvars_AllowVoteFlags = 0x14019DF75;

	static const std::uint8_t allowVoteFlags[] = { 0x41, 0xB8, 0x80, 0x00, 0x00, 0x00 };

	constexpr int CS_VOTE_TIME = 17;
	constexpr int CS_VOTE_STRING = 18;
	constexpr int CS_VOTE_YES = 19;
	constexpr int CS_VOTE_NO = 20;

	constexpr std::uintptr_t CG_Draw2D_Con_DrawSayCall = 0x1400D1006;
	constexpr std::uintptr_t Con_DrawSay = 0x1400ECF10;

	constexpr std::uintptr_t UI_GetFontHandle = 0x14026EE10;

	constexpr std::uintptr_t UI_GetKeyBindingLocalizedString = 0x140269830;

	constexpr std::uintptr_t SEH_LocalizeTextMessage = 0x14024EB90;

	constexpr std::uintptr_t cg_hudVotePosition = 0x1406BCF58;

	constexpr std::uintptr_t cg_time = 0x1404E1120;

	constexpr std::uintptr_t cls_serverId = 0x140C9DADC;

	static const float voteColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };

	constexpr std::uintptr_t sharedUiInfo_playerCount = 0x1465D0AF0;
	constexpr std::uintptr_t sharedUiInfo_playerNames = 0x1465D0AF4;
	constexpr std::size_t playerNameSize = 32;
	constexpr std::uintptr_t sharedUiInfo_numGameTypes = 0x1465D0FC0;
	constexpr std::uintptr_t sharedUiInfo_gameTypes = 0x1465D0FC4;
	constexpr std::size_t gameTypeInfoSize = 0x2C;
	constexpr std::uintptr_t sharedUiInfo_mapCount = 0x1465D2850;
	constexpr std::uintptr_t sharedUiInfo_mapList = 0x1465D2854;
	constexpr std::size_t stockMapInfoSize = 0xB00;
	constexpr std::size_t stockMapNameOffset = 32;

	constexpr std::uintptr_t uiInfo_playerIndex = 0x1466436C0;

	constexpr std::uintptr_t ui_currentMap = 0x1465D09B0;
	constexpr std::uintptr_t ui_netGametype = 0x1465D09F8;

	static Utils::Hook drawSayHook;

	struct VoteState
	{
		char voteString[1024];
		char voteDisplayString[1024];
		int voteTime;
		int voteExecuteTime;
		int voteYes;
		int voteNo;
	};

	static VoteState voteState;
	static std::array<bool, ClientSlots::CLIENT_LIMIT> hasVoted;
	static int levelStartTime = -1;

	Dvar::Var Vote::sv_votesRequired;
	Dvar::Var Vote::g_oldVoting;
	Dvar::Var Vote::g_voteAbstainWeight;

	std::unordered_map<std::string, Vote::CommandHandler> Vote::voteCommands =
	{
		{ "map_restart", HandleMapRestart },
		{ "map_rotate", HandleMapRotate },
		{ "typemap", HandleTypemap },
		{ "map", HandleMap },
		{ "g_gametype", HandleGametype },
		{ "kick", HandleKick },
		{ "tempBanUser", HandleKick },
	};

	template <typename T>
	static T& LevelField(std::uintptr_t address)
	{
		return *reinterpret_cast<T*>(Utils::Hook::Rebase(address));
	}

	static void SetConfigstring(int index, const char* value)
	{
		reinterpret_cast<void(*)(int, const char*)>(Utils::Hook::Rebase(SV_SetConfigstring))(index, value);
	}

	static bool IsValidGameType(const char* gameType)
	{
		return reinterpret_cast<bool(*)(const char*)>(Utils::Hook::Rebase(Scr_IsValidGameType))(gameType);
	}

	static const char* GetGameTypeNameForScript(const char* gameType)
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Scr_UpdateGameTypeList))();

		const int count = LevelField<int>(gameTypeCount);
		const auto* const list = reinterpret_cast<const char*>(Utils::Hook::Rebase(gameTypeList));

		for (int i = 0; i < count; ++i)
		{
			const char* const entry = list + i * gameTypeSize;

			if (!_stricmp(entry, gameType))
			{
				return entry + gameTypeDisplayNameOffset;
			}
		}

		return nullptr;
	}

	static int ClientNumber(const Game::gentity_s* ent)
	{
		return static_cast<int>(ent - Game::g_entities);
	}

	static void ResetOnNewLevel()
	{
		const int startTime = LevelField<int>(level_startTime);

		if (startTime == levelStartTime)
		{
			return;
		}

		levelStartTime = startTime;
		voteState = {};
		hasVoted.fill(false);
	}

	void Vote::DisplayVote(const Game::gentity_s* ent)
	{
		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_CALLEDAVOTE\x15%s\"", 0x65, ent->client->sess.cs.name));

		voteState.voteNo = 0;
		voteState.voteYes = 1;
		voteState.voteTime = LevelField<int>(level_time) + 30000;

		hasVoted.fill(false);
		hasVoted[ClientNumber(ent)] = true;

		SetConfigstring(CS_VOTE_TIME, Utils::String::VA("%i %i", voteState.voteTime, LevelField<int>(sv_serverid_value)));
		SetConfigstring(CS_VOTE_STRING, voteState.voteDisplayString);
		SetConfigstring(CS_VOTE_YES, Utils::String::VA("%i", voteState.voteYes));
		SetConfigstring(CS_VOTE_NO, Utils::String::VA("%i", voteState.voteNo));
	}

	int Vote::VotesRequired()
	{
		const auto votesRequired = sv_votesRequired.Get<int>();

		if (votesRequired > 0)
		{
			return votesRequired;
		}

		return LevelField<int>(level_numConnectedClients) / 2 + 1;
	}

	bool Vote::IsInvalidVoteString(const std::string& input)
	{
		static const char* separators[] = { "\n", "\r", ";" };

		return std::ranges::any_of(separators, [&](const std::string& separator)
		{
			return input.find(separator) != std::string::npos;
		});
	}

	void Vote::CheckVote()
	{
		ResetOnNewLevel();

		const int time = LevelField<int>(level_time);

		if (voteState.voteExecuteTime && voteState.voteExecuteTime < time)
		{
			voteState.voteExecuteTime = 0;
			Game::Cbuf_AddText(0, Utils::String::VA("%s\n", voteState.voteString));
		}

		if (!voteState.voteTime)
		{
			return;
		}

		const int connected = LevelField<int>(level_numConnectedClients);
		bool hasPassed;

		if (time - voteState.voteTime < 0)
		{
			const int majority = connected / 2 + 1;

			if (voteState.voteYes >= majority)
			{
				hasPassed = true;
			}
			else if (voteState.voteNo <= connected - majority)
			{
				return;
			}
			else
			{
				hasPassed = false;
			}
		}
		else
		{
			const auto abstained = static_cast<double>(connected - voteState.voteNo - voteState.voteYes) * g_voteAbstainWeight.Get<float>();
			hasPassed = voteState.voteYes > static_cast<int>(abstained + 0.4999999990686774) + voteState.voteNo;
		}

		if (hasPassed)
		{
			Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTEPASSED\"", 0x65));
			voteState.voteExecuteTime = time + 3000;
		}
		else
		{
			Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTEFAILED\"", 0x65));
		}

		voteState.voteTime = 0;
		SetConfigstring(CS_VOTE_TIME, "");
	}

	void Vote::DrawVote(int localClientNum)
	{
		int voteTime = 0;
		int serverId = 0;

		if (std::sscanf(Game::CL_GetConfigString(CS_VOTE_TIME), "%d %d", &voteTime, &serverId) != 2 || !voteTime
			|| serverId != LevelField<int>(cls_serverId))
		{
			return;
		}

		const int voteYes = std::atoi(Game::CL_GetConfigString(CS_VOTE_YES));
		const int voteNo = std::atoi(Game::CL_GetConfigString(CS_VOTE_NO));

		static std::string localizedFrom;
		static std::string voteString;
		const std::string rawVoteString = Game::CL_GetConfigString(CS_VOTE_STRING);

		if (rawVoteString != localizedFrom)
		{
			localizedFrom = rawVoteString;
			voteString = reinterpret_cast<const char*(*)(const char*, const char*, int)>(Utils::Hook::Rebase(SEH_LocalizeTextMessage))(rawVoteString.data(), "vote string", 0);
		}

		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);
		const float size = Renderer::Height() <= 768 ? 16.0f : 10.0f;
		const float scale = size / 48.0f;
		auto* const font = reinterpret_cast<Game::Font_s*(*)(const float*, int, float)>(Utils::Hook::Rebase(UI_GetFontHandle))(placement, 0, scale);

		const auto* const position = *reinterpret_cast<const Game::dvar_t* const*>(Utils::Hook::Rebase(cg_hudVotePosition));
		const float x = position->current.vector[0];
		float y = position->current.vector[1] + size;

		const auto getKeys = reinterpret_cast<int(*)(int, const char*, char*, int)>(Utils::Hook::Rebase(UI_GetKeyBindingLocalizedString));
		char yesKeys[256]{};
		char noKeys[256]{};

		if (!getKeys(localClientNum, "vote yes", yesKeys, sizeof(yesKeys)))
		{
			strncpy_s(yesKeys, "vote yes", _TRUNCATE);
		}

		if (!getKeys(localClientNum, "vote no", noKeys, sizeof(noKeys)))
		{
			strncpy_s(noKeys, "vote no", _TRUNCATE);
		}

		const int secondsLeft = std::max((voteTime - LevelField<int>(cg_time)) / 1000, 0);

		const char* const voteLine = Utils::String::VA("%s(%i):%s", Game::UI_SafeTranslateString("CGAME_VOTE"), secondsLeft, voteString.data());
		Game::UI_DrawText(placement, voteLine, 0x7FFFFFFF, font, x, y, 1, 1, scale, voteColor, 3);

		y += size;

		const char* const countLine = Utils::String::VA("%s(%s):%i, %s(%s):%i", Game::UI_SafeTranslateString("CGAME_YES"), yesKeys, voteYes,
			Game::UI_SafeTranslateString("CGAME_NO"), noKeys, voteNo);
		Game::UI_DrawText(placement, countLine, 0x7FFFFFFF, font, x, y, 1, 1, scale, voteColor, 3);
	}

	static int DvarInteger(std::uintptr_t dvar)
	{
		const auto* const value = *reinterpret_cast<const Game::dvar_t* const*>(Utils::Hook::Rebase(dvar));
		return value ? value->current.integer : -1;
	}

	static const char* SelectedPlayerName()
	{
		const int index = LevelField<int>(uiInfo_playerIndex);

		if (index < 0 || index >= LevelField<int>(sharedUiInfo_playerCount))
		{
			return nullptr;
		}

		return reinterpret_cast<const char*>(Utils::Hook::Rebase(sharedUiInfo_playerNames)) + index * playerNameSize;
	}

	static const char* SelectedGameType()
	{
		const int index = DvarInteger(ui_netGametype);

		if (index < 0 || index >= LevelField<int>(sharedUiInfo_numGameTypes))
		{
			return nullptr;
		}

		return reinterpret_cast<const char*>(Utils::Hook::Rebase(sharedUiInfo_gameTypes)) + index * gameTypeInfoSize;
	}

	static const char* SelectedMapName()
	{
		const int index = DvarInteger(ui_currentMap);

		if (index < 0 || index >= LevelField<int>(sharedUiInfo_mapCount))
		{
			return nullptr;
		}

		if (ArenaLength::newArenas)
		{
			return ArenaLength::newArenas[index].mapName;
		}

		return reinterpret_cast<const char*>(Utils::Hook::Rebase(sharedUiInfo_mapList)) + index * stockMapInfoSize + stockMapNameOffset;
	}

	void Vote::Con_DrawSay_Hook(int localClientNum, int x, int y)
	{
		DrawVote(localClientNum);
		reinterpret_cast<void(*)(int, int, int)>(Utils::Hook::Rebase(Con_DrawSay))(localClientNum, x, y);
	}

	bool Vote::HandleMapRestart([[maybe_unused]] const Game::gentity_s* ent, [[maybe_unused]] const Command::ServerParams* params)
	{
		sprintf_s(voteState.voteString, "fast_restart");
		sprintf_s(voteState.voteDisplayString, "GAME_VOTE_MAPRESTART");
		return true;
	}

	bool Vote::HandleMapRotate([[maybe_unused]] const Game::gentity_s* ent, const Command::ServerParams* params)
	{
		sprintf_s(voteState.voteString, "%s", params->Get(1));
		sprintf_s(voteState.voteDisplayString, "GAME_VOTE_NEXTMAP");
		return true;
	}

	bool Vote::HandleTypemap(const Game::gentity_s* ent, const Command::ServerParams* params)
	{
		char gameType[0x100]{};
		char mapName[0x100]{};

		strncpy_s(gameType, params->Get(2), _TRUNCATE);
		strncpy_s(mapName, params->Get(3), _TRUNCATE);

		if (!MapRotation::Contains("map", mapName))
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_NOTONROTATION\"", 0x65));
			return false;
		}

		if (!IsValidGameType(gameType))
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_INVALIDGAMETYPE\"", 0x65));
			return false;
		}

		const auto* const currentGameType = Game::Dvar_FindVar("g_gametype");

		if (currentGameType && !std::strcmp(gameType, currentGameType->current.string))
		{
			gameType[0] = '\0';
		}

		const auto* const currentMap = Game::Dvar_FindVar("mapname");

		if (currentMap && !std::strcmp(mapName, currentMap->current.string))
		{
			mapName[0] = '\0';
		}

		if (!gameType[0] && !mapName[0])
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_TYPEMAP_NOCHANGE\"", 0x65));
			return false;
		}

		if (mapName[0])
		{
			if (gameType[0])
			{
				sprintf_s(voteState.voteString, "g_gametype %s; map %s", gameType, mapName);
				sprintf_s(voteState.voteDisplayString, "GAME_VOTE_GAMETYPE\x14%s\x15 - \x14GAME_VOTE_MAP\x15%s", GetGameTypeNameForScript(gameType), mapName);
			}
			else
			{
				sprintf_s(voteState.voteString, "map %s", mapName);
				sprintf_s(voteState.voteDisplayString, "GAME_VOTE_MAP\x15%s", mapName);
			}
		}
		else
		{
			sprintf_s(voteState.voteString, "g_gametype %s; map_restart", gameType);
			sprintf_s(voteState.voteDisplayString, "GAME_VOTE_GAMETYPE\x14%s", GetGameTypeNameForScript(gameType));
		}

		return true;
	}

	bool Vote::HandleMap(const Game::gentity_s* ent, const Command::ServerParams* params)
	{
		if (!MapRotation::Contains("map", params->Get(2)))
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_NOTONROTATION\"", 0x65));
			return false;
		}

		sprintf_s(voteState.voteString, "%s %s", params->Get(1), params->Get(2));
		sprintf_s(voteState.voteDisplayString, "GAME_VOTE_MAP\x15%s", params->Get(2));
		return true;
	}

	bool Vote::HandleGametype(const Game::gentity_s* ent, const Command::ServerParams* params)
	{
		if (!IsValidGameType(params->Get(2)))
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_INVALIDGAMETYPE\"", 0x65));
			return false;
		}

		sprintf_s(voteState.voteString, "%s %s; map_restart", params->Get(1), params->Get(2));
		sprintf_s(voteState.voteDisplayString, "GAME_VOTE_GAMETYPE\x14%s", GetGameTypeNameForScript(params->Get(2)));
		return true;
	}

	bool Vote::HandleKick(const Game::gentity_s* ent, const Command::ServerParams* params)
	{
		char cleanName[0x40]{};

		const int maxClients = LevelField<int>(level_maxclients);
		int kickNumber = maxClients;

		for (int i = 0; i < maxClients; ++i)
		{
			const auto& client = Game::level->clients[i];

			if (client.sess.connected != Game::CON_CONNECTED)
			{
				continue;
			}

			strncpy_s(cleanName, client.sess.cs.name, _TRUNCATE);
			reinterpret_cast<char*(*)(char*)>(Utils::Hook::Rebase(I_CleanStr))(cleanName);

			if (Utils::String::Compare(cleanName, params->Get(2)))
			{
				kickNumber = i;
			}
		}

		if (kickNumber == maxClients)
		{
			Game::SV_GameSendServerCommand(ClientNumber(ent), Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_CLIENTNOTONSERVER\"", 0x65));
			return false;
		}

		sprintf_s(voteState.voteString, "%s \"%d\"", "tempBanClient", kickNumber);
		sprintf_s(voteState.voteDisplayString, "GAME_VOTE_KICK\x15(%i)%s", kickNumber, Game::level->clients[kickNumber].sess.cs.name);
		return true;
	}

	void Vote::Scr_VoteCalled(Game::gentity_s* self, const char* command, const char* param1, const char* param2)
	{
		Game::Scr_AddString(param2);
		Game::Scr_AddString(param1);
		Game::Scr_AddString(command);
		Game::Scr_Notify(self, LevelField<unsigned short>(scr_const_call_vote), 3);
	}

	void Vote::Scr_PlayerVote(Game::gentity_s* self, const char* option)
	{
		Game::Scr_AddString(option);
		Game::Scr_Notify(self, LevelField<unsigned short>(scr_const_vote), 1);
	}

	void Vote::Cmd_CallVote_f(Game::gentity_s* ent, const Command::ServerParams* params)
	{
		ResetOnNewLevel();

		const int clientNumber = ClientNumber(ent);
		const auto* const allowVote = Game::Dvar_FindVar("g_allowVote");

		if (!allowVote || !allowVote->current.enabled)
		{
			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTINGNOTENABLED\"", 0x65));
			return;
		}

		if (LevelField<int>(level_numConnectedClients) < VotesRequired())
		{
			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTINGNOTENOUGHPLAYERS\"", 0x65));
			return;
		}

		if (g_oldVoting.Get<bool>())
		{
			if (voteState.voteTime)
			{
				Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTEALREADYINPROGRESS\"", 0x65));
				return;
			}

			if (ent->client->sess.cs.team == Game::TEAM_SPECTATOR)
			{
				Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_NOSPECTATORCALLVOTE\"", 0x65));
				return;
			}
		}

		if (IsInvalidVoteString(params->Get(1)) || IsInvalidVoteString(params->Get(2)) || IsInvalidVoteString(params->Get(3)))
		{
			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_INVALIDVOTESTRING\"", 0x65));
			return;
		}

		if (!g_oldVoting.Get<bool>())
		{
			Scr_VoteCalled(ent, params->Get(1), params->Get(2), params->Get(3));
			return;
		}

		const auto itr = voteCommands.find(params->Get(1));

		if (itr == voteCommands.end())
		{
			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_INVALIDVOTESTRING\"", 0x65));
			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA(callVoteDescription, 0x65));
			return;
		}

		if (voteState.voteExecuteTime)
		{
			voteState.voteExecuteTime = 0;
			Game::Cbuf_AddText(0, Utils::String::VA("%s\n", voteState.voteString));
		}

		if (itr->second(ent, params))
		{
			DisplayVote(ent);
		}
	}

	void Vote::Cmd_Vote_f(Game::gentity_s* ent, const Command::ServerParams* params)
	{
		ResetOnNewLevel();

		const int clientNumber = ClientNumber(ent);

		if (g_oldVoting.Get<bool>())
		{
			if (!voteState.voteTime)
			{
				Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_NOVOTEINPROGRESS\"", 0x65));
				return;
			}

			if (hasVoted[clientNumber])
			{
				Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTEALREADYCAST\"", 0x65));
				return;
			}

			if (ent->client->sess.cs.team == Game::TEAM_SPECTATOR)
			{
				Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_NOSPECTATORVOTE\"", 0x65));
				return;
			}

			Game::SV_GameSendServerCommand(clientNumber, Game::SV_CMD_CAN_IGNORE, Utils::String::VA("%c \"GAME_VOTECAST\"", 0x65));
			hasVoted[clientNumber] = true;
		}

		const char option = params->Get(1)[0];

		if (option == 'y' || option == 'Y' || option == '1')
		{
			if (g_oldVoting.Get<bool>())
			{
				SetConfigstring(CS_VOTE_YES, Utils::String::VA("%i", ++voteState.voteYes));
			}
			else
			{
				Scr_PlayerVote(ent, "yes");
			}
		}
		else if (g_oldVoting.Get<bool>())
		{
			SetConfigstring(CS_VOTE_NO, Utils::String::VA("%i", ++voteState.voteNo));
		}
		else
		{
			Scr_PlayerVote(ent, "no");
		}
	}

	Vote::Vote()
	{
		if (Utils::Hook::MatchesBytes(G_RegisterDvars_AllowVoteFlags, allowVoteFlags, sizeof(allowVoteFlags)))
		{
			Utils::Hook::Set<std::uint32_t>(G_RegisterDvars_AllowVoteFlags + 2, 0x80 | Game::DVAR_CODINFO);
		}
		else
		{
			Logger::Error("vote: g_allowVote's registration does not read as expected, it stays server only\n");
		}

		Events::OnDvarInit([]
		{
			g_oldVoting = Dvar::Register("g_oldVoting", true, Game::DVAR_ARCHIVE | 0x80, "Use old voting method");
			g_voteAbstainWeight = Dvar::Register("g_voteAbstainWeight", 0.5f, 0.0f, 1.0f, Game::DVAR_ARCHIVE | 0x80, "How much an abstained vote counts as a 'no' vote");
			sv_votesRequired = Dvar::Register("sv_votesRequired", 0, 0, static_cast<int>(ClientSlots::CLIENT_LIMIT), Game::DVAR_NONE, "Set the amount of votes required for a vote to pass.\n0 = (players / 2) + 1");
		});

		ClientCommand::Add("callvote", Cmd_CallVote_f);
		ClientCommand::Add("vote", Cmd_Vote_f);

		Scheduler::Loop(CheckVote, Scheduler::Pipeline::SERVER);

		if (Dedicated::IsEnabled())
		{
			return;
		}

		Menus::Add("ui_mp/scriptmenus/callvote.menu");
		Menus::Add("ui_mp/scriptmenus/kickplayer.menu");

		UIScript::Add("voteKick", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (const char* const name = SelectedPlayerName())
			{
				Game::Cbuf_AddText(0, Utils::String::VA("callvote kick \"%s\"\n", name));
			}
		});

		UIScript::Add("voteTempBan", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (const char* const name = SelectedPlayerName())
			{
				Game::Cbuf_AddText(0, Utils::String::VA("callvote tempBanUser \"%s\"\n", name));
			}
		});

		UIScript::Add("voteTypeMap", []([[maybe_unused]] const UIScript::Token& token)
		{
			const char* const gameType = SelectedGameType();
			const char* const mapName = SelectedMapName();

			if (gameType && mapName)
			{
				Game::Cbuf_AddText(0, Utils::String::VA("callvote typemap %s %s\n", gameType, mapName));
			}
		});

		UIScript::Add("voteMap", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (const char* const mapName = SelectedMapName())
			{
				Game::Cbuf_AddText(0, Utils::String::VA("callvote map %s\n", mapName));
			}
		});

		UIScript::Add("voteGame", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (const char* const gameType = SelectedGameType())
			{
				Game::Cbuf_AddText(0, Utils::String::VA("callvote g_gametype %s\n", gameType));
			}
		});

		for (const auto* const name : { "vote yes", "vote no" })
		{
			if (!Command::AddBindable(name))
			{
				Logger::Error("vote: {} cannot be bound to a key\n", name);
			}
		}

		if (!Utils::Hook::BranchesTo(CG_Draw2D_Con_DrawSayCall, Con_DrawSay, false))
		{
			Logger::Error("vote: CG_Draw2D does not read as expected, no vote on the hud\n");
			return;
		}

		if (!drawSayHook.Initialize(CG_Draw2D_Con_DrawSayCall, reinterpret_cast<void*>(Con_DrawSay_Hook), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("vote: could not seat the hud hook, no vote on the hud\n");
			return;
		}

		drawSayHook.Quick();
	}
}
