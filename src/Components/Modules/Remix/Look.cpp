#include "STDInclude.hpp"

#include "Look.hpp"

#include <cstdio>
#include <string>

#include "Device.hpp"
#include "Api.hpp"
#include "Options.hpp"
#include "PbrMaterials.hpp"
#include "Weather.hpp"

namespace Components::Remix
{
	static LookSettings desired;
	static LookSettings submitted;
	static bool isLightCacheOff = false;
	static bool hasSentAll = false;
	static float sentRoughnessScale = 1.0f;
	static float sentMetallicBias = 0.0f;
	static constexpr float mirrorRoughnessScale = 0.05f;
	static float sentAlbedoScale = 1.0f;
	static constexpr float wetAlbedoScale = 0.8f;
	static constexpr float wetRoughnessScale = 0.45f;
	static int sentDisplacementMode = 0;
	static constexpr int raymarchDisplacement = 1;
	static bool sentFreeCamera = false;
	static bool sentReferenceMode = false;
	static bool sentCameraLock = false;
	static constexpr const char* photoKeys[][2] = {
		{ "rtx.freeCam.keyMoveForward", "NUMPAD8" },
		{ "rtx.freeCam.keyMoveBack", "NUMPAD5" },
		{ "rtx.freeCam.keyMoveLeft", "NUMPAD4" },
		{ "rtx.freeCam.keyMoveRight", "NUMPAD6" },
		{ "rtx.freeCam.keyMoveUp", "NUMPAD9" },
		{ "rtx.freeCam.keyMoveDown", "NUMPAD7" },
		{ "rtx.freeCam.keyMoveFaster", "NUMPAD0" },
		{ "rtx.freeCam.keyPitchDown", "DOWN" },
		{ "rtx.freeCam.keyPitchUp", "UP" },
		{ "rtx.freeCam.keyYawLeft", "LEFT" },
		{ "rtx.freeCam.keyYawRight", "RIGHT" },
	};

	static void SendFloat(const remixapi_Interface* api, const char* name, float value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%.4f", value);
		SendOption(api, name, text);
	}

	static void SendBool(const remixapi_Interface* api, const char* name, bool value)
	{
		if (value)
		{
			SendOption(api, name, "True");
		}
		else
		{
			SendOption(api, name, "False");
		}
	}

	static void SendInt(const remixapi_Interface* api, const char* name, int value)
	{
		char text[16];
		std::snprintf(text, sizeof(text), "%d", value);
		SendOption(api, name, text);
	}

	static void SendFloatIfChanged(const remixapi_Interface* api, const char* name, float value, float& sent)
	{
		if (value != sent || !hasSentAll)
		{
			SendFloat(api, name, value);
			sent = value;
		}
	}

	static void SendBoolIfChanged(const remixapi_Interface* api, const char* name, bool value, bool& sent)
	{
		if (value != sent || !hasSentAll)
		{
			SendBool(api, name, value);
			sent = value;
		}
	}

	static bool IsPostEffectWanted(const LookSettings& settings)
	{
		return settings.isVignette || settings.isChromaticAberration || settings.isMotionBlur;
	}

	void SetLookSettings(const LookSettings& settings)
	{
		desired = settings;
	}

	LookSettings GetLookSettings()
	{
		return desired;
	}

