#pragma once

namespace Components::BotAI
{
	using Dvar_FindVar_t = void* (*)(const char* name);
	using Dvar_SetFromStringByName_t = void* (*)(const char* name, const char* value);
	using Dvar_RegisterInt_t = void* (*)(const char* name, int value, int minValue, int maxValue,
										 int flags, const char* description);

	using Session_GetXuid_t = unsigned long long (*)(void* session, int clientNum);
	using Cmd_AddCommandInternal_t = void* (*)(const char* name, void (*func)(), void* storage);
	using G_LocationalTracePassed_t = int (*)(const float* start, const float* end,
											  int passEntNum, int passEntNum2,
											  int contentmask, unsigned char* priorityMap);
	using BG_GetWeaponCompleteDef_t = const char* (*)(int index);
	using StructuredDataDefGetAsset_t = void* (*)(const char* name, unsigned bufferSize);
	using StructuredDataSetString_t = int (*)(void* lookup, void* buffer,
											  unsigned char* modifiedFlags, const char* value);
	using StructuredDataInitLookup_t = void (*)(void* def, void* lookup);
	using StructuredDataLookupString_t = void (*)(void* lookup, const char* propertyName);
	using StructuredDataLookupInt_t = void (*)(void* lookup, int arrayIndex);

	inline Dvar_FindVar_t Dvar_FindVar = nullptr;
	inline Dvar_SetFromStringByName_t Dvar_SetFromStringByName = nullptr;

	inline Dvar_RegisterInt_t Dvar_RegisterInt = nullptr;

	constexpr int dvarCurrent = 0x10;
	constexpr int dvarFlags = 0x8;
	constexpr unsigned dvarFlagCheat = 1 << 2;

	using Cbuf_AddText_t = void (*)(int localClient, const char* text);

	inline Cbuf_AddText_t Cbuf_AddText = nullptr;

	inline std::uintptr_t sv_privateClients = 0;

	inline std::uintptr_t lobbySession = 0;

	inline std::uintptr_t svState = 0;
	inline std::uintptr_t cmd_args_ptr = 0;

	constexpr int cmdArgcIndex = 17;
	constexpr int cmdArgvOffset = 104;

	inline std::uintptr_t svs_time = 0;
	inline std::uintptr_t svs_clients = 0;

	constexpr int svClientStride = 681904;
	constexpr int svClientState = 0x0;

	constexpr int svClientPing = 135908;

	constexpr int maxClients = static_cast<int>(Game::MAX_CLIENTS);

	constexpr int sessionUserCount = 18;

	using SV_DropClient_t = void (*)(void* client, const char* reason, unsigned char tellThem);

	inline std::uintptr_t svs_numClients = 0;

	constexpr int gentityIsLinked = 256;
	constexpr int gentityModelType = 257;

	constexpr int gentityContents = 284;
	constexpr int gentityAbsMid = 288;
	constexpr int gentityAbsHalf = 300;
	constexpr int gentityOrigin = 312;
	constexpr int gentityAngles = 324;
	constexpr int gentityClient = 344;

	constexpr int gentityClassname = 388;

	constexpr int gentityTargetname = 394;

	constexpr int gentityHealth = 428;
	constexpr int gentityDamage = 436;

	constexpr int gentityStride = 664;
	constexpr int gentityMaxEntities = 2048;
	constexpr int gentityModelTypeTrigger = 3;
	constexpr int gentityModelTypeBrush = 4;

	constexpr int psPmFlags = 12;
	constexpr int psOrigin = 28;
	constexpr int psVelocity = 40;
	constexpr int psLadderVec = 112;
	constexpr int psThrowBackOwner = 56;
	constexpr int psRemoteControlEnt = 76;
	constexpr int psDeltaAngles = 96;

	constexpr int psViewAngles = 268;
	constexpr int psViewHeight = 284;
	constexpr int psDamageEvent = 316;
	constexpr int psDamageYaw = 320;
	constexpr int psDamagePitch = 324;
	constexpr int psDamageCount = 328;
	constexpr int psLocationSelection = 436;
	constexpr int psHeldWeapons = 544;
	constexpr int psWeapon = 692;

