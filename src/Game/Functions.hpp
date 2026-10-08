#pragma once

namespace Game
{
	typedef void(*Menus_CloseAll_t)(UiContext* context);
	extern Menus_CloseAll_t Menus_CloseAll;

	typedef Material*(*Material_RegisterHandle_t)(const char* name, int imageTrack);
	extern Material_RegisterHandle_t Material_RegisterHandle;

	typedef void(*R_Cinematic_StartPlayback_t)(const char* name, unsigned int playbackFlags, int startTimeMs);
	extern R_Cinematic_StartPlayback_t R_Cinematic_StartPlayback;

	typedef void(*R_Cinematic_StopPlayback_t)();
	extern R_Cinematic_StopPlayback_t R_Cinematic_StopPlayback;

	typedef Font_s*(*R_RegisterFont_t)(const char* name, int imageTrack);
	extern R_RegisterFont_t R_RegisterFont;

	typedef void(*R_AddCmdDrawText_t)(const char* text, int maxChars, Font_s* font,
		float x, float y, float xScale, float yScale, float rotation,
		const float* color, int style);
	extern R_AddCmdDrawText_t R_AddCmdDrawText;

	typedef void(*R_AddCmdDrawTextWithCursor_t)(const char* text, int maxChars, Font_s* font,
		float x, float y, float xScale, float yScale, float rotation,
		const float* color, int style, int cursorPos, char cursorChar);
	extern R_AddCmdDrawTextWithCursor_t R_AddCmdDrawTextWithCursor;

	typedef int(*R_TextWidth_t)(const char* text, int maxChars, Font_s* font);
	extern R_TextWidth_t R_TextWidth;

	typedef int(*R_TextHeight_t)(Font_s* font);
	extern R_TextHeight_t R_TextHeight;

	typedef void(*CL_KeyEvent_t)(int localClientNum, int key, int down, unsigned int time);
	extern CL_KeyEvent_t CL_KeyEvent;

	typedef void(*CL_CharEvent_t)(int localClientNum, int character);
	extern CL_CharEvent_t CL_CharEvent;

	typedef void(*Cbuf_AddText_t)(int localClientNum, const char* text);
	extern Cbuf_AddText_t Cbuf_AddText;

	typedef void(*Cmd_ExecuteSingleCommand_t)(int localClientNum, int controllerIndex, const char* text);
	extern Cmd_ExecuteSingleCommand_t Cmd_ExecuteSingleCommand;

	typedef const float*(*ScrPlace_GetActivePlacement_t)(int localClientNum);
	extern ScrPlace_GetActivePlacement_t ScrPlace_GetActivePlacement;

	typedef void(*ScrPlace_EndFrame_t)();
	extern ScrPlace_EndFrame_t ScrPlace_EndFrame;

	typedef void*(*CL_ConsolePrint_AddLine_t)(int localClientNum, int channel, const char* text,
		int duration, int pixelWidth, unsigned char color, int flags);
	extern CL_ConsolePrint_AddLine_t CL_ConsolePrint_AddLine;

	typedef void(*Cmd_AddCommandInternal_t)(const char* name, void(*function)(), cmd_function_s* allocated);
	extern Cmd_AddCommandInternal_t Cmd_AddCommandInternal;

	typedef void(*Cmd_AddServerCommandInternal_t)(const char* name, void(*function)(), cmd_function_s* allocated);
	extern Cmd_AddServerCommandInternal_t Cmd_AddServerCommandInternal;

	typedef void(*Cbuf_Execute_t)(int localClientNum, int controllerIndex);
	extern Cbuf_Execute_t Cbuf_Execute;

	typedef void(*Cbuf_InsertText_t)(int localClientNum, const char* text);
	extern Cbuf_InsertText_t Cbuf_InsertText;

	typedef bool(*Dvar_GetBool_t)(const char* name);
	extern Dvar_GetBool_t Dvar_GetBool;

	typedef int(*Dvar_GetInt_t)(const char* name);
	extern Dvar_GetInt_t Dvar_GetInt;

	typedef float(*Dvar_GetFloat_t)(const char* name);
	extern Dvar_GetFloat_t Dvar_GetFloat;

