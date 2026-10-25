#include "STDInclude.hpp"

#include "Look.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>

#include "Core.hpp"
#include "Device.hpp"
#include "Api.hpp"
#include "AreaExposure.hpp"
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

	const PlusWeatherPreset* GetPlusWeather()
	{
		if (!IsRemixPlus())
		{
			return nullptr;
		}

		static constexpr int autoWeather = 0;
		static constexpr int firstPreset = 2;
		static constexpr const char* autoRainPreset = "rainstorm";

		if (desired.plusWeather == autoWeather)
		{
			if (!IsRaining())
			{
				return nullptr;
			}

			for (const PlusWeatherPreset& preset : plusWeatherPresets)
			{
				if (std::string_view(preset.name) == autoRainPreset)
				{
					return &preset;
				}
			}

			return nullptr;
		}

		const int index = desired.plusWeather - firstPreset;
		return index >= 0 && index < static_cast<int>(std::size(plusWeatherPresets)) ? &plusWeatherPresets[index] : nullptr;
	}

	static bool IsRainPreset(const PlusWeatherPreset& preset)
	{
		const std::string_view name = preset.name;
		return preset.hasPrecipitation && name != "snow" && name != "blizzard" && name != "sandstorm";
	}

	// Rain is zw3's streaks unless the fork's particles are asked for; snow and sand only exist as fork particles.
	static bool IsForkParticleWeather(const PlusWeatherPreset* preset)
	{
		return preset && preset->hasPrecipitation && (desired.isPlusRainParticles || !IsRainPreset(*preset));
	}

	bool IsPlusPrecipitationActive()
	{
		return IsForkParticleWeather(GetPlusWeather());
	}

	// Remix Plus weather goes through its game-value store (remixapi_SetGameValue, __weather.* in rtx_remix_api.cpp).
	// An empty target leaves the blender dormant.
	static void SendPlusWeather(const remixapi_Interface* api)
	{
		static std::string sentTarget = "unsent";
		static constexpr const char* blendSeconds = "5";

		if (!api->SetGameValue)
		{
			return;
		}

		if (!hasSentAll)
		{
			api->SetGameValue("__weather.blend_seconds", blendSeconds);
		}

		const PlusWeatherPreset* preset = GetPlusWeather();
		const std::string target = preset ? preset->name : "";

		// zw3 draws the rain itself (Weather.cpp); the preset still sets the sky, clouds and fog either way.
		static int sentParticles = -1;
		const int isParticles = IsForkParticleWeather(preset) ? 1 : 0;

		if (isParticles != sentParticles || !hasSentAll)
		{
			SendBool(api, "rtx.weather.precipitation.enable", isParticles != 0);
			sentParticles = isParticles;
		}

		if (target != sentTarget)
		{
			api->SetGameValue("__weather.target", target.c_str());
			sentTarget = target;
			Log() << "remix: Remix Plus weather " << (target.empty() ? "off" : target);
		}
	}

	// Remix Plus has no local tonemapper, EV range or tone curve rebuild. It picks an operator, and its auto exposure
	// The auto exposure's EV range: the configured one, or with Area exposure a window inside it placed by the light
	// of the spot the player stands on (AreaExposure.cpp), the same on both runtimes.
	static void SendExposureRange(const remixapi_Interface* api)
	{
		float minEv = desired.exposureMinEv;
		float maxEv = desired.exposureMaxEv;

		if (desired.isAutoExposure)
		{
			AreaExposureSettings area;
			area.isEnabled = desired.isAreaExposure;
			area.strength = desired.areaExposureStrength;
			area.sunlitEv = desired.areaExposureSunlitEv;
			area.windowEv = desired.areaExposureWindowEv;
			TryGetAreaExposure(area, desired.exposureMinEv, desired.exposureMaxEv, minEv, maxEv);
		}

		SendFloatIfChanged(api, "rtx.autoExposure.evMinValue", minEv, submitted.exposureMinEv);
		SendFloatIfChanged(api, "rtx.autoExposure.evMaxValue", maxEv, submitted.exposureMaxEv);
	}

	// takes a target brightness, a cap on brightening and adaptation times.
	static void SendPlusTonemapping(const remixapi_Interface* api)
	{
		if (desired.plusOperator != submitted.plusOperator || !hasSentAll)
		{
			SendInt(api, "rtx.tonemap.tonemapOperator", desired.plusOperator);
			submitted.plusOperator = desired.plusOperator;
		}

		SendFloatIfChanged(api, "rtx.autoExposure.targetAdaptedYf", desired.plusExposureTarget, submitted.plusExposureTarget);

		// zw3's fork build bounds the exposure by the same EV range as stock (rtx_auto_exposure.cpp), so saved maps
		// behave the same on both. The fork's own brightening cap is lifted so only that range applies, like stock.
		SendExposureRange(api);

		if (!hasSentAll)
		{
			SendFloat(api, "rtx.autoExposure.maxExposure", 1000000.0f);
		}

		// Adapt speed divides the fork's default adaptation times (0.15 s to dim, 0.75 s to brighten).
		if (desired.exposureSpeed != submitted.exposureSpeed || !hasSentAll)
		{
			const float speed = std::max(desired.exposureSpeed, 0.01f);
			SendFloat(api, "rtx.autoExposure.lightAdaptTau", 0.15f / speed);
			SendFloat(api, "rtx.autoExposure.darkAdaptTau", 0.75f / speed);
			submitted.exposureSpeed = desired.exposureSpeed;
		}

		SendFloatIfChanged(api, "rtx.tonemap.psycho17.shadows", desired.plusShadows, submitted.plusShadows);
		SendFloatIfChanged(api, "rtx.tonemap.psycho17.highlights", desired.plusHighlights, submitted.plusHighlights);
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

		const bool isPlus = IsRemixPlus();

		SendBoolIfChanged(api, "rtx.autoExposure.enabled", desired.isAutoExposure, submitted.isAutoExposure);

		if (isPlus)
		{
			SendPlusTonemapping(api);

			// rtx.skyMode 1 is Numos. Stock has no such option, so it is only sent to Remix Plus.
			if (desired.isPhysicalSky != submitted.isPhysicalSky || !hasSentAll)
			{
				SendInt(api, "rtx.skyMode", desired.isPhysicalSky ? 1 : 0);
				submitted.isPhysicalSky = desired.isPhysicalSky;
			}

			SendPlusWeather(api);
		}
		else
		{
			if (desired.tonemapper != submitted.tonemapper || !hasSentAll)
			{
				SendInt(api, "rtx.tonemappingMode", desired.tonemapper);
				submitted.tonemapper = desired.tonemapper;
			}

			SendExposureRange(api);
			SendFloatIfChanged(api, "rtx.autoExposure.autoExposureSpeed", desired.exposureSpeed, submitted.exposureSpeed);

			SendFloatIfChanged(api, "rtx.localtonemap.exposure", localExposureNow, submitted.localExposure);
			SendFloatIfChanged(api, "rtx.localtonemap.shadows", desired.localShadows, submitted.localShadows);
			SendFloatIfChanged(api, "rtx.localtonemap.highlights", desired.localHighlights, submitted.localHighlights);
			SendBoolIfChanged(api, "rtx.localtonemap.boostLocalContrast", desired.isLocalContrastBoosted,
				submitted.isLocalContrastBoosted);
			SendBoolIfChanged(api, "rtx.localtonemap.finalizeWithACES", desired.isLocalAces, submitted.isLocalAces);
		}

		SendFloatIfChanged(api, "rtx.tonemap.exposureBias", exposureBiasNow, submitted.exposureBiasEv);

		static bool isMicromapSent = false;
		SendBoolIfChanged(api, "rtx.opacityMicromap.enable", desired.isOpacityMicromaps, isMicromapSent);
		static bool isCullingSent = true;
		SendBoolIfChanged(api, "rtx.enableCulling", desired.isBackfaceCulling, isCullingSent);

		if (isPlus)
		{
			static bool isSparseSent = false;
			static float sentSparseRate = 1.0f;
			SendBoolIfChanged(api, "rtx.sparseRendering.enableSparseRendering", desired.isSparseRendering, isSparseSent);
			SendFloatIfChanged(api, "rtx.sparseRendering.samplingRate", std::clamp(desired.sparseRate, 0.25f, 1.0f),
				sentSparseRate);
		}
		const auto grade = [](float value) { return std::round(value * 200.0f) / 200.0f; };
		SendBoolIfChanged(api, "rtx.tonemap.colorGradingEnabled", desired.isColourGrading || isZoneGrading,
			submitted.isColourGrading);
		SendFloatIfChanged(api, "rtx.tonemap.contrast", grade(zoneValues.contrast), submitted.contrast);
		SendFloatIfChanged(api, "rtx.tonemap.saturation", grade(zoneValues.saturation), submitted.saturation);
		static bool isToneCurveSent = false;

		if (isPlus)
		{
			// Remix Plus removed the per-frame tone curve; this flag now switches the whole operator off.
			SendBoolIfChanged(api, "rtx.tonemap.tonemappingEnabled", true, isToneCurveSent);
		}
		else
		{
			SendBoolIfChanged(api, "rtx.tonemap.finalizeWithACES", desired.isGlobalAces, submitted.isGlobalAces);
			SendBoolIfChanged(api, "rtx.tonemap.tonemappingEnabled", !desired.isSteadyToneCurve, isToneCurveSent);

			if (!hasSentAll)
			{
				SendInt(api, "rtx.autoExposure.exposureAverageMode", 1);
			}
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
			// SHARC (3) exists only in Remix Plus; a map saved with it falls back to ReSTIR GI on stock.
			static constexpr int sharcMode = 3;
			static constexpr int restirGiMode = 1;
			SendInt(api, "rtx.integrateIndirectMode",
				desired.indirectMode == sharcMode && !IsRemixPlus() ? restirGiMode : desired.indirectMode);
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

		// On Remix Plus the shader wets each surface itself (GetPlusWetnessState), only where rain reaches.
		const bool isShaderWetness = GetPlusWetnessState().modifier != 0;

		if (IsWetActive() && !isShaderWetness)
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

		SetPuddlesActive(IsWetActive() && desired.isPuddles && !isShaderWetness);
		SetFixtureGlow(desired.isFixtureLights, desired.fixtureBrightness);

		WeaponLook weapon;
		weapon.isDepth = desired.isWeaponDepth;
		weapon.roughnessScale = desired.weaponRoughness;
		weapon.metalBoost = desired.weaponMetal;
		weapon.normalStrength = desired.weaponNormal;
		SetWeaponLook(weapon);

		int displacementMode = 0;

		// Height maps only show with Remix's displacement on; weapon depth turns it on for the weapon's own maps.
		if (desired.isParallax || desired.isWeaponDepth)
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

	bool IsAnyRain()
	{
		if (IsRaining())
		{
			return true;
		}

		const PlusWeatherPreset* preset = GetPlusWeather();
		return preset && IsRainPreset(*preset);
	}

	bool IsWetActive()
	{
		if (desired.wetMode == 2)
		{
			return true;
		}

		return desired.wetMode == 1 && IsAnyRain();
	}

	// Packed exactly as the GTA4 mod's set_remix_roughness_scalar (renderer.cpp) and decoded by the ported shader:
	// low 16 bits = roughness scalar + 2 over 0..4 in 6 bits, max z (wet above this normal z) and blend width in 5 bits
	// each; high 16 bits = raindrop scale over 0..10 in 8 bits, then the wetness flags. Values are the mod's world
	// surface defaults (comp_settings.hpp timecycle_wetness_world_*); scalar 0 is fully wet.
	PlusWetnessState GetPlusWetnessState()
	{
		static constexpr std::uint32_t roughnessScalarModifier = 1u << 1;
		static constexpr std::uint8_t flagVariation = 1u << 0;
		static constexpr std::uint8_t flagRaindrops = 1u << 1;
		static constexpr std::uint8_t flagPuddleLayer = 1u << 5;
		static constexpr std::uint8_t flagOcclusionTest = 1u << 6;
		static constexpr std::uint8_t flagOcclusionSmoothing = 1u << 7;
		static constexpr float wetScalar = 0.0f;
		static constexpr float maxZ = 0.30f;
		static constexpr float blendWidth = 0.65f;
		static constexpr float raindropScale = 0.23f;

		if (!IsRemixPlus() || !IsWetActive())
		{
			return {};
		}

		const auto encode = [](float value, int bits, float range)
		{
			const int maximum = (1 << bits) - 1;
			return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, range) / range * static_cast<float>(maximum)));
		};

		std::uint8_t flags = flagVariation | flagOcclusionTest | flagOcclusionSmoothing;

		if (desired.isPuddles)
		{
			flags |= flagPuddleLayer;
		}

		if (IsAnyRain())
		{
			flags |= flagRaindrops;
		}

		const std::uint32_t params1 = (encode(wetScalar + 2.0f, 6, 4.0f) << 10) | (encode(maxZ, 5, 1.0f) << 5)
			| encode(blendWidth, 5, 1.0f);
		const std::uint32_t params2 = encode(raindropScale, 8, 10.0f) | (static_cast<std::uint32_t>(flags) << 8);

		return { roughnessScalarModifier, (params2 << 16) | params1 };
	}
}