	constexpr int psPerks = 1064;
	constexpr int psAdsAmount = 704;
	constexpr int psAimSpreadScale = 708;
	constexpr int psAmmoPoolId = 848;
	constexpr int psAmmoClip = 852;
	constexpr int psAmmoClipLeft = 856;
	constexpr int psStockPoolId = 728;
	constexpr int psStockCount = 732;
	constexpr int psStockStride = 8;
	constexpr int psAmmoStride = 12;
	constexpr int psAmmoSlots = 15;
	constexpr int psShellshockTime = 1168;
	constexpr int psShellshockDuration = 1172;

	constexpr int cmdButtonAttack = 0x1;
	constexpr int cmdButtonSprint = 0x2;
	constexpr int cmdButtonMelee = 0x4;
	constexpr int cmdButtonUse = 0x8;
	constexpr int cmdButtonReload = 0x20;

	constexpr int cmdButtonProne = 0x100;
	constexpr int cmdButtonCrouch = 0x200;
	constexpr int cmdButtonUp = 0x400;
	constexpr int cmdButtonAds = 0x800;
	constexpr int cmdButtonBreath = 0x2000;
	constexpr int cmdButtonFrag = 0x4000;
	constexpr int cmdButtonSpecial = 0x8000;
	constexpr int cmdButtonThrow = 0x80000;
	constexpr int cmdButtonSelectLocation = 0x10000;
	constexpr int cmdButtonCancelLocation = 0x20000;
	constexpr int cmdButtonRemoteAngles = 0x100000;

	constexpr int entityNumNone = 2047;
	constexpr int pmFlagLadder = 0x8;
	constexpr int pmFlagMantle = 0x4;
	constexpr int pmFlagSprint = 0x4000;
	constexpr int psWeaponStateHand0 = 504;
	constexpr int weaponStateOffhandFirst = 16;
	constexpr int weaponStateOffhandStart = 19;

	constexpr int weapClassRifle = 0;
	constexpr int weapClassSniper = 1;
	constexpr int weapClassMg = 2;
	constexpr int weapClassSmg = 3;
	constexpr int weapClassSpread = 4;
	constexpr int weapClassPistol = 5;
	constexpr int weapClassGrenade = 6;
	constexpr int maskWorldOnly = 0x806831;

	constexpr int contentsMissileClip = 0x80;
	constexpr int surfaceSky = 0x4;
	constexpr int contentsFoliage = 0x2;
	constexpr int contentsAiNoSight = 0x1000;
	constexpr int contentsGlass = 0x10;
	constexpr int maskSight = maskWorldOnly | contentsFoliage | contentsAiNoSight;

	constexpr int maskSightThroughGlass = maskSight & ~contentsGlass;
	constexpr int maskPlayerSolidNoBodies = 0x810011;
	constexpr int traceFraction = 0x0;
	constexpr int traceNormal = 0x4;
	constexpr int traceSurfaceFlags = 0x10;
	constexpr int traceContents = 0x14;
	constexpr int traceHitType = 0x20;
	constexpr int traceHitId = 0x24;
	constexpr int traceStartSolid = 0x2D;
	constexpr int traceAllSolid = 0x2C;
	constexpr int glassPieceStride = 12;
	constexpr int contentsPlayerClip = 0x10000;
	constexpr int surfLadder = 0x8;
	constexpr float mantleOverDrop = 18.0f;
	constexpr int traceHitTypeGlass = 4;

	constexpr int contentsMantle = 0x1000000;
	constexpr int surfMantleOn = 0x2000000;
	constexpr int surfMantleOver = 0x4000000;
	constexpr int surfMantleAny = 0x6000000;
	constexpr float mantleCheckBehind = 14.9f;
	constexpr float mantleOverReach = 31.0f;

	constexpr int svClientDeltaMessage = 8;
	constexpr int svClientOutgoingSeq = 24;
	constexpr int svClientRemotePort = 48;
	constexpr int svClientGentity = 135864;
	constexpr int svClientIsTest = 269068;

	constexpr int gclientSessionState = 12572;
	constexpr int gclientConnected = 12616;
	constexpr int gclientFlags = 13204;
	constexpr int gclientFlagFrozen = 0x4;

	constexpr int gclientRank = 12848;
	constexpr int gclientPrestige = 12852;
	constexpr int gclientCardIcon = 12872;
	constexpr int gclientCardTitle = 12876;
	constexpr int gclientCardNameplate = 12880;

	constexpr int gclientDeaths = 12600;
	constexpr int gclientKills = 12604;

	constexpr int weapDefClipPool = 856;
	constexpr int weapDefAmmoPool = 840;
	constexpr int weapDefInventoryType = 96;
	constexpr int weapInventoryAltMode = 3;

