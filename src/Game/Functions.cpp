#include "STDInclude.hpp"

namespace Game
{
	Menus_CloseAll_t Menus_CloseAll = nullptr;
	Material_RegisterHandle_t Material_RegisterHandle = nullptr;
	R_Cinematic_StartPlayback_t R_Cinematic_StartPlayback = nullptr;
	R_Cinematic_StopPlayback_t R_Cinematic_StopPlayback = nullptr;
	R_RegisterFont_t R_RegisterFont = nullptr;
	R_AddCmdDrawText_t R_AddCmdDrawText = nullptr;
	R_AddCmdDrawTextWithCursor_t R_AddCmdDrawTextWithCursor = nullptr;
	R_TextWidth_t R_TextWidth = nullptr;
	R_TextHeight_t R_TextHeight = nullptr;
	CL_KeyEvent_t CL_KeyEvent = nullptr;
	CL_CharEvent_t CL_CharEvent = nullptr;
	Cbuf_AddText_t Cbuf_AddText = nullptr;
	Cmd_ExecuteSingleCommand_t Cmd_ExecuteSingleCommand = nullptr;
	ScrPlace_GetActivePlacement_t ScrPlace_GetActivePlacement = nullptr;
	ScrPlace_EndFrame_t ScrPlace_EndFrame = nullptr;
	CL_ConsolePrint_AddLine_t CL_ConsolePrint_AddLine = nullptr;
	Cmd_AddCommandInternal_t Cmd_AddCommandInternal = nullptr;
	Cmd_AddServerCommandInternal_t Cmd_AddServerCommandInternal = nullptr;
	Cbuf_Execute_t Cbuf_Execute = nullptr;
	Cbuf_InsertText_t Cbuf_InsertText = nullptr;
	Dvar_GetBool_t Dvar_GetBool = nullptr;
	Dvar_GetInt_t Dvar_GetInt = nullptr;
	Dvar_GetFloat_t Dvar_GetFloat = nullptr;
	Dvar_ValueToString_t Dvar_ValueToString = nullptr;
	CL_ConsolePrint_t CL_ConsolePrint = nullptr;
	CmdArgs* cmd_args = nullptr;
	CmdArgs* sv_cmd_args = nullptr;
	cmd_function_s** cmd_functions = nullptr;
	NET_OutOfBandPrint_t NET_OutOfBandPrint = nullptr;
	NET_OutOfBandData_t NET_OutOfBandData = nullptr;
	NET_OutOfBandVoiceData_t NET_OutOfBandVoiceData = nullptr;
	MSG_Init_t MSG_Init = nullptr;
	MSG_WriteString_t MSG_WriteString = nullptr;
	MSG_WriteByte_t MSG_WriteByte = nullptr;
	MSG_WriteShort_t MSG_WriteShort = nullptr;
	MSG_WriteData_t MSG_WriteData = nullptr;
	MSG_WriteLong_t MSG_WriteLong = nullptr;
	MSG_ReadByte_t MSG_ReadByte = nullptr;
	MSG_ReadShort_t MSG_ReadShort = nullptr;
	MSG_ReadData_t MSG_ReadData = nullptr;
	MSG_ReadStringLine_t MSG_ReadStringLine = nullptr;
	Voice_IsClientTalking_t Voice_IsClientTalking = nullptr;
	Voice_IncomingVoiceData_t Voice_IncomingVoiceData = nullptr;
	NET_SendPacket_t NET_SendPacket = nullptr;
	netadr_t* clc_serverAddress = nullptr;
	NET_CompareAdr_t NET_CompareAdr = nullptr;
	NET_CompareBaseAdr_t NET_CompareBaseAdr = nullptr;
	NET_IsLocalAddress_t NET_IsLocalAddress = nullptr;
	NET_StringToAdr_t NET_StringToAdr = nullptr;
	NetadrToSockadr_t NetadrToSockadr = nullptr;
	SockadrToNetadr_t SockadrToNetadr = nullptr;
	int* numIP = nullptr;
	netIP_t* localIP = nullptr;
	ClientUserinfoChanged_t ClientUserinfoChanged = nullptr;
	G_SetConstString_t G_SetConstString = nullptr;
	Touch_Item_t Touch_Item = nullptr;
	Add_Ammo_t Add_Ammo = nullptr;
	player_die_t player_die = nullptr;
	TeleportPlayer_t TeleportPlayer = nullptr;
	level_locals_t* level = nullptr;
	bgs_t* level_bgs = nullptr;
	unsigned long* g_dwTlsIndex = nullptr;
	CG_SetupWeaponDef_t CG_SetupWeaponDef = nullptr;
	BG_GetClipSize_t BG_GetClipSize = nullptr;
	BG_GetSharedAmmoCapSize_t BG_GetSharedAmmoCapSize = nullptr;
	SV_LinkEntity_t SV_LinkEntity = nullptr;
	SV_LinkEntity_t SV_UnlinkEntity = nullptr;
	SV_SetBrushModel_t SV_SetBrushModel = nullptr;
	Win_GetLanguage_t Win_GetLanguage = nullptr;
	UI_UpdateArenas_t UI_UpdateArenas = nullptr;
	int* arenaCount = nullptr;
	FS_LoadZipFile_t FS_LoadZipFile = nullptr;
	unzClose_t unzClose = nullptr;
	Info_ValueForKey_t Info_ValueForKey = nullptr;
	const char** g_assetNames = nullptr;
	DB_GetXAssetTypeSize_t DB_GetXAssetTypeSize = nullptr;
	Menus_FindByName_t Menus_FindByName = nullptr;
	Menus_OpenByName_t Menus_OpenByName = nullptr;
	Menus_CloseRequest_t Menus_CloseRequest = nullptr;
	Menus_MenuIsInStack_t Menus_MenuIsInStack = nullptr;
	UI_AddMenuList_t UI_AddMenuList = nullptr;
	Key_StringToKeynum_t Key_StringToKeynum = nullptr;
	Menu_IsVisible_t Menu_IsVisible = nullptr;
	UI_GetGameTypeDisplayName_t UI_GetGameTypeDisplayName = nullptr;
	UI_GetMapDisplayName_t UI_GetMapDisplayName = nullptr;
	UI_SafeTranslateString_t UI_SafeTranslateString = nullptr;
	Party_AreWeHost_t Party_AreWeHost = nullptr;
	PartyClient_CountMembersEvenIfInactive_t PartyClient_CountMembersEvenIfInactive = nullptr;
	PartyData* g_lobbyData = nullptr;
	PartyHost_GetMemberName_t PartyHost_GetMemberName = nullptr;
	G_GetClientScore_t G_GetClientScore = nullptr;
	UI_DrawText_t UI_DrawText = nullptr;
	UI_TextWidth_t UI_TextWidth = nullptr;
	UI_GetFontHandle_t UI_GetFontHandle = nullptr;
	PartyData* g_partyData = nullptr;
	String_Parse_t String_Parse = nullptr;
	UI_OwnerDrawHandleKey_t UI_OwnerDrawHandleKey = nullptr;
	UI_FeederCount_t UI_FeederCount = nullptr;
	UI_FeederItemText_t UI_FeederItemText = nullptr;
	UI_FeederSelection_t UI_FeederSelection = nullptr;
	UI_FeederDoubleClick_t UI_FeederDoubleClick = nullptr;
	UI_OverrideCursorPos_t UI_OverrideCursorPos = nullptr;
	UI_FeederItemColor_t UI_FeederItemColor = nullptr;
	GetLobbyMemberCount_t GetLobbyMemberCount = nullptr;
	GetLobbyMemberXuid_t GetLobbyMemberXuid = nullptr;
	UpdatePartyDvars_t UpdatePartyDvars = nullptr;
	std::uint64_t* selectedPlayerXuid = nullptr;
	Live_GetXp_t Live_GetXp = nullptr;
	Live_GetPrestige_t Live_GetPrestige = nullptr;
	Image_Setup_t Image_Setup = nullptr;
	Image_Release_t Image_Release = nullptr;
	Image_LoadFromFileWithReader_t Image_LoadFromFileWithReader = nullptr;
	Load_Texture_t Load_Texture = nullptr;
	CL_GetRankData_t CL_GetRankData = nullptr;
	PlayerCards_GetPartyMemberData_t PlayerCards_GetPartyMemberData = nullptr;
	PartyUI_GetSelectedPlayerListChangedTime_t PartyUI_GetSelectedPlayerListChangedTime = nullptr;
	Party_SetUIPlayerCount_t Party_SetUIPlayerCount = nullptr;
	UI_ReplaceConversionInts_t UI_ReplaceConversionInts = nullptr;
	Item_GetListBoxDef_t Item_GetListBoxDef = nullptr;
	GetBlinkColor_t GetBlinkColor = nullptr;
	SND_PlayLocalSoundAliasByName_t SND_PlayLocalSoundAliasByName = nullptr;
	SEH_GetCurrentLanguage_t SEH_GetCurrentLanguage = nullptr;
	Vec3UnpackUnitVec_t Vec3UnpackUnitVec = nullptr;
	Vec3Normalize_t Vec3Normalize = nullptr;
	Vec2Normalize_t Vec2Normalize = nullptr;
	AngleNormalize360_t AngleNormalize360 = nullptr;
	vectoyaw_t vectoyaw = nullptr;
	AimAssist_ApplyDeltas_t AimAssist_ApplyDeltas = nullptr;
	AimAssistGlobals* aaGlobArray = nullptr;
	GraphFloat* aaInputGraph = nullptr;
	Key_IsCatcherActive_t Key_IsCatcherActive = nullptr;
	Key_RemoveCatcher_t Key_RemoveCatcher = nullptr;
	UI_KeyEvent_t UI_KeyEvent = nullptr;
	UI_GetActiveMenu_t UI_GetActiveMenu = nullptr;
	Scoreboard_HandleInput_t Scoreboard_HandleInput = nullptr;
	CG_ScoresDown_t CG_ScoresDown = nullptr;
	CG_ScoresUp_t CG_ScoresUp = nullptr;
	Key_GetBindingForCmd_t Key_GetBindingForCmd = nullptr;
	Key_SetBinding_t Key_SetBinding = nullptr;
	Key_SetCatcher_t Key_SetCatcher = nullptr;
	Key_WriteBindings_t Key_WriteBindings = nullptr;
	PlayerKeyState* playerKeys = nullptr;
	int* g_waitingForKey = nullptr;

