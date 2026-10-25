#pragma once

#include <cstdint>
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

		// Area exposure (AreaExposure.hpp): the auto exposure follows the light of where you stand, not where you look.
		bool isAreaExposure = true;
		float areaExposureStrength = 0.7f;
		float areaExposureSunlitEv = 1.0f;
		float areaExposureWindowEv = 0.75f;

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

		// Remix Plus replaced the tonemapper: an operator list and its own auto exposure. Defaults are the fork's.
		int plusOperator = 6;
		float plusExposureTarget = 0.18f;
		float plusShadows = 1.0f;
		float plusHighlights = 1.0f;

		// Remix Plus's Numos atmosphere in place of the map's skybox, lit by the map's sun. Off by default: it replaces
		// the skybox outright, so night and painted skies stay as they are unless a map turns it on.
		bool isPhysicalSky = false;

		// With the physical sky, light the map with zw3's own sun (the map's direction, colour and shadows) instead of
		// the fork's sun, which its clouds shade. The sky still follows this sun.
		bool isPhysicalSkyMapSun = false;

		// Remix Plus weather preset: 0 = auto (rain where the map rains), 1 = off, 2 and up = plusWeatherPresets[n - 2].
		int plusWeather = 0;

		// Remix Plus's own GPU rain particles in place of zw3's streaks. Off by default: at IW4's inch scale its
		// centimetre sized drops come out under a pixel wide (rtx_precipitation.h dropWidth 0.35).
		bool isPlusRainParticles = false;

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
		bool isScreenDrops = false;
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
		// rtx.enableCulling: Remix skips the back of one-sided opaque surfaces, as the game does.
		bool isBackfaceCulling = true;

		// Remix Plus sparse rendering (rtx_sparse_rendering.h): lights only this share of pixels each frame and lets DLSS
		// Ray Reconstruction fill the rest. Needs Ray Reconstruction.
		bool isSparseRendering = false;
		float sparseRate = 0.5f;
		bool isSkyReprojected = false;

		// First person weapon and arms only (PbrMaterials WeaponLook).
		bool isWeaponDepth = false;
		float weaponRoughness = 1.0f;
		float weaponMetal = 0.0f;
		float weaponNormal = 1.0f;
	};

	void SetLookSettings(const LookSettings& settings);

	LookSettings GetLookSettings();

	void UpdateLook(IDirect3DDevice9* device);

	bool IsWetActive();

	// Remix Plus wetness: render state 149 (modifier bits) and 210 (packed wetness) for a world or model draw, both 0
	// when the world is dry or the runtime is stock. zw3's fork build reads them per draw (port of the GTA4 branch).
	struct PlusWetnessState
	{
		std::uint32_t modifier = 0;
		std::uint32_t packed = 0;
	};

	PlusWetnessState GetPlusWetnessState();

	struct PlusWeatherPreset
	{
		const char* name;
		const char* label;
		bool hasPrecipitation;
	};

	// Remix Plus's weather presets (kPresetDescs in rtx_weather.cpp). Precipitation follows each preset's
	// precipitationIntensity in rtx_weather.h; foggy's light drizzle counts.
	inline constexpr PlusWeatherPreset plusWeatherPresets[] = {
		{ "clear", "Clear", false },
		{ "partlyCloudy", "Partly cloudy", false },
		{ "overcast", "Overcast", false },
		{ "hazy", "Hazy", false },
		{ "foggy", "Foggy", true },
		{ "drizzle", "Drizzle", true },
		{ "rainstorm", "Rainstorm", true },
		{ "thunderstorm", "Thunderstorm", true },
		{ "snow", "Snow", true },
		{ "blizzard", "Blizzard", true },
		{ "sandstorm", "Sandstorm", true },
		{ "smoggy", "Smoggy", false },
	};

	// The Remix Plus preset zw3 wants now, or null for none (the fork's own atmosphere settings).
	const PlusWeatherPreset* GetPlusWeather();

	// True while Remix Plus draws rain or snow itself, so the game's rain and zw3's own rain stay off.
	bool IsPlusPrecipitationActive();

	// The map's own rain, or a Remix Plus weather preset that rains (not snow or sand).
	bool IsAnyRain();

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