	typedef const char*(*Dvar_ValueToString_t)(const dvar_t* dvar, const DvarValue* value);
	extern Dvar_ValueToString_t Dvar_ValueToString;

	typedef void(*CL_ConsolePrint_t)(int localClientNum, int channel, const char* text, int a4, int a5, int a6);
	extern CL_ConsolePrint_t CL_ConsolePrint;

	extern CmdArgs* cmd_args;
	extern CmdArgs* sv_cmd_args;
	extern cmd_function_s** cmd_functions;

	typedef bool(*NET_OutOfBandPrint_t)(netsrc_t source, const netadr_t* target, const char* data);
	extern NET_OutOfBandPrint_t NET_OutOfBandPrint;

	typedef bool(*NET_OutOfBandData_t)(netsrc_t source, const netadr_t* target, const void* data, int length);
	extern NET_OutOfBandData_t NET_OutOfBandData;

	typedef bool(*NET_OutOfBandVoiceData_t)(netsrc_t source, const netadr_t* target, const unsigned char* data, int length, bool);
	extern NET_OutOfBandVoiceData_t NET_OutOfBandVoiceData;

	typedef void(*MSG_Init_t)(msg_t* buf, unsigned char* data, int length);
	extern MSG_Init_t MSG_Init;

	typedef void(*MSG_WriteString_t)(msg_t* sb, const char* s);
	extern MSG_WriteString_t MSG_WriteString;

	typedef void(*MSG_WriteByte_t)(msg_t* msg, int c);
	extern MSG_WriteByte_t MSG_WriteByte;

	typedef void(*MSG_WriteShort_t)(msg_t* msg, int c);
	extern MSG_WriteShort_t MSG_WriteShort;

	typedef void(*MSG_WriteData_t)(msg_t* buf, const void* data, int length);
	extern MSG_WriteData_t MSG_WriteData;

	typedef void(*MSG_WriteLong_t)(msg_t* msg, int c);
	extern MSG_WriteLong_t MSG_WriteLong;

	typedef int(*MSG_ReadByte_t)(msg_t* msg);
	extern MSG_ReadByte_t MSG_ReadByte;

	typedef int(*MSG_ReadShort_t)(msg_t* msg);
	extern MSG_ReadShort_t MSG_ReadShort;

	typedef void(*MSG_ReadData_t)(msg_t* msg, void* data, int len);
	extern MSG_ReadData_t MSG_ReadData;

	typedef char*(*MSG_ReadStringLine_t)(msg_t* msg, char* string, unsigned int maxChars);
	extern MSG_ReadStringLine_t MSG_ReadStringLine;

	typedef bool(*Voice_IsClientTalking_t)(unsigned int clientNum);
	extern Voice_IsClientTalking_t Voice_IsClientTalking;

	typedef void(*Voice_IncomingVoiceData_t)(const SessionData* session, int clientNum, unsigned char* data, int size);
	extern Voice_IncomingVoiceData_t Voice_IncomingVoiceData;

	typedef bool(*NET_SendPacket_t)(netsrc_t source, int length, const void* data, const netadr_t* target);
	extern NET_SendPacket_t NET_SendPacket;

	extern netadr_t* clc_serverAddress;

	typedef bool(*NET_CompareAdr_t)(const netadr_t* a, const netadr_t* b);
	extern NET_CompareAdr_t NET_CompareAdr;

	typedef bool(*NET_CompareBaseAdr_t)(const netadr_t* a, const netadr_t* b);
	extern NET_CompareBaseAdr_t NET_CompareBaseAdr;

	typedef bool(*NET_IsLocalAddress_t)(const netadr_t* address);
	extern NET_IsLocalAddress_t NET_IsLocalAddress;

	typedef bool(*NET_StringToAdr_t)(const char* text, netadr_t* out);
	extern NET_StringToAdr_t NET_StringToAdr;

	typedef void(*NetadrToSockadr_t)(const netadr_t* address, sockaddr* out);
	extern NetadrToSockadr_t NetadrToSockadr;

