#include "STDInclude.hpp"

#include "Scene.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Iw4.hpp"
#include "Api.hpp"
#include "FixedFunction.hpp"
#include "Culling.hpp"
#include "DebugDraw.hpp"
#include "Fog.hpp"
#include "Sky.hpp"
#include "Look.hpp"
#include "Settings.hpp"
#include "Lights.hpp"
#include "MapSettings.hpp"
#include "Markers.hpp"
#include "MotionTracker.hpp"
#include "Options.hpp"
#include "Outdoor.hpp"
#include "PatchCalls.hpp"
#include "PbrMaterials.hpp"
#include "Profiler.hpp"
#include "Read.hpp"
#include "TextureTags.hpp"
#include "Triggers.hpp"
#include "Weather.hpp"

namespace Components::Remix
{
	using StandardDrawCommands = void(__fastcall*)(void*);
	using RendererShutdown = void(__fastcall*)(int);
	using EmissivePass = void(__fastcall*)(void*);
	using TessDrawList = void(__fastcall*)(void*, void**);

	static StandardDrawCommands originalStandardDraw = nullptr;
	static RendererShutdown originalShutdown = nullptr;
	static EmissivePass originalEmissivePass = nullptr;
	static TessDrawList originalMarksTess = nullptr;
	static TessDrawList originalGlassTess = nullptr;
	static TessDrawList originalParticleCloudTess = nullptr;
	static bool isWeatherEnabled = true;
	static bool isOffscreenWorldEnabled = true;

	static bool hasAttempted = false;
	static bool isInstalled = false;
	static bool didFault = false;

	static IDirect3DDevice9* sceneDevice = nullptr;
	static IDirect3DStateBlock9* injectionState = nullptr;

	struct InjectionVertex
	{
		float position[3];
		D3DCOLOR colour;
	};

	static const D3DMATRIX orthographic = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	static RemixStats stats;
	static float cameraOrigin[3] = {};
	static bool hasCameraOrigin = false;
	static D3DMATRIX sceneView{};
	static D3DMATRIX sceneProjection{};
	static bool hasSceneMatrices = false;
	static bool isEffectHidden[effectKindCount] = {};

	static const D3DMATRIX identity = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	static bool TryReadSceneView(const void* viewInfo, D3DMATRIX& view, D3DMATRIX& projection)
	{
		__try
		{
			const auto* bytes = static_cast<const unsigned char*>(viewInfo);

			int viewType = 0;
			std::memcpy(&viewType, bytes + Iw4::viewInfoViewType, sizeof(viewType));

			if (viewType != Iw4::viewTypeScene)
			{
				return false;
			}

			D3DMATRIX cameraView;
			float origin[3];

			std::memcpy(cameraView.m, bytes + Iw4::viewParmsView, sizeof(cameraView.m));
			std::memcpy(origin, bytes + Iw4::viewParmsOrigin, sizeof(origin));
			std::memcpy(projection.m, bytes + Iw4::viewParmsProjection, sizeof(projection.m));
			std::memcpy(cameraOrigin, origin, sizeof(cameraOrigin));
			hasCameraOrigin = true;

			view = cameraView;

			for (int column = 0; column < 4; ++column)
			{
				view.m[3][column] = cameraView.m[3][column] - origin[0] * cameraView.m[0][column]
					- origin[1] * cameraView.m[1][column] - origin[2] * cameraView.m[2][column];
			}

			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			didFault = true;
			return false;
		}
	}

