#include "STDInclude.hpp"

#include "Iw4.hpp"
#include "Components/Modules/Logger.hpp"

namespace Components::BotAI
{
	void BindAddresses()
	{
		Dvar_FindVar = reinterpret_cast<Dvar_FindVar_t>(Utils::Hook::Rebase(0x140285000));
		Dvar_SetFromStringByName = reinterpret_cast<Dvar_SetFromStringByName_t>(Utils::Hook::Rebase(0x140287500));
		Dvar_RegisterInt = reinterpret_cast<Dvar_RegisterInt_t>(Utils::Hook::Rebase(0x140286180));
		Cbuf_AddText = reinterpret_cast<Cbuf_AddText_t>(Utils::Hook::Rebase(0x1401E6DC0));
		sv_privateClients = Utils::Hook::Rebase(0x14650D918);
		lobbySession = Utils::Hook::Rebase(0x1465117B0);
		svState = Utils::Hook::Rebase(0x1464FDF00);
		cmd_args_ptr = Utils::Hook::Rebase(0x141BBC640);
		svs_numClients = Utils::Hook::Rebase(0x14340EC88);
		svs_time = Utils::Hook::Rebase(0x14340EC80);
		SV_AddTestClient = reinterpret_cast<SV_AddTestClient_t>(Utils::Hook::Rebase(0x140236D00));
		SV_ClientThink = reinterpret_cast<SV_ClientThink_t>(Utils::Hook::Rebase(0x140237260));
		SV_GetPlayerstateForClientNum = reinterpret_cast<SV_GetPlayerstate_t>(Utils::Hook::Rebase(0x140233470));
		SV_GetPersistentDataBuffer = reinterpret_cast<SV_GetPersistentData_t>(Utils::Hook::Rebase(0x140238760));
		SV_GetPersistentDataFlags = reinterpret_cast<SV_GetPersistentData_t>(Utils::Hook::Rebase(0x140238780));
		SV_Trace = reinterpret_cast<SV_Trace_t>(Utils::Hook::Rebase(0x1402351F0));
		G_LocationalTracePassed = reinterpret_cast<G_LocationalTracePassed_t>(Utils::Hook::Rebase(0x14019D790));
		SV_EntityContactBounds = reinterpret_cast<SV_EntityContactBounds_t>(Utils::Hook::Rebase(0x1402331C0));
		Scr_AddString = reinterpret_cast<Scr_AddString_t>(Utils::Hook::Rebase(0x1402294D0));
		Scr_AddBool = reinterpret_cast<Scr_AddBool_t>(Utils::Hook::Rebase(0x140229310));
		Scr_Notify = reinterpret_cast<Scr_Notify_t>(Utils::Hook::Rebase(0x1401A9DF0));
		BG_GetWeaponDef = reinterpret_cast<BG_GetWeaponDef_t>(Utils::Hook::Rebase(0x14009C8A0));
		perk_bulletPenetrationMultiplier = Utils::Hook::Rebase(0x140440F28);
		bullet_penetration_enabled = Utils::Hook::Rebase(0x140440EF8);
		penetrationDepthTable = Utils::Hook::Rebase(0x140446AB0);
		cm = Utils::Hook::Rebase(0x141BD3180);
		g_worldDpvs = Utils::Hook::Rebase(0x148CCC9B0);
		g_worldDraw = Utils::Hook::Rebase(0x148CCA1A8);
		CM_EntityString = reinterpret_cast<CM_EntityString_t>(Utils::Hook::Rebase(0x1401E8780));
		G_Glass_GetPieceOrigin = reinterpret_cast<G_Glass_GetPieceOrigin_t>(Utils::Hook::Rebase(0x140168C80));
		StructuredDataDefGetAsset = reinterpret_cast<StructuredDataDefGetAsset_t>(Utils::Hook::Rebase(0x140281080));
		StructuredDataSetString = reinterpret_cast<StructuredDataSetString_t>(Utils::Hook::Rebase(0x1402827A0));
		StructuredDataSetInt = reinterpret_cast<StructuredDataSetNumber_t>(Utils::Hook::Rebase(0x1402822F0));
		StructuredDataSetShort = reinterpret_cast<StructuredDataSetNumber_t>(Utils::Hook::Rebase(0x140282570));
		StructuredDataSetBool = reinterpret_cast<StructuredDataSetNumber_t>(Utils::Hook::Rebase(0x140281E30));
		StructuredDataInitLookup = reinterpret_cast<StructuredDataInitLookup_t>(Utils::Hook::Rebase(0x140281790));
		StructuredDataLookupString = reinterpret_cast<StructuredDataLookupString_t>(Utils::Hook::Rebase(0x140281990));
		StructuredDataLookupInt = reinterpret_cast<StructuredDataLookupInt_t>(Utils::Hook::Rebase(0x1402817B0));
		SL_ConvertToString = reinterpret_cast<SL_ConvertToString_t>(Utils::Hook::Rebase(0x140221660));
		GScr_IsItemUnlocked = reinterpret_cast<GScr_IsItemUnlocked_t>(Utils::Hook::Rebase(0x1401A57C0));
		scr_const_menuresponse = Utils::Hook::Rebase(0x1417CC4C0);
		g_entities = Utils::Hook::Rebase(0x141869B40);
		g_glassData = Utils::Hook::Rebase(0x1417810F8);
		sv_mapname = Utils::Hook::Rebase(0x14650D878);
		Session_GetXuidEvenIfInactive = reinterpret_cast<Session_GetXuid_t>(Utils::Hook::Rebase(0x140243AE0));
		Sys_Milliseconds = reinterpret_cast<Sys_Milliseconds_t>(Utils::Hook::Rebase(0x1402A8620));
		Cmd_AddCommandInternal = reinterpret_cast<Cmd_AddCommandInternal_t>(Utils::Hook::Rebase(0x1401E72B0));
		SV_DropClientFn = reinterpret_cast<SV_DropClient_t>(Utils::Hook::Rebase(0x140237EC0));
		BG_GetWeaponCompleteDef = reinterpret_cast<BG_GetWeaponCompleteDef_t>(Utils::Hook::Rebase(0x14009C890));
		svs_clients = reinterpret_cast<std::uintptr_t>(Game::svs_clients);
	}

	void Print(const char* format, ...)
	{
		char text[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);
		Logger::Print("{}", text);
	}
}
