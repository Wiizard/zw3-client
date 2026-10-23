#include "STDInclude.hpp"

#include "Look.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "Device.hpp"
#include "Api.hpp"
#include "Options.hpp"
#include "PbrMaterials.hpp"
#include "Culling.hpp"
#include "MapSettings.hpp"
#include "Outdoor.hpp"
#include "Scene.hpp"
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

	static float outdoorBlend = 0.0f;
	static ULONGLONG lastOutdoorTick = 0;
	static bool hasOutdoorMask = false;
	static bool isCameraOutdoors = false;

	static void UpdateOutdoorBlend()
	{
		const ULONGLONG now = GetTickCount64();
		const float seconds = lastOutdoorTick == 0 ? 0.0f : std::min(static_cast<float>(now - lastOutdoorTick) / 1000.0f, 0.25f);
		lastOutdoorTick = now;

		float camera[3];
		bool isOutdoors = false;
		hasOutdoorMask = TryGetCameraOrigin(camera) && TryTestPointOutdoors(camera, isOutdoors);
		isCameraOutdoors = hasOutdoorMask && isOutdoors;

		const float target = isCameraOutdoors ? 1.0f : 0.0f;
		constexpr float blendPerSecond = 1.5f;
		const float step = std::min(1.0f, seconds * blendPerSecond);
		outdoorBlend += (target - outdoorBlend) * step;

		if (std::fabs(target - outdoorBlend) < 0.01f)
		{
			outdoorBlend = target;
		}
	}

	OutdoorExposureState GetOutdoorExposureState()
	{
		return { hasOutdoorMask, isCameraOutdoors, outdoorBlend };
	}

	struct ZoneValues
	{
		float exposureEv = 0.0f;
		float shadowLift = 0.0f;
		float effectGlow = 0.2f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float tint[3] = { 1.0f, 1.0f, 1.0f };
	};

	static bool isZoneGrading = false;

	static ZoneValues zoneValues;
	static bool hasZoneValues = false;
	static ULONGLONG lastZoneTick = 0;
	static int zoneCell = -1;
	static std::string zoneName;

	static const MapZone* FindActiveZone(int cell, const float* camera, bool hasCamera)
	{
		if (!desired.isMapZones)
		{
			return nullptr;
		}

		for (const MapZone& zone : GetMapSettings().zones)
		{
			if (cell >= 0 && std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end())
			{
				return &zone;
			}

			if (zone.hasBox && hasCamera)
			{
				bool isInside = true;

				for (int axis = 0; axis < 3; ++axis)
				{
					const float low = std::min(zone.boxMin[axis], zone.boxMax[axis]);
					const float high = std::max(zone.boxMin[axis], zone.boxMax[axis]);
					isInside = isInside && camera[axis] >= low && camera[axis] <= high;
				}

				if (isInside)
				{
					return &zone;
				}
			}
		}

		return nullptr;
	}

	static void UpdateZoneValues()
	{
		const ULONGLONG now = GetTickCount64();
		const float seconds = lastZoneTick == 0 ? 0.0f : std::min(static_cast<float>(now - lastZoneTick) / 1000.0f, 0.25f);
		lastZoneTick = now;

		float camera[3];
		const bool hasCamera = TryGetCameraOrigin(camera);
		zoneCell = GetCameraCell();

		const MapZone* zone = FindActiveZone(zoneCell, camera, hasCamera);
		zoneName = zone ? (zone->name.empty() ? std::string("(unnamed)") : zone->name) : std::string();

		ZoneValues target;
		target.exposureEv = zone && zone->exposureEv ? *zone->exposureEv : 0.0f;
		target.shadowLift = zone && zone->shadowLift ? *zone->shadowLift : desired.shadowLift;
		target.effectGlow = zone && zone->effectGlow ? *zone->effectGlow : desired.effectGlow;

		const bool hasZoneGrade = zone && (zone->contrast || zone->saturation || zone->hasTint);
		const float baseContrast = desired.isColourGrading ? desired.contrast : 1.0f;
		const float baseSaturation = desired.isColourGrading ? desired.saturation : 1.0f;
		target.contrast = zone && zone->contrast ? *zone->contrast : baseContrast;
		target.saturation = zone && zone->saturation ? *zone->saturation : baseSaturation;

		for (int channel = 0; channel < 3; ++channel)
		{
			const float baseTint = desired.isColourGrading ? desired.colourBalance[channel] : 1.0f;
			target.tint[channel] = zone && zone->hasTint ? zone->tint[channel] : baseTint;
		}

		if (!hasZoneValues)
		{
			zoneValues = target;
			hasZoneValues = true;
			return;
		}

		const float blendSeconds = zone ? zone->blendSeconds : 1.0f;
		const float step = blendSeconds <= 0.0f ? 1.0f : std::min(1.0f, seconds / blendSeconds * 3.0f);

		const auto ease = [step](float& value, float goal)
		{
			value += (goal - value) * step;

			if (std::fabs(goal - value) < 0.002f)
			{
				value = goal;
			}
		};

		ease(zoneValues.exposureEv, target.exposureEv);
		ease(zoneValues.shadowLift, target.shadowLift);
		ease(zoneValues.effectGlow, target.effectGlow);
		ease(zoneValues.contrast, target.contrast);
		ease(zoneValues.saturation, target.saturation);

		bool isNeutral = std::fabs(zoneValues.contrast - 1.0f) < 0.002f && std::fabs(zoneValues.saturation - 1.0f) < 0.002f;

		for (int channel = 0; channel < 3; ++channel)
		{
			ease(zoneValues.tint[channel], target.tint[channel]);
			isNeutral = isNeutral && std::fabs(zoneValues.tint[channel] - 1.0f) < 0.002f;
		}

		isZoneGrading = hasZoneGrade || !isNeutral;
	}

	ZoneState GetZoneState()
	{
		return { zoneCell, zoneName, zoneValues.exposureEv, zoneValues.shadowLift, zoneValues.effectGlow,
			zoneValues.contrast, zoneValues.saturation, { zoneValues.tint[0], zoneValues.tint[1], zoneValues.tint[2] },
			isZoneGrading || desired.isColourGrading };
	}

	float GetEffectiveShadowLift()
	{
		return hasZoneValues ? zoneValues.shadowLift : desired.shadowLift;
	}

	void UpdateLook(IDirect3DDevice9* device)
	{
		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		UpdateOutdoorBlend();
		UpdateZoneValues();

		const float outdoorEvNow = std::round(outdoorBlend * desired.outdoorEv * 50.0f) / 50.0f;
		const float extraEv = std::round((zoneValues.exposureEv + outdoorEvNow) * 50.0f) / 50.0f;
		const float exposureBiasNow = desired.exposureBiasEv + extraEv;
		const float localExposureNow = desired.localExposure * std::pow(2.0f, extraEv);

		if (desired.tonemapper != submitted.tonemapper || !hasSentAll)
		{
			SendInt(api, "rtx.tonemappingMode", desired.tonemapper);
			submitted.tonemapper = desired.tonemapper;
		}

		SendBoolIfChanged(api, "rtx.autoExposure.enabled", desired.isAutoExposure, submitted.isAutoExposure);
		SendFloatIfChanged(api, "rtx.autoExposure.evMinValue", desired.exposureMinEv, submitted.exposureMinEv);
		SendFloatIfChanged(api, "rtx.autoExposure.evMaxValue", desired.exposureMaxEv, submitted.exposureMaxEv);
		SendFloatIfChanged(api, "rtx.autoExposure.autoExposureSpeed", desired.exposureSpeed, submitted.exposureSpeed);

		SendFloatIfChanged(api, "rtx.localtonemap.exposure", localExposureNow, submitted.localExposure);
		SendFloatIfChanged(api, "rtx.localtonemap.shadows", desired.localShadows, submitted.localShadows);
		SendFloatIfChanged(api, "rtx.localtonemap.highlights", desired.localHighlights, submitted.localHighlights);
		SendBoolIfChanged(api, "rtx.localtonemap.boostLocalContrast", desired.isLocalContrastBoosted,
			submitted.isLocalContrastBoosted);
		SendBoolIfChanged(api, "rtx.localtonemap.finalizeWithACES", desired.isLocalAces, submitted.isLocalAces);

		SendFloatIfChanged(api, "rtx.tonemap.exposureBias", exposureBiasNow, submitted.exposureBiasEv);

		static bool isMicromapSent = false;
		SendBoolIfChanged(api, "rtx.opacityMicromap.enable", desired.isOpacityMicromaps, isMicromapSent);
		const auto grade = [](float value) { return std::round(value * 200.0f) / 200.0f; };
		SendBoolIfChanged(api, "rtx.tonemap.colorGradingEnabled", desired.isColourGrading || isZoneGrading,
			submitted.isColourGrading);
		SendFloatIfChanged(api, "rtx.tonemap.contrast", grade(zoneValues.contrast), submitted.contrast);
		SendFloatIfChanged(api, "rtx.tonemap.saturation", grade(zoneValues.saturation), submitted.saturation);
		SendBoolIfChanged(api, "rtx.tonemap.finalizeWithACES", desired.isGlobalAces, submitted.isGlobalAces);
		static bool isToneCurveSent = false;
		SendBoolIfChanged(api, "rtx.tonemap.tonemappingEnabled", !desired.isSteadyToneCurve, isToneCurveSent);

		if (!hasSentAll)
		{
			SendInt(api, "rtx.autoExposure.exposureAverageMode", 1);
		}

		const float balance[3] = { grade(zoneValues.tint[0]), grade(zoneValues.tint[1]), grade(zoneValues.tint[2]) };
		const bool isBalanceChanged = balance[0] != submitted.colourBalance[0] || balance[1] != submitted.colourBalance[1]
			|| balance[2] != submitted.colourBalance[2];

		if (isBalanceChanged || !hasSentAll)
		{
			char text[64];
			std::snprintf(text, sizeof(text), "%.4f, %.4f, %.4f", balance[0], balance[1], balance[2]);
			SendOption(api, "rtx.tonemap.colorBalance", text);

			for (int channel = 0; channel < 3; ++channel)
			{
				submitted.colourBalance[channel] = balance[channel];
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
		SendFloatIfChanged(api, "rtx.emissiveBlendOverrideEmissiveIntensity",
			std::round(zoneValues.effectGlow * 100.0f) / 100.0f, submitted.effectGlow);

		static bool isStochasticSent = false;
		SendBoolIfChanged(api, "rtx.enableStochasticAlphaBlend", !desired.isSteadyTransparency, isStochasticSent);
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
