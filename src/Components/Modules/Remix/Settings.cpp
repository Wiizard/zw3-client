#include "STDInclude.hpp"

#include "Settings.hpp"

#include "Engine.hpp"
#include "Culling.hpp"
#include "FixedFunction.hpp"
#include "Fog.hpp"
#include "Lights.hpp"
#include "Look.hpp"
#include "MotionTracker.hpp"
#include "Options.hpp"
#include "Outdoor.hpp"
#include "PbrMaterials.hpp"
#include "Scene.hpp"
#include "Sky.hpp"
#include "TextureTags.hpp"

namespace Components::Remix
{
	static const EffectKind effectKinds[effectKindCount] = {
		EffectKind::CodeMesh,
		EffectKind::Marks,
		EffectKind::Glass,
		EffectKind::ParticleClouds,
		EffectKind::Sparks,
		EffectKind::BulletTracers,
	};

	static bool Core::Config::* const effectMembers[effectKindCount] = {
		&Core::Config::remixDrawEffects,
		&Core::Config::remixDrawDecals,
		&Core::Config::remixDrawGlass,
		&Core::Config::remixDrawClouds,
		&Core::Config::remixDrawSparks,
		&Core::Config::remixDrawTracers,
	};

	static bool hasApplied = false;

	static LookSettings LookFromConfig(const Core::Config& config)
	{
		LookSettings look;
		look.tonemapper = config.remixTonemapper;
		look.isAutoExposure = config.remixAutoExposure;
		look.exposureMinEv = config.remixExposureMinEv;
		look.exposureMaxEv = config.remixExposureMaxEv;
		look.exposureSpeed = config.remixExposureSpeed;
		look.isAreaExposure = config.remixAreaExposure;
		look.areaExposureStrength = config.remixAreaExposureStrength;
		look.areaExposureSunlitEv = config.remixAreaExposureSunlit;
		look.areaExposureWindowEv = config.remixAreaExposureWindow;
		look.localExposure = config.remixLocalExposure;
		look.localShadows = config.remixLocalShadows;
		look.localHighlights = config.remixLocalHighlights;
		look.isLocalContrastBoosted = config.remixLocalContrastBoosted;
		look.isLocalAces = config.remixLocalAces;
		look.exposureBiasEv = config.remixExposureBiasEv;
		look.isColourGrading = config.remixColourGrading;
		look.contrast = config.remixContrast;
		look.saturation = config.remixSaturation;
		look.colourBalance[0] = config.remixColourBalanceR;
		look.colourBalance[1] = config.remixColourBalanceG;
		look.colourBalance[2] = config.remixColourBalanceB;
		look.isGlobalAces = config.remixGlobalAces;
		look.isSteadyToneCurve = config.remixSteadyToneCurve;
		look.plusOperator = config.remixPlusOperator;
		look.plusExposureTarget = config.remixPlusExposureTarget;
		look.plusShadows = config.remixPlusShadows;
		look.plusHighlights = config.remixPlusHighlights;
		look.isPhysicalSky = config.remixPhysicalSky;
		look.isPhysicalSkyMapSun = config.remixPhysicalSkyMapSun;
		look.plusWeather = config.remixPlusWeather;
		look.isPlusRainParticles = config.remixPlusRainParticles;
		look.isBloom = config.remixBloom;
		look.bloomIntensity = config.remixBloomIntensity;
		look.bloomThreshold = config.remixBloomThreshold;
		look.isVignette = config.remixVignette;
		look.vignetteIntensity = config.remixVignetteIntensity;
		look.vignetteRadius = config.remixVignetteRadius;
		look.vignetteSoftness = config.remixVignetteSoftness;
		look.isChromaticAberration = config.remixChromaticAberration;
		look.chromaticAmount = config.remixChromaticAmount;
		look.isMotionBlur = config.remixMotionBlur;
		look.isHaze = config.remixHaze;
		look.isWhiteMaterial = config.remixWhiteMaterial;
		look.indirectMode = config.remixIndirectMode;
		look.maxBounces = config.remixMaxBounces;
		look.isBounceLight = config.remixBounceLight;
		look.isGlassShadows = config.remixGlassShadows;
		look.isSkinScattering = config.remixSkinScattering;
		look.albedoScale = config.remixAlbedoScale;
		look.roughnessScale = config.remixRoughnessScale;
		look.roughnessBias = config.remixRoughnessBias;
		look.metallicScale = config.remixMetallicScale;
		look.normalIntensity = config.remixNormalIntensity;
		look.isNeonCacheOff = config.remixNeonCacheOff;
		look.isPhotoMode = config.remixPhotoMode;
		look.isPhotoAccumulate = config.remixPhotoAccumulate;
		look.isPhotoLocked = config.remixPhotoLocked;
		look.isLaserLights = config.remixLaserLights;
		look.isFixtureLights = config.remixFixtureLights;
		look.fixtureBrightness = config.remixFixtureBrightness;
		look.fixtureOffset = config.remixFixtureOffset;
		look.fixtureSize = config.remixFixtureSize;
		look.isPuddles = config.remixPuddles;
		look.isParallax = config.remixParallax;
		look.parallaxDepth = config.remixParallaxDepth;
		look.wetMode = config.remixWetMode;
		look.isRainEverywhere = config.remixRainEverywhere;
		look.rainAmount = config.remixRainAmount;
		look.isScreenDrops = config.remixScreenDrops;
		look.isTimeOfDay = config.remixTimeOfDay;
		look.timeOfDayHours = config.remixTimeOfDayHours;
		look.isNight = config.remixNight;
		look.nightSky = config.remixNightSky;
		look.isGodRays = config.remixGodRays;
		look.godRayStrength = config.remixGodRayStrength;
		look.isMirror = config.remixMirror;
		look.mirrorMetal = config.remixMirrorMetal;
		look.effectGlow = config.remixEffectGlow;
		look.shadowLift = config.remixShadowLift;
		look.eyeGlowBrightness = config.remixEyeGlowBrightness;
		look.eyeGlowSize = config.remixEyeGlowSize;
		look.isMapZones = config.remixMapZones;
		look.isSteadyTransparency = config.remixSteadyTransparency;
		look.outdoorEv = config.remixOutdoorEv;
		look.isOpacityMicromaps = config.remixOpacityMicromaps;
		look.isBackfaceCulling = config.remixBackfaceCulling;
		look.isSparseRendering = config.remixSparseRendering;
		look.sparseRate = config.remixSparseRate;
		look.isSkyReprojected = config.remixSkyReprojected;
		look.isWeaponDepth = config.remixWeaponDepth;
		look.weaponRoughness = config.remixWeaponRoughness;
		look.weaponMetal = config.remixWeaponMetal;
		look.weaponNormal = config.remixWeaponNormal;
		return look;
	}