	typedef void(*SockadrToNetadr_t)(const sockaddr* address, netadr_t* out);
	extern SockadrToNetadr_t SockadrToNetadr;

	extern int* numIP;
	extern netIP_t* localIP;

	typedef void(*ClientUserinfoChanged_t)(int clientNum);
	extern ClientUserinfoChanged_t ClientUserinfoChanged;

	constexpr std::size_t MAX_CLIENTS = 127;

	typedef void(*G_SetConstString_t)(unsigned short* to, const char* from);
	extern G_SetConstString_t G_SetConstString;

	typedef void(*Touch_Item_t)(gentity_s* ent, gentity_s* other, int touched);
	extern Touch_Item_t Touch_Item;

	typedef void(*Add_Ammo_t)(gentity_s* ent, unsigned int weaponIndex, unsigned char weaponModel, int count, int fillClip);
	extern Add_Ammo_t Add_Ammo;

	typedef void(*player_die_t)(gentity_s* self, const gentity_s* inflictor, gentity_s* attacker, int damage, int meansOfDeath, int iWeapon, const float* vDir, hitLocation_t hitLoc, int psTimeOffset);
	extern player_die_t player_die;

	typedef void(*TeleportPlayer_t)(gentity_s* entity, float* pos, float* orientation);
	extern TeleportPlayer_t TeleportPlayer;

	extern level_locals_t* level;
	extern bgs_t* level_bgs;

	extern unsigned long* g_dwTlsIndex;

	typedef void(*CG_SetupWeaponDef_t)(int localClientNum, unsigned int weaponIndex);
	extern CG_SetupWeaponDef_t CG_SetupWeaponDef;

	typedef int(*BG_GetClipSize_t)(const playerState_s* ps, unsigned int weaponIndex);
	extern BG_GetClipSize_t BG_GetClipSize;

	typedef int(*BG_GetSharedAmmoCapSize_t)(int sharedAmmoCapIndex);
	extern BG_GetSharedAmmoCapSize_t BG_GetSharedAmmoCapSize;

	typedef void(*SV_LinkEntity_t)(gentity_s* ent);
	extern SV_LinkEntity_t SV_LinkEntity;
	extern SV_LinkEntity_t SV_UnlinkEntity;

	typedef bool(*SV_SetBrushModel_t)(gentity_s* ent);
	extern SV_SetBrushModel_t SV_SetBrushModel;

	typedef const char*(*Win_GetLanguage_t)();
	extern Win_GetLanguage_t Win_GetLanguage;

	typedef iwd_t*(*FS_LoadZipFile_t)(const char* zipfile, const char* basename);
	extern FS_LoadZipFile_t FS_LoadZipFile;

	typedef void(*unzClose_t)(void* handle);
	extern unzClose_t unzClose;

	typedef void(*UI_UpdateArenas_t)();
	extern UI_UpdateArenas_t UI_UpdateArenas;

	extern int* arenaCount;

	typedef const char*(*Info_ValueForKey_t)(const char* infoString, const char* key);
	extern Info_ValueForKey_t Info_ValueForKey;

	typedef menuDef_t*(*Menus_FindByName_t)(UiContext* context, const char* name);
	extern Menus_FindByName_t Menus_FindByName;

	typedef int(*Menus_OpenByName_t)(UiContext* context, const char* name);
	extern Menus_OpenByName_t Menus_OpenByName;

	typedef void(*Menus_CloseRequest_t)(UiContext* context, menuDef_t* menu);
	extern Menus_CloseRequest_t Menus_CloseRequest;

	typedef bool(*Menus_MenuIsInStack_t)(UiContext* context, menuDef_t* menu);
	extern Menus_MenuIsInStack_t Menus_MenuIsInStack;

	typedef void(*UI_AddMenuList_t)(UiContext* context, MenuList* list, int closeRequest);
	extern UI_AddMenuList_t UI_AddMenuList;

	typedef int(*Key_StringToKeynum_t)(const char* name);
	extern Key_StringToKeynum_t Key_StringToKeynum;

	typedef bool(*Menu_IsVisible_t)(UiContext* context, menuDef_t* menu);
	extern Menu_IsVisible_t Menu_IsVisible;