	constexpr int weapDefClass = 88;
	constexpr int gclientTeam = 12764;
	inline std::uintptr_t scr_const_menuresponse = 0;

	constexpr bool botsCompassMapped = false;

	constexpr int cmdAltWeapon = 22;
	constexpr int cmdOffhandWeapon = 24;
	constexpr int cmdMeleeYaw = 28;
	constexpr int cmdMeleeDist = 32;
	constexpr int cmdSelectedLoc = 33;
	constexpr int cmdRemoteAngles = 36;
	constexpr int cmdServerTime = 0;
	constexpr int cmdButtons = 4;
	constexpr int cmdAngles = 8;
	constexpr int cmdWeapon = 20;
	constexpr int cmdForwardMove = 26;
	constexpr int cmdRightMove = 27;
	constexpr int cmdSize = 40;

	constexpr int playerDataSize = 8188;

	using SV_AddTestClient_t = void* (*)();
	using SV_ClientThink_t = void (*)(void* client, void* userCmd);
	using SV_GetPlayerstate_t = char* (*)(int clientNum);
	using SV_GetPersistentData_t = unsigned char* (*)(int clientNum);
	using SV_Trace_t = void (*)(void* results, const float* start, const float* end,
								const float* bounds, const int* ignoreParams, int contentmask,
								int locational, unsigned char* priorityMap, int staticModels);

	using SV_EntityContactBounds_t = int (*)(const float* bounds, const void* entity);
	using G_RunFrame_t = void (*)(int levelTime);

	using Scr_AddString_t = void (*)(const char* value);
	using Scr_AddBool_t = void (*)(int value);
	using Scr_Notify_t = void (*)(void* entity, unsigned short stringId, unsigned int paramCount);

	using BG_GetWeaponDef_t = const char* (*)(int index);

	constexpr int weaponCompletePenetrateScale = 68;
	constexpr int weaponCompleteAdsViewKickCenterSpeed = 72;
	constexpr int weaponCompleteHipViewKickCenterSpeed = 76;
	constexpr int weaponCompleteAltWeaponIndex = 88;
	using CM_EntityString_t = const char* (*)();
	using G_Glass_GetPieceOrigin_t = void (*)(unsigned piece, float* out);
	using SL_GetString_t = unsigned (*)(const char* text);

	using StructuredDataSetNumber_t = int (*)(void* lookup, void* buffer,
											  unsigned char* modifiedFlags, int value);

	inline SV_AddTestClient_t SV_AddTestClient = nullptr;
	inline SV_ClientThink_t SV_ClientThink = nullptr;
	inline SV_GetPlayerstate_t SV_GetPlayerstateForClientNum = nullptr;
	inline SV_GetPersistentData_t SV_GetPersistentDataBuffer = nullptr;
	inline SV_GetPersistentData_t SV_GetPersistentDataFlags = nullptr;
	inline SV_Trace_t SV_Trace = nullptr;
	inline G_LocationalTracePassed_t G_LocationalTracePassed = nullptr;
	inline SV_EntityContactBounds_t SV_EntityContactBounds = nullptr;
	inline Scr_AddString_t Scr_AddString = nullptr;
	inline Scr_AddBool_t Scr_AddBool = nullptr;
	inline Scr_Notify_t Scr_Notify = nullptr;

	inline BG_GetWeaponDef_t BG_GetWeaponDef = nullptr;
	inline BG_GetWeaponCompleteDef_t BG_GetWeaponCompleteDef = nullptr;
	inline CM_EntityString_t CM_EntityString = nullptr;
	inline G_Glass_GetPieceOrigin_t G_Glass_GetPieceOrigin = nullptr;

	inline StructuredDataDefGetAsset_t StructuredDataDefGetAsset = nullptr;
	inline StructuredDataSetString_t StructuredDataSetString = nullptr;
	inline StructuredDataSetNumber_t StructuredDataSetInt = nullptr;
	inline StructuredDataSetNumber_t StructuredDataSetShort = nullptr;
	inline StructuredDataSetNumber_t StructuredDataSetBool = nullptr;

	using SL_ConvertToString_t = const char* (*)(unsigned short id);
	using GScr_IsItemUnlocked_t = int (*)(unsigned entref);