	UiContext* uiContext = nullptr;
	UiContext* cgDC = nullptr;
	const char** expressionOperatorNames = nullptr;
	int* gameTypeCount = nullptr;
	gameTypeName_t* gameTypes = nullptr;
	MSG_ReadBit_t MSG_ReadBit = nullptr;
	MSG_ReadBits_t MSG_ReadBits = nullptr;
	MSG_ReadBigString_t MSG_ReadBigString = nullptr;
	I_strncpyz_t I_strncpyz = nullptr;
	GamerSettingState* gamerSettings = nullptr;
	ParseConfigStringToStructCustomSize_t ParseConfigStringToStructCustomSize = nullptr;
	BG_Turret_ComputeBarrelSpinRate_t BG_Turret_ComputeBarrelSpinRate = nullptr;
	LiveStorage_GetStat_t LiveStorage_GetStat = nullptr;
	LiveStorage_DoWeHaveStats_t LiveStorage_DoWeHaveStats = nullptr;
	LiveStorage_GetStatBuffer_t LiveStorage_GetStatBuffer = nullptr;
	LiveStorage_EnsureWeHaveStats_t LiveStorage_EnsureWeHaveStats = nullptr;
	StringTable_GetAsset_t StringTable_GetAsset = nullptr;
	StringTable_LookupRowNumForValue_t StringTable_LookupRowNumForValue = nullptr;
	StringTable_GetColumnValueForRow_t StringTable_GetColumnValueForRow = nullptr;
	CL_GetSnapshot_t CL_GetSnapshot = nullptr;
	CG_ExecuteNewServerCommands_t CG_ExecuteNewServerCommands = nullptr;
	R_RegisterModel_t R_RegisterModel = nullptr;
	UI_DrawHandlePic_t UI_DrawHandlePic = nullptr;
	IN_Init_t IN_Init = nullptr;
	IN_Frame_t IN_Frame = nullptr;
	IN_RecenterMouse_t IN_RecenterMouse = nullptr;
	IN_MouseEvent_t IN_MouseEvent = nullptr;
	WinMouseVars_t* s_wmv = nullptr;
	IDirect3DDevice9** dx_device = nullptr;
	R_AllocStaticIndexBuffer_t R_AllocStaticIndexBuffer = nullptr;
	Load_VertexBuffer_t Load_VertexBuffer = nullptr;
	R_SyncRenderThread_t R_SyncRenderThread = nullptr;
	R_WaitWorkerCmds_t R_WaitWorkerCmds = nullptr;
	Com_GetClientDObj_t Com_GetClientDObj = nullptr;
	DObjGetBoneIndex_t DObjGetBoneIndex = nullptr;
	CG_PlayBoltedEffect_t CG_PlayBoltedEffect = nullptr;
	CG_StopBoltedEffects_t CG_StopBoltedEffects = nullptr;
	CG_WeaponDObjHandle_t CG_WeaponDObjHandle = nullptr;
	visField_t* visionDefFields = nullptr;

