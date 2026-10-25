#pragma once

namespace Components::Remix::Iw4
{
	struct CallSite
	{
		std::uintptr_t address;
		unsigned char bytes[5];
	};

	struct BytePatch
	{
		std::uintptr_t address;
		unsigned char original[8];
		unsigned char replacement[8];
		std::size_t size;
	};

	struct GameBuild
	{
		std::uintptr_t rChangeDepthRange;

		CallSite emissivePassCall;
		CallSite rbDrawSunPostEffectsCall;
		CallSite rbStandardDrawCommandsJump;
		CallSite bspSurfaceListCall;
		CallSite bspSurfaceListSecondPassCall;
		CallSite staticModelRigidCall;
		CallSite staticModelRigidSecondPassCall;
		CallSite staticModelDeformedCall;
		CallSite staticModelDeformedSecondPassCall;

		BytePatch rbShadowMapsBranch;
		BytePatch rbFloatZBranch;
		BytePatch rbDepthPrepassBranch;
		BytePatch rbSunSpriteCall;
		BytePatch rbPostEffectsCall;
		BytePatch skyFrustumCullBranch;

		CallSite R_ShutdownCalls[3];

		BytePatch effectSkipBranches[6];

		BytePatch R_ToggleSmpFrame_SkinCacheImm;
		BytePatch R_SetupDrawGroup_DistortionBranches[3];
		BytePatch R_SetupDrawGroup_ShadowMapBranches[2];

		BytePatch surfaceListCapacityPatches[3];
		BytePatch staticModelLightingPatch;
		BytePatch cellPlaneCountPatches[2];
		BytePatch staticModelCullDistPatch;
		BytePatch wholeMapPatches[2];
		CallSite R_AddWorldSurfacesDpvsCall;
		CallSite XModelGetLodForDistCall;
		std::uintptr_t s_warnCount;
		std::uintptr_t warnFrameCount;

		CallSite marksTessCalls[2];
		CallSite glassTessCalls[2];
		CallSite particleCloudTessCalls[2];
		CallSite FX_DrawSpriteElemsCall;
		CallSite FX_DrawElementSpriteCalls[2];
		std::uintptr_t R_AddCodeSurf;
		std::uintptr_t fxDrawElemBillboardSlot;
		std::uintptr_t FX_DrawElem_BillboardSprite;
		CallSite R_AddAllStaticModelSurfacesCameraCall;

		CallSite R_AddCode3DQuadSurfCalls[3];
		CallSite R_TessMarkSurfListCalls[2];
		std::uintptr_t motionTrackerBackgroundMaterial;
		std::uintptr_t motionTrackerSweepMaterial;
		std::uintptr_t motionTrackerEnemyPingMaterial;
		std::uintptr_t motionTrackerFriendlyPingMaterial;

		std::uintptr_t g_worldDraw;
		std::uintptr_t g_worldDpvs;
		std::uintptr_t gfxWorld;
		std::uintptr_t backEndData;
		std::uintptr_t r_diffuseColorScaleDvar;
		std::uintptr_t sceneAddedLightCount;
		std::uintptr_t sceneAddedLight;
		std::uintptr_t g_postLightCount;
		std::uintptr_t g_postLightInfo;
		std::uintptr_t r_smp_backendDvar;
		std::uintptr_t r_fogDvar;
		std::uintptr_t fogSettingsCurrent;
		std::uintptr_t fogIndex;
		std::uintptr_t tessTableBModelSlot;
		std::uintptr_t R_TessBModel;
		std::uintptr_t tessTableRigidSlot;
		std::uintptr_t R_TessXModelRigidDrawSurfList;
		std::uintptr_t tessTableRigidSkinnedSlot;
		std::uintptr_t R_TessXModelRigidSkinnedDrawSurfList;
		std::uintptr_t tessTableSkinnedSlot;
		std::uintptr_t R_TessXModelSkinnedDrawSurfList;
		std::uintptr_t materialPool;

		BytePatch entityFrustumPatches[4];
		BytePatch lodFovPatches[2];
		CallSite localBodyThirdPersonCall;
		std::uintptr_t killCamState;
		CallSite addPlayerDObjCall;
		std::uintptr_t cgOtherFlags;
		std::uintptr_t predictedClientNum;
		std::uintptr_t renderingThirdPerson;
		std::uintptr_t cgArray;
		std::uintptr_t BG_IsThirdPersonMode;
		std::uintptr_t R_SetFilmConstants;
		CallSite SND_PlaySoundAlias_InternalCalls[12];
		std::uintptr_t visionNameNaked;
		BytePatch gpuSyncPollFlushPatches[4];
		CallSite renderCommandFrameCalls[3];
		CallSite renderThreadUpdateScreenCall;
	};

