#include "STDInclude.hpp"

namespace Game
{
	Scr_GetNumParam_t Scr_GetNumParam = nullptr;
	Scr_GetString_t Scr_GetString = nullptr;
	Scr_GetConstString_t Scr_GetConstString = nullptr;
	Scr_GetInt_t Scr_GetInt = nullptr;
	Scr_GetFloat_t Scr_GetFloat = nullptr;
	Scr_GetVector_t Scr_GetVector = nullptr;
	Scr_GetType_t Scr_GetType = nullptr;
	Scr_GetPointerType_t Scr_GetPointerType = nullptr;
	Scr_GetTypeName_t Scr_GetTypeName = nullptr;
	Scr_AddInt_t Scr_AddInt = nullptr;
	Scr_AddInt_t Scr_AddBool = nullptr;
	Scr_AddFloat_t Scr_AddFloat = nullptr;
	Scr_AddString_t Scr_AddString = nullptr;
	Scr_AddString_t Scr_AddIString = nullptr;
	Scr_AddConstString_t Scr_AddConstString = nullptr;
	Scr_AddObject_t Scr_AddObject = nullptr;
	Scr_AddArray_t Scr_AddArray = nullptr;
	Scr_AddArray_t Scr_MakeArray = nullptr;
	Scr_LoadScript_t Scr_LoadScript = nullptr;
	Scr_GetFunctionHandle_t Scr_GetFunctionHandle = nullptr;
	Scr_ExecThread_t Scr_ExecThread = nullptr;
	Scr_FreeThread_t Scr_FreeThread = nullptr;
	Scr_ClearOutParams_t Scr_ClearOutParams = nullptr;
	Scr_GetEntityId_t Scr_GetEntityId = nullptr;
	AddRefToObject_t AddRefToObject = nullptr;
	AllocThread_t AllocThread = nullptr;
	RemoveRefToObject_t RemoveRefToObject = nullptr;
	AllocObject_t AllocObject = nullptr;
	Scr_IsSystemActive_t Scr_IsSystemActive = nullptr;
	Scr_NotifyId_t Scr_NotifyId = nullptr;
	VM_Execute_t VM_Execute = nullptr;
	RemoveRefToValue_t RemoveRefToValue = nullptr;
	Scr_ErrorInternal_t Scr_ErrorInternal = nullptr;
	Scr_RegisterFunction_t Scr_RegisterFunction = nullptr;
	SL_ConvertToString_t SL_ConvertToString = nullptr;
	SL_AddRefToString_t SL_AddRefToString = nullptr;
	SL_AddRefToString_t SL_RemoveRefToString = nullptr;
	Scr_LoadGameType_t Scr_LoadGameType = nullptr;
	Scr_LoadGameType_t Scr_StartupGameType = nullptr;
	Scr_LoadGameType_t GScr_LoadGameTypeScript = nullptr;
	Scr_SetObjectField_t Scr_SetObjectField = nullptr;
	Scr_SetClientField_t Scr_SetClientField = nullptr;
	Scr_GetEntityField_t Scr_GetEntityField = nullptr;
	Scr_AddClassField_t Scr_AddClassField = nullptr;
	Scr_Notify_t Scr_Notify = nullptr;
	Scr_NotifyLevel_t Scr_NotifyLevel = nullptr;
	Scr_AddEntity_t Scr_AddEntity = nullptr;
	SL_GetString_t SL_GetString = nullptr;
	SL_FindLowercaseString_t SL_FindLowercaseString = nullptr;
	scr_const_t* scr_const = nullptr;
	GetEntity_t GetEntity = nullptr;
	BuiltinMethod PlayerCmd_switchToWeapon = nullptr;

	VariableValue** scrVmPub_top = nullptr;
	unsigned int* scrVmPub_inparamcount = nullptr;
	unsigned int* scrVmPub_outparamcount = nullptr;
	VariableValue** scrVmPub_maxstack = nullptr;