	unsigned char** g_largeLocalBuf = nullptr;
	int* g_largeLocalPos = nullptr;
	int* g_largeLocalRightPos = nullptr;
	int* g_maxLargeLocalPos = nullptr;
	int* g_minLargeLocalRightPos = nullptr;

	void I_strncpyz_s(char* dest, std::size_t destsize, const char* src, std::size_t count)
	{
		if (!destsize && !dest)
		{
			return;
		}

		if (!src || !count)
		{
			*dest = '\0';
			return;
		}

		const auto* p = reinterpret_cast<const unsigned char*>(src - 1);
		auto* q = reinterpret_cast<unsigned char*>(dest - 1);
		auto n = count + 1;
		auto s = count;

		if (destsize <= count)
		{
			n = destsize + 1;
			s = destsize - 1;
		}

		do
		{
			if (!--n)
			{
				dest[s] = '\0';
				return;
			}

			*++q = *++p;
		}
		while (*q);
	}

	void I_strcpy(char* dest, std::size_t destsize, const char* src)
	{
		I_strncpyz_s(dest, destsize, src, destsize);
	}

	int LargeLocalBegin(int size)
	{
		constexpr auto pageMask = ~static_cast<std::uintptr_t>(PAGE_SIZE - 1);

		const int startPos = *g_largeLocalPos;
		const auto buffer = reinterpret_cast<std::uintptr_t>(*g_largeLocalBuf);
		const auto committedEnd = (buffer + *g_maxLargeLocalPos + PAGE_SIZE - 1) & pageMask;

		*g_largeLocalPos = startPos + size;
		*g_maxLargeLocalPos = std::max(*g_maxLargeLocalPos, *g_largeLocalPos);

		const auto neededEnd = (buffer + *g_maxLargeLocalPos + PAGE_SIZE - 1) & pageMask;

		if (neededEnd != committedEnd)
		{
			Z_VirtualCommit(reinterpret_cast<void*>(committedEnd), static_cast<int>(neededEnd - committedEnd));
		}

		return startPos;
	}