	static void LookToConfig(const LookSettings& look, Core::Config& config)
	{
		config.remixTonemapper = look.tonemapper;
		config.remixAutoExposure = look.isAutoExposure;
		config.remixExposureMinEv = look.exposureMinEv;
		config.remixExposureMaxEv = look.exposureMaxEv;
		config.remixExposureSpeed = look.exposureSpeed;
		config.remixAreaExposure = look.isAreaExposure;
		config.remixAreaExposureStrength = look.areaExposureStrength;
		config.remixAreaExposureSunlit = look.areaExposureSunlitEv;
		config.remixAreaExposureWindow = look.areaExposureWindowEv;
		config.remixLocalExposure = look.localExposure;
		config.remixLocalShadows = look.localShadows;
		config.remixLocalHighlights = look.localHighlights;
		config.remixLocalContrastBoosted = look.isLocalContrastBoosted;
		config.remixLocalAces = look.isLocalAces;
		config.remixExposureBiasEv = look.exposureBiasEv;
		config.remixColourGrading = look.isColourGrading;
		config.remixContrast = look.contrast;
		config.remixSaturation = look.saturation;
		config.remixColourBalanceR = look.colourBalance[0];
		config.remixColourBalanceG = look.colourBalance[1];
		config.remixColourBalanceB = look.colourBalance[2];
		config.remixGlobalAces = look.isGlobalAces;
		config.remixSteadyToneCurve = look.isSteadyToneCurve;
		config.remixPlusOperator = look.plusOperator;
		config.remixPlusExposureTarget = look.plusExposureTarget;
		config.remixPlusShadows = look.plusShadows;
		config.remixPlusHighlights = look.plusHighlights;
		config.remixPhysicalSky = look.isPhysicalSky;
		config.remixPhysicalSkyMapSun = look.isPhysicalSkyMapSun;
		config.remixPlusWeather = look.plusWeather;
		config.remixPlusRainParticles = look.isPlusRainParticles;
		config.remixBloom = look.isBloom;
		config.remixBloomIntensity = look.bloomIntensity;
		config.remixBloomThreshold = look.bloomThreshold;
		config.remixVignette = look.isVignette;
		config.remixVignetteIntensity = look.vignetteIntensity;
		config.remixVignetteRadius = look.vignetteRadius;
		config.remixVignetteSoftness = look.vignetteSoftness;
		config.remixChromaticAberration = look.isChromaticAberration;
		config.remixChromaticAmount = look.chromaticAmount;
		config.remixMotionBlur = look.isMotionBlur;
		config.remixHaze = look.isHaze;
		config.remixWhiteMaterial = look.isWhiteMaterial;
		config.remixIndirectMode = look.indirectMode;
		config.remixMaxBounces = look.maxBounces;
		config.remixBounceLight = look.isBounceLight;
		config.remixGlassShadows = look.isGlassShadows;
		config.remixSkinScattering = look.isSkinScattering;
		config.remixAlbedoScale = look.albedoScale;
		config.remixRoughnessScale = look.roughnessScale;
		config.remixRoughnessBias = look.roughnessBias;
		config.remixMetallicScale = look.metallicScale;
		config.remixNormalIntensity = look.normalIntensity;
		config.remixNeonCacheOff = look.isNeonCacheOff;
		config.remixPhotoMode = look.isPhotoMode;
		config.remixPhotoAccumulate = look.isPhotoAccumulate;
		config.remixPhotoLocked = look.isPhotoLocked;
		config.remixLaserLights = look.isLaserLights;
		config.remixFixtureLights = look.isFixtureLights;
		config.remixFixtureBrightness = look.fixtureBrightness;
		config.remixFixtureOffset = look.fixtureOffset;
		config.remixFixtureSize = look.fixtureSize;
		config.remixPuddles = look.isPuddles;
		config.remixParallax = look.isParallax;
		config.remixParallaxDepth = look.parallaxDepth;
		config.remixWetMode = look.wetMode;
		config.remixRainEverywhere = look.isRainEverywhere;
		config.remixRainAmount = look.rainAmount;
		config.remixScreenDrops = look.isScreenDrops;
		config.remixTimeOfDay = look.isTimeOfDay;
		config.remixTimeOfDayHours = look.timeOfDayHours;
		config.remixNight = look.isNight;
		config.remixNightSky = look.nightSky;
		config.remixGodRays = look.isGodRays;
		config.remixGodRayStrength = look.godRayStrength;
		config.remixMirror = look.isMirror;
		config.remixMirrorMetal = look.mirrorMetal;
		config.remixEffectGlow = look.effectGlow;
		config.remixShadowLift = look.shadowLift;
		config.remixEyeGlowBrightness = look.eyeGlowBrightness;
		config.remixEyeGlowSize = look.eyeGlowSize;
		config.remixMapZones = look.isMapZones;
		config.remixSteadyTransparency = look.isSteadyTransparency;
		config.remixOutdoorEv = look.outdoorEv;
		config.remixOpacityMicromaps = look.isOpacityMicromaps;
		config.remixBackfaceCulling = look.isBackfaceCulling;
		config.remixSparseRendering = look.isSparseRendering;
		config.remixSparseRate = look.sparseRate;
		config.remixSkyReprojected = look.isSkyReprojected;
		config.remixWeaponDepth = look.isWeaponDepth;
		config.remixWeaponRoughness = look.weaponRoughness;
		config.remixWeaponMetal = look.weaponMetal;
		config.remixWeaponNormal = look.weaponNormal;
	}

