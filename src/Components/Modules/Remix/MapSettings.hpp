#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Options.hpp"
#include "TextureTags.hpp"

namespace Components::Remix
{
	struct MapSunOverride
	{
		std::optional<float> brightness;
		std::optional<float> maxRadiance;
		bool hasTint = false;
		float tint[3] = { 1.0f, 1.0f, 1.0f };
		bool hasDirection = false;
		float yawDeg = 0.0f;
		float pitchDeg = 45.0f;
		// The engine's sun colour (GfxLight colour of the map's sun), for a map whose zone has no sun. With a direction
		// it gives the map a sun.
		bool hasColour = false;
		float colour[3] = { 1.0f, 1.0f, 1.0f };
	};

	// A primary light as the engine stores it (ComPrimaryLight, copied unchanged into GfxLight by R_InitPrimaryLights),
	// for converted maps whose zone lost its lights. They go through the same conversion as the zone's own lights.
	struct PrimaryLight
	{
		std::uint8_t type = 0;
		float colour[3] = {};
		float direction[3] = {};
		float origin[3] = {};
		float radius = 0.0f;
		float cosOuter = 0.0f;
		float cosInner = 0.0f;
	};

	struct LightKeyframe
	{
		float timeSeconds = 0.0f;
		float intensity = 1.0f;
		bool hasColour = false;
		float colour[3] = { 1.0f, 1.0f, 1.0f };
	};

	struct AuthoredLight
	{
		float position[3] = {};
		float colour[3] = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		float radius = 4.0f;
		bool hasDirection = false;
		float direction[3] = { 0.0f, 0.0f, -1.0f };
		float coneDeg = 45.0f;
		float softness = 0.1f;
		float focus = 0.0f;
		float volumetric = 1.0f;
		bool isLooped = true;
		std::vector<LightKeyframe> keyframes;
		std::string trigger;
		std::string model;
	};

	struct LightOverride
	{
		std::uint64_t hash = 0;
		std::optional<float> brightness;
		bool hasColour = false;
		float colour[3] = { 1.0f, 1.0f, 1.0f };
		std::optional<float> radius;
		std::optional<float> coneDeg;
		std::optional<float> softness;
		std::optional<float> focus;
		std::optional<float> volumetric;
	};

	enum class TriggerEvent
	{
		Vision,
		Sound,
		Indoors,
		Outdoors,
		Area,
	};

	struct TriggerRule
	{
		std::string name;
		TriggerEvent event = TriggerEvent::Sound;
		std::string match;
		float holdSeconds = 5.0f;
		float blendSeconds = 1.0f;
		bool hasBox = false;
		float boxMin[3] = {};
		float boxMax[3] = {};
		bool hasSphere = false;
		float centre[3] = {};
		float radius = 0.0f;
		OptionList remixOptions;
		bool isGlobal = false;
	};

	struct MapMarker
	{
		int id = 0;
		float position[3] = {};
		float rotationDeg[3] = {};
		float scale[3] = { 1.0f, 1.0f, 1.0f };
		float maxDistance = 0.0f;
		std::optional<bool> whenRaining;
		std::string trigger;
	};

	struct CullingArea
	{
		float boxMin[3] = {};
		float boxMax[3] = {};
		float radius = 0.0f;
	};

	struct MapZone
	{
		std::string name;
		std::vector<int> cells;
		bool hasBox = false;
		float boxMin[3] = {};
		float boxMax[3] = {};
		std::optional<float> exposureEv;
		std::optional<float> shadowLift;
		std::optional<float> effectGlow;
		std::optional<float> lampBrightness;
		bool hasLampColour = false;
		float lampColour[3] = { 1.0f, 0.9f, 0.75f };
		std::optional<float> contrast;
		std::optional<float> saturation;
		bool hasTint = false;
		float tint[3] = { 1.0f, 1.0f, 1.0f };
		float blendSeconds = 1.0f;
	};

	struct MapSettings
	{
		std::string mapName;
		bool hasFile = false;
		std::string loadError;

		MapSunOverride sun;
		std::optional<bool> isFogEnabled;
		std::optional<float> fogStrength;
		std::optional<int> cullingLevel;
		std::optional<float> cullingRadius;
		std::optional<float> propDistanceScale;
		std::vector<std::string> keptModels;
		std::vector<CullingArea> cullingAreas;

		OptionList remixOptions;
		float remixBlendSeconds = 0.0f;
		std::vector<std::pair<std::string, std::string>> dvars;

		std::vector<std::uint64_t> ignoredLights;
		std::vector<LightOverride> lightOverrides;
		std::vector<PrimaryLight> primaryLights;
		std::vector<AuthoredLight> addedLights;
		std::vector<AuthoredLight> attachedLights;

		std::vector<MapMarker> markers;
		std::vector<TriggerRule> triggers;

		std::vector<std::pair<std::uint64_t, TextureTag>> textureTags;
		std::vector<std::string> cutoutMaterials;
		std::vector<MapZone> zones;
	};

	void UpdateMapSettings(IDirect3DDevice9* device);

	const MapSettings& GetMapSettings();

	unsigned GetMapSettingsVersion();

	void ReloadMapSettings();

	std::wstring GetMapSettingsPath(const std::string& mapName);
}