	int LargeLocalBeginRight(int size)
	{
		constexpr auto pageMask = ~static_cast<std::uintptr_t>(PAGE_SIZE - 1);

		const int startPos = *g_largeLocalRightPos;
		const auto buffer = reinterpret_cast<std::uintptr_t>(*g_largeLocalBuf);
		const auto committedStart = (buffer + *g_minLargeLocalRightPos) & pageMask;

		*g_largeLocalRightPos = startPos - size;
		*g_minLargeLocalRightPos = std::min(*g_minLargeLocalRightPos, *g_largeLocalRightPos);

		const auto neededStart = (buffer + *g_minLargeLocalRightPos) & pageMask;

		if (neededStart != committedStart)
		{
			Z_VirtualCommit(reinterpret_cast<void*>(neededStart), static_cast<int>(committedStart - neededStart));
		}

		return startPos;
	}

	void Load_IndexBuffer(const void* data, IDirect3DIndexBuffer9** storeHere, int count)
	{
		*storeHere = nullptr;

		const auto length = static_cast<int>(count * sizeof(unsigned short));
		void* const buffer = R_AllocStaticIndexBuffer(storeHere, length);

		if (!buffer)
		{
			*storeHere = nullptr;
			return;
		}

		std::memcpy(buffer, data, length);
		(*storeHere)->Unlock();
	}