	void ApplyConfig(const Core::Config& config)
	{
		const FogStats fogStats = GetFogStats();

		if (fogStats.isEnabled != config.remixMapFog)
		{
			SetMapFogEnabled(config.remixMapFog);
		}

		if (fogStats.strength != config.remixFogStrength)
		{
			SetMapFogStrength(config.remixFogStrength);
		}

		SetGlowBrightness(config.remixGlowBrightness);
		SetLampFlaresHidden(config.remixHideLampFlares);

		SetLightBrightness(config.remixLightBrightness);

		LightTuning tuning;
		tuning.spotBrightness = config.remixSpotBrightness;
		tuning.omniBrightness = config.remixOmniBrightness;
		tuning.sphereRadius = config.remixLightSize;
		tuning.coneOffsetDeg = config.remixConeOffset;
		tuning.softnessOffset = config.remixConeSoftness;
		tuning.focusExponent = config.remixLightFocus;
		tuning.volumetric = config.remixLightVolumetric;
		tuning.dimLightCutoff = config.remixDimLightCutoff;
		SetLightTuning(tuning);
		SetTrackerBrightness(config.remixTrackerBrightness);

		SunSettings sun = GetSunSettings();
		sun.brightness = config.remixSunBrightness;
		sun.isCapped = config.remixSunCapped;
		sun.maxRadiance = config.remixSunCap;
		sun.tint[0] = config.remixSunTintR;
		sun.tint[1] = config.remixSunTintG;
		sun.tint[2] = config.remixSunTintB;
		sun.isDirectionOverridden = config.remixSunOverridden;
		sun.yawDeg = config.remixSunYawDeg;
		sun.pitchDeg = config.remixSunHeightDeg;
		SetSunSettings(sun);

		SetWeatherEnabled(config.remixWeather);
		SetOffscreenWorldEnabled(config.remixOffscreenWorld);
		SetSharedDecalTags(config.remixSharedDecals);

		if (GetOutdoorStats().isEnabled != config.remixRainOutdoorsOnly)
		{
			SetRainMaskEnabled(config.remixRainOutdoorsOnly);
		}

		if (IsEffectCullingDisabled() != config.remixEffectsOffscreen)
		{
			SetEffectCullingDisabled(config.remixEffectsOffscreen);
		}

		SetAllEffectLights(config.remixAllEffectLights);

		if (IsEntityCullingDisabled() != config.remixEntitiesOffscreen)
		{
			SetEntityCullingDisabled(config.remixEntitiesOffscreen);
		}

		if (IsSteadyModelDetail() != config.remixSteadyModelDetail)
		{
			SetSteadyModelDetail(config.remixSteadyModelDetail);
		}

		if (IsPlayerBodyShadow() != config.remixPlayerBodyShadow)
		{
			SetPlayerBodyShadow(config.remixPlayerBodyShadow);
		}

		if (static_cast<int>(GetCullingStats().level) != config.remixViewCulling
			&& config.remixViewCulling >= 0 && config.remixViewCulling < cullingLevelCount)
		{
			TrySetCullingLevel(static_cast<CullingLevel>(config.remixViewCulling));
		}

		SetDiSampleCount(config.remixDiSamples);
		SetDrawDistance(config.remixDrawDistance);

		CullingTuning cullingTuning;
		cullingTuning.radius = config.remixCullingRadius;
		cullingTuning.propDistanceScale = config.remixPropDistanceScale;
		SetCullingTuning(cullingTuning);

		if (IsMapColourGrade() != config.remixMapGrade)
		{
			SetMapColourGrade(config.remixMapGrade);
		}

		if (GetMapColourGradeStrength() != config.remixMapGradeStrength)
		{
			SetMapColourGradeStrength(config.remixMapGradeStrength);
		}

		for (int i = 0; i < effectKindCount; ++i)
		{
			const bool isDrawn = config.*effectMembers[i];

			if (IsEffectDrawn(effectKinds[i]) != isDrawn)
			{
				TrySetEffectDrawn(effectKinds[i], isDrawn);
			}
		}

		SetSkyScale(config.remixSkyScale);
		const LookSettings current = GetLookSettings();
		LookSettings look = LookFromConfig(config);
		look.isPlayerShadowCopies = current.isPlayerShadowCopies;
		look.isPlayerVirtualInstances = current.isPlayerVirtualInstances;
		SetLookSettings(look);
		SetNeonFoliage(config.remixNeonFoliage, config.remixNeonIntensity);

		NeonPulse pulse;
		pulse.isEnabled = config.remixNeonPulse;
		pulse.speed = config.remixNeonPulseSpeed;
		pulse.depth = config.remixNeonPulseDepth;
		SetNeonPulse(pulse);

		AlienGlow alien;
		alien.isEnabled = config.remixAlien;
		alien.style = config.remixAlienStyle;
		alien.speed = config.remixAlienSpeed;
		alien.depth = config.remixAlienDepth;
		alien.scale = config.remixAlienScale;
		alien.colourMode = config.remixAlienColours;
		alien.tint[0] = config.remixAlienTintR;
		alien.tint[1] = config.remixAlienTintG;
		alien.tint[2] = config.remixAlienTintB;
		SetAlienGlow(alien);

		hasApplied = true;
	}

