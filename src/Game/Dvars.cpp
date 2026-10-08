#include "STDInclude.hpp"

namespace Game
{
	Dvar_RegisterBool_t Dvar_RegisterBool = nullptr;
	Dvar_RegisterFloat_t Dvar_RegisterFloat = nullptr;
	Dvar_RegisterVec2_t Dvar_RegisterVec2 = nullptr;
	Dvar_RegisterVec3_t Dvar_RegisterVec3 = nullptr;
	Dvar_RegisterVec4_t Dvar_RegisterVec4 = nullptr;
	Dvar_RegisterInt_t Dvar_RegisterInt = nullptr;
	Dvar_RegisterEnum_t Dvar_RegisterEnum = nullptr;
	Dvar_RegisterString_t Dvar_RegisterString = nullptr;

	Dvar_GetString_t Dvar_GetString = nullptr;
	Dvar_FindVar_t Dvar_FindVar = nullptr;
	Dvar_InfoString_Big_t Dvar_InfoString_Big = nullptr;

	Dvar_SetFromStringByName_t Dvar_SetFromStringByName = nullptr;
	Dvar_SetStringByName_t Dvar_SetStringByName = nullptr;
	Dvar_SetString_t Dvar_SetString = nullptr;
	Dvar_SetBool_t Dvar_SetBool = nullptr;
	Dvar_SetFloat_t Dvar_SetFloat = nullptr;
	Dvar_SetInt_t Dvar_SetInt = nullptr;
	Dvar_SetVariant_t Dvar_SetVariant = nullptr;
	Dvar_SetFromStringFromSource_t Dvar_SetFromStringFromSource = nullptr;

	dvar_t** com_developer = nullptr;
	dvar_t** com_sv_running = nullptr;
	dvar_t** com_masterServerName = nullptr;
	dvar_t** com_masterPort = nullptr;

	dvar_t** r_displayMode = nullptr;

	dvar_t** fs_basepath = nullptr;
	dvar_t** fs_gameDirVar = nullptr;

	dvar_t** sv_privatePassword = nullptr;
	dvar_t** sv_privateClients = nullptr;
	dvar_t** sv_maxclients = nullptr;

	dvar_t** cl_voice = nullptr;
	dvar_t** cl_ingame = nullptr;

	dvar_t** g_deadChat = nullptr;

	dvar_t** ui_joinGametype = nullptr;
	dvar_t** ui_netSource = nullptr;

	dvar_t** port = nullptr;

	void BindDvars()
	{
		Dvar_RegisterBool = BindFunction<Dvar_RegisterBool_t>(0x140285C70);
		Dvar_RegisterFloat = BindFunction<Dvar_RegisterFloat_t>(0x140286050);
		Dvar_RegisterVec2 = BindFunction<Dvar_RegisterVec2_t>(0x1402865F0);
		Dvar_RegisterVec3 = BindFunction<Dvar_RegisterVec3_t>(0x140286710);
		Dvar_RegisterVec4 = BindFunction<Dvar_RegisterVec4_t>(0x140286960);
		Dvar_RegisterInt = BindFunction<Dvar_RegisterInt_t>(0x140286180);
		Dvar_RegisterEnum = BindFunction<Dvar_RegisterEnum_t>(0x140285F50);
		Dvar_RegisterString = BindFunction<Dvar_RegisterString_t>(0x140286520);

		Dvar_GetString = BindFunction<Dvar_GetString_t>(0x140285260);
		Dvar_FindVar = BindFunction<Dvar_FindVar_t>(0x140285000);
		Dvar_InfoString_Big = BindFunction<Dvar_InfoString_Big_t>(0x1401FC570);

		Dvar_SetFromStringByName = BindFunction<Dvar_SetFromStringByName_t>(0x140287500);
		Dvar_SetStringByName = BindFunction<Dvar_SetStringByName_t>(0x140287A70);
		Dvar_SetString = BindFunction<Dvar_SetString_t>(0x140287A10);
		Dvar_SetBool = BindFunction<Dvar_SetBool_t>(0x140286F40);
		Dvar_SetFloat = BindFunction<Dvar_SetFloat_t>(0x140287330);
		Dvar_SetInt = BindFunction<Dvar_SetInt_t>(0x140287670);
		Dvar_SetVariant = BindFunction<Dvar_SetVariant_t>(0x140287B10);
		Dvar_SetFromStringFromSource = BindFunction<Dvar_SetFromStringFromSource_t>(0x1402875D0);

		com_developer = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141BD9A50));
		com_sv_running = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141BD9A90));
		com_masterServerName = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141BD9A60));
		com_masterPort = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141BD9A70));

		r_displayMode = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x148CC6AE0));

		fs_basepath = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x146644B68));
		fs_gameDirVar = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x146644B88));

		sv_privatePassword = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x14650D840));
		sv_privateClients = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x14650D918));
		sv_maxclients = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x14650D858));

		cl_voice = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x140D050F0));
		cl_ingame = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x140D050E0));

		g_deadChat = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141869A40));

		ui_joinGametype = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x1465D0A00));
		ui_netSource = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x1465D09A8));

		port = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x14678C3C0));
	}
}