	typedef const char*(*UI_GetGameTypeDisplayName_t)(const char* gameType);
	extern UI_GetGameTypeDisplayName_t UI_GetGameTypeDisplayName;

	typedef const char*(*UI_GetMapDisplayName_t)(const char* mapName);
	extern UI_GetMapDisplayName_t UI_GetMapDisplayName;

	typedef const char*(*UI_SafeTranslateString_t)(const char* reference);
	extern UI_SafeTranslateString_t UI_SafeTranslateString;

	typedef bool(*Party_AreWeHost_t)(PartyData* party);
	extern Party_AreWeHost_t Party_AreWeHost;

	typedef int(*PartyClient_CountMembersEvenIfInactive_t)(PartyData* party);
	extern PartyClient_CountMembersEvenIfInactive_t PartyClient_CountMembersEvenIfInactive;

	typedef const char*(*PartyHost_GetMemberName_t)(PartyData* party, int member);
	extern PartyHost_GetMemberName_t PartyHost_GetMemberName;

	void PartyHost_RemovePlayer(PartyData* party, int clientNum, bool shouldSendEndParty, const char* reason);

	typedef int(*G_GetClientScore_t)(int clientNum);
	extern G_GetClientScore_t G_GetClientScore;

	typedef void(*UI_DrawText_t)(const float* placement, const char* text, int maxChars, Font_s* font,
		float x, float y, int horzAlign, int vertAlign, float scale, const float* color, int style);
	extern UI_DrawText_t UI_DrawText;

	typedef int(*UI_TextWidth_t)(const char* text, int maxChars, Font_s* font, float scale);
	extern UI_TextWidth_t UI_TextWidth;

	typedef Font_s*(*UI_GetFontHandle_t)(const float* placement, int fontEnum, float scale);
	extern UI_GetFontHandle_t UI_GetFontHandle;

	void UI_FilterStringForButtonAnimation(char* str, unsigned int strMaxSize);

	extern PartyData* g_lobbyData;
	extern PartyData* g_partyData;

	void I_strncpyz_s(char* dest, std::size_t destsize, const char* src, std::size_t count);
	void I_strcpy(char* dest, std::size_t destsize, const char* src);

	int LargeLocalBegin(int size);
	int LargeLocalBeginRight(int size);

	extern unsigned char** g_largeLocalBuf;
	extern int* g_largeLocalPos;
	extern int* g_largeLocalRightPos;
	extern int* g_maxLargeLocalPos;
	extern int* g_minLargeLocalRightPos;

	typedef int(*String_Parse_t)(const char** text, char* buffer, int size);
	extern String_Parse_t String_Parse;

	typedef void(*UI_OwnerDrawHandleKey_t)(int ownerDraw, int flags, float* special, int key);
	extern UI_OwnerDrawHandleKey_t UI_OwnerDrawHandleKey;

	typedef int(*UI_FeederCount_t)(int localClientNum, float feeder);
	extern UI_FeederCount_t UI_FeederCount;

	typedef const char*(*UI_FeederItemText_t)(int localClientNum, itemDef_s* item, float feeder,
		int index, int column, float* a6, float* a7, float* a8, float* a9, Material** material);
	extern UI_FeederItemText_t UI_FeederItemText;

	typedef void(*UI_FeederSelection_t)(int localClientNum, float feeder, int index);
	extern UI_FeederSelection_t UI_FeederSelection;

	typedef int(*UI_FeederDoubleClick_t)(int localClientNum, float feeder, int index);
	extern UI_FeederDoubleClick_t UI_FeederDoubleClick;

	typedef void(*UI_OverrideCursorPos_t)(int localClientNum, itemDef_s* item);
	extern UI_OverrideCursorPos_t UI_OverrideCursorPos;

	typedef void(*UI_FeederItemColor_t)(int localClientNum, itemDef_s* item, float feeder,
		int index, int column, float* color);
	extern UI_FeederItemColor_t UI_FeederItemColor;

	typedef int(*GetLobbyMemberCount_t)();
	extern GetLobbyMemberCount_t GetLobbyMemberCount;