	void CaptureConfig(Core::Config& config)
	{
		if (!hasApplied)
		{
			return;
		}

		const FogStats fogStats = GetFogStats();
		config.remixMapFog = fogStats.isEnabled;
		config.remixFogStrength = fogStats.strength;
		config.remixGlowBrightness = GetGlowBrightness();
		config.remixHideLampFlares = AreLampFlaresHidden();
		config.remixLightBrightness = GetLightBrightness();

		const LightTuning tuning = GetLightTuning();
		config.remixSpotBrightness = tuning.spotBrightness;
		config.remixOmniBrightness = tuning.omniBrightness;
		config.remixLightSize = tuning.sphereRadius;
		config.remixConeOffset = tuning.coneOffsetDeg;
		config.remixConeSoftness = tuning.softnessOffset;
		config.remixLightFocus = tuning.focusExponent;
		config.remixLightVolumetric = tuning.volumetric;
		config.remixDimLightCutoff = tuning.dimLightCutoff;
		config.remixTrackerBrightness = GetTrackerBrightness();

		const SunSettings sun = GetSunSettings();
		config.remixSunBrightness = sun.brightness;
		config.remixSunCapped = sun.isCapped;
		config.remixSunCap = sun.maxRadiance;
		config.remixSunTintR = sun.tint[0];
		config.remixSunTintG = sun.tint[1];
		config.remixSunTintB = sun.tint[2];
		config.remixSunOverridden = sun.isDirectionOverridden;
		config.remixSunYawDeg = sun.yawDeg;
		config.remixSunHeightDeg = sun.pitchDeg;

		config.remixWeather = IsWeatherEnabled();
		config.remixOffscreenWorld = IsOffscreenWorldEnabled();
		config.remixSharedDecals = IsSharedDecalTags();
		config.remixRainOutdoorsOnly = GetOutdoorStats().isEnabled;
		config.remixEffectsOffscreen = IsEffectCullingDisabled();
		config.remixAllEffectLights = IsAllEffectLights();
		config.remixEntitiesOffscreen = IsEntityCullingDisabled();
		config.remixSteadyModelDetail = IsSteadyModelDetail();
		config.remixPlayerBodyShadow = IsPlayerBodyShadow();
		const CullingStats cullingStats = GetCullingStats();

		if (cullingStats.appliedPatches > 0)
		{
			config.remixViewCulling = static_cast<int>(cullingStats.level);
		}
		config.remixDiSamples = GetDiSampleCount();

		const CullingTuning cullingTuning = GetCullingTuning();
		config.remixCullingRadius = cullingTuning.radius;
		config.remixPropDistanceScale = cullingTuning.propDistanceScale;
		config.remixMapGrade = IsMapColourGrade();
		config.remixMapGradeStrength = GetMapColourGradeStrength();

		if (Engine::EngineAddressable())
		{
			for (int i = 0; i < effectKindCount; ++i)
			{
				config.*effectMembers[i] = IsEffectDrawn(effectKinds[i]);
			}
		}

		config.remixSkyScale = GetSkyScale();
		LookToConfig(GetLookSettings(), config);
		config.remixNeonFoliage = IsNeonFoliage();
		config.remixNeonIntensity = GetNeonIntensity();

		const NeonPulse pulse = GetNeonPulse();
		config.remixNeonPulse = pulse.isEnabled;
		config.remixNeonPulseSpeed = pulse.speed;
		config.remixNeonPulseDepth = pulse.depth;

		const AlienGlow alien = GetAlienGlow();
		config.remixAlien = alien.isEnabled;
		config.remixAlienStyle = alien.style;
		config.remixAlienSpeed = alien.speed;
		config.remixAlienDepth = alien.depth;
		config.remixAlienScale = alien.scale;
		config.remixAlienColours = alien.colourMode;
		config.remixAlienTintR = alien.tint[0];
		config.remixAlienTintG = alien.tint[1];
		config.remixAlienTintB = alien.tint[2];
	}
}