	void UpdateLook(IDirect3DDevice9* device)
	{
		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		if (desired.tonemapper != submitted.tonemapper || !hasSentAll)
		{
			SendInt(api, "rtx.tonemappingMode", desired.tonemapper);
			submitted.tonemapper = desired.tonemapper;
		}

		SendBoolIfChanged(api, "rtx.autoExposure.enabled", desired.isAutoExposure, submitted.isAutoExposure);
		SendFloatIfChanged(api, "rtx.autoExposure.evMinValue", desired.exposureMinEv, submitted.exposureMinEv);
		SendFloatIfChanged(api, "rtx.autoExposure.evMaxValue", desired.exposureMaxEv, submitted.exposureMaxEv);
		SendFloatIfChanged(api, "rtx.autoExposure.autoExposureSpeed", desired.exposureSpeed, submitted.exposureSpeed);

		SendFloatIfChanged(api, "rtx.localtonemap.exposure", desired.localExposure, submitted.localExposure);
		SendFloatIfChanged(api, "rtx.localtonemap.shadows", desired.localShadows, submitted.localShadows);
		SendFloatIfChanged(api, "rtx.localtonemap.highlights", desired.localHighlights, submitted.localHighlights);
		SendBoolIfChanged(api, "rtx.localtonemap.boostLocalContrast", desired.isLocalContrastBoosted,
			submitted.isLocalContrastBoosted);
		SendBoolIfChanged(api, "rtx.localtonemap.finalizeWithACES", desired.isLocalAces, submitted.isLocalAces);

		SendFloatIfChanged(api, "rtx.tonemap.exposureBias", desired.exposureBiasEv, submitted.exposureBiasEv);
		SendBoolIfChanged(api, "rtx.tonemap.colorGradingEnabled", desired.isColourGrading, submitted.isColourGrading);
		SendFloatIfChanged(api, "rtx.tonemap.contrast", desired.contrast, submitted.contrast);
		SendFloatIfChanged(api, "rtx.tonemap.saturation", desired.saturation, submitted.saturation);
		SendBoolIfChanged(api, "rtx.tonemap.finalizeWithACES", desired.isGlobalAces, submitted.isGlobalAces);

		const bool isBalanceChanged = desired.colourBalance[0] != submitted.colourBalance[0]
			|| desired.colourBalance[1] != submitted.colourBalance[1]
			|| desired.colourBalance[2] != submitted.colourBalance[2];

		if (isBalanceChanged || !hasSentAll)
		{
			char text[64];
			std::snprintf(text, sizeof(text), "%.4f, %.4f, %.4f", desired.colourBalance[0], desired.colourBalance[1],
				desired.colourBalance[2]);
			SendOption(api, "rtx.tonemap.colorBalance", text);

			for (int channel = 0; channel < 3; ++channel)
			{
				submitted.colourBalance[channel] = desired.colourBalance[channel];
			}
		}

		SendBoolIfChanged(api, "rtx.bloom.enable", desired.isBloom, submitted.isBloom);
		SendFloatIfChanged(api, "rtx.bloom.burnIntensity", desired.bloomIntensity, submitted.bloomIntensity);
		SendFloatIfChanged(api, "rtx.bloom.luminanceThreshold", desired.bloomThreshold, submitted.bloomThreshold);

		const bool isPostWanted = IsPostEffectWanted(desired);

		if (isPostWanted != IsPostEffectWanted(submitted) || !hasSentAll)
		{
			SendBool(api, "rtx.postfx.enable", isPostWanted);
		}

		SendBoolIfChanged(api, "rtx.postfx.enableVignette", desired.isVignette, submitted.isVignette);
		SendFloatIfChanged(api, "rtx.postfx.vignetteIntensity", desired.vignetteIntensity, submitted.vignetteIntensity);
		SendFloatIfChanged(api, "rtx.postfx.vignetteRadius", desired.vignetteRadius, submitted.vignetteRadius);
		SendFloatIfChanged(api, "rtx.postfx.vignetteSoftness", desired.vignetteSoftness, submitted.vignetteSoftness);

		SendBoolIfChanged(api, "rtx.postfx.enableChromaticAberration", desired.isChromaticAberration,
			submitted.isChromaticAberration);
		SendFloatIfChanged(api, "rtx.postfx.chromaticAberrationAmount", desired.chromaticAmount,
			submitted.chromaticAmount);

		SendBoolIfChanged(api, "rtx.postfx.enableMotionBlur", desired.isMotionBlur, submitted.isMotionBlur);

		SendBoolIfChanged(api, "rtx.volumetrics.enable", desired.isHaze || desired.isGodRays, submitted.isHaze);
		SendFloatIfChanged(api, "rtx.emissiveBlendOverrideEmissiveIntensity", desired.effectGlow, submitted.effectGlow);
		SendBoolIfChanged(api, "rtx.skyReprojectToMainCameraSpace", desired.isSkyReprojected, submitted.isSkyReprojected);
		SendBoolIfChanged(api, "rtx.useWhiteMaterialMode", desired.isWhiteMaterial, submitted.isWhiteMaterial);

		SendBoolIfChanged(api, "rtx.playerModel.enablePrimaryShadows", desired.isPlayerShadowCopies,
			submitted.isPlayerShadowCopies);
		SendBoolIfChanged(api, "rtx.playerModel.enableVirtualInstances", desired.isPlayerVirtualInstances,
			submitted.isPlayerVirtualInstances);

		const bool shouldCacheBeOff = desired.isNeonCacheOff && IsNeonFoliage();

		if (shouldCacheBeOff != isLightCacheOff)
		{
			SendBool(api, "rtx.neeCache.enable", !shouldCacheBeOff);
			isLightCacheOff = shouldCacheBeOff;
		}

		if (desired.indirectMode != submitted.indirectMode || !hasSentAll)
		{
			SendInt(api, "rtx.integrateIndirectMode", desired.indirectMode);
			submitted.indirectMode = desired.indirectMode;
		}

		if (desired.maxBounces != submitted.maxBounces || !hasSentAll)
		{
			SendInt(api, "rtx.pathMaxBounces", desired.maxBounces);
			submitted.maxBounces = desired.maxBounces;
		}

		SendBoolIfChanged(api, "rtx.enableSecondaryBounces", desired.isBounceLight, submitted.isBounceLight);

		if (desired.isGlassShadows != submitted.isGlassShadows || !hasSentAll)
		{
			SendBool(api, "rtx.enableDirectTranslucentShadows", desired.isGlassShadows);
			SendBool(api, "rtx.enableIndirectTranslucentShadows", desired.isGlassShadows);
			submitted.isGlassShadows = desired.isGlassShadows;
		}

		SendBoolIfChanged(api, "rtx.subsurface.enableDiffusionProfile", desired.isSkinScattering,
			submitted.isSkinScattering);

		float albedoScale = desired.albedoScale;
		float roughnessScale = desired.roughnessScale;
		float metallicBias = 0.0f;

		if (IsWetActive())
		{
			albedoScale *= wetAlbedoScale;
			roughnessScale *= wetRoughnessScale;
		}

		SendFloatIfChanged(api, "rtx.opaqueMaterial.albedoScale", albedoScale, sentAlbedoScale);

		if (desired.isMirror)
		{
			roughnessScale *= mirrorRoughnessScale;
			metallicBias = desired.mirrorMetal;
		}

		SendFloatIfChanged(api, "rtx.opaqueMaterial.roughnessScale", roughnessScale, sentRoughnessScale);
		SendFloatIfChanged(api, "rtx.opaqueMaterial.metallicBias", metallicBias, sentMetallicBias);
		SendFloatIfChanged(api, "rtx.opaqueMaterial.roughnessBias", desired.roughnessBias, submitted.roughnessBias);
		SendFloatIfChanged(api, "rtx.opaqueMaterial.metallicScale", desired.metallicScale, submitted.metallicScale);
		SendFloatIfChanged(api, "rtx.opaqueMaterial.normalIntensity", desired.normalIntensity,
			submitted.normalIntensity);

		SetPuddlesActive(IsWetActive() && desired.isPuddles);
		SetFixtureGlow(desired.isFixtureLights, desired.fixtureBrightness);

		int displacementMode = 0;

		if (desired.isParallax)
		{
			displacementMode = raymarchDisplacement;
		}

		if (displacementMode != sentDisplacementMode || !hasSentAll)
		{
			SendInt(api, "rtx.displacement.mode", displacementMode);
			sentDisplacementMode = displacementMode;
		}

		SendFloatIfChanged(api, "rtx.displacement.displacementFactor", desired.parallaxDepth, submitted.parallaxDepth);

		if (!hasSentAll)
		{
			for (const auto& key : photoKeys)
			{
				SendOption(api, key[0], key[1]);
			}

			Device::NoteFrameEvent("remix photo options");
		}

		SendBoolIfChanged(api, "rtx.camera.enableFreeCamera", desired.isPhotoMode, sentFreeCamera);
		SendBoolIfChanged(api, "rtx.camera.lockFreeCamera", desired.isPhotoMode && desired.isPhotoLocked, sentCameraLock);
		SendBoolIfChanged(api, "rtx.useDenoiserReferenceMode", desired.isPhotoMode && desired.isPhotoAccumulate,
			sentReferenceMode);

		hasSentAll = true;
	}

	bool IsWetActive()
	{
		if (desired.wetMode == 2)
		{
			return true;
		}

		return desired.wetMode == 1 && IsRaining();
	}
}