	static void __fastcall StandardDrawDetour(void* viewInfo)
	{
		D3DMATRIX view;
		D3DMATRIX projection;

		const bool isScene = sceneDevice && viewInfo && !didFault && TryReadSceneView(viewInfo, view, projection);

		if (isScene)
		{
			sceneDevice->SetTransform(D3DTS_WORLD, &identity);
			sceneDevice->SetTransform(D3DTS_VIEW, &view);
			sceneDevice->SetTransform(D3DTS_PROJECTION, &projection);

			++stats.cameraFrames;

			if (stats.cameraFrames == 1 || stats.cameraFrames % 3000 == 0)
			{
				Log() << "remix: camera handed over " << stats.cameraFrames << " times (" << stats.skippedViews
					  << " other views); projection row0 " << projection._11 << " " << projection._12 << " "
					  << projection._13 << " " << projection._14 << " row1 " << projection._21 << " " << projection._22
					  << " " << projection._23 << " " << projection._24 << " row2 " << projection._31 << " "
					  << projection._32 << " " << projection._33 << " " << projection._34 << " row3 " << projection._41
					  << " " << projection._42 << " " << projection._43 << " " << projection._44;
			}

			sceneView = view;
			sceneProjection = projection;
			hasSceneMatrices = true;

			{
				const ProfileScope scope(ProfileSection::Lights);
				SubmitSceneLights(sceneDevice, viewInfo);
			}

			{
				const ProfileScope scope(ProfileSection::DebugDraw);
				DrawDebugShapes(sceneDevice);
			}

			BeginSceneView(sceneDevice);
		}
		else
		{
			++stats.skippedViews;

			if (didFault)
			{
				static bool hasLogged = false;

				if (!hasLogged)
				{
					hasLogged = true;
					LogError() << "remix: faulted reading the scene view, the camera is no longer handed to Remix";
				}
			}
		}

		originalStandardDraw(viewInfo);

		if (isScene)
		{
			EndSceneView();
		}
	}

	static void __fastcall ShutdownDetour(int shouldDestroyDevice)
	{
		OnDeviceLost();
		originalShutdown(shouldDestroyDevice);
	}

	static int InstallShutdownHooks()
	{
		int patched = 0;

		for (const Iw4::CallSite& call : Engine::CurrentBuild().R_ShutdownCalls)
		{
			if (call.address == 0)
			{
				continue;
			}

			const std::uintptr_t site = Engine::RebaseAddress(call.address);
			const std::uintptr_t target = Hook::ResolveCallTarget(site);

			if (!target)
			{
				continue;
			}

			if (!originalShutdown)
			{
				originalShutdown = reinterpret_cast<RendererShutdown>(target);
			}

			if (reinterpret_cast<std::uintptr_t>(originalShutdown) != target)
			{
				LogError() << "remix: the call at 0x" << std::hex << site << std::dec
						   << " does not reach the same R_Shutdown, refusing to patch it";
				continue;
			}

			if (Hook::PatchCall(site, call.bytes, sizeof(call.bytes), reinterpret_cast<const void*>(&ShutdownDetour),
					nullptr))
			{
				++patched;
			}
		}

		Log() << "remix: " << patched << " of 3 R_Shutdown calls hooked, Remix's buffers and state block are released"
				 " before the game releases the device";

		return patched;
	}

	static int SuppressHiddenPasses()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		const Iw4::BytePatch* patches[] = {
			&build.rbShadowMapsBranch,
			&build.rbFloatZBranch,
			&build.rbDepthPrepassBranch,
			&build.rbSunSpriteCall,
			&build.skyFrustumCullBranch,
			&build.R_SetupDrawGroup_DistortionBranches[0],
			&build.R_SetupDrawGroup_DistortionBranches[1],
			&build.R_SetupDrawGroup_DistortionBranches[2],
			&build.R_SetupDrawGroup_ShadowMapBranches[0],
			&build.R_SetupDrawGroup_ShadowMapBranches[1],
		};

		int applied = 0;

		for (const Iw4::BytePatch* patch : patches)
		{
			if (patch->address == 0)
			{
				continue;
			}

			if (Hook::PatchBytes(Engine::RebaseAddress(patch->address), patch->original, patch->replacement,
					patch->size))
			{
				++applied;
			}
		}

		Log() << "remix: " << applied << " of 10 engine patches applied (shadow maps, float z, depth prepass and the"
				 " sun sprite, which Remix would path trace as extra geometry, the sky frustum test, so the whole sky"
				 " is drawn every frame and Remix's sky probe never loses the sky light when it is out of view, and"
				 " the three distortion checks, whose resolved scene copy Remix would draw as black sheets, and the two"
				 " shadow map checks, which drop every surface lit by a shadowing light while shadow maps are off)";