	typedef std::uint64_t(*GetLobbyMemberXuid_t)(int localClientNum, int index);
	extern GetLobbyMemberXuid_t GetLobbyMemberXuid;

	typedef void(*UpdatePartyDvars_t)(int localClientNum, int index, PartyData* party, std::uint64_t xuid);
	extern UpdatePartyDvars_t UpdatePartyDvars;

	extern std::uint64_t* selectedPlayerXuid;

	typedef int(*Live_GetXp_t)(int controllerIndex);
	extern Live_GetXp_t Live_GetXp;

	typedef int(*Live_GetPrestige_t)(int controllerIndex);
	extern Live_GetPrestige_t Live_GetPrestige;

	typedef void(*Image_Setup_t)(GfxImage* image, int width, int height, int depth, unsigned int flags, D3DFORMAT format);
	extern Image_Setup_t Image_Setup;

	typedef void(*Image_Release_t)(GfxImage* image);
	extern Image_Release_t Image_Release;

	typedef int(*ImageFileReader_t)(const char* filename, std::int64_t* file);
	typedef bool(*Image_LoadFromFileWithReader_t)(GfxImage* image, ImageFileReader_t reader);
	extern Image_LoadFromFileWithReader_t Image_LoadFromFileWithReader;

	typedef void(*Load_Texture_t)(GfxImageLoadDef** loadDef, GfxImage* image);
	extern Load_Texture_t Load_Texture;

	typedef const char*(*CL_GetRankData_t)(int rank, int column);
	extern CL_GetRankData_t CL_GetRankData;

	typedef PlayerCardData*(*PlayerCards_GetPartyMemberData_t)(int localClientNum, int lookupType, unsigned int index);
	extern PlayerCards_GetPartyMemberData_t PlayerCards_GetPartyMemberData;

	typedef int(*PartyUI_GetSelectedPlayerListChangedTime_t)();
	extern PartyUI_GetSelectedPlayerListChangedTime_t PartyUI_GetSelectedPlayerListChangedTime;

	typedef void(*Party_SetUIPlayerCount_t)(PartyData* party);
	extern Party_SetUIPlayerCount_t Party_SetUIPlayerCount;

	typedef const char*(*UI_ReplaceConversionInts_t)(const char* text, int count, int* values);
	extern UI_ReplaceConversionInts_t UI_ReplaceConversionInts;

	typedef listBoxDef_s*(*Item_GetListBoxDef_t)(itemDef_s* item);
	extern Item_GetListBoxDef_t Item_GetListBoxDef;

	typedef void(*GetBlinkColor_t)(UiContext* context, const float* color, float* out);
	extern GetBlinkColor_t GetBlinkColor;

	typedef int(*SND_PlayLocalSoundAliasByName_t)(int localClientNum, const char* alias, int system);
	extern SND_PlayLocalSoundAliasByName_t SND_PlayLocalSoundAliasByName;

	typedef int(*SEH_GetCurrentLanguage_t)();
	extern SEH_GetCurrentLanguage_t SEH_GetCurrentLanguage;

	typedef void(*Vec3UnpackUnitVec_t)(PackedUnitVec in, float* out);
	extern Vec3UnpackUnitVec_t Vec3UnpackUnitVec;

	typedef void(*Vec3Normalize_t)(float* v);
	extern Vec3Normalize_t Vec3Normalize;

	typedef float(*Vec2Normalize_t)(float* v);
	extern Vec2Normalize_t Vec2Normalize;

	typedef float(*AngleNormalize360_t)(float angle);
	extern AngleNormalize360_t AngleNormalize360;

	typedef float(*vectoyaw_t)(const float* vec);
	extern vectoyaw_t vectoyaw;

	typedef void(*AimAssist_ApplyDeltas_t)(const AimInput* input, AimOutput* output);
	extern AimAssist_ApplyDeltas_t AimAssist_ApplyDeltas;

	extern AimAssistGlobals* aaGlobArray;
	extern GraphFloat* aaInputGraph;

	float GraphGetValueFromFraction(int knotCount, const float(*knots)[2], float fraction);
	float GraphFloat_GetValue(const GraphFloat* graph, float fraction);