	void BindFunctions()
	{
		Menus_CloseAll = BindFunction<Menus_CloseAll_t>(0x140266FC0);
		Material_RegisterHandle = BindFunction<Material_RegisterHandle_t>(0x14001A620);

		R_Cinematic_StartPlayback = BindFunction<R_Cinematic_StartPlayback_t>(0x140036790);
		R_Cinematic_StopPlayback = BindFunction<R_Cinematic_StopPlayback_t>(0x140036850);
		R_RegisterFont = BindFunction<R_RegisterFont_t>(0x14001A9F0);
		R_AddCmdDrawText = BindFunction<R_AddCmdDrawText_t>(0x14001C410);
		R_AddCmdDrawTextWithCursor = BindFunction<R_AddCmdDrawTextWithCursor_t>(0x14001C400);
		R_TextWidth = BindFunction<R_TextWidth_t>(0x14001AA20);
		R_TextHeight = BindFunction<R_TextHeight_t>(0x14001AB70);
		CL_KeyEvent = BindFunction<CL_KeyEvent_t>(0x1400EE7E0);
		CL_CharEvent = BindFunction<CL_CharEvent_t>(0x1400EE600);
		Cbuf_AddText = BindFunction<Cbuf_AddText_t>(0x1401E6DC0);
		Cmd_ExecuteSingleCommand = BindFunction<Cmd_ExecuteSingleCommand_t>(0x1401E7680);
		ScrPlace_GetActivePlacement = BindFunction<ScrPlace_GetActivePlacement_t>(0x1400F2B90);
		ScrPlace_EndFrame = BindFunction<ScrPlace_EndFrame_t>(0x1400F2B80);
		CL_ConsolePrint_AddLine = BindFunction<CL_ConsolePrint_AddLine_t>(0x1400EB8B0);
		Cmd_AddCommandInternal = BindFunction<Cmd_AddCommandInternal_t>(0x1401E72B0);
		Cmd_AddServerCommandInternal = BindFunction<Cmd_AddServerCommandInternal_t>(0x1401E7320);
		Cbuf_Execute = BindFunction<Cbuf_Execute_t>(0x1401E6EA0);
		Cbuf_InsertText = BindFunction<Cbuf_InsertText_t>(0x1401E71F0);
		Dvar_GetBool = BindFunction<Dvar_GetBool_t>(0x1402851A0);
		Dvar_GetInt = BindFunction<Dvar_GetInt_t>(0x140285220);
		Dvar_GetFloat = BindFunction<Dvar_GetFloat_t>(0x1402851E0);
		Dvar_ValueToString = BindFunction<Dvar_ValueToString_t>(0x140288690);
		CL_ConsolePrint = BindFunction<CL_ConsolePrint_t>(0x1400EB830);

		cmd_args = reinterpret_cast<CmdArgs*>(Utils::Hook::Rebase(0x141BBC640));
		sv_cmd_args = reinterpret_cast<CmdArgs*>(Utils::Hook::Rebase(0x141BBC6F0));
		cmd_functions = reinterpret_cast<cmd_function_s**>(Utils::Hook::Rebase(0x141BBC798));

		NET_OutOfBandPrint = BindFunction<NET_OutOfBandPrint_t>(0x140206A90);
		NET_OutOfBandData = BindFunction<NET_OutOfBandData_t>(0x1402069D0);
		NET_OutOfBandVoiceData = BindFunction<NET_OutOfBandVoiceData_t>(0x140206B50);

		MSG_Init = BindFunction<MSG_Init_t>(0x140201D20);
		MSG_WriteString = BindFunction<MSG_WriteString_t>(0x140202B60);
		MSG_WriteByte = BindFunction<MSG_WriteByte_t>(0x140202A40);
		MSG_WriteShort = BindFunction<MSG_WriteShort_t>(0x140202B30);
		MSG_WriteData = BindFunction<MSG_WriteData_t>(0x140202A60);
		MSG_WriteLong = BindFunction<MSG_WriteLong_t>(0x140202B00);
		MSG_ReadByte = BindFunction<MSG_ReadByte_t>(0x140202040);
		MSG_ReadShort = BindFunction<MSG_ReadShort_t>(0x1402024A0);
		MSG_ReadData = BindFunction<MSG_ReadData_t>(0x1402020B0);
		MSG_ReadStringLine = BindFunction<MSG_ReadStringLine_t>(0x140202630);

		Voice_IsClientTalking = BindFunction<Voice_IsClientTalking_t>(0x1402A9E90);
		Voice_IncomingVoiceData = BindFunction<Voice_IncomingVoiceData_t>(0x1402A99C0);

		NET_SendPacket = BindFunction<NET_SendPacket_t>(0x140206BF0);
		clc_serverAddress = reinterpret_cast<netadr_t*>(Utils::Hook::Rebase(0x140BFB7B0));
		NET_CompareAdr = BindFunction<NET_CompareAdr_t>(0x140206520);
		NET_CompareBaseAdr = BindFunction<NET_CompareBaseAdr_t>(0x1402065F0);
		NET_IsLocalAddress = BindFunction<NET_IsLocalAddress_t>(0x1402069C0);
		NET_StringToAdr = BindFunction<NET_StringToAdr_t>(0x140206D20);
		NetadrToSockadr = BindFunction<NetadrToSockadr_t>(0x14027F970);
		SockadrToNetadr = BindFunction<SockadrToNetadr_t>(0x14027FA00);
		numIP = reinterpret_cast<int*>(Utils::Hook::Rebase(0x14678C440));
		localIP = reinterpret_cast<netIP_t*>(Utils::Hook::Rebase(0x14678C450));

		ClientUserinfoChanged = BindFunction<ClientUserinfoChanged_t>(0x1401968B0);

		G_SetConstString = BindFunction<G_SetConstString_t>(0x1401AC240);

		Touch_Item = BindFunction<Touch_Item_t>(0x14016DEC0);

		Add_Ammo = BindFunction<Add_Ammo_t>(0x14016BE10);

		player_die = BindFunction<player_die_t>(0x14019C4E0);

		TeleportPlayer = BindFunction<TeleportPlayer_t>(0x1401A02A0);

		level = reinterpret_cast<level_locals_t*>(Utils::Hook::Rebase(0x141867020));

		level_bgs = reinterpret_cast<bgs_t*>(Utils::Hook::Rebase(0x1417CDF40));

		g_dwTlsIndex = reinterpret_cast<unsigned long*>(Utils::Hook::Rebase(0x148C26D58));

		CG_SetupWeaponDef = BindFunction<CG_SetupWeaponDef_t>(0x1400C54D0);

		BG_GetClipSize = BindFunction<BG_GetClipSize_t>(0x14009D260);

		BG_GetSharedAmmoCapSize = BindFunction<BG_GetSharedAmmoCapSize_t>(0x14009C820);

		SV_LinkEntity = BindFunction<SV_LinkEntity_t>(0x140233F90);

		SV_UnlinkEntity = BindFunction<SV_LinkEntity_t>(0x140235600);

		SV_SetBrushModel = BindFunction<SV_SetBrushModel_t>(0x140233740);

		Win_GetLanguage = BindFunction<Win_GetLanguage_t>(0x1402A46B0);

		UI_UpdateArenas = BindFunction<UI_UpdateArenas_t>(0x14026B800);
		arenaCount = reinterpret_cast<int*>(Utils::Hook::Rebase(0x1465D2850));

		FS_LoadZipFile = BindFunction<FS_LoadZipFile_t>(0x1402775D0);

		unzClose = BindFunction<unzClose_t>(0x1402CBD40);

		Info_ValueForKey = BindFunction<Info_ValueForKey_t>(0x14028CA30);
		g_assetNames = reinterpret_cast<const char**>(Utils::Hook::Rebase(0x140421280));
		DB_GetXAssetTypeSize = BindFunction<DB_GetXAssetTypeSize_t>(0x140117630);
		MSG_ReadBit = BindFunction<MSG_ReadBit_t>(0x1400AFB50);
		MSG_ReadBits = BindFunction<MSG_ReadBits_t>(0x1400AFBF0);
		MSG_ReadBigString = BindFunction<MSG_ReadBigString_t>(0x140201EF0);
		I_strncpyz = BindFunction<I_strncpyz_t>(0x14028C390);

		gamerSettings = reinterpret_cast<GamerSettingState*>(Utils::Hook::Rebase(0x1406CCCC0));

		ParseConfigStringToStructCustomSize = BindFunction<ParseConfigStringToStructCustomSize_t>(0x14028CD40);

		BG_Turret_ComputeBarrelSpinRate = BindFunction<BG_Turret_ComputeBarrelSpinRate_t>(0x140096700);

		LiveStorage_GetStat = BindFunction<LiveStorage_GetStat_t>(0x1401F7D20);

		LiveStorage_DoWeHaveStats = BindFunction<LiveStorage_DoWeHaveStats_t>(0x1401F7580);

		LiveStorage_GetStatBuffer = BindFunction<LiveStorage_GetStatBuffer_t>(0x1401F7D90);

		LiveStorage_EnsureWeHaveStats = BindFunction<LiveStorage_EnsureWeHaveStats_t>(0x1401F77B0);
		StringTable_GetAsset = BindFunction<StringTable_GetAsset_t>(0x140280D50);
		StringTable_LookupRowNumForValue = BindFunction<StringTable_LookupRowNumForValue_t>(0x140280EC0);
		StringTable_GetColumnValueForRow = BindFunction<StringTable_GetColumnValueForRow_t>(0x140280D70);
		CL_GetSnapshot = BindFunction<CL_GetSnapshot_t>(0x1400F4840);
		CG_ExecuteNewServerCommands = BindFunction<CG_ExecuteNewServerCommands_t>(0x1400E76F0);
		R_RegisterModel = BindFunction<R_RegisterModel_t>(0x1400300A0);
		UI_DrawHandlePic = BindFunction<UI_DrawHandlePic_t>(0x140250F40);
		IN_Init = BindFunction<IN_Init_t>(0x1402A2D80);
		IN_Frame = BindFunction<IN_Frame_t>(0x1402A2B70);
		IN_RecenterMouse = BindFunction<IN_RecenterMouse_t>(0x1402A2E70);
		IN_MouseEvent = BindFunction<IN_MouseEvent_t>(0x1402A2DD0);
		s_wmv = reinterpret_cast<WinMouseVars_t*>(Utils::Hook::Rebase(0x146786F58));
		dx_device = reinterpret_cast<IDirect3DDevice9**>(Utils::Hook::Rebase(0x148CC6BF0));
		R_AllocStaticIndexBuffer = BindFunction<R_AllocStaticIndexBuffer_t>(0x140038B40);
		Load_VertexBuffer = BindFunction<Load_VertexBuffer_t>(0x140038BD0);
		R_SyncRenderThread = BindFunction<R_SyncRenderThread_t>(0x14001B5D0);
		R_WaitWorkerCmds = BindFunction<R_WaitWorkerCmds_t>(0x1400478A0);
		Com_GetClientDObj =BindFunction<Com_GetClientDObj_t>(0x1401FBCB0);
		DObjGetBoneIndex = BindFunction<DObjGetBoneIndex_t>(0x1402ABF90);
		CG_PlayBoltedEffect = BindFunction<CG_PlayBoltedEffect_t>(0x1400AF2C0);
		CG_StopBoltedEffects = BindFunction<CG_StopBoltedEffects_t>(0x1400AF470);
		CG_WeaponDObjHandle = BindFunction<CG_WeaponDObjHandle_t>(0x1400C8130);
		visionDefFields = reinterpret_cast<visField_t*>(Utils::Hook::Rebase(0x14041F9F0));

		g_largeLocalBuf = reinterpret_cast<unsigned char**>(Utils::Hook::Rebase(0x146655B00));
		g_largeLocalPos = reinterpret_cast<int*>(Utils::Hook::Rebase(0x146653AD4));
		g_largeLocalRightPos = reinterpret_cast<int*>(Utils::Hook::Rebase(0x146655AF4));
		g_maxLargeLocalPos = reinterpret_cast<int*>(Utils::Hook::Rebase(0x146655AF0));
		g_minLargeLocalRightPos = reinterpret_cast<int*>(Utils::Hook::Rebase(0x146655AF8));

		Menus_FindByName = BindFunction<Menus_FindByName_t>(0x140267330);
		Menus_OpenByName = BindFunction<Menus_OpenByName_t>(0x140267DB0);
		Menus_CloseRequest = BindFunction<Menus_CloseRequest_t>(0x1402671E0);
		Menus_MenuIsInStack = BindFunction<Menus_MenuIsInStack_t>(0x140267A20);
		UI_AddMenuList = BindFunction<UI_AddMenuList_t>(0x140269510);
		Key_StringToKeynum = BindFunction<Key_StringToKeynum_t>(0x1400EFAA0);
		Menu_IsVisible = BindFunction<Menu_IsVisible_t>(0x140265BB0);
		UI_GetGameTypeDisplayName = BindFunction<UI_GetGameTypeDisplayName_t>(0x14026EEC0);
		UI_GetMapDisplayName = BindFunction<UI_GetMapDisplayName_t>(0x14026F240);
		UI_SafeTranslateString = BindFunction<UI_SafeTranslateString_t>(0x140272770);
		Party_AreWeHost = BindFunction<Party_AreWeHost_t>(0x1401086A0);
		PartyClient_CountMembersEvenIfInactive = BindFunction<PartyClient_CountMembersEvenIfInactive_t>(0x140105790);
		g_lobbyData = reinterpret_cast<PartyData*>(Utils::Hook::Rebase(0x140D0BB80));
		PartyHost_GetMemberName = BindFunction<PartyHost_GetMemberName_t>(0x1401108E0);
		G_GetClientScore = BindFunction<G_GetClientScore_t>(0x14019CF00);
		UI_DrawText = BindFunction<UI_DrawText_t>(0x14026D0C0);
		UI_TextWidth = BindFunction<UI_TextWidth_t>(0x140273090);
		UI_GetFontHandle = BindFunction<UI_GetFontHandle_t>(0x14026EE10);
		g_partyData = reinterpret_cast<PartyData*>(Utils::Hook::Rebase(0x140D0DF90));
		String_Parse = BindFunction<String_Parse_t>(0x1402693D0);
		UI_OwnerDrawHandleKey = BindFunction<UI_OwnerDrawHandleKey_t>(0x1402708F0);
		UI_FeederCount = BindFunction<UI_FeederCount_t>(0x14026D770);
		UI_FeederItemText = BindFunction<UI_FeederItemText_t>(0x14026DEB0);
		UI_FeederSelection = BindFunction<UI_FeederSelection_t>(0x14026E810);
		UI_FeederDoubleClick = BindFunction<UI_FeederDoubleClick_t>(0x14026D9F0);
		UI_OverrideCursorPos = BindFunction<UI_OverrideCursorPos_t>(0x14026FDC0);
		UI_FeederItemColor = BindFunction<UI_FeederItemColor_t>(0x14026DB50);
		GetLobbyMemberCount = BindFunction<GetLobbyMemberCount_t>(0x140273360);
		GetLobbyMemberXuid = BindFunction<GetLobbyMemberXuid_t>(0x140273480);
		UpdatePartyDvars = BindFunction<UpdatePartyDvars_t>(0x140273260);
		selectedPlayerXuid = reinterpret_cast<std::uint64_t*>(Utils::Hook::Rebase(0x14662AAA0));
		Live_GetXp = BindFunction<Live_GetXp_t>(0x1402A8D50);
		Live_GetPrestige = BindFunction<Live_GetPrestige_t>(0x1402A8CD0);
		Image_Setup = BindFunction<Image_Setup_t>(0x14006AC60);
		Image_Release = BindFunction<Image_Release_t>(0x140037670);
		Image_LoadFromFileWithReader = BindFunction<Image_LoadFromFileWithReader_t>(0x14006A180);
		Load_Texture = BindFunction<Load_Texture_t>(0x140037B00);
		CL_GetRankData = BindFunction<CL_GetRankData_t>(0x140101640);
		PlayerCards_GetPartyMemberData = BindFunction<PlayerCards_GetPartyMemberData_t>(0x1401E6910);
		PartyUI_GetSelectedPlayerListChangedTime = BindFunction<PartyUI_GetSelectedPlayerListChangedTime_t>(0x140116520);
		Party_SetUIPlayerCount = BindFunction<Party_SetUIPlayerCount_t>(0x140116C30);
		UI_ReplaceConversionInts = BindFunction<UI_ReplaceConversionInts_t>(0x140271A70);
		Item_GetListBoxDef = BindFunction<Item_GetListBoxDef_t>(0x14026B120);
		GetBlinkColor = BindFunction<GetBlinkColor_t>(0x14025E920);
		SND_PlayLocalSoundAliasByName = BindFunction<SND_PlayLocalSoundAliasByName_t>(0x140246D20);
		SEH_GetCurrentLanguage = BindFunction<SEH_GetCurrentLanguage_t>(0x14024E9D0);
		Vec3UnpackUnitVec = BindFunction<Vec3UnpackUnitVec_t>(0x14027FDA0);
		Vec3Normalize = BindFunction<Vec3Normalize_t>(0x14001ED70);
		Vec2Normalize = BindFunction<Vec2Normalize_t>(0x14027D4A0);
		AngleNormalize360 = BindFunction<AngleNormalize360_t>(0x140279250);
		vectoyaw = BindFunction<vectoyaw_t>(0x14027DF90);
		AimAssist_ApplyDeltas = BindFunction<AimAssist_ApplyDeltas_t>(0x140081000);
		aaGlobArray = reinterpret_cast<AimAssistGlobals*>(Utils::Hook::Rebase(0x140431550));
		aaInputGraph = reinterpret_cast<GraphFloat*>(Utils::Hook::Rebase(0x1404323B0));
		Key_IsCatcherActive = BindFunction<Key_IsCatcherActive_t>(0x1400EF740);
		Key_RemoveCatcher = BindFunction<Key_RemoveCatcher_t>(0x1400EF950);
		UI_KeyEvent = BindFunction<UI_KeyEvent_t>(0x14026F970);
		UI_GetActiveMenu = BindFunction<UI_GetActiveMenu_t>(0x14026E8F0);
		Scoreboard_HandleInput = BindFunction<Scoreboard_HandleInput_t>(0x1400E5A20);
		CG_ScoresDown = BindFunction<CG_ScoresDown_t>(0x1400D0680);
		CG_ScoresUp = BindFunction<CG_ScoresUp_t>(0x1400D0710);
		Key_GetBindingForCmd = BindFunction<Key_GetBindingForCmd_t>(0x1400EF620);
		Key_SetBinding = BindFunction<Key_SetBinding_t>(0x1400EF980);
		Key_SetCatcher = BindFunction<Key_SetCatcher_t>(0x1400EFA00);
		Key_WriteBindings = BindFunction<Key_WriteBindings_t>(0x1400EFCD0);
		playerKeys = reinterpret_cast<PlayerKeyState*>(Utils::Hook::Rebase(0x1406C70A0));
		g_waitingForKey = reinterpret_cast<int*>(Utils::Hook::Rebase(0x1465806A0));

		uiContext = reinterpret_cast<UiContext*>(Utils::Hook::Rebase(0x1466409F0));
		cgDC = reinterpret_cast<UiContext*>(Utils::Hook::Rebase(0x1406BD400));
		expressionOperatorNames = reinterpret_cast<const char**>(Utils::Hook::Rebase(0x140426A20));

		gameTypeCount = reinterpret_cast<int*>(Utils::Hook::Rebase(0x1465D0FC0));
		gameTypes = reinterpret_cast<gameTypeName_t*>(Utils::Hook::Rebase(0x1465D0FC4));
	}