	void BindScript()
	{
		Scr_GetNumParam = BindFunction<Scr_GetNumParam_t>(0x14022A1B0);

		Scr_GetString = BindFunction<Scr_GetString_t>(0x14022A470);

		Scr_GetConstString = BindFunction<Scr_GetConstString_t>(0x140229D20);

		Scr_GetInt = BindFunction<Scr_GetInt_t>(0x14022A100);

		Scr_GetFloat = BindFunction<Scr_GetFloat_t>(0x140229FC0);

		Scr_GetVector = BindFunction<Scr_GetVector_t>(0x14022A5F0);

		Scr_GetType = BindFunction<Scr_GetType_t>(0x14022A490);

		Scr_GetPointerType = BindFunction<Scr_GetPointerType_t>(0x14022A3C0);

		Scr_GetTypeName = BindFunction<Scr_GetTypeName_t>(0x14022A500);

		Scr_AddInt = BindFunction<Scr_AddInt_t>(0x140229310);
		Scr_AddBool = Scr_AddInt;

		Scr_AddFloat = BindFunction<Scr_AddFloat_t>(0x140229420);

		Scr_AddString = BindFunction<Scr_AddString_t>(0x1402294D0);

		Scr_AddIString = BindFunction<Scr_AddString_t>(0x140229460);

		Scr_AddConstString = BindFunction<Scr_AddConstString_t>(0x140229340);

		Scr_AddObject = BindFunction<Scr_AddObject_t>(0x1402294A0);

		Scr_AddArray = BindFunction<Scr_AddArray_t>(0x140229270);

		Scr_MakeArray = BindFunction<Scr_AddArray_t>(0x14022A9F0);

		Scr_LoadScript = BindFunction<Scr_LoadScript_t>(0x14021DC10);

		Scr_GetFunctionHandle = BindFunction<Scr_GetFunctionHandle_t>(0x14021DAE0);

		Scr_ExecThread = BindFunction<Scr_ExecThread_t>(0x140229960);

		Scr_FreeThread = BindFunction<Scr_FreeThread_t>(0x1402299F0);

		Scr_ClearOutParams = BindFunction<Scr_ClearOutParams_t>(0x140229820);

		Scr_GetEntityId = BindFunction<Scr_GetEntityId_t>(0x1402283C0);

		AddRefToObject = BindFunction<AddRefToObject_t>(0x1402226E0);

		AllocThread = BindFunction<AllocThread_t>(0x1402229D0);

		RemoveRefToObject = BindFunction<RemoveRefToObject_t>(0x140224E70);

		AllocObject = BindFunction<AllocObject_t>(0x1402229A0);

		Scr_IsSystemActive = BindFunction<Scr_IsSystemActive_t>(0x14022A9E0);

		Scr_NotifyId = BindFunction<Scr_NotifyId_t>(0x14022AA20);

		VM_Execute = BindFunction<VM_Execute_t>(0x14022B7B0);

		RemoveRefToValue = BindFunction<RemoveRefToValue_t>(0x140224F90);

		Scr_ErrorInternal = BindFunction<Scr_ErrorInternal_t>(0x140229860);

		Scr_RegisterFunction = BindFunction<Scr_RegisterFunction_t>(0x14021CCE0);

		SL_ConvertToString = BindFunction<SL_ConvertToString_t>(0x140221660);

		SL_AddRefToString = BindFunction<SL_AddRefToString_t>(0x1402215E0);

		SL_RemoveRefToString = BindFunction<SL_AddRefToString_t>(0x140221BC0);

		Scr_LoadGameType = BindFunction<Scr_LoadGameType_t>(0x1401A7630);

		Scr_StartupGameType = BindFunction<Scr_LoadGameType_t>(0x1401A7D50);

		GScr_LoadGameTypeScript = BindFunction<Scr_LoadGameType_t>(0x1401A6710);

		Scr_SetObjectField = BindFunction<Scr_SetObjectField_t>(0x1401A9F30);

		Scr_SetClientField = BindFunction<Scr_SetClientField_t>(0x140163E80);

		Scr_GetEntityField = BindFunction<Scr_GetEntityField_t>(0x1401A9A90);

		Scr_AddClassField = BindFunction<Scr_AddClassField_t>(0x140225340);

		Scr_Notify = BindFunction<Scr_Notify_t>(0x1401A9DF0);

		Scr_NotifyLevel = BindFunction<Scr_NotifyLevel_t>(0x14022AB10);

		Scr_AddEntity = BindFunction<Scr_AddEntity_t>(0x1401A9700);

		SL_GetString = BindFunction<SL_GetString_t>(0x140221970);

		SL_FindLowercaseString = BindFunction<SL_FindLowercaseString_t>(0x1402216D0);
		scr_const = reinterpret_cast<scr_const_t*>(Utils::Hook::Rebase(0x1417CC2E0));

		GetEntity = BindFunction<GetEntity_t>(0x140181DF0);

		PlayerCmd_switchToWeapon = BindFunction<BuiltinMethod>(0x140164700);

		scrVmPub_top = reinterpret_cast<VariableValue**>(Utils::Hook::Rebase(0x14227DA30));
		scrVmPub_inparamcount = reinterpret_cast<unsigned int*>(Utils::Hook::Rebase(0x14227DA38));
		scrVmPub_outparamcount = reinterpret_cast<unsigned int*>(Utils::Hook::Rebase(0x14227DA3C));

		scrVmPub_maxstack = reinterpret_cast<VariableValue**>(Utils::Hook::Rebase(0x14227DA18));
	}
}