	unsigned int R_HashString(const char* string);
	void Vec2UnpackTexCoords(PackedTexCoords in, float* out);
	void MatrixVecMultiply(const float (&mulMat)[3][3], const float* mulVec, float* solution);

	typedef bool(*Key_IsCatcherActive_t)(int localClientNum, int mask);
	extern Key_IsCatcherActive_t Key_IsCatcherActive;

	typedef void(*Key_RemoveCatcher_t)(int localClientNum, int andMask);
	extern Key_RemoveCatcher_t Key_RemoveCatcher;

	typedef void(*UI_KeyEvent_t)(int localClientNum, int key, int down);
	extern UI_KeyEvent_t UI_KeyEvent;

	typedef int(*UI_GetActiveMenu_t)(int localClientNum);
	extern UI_GetActiveMenu_t UI_GetActiveMenu;

	typedef bool(*Scoreboard_HandleInput_t)(int localClientNum, int key);
	extern Scoreboard_HandleInput_t Scoreboard_HandleInput;

	typedef void(*CG_ScoresDown_t)(int localClientNum);
	extern CG_ScoresDown_t CG_ScoresDown;

	typedef void(*CG_ScoresUp_t)(int localClientNum);
	extern CG_ScoresUp_t CG_ScoresUp;

	typedef int(*Key_GetBindingForCmd_t)(const char* command);
	extern Key_GetBindingForCmd_t Key_GetBindingForCmd;

	typedef void(*Key_SetBinding_t)(int localClientNum, int keyNum, int binding);
	extern Key_SetBinding_t Key_SetBinding;

	typedef void(*Key_SetCatcher_t)(int localClientNum, int catcher);
	extern Key_SetCatcher_t Key_SetCatcher;

	typedef void(*Key_WriteBindings_t)(int localClientNum, fileHandle_t file);
	extern Key_WriteBindings_t Key_WriteBindings;

	extern PlayerKeyState* playerKeys;
	extern int* g_waitingForKey;

	extern UiContext* uiContext;

	extern UiContext* cgDC;

	extern const char** expressionOperatorNames;

	extern int* gameTypeCount;
	extern gameTypeName_t* gameTypes;

	extern const char** g_assetNames;

	typedef int(*DB_GetXAssetTypeSize_t)(int type);
	extern DB_GetXAssetTypeSize_t DB_GetXAssetTypeSize;

	typedef int(*MSG_ReadBit_t)(msg_t* msg);
	extern MSG_ReadBit_t MSG_ReadBit;

	typedef int(*MSG_ReadBits_t)(msg_t* msg, int bits);
	extern MSG_ReadBits_t MSG_ReadBits;

	typedef const char*(*MSG_ReadBigString_t)(msg_t* msg);
	extern MSG_ReadBigString_t MSG_ReadBigString;

	typedef char*(*I_strncpyz_t)(char* dest, const char* src, int destsize);
	extern I_strncpyz_t I_strncpyz;

	constexpr std::size_t MAX_LOCAL_CLIENTS = 1;

	extern GamerSettingState* gamerSettings;

	void ShowMessageBox(const std::string& message, const std::string& title);

	typedef bool(*ParseConfigStringToStructCustomSize_t)(void* pStruct, const cspField_t* pFieldList, int iNumFields,
		const char* pszBuffer, int iMaxFieldTypes, int(*parseSpecialFieldType)(void*, const char*, int),
		void(*parseStrcpy)(void*, const char*));
	extern ParseConfigStringToStructCustomSize_t ParseConfigStringToStructCustomSize;

	typedef float(*BG_Turret_ComputeBarrelSpinRate_t)(const WeaponDef* weapDef, const LerpEntityStateTurret* turretState, int time);
	extern BG_Turret_ComputeBarrelSpinRate_t BG_Turret_ComputeBarrelSpinRate;

	typedef int(*LiveStorage_GetStat_t)(int controllerIndex, int index);
	extern LiveStorage_GetStat_t LiveStorage_GetStat;

	typedef bool(*LiveStorage_DoWeHaveStats_t)(int controllerIndex);
	extern LiveStorage_DoWeHaveStats_t LiveStorage_DoWeHaveStats;