	void ShowMessageBox(const std::string& message, const std::string& title)
	{
		if (!CL_IsCgameInitialized(0))
		{
			Dvar_SetStringByName("com_errorMessage", message.data());
			Dvar_SetStringByName("com_errorTitle", title.data());
			Cbuf_AddText(0, "openmenu error_popmenu_lobby\n");
		}
	}

	void UI_FilterStringForButtonAnimation(char* str, unsigned int strMaxSize)
	{
		constexpr int stillButtonsLanguage = 8;
		constexpr int pressedFromMs = 800;

		if (SEH_GetCurrentLanguage() == stillButtonsLanguage)
		{
			return;
		}

		if (Sys_Milliseconds() % 1000 <= pressedFromMs)
		{
			return;
		}

		for (std::size_t i = 0; i < strMaxSize && str[i]; ++i)
		{
			if (str[i] == 16)
			{
				str[i] = static_cast<char>(-68);
			}
			else if (str[i] == 17)
			{
				str[i] = static_cast<char>(-67);
			}
		}
	}

	unsigned int R_HashString(const char* string)
	{
		unsigned int hash = 0;

		while (*string)
		{
			hash = (*string | 0x20) ^ (33 * hash);
			++string;
		}

		return hash;
	}

	void Vec2UnpackTexCoords(PackedTexCoords in, float* out)
	{
		const unsigned int low = in.packed & 0xFFFF;
		const unsigned int high = in.packed >> 16;

		unsigned int lowBits = 0;
		unsigned int highBits = 0;

		if (low)
		{
			lowBits = ((low & 0x8000) << 16) | (((((low & 0x3FFF) << 14) - (~(low << 14) & 0x10000000)) ^ 0x80000001) >> 1);
		}

		if (high)
		{
			highBits = ((high & 0x8000) << 16) | (((((high & 0x3FFF) << 14) - (~(high << 14) & 0x10000000)) ^ 0x80000001) >> 1);
		}

		std::memcpy(&out[0], &lowBits, sizeof(float));
		std::memcpy(&out[1], &highBits, sizeof(float));
	}

