#pragma once

#include <string>

namespace Components::Remix
{
	struct LookSettings
	{
		int tonemapper = 0;

		bool isAutoExposure = false;
		float exposureMinEv = -1.0f;
		float exposureMaxEv = 2.0f;
		float exposureSpeed = 1.0f;

		float localExposure = 0.75f;
		float localShadows = 2.0f;
		float localHighlights = 4.0f;
		bool isLocalContrastBoosted = false;
		bool isLocalAces = true;

		float exposureBiasEv = 0.0f;
		bool isColourGrading = false;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float colourBalance[3] = { 1.0f, 1.0f, 1.0f };
		bool isGlobalAces = true;
		bool isSteadyToneCurve = true;

		bool isBloom = true;
		float bloomIntensity = 1.0f;
		float bloomThreshold = 0.25f;

		bool isVignette = false;
		float vignetteIntensity = 0.8f;
		float vignetteRadius = 0.8f;
		float vignetteSoftness = 0.2f;

		bool isChromaticAberration = false;
		float chromaticAmount = 0.02f;

		bool isMotionBlur = false;

		bool isHaze = false;
		bool isWhiteMaterial = false;

		int indirectMode = 0;
		int maxBounces = 2;
		bool isBounceLight = true;
		bool isGlassShadows = false;
		bool isSkinScattering = false;

		float albedoScale = 1.0f;
		float roughnessScale = 1.0f;
		float roughnessBias = 0.0f;
		float metallicScale = 1.0f;
		float normalIntensity = 2.0f;

		bool isNeonCacheOff = true;

		bool isPlayerShadowCopies = true;
		bool isPlayerVirtualInstances = true;
		bool isPhotoMode = false;
		bool isPhotoAccumulate = true;
		bool isPhotoLocked = false;
		bool isLaserLights = false;
		bool isFixtureLights = true;
		float fixtureBrightness = 1.0f;
		float fixtureOffset = 0.0f;
		float fixtureSize = 4.0f;
		bool isPuddles = false;
		bool isParallax = false;
		float parallaxDepth = 1.0f;
		int wetMode = 0;
		bool isRainEverywhere = false;
		float rainAmount = 1.0f;
		bool isScreenDrops = true;
		bool isTimeOfDay = false;
		float timeOfDayHours = 17.5f;
		bool isNight = false;
		float nightSky = 0.03f;
		bool isGodRays = false;
		float godRayStrength = 2.0f;
		bool isMirror = false;
		float mirrorMetal = 0.0f;
		float effectGlow = 0.2f;
		float shadowLift = 0.0f;
		float eyeGlowBrightness = 1.0f;
		float eyeGlowSize = 1.0f;
		bool isMapZones = true;
		bool isSteadyTransparency = true;
		float outdoorEv = 0.0f;
		bool isOpacityMicromaps = false;
		bool isSkyReprojected = false;
	};

	void SetLookSettings(const LookSettings& settings);

	LookSettings GetLookSettings();

	void UpdateLook(IDirect3DDevice9* device);

	bool IsWetActive();

	struct OutdoorExposureState
	{
		bool hasOutdoorMask;
		bool isOutdoors;
		float blend;
	};

	OutdoorExposureState GetOutdoorExposureState();

	struct ZoneState
	{
		int cell;
		std::string zoneName;
		float exposureEv;
		float shadowLift;
		float effectGlow;
		float contrast;
		float saturation;
		float tint[3];
		bool isGrading;
	};

	ZoneState GetZoneState();

	float GetEffectiveShadowLift();
}