		return applied;
	}

	using PostEffects = void(__fastcall*)(void*);

	static PostEffects originalPostEffects = nullptr;
	static bool isMapColourGrade = true;
	static float mapColourGradeStrength = 0.5f;

	using SetFilmConstants = void(__fastcall*)(void*);

	static float TowardNeutral(float value, float neutral, float strength)
	{
		return neutral + (value - neutral) * strength;
	}

	static void SoftenFilmGrade(unsigned char* view)
	{
		const std::uintptr_t setFilmConstants = Engine::CurrentBuild().R_SetFilmConstants;

		if (setFilmConstants == 0 || mapColourGradeStrength >= 1.0f)
		{
			return;
		}

		auto* brightness = reinterpret_cast<float*>(view + Iw4::viewInfoFilmBrightness);
		auto* contrast = reinterpret_cast<float*>(view + Iw4::viewInfoFilmContrast);
		auto* desaturation = reinterpret_cast<float*>(view + Iw4::viewInfoFilmDesaturation);
		auto* desaturationDark = reinterpret_cast<float*>(view + Iw4::viewInfoFilmDesaturationDark);
		auto* tints = reinterpret_cast<float*>(view + Iw4::viewInfoFilmTints);

		*brightness = TowardNeutral(*brightness, 0.0f, mapColourGradeStrength);
		*contrast = TowardNeutral(*contrast, 1.0f, mapColourGradeStrength);
		*desaturation = TowardNeutral(*desaturation, 0.0f, mapColourGradeStrength);
		*desaturationDark = TowardNeutral(*desaturationDark, 0.0f, mapColourGradeStrength);

		for (int i = 0; i < Iw4::filmTintFloats; ++i)
		{
			tints[i] = TowardNeutral(tints[i], 1.0f, mapColourGradeStrength);
		}

		reinterpret_cast<SetFilmConstants>(Engine::RebaseAddress(setFilmConstants))(view);
	}

	static void __fastcall PostEffectsDetour(void* viewInfo)
	{
		if (!isMapColourGrade || !originalPostEffects || !viewInfo)
		{
			return;
		}

		auto* view = static_cast<unsigned char*>(viewInfo);
		std::memset(view + Iw4::viewInfoDepthOfField, 0, Iw4::viewInfoDepthOfFieldBytes);
		std::memset(view + Iw4::viewInfoBlurRadius, 0, sizeof(float));
		view[Iw4::viewInfoGlowEnabled] = 0;
		view[Iw4::viewInfoWaterSheeting] = 0;
		SoftenFilmGrade(view);

		originalPostEffects(viewInfo);
	}

	static bool InstallPostEffectsHook()
	{
		const Iw4::BytePatch& call = Engine::CurrentBuild().rbPostEffectsCall;

		if (call.address == 0)
		{
			return false;
		}

		const std::uintptr_t site = Engine::RebaseAddress(call.address);
		const std::uintptr_t target = Hook::ResolveCallTarget(site);

		if (!target)
		{
			return false;
		}

		originalPostEffects = reinterpret_cast<PostEffects>(target);

		if (!Hook::PatchCall(site, call.original, call.size, reinterpret_cast<const void*>(&PostEffectsDetour),
				nullptr))
		{
			originalPostEffects = nullptr;
			return false;
		}

		Log() << "remix: the engine's post pass runs on Remix's finished frame with only its film colour grade (vision"
				 " sets, night vision, thermal), depth of field, glow, blur and water sheeting are switched off per frame";

		return true;
	}

	void SetMapColourGrade(bool isEnabled)
	{
		isMapColourGrade = isEnabled;
	}

	bool IsMapColourGrade()
	{
		return isMapColourGrade;
	}

	void SetMapColourGradeStrength(float strength)
	{
		mapColourGradeStrength = std::clamp(strength, 0.0f, 1.0f);
	}

	float GetMapColourGradeStrength()
	{
		return mapColourGradeStrength;
	}

	static void __fastcall EmissivePassDetour(void* viewInfo)
	{
		if (!sceneDevice)
		{
			originalEmissivePass(viewInfo);
			return;
		}

		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;

		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOperation);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArgument1);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG2, &alphaArgument2);

		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

		Device::BeginEffectBlendRemap(sceneDevice);
		originalEmissivePass(viewInfo);
		Device::EndEffectBlendRemap(sceneDevice);

		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOperation);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArgument1);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, alphaArgument2);
	}

	static void DrawWithPremultipliedRemap(TessDrawList original, void* context, void** iterator)
	{
		if (!sceneDevice)
		{
			original(context, iterator);
			return;
		}

		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;

		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOperation);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArgument1);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG2, &alphaArgument2);

		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

		Device::BeginPremultipliedRemap(sceneDevice);
		original(context, iterator);
		Device::EndPremultipliedRemap(sceneDevice);

		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOperation);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArgument1);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, alphaArgument2);
	}

	static void __fastcall MarksTessDetour(void* context, void** iterator)
	{
		DrawWithPremultipliedRemap(originalMarksTess, context, iterator);
	}

	static void __fastcall GlassTessDetour(void* context, void** iterator)
	{
		DrawWithPremultipliedRemap(originalGlassTess, context, iterator);
	}

	static void __fastcall ParticleCloudTessDetour(void* context, void** iterator)
	{
		if (!sceneDevice)
		{
			originalParticleCloudTess(context, iterator);
			return;
		}

		const auto source = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context));
		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);
		if (!isWeatherEnabled)
		{
			iterator[0] = static_cast<std::uint8_t*>(iterator[0]) + Iw4::particleCloudEntryBytes;
			return;
		}

		const auto cloudIndex = ReadAt<std::uint8_t>(reinterpret_cast<std::uintptr_t>(iterator[0]) + 2);
		const auto backEnd = ReadAt<std::uintptr_t>(source + Iw4::sourceBackEndData);
		const std::uintptr_t cloud = backEnd + (Iw4::backEndParticleCloudBase + cloudIndex)
			* Iw4::particleCloudStride;
		const auto colour = ReadAt<std::uint32_t>(cloud + Iw4::particleCloudColor);
		const auto material = ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);
		const bool isOutdoorMasked = IsOutdoorMaskedMaterial(material);

		if (TryDrawWeatherCloud(sceneDevice, cloud, material, isOutdoorMasked))
		{
			iterator[0] = static_cast<std::uint8_t*>(iterator[0]) + Iw4::particleCloudEntryBytes;
			return;
		}

		if (isOutdoorMasked && !IsCloudOutdoors(cloud))
		{
			iterator[0] = static_cast<std::uint8_t*>(iterator[0]) + Iw4::particleCloudEntryBytes;
			return;
		}

		DWORD colourOperation = D3DTOP_MODULATE;
		DWORD colourArgument1 = D3DTA_TEXTURE;
		DWORD colourArgument2 = D3DTA_CURRENT;
		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;
		DWORD textureFactor = 0xFFFFFFFF;

		sceneDevice->GetTextureStageState(0, D3DTSS_COLOROP, &colourOperation);
		sceneDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &colourArgument1);
		sceneDevice->GetTextureStageState(0, D3DTSS_COLORARG2, &colourArgument2);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOperation);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArgument1);
		sceneDevice->GetTextureStageState(0, D3DTSS_ALPHAARG2, &alphaArgument2);
		sceneDevice->GetRenderState(D3DRS_TEXTUREFACTOR, &textureFactor);

		sceneDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		sceneDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		sceneDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		sceneDevice->SetRenderState(D3DRS_TEXTUREFACTOR, colour);

		originalParticleCloudTess(context, iterator);

		sceneDevice->SetRenderState(D3DRS_TEXTUREFACTOR, textureFactor);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, alphaArgument2);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArgument1);
		sceneDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOperation);
		sceneDevice->SetTextureStageState(0, D3DTSS_COLORARG2, colourArgument2);
		sceneDevice->SetTextureStageState(0, D3DTSS_COLORARG1, colourArgument1);
		sceneDevice->SetTextureStageState(0, D3DTSS_COLOROP, colourOperation);
	}

	static bool InstallParticleCloudColourHook()
	{
		const bool isHooked = TryPatchCallGroup(Engine::CurrentBuild().particleCloudTessCalls, 2,
			reinterpret_cast<const void*>(&ParticleCloudTessDetour), reinterpret_cast<void**>(&originalParticleCloudTess));

		Log() << "remix: particle clouds (rain, snow, dust) reach Remix with their shader colour and fade as a texture"
				 " factor (" << isHooked << ")";

		return isHooked;
	}

	static bool InstallMarksBlendHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		const bool isMarksHooked = TryPatchCallGroup(build.marksTessCalls, 2, reinterpret_cast<const void*>(&MarksTessDetour),
			reinterpret_cast<void**>(&originalMarksTess));
		const bool isGlassHooked = TryPatchCallGroup(build.glassTessCalls, 2, reinterpret_cast<const void*>(&GlassTessDetour),
			reinterpret_cast<void**>(&originalGlassTess));

		Log() << "remix: marks (" << isMarksHooked << ") and breakable glass (" << isGlassHooked << ") reach Remix with"
				 " premultiplied blending as plain alpha blending, so their transparent texels stay transparent";

		return isMarksHooked && isGlassHooked;
	}

	static bool InstallEffectAlphaHook()
	{
		const Iw4::CallSite& call = Engine::CurrentBuild().emissivePassCall;

		if (call.address == 0)
		{
			return false;
		}

		const std::uintptr_t site = Engine::RebaseAddress(call.address);
		const std::uintptr_t original = Hook::ResolveCallTarget(site);

		if (!original)
		{
			LogError() << "remix: the emissive pass site at 0x" << std::hex << site << std::dec
					   << " is not a call, refusing to patch";
			return false;
		}

		originalEmissivePass = reinterpret_cast<EmissivePass>(original);

		if (!Hook::PatchCall(site, call.bytes, sizeof(call.bytes), reinterpret_cast<const void*>(&EmissivePassDetour),
				nullptr))
		{
			originalEmissivePass = nullptr;
			return false;
		}

		Log() << "remix: effects pass wrapped, sprites and tails reach Remix with texture x vertex alpha so their fades"
				 " show, and additive and screen blending reach it as SRCALPHA/ONE, emissive weighted by alpha as the"
				 " engine's shaders do";

		return true;
	}

	bool InstallHooks()
	{
		if (isInstalled || hasAttempted)
		{
			return isInstalled;
		}

		if (!Core::IsRemixMode() || !Engine::EngineAddressable())
		{
			return false;
		}

		hasAttempted = true;

		const Iw4::CallSite& call = Engine::CurrentBuild().rbStandardDrawCommandsJump;

		if (call.address == 0)
		{
			LogWarn() << "remix: no RB_StandardDrawCommands jump is mapped for this build, Remix will not"
						 " find the camera";
			return false;
		}

		const std::uintptr_t site = Engine::RebaseAddress(call.address);
		const std::uintptr_t original = Hook::ResolveCallTarget(site);

		if (!original)
		{
			LogError() << "remix: the site at 0x" << std::hex << site << std::dec << " is not a jump, refusing to patch";
			return false;
		}

		originalStandardDraw = reinterpret_cast<StandardDrawCommands>(original);

		if (!Hook::PatchCall(site, call.bytes, sizeof(call.bytes),
				reinterpret_cast<const void*>(&StandardDrawDetour), nullptr))
		{
			originalStandardDraw = nullptr;
			return false;
		}

		isInstalled = true;
		stats.isHookInstalled = true;

		Log() << "remix: handing the scene camera to Remix at the start of RB_StandardDrawCommands";

		stats.suppressedPasses = SuppressHiddenPasses();
		InstallPostEffectsHook();
		stats.isEffectAlphaHooked = InstallEffectAlphaHook();
		stats.isMotionTrackerHooked = InstallMotionTrackerHooks();
		stats.isMarksBlendHooked = InstallMarksBlendHooks();
		stats.isParticleCloudColourHooked = InstallParticleCloudColourHook();
		InstallShutdownHooks();
		InstallWorldHooks();
		InstallCullingPatches();
		InstallTriggerHooks();

		Log() << "remix: particle clouds (rain, snow, dust) are rebuilt on the CPU as fixed-function quads, faded near"
				 " the eye and in the distance and dropped under cover per flake";

		return true;
	}

	static void RestoreLegacyKeyboard()
	{
		constexpr UINT maxDevices = 16;
		constexpr USHORT genericDesktop = 0x01;
		constexpr USHORT keyboardUsage = 0x06;

		RAWINPUTDEVICE devices[maxDevices]{};
		UINT count = maxDevices;

		const UINT registered = GetRegisteredRawInputDevices(devices, &count, sizeof(RAWINPUTDEVICE));

		if (registered == static_cast<UINT>(-1))
		{
			return;
		}

		for (UINT i = 0; i < registered; ++i)
		{
			const bool isKeyboard = devices[i].usUsagePage == genericDesktop && devices[i].usUsage == keyboardUsage;

			if (!isKeyboard || !(devices[i].dwFlags & RIDEV_NOLEGACY))
			{
				continue;
			}

			RAWINPUTDEVICE keyboard = devices[i];
			keyboard.dwFlags &= ~static_cast<DWORD>(RIDEV_NOLEGACY | RIDEV_APPKEYS);

			if (RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard)))
			{
				Log() << "remix: the keyboard was registered for raw input with RIDEV_NOLEGACY, which stops the"
						 " game's key messages; re-registered without it, raw input still goes to the same window";
				return;
			}

			const DWORD error = GetLastError();

			RAWINPUTDEVICE plain{};
			plain.usUsagePage = genericDesktop;
			plain.usUsage = keyboardUsage;

			if (RegisterRawInputDevices(&plain, 1, sizeof(plain)))
			{
				Log() << "remix: could not keep Remix's keyboard target (error " << error
					  << "), re-registered the keyboard plainly so the game gets its key messages";
				return;
			}

			static bool hasLogged = false;

			if (!hasLogged)
			{
				hasLogged = true;
				LogError() << "remix: could not undo RIDEV_NOLEGACY on the keyboard (errors " << error << ", "
						   << GetLastError() << "), the game will not see key presses";
			}

			return;
		}
	}

	static bool isEffectCullingDisabled = false;

	static void ApplyEffectCulling()
	{
		const char* value = "1";

		if (isEffectCullingDisabled)
		{
			value = "0";
		}

		Engine::SetDvar("fx_cull_elem_draw", value);
		Engine::SetDvar("fx_cull_elem_spawn", value);
	}

	void SetEffectCullingDisabled(bool isDisabled)
	{
		isEffectCullingDisabled = isDisabled;
		ApplyEffectCulling();
	}

	bool IsEffectCullingDisabled()
	{
		return isEffectCullingDisabled;
	}

	void SetWeatherEnabled(bool isEnabled)
	{
		isWeatherEnabled = isEnabled;
	}

	bool IsWeatherEnabled()
	{
		return isWeatherEnabled;
	}

	void SetOffscreenWorldEnabled(bool isEnabled)
	{
		isOffscreenWorldEnabled = isEnabled;
	}

	bool IsOffscreenWorldEnabled()
	{
		return isOffscreenWorldEnabled;
	}

	void OnPresent(IDirect3DDevice9* device)
	{
		if (!Core::IsRemixMode())
		{
			return;
		}

		sceneDevice = device;
		stats.hasDevice = device != nullptr;

		static bool hasAppliedConfig = false;

		if (!hasAppliedConfig && isInstalled && device)
		{
			hasAppliedConfig = true;
			ApplyConfig(Core::GetConfig());
		}

		static std::string configMap;

		if (isInstalled && device)
		{
			std::string mapName;

			if (GetMapIdentity())
			{
				mapName = GetMapName();
			}

			if (mapName != configMap)
			{
				configMap = mapName;
				Core::LoadConfigForMap(mapName);
				ApplyConfig(Core::GetConfig());
			}
		}

		{
			const ProfileScope scope(ProfileSection::TextureTags);
			UpdateTextureTags(device);
		}

		{
			const ProfileScope scope(ProfileSection::PbrMaterials);
			UpdatePbrMaterials();
		}
		{
			const ProfileScope scope(ProfileSection::MapSettings);
			UpdateMapSettings(device);
		}

		{
			const ProfileScope scope(ProfileSection::Triggers);
			UpdateTriggers();
		}

		{
			const ProfileScope scope(ProfileSection::Culling);
			UpdateCulling();
		}

		UpdateMapFog(device);
		UpdateSkyBrightness(device);
		UpdateLook(device);

		{
			const ProfileScope scope(ProfileSection::Options);
			UpdateOptions(device);
		}

		EndProfileFrame();
		LogEngineLimits();

		static unsigned keyboardTick = 0;

		if ((keyboardTick++ % 60) == 0)
		{
			RestoreLegacyKeyboard();
		}

		static unsigned tick = 0;
		static bool hasForcedDvars = false;
		constexpr unsigned reapplyInterval = 600;

		const bool isDue = !hasForcedDvars || (tick % reapplyInterval) == 0;
		++tick;

		if (!isDue || !Engine::SetDvar("r_smp_backend", "0") || !Engine::SetDvar("r_pretess", "0")
			|| !Engine::SetDvar("r_smc_enable", "0"))
		{
			return;
		}

		ApplyEffectCulling();

		if (!hasForcedDvars)
		{
			hasForcedDvars = true;
			Log() << "remix: r_smp_backend forced to 0, the engine renders on one thread so Remix never"
					 " sees two threads at once; r_pretess and r_smc_enable forced to 0, so the world and static"
					 " models take the draw paths Remix mode replaces (re-applied every " << reapplyInterval
				  << " frames)";
		}
	}

	// a zero-area orthographic draw is the first thing Remix classes as UI: it composites the path-traced frame there
	// and rasterises everything after it, which is what keeps HUD, menus and zw3's own lobby drawing out of the scene
	void MarkUiPhase(IDirect3DDevice9* device)
	{
		if (!device)
		{
			return;
		}

		if (!injectionState && FAILED(device->CreateStateBlock(D3DSBT_ALL, &injectionState)))
		{
			injectionState = nullptr;
			return;
		}

		injectionState->Capture();

		const InjectionVertex triangle[3] = {
			{ { 0.0f, 0.0f, 0.5f }, D3DCOLOR_ARGB(0, 0, 0, 0) },
			{ { 0.0f, 0.0f, 0.5f }, D3DCOLOR_ARGB(0, 0, 0, 0) },
			{ { 0.0f, 0.0f, 0.5f }, D3DCOLOR_ARGB(0, 0, 0, 0) },
		};

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
		device->SetTexture(0, nullptr);
		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetTransform(D3DTS_VIEW, &identity);
		device->SetTransform(D3DTS_PROJECTION, &orthographic);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);

		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(InjectionVertex));

		injectionState->Apply();
	}

	void OnPostEffectsBegin()
	{
		if (!sceneDevice)
		{
			return;
		}

		if (!injectionState && FAILED(sceneDevice->CreateStateBlock(D3DSBT_ALL, &injectionState)))
		{
			injectionState = nullptr;

			static bool hasLogged = false;

			if (!hasLogged)
			{
				hasLogged = true;
				LogError() << "remix: could not create a state block, the HUD stays inside the path-traced frame";
			}

			return;
		}

		if (isOffscreenWorldEnabled)
		{
			DrawCulledBatches(sceneDevice);
		}

		DrawRainEverywhere(sceneDevice);

		{
			const ProfileScope scope(ProfileSection::Markers);
			DrawMapMarkers(sceneDevice);
		}

		MarkUiPhase(sceneDevice);

		++stats.injectionFrames;

		if (stats.injectionFrames == 1)
		{
			Log() << "remix: a zero-area UI draw at the end of the 3D scene makes Remix composite the path-traced"
					 " frame there, so post effects, the HUD and menus draw on top of it";
		}
	}

	void OnDeviceLost()
	{
		ResetTextureTags();
		ForgetSkyMaterial();
		ReleaseWeather();
		ReleaseDebugDraw();
		ReleaseLights();
		ReleaseWorldBuffers();
		ReleaseMarkers();

		if (injectionState)
		{
			injectionState->Release();
			injectionState = nullptr;
		}

		sceneDevice = nullptr;
		stats.hasDevice = false;
	}

	IDirect3DDevice9* GetSceneDevice()
	{
		return sceneDevice;
	}

	static float glowBrightness = 1.0f;

	void SetGlowBrightness(float brightness)
	{
		glowBrightness = std::clamp(brightness, 0.0f, 8.0f);

		const remixapi_Interface* api = TryGetApi(sceneDevice);

		if (!api)
		{
			return;
		}

		char text[32];
		std::snprintf(text, sizeof(text), "%.3f", glowBrightness);
		SendOption(api, "rtx.emissiveIntensity", text);
	}

	float GetGlowBrightness()
	{
		return glowBrightness;
	}

	std::uintptr_t GetMapIdentity()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.gfxWorld == 0)
		{
			return 0;
		}

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));

		if (!world)
		{
			return 0;
		}

		return world ^ (static_cast<std::uintptr_t>(ReadAt<std::uint32_t>(world + Iw4::worldSModelCount)) << 44);
	}

	std::string GetMapName()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.gfxWorld == 0)
		{
			return {};
		}

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));

		if (!world)
		{
			return {};
		}

		const auto* assetName = ReadAt<const char*>(world + Iw4::worldName);

		if (!assetName)
		{
			return {};
		}

		std::string name(assetName, strnlen(assetName, 128));

		const std::size_t slash = name.find_last_of('/');

		if (slash != std::string::npos)
		{
			name.erase(0, slash + 1);
		}

		const std::size_t dot = name.find('.');

		if (dot != std::string::npos)
		{
			name.erase(dot);
		}

		return name;
	}

	bool TryProjectToScreen(const float* point, float width, float height, float* screen)
	{
		if (!hasSceneMatrices)
		{
			return false;
		}

		float viewSpace[4];

		for (int column = 0; column < 4; ++column)
		{
			viewSpace[column] = point[0] * sceneView.m[0][column] + point[1] * sceneView.m[1][column]
				+ point[2] * sceneView.m[2][column] + sceneView.m[3][column];
		}

		float clip[4];

		for (int column = 0; column < 4; ++column)
		{
			clip[column] = viewSpace[0] * sceneProjection.m[0][column] + viewSpace[1] * sceneProjection.m[1][column]
				+ viewSpace[2] * sceneProjection.m[2][column] + viewSpace[3] * sceneProjection.m[3][column];
		}

		if (clip[3] <= 0.001f)
		{
			return false;
		}

		const float x = clip[0] / clip[3];
		const float y = clip[1] / clip[3];

		if (x < -1.0f || x > 1.0f || y < -1.0f || y > 1.0f)
		{
			return false;
		}

		screen[0] = (x * 0.5f + 0.5f) * width;
		screen[1] = (0.5f - y * 0.5f) * height;
		return true;
	}

	bool TryGetCameraOrigin(float* origin)
	{
		if (!hasCameraOrigin)
		{
			return false;
		}

		std::memcpy(origin, cameraOrigin, sizeof(cameraOrigin));
		return true;
	}

	bool IsEffectDrawn(EffectKind kind)
	{
		return !isEffectHidden[static_cast<int>(kind)];
	}

	bool TrySetEffectDrawn(EffectKind kind, bool isDrawn)
	{
		const int index = static_cast<int>(kind);

		if (!Core::IsRemixMode() || !Engine::EngineAddressable() || isEffectHidden[index] != isDrawn)
		{
			return false;
		}

		const Iw4::BytePatch& patch = Engine::CurrentBuild().effectSkipBranches[index];

		if (patch.address == 0)
		{
			return false;
		}

		const std::uintptr_t site = Engine::RebaseAddress(patch.address);

		bool didChange = false;

		if (isDrawn)
		{
			didChange = Hook::RestoreBytes(site);
		}
		else
		{
			didChange = Hook::PatchBytes(site, patch.original, patch.replacement, patch.size);
		}

		if (didChange)
		{
			isEffectHidden[index] = !isDrawn;
		}

		return didChange;
	}

	RemixStats GetStats()
	{
		return stats;
	}
}
