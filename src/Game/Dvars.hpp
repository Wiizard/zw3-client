#pragma once

namespace Game
{
	typedef dvar_t*(*Dvar_RegisterBool_t)(const char* name, bool value, unsigned int flags, const char* description);
	extern Dvar_RegisterBool_t Dvar_RegisterBool;

	typedef dvar_t*(*Dvar_RegisterFloat_t)(const char* name, float value, float min, float max, unsigned int flags, const char* description);
	extern Dvar_RegisterFloat_t Dvar_RegisterFloat;

	typedef dvar_t*(*Dvar_RegisterVec2_t)(const char* name, float x, float y, float min, float max, unsigned int flags, const char* description);
	extern Dvar_RegisterVec2_t Dvar_RegisterVec2;

	typedef dvar_t*(*Dvar_RegisterVec3_t)(const char* name, float x, float y, float z, float min, float max, unsigned int flags, const char* description);
	extern Dvar_RegisterVec3_t Dvar_RegisterVec3;

	typedef dvar_t*(*Dvar_RegisterVec4_t)(const char* name, float x, float y, float z, float w, float min, float max, unsigned int flags, const char* description);
	extern Dvar_RegisterVec4_t Dvar_RegisterVec4;

	typedef dvar_t*(*Dvar_RegisterInt_t)(const char* name, int value, int minValue, int maxValue,
		unsigned int flags, const char* description);
	extern Dvar_RegisterInt_t Dvar_RegisterInt;

	typedef dvar_t*(*Dvar_RegisterEnum_t)(const char* name, const char** valueList, int defaultIndex, unsigned int flags, const char* description);
	extern Dvar_RegisterEnum_t Dvar_RegisterEnum;

	typedef dvar_t*(*Dvar_RegisterString_t)(const char* name, const char* value, unsigned int flags, const char* description);
	extern Dvar_RegisterString_t Dvar_RegisterString;

	typedef void(*Dvar_SetFromStringByName_t)(const char* name, const char* value);
	extern Dvar_SetFromStringByName_t Dvar_SetFromStringByName;

	typedef void(*Dvar_SetStringByName_t)(const char* dvarName, const char* value);
	extern Dvar_SetStringByName_t Dvar_SetStringByName;

	typedef void(*Dvar_SetString_t)(const dvar_t* dvar, const char* value);
	extern Dvar_SetString_t Dvar_SetString;

	typedef void(*Dvar_SetBool_t)(const dvar_t* dvar, bool value);
	extern Dvar_SetBool_t Dvar_SetBool;

	typedef void(*Dvar_SetFloat_t)(const dvar_t* dvar, float value);
	extern Dvar_SetFloat_t Dvar_SetFloat;

	typedef void(*Dvar_SetInt_t)(const dvar_t* dvar, int value);
	extern Dvar_SetInt_t Dvar_SetInt;

	typedef const char*(*Dvar_GetString_t)(const char* name);
	extern Dvar_GetString_t Dvar_GetString;

	typedef dvar_t*(*Dvar_FindVar_t)(const char* name);
	extern Dvar_FindVar_t Dvar_FindVar;

	typedef const char*(*Dvar_InfoString_Big_t)(unsigned int flag);
	extern Dvar_InfoString_Big_t Dvar_InfoString_Big;

	extern dvar_t** com_developer;
	extern dvar_t** com_sv_running;
	extern dvar_t** com_masterServerName;
	extern dvar_t** com_masterPort;

	extern dvar_t** r_displayMode;

	extern dvar_t** fs_basepath;
	extern dvar_t** fs_gameDirVar;

	extern dvar_t** sv_privatePassword;
	extern dvar_t** sv_privateClients;
	extern dvar_t** sv_maxclients;

	extern dvar_t** cl_voice;
	extern dvar_t** cl_ingame;

	extern dvar_t** g_deadChat;

	extern dvar_t** ui_joinGametype;
	extern dvar_t** ui_netSource;

	extern dvar_t** port;

	typedef void(*Dvar_SetVariant_t)(dvar_t* dvar, DvarValue value, int source);
	extern Dvar_SetVariant_t Dvar_SetVariant;

	typedef void(*Dvar_SetFromStringFromSource_t)(const dvar_t* dvar, const char* string, DvarSetSource source);
	extern Dvar_SetFromStringFromSource_t Dvar_SetFromStringFromSource;

	void BindDvars();
}
