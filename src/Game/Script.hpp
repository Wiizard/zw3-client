#pragma once

namespace Game
{
	typedef unsigned int(*Scr_GetNumParam_t)();
	extern Scr_GetNumParam_t Scr_GetNumParam;

	typedef const char*(*Scr_GetString_t)(unsigned int index);
	extern Scr_GetString_t Scr_GetString;

	typedef unsigned int(*Scr_GetConstString_t)(unsigned int index);
	extern Scr_GetConstString_t Scr_GetConstString;

	typedef int(*Scr_GetInt_t)(unsigned int index);
	extern Scr_GetInt_t Scr_GetInt;

	typedef float(*Scr_GetFloat_t)(unsigned int index);
	extern Scr_GetFloat_t Scr_GetFloat;

	typedef void(*Scr_GetVector_t)(unsigned int index, float* vectorValue);
	extern Scr_GetVector_t Scr_GetVector;

	typedef int(*Scr_GetType_t)(unsigned int index);
	extern Scr_GetType_t Scr_GetType;

	typedef int(*Scr_GetPointerType_t)(unsigned int index);
	extern Scr_GetPointerType_t Scr_GetPointerType;

	typedef const char*(*Scr_GetTypeName_t)(unsigned int index);
	extern Scr_GetTypeName_t Scr_GetTypeName;

	typedef void(*Scr_AddInt_t)(int value);
	extern Scr_AddInt_t Scr_AddInt;
	extern Scr_AddInt_t Scr_AddBool;

	typedef void(*Scr_AddFloat_t)(float value);
	extern Scr_AddFloat_t Scr_AddFloat;

	typedef void(*Scr_AddString_t)(const char* value);
	extern Scr_AddString_t Scr_AddString;
	extern Scr_AddString_t Scr_AddIString;

	typedef void(*Scr_AddConstString_t)(unsigned int value);
	extern Scr_AddConstString_t Scr_AddConstString;

	typedef void(*Scr_AddObject_t)(unsigned int id);
	extern Scr_AddObject_t Scr_AddObject;

	typedef void(*Scr_AddArray_t)();
	extern Scr_AddArray_t Scr_AddArray;
	extern Scr_AddArray_t Scr_MakeArray;

	typedef unsigned int(*Scr_LoadScript_t)(const char* filename);
	extern Scr_LoadScript_t Scr_LoadScript;

	typedef int(*Scr_GetFunctionHandle_t)(const char* filename, const char* name);
	extern Scr_GetFunctionHandle_t Scr_GetFunctionHandle;

	typedef unsigned short(*Scr_ExecThread_t)(int handle, unsigned int paramcount);
	extern Scr_ExecThread_t Scr_ExecThread;

	typedef void(*Scr_FreeThread_t)(unsigned short handle);
	extern Scr_FreeThread_t Scr_FreeThread;

	typedef void(*Scr_ClearOutParams_t)();
	extern Scr_ClearOutParams_t Scr_ClearOutParams;

	typedef unsigned int(*Scr_GetEntityId_t)(int entnum, unsigned int classnum);
	extern Scr_GetEntityId_t Scr_GetEntityId;

	typedef void(*AddRefToObject_t)(unsigned int id);
	extern AddRefToObject_t AddRefToObject;

	typedef unsigned int(*AllocThread_t)(unsigned int self);
	extern AllocThread_t AllocThread;

	typedef void(*RemoveRefToObject_t)(unsigned int id);
	extern RemoveRefToObject_t RemoveRefToObject;

	typedef unsigned int(*AllocObject_t)();
	extern AllocObject_t AllocObject;

	typedef bool(*Scr_IsSystemActive_t)();
	extern Scr_IsSystemActive_t Scr_IsSystemActive;

	typedef void(*Scr_NotifyId_t)(unsigned int id, unsigned int stringValue, unsigned int paramcount);
	extern Scr_NotifyId_t Scr_NotifyId;

	typedef unsigned int(*VM_Execute_t)(unsigned int localId, const char* pos, unsigned int paramcount);
	extern VM_Execute_t VM_Execute;

	typedef void(*RemoveRefToValue_t)(int type, VariableUnion u);
	extern RemoveRefToValue_t RemoveRefToValue;

	typedef void(*Scr_ErrorInternal_t)();
	extern Scr_ErrorInternal_t Scr_ErrorInternal;

	typedef void(*Scr_RegisterFunction_t)(void* function, const char* name);
	extern Scr_RegisterFunction_t Scr_RegisterFunction;

	typedef const char*(*SL_ConvertToString_t)(unsigned int stringValue);
	extern SL_ConvertToString_t SL_ConvertToString;

	typedef void(*SL_AddRefToString_t)(unsigned int stringValue);
	extern SL_AddRefToString_t SL_AddRefToString;
	extern SL_AddRefToString_t SL_RemoveRefToString;

	typedef void(*Scr_LoadGameType_t)();
	extern Scr_LoadGameType_t Scr_LoadGameType;
	extern Scr_LoadGameType_t Scr_StartupGameType;
	extern Scr_LoadGameType_t GScr_LoadGameTypeScript;

	typedef int(*Scr_SetObjectField_t)(unsigned int classnum, int entnum, int offset);
	extern Scr_SetObjectField_t Scr_SetObjectField;

	typedef void(*Scr_SetClientField_t)(gclient_s* client, int offset);
	extern Scr_SetClientField_t Scr_SetClientField;

	typedef void(*Scr_GetEntityField_t)(int entnum, int offset);
	extern Scr_GetEntityField_t Scr_GetEntityField;

	typedef void(*Scr_AddClassField_t)(unsigned int classnum, const char* name, unsigned int offset);
	extern Scr_AddClassField_t Scr_AddClassField;

	typedef void(*Scr_Notify_t)(gentity_s* ent, unsigned short stringValue, unsigned int paramcount);
	extern Scr_Notify_t Scr_Notify;

	typedef void(*Scr_NotifyLevel_t)(unsigned int stringValue, unsigned int paramcount);
	extern Scr_NotifyLevel_t Scr_NotifyLevel;

	typedef void(*Scr_AddEntity_t)(const gentity_s* ent);
	extern Scr_AddEntity_t Scr_AddEntity;

	typedef unsigned int(*SL_GetString_t)(const char* str, unsigned int user);
	extern SL_GetString_t SL_GetString;

	typedef unsigned int(*SL_FindLowercaseString_t)(const char* str);
	extern SL_FindLowercaseString_t SL_FindLowercaseString;

	extern scr_const_t* scr_const;

	typedef gentity_s*(*GetEntity_t)(scr_entref_t entref);
	extern GetEntity_t GetEntity;

	extern BuiltinMethod PlayerCmd_switchToWeapon;

	extern VariableValue** scrVmPub_top;
	extern unsigned int* scrVmPub_inparamcount;
	extern unsigned int* scrVmPub_outparamcount;
	extern VariableValue** scrVmPub_maxstack;

	constexpr auto SCRIPTDATA_DIR = "scriptdata";

	void BindScript();
}