	float GraphGetValueFromFraction(int knotCount, const float(*knots)[2], float fraction)
	{
		for (int knotIndex = 1; knotIndex < knotCount; ++knotIndex)
		{
			if (knots[knotIndex][0] < fraction)
			{
				continue;
			}

			const auto adjustedFraction = (fraction - knots[knotIndex - 1][0]) / (knots[knotIndex][0] - knots[knotIndex - 1][0]);
			return (knots[knotIndex][1] - knots[knotIndex - 1][1]) * adjustedFraction + knots[knotIndex - 1][1];
		}

		return -1.0f;
	}

	float GraphFloat_GetValue(const GraphFloat* graph, float fraction)
	{
		return GraphGetValueFromFraction(graph->knotCount, graph->knots, fraction) * graph->scale;
	}

	void MatrixVecMultiply(const float (&mulMat)[3][3], const float* mulVec, float* solution)
	{
		float result[3];
		result[0] = mulMat[0][0] * mulVec[0] + mulMat[1][0] * mulVec[1] + mulMat[2][0] * mulVec[2];
		result[1] = mulMat[0][1] * mulVec[0] + mulMat[1][1] * mulVec[1] + mulMat[2][1] * mulVec[2];
		result[2] = mulMat[0][2] * mulVec[0] + mulMat[1][2] * mulVec[1] + mulMat[2][2] * mulVec[2];

		std::memcpy(solution, result, sizeof(result));
	}

	void PartyHost_RemovePlayer(PartyData* party, int clientNum, bool shouldSendEndParty, const char* reason)
	{
		reinterpret_cast<void(*)(PartyData*, int, bool, const char*)>(Utils::Hook::Rebase(0x140110F90))(party, clientNum, shouldSendEndParty, reason);
	}
}