	inline StructuredDataInitLookup_t StructuredDataInitLookup = nullptr;
	inline StructuredDataLookupString_t StructuredDataLookupString = nullptr;
	inline StructuredDataLookupInt_t StructuredDataLookupInt = nullptr;
	inline SL_ConvertToString_t SL_ConvertToString = nullptr;
	inline GScr_IsItemUnlocked_t GScr_IsItemUnlocked = nullptr;

	constexpr std::uintptr_t GScr_IsItemUnlockedSlot = 0x1404248B8;
	constexpr std::uintptr_t GScr_IsItemUnlockedAddress = 0x1401A57C0;

	inline std::uintptr_t g_entities = 0;

	inline std::uintptr_t g_glassData = 0;

	inline bool TrySetDvarString(const char* name, const char* value)
	{
		if (!Dvar_FindVar || !Dvar_SetFromStringByName)
		{
			return false;
		}

		void* const dvar = Dvar_FindVar(name);
		if (!dvar)
		{
			return false;
		}

		unsigned* const flags = reinterpret_cast<unsigned*>(static_cast<char*>(dvar) + dvarFlags);
		*flags = *flags & ~dvarFlagCheat;

		Dvar_SetFromStringByName(name, value);

		return true;
	}

	inline bool TrySetDvarInt(const char* name, int value)
	{
		char text[16];
		std::snprintf(text, sizeof(text), "%d", value);

		return TrySetDvarString(name, text);
	}

	inline bool TryReadDvarInt(const char* name, int& out)
	{
		if (!Dvar_FindVar)
		{
			return false;
		}

		const void* const dvar = Dvar_FindVar(name);
		if (!dvar)
		{
			return false;
		}

		out = *reinterpret_cast<const int*>(static_cast<const char*>(dvar) + dvarCurrent);

		return true;
	}

	constexpr int weaponDefPenetrateType = 92;
	inline std::uintptr_t perk_bulletPenetrationMultiplier = 0;
	constexpr int dvarCurrentValue = 16;
	constexpr int psPerkArmorPiercing = 0x20;
	inline std::uintptr_t bullet_penetration_enabled = 0;
	inline std::uintptr_t penetrationDepthTable = 0;
	constexpr int penetrateTypeCount = 4;
	constexpr int surfaceTypeCount = 31;
	constexpr int surfaceTypeShift = 20;
	constexpr int surfaceTypeBits = 0x1F;
	constexpr int surfNoPenetrate = 0x100;
	inline std::uintptr_t cm = 0;
	inline std::uintptr_t g_worldDpvs = 0;
	inline std::uintptr_t g_worldDraw = 0;
	constexpr int maxBulletPenetrations = 5;
	constexpr float bulletAdvanceThroughSurface = 0.135f;
	constexpr float bulletAdvancePastExit = 0.01f;

	constexpr int weaponDefProjectileSpeed = 1376;
	constexpr int weaponDefAdsGunKickReducedKickPercent = 1560;
	constexpr int weaponDefAdsViewKickPitchMin = 1596;

	constexpr int weaponDefAdsViewKickYawMin = 1604;

	constexpr int weaponDefHipGunKickReducedKickPercent = 1628;
	constexpr int weaponDefHipViewKickPitchMin = 1664;

	constexpr int weaponDefHipViewKickYawMin = 1672;

	constexpr int weaponDefNeedsRechamber = 2150;
	constexpr int psWeaponRestrictKickTime = 500;

	constexpr int glassDamageDestroy = 100;
	constexpr int glassDamageDeleted = 0xFFFF;
	inline std::uintptr_t sv_mapname = 0;

	inline const char* BG_GetWeaponName(int index)
	{
		return *reinterpret_cast<const char* const*>(BG_GetWeaponCompleteDef(index));
	}

	constexpr std::uintptr_t G_RunFrame = 0x14019EA70;
	constexpr std::uintptr_t G_RunFrameJump = 0x14023D188;
	constexpr std::uintptr_t G_RunFrameCall = 0x14023D4F9;

	using Sys_Milliseconds_t = int (*)();

	inline Session_GetXuid_t Session_GetXuidEvenIfInactive = nullptr;
	inline Sys_Milliseconds_t Sys_Milliseconds = nullptr;

	inline Cmd_AddCommandInternal_t Cmd_AddCommandInternal = nullptr;

	inline SV_DropClient_t SV_DropClientFn = nullptr;

	void BindAddresses();
	void Print(const char* format, ...);
}