	typedef char*(*LiveStorage_GetStatBuffer_t)(int controllerIndex);
	extern LiveStorage_GetStatBuffer_t LiveStorage_GetStatBuffer;

	typedef void(*LiveStorage_EnsureWeHaveStats_t)(int controllerIndex);
	extern LiveStorage_EnsureWeHaveStats_t LiveStorage_EnsureWeHaveStats;

	typedef void(*StringTable_GetAsset_t)(const char* filename, const StringTable** table);
	extern StringTable_GetAsset_t StringTable_GetAsset;

	typedef int(*StringTable_LookupRowNumForValue_t)(const StringTable* table, int comparisonColumn, const char* value);
	extern StringTable_LookupRowNumForValue_t StringTable_LookupRowNumForValue;

	typedef const char*(*StringTable_GetColumnValueForRow_t)(const StringTable* table, int row, int column);
	extern StringTable_GetColumnValueForRow_t StringTable_GetColumnValueForRow;

	typedef int(*CL_GetSnapshot_t)(int localClientNum, int snapshotNumber, snapshot_s* snapshot);
	extern CL_GetSnapshot_t CL_GetSnapshot;

	typedef void(*CG_ExecuteNewServerCommands_t)(int localClientNum, int latestSequence);
	extern CG_ExecuteNewServerCommands_t CG_ExecuteNewServerCommands;

	typedef XModel*(*R_RegisterModel_t)(const char* name);
	extern R_RegisterModel_t R_RegisterModel;

	typedef void(*UI_DrawHandlePic_t)(const ScreenPlacement* scrPlace, float x, float y, float w, float h, int horzAlign, int vertAlign, const float* color, Material* material);
	extern UI_DrawHandlePic_t UI_DrawHandlePic;

	typedef void(*IN_Init_t)();
	extern IN_Init_t IN_Init;

	typedef void(*IN_Frame_t)();
	extern IN_Frame_t IN_Frame;

	typedef BOOL(*IN_RecenterMouse_t)();
	extern IN_RecenterMouse_t IN_RecenterMouse;

	typedef void(*IN_MouseEvent_t)(int mstate);
	extern IN_MouseEvent_t IN_MouseEvent;

	extern WinMouseVars_t* s_wmv;

	extern IDirect3DDevice9** dx_device;

	typedef void*(*R_AllocStaticIndexBuffer_t)(IDirect3DIndexBuffer9** store, int length);
	extern R_AllocStaticIndexBuffer_t R_AllocStaticIndexBuffer;

	typedef void(*Load_VertexBuffer_t)(IDirect3DVertexBuffer9** store, const void* data, int length);
	extern Load_VertexBuffer_t Load_VertexBuffer;

	void Load_IndexBuffer(const void* data, IDirect3DIndexBuffer9** storeHere, int count);

	typedef void(*R_SyncRenderThread_t)();
	extern R_SyncRenderThread_t R_SyncRenderThread;

	typedef void(*R_WaitWorkerCmds_t)();
	extern R_WaitWorkerCmds_t R_WaitWorkerCmds;

	typedef DObj*(*Com_GetClientDObj_t)(int handle, int localClientNum);
	extern Com_GetClientDObj_t Com_GetClientDObj;

	typedef int(*DObjGetBoneIndex_t)(const DObj* obj, unsigned int name, unsigned char* index);
	extern DObjGetBoneIndex_t DObjGetBoneIndex;

	typedef void(*CG_PlayBoltedEffect_t)(int localClientNum, const FxEffectDef* fxDef, int dobjHandle, unsigned int boneName);
	extern CG_PlayBoltedEffect_t CG_PlayBoltedEffect;

	typedef void(*CG_StopBoltedEffects_t)(int localClientNum, const FxEffectDef* fxDef, int dobjHandle, unsigned int boneName);
	extern CG_StopBoltedEffects_t CG_StopBoltedEffects;

	typedef int(*CG_WeaponDObjHandle_t)(int hand);
	extern CG_WeaponDObjHandle_t CG_WeaponDObjHandle;

	extern visField_t* visionDefFields;

	void BindFunctions();
}
