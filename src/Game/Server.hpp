#pragma once

namespace Game
{
	typedef gentity_s*(*SV_AddTestClient_t)();
	extern SV_AddTestClient_t SV_AddTestClient;

	typedef int(*SV_IsTestClient_t)(int clientNum);
	extern SV_IsTestClient_t SV_IsTestClient;

	typedef void(*SV_GameSendServerCommand_t)(int clientNum, svscmd_type type, const char* text);
	extern SV_GameSendServerCommand_t SV_GameSendServerCommand;

	typedef void(*SV_Cmd_TokenizeString_t)(const char* text);
	extern SV_Cmd_TokenizeString_t SV_Cmd_TokenizeString;

	typedef void(*SV_Cmd_EndTokenizedString_t)();
	extern SV_Cmd_EndTokenizedString_t SV_Cmd_EndTokenizedString;

	typedef void(*SV_SetConfigstring_t)(int index, const char* value);
	extern SV_SetConfigstring_t SV_SetConfigstring;

	typedef unsigned int(*SV_GetConfigstringConst_t)(int index);
	extern SV_GetConfigstringConst_t SV_GetConfigstringConst;

	typedef void(*SV_DirectConnect_t)(const netadr_t* from);
	extern SV_DirectConnect_t SV_DirectConnect;

	typedef void(*SV_ClientThink_t)(client_s* cl, usercmd_s* cmd);
	extern SV_ClientThink_t SV_ClientThink;

	typedef void(*SV_DropClient_t)(client_s* drop, const char* reason, bool tellThem);
	extern SV_DropClient_t SV_DropClient;

	typedef client_s*(*SV_FindClientByAddress_t)(const netadr_t* from, int qport, int remoteClientIndex);
	extern SV_FindClientByAddress_t SV_FindClientByAddress;

	typedef void(*SV_GameDropClient_t)(int clientNum, const char* reason);
	extern SV_GameDropClient_t SV_GameDropClient;

	extern int* svs_time;
	extern int* svs_clientCount;
	extern client_s* svs_clients;

	extern volatile long* sv_thread_owns_game;

	playerState_s* SV_GetPlayerstateForClientNum(int clientNum);

	int SV_GetServerThreadOwnsGame();
	void SV_DropAllBots();
	int SV_GetClientStat(int clientNum, int index);

	constexpr char setStatCommand = 'Z';

	void SV_SetClientStat(int clientNum, int index, int value);

	void BindServer();

	void AddOperatorCommands();

	bool IsTempBanned(std::uint64_t xuid);

	bool IsMapOnDisk(const char* name);
}