	inline constexpr GameBuild multiplayer{
		.rChangeDepthRange = 0x1400478B0,
		.emissivePassCall = { 0x1400738FF, { 0xE8, 0xBC, 0x70, 0x00, 0x00 } },
		.rbDrawSunPostEffectsCall = { 0x140073919, { 0xE8, 0xB2, 0x2B, 0xFF, 0xFF } },
		.rbStandardDrawCommandsJump = { 0x140073997, { 0xE9, 0xE4, 0xFB, 0xFF, 0xFF } },
		.bspSurfaceListCall = { 0x140050936, { 0xE8, 0x35, 0x66, 0x02, 0x00 } },
		.bspSurfaceListSecondPassCall = { 0x140050962, { 0xE8, 0x09, 0x66, 0x02, 0x00 } },
		.staticModelRigidCall = { 0x140050BA5, { 0xE8, 0xB6, 0x59, 0x02, 0x00 } },
		.staticModelRigidSecondPassCall = { 0x140050BD2, { 0xE8, 0x89, 0x59, 0x02, 0x00 } },
		.staticModelDeformedCall = { 0x140050CD5, { 0xE8, 0x76, 0x5C, 0x02, 0x00 } },
		.staticModelDeformedSecondPassCall = { 0x140050D02, { 0xE8, 0x49, 0x5C, 0x02, 0x00 } },
		.rbShadowMapsBranch = { 0x1400735AE, { 0x0F, 0x85, 0xC4, 0x00, 0x00, 0x00 }, { 0x90, 0xE9, 0xC4, 0x00, 0x00, 0x00 }, 6 },
		.rbFloatZBranch = { 0x140073698, { 0x0F, 0x84, 0x89, 0x00, 0x00, 0x00 }, { 0x90, 0xE9, 0x89, 0x00, 0x00, 0x00 }, 6 },
		.rbDepthPrepassBranch = { 0x1400737D4, { 0x74, 0x0E }, { 0xEB, 0x0E }, 2 },
		.rbSunSpriteCall = { 0x14007381E, { 0xE8, 0xED, 0x2B, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 }, 5 },
		.rbPostEffectsCall = { 0x140050103, { 0xE8, 0x48, 0x33, 0x01, 0x00 }, { 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 5 },
		.skyFrustumCullBranch = { 0x1400641EE, { 0x0F, 0x84, 0x9E, 0x00, 0x00, 0x00 }, { 0xE9, 0x9F, 0x00, 0x00, 0x00, 0x90 }, 6 },
		.R_ShutdownCalls = {
			{ 0x1400FD6A2, { 0xE8, 0x69, 0x55, 0xF3, 0xFF } },
			{ 0x1400FD8B6, { 0xE8, 0x55, 0x53, 0xF3, 0xFF } },
			{ 0x1400FDCCC, { 0xE8, 0x3F, 0x4F, 0xF3, 0xFF } },
		},
		.effectSkipBranches = {
			{ 0x140051170, { 0x74, 0x72 }, { 0xEB, 0x72 }, 2 },
			{ 0x1400512A0, { 0x75, 0x2D }, { 0x90, 0x90 }, 2 },
			{ 0x1400513F0, { 0x75, 0x2D }, { 0x90, 0x90 }, 2 },
			{ 0x14005153F, { 0x74, 0x72 }, { 0xEB, 0x72 }, 2 },
			{ 0x140051689, { 0x74, 0x72 }, { 0xEB, 0x72 }, 2 },
			{ 0x1400B367A, { 0xE8, 0x71 }, { 0xEB, 0x03 }, 2 },
		},
		.R_ToggleSmpFrame_SkinCacheImm = { 0x14001BC77, { 0x01, 0x12, 0xEF, 0x08, 0x01 }, { 0x01, 0x12, 0xEF, 0x08, 0x00 }, 5 },
		.R_SetupDrawGroup_DistortionBranches = {
			{ 0x140077BC2, { 0x0F, 0x84, 0x87, 0x00, 0x00, 0x00 }, { 0x90, 0xE9, 0x87, 0x00, 0x00, 0x00 }, 6 },
			{ 0x140077CB7, { 0x74, 0x96 }, { 0xEB, 0x96 }, 2 },
			{ 0x140077D03, { 0x0F, 0x84, 0x46, 0xFF, 0xFF, 0xFF }, { 0x90, 0xE9, 0x46, 0xFF, 0xFF, 0xFF }, 6 },
		},
		.R_SetupDrawGroup_ShadowMapBranches = {
			{ 0x140077BE1, { 0x74, 0x6C }, { 0x90, 0x90 }, 2 },
			{ 0x140077D24, { 0x0F, 0x84, 0x25, 0xFF, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }, 6 },
		},
		.surfaceListCapacityPatches = {
			{ 0x140058A5D, { 0x00, 0x30, 0x00, 0x00 }, { 0x00, 0x50, 0x00, 0x00 }, 4 },
			{ 0x140058A8F, { 0x00, 0x10, 0x00, 0x00 }, { 0x00, 0x2C, 0x00, 0x00 }, 4 },
			{ 0x140022B96, { 0x00, 0x04, 0x00, 0x00 }, { 0x00, 0x08, 0x00, 0x00 }, 4 },
		},
		.staticModelLightingPatch = { 0x140060501, { 0x0F, 0x85, 0x3B, 0x02, 0x00, 0x00 }, { 0x90, 0xE9, 0x3B, 0x02, 0x00, 0x00 }, 6 },
		.cellPlaneCountPatches = {
			{ 0x14002BE80, { 0x45, 0x8B, 0xCE }, { 0x45, 0x31, 0xC9 }, 3 },
			{ 0x14002BD41, { 0x45, 0x8B, 0xCF }, { 0x45, 0x31, 0xC9 }, 3 },
		},
		.staticModelCullDistPatch = { 0x140060431, { 0x72, 0x09 }, { 0xEB, 0x09 }, 2 },
		.wholeMapPatches = {
			{ 0x140021CEF, { 0x0F, 0x88, 0x80, 0x00, 0x00, 0x00 }, { 0x90, 0xE9, 0x80, 0x00, 0x00, 0x00 }, 6 },
			{ 0x140021DA0, { 0x46, 0x8B, 0x8C, 0x37, 0x10, 0x8A, 0x3C, 0x09 }, { 0x45, 0x31, 0xC9, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 8 },
		},
		.R_AddWorldSurfacesDpvsCall = { 0x140021D63, { 0xE8, 0x98, 0xA7, 0x00, 0x00 } },
		.XModelGetLodForDistCall = { 0x1400604E8, { 0xE8, 0x13, 0xBC, 0x25, 0x00 } },
		.s_warnCount = 0x149450C80,
		.warnFrameCount = 0x148CCA31C,
		.marksTessCalls = {
			{ 0x14005131C, { 0xE8, 0x3F, 0x32, 0x02, 0x00 } },
			{ 0x140051348, { 0xE8, 0x13, 0x32, 0x02, 0x00 } },
		},
		.glassTessCalls = {
			{ 0x14005146C, { 0xE8, 0x5F, 0x2D, 0x02, 0x00 } },
			{ 0x140051498, { 0xE8, 0x33, 0x2D, 0x02, 0x00 } },
		},
		.particleCloudTessCalls = {
			{ 0x140051587, { 0xE8, 0xC4, 0x37, 0x02, 0x00 } },
			{ 0x1400515AC, { 0xE8, 0x9F, 0x37, 0x02, 0x00 } },
		},
		.FX_DrawSpriteElemsCall = { 0x14013D0DA, { 0xE8, 0xA1, 0xDE, 0xFF, 0xFF } },
		.FX_DrawElementSpriteCalls = {
			{ 0x14013B0AE, { 0xE8, 0x2D, 0xF7, 0xFF, 0xFF } },
			{ 0x14013B128, { 0xE8, 0xB3, 0xF6, 0xFF, 0xFF } },
		},
		.R_AddCodeSurf = 0x14001E1F0,
		.fxDrawElemBillboardSlot = 0x14037D930,
		.FX_DrawElem_BillboardSprite = 0x140139F40,
		.R_AddAllStaticModelSurfacesCameraCall = { 0x140022BD0, { 0xE8, 0x0B, 0xD1, 0x03, 0x00 } },
		.R_AddCode3DQuadSurfCalls = {
			{ 0x1400B4A3B, { 0xE8, 0x80, 0x98, 0xF6, 0xFF } },
			{ 0x1400B51CF, { 0xE8, 0xEC, 0x90, 0xF6, 0xFF } },
			{ 0x1400B3D37, { 0xE8, 0x84, 0xA5, 0xF6, 0xFF } },
		},
		.R_TessMarkSurfListCalls = {
			{ 0x1400511B8, { 0xE8, 0xE3, 0x2A, 0x02, 0x00 } },
			{ 0x1400511DD, { 0xE8, 0xBE, 0x2A, 0x02, 0x00 } },
		},
		.motionTrackerBackgroundMaterial = 0x140462678,
		.motionTrackerSweepMaterial = 0x140462680,
		.motionTrackerEnemyPingMaterial = 0x140462688,
		.motionTrackerFriendlyPingMaterial = 0x140462690,
		.g_worldDraw = 0x148CCA1A8,
		.g_worldDpvs = 0x148CCC9B0,
		.gfxWorld = 0x148CCC7A0,
		.backEndData = 0x148F0F7D8,
		.r_diffuseColorScaleDvar = 0x148CC6B88,
		.sceneAddedLightCount = 0x148C89370,
		.sceneAddedLight = 0x148C89378,
		.g_postLightCount = 0x14165A380,
		.g_postLightInfo = 0x141659480,
		.r_smp_backendDvar = 0x148C285E8,
		.r_fogDvar = 0x148CC6A08,
		.fogSettingsCurrent = 0x148CCA394,
		.fogIndex = 0x148CCA43C,
		.tessTableBModelSlot = 0x1403CEF90,
		.R_TessBModel = 0x140077690,
		.tessTableRigidSlot = 0x1403CEF98,
		.R_TessXModelRigidDrawSurfList = 0x140075960,
		.tessTableRigidSkinnedSlot = 0x1403CEFA0,
		.R_TessXModelRigidSkinnedDrawSurfList = 0x140075C90,
		.tessTableSkinnedSlot = 0x1403CEFA8,
		.R_TessXModelSkinnedDrawSurfList = 0x140075240,
		.materialPool = 0x140FC6E00,
		.entityFrustumPatches = {
			{ 0x14006BF63, { 0x44, 0x0F, 0xB6, 0x72, 0x0C }, { 0x45, 0x31, 0xF6, 0x66, 0x90 }, 5 },
			{ 0x14002AA9B, { 0x45, 0x8B, 0x8C, 0x2A, 0xC0, 0x81, 0x00, 0x00 }, { 0x45, 0x31, 0xC9, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 8 },
			{ 0x14002AD26, { 0x45, 0x8B, 0x8C, 0x2A, 0xC0, 0x81, 0x00, 0x00 }, { 0x45, 0x31, 0xC9, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 8 },
			{ 0x14002AEE2, { 0x45, 0x8B, 0x8C, 0x2A, 0xC0, 0x81, 0x00, 0x00 }, { 0x45, 0x31, 0xC9, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 8 },
		},
		.lodFovPatches = {
			{ 0x14002461B, { 0xF3, 0x0F, 0x59, 0x1D, 0xB5, 0x03, 0x3C, 0x00 }, { 0xF3, 0x0F, 0x10, 0x1D, 0x11, 0x02, 0x3C, 0x00 }, 8 },
			{ 0x140024F2E, { 0xF3, 0x0F, 0x59, 0x1D, 0xA2, 0xFA, 0x3B, 0x00 }, { 0xF3, 0x0F, 0x10, 0x1D, 0xFE, 0xF8, 0x3B, 0x00 }, 8 },
		},
		.localBodyThirdPersonCall = { 0x1400DFE30, { 0xE8, 0x7B, 0xA8, 0xFA, 0xFF } },
		.killCamState = 0x1404ED270,
		.addPlayerDObjCall = { 0x1400DFFAE, { 0xE8, 0xDD, 0xF2, 0xF3, 0xFF } },
		.cgOtherFlags = 0x1404769B0,
		.predictedClientNum = 0x140476AA4,
		.renderingThirdPerson = 0x1404E1130,
		.cgArray = 0x1404769A0,
		.BG_IsThirdPersonMode = 0x14008A6B0,
		.R_SetFilmConstants = 0x140020980,
		.SND_PlaySoundAlias_InternalCalls = {
			{ 0x140246AF5, { 0xE8, 0xD6, 0x05, 0x00, 0x00 } },
			{ 0x140246BBE, { 0xE8, 0x0D, 0x05, 0x00, 0x00 } },
			{ 0x140246C86, { 0xE8, 0x45, 0x04, 0x00, 0x00 } },
			{ 0x140246D16, { 0xE8, 0xB5, 0x03, 0x00, 0x00 } },
			{ 0x140246E85, { 0xE8, 0x46, 0x02, 0x00, 0x00 } },
			{ 0x140246F71, { 0xE8, 0x5A, 0x01, 0x00, 0x00 } },
			{ 0x140246FF6, { 0xE8, 0xD5, 0x00, 0x00, 0x00 } },
			{ 0x140247056, { 0xE8, 0x75, 0x00, 0x00, 0x00 } },
			{ 0x1402470BA, { 0xE8, 0x11, 0x00, 0x00, 0x00 } },
			{ 0x140249F95, { 0xE8, 0x36, 0xD1, 0xFF, 0xFF } },
			{ 0x14024A14A, { 0xE8, 0x81, 0xCF, 0xFF, 0xFF } },
			{ 0x14024AE40, { 0xE8, 0x8B, 0xC2, 0xFF, 0xFF } },
		},
		.visionNameNaked = 0x140586E84,
		.gpuSyncPollFlushPatches = {
			{ 0x140031C8A, { 0x41, 0xB9, 0x01, 0x00, 0x00, 0x00 }, { 0x41, 0xB9, 0x00, 0x00, 0x00, 0x00 }, 6 },
			{ 0x140031C90, { 0x45, 0x8D, 0x41, 0x03 }, { 0x45, 0x8D, 0x41, 0x04 }, 4 },
			{ 0x14001B69A, { 0x41, 0xB9, 0x01, 0x00, 0x00, 0x00 }, { 0x41, 0xB9, 0x00, 0x00, 0x00, 0x00 }, 6 },
			{ 0x14001B6A0, { 0x45, 0x8D, 0x41, 0x03 }, { 0x45, 0x8D, 0x41, 0x04 }, 4 },
		},
		.renderCommandFrameCalls = {
			{ 0x14005050A, { 0xE8, 0x31, 0xFE, 0xFF, 0xFF } },
			{ 0x140050561, { 0xE8, 0xDA, 0xFD, 0xFF, 0xFF } },
			{ 0x1400505C7, { 0xE8, 0x74, 0xFD, 0xFF, 0xFF } },
		},
		.renderThreadUpdateScreenCall = { 0x14005057E, { 0xE8, 0xAD, 0x16, 0x0B, 0x00 } },
	};

	inline constexpr std::uintptr_t dvarValueOffset = 16;
	inline constexpr std::size_t viewParmsProjection = 0x40;
	inline constexpr std::uintptr_t gfxImageTexture = 0x00;
	inline constexpr std::uintptr_t gfxImageName = 0x20;
	inline constexpr std::uintptr_t assetPoolEntries = 0x08;
	inline constexpr std::size_t materialPoolCount = 4096;
	inline constexpr std::size_t materialStride = 0x78;
	inline constexpr std::uintptr_t materialSortKey = 0x09;
	inline constexpr std::uintptr_t materialStateBitsEntry = 0x20;
	inline constexpr std::uintptr_t materialTextureCount = 0x50;
	inline constexpr std::uintptr_t materialConstantCount = 0x51;
	inline constexpr std::uintptr_t materialStateBitsCount = 0x52;
	inline constexpr std::uintptr_t materialCameraRegion = 0x54;
	inline constexpr std::uintptr_t materialTechniqueSet = 0x58;
	inline constexpr std::uintptr_t materialTextureTable = 0x60;
	inline constexpr std::uintptr_t materialConstantTable = 0x68;
	inline constexpr std::uintptr_t materialStateBitsTable = 0x70;
	inline constexpr std::size_t stateBitsStride = 8;
	inline constexpr std::uint32_t stateBitsBlendMask = 0x7FF;
	inline constexpr std::uint32_t doubleMultiplyBlend = 0x139;
	inline constexpr std::size_t textureDefStride = 16;
	inline constexpr std::uintptr_t textureDefNameHash = 0x00;
	inline constexpr std::uintptr_t textureDefSemantic = 0x07;
	inline constexpr std::uintptr_t textureDefImage = 0x08;
	inline constexpr std::uintptr_t waterImage = 0x58;
	inline constexpr std::uint32_t waterColorNameHash = 0xB82A51E8;
	inline constexpr std::uintptr_t gfxImageMapType = 0x08;
	inline constexpr std::uint8_t gfxImageMapType2D = 3;
	inline constexpr std::uint8_t textureSemanticWater = 11;
	inline constexpr std::uint32_t colorMapNameHash = 0xA0AB1041u;
	inline constexpr std::uint32_t uvAnimParmsNameHash = 0x70EBAF95u;
	inline constexpr std::uint32_t detailMapNameHash = 0xEB529B4Du;
	inline constexpr std::uint32_t normalMapNameHash = 0x59D30D0Fu;
	inline constexpr std::uint32_t specularMapNameHash = 0x34ECCCB3u;
	inline constexpr std::uint32_t envMapParmsNameHash = 0x3D9994DCu;
	inline constexpr std::uint8_t textureSemanticColour = 2;
	inline constexpr std::uint8_t textureSemanticNormal = 5;
	inline constexpr std::uint8_t textureSemanticSpecular = 8;
	inline constexpr std::uintptr_t materialSurfaceTypeBits = 0x18;
	inline constexpr std::uint32_t detailScaleNameHash = 0x08D36A09u;
	inline constexpr std::uint8_t textureSemanticDetail = 3;
	inline constexpr std::uintptr_t gfxImageUseSrgbReads = 0x0B;
	inline constexpr std::size_t constantDefStride = 32;
	inline constexpr std::uintptr_t constantDefNameHash = 0x00;
	inline constexpr std::uintptr_t constantDefLiteral = 0x10;
	inline constexpr float gameTimeWrap = 43200.0f;
	inline constexpr std::uintptr_t techniqueUnlit = 4;
	inline constexpr std::uintptr_t techniqueEmissive = 5;
	inline constexpr std::uintptr_t techniqueLit = 9;
	inline constexpr std::uintptr_t viewParmsView = 0x000;
	inline constexpr std::uintptr_t viewParmsOrigin = 0x100;
	inline constexpr std::uintptr_t viewInfoViewType = 0x1380;
	inline constexpr std::uintptr_t viewInfoShadowableLightCount = 0x1B8;
	inline constexpr int viewTypeScene = 2;
	inline constexpr std::uintptr_t worldDrawVertexCount = 0x50;
	inline constexpr std::uintptr_t worldDrawVertices = 0x58;
	inline constexpr std::uintptr_t worldDrawIndexCount = 0x80;
	inline constexpr std::uintptr_t worldDrawIndices = 0x88;
	inline constexpr std::uintptr_t worldDpvsSurfaces = 0x78;
	inline constexpr std::uintptr_t worldDpvsSortedSurfIndex = 0x68;
	inline constexpr std::uintptr_t worldDpvsLitOpaqueBegin = 0x0C;
	inline constexpr std::uintptr_t worldDpvsLitOpaqueEnd = 0x10;
	inline constexpr std::uintptr_t worldDpvsLitTransBegin = 0x14;
	inline constexpr std::uintptr_t worldDpvsLitTransEnd = 0x18;
	inline constexpr std::uintptr_t worldDpvsEmissiveBegin = 0x24;
	inline constexpr std::uintptr_t worldDpvsEmissiveEnd = 0x28;
	inline constexpr std::uintptr_t worldName = 0x0;
	inline constexpr std::uintptr_t worldCellCount = 0x40;
	inline constexpr std::uintptr_t worldCellHasSunLitSurfsBits = 0x258;
	inline constexpr std::uintptr_t worldCellTreeCounts = 0x60;
	inline constexpr std::uintptr_t worldCellTrees = 0x68;
	inline constexpr std::uintptr_t worldCells = 0x70;
	inline constexpr std::size_t cellStride = 0x38;
	inline constexpr std::uintptr_t cellMidPoint = 0x00;
	inline constexpr std::uintptr_t cellHalfSize = 0x0C;
	inline constexpr std::uintptr_t cellPortalCount = 0x18;
	inline constexpr std::uintptr_t cellPortals = 0x20;
	inline constexpr std::size_t portalStride = 0x50;
	inline constexpr std::uintptr_t portalCellIndex = 0x30;
	inline constexpr std::size_t aabbTreeStride = 0x38;
	inline constexpr std::uintptr_t aabbTreeSurfaceCount = 0x1A;
	inline constexpr std::uintptr_t aabbTreeStartSurfIndex = 0x1C;
	inline constexpr std::uintptr_t aabbTreeSModelCount = 0x22;
	inline constexpr std::uintptr_t aabbTreeSModelIndexes = 0x28;
	inline constexpr std::uintptr_t cellForPointFunction = 0x140027780;
	inline constexpr std::uintptr_t worldSkyCount = 0x1C;
	inline constexpr std::uintptr_t worldLastSunPrimaryLightIndex = 0x28;
	inline constexpr std::uintptr_t worldPrimaryLightCount = 0x2C;
	inline constexpr std::uintptr_t backEndSunDirection = 0x41A48;
	inline constexpr std::uintptr_t backEndPrimaryLights = 0x104F80;
	inline constexpr std::size_t gfxLightStride = 72;
	inline constexpr std::uint32_t maxSceneAddedLights = 32;
	inline constexpr std::size_t postLightStride = 40;
	inline constexpr std::uint32_t maxPostLights = 96;
	inline constexpr std::uintptr_t postLightBegin = 0x00;
	inline constexpr std::uintptr_t postLightEnd = 0x0C;
	inline constexpr std::uintptr_t postLightRadius = 0x18;
	inline constexpr std::uintptr_t centClientNum = 0xE8;
	inline constexpr std::uint32_t cgFollowOrPlayerFlags = 0x1800;
	inline constexpr std::uint32_t cgFollowFlag = 0x800;
	inline constexpr std::uintptr_t gfxEntityMaterialTime = 0x04;
	inline constexpr int drawSurfHasEntityBit = 24;
	inline constexpr std::uintptr_t viewInfoBlurRadius = 0x1BC;
	inline constexpr std::uintptr_t viewInfoDepthOfField = 0x200;
	inline constexpr std::size_t viewInfoDepthOfFieldBytes = 0x20;
	inline constexpr std::uintptr_t viewInfoGlowEnabled = 0x25C;
	inline constexpr std::uintptr_t viewInfoWaterSheeting = 0x300;
	inline constexpr std::uintptr_t viewInfoFilmBrightness = 0x224;
	inline constexpr std::uintptr_t viewInfoFilmContrast = 0x228;
	inline constexpr std::uintptr_t viewInfoFilmDesaturation = 0x22C;
	inline constexpr std::uintptr_t viewInfoFilmDesaturationDark = 0x230;
	inline constexpr std::uintptr_t viewInfoFilmTints = 0x238;
	inline constexpr int filmTintFloats = 9;
	inline constexpr std::size_t codeSurfStride = 24;
	inline constexpr std::uintptr_t codeSurfTriangleCount = 0x04;
	inline constexpr std::uintptr_t codeSurfIndices = 0x08;
	inline constexpr std::uintptr_t codeSurfArgCount = 0x12;
	inline constexpr std::uintptr_t gfxLightType = 0x00;
	inline constexpr std::uintptr_t gfxLightColor = 0x04;
	inline constexpr std::uintptr_t gfxLightDir = 0x10;
	inline constexpr std::uintptr_t gfxLightOrigin = 0x1C;
	inline constexpr std::uintptr_t gfxLightRadius = 0x28;
	inline constexpr std::uintptr_t gfxLightCosHalfFovOuter = 0x2C;
	inline constexpr std::uintptr_t gfxLightCosHalfFovInner = 0x30;
	inline constexpr std::uint8_t gfxLightTypeSpot = 2;
	inline constexpr std::uint8_t gfxLightTypeOmni = 3;
	inline constexpr std::uintptr_t worldSkies = 0x20;
	inline constexpr std::uintptr_t worldSunSpriteMaterial = 0x198 + 0x8;
	inline constexpr std::uintptr_t worldSunFlareMaterial = 0x198 + 0x10;
	inline constexpr std::size_t gfxSkyStride = 32;
	inline constexpr std::uintptr_t gfxSkySurfaceCount = 0x00;
	inline constexpr std::uintptr_t gfxSkyStartSurfaces = 0x08;
	inline constexpr std::size_t worldVertexStride = 44;
	inline constexpr std::uintptr_t worldVertexColor = 0x10;
	inline constexpr std::uintptr_t worldVertexTexCoord = 0x14;
	inline constexpr std::uintptr_t worldVertexNormal = 0x24;
	inline constexpr std::size_t gfxSurfaceStride = 32;
	inline constexpr std::uintptr_t gfxSurfaceFirstVertex = 0x04;
	inline constexpr std::uintptr_t gfxSurfaceVertexCount = 0x08;
	inline constexpr std::uintptr_t gfxSurfaceTriCount = 0x0A;
	inline constexpr std::uintptr_t gfxSurfaceBaseIndex = 0x0C;
	inline constexpr std::uintptr_t gfxSurfaceMaterial = 0x10;
	inline constexpr std::uintptr_t drawListArgsSource = 0x00;
	inline constexpr std::uintptr_t drawListArgsState = 0x08;
	inline constexpr std::uintptr_t drawListArgsInfo = 0x10;
	inline constexpr std::uintptr_t sourceSceneDepthRange = 0x16DC;
	inline constexpr std::uintptr_t sourceSkinnedPlacement = 0x16BC;
	inline constexpr std::uintptr_t sourceFloatTime = 0x16EC;
	inline constexpr std::uintptr_t stateMaterial = 0x120;
	inline constexpr std::uintptr_t statePassIndex = 0x140;
	inline constexpr std::uintptr_t sourceBackEndData = 0x13B8;
	inline constexpr std::uintptr_t backEndParticleCloudBase = 0x383E;
	inline constexpr std::size_t particleCloudStride = 64;
	inline constexpr std::uintptr_t particleCloudColor = 0x2C;
	inline constexpr std::uintptr_t particleCloudQuat = 0x00;
	inline constexpr std::uintptr_t particleCloudOrigin = 0x10;
	inline constexpr std::uintptr_t particleCloudScale = 0x1C;
	inline constexpr std::uintptr_t particleCloudTail = 0x20;
	inline constexpr std::uintptr_t particleCloudSize = 0x30;
	inline constexpr std::uintptr_t particleCloudLength = 0x34;
	inline constexpr std::uintptr_t particleCloudFlags = 0x38;
	inline constexpr std::uint8_t particleCloudCountShiftMask = 3;
	inline constexpr std::uint8_t particleCloudSparkFlag = 4;
	inline constexpr std::size_t particleCloudEntryBytes = 4;
	inline constexpr std::uintptr_t worldOutdoorLookupMatrix = 0x208;
	inline constexpr std::uintptr_t worldOutdoorImage = 0x248;
	inline constexpr std::uintptr_t fogColor = 0x08;
	inline constexpr std::uintptr_t fogStart = 0x0C;
	inline constexpr std::uintptr_t fogDensity = 0x10;
	inline constexpr std::uintptr_t fogMaxOpacity = 0x14;
	inline constexpr std::uintptr_t fogSunEnabled = 0x18;
	inline constexpr std::uintptr_t fogSunBeginFade = 0x2C;
	inline constexpr std::uintptr_t fogSunEndFade = 0x30;
	inline constexpr std::uintptr_t backEndBModelSurfaces = 0x42980;
	inline constexpr std::uintptr_t bmodelSurfacePlacement = 0x00;
	inline constexpr std::uintptr_t bmodelSurfaceSurface = 0x08;
	inline constexpr std::uintptr_t gfxPlacementOrigin = 0x10;
	inline constexpr std::uint64_t drawSurfObjectMask = 0xFFFF;
	inline constexpr std::uint64_t drawSurfGroupMask = 0xFFFFFFFFC1000000ull;
	inline constexpr std::uint64_t drawSurfUsesGfxEntity = 0x1000000ull;

	inline constexpr std::uint64_t drawSurfGroupMaskWide = 0xFFFFFFFFF0400000ull;
	inline constexpr std::uint64_t drawSurfUsesGfxEntityWide = 0x400000ull;
	inline constexpr int drawSurfHasEntityBitWide = 22;
	inline constexpr unsigned smodelListCountShiftWide = 14;
	inline constexpr std::uintptr_t drawListInfoCameraView = 0x20;
	inline constexpr std::uintptr_t modelSurfacePartCount = 0x00;
	inline constexpr std::uintptr_t modelSurfaceGfxEntity = 0x12;
	inline constexpr std::uintptr_t modelSurfaceXSurface = 0x20;
	inline constexpr std::uintptr_t modelSurfaceScale = 0x28;
	inline constexpr std::uintptr_t modelSurfaceSkinnedCachedOffset = 0x00;
	inline constexpr std::uintptr_t modelSurfaceSkinnedVertices = 0x28;
	inline constexpr std::uintptr_t modelSurfaceParts = 0x2C;
	inline constexpr std::size_t modelPartStride = 32;
	inline constexpr std::uintptr_t modelPartTriOffset = 0x1C;
	inline constexpr std::uintptr_t modelPartTriCount = 0x1E;
	inline constexpr std::uintptr_t backEndGfxEntities = 0xDF380;
	inline constexpr std::size_t gfxEntityStride = 8;
	inline constexpr std::uint32_t gfxEntityDepthHack = 2;
	inline constexpr std::uintptr_t worldSModelDrawInsts = 0x328;
	inline constexpr std::uintptr_t worldSModelCount = 0x2A0;
	inline constexpr std::uintptr_t worldSModelVisibility = 0x2D8;
	inline constexpr std::uintptr_t smodelDrawInstOrigin = 0x00;
	inline constexpr std::uintptr_t smodelDrawInstModel = 0x38;
	inline constexpr std::uintptr_t xModelMaterialHandles = 0x58;
	inline constexpr std::uintptr_t xModelName = 0x0;
	inline constexpr std::uintptr_t soundAliasName = 0x0;
	inline constexpr std::size_t visionNameBytes = 64;
	inline constexpr std::uintptr_t xModelLodInfo = 0x60;
	inline constexpr std::size_t xModelLodInfoStride = 56;
	inline constexpr std::uintptr_t xModelFirstLod = 0x140;
	inline constexpr std::uintptr_t lodInfoSurfaceCount = 0x04;
	inline constexpr std::uintptr_t lodInfoSurfaceIndex = 0x06;
	inline constexpr std::uintptr_t lodInfoSurfaces = 0x28;
	inline constexpr std::uint8_t cameraRegionNone = 4;
	inline constexpr std::size_t smodelDrawInstStride = 88;
	inline constexpr std::uintptr_t smodelDrawInstAxis = 0x0C;
	inline constexpr std::uintptr_t smodelDrawInstScale = 0x30;
	inline constexpr std::uintptr_t smodelRecordSurfaces = 0x00;
	inline constexpr std::uintptr_t smodelRecordInstanceCount = 0x08;
	inline constexpr std::uintptr_t smodelRecordInstances = 0x0A;
	inline constexpr unsigned smodelListCountShift = 12;
	inline constexpr std::size_t xSurfaceStride = 88;
	inline constexpr std::uintptr_t xSurfaceVertCount = 0x02;
	inline constexpr std::uintptr_t xSurfaceTriCount = 0x04;
	inline constexpr std::uintptr_t xSurfaceTriIndices = 0x10;
	inline constexpr std::uintptr_t xSurfaceVerts0 = 0x28;
	inline constexpr std::size_t packedVertexStride = 32;
	inline constexpr std::uintptr_t packedVertexColor = 0x10;
	inline constexpr std::uintptr_t packedVertexTexCoord = 0x14;
	inline constexpr std::uintptr_t packedVertexNormal = 0x18;
	inline constexpr std::uintptr_t packedVertexTangent = 0x1C;
	inline constexpr std::uint16_t bspListGroupStart = 0x8000;
	inline constexpr std::uint16_t bspListRunMask = 0x7FFF;
	inline constexpr int TECHNIQUE_BUILD_SHADOWMAP_COLOR = 3;
	inline constexpr int TECHNIQUE_LIGHT_SPOT = 0x25;
	inline constexpr int TECHNIQUE_LIGHT_SPOT_SHADOW = 0x27;
}
