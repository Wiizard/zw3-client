#pragma once

#include <cstdint>
#include <vector>

namespace Components::Remix
{
	struct LightStats
	{
		bool isApiReady = false;
		bool hasSun = false;
		int mapLights = 0;
		int dynamicLights = 0;
		int fixtureLights = 0;
		int laserLights = 0;
		int ignoredMapLights = 0;
		int changedMapLights = 0;
		int authoredLights = 0;
		int animatedLights = 0;
		unsigned long long dynamicLightUpdates = 0;
		float sunRadiance[3] = {};
		float mapSunRadiance[3] = {};
		float sunDirection[3] = {};
		unsigned long long submittedFrames = 0;
	};

	struct SunSettings
	{
		float brightness = 1.0f;
		bool isCapped = false;
		float maxRadiance = 3.0f;
		float tint[3] = { 1.0f, 1.0f, 1.0f };
		bool isDirectionOverridden = false;
		float yawDeg = 0.0f;
		float pitchDeg = 45.0f;
	};

	struct LightTuning
	{
		float spotBrightness = 1.0f;
		float omniBrightness = 1.0f;
		float sphereRadius = 4.0f;
		float coneOffsetDeg = 0.0f;
		float softnessOffset = 0.0f;
		float focusExponent = 0.0f;
		float volumetric = 1.0f;
		float dimLightCutoff = 0.0f;
	};

	struct MapLightInfo
	{
		std::uint64_t hash = 0;
		float origin[3] = {};
		float radius = 0.0f;
		bool isSpot = false;
		bool isIgnored = false;
		bool isChanged = false;
		bool isAuthored = false;
	};

	void SubmitSceneLights(IDirect3DDevice9* device, const void* viewInfo);

	void SetLightBrightness(float brightness);

	void SetAllEffectLights(bool isEnabled);

	bool IsAllEffectLights();

	float GetLightBrightness();

	void SetSunSettings(const SunSettings& settings);

	SunSettings GetSunSettings();

	void ReleaseLights();

	void SetLightTuning(const LightTuning& tuning);

	LightTuning GetLightTuning();

	std::vector<MapLightInfo> GetMapLightInfo();

	LightStats GetLightStats();
}
