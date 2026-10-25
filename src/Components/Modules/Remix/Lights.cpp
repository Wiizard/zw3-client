#include "STDInclude.hpp"

#include "Lights.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "Core.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Api.hpp"
#include "FixedFunction.hpp"
#include "PbrMaterials.hpp"
#include "Look.hpp"
#include "Culling.hpp"
#include "MapSettings.hpp"
#include "Options.hpp"
#include "Read.hpp"
#include "Scene.hpp"
#include "Triggers.hpp"

namespace Components::Remix
{
	static constexpr std::uint64_t sunHash = 0x4D57325F53554E31ull;
	static constexpr std::uint64_t mapLightHashBase = 0x4D57325F4C495431ull;
	static constexpr float sunAngularDiameterDeg = 0.5f;
	static constexpr float noonElevationDeg = 70.0f;
	static constexpr float degreesPerHour = 15.0f;
	static constexpr float minimumTimeOfDayElevationDeg = 2.0f;
	static constexpr float sunsetStartDeg = 8.0f;
	static constexpr float sunsetEndDeg = -2.0f;
	static constexpr float warmBelowDeg = 30.0f;
	static constexpr float sphereRadius = 4.0f;
	static constexpr float minimumLightBrightness = 0.0f;
	static constexpr float maximumLightBrightness = 1000.0f;
	static constexpr float matchDistanceOfRadius = 0.5f;
	static constexpr int maxMapLights = 256;
	static constexpr std::uint64_t dynamicLightHashBase = 0x4D57325F444C5431ull;
	static constexpr int maxDynamicLights = 32;
	static constexpr std::size_t dynamicLightCompareBytes = 0x34;
	static constexpr float dynamicLightBrightness = 0.25f;
	static constexpr std::uint64_t fixtureLightHashBase = 0x4D57325F46495854ull;
	static constexpr int maxFixtureLights = 384;
	static constexpr int maxDrawnFixtureLights = 96;
	static constexpr float maxFixtureLightDistance = 2500.0f;
	static constexpr float fixtureRange = 200.0f;
	static constexpr float fixtureColour[3] = { 1.0f, 0.9f, 0.75f };
	static constexpr std::uint64_t laserLightHashBase = 0x4D57325F4C415352ull;
	static constexpr int maxLaserLights = 16;
	static constexpr float laserBeamRadius = 0.5f;
	static constexpr float laserColour[3] = { 1.0f, 0.12f, 0.08f };
	static constexpr std::uint64_t authoredLightHashBase = 0x4D57325F41555448ull;
	static constexpr std::uint64_t attachedLightHashBase = 0x4D57325F41545448ull;
	static constexpr int maxAuthoredLights = 256;

	static const remixapi_Interface* api = nullptr;

	static unsigned long long lightsCreated = 0;
	static unsigned long long lightsDestroyed = 0;

	static remixapi_ErrorCode CreateCountedLight(const remixapi_LightInfo* info, remixapi_LightHandle* handle)
	{
		const remixapi_ErrorCode result = api->CreateLight(info, handle);

		if (result == REMIXAPI_ERROR_CODE_SUCCESS && *handle)
		{
			++lightsCreated;
		}

		return result;
	}

	static void DestroyCountedLight(remixapi_LightHandle handle)
	{
		api->DestroyLight(handle);
		++lightsDestroyed;
	}

	static remixapi_LightHandle sunLight = nullptr;
	static float submittedSun[7] = {};
	static SunSettings sunSettings;

	static remixapi_LightHandle mapLights[maxMapLights];
	static int mapLightCount = 0;
	static std::vector<MapLightInfo> mapLightInfo;
	static unsigned mapLightsSettingsVersion = 0;
	static unsigned mapLightsTuningVersion = 0;
	static LightTuning tuning;
	static unsigned tuningVersion = 1;
	static std::uintptr_t mapLightsWorld = 0;
	static float mapLightsBrightness = 1.0f;
	static float mapLightsColourScale = 1.0f;
	static float lightBrightness = 1.0f;

	static float settledLightBrightness = 1.0f;
	static float settledFixtureBrightness = 1.0f;
	static float settledFixtureOffset = 0.0f;
	static float settledFixtureSize = sphereRadius;
	static float pendingLightBrightness = 1.0f;
	static float pendingFixtureBrightness = 1.0f;
	static float pendingFixtureOffset = 0.0f;
	static float pendingFixtureSize = sphereRadius;
	static ULONGLONG brightnessChangedMs = 0;
	static constexpr ULONGLONG brightnessSettleMs = 300;

	static void SettleBrightness(const LookSettings& look)
	{
		const ULONGLONG now = GetTickCount64();
		const float fixtureOffset = std::clamp(look.fixtureOffset, 0.0f, 64.0f);
		const float fixtureSize = std::clamp(look.fixtureSize, 0.5f, 64.0f);

		if (lightBrightness != pendingLightBrightness || look.fixtureBrightness != pendingFixtureBrightness
			|| fixtureOffset != pendingFixtureOffset || fixtureSize != pendingFixtureSize)
		{
			pendingLightBrightness = lightBrightness;
			pendingFixtureBrightness = look.fixtureBrightness;
			pendingFixtureOffset = fixtureOffset;
			pendingFixtureSize = fixtureSize;
			brightnessChangedMs = now;
		}

		if (now - brightnessChangedMs >= brightnessSettleMs)
		{
			settledLightBrightness = pendingLightBrightness;
			settledFixtureBrightness = pendingFixtureBrightness;
			settledFixtureOffset = pendingFixtureOffset;
			settledFixtureSize = pendingFixtureSize;
		}
	}
	static bool isAllEffectLights = true;

	static remixapi_LightHandle dynamicLights[maxDynamicLights];
	static unsigned char submittedDynamic[maxDynamicLights][dynamicLightCompareBytes];
	static float submittedDynamicScale[maxDynamicLights];
	static bool isDynamicDrawn[maxDynamicLights];

	static LightStats stats;

	static remixapi_LightHandle fixtureLights[maxFixtureLights];
	static float fixturePositions[maxFixtureLights][3];
	static int fixtureLightCount = 0;
	static unsigned fixtureLightsSpots = 0;
	static unsigned fixtureLightsVersion = 0;
	static float fixtureLightsBrightness = 0.0f;
	static float fixtureLightsOffset = -1.0f;
	static float fixtureLightsSize = -1.0f;

	static remixapi_LightHandle laserLights[maxLaserLights];
	static int laserLightCount = 0;

	struct AuthoredInstance
	{
		AuthoredLight shape;
		float position[3] = {};
		float direction[3] = {};
		std::uint64_t hash = 0;
		remixapi_LightHandle handle = nullptr;
	};

	static std::vector<AuthoredInstance> authoredInstances;
	static unsigned authoredSettingsVersion = 0;
	static std::uintptr_t authoredWorld = 0;
	static float authoredBrightness = 0.0f;
	static ULONGLONG authoredStartMs = 0;

	static float SrgbToLinear(float channel)
	{
		if (channel <= 0.03928f)
		{
			return channel / 12.92f;
		}

		return std::pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	static void LightColourToLinear(const float* colour, float colourScale, float* linear)
	{
		float peak = 1.0f;

		for (int channel = 0; channel < 3; ++channel)
		{
			peak = std::max(peak, colour[channel] * colourScale);
		}

		for (int channel = 0; channel < 3; ++channel)
		{
			linear[channel] = SrgbToLinear(colour[channel] * colourScale / peak) * peak;
		}
	}

	static void DestroyMapLights()
	{
		for (int i = 0; i < mapLightCount; ++i)
		{
			DestroyCountedLight(mapLights[i]);
		}

		mapLightCount = 0;
		mapLightsWorld = 0;
		stats.mapLights = 0;
	}

	static SunSettings GetEffectiveSun()
	{
		SunSettings sun = sunSettings;
		const MapSunOverride& mapSun = GetMapSettings().sun;

		if (mapSun.brightness)
		{
			sun.brightness = *mapSun.brightness;
		}

		if (mapSun.maxRadiance)
		{
			sun.isCapped = true;
			sun.maxRadiance = *mapSun.maxRadiance;
		}

		if (mapSun.hasTint)
		{
			for (int channel = 0; channel < 3; ++channel)
			{
				sun.tint[channel] = mapSun.tint[channel];
			}
		}

		if (mapSun.hasDirection)
		{
			sun.isDirectionOverridden = true;
			sun.yawDeg = mapSun.yawDeg;
			sun.pitchDeg = mapSun.pitchDeg;
		}

		return sun;
	}

	static bool IsPhysicalSky()
	{
		return IsRemixPlus() && GetLookSettings().isPhysicalSky;
	}

	static void SendAtmosphereOption(const char* name, float value, std::string& sent)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%.3f", value);

		if (sent != text)
		{
			SendOption(api, name, text);
			sent = text;
		}
	}

	// Remix Plus's Numos sky lights the scene with its own sun (RtxAtmosphere::syncDistantLights), so on the physical
	// sky zw3 places that sun rather than adding a second one. The fork turns elevation e and rotation a into the Y-up
	// vector (cos e sin a, sin e, cos e cos a) and, with rtx.zUp, into world (x, z, y), so for the direction towards
	// the sun: e = asin(z), a = atan2(x, y).
	// With "Keep the map's sun" zw3 still lights the scene with its own sun (the map's colour and brightness, never
	// shaded by the fork's clouds, which is what took Kino's shadows under the rainstorm preset); the fork's sun light
	// gets radiance 0 through rtx.atmosphere.directionalLightRadianceScale, and its sky is still placed at this sun.
	static void SubmitAtmosphereSun(const float* travel, const SunSettings& sun, bool isBelowHorizon, bool isMapSunKept)
	{
		static std::string sentElevation;
		static std::string sentRotation;
		static std::string sentIntensity;
		static std::string sentIlluminance;
		static std::string sentRadianceScale;
		static constexpr float nightElevationDeg = -20.0f;
		static constexpr float forkSunIntensity = 1.09f;
		static constexpr float forkSunIlluminance = 15.0f;
		static constexpr float radToDeg = 180.0f / 3.14159265f;

		SendAtmosphereOption("rtx.atmosphere.directionalLightRadianceScale", isMapSunKept ? 0.0f : 1.0f, sentRadianceScale);

		if (sunLight && !isMapSunKept)
		{
			DestroyCountedLight(sunLight);
			sunLight = nullptr;
		}

		const float toSun[3] = { -travel[0], -travel[1], -travel[2] };
		const float elevation = isBelowHorizon ? nightElevationDeg : std::asin(std::clamp(toSun[2], -1.0f, 1.0f)) * radToDeg;
		const float rotation = std::atan2(toSun[0], toSun[1]) * radToDeg;

		SendAtmosphereOption("rtx.atmosphere.sunElevation", elevation, sentElevation);
		SendAtmosphereOption("rtx.atmosphere.sunRotation", rotation, sentRotation);
		SendAtmosphereOption("rtx.atmosphere.sunIntensity", forkSunIntensity * sun.brightness, sentIntensity);

		char illuminance[64];
		std::snprintf(illuminance, sizeof(illuminance), "%.3f, %.3f, %.3f", forkSunIlluminance * SrgbToLinear(sun.tint[0]),
			forkSunIlluminance * SrgbToLinear(sun.tint[1]), forkSunIlluminance * SrgbToLinear(sun.tint[2]));

		if (sentIlluminance != illuminance)
		{
			SendOption(api, "rtx.atmosphere.sunIlluminance", illuminance);
			sentIlluminance = illuminance;
		}

		std::memcpy(stats.sunDirection, travel, sizeof(stats.sunDirection));
		stats.hasSun = !isBelowHorizon;
		stats.isAtmosphereSun = !isMapSunKept;
		stats.atmosphereElevationDeg = elevation;
		stats.atmosphereRotationDeg = rotation;
		stats.atmosphereIntensity = forkSunIntensity * sun.brightness;
	}

	static void SubmitSun(std::uintptr_t backEnd, std::uintptr_t world, float colourScale)
	{
		if (GetLookSettings().isNight)
		{
			if (IsPhysicalSky())
			{
				const float straightDown[3] = { 0.0f, 0.0f, -1.0f };
				SubmitAtmosphereSun(straightDown, GetEffectiveSun(), true, false);
			}

			stats.hasSun = false;
			return;
		}

		const SunSettings effectiveSun = GetEffectiveSun();

		float direction[3];
		std::memset(direction, 0, sizeof(direction));

		// No back end (the menu scene): the map file's [sun] gives the direction.
		if (backEnd)
		{
			std::memcpy(direction, reinterpret_cast<const void*>(backEnd + Iw4::backEndSunDirection), sizeof(direction));
		}

		float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
		float colour[3] = { 1.0f, 1.0f, 1.0f };

		// A converted map whose zone lost its sun gets it from the map settings: [sun] yaw/pitch (the direction towards
		// the sun, as the override below) and colour (the engine's sun colour).
		const MapSunOverride& mapSun = GetMapSettings().sun;
		const bool isSettingsSun = length < 0.001f && mapSun.hasDirection && mapSun.hasColour;

		if (isSettingsSun)
		{
			const float degToRad = 3.14159265f / 180.0f;
			direction[0] = std::cos(mapSun.pitchDeg * degToRad) * std::cos(mapSun.yawDeg * degToRad);
			direction[1] = std::cos(mapSun.pitchDeg * degToRad) * std::sin(mapSun.yawDeg * degToRad);
			direction[2] = std::sin(mapSun.pitchDeg * degToRad);
			length = 1.0f;
			std::memcpy(colour, mapSun.colour, sizeof(colour));
		}

		if (length < 0.001f)
		{
			stats.hasSun = false;
			return;
		}

		const std::uint32_t lastSun = world ? ReadAt<std::uint32_t>(world + Iw4::worldLastSunPrimaryLightIndex) : 0;
		const std::uint32_t lightCount = world ? ReadAt<std::uint32_t>(world + Iw4::worldPrimaryLightCount) : 0;
		const std::uintptr_t lights = backEnd + Iw4::backEndPrimaryLights;

		for (std::uint32_t i = 1; !isSettingsSun && i <= lastSun && i < lightCount; ++i)
		{
			const std::uintptr_t light = lights + i * Iw4::gfxLightStride;

			float lightDirection[3];
			std::memcpy(lightDirection, reinterpret_cast<const void*>(light + Iw4::gfxLightDir), sizeof(lightDirection));

			const bool isSameDirection = std::fabs(lightDirection[0] - direction[0]) < 0.0001f
				&& std::fabs(lightDirection[1] - direction[1]) < 0.0001f
				&& std::fabs(lightDirection[2] - direction[2]) < 0.0001f;

			if (isSameDirection)
			{
				std::memcpy(colour, reinterpret_cast<const void*>(light + Iw4::gfxLightColor), sizeof(colour));
				break;
			}
		}

		float radiance[3];
		float peak = 0.0f;

		for (int channel = 0; channel < 3; ++channel)
		{
			stats.mapSunRadiance[channel] = SrgbToLinear(colour[channel] * colourScale);
			radiance[channel] = SrgbToLinear(colour[channel] * effectiveSun.tint[channel] * colourScale) * effectiveSun.brightness;
			peak = std::max(peak, radiance[channel]);
		}

		if (effectiveSun.isCapped && peak > effectiveSun.maxRadiance)
		{
			for (int channel = 0; channel < 3; ++channel)
			{
				radiance[channel] *= effectiveSun.maxRadiance / peak;
			}
		}

		float travel[3] = {
			-direction[0] / length,
			-direction[1] / length,
			-direction[2] / length,
		};

		const LookSettings look = GetLookSettings();

		if (look.isTimeOfDay)
		{
			const float degToRad = 3.14159265f / 180.0f;
			const float elevationDeg = noonElevationDeg * std::sin(3.14159265f * (look.timeOfDayHours - 6.0f) / 12.0f);
			const float mapAzimuthDeg = std::atan2(-travel[1], -travel[0]) / degToRad;
			const float yaw = (mapAzimuthDeg + (look.timeOfDayHours - 12.0f) * degreesPerHour) * degToRad;
			const float pitch = std::max(elevationDeg, minimumTimeOfDayElevationDeg) * degToRad;

			travel[0] = -std::cos(pitch) * std::cos(yaw);
			travel[1] = -std::cos(pitch) * std::sin(yaw);
			travel[2] = -std::sin(pitch);

			const float daylight = std::clamp((elevationDeg - sunsetEndDeg) / (sunsetStartDeg - sunsetEndDeg), 0.0f, 1.0f);
			const float warmth = std::clamp(1.0f - elevationDeg / warmBelowDeg, 0.0f, 1.0f);
			const float sunsetTint[3] = { 1.0f, 1.0f - 0.45f * warmth, 1.0f - 0.7f * warmth };

			for (int channel = 0; channel < 3; ++channel)
			{
				radiance[channel] *= sunsetTint[channel] * daylight;
			}
		}

		if (effectiveSun.isDirectionOverridden && !look.isTimeOfDay)
		{
			const float degToRad = 3.14159265f / 180.0f;
			const float yaw = effectiveSun.yawDeg * degToRad;
			const float pitch = effectiveSun.pitchDeg * degToRad;

			travel[0] = -std::cos(pitch) * std::cos(yaw);
			travel[1] = -std::cos(pitch) * std::sin(yaw);
			travel[2] = -std::sin(pitch);
		}

		if (IsPhysicalSky())
		{
			const bool isMapSunKept = look.isPhysicalSkyMapSun;
			SubmitAtmosphereSun(travel, effectiveSun, false, isMapSunKept);

			if (!isMapSunKept)
			{
				return;
			}
		}

		float volumetricScale = 1.0f;

		if (look.isGodRays)
		{
			volumetricScale = look.godRayStrength;
		}

		const float sun[7] = {
			travel[0],
			travel[1],
			travel[2],
			radiance[0],
			radiance[1],
			radiance[2],
			volumetricScale,
		};

		if (!sunLight || std::memcmp(sun, submittedSun, sizeof(sun)) != 0)
		{
			if (sunLight)
			{
				DestroyCountedLight(sunLight);
				sunLight = nullptr;
			}

			remixapi_LightInfoDistantEXT distant{};
			distant.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
			distant.direction = { sun[0], sun[1], sun[2] };
			distant.angularDiameterDegrees = sunAngularDiameterDeg;
			distant.volumetricRadianceScale = sun[6];

			remixapi_LightInfo info{};
			info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
			info.pNext = &distant;
			info.hash = sunHash;
			info.radiance = { sun[3], sun[4], sun[5] };

			if (CreateCountedLight(&info, &sunLight) != REMIXAPI_ERROR_CODE_SUCCESS)
			{
				sunLight = nullptr;
				stats.hasSun = false;
				return;
			}

			std::memcpy(submittedSun, sun, sizeof(sun));
			std::memcpy(stats.sunRadiance, sun + 3, sizeof(stats.sunRadiance));
			std::memcpy(stats.sunDirection, sun, sizeof(stats.sunDirection));

			Log() << "remix: sun light direction " << sun[0] << " " << sun[1] << " " << sun[2] << ", radiance "
				  << sun[3] << " " << sun[4] << " " << sun[5];
		}

		api->DrawLightInstance(sunLight);
		stats.hasSun = true;
	}

	static const LightOverride* FindLightOverride(const MapSettings& mapSettings, std::uint64_t hash)
	{
		for (const LightOverride& change : mapSettings.lightOverrides)
		{
			if (change.hash == hash)
			{
				return &change;
			}
		}

		return nullptr;
	}

	static void BuildMapLights(std::uintptr_t backEnd, std::uintptr_t world, float colourScale, std::uintptr_t mapIdentity)
	{
		const bool isNewMap = mapIdentity != mapLightsWorld;

		DestroyMapLights();
		mapLightsWorld = mapIdentity;
		mapLightsBrightness = settledLightBrightness;
		mapLightsColourScale = colourScale;
		mapLightsSettingsVersion = GetMapSettingsVersion();
		mapLightsTuningVersion = tuningVersion;
		mapLightInfo.clear();
		stats.ignoredMapLights = 0;
		stats.changedMapLights = 0;

		// No world (the menu scene): no engine lights, only the map file's.
		const std::uint32_t lastSun = world ? ReadAt<std::uint32_t>(world + Iw4::worldLastSunPrimaryLightIndex) : 0;
		const std::uint32_t lightCount = world && backEnd ? ReadAt<std::uint32_t>(world + Iw4::worldPrimaryLightCount) : 0;
		const std::uintptr_t lights = backEnd + Iw4::backEndPrimaryLights;

		const std::string mapName = world ? GetMapName() : GetMapSettings().mapName;
		const std::uint64_t mapSeed = XXH3_64bits_withSeed(mapName.data(), mapName.size(), mapLightHashBase);
		const MapSettings& mapSettings = GetMapSettings();

		// The zone's own lights, then any the map settings restore for a converted map that lost them. Both are the
		// engine's ComPrimaryLight values (R_InitPrimaryLights copies them unchanged), so one conversion serves both.
		std::vector<PrimaryLight> sources;

		for (std::uint32_t i = lastSun + 1; i < lightCount; ++i)
		{
			const std::uintptr_t light = lights + i * Iw4::gfxLightStride;
			PrimaryLight source;
			source.type = ReadAt<std::uint8_t>(light + Iw4::gfxLightType);
			std::memcpy(source.colour, reinterpret_cast<const void*>(light + Iw4::gfxLightColor), sizeof(source.colour));
			std::memcpy(source.direction, reinterpret_cast<const void*>(light + Iw4::gfxLightDir), sizeof(source.direction));
			std::memcpy(source.origin, reinterpret_cast<const void*>(light + Iw4::gfxLightOrigin), sizeof(source.origin));
			source.radius = ReadAt<float>(light + Iw4::gfxLightRadius);
			source.cosOuter = ReadAt<float>(light + Iw4::gfxLightCosHalfFovOuter);
			source.cosInner = ReadAt<float>(light + Iw4::gfxLightCosHalfFovInner);
			sources.push_back(source);
		}

		sources.insert(sources.end(), mapSettings.primaryLights.begin(), mapSettings.primaryLights.end());

		for (std::uint32_t i = 0; i < sources.size() && mapLightCount < maxMapLights; ++i)
		{
			const PrimaryLight& source = sources[i];
			const auto type = source.type;

			if (type != Iw4::gfxLightTypeSpot && type != Iw4::gfxLightTypeOmni)
			{
				continue;
			}

			float colour[3];
			float direction[3];
			float origin[3];
			std::memcpy(colour, source.colour, sizeof(colour));
			std::memcpy(direction, source.direction, sizeof(direction));
			std::memcpy(origin, source.origin, sizeof(origin));

			const float radius = source.radius;
			const float cosOuter = std::clamp(source.cosOuter, -1.0f, 1.0f);
			const float cosInner = std::clamp(source.cosInner, -1.0f, 1.0f);

			const std::int32_t placement[4] = {
				static_cast<std::int32_t>(std::lround(origin[0])),
				static_cast<std::int32_t>(std::lround(origin[1])),
				static_cast<std::int32_t>(std::lround(origin[2])),
				static_cast<std::int32_t>(type),
			};

			std::uint64_t hash = XXH3_64bits_withSeed(placement, sizeof(placement), mapSeed);

			// The engine light index, as before, so saved overrides of colliding lights keep their hashes.
			const std::uint32_t lightIndex = lastSun + 1 + i;

			for (const MapLightInfo& earlier : mapLightInfo)
			{
				if (earlier.hash == hash)
				{
					hash = XXH3_64bits_withSeed(&lightIndex, sizeof(lightIndex), hash);
					break;
				}
			}

			const bool isSpot = type == Iw4::gfxLightTypeSpot;

			MapLightInfo info;
			info.hash = hash;
			std::memcpy(info.origin, origin, sizeof(origin));
			info.radius = radius;
			info.isSpot = isSpot;
			info.isIgnored = std::find(mapSettings.ignoredLights.begin(), mapSettings.ignoredLights.end(), hash)
				!= mapSettings.ignoredLights.end();

			const LightOverride* change = FindLightOverride(mapSettings, hash);
			info.isChanged = change != nullptr;
			mapLightInfo.push_back(info);

			if (info.isIgnored)
			{
				++stats.ignoredMapLights;
				continue;
			}

			float typeBrightness = tuning.omniBrightness;

			if (isSpot)
			{
				typeBrightness = tuning.spotBrightness;
			}

			float lightSize = tuning.sphereRadius;
			float coneDeg = std::acos(cosOuter) * 57.2957795f + tuning.coneOffsetDeg;
			float softness = std::max(0.0f, cosInner - cosOuter) + tuning.softnessOffset;
			float focus = tuning.focusExponent;
			float volumetric = tuning.volumetric;
			float overrideBrightness = 1.0f;

			if (change)
			{
				++stats.changedMapLights;

				if (change->hasColour)
				{
					std::memcpy(colour, change->colour, sizeof(colour));
				}

				if (change->brightness)
				{
					overrideBrightness = *change->brightness;
				}

				if (change->radius)
				{
					lightSize = *change->radius;
				}

				if (change->coneDeg)
				{
					coneDeg = *change->coneDeg;
				}

				if (change->softness)
				{
					softness = *change->softness;
				}

				if (change->focus)
				{
					focus = *change->focus;
				}

				if (change->volumetric)
				{
					volumetric = *change->volumetric;
				}
			}

			lightSize = std::max(lightSize, 0.1f);

			const float brightness = settledLightBrightness * typeBrightness * overrideBrightness;
			const float matchDistance = matchDistanceOfRadius * radius;
			const float radianceScale = (matchDistance * matchDistance) / (lightSize * lightSize) * brightness;

			float linear[3];
			LightColourToLinear(colour, colourScale, linear);

			const float peak = std::max({ linear[0], linear[1], linear[2] }) * brightness;

			if (!change && peak < tuning.dimLightCutoff)
			{
				continue;
			}

			remixapi_LightInfoSphereEXT sphere{};
			sphere.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
			sphere.position = { origin[0], origin[1], origin[2] };
			sphere.radius = lightSize;
			sphere.volumetricRadianceScale = volumetric;

			const float directionLength = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]
				+ direction[2] * direction[2]);

			if (isSpot && directionLength > 0.001f)
			{
				sphere.shaping_hasvalue = 1;
				sphere.shaping_value.direction = {
					-direction[0] / directionLength,
					-direction[1] / directionLength,
					-direction[2] / directionLength,
				};
				sphere.shaping_value.coneAngleDegrees = std::clamp(coneDeg, 1.0f, 179.0f);
				sphere.shaping_value.coneSoftness = std::clamp(softness, 0.0f, 1.0f);
				sphere.shaping_value.focusExponent = std::max(focus, 0.0f);
			}

			remixapi_LightInfo lightInfo{};
			lightInfo.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
			lightInfo.pNext = &sphere;
			lightInfo.hash = hash;
			lightInfo.radiance = { linear[0] * radianceScale, linear[1] * radianceScale, linear[2] * radianceScale };

			remixapi_LightHandle handle = nullptr;

			if (CreateCountedLight(&lightInfo, &handle) == REMIXAPI_ERROR_CODE_SUCCESS && handle)
			{
				mapLights[mapLightCount] = handle;
				++mapLightCount;
			}
		}

		stats.mapLights = mapLightCount;

		if (isNewMap)
		{
			Log() << "remix: " << mapLightCount << " map lights (spot and omni primary lights) sent to Remix, "
				  << lastSun << " sun light(s) on this map, " << stats.ignoredMapLights << " ignored and "
				  << stats.changedMapLights << " changed by the map settings";
		}
	}

	static void DestroyDynamicLights()
	{
		for (int slot = 0; slot < maxDynamicLights; ++slot)
		{
			if (dynamicLights[slot])
			{
				DestroyCountedLight(dynamicLights[slot]);
				dynamicLights[slot] = nullptr;
			}

			isDynamicDrawn[slot] = false;
		}

		stats.dynamicLights = 0;
	}

	static bool IsRendererSingleThreaded()
	{
		const auto smpBackend = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().r_smp_backendDvar));

		return smpBackend && ReadAt<std::uint8_t>(smpBackend + Iw4::dvarValueOffset) == 0;
	}

	static void SubmitDynamicLights(std::uintptr_t backEnd, std::uintptr_t world, std::uintptr_t viewInfo, float colourScale)
	{
		std::uint32_t first = ReadAt<std::uint32_t>(world + Iw4::worldPrimaryLightCount);
		std::uint32_t end = ReadAt<std::uint32_t>(viewInfo + Iw4::viewInfoShadowableLightCount);
		std::uintptr_t lights = backEnd + Iw4::backEndPrimaryLights;

		if (isAllEffectLights && IsRendererSingleThreaded())
		{
			const Iw4::GameBuild& build = Engine::CurrentBuild();

			first = 0;
			end = std::min(ReadAt<std::uint32_t>(Engine::RebaseAddress(build.sceneAddedLightCount)),
				Iw4::maxSceneAddedLights);
			lights = Engine::RebaseAddress(build.sceneAddedLight);
		}

		bool isDrawnNow[maxDynamicLights] = {};
		int drawn = 0;

		for (std::uint32_t i = first; i < end && i - first < static_cast<std::uint32_t>(maxDynamicLights); ++i)
		{
			const int slot = static_cast<int>(i - first);
			const std::uintptr_t light = lights + i * Iw4::gfxLightStride;
			const auto type = ReadAt<std::uint8_t>(light + Iw4::gfxLightType);

			if (type != Iw4::gfxLightTypeSpot && type != Iw4::gfxLightTypeOmni)
			{
				continue;
			}

			float colour[3];
			std::memcpy(colour, reinterpret_cast<const void*>(light + Iw4::gfxLightColor), sizeof(colour));

			for (float& channel : colour)
			{
				channel = std::max(channel, 0.0f);
			}

			if (std::max({ colour[0], colour[1], colour[2] }) <= 0.0f)
			{
				continue;
			}

			const float submitScale = colourScale * lightBrightness;
			const bool hasChanged = std::memcmp(reinterpret_cast<const void*>(light), submittedDynamic[slot],
				dynamicLightCompareBytes) != 0 || submittedDynamicScale[slot] != submitScale;

			if (!dynamicLights[slot] || !isDynamicDrawn[slot] || hasChanged)
			{
				float direction[3];
				float origin[3];
				std::memcpy(direction, reinterpret_cast<const void*>(light + Iw4::gfxLightDir), sizeof(direction));
				std::memcpy(origin, reinterpret_cast<const void*>(light + Iw4::gfxLightOrigin), sizeof(origin));

				const float radius = ReadAt<float>(light + Iw4::gfxLightRadius);
				const float cosOuter = std::clamp(ReadAt<float>(light + Iw4::gfxLightCosHalfFovOuter), -1.0f, 1.0f);
				const float cosInner = std::clamp(ReadAt<float>(light + Iw4::gfxLightCosHalfFovInner), -1.0f, 1.0f);

				const float matchDistance = matchDistanceOfRadius * radius;
				const float radianceScale = (matchDistance * matchDistance) / (sphereRadius * sphereRadius) * lightBrightness;

				remixapi_LightInfoSphereEXT sphere{};
				sphere.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
				sphere.position = { origin[0], origin[1], origin[2] };
				sphere.radius = sphereRadius;
				sphere.volumetricRadianceScale = 1.0f;

				const float directionLength = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]
					+ direction[2] * direction[2]);

				if (type == Iw4::gfxLightTypeSpot && directionLength > 0.001f)
				{
					sphere.shaping_hasvalue = 1;
					sphere.shaping_value.direction = {
						-direction[0] / directionLength,
						-direction[1] / directionLength,
						-direction[2] / directionLength,
					};
					sphere.shaping_value.coneAngleDegrees = std::acos(cosOuter) * 57.2957795f;
					sphere.shaping_value.coneSoftness = std::max(0.0f, cosInner - cosOuter);
					sphere.shaping_value.focusExponent = 0.0f;
				}

				float linear[3];
				LightColourToLinear(colour, colourScale, linear);

				const float dynamicScale = radianceScale * dynamicLightBrightness;

				remixapi_LightInfo info{};
				info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
				info.pNext = &sphere;
				info.hash = dynamicLightHashBase + static_cast<std::uint64_t>(slot);
				info.radiance = { linear[0] * dynamicScale, linear[1] * dynamicScale, linear[2] * dynamicScale };

				if (dynamicLights[slot])
				{
					DestroyCountedLight(dynamicLights[slot]);
					dynamicLights[slot] = nullptr;
				}

				remixapi_LightHandle handle = nullptr;

				if (CreateCountedLight(&info, &handle) != REMIXAPI_ERROR_CODE_SUCCESS || !handle)
				{
					continue;
				}

				dynamicLights[slot] = handle;
				std::memcpy(submittedDynamic[slot], reinterpret_cast<const void*>(light), dynamicLightCompareBytes);
				submittedDynamicScale[slot] = submitScale;
				++stats.dynamicLightUpdates;
			}

			api->DrawLightInstance(dynamicLights[slot]);
			isDrawnNow[slot] = true;
			++drawn;
		}

		std::memcpy(isDynamicDrawn, isDrawnNow, sizeof(isDynamicDrawn));
		stats.dynamicLights = drawn;
	}

	static std::uint64_t fixtureHashes[maxFixtureLights];
	static int fixtureCells[maxFixtureLights];
	static std::unordered_map<std::uint64_t, FixtureEdit> fixtureEdits;
	static std::unordered_set<std::uint64_t> dirtyFixtures;
	static float fixtureRadianceScale = 0.0f;

	static int FixtureCellAt(const float* position)
	{
		const int cell = CellForPoint(position);

		if (cell >= 0)
		{
			return cell;
		}

		static constexpr float steps[] = { 8.0f, 16.0f, 32.0f, 48.0f };
		static constexpr float directions[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };

		for (const float step : steps)
		{
			for (const auto& direction : directions)
			{
				const float point[3] = { position[0] + direction[0] * step, position[1] + direction[1] * step,
					position[2] + direction[2] * step };
				const int nearby = CellForPoint(point);

				if (nearby >= 0)
				{
					return nearby;
				}
			}
		}

		return -1;
	}

	static std::uint64_t FixtureHashAt(const float* position)
	{
		const std::int32_t cell[3] = {
			static_cast<std::int32_t>(std::lround(position[0])),
			static_cast<std::int32_t>(std::lround(position[1])),
			static_cast<std::int32_t>(std::lround(position[2])),
		};

		return XXH3_64bits_withSeed(cell, sizeof(cell), fixtureLightHashBase);
	}

	static FixtureEdit EffectiveFixtureEdit(std::uint64_t hash, int cell)
	{
		const auto live = fixtureEdits.find(hash);

		if (live != fixtureEdits.end())
		{
			return live->second;
		}

		FixtureEdit edit;
		const MapSettings& settings = GetMapSettings();

		if (std::find(settings.ignoredLights.begin(), settings.ignoredLights.end(), hash) != settings.ignoredLights.end())
		{
			edit.isOn = false;
			return edit;
		}

		for (const LightOverride& change : settings.lightOverrides)
		{
			if (change.hash != hash)
			{
				continue;
			}

			edit.brightness = change.brightness.value_or(1.0f);
			edit.hasColour = change.hasColour;
			std::memcpy(edit.colour, change.colour, sizeof(edit.colour));
			edit.size = change.radius.value_or(sphereRadius);
			return edit;
		}

		for (const MapZone& zone : settings.zones)
		{
			if (!GetLookSettings().isMapZones)
			{
				break;
			}

			if (cell < 0 || std::find(zone.cells.begin(), zone.cells.end(), cell) == zone.cells.end())
			{
				continue;
			}

			edit.brightness = zone.lampBrightness.value_or(1.0f);

			if (zone.hasLampColour)
			{
				edit.hasColour = true;
				std::memcpy(edit.colour, zone.lampColour, sizeof(edit.colour));
			}

			break;
		}

		return edit;
	}

	static int FixtureCellOf(std::uint64_t hash)
	{
		for (int i = 0; i < fixtureLightCount; ++i)
		{
			if (fixtureHashes[i] == hash)
			{
				return fixtureCells[i];
			}
		}

		return -1;
	}

	static remixapi_LightHandle CreateFixtureLight(int index)
	{
		const FixtureEdit edit = EffectiveFixtureEdit(fixtureHashes[index], fixtureCells[index]);

		if (!edit.isOn)
		{
			return nullptr;
		}

		const float* colour = edit.hasColour ? edit.colour : fixtureColour;
		const float size = std::max(edit.size * settledFixtureSize / sphereRadius, 0.5f);
		const float scale = fixtureRadianceScale * edit.brightness * (sphereRadius * sphereRadius) / (size * size);

		remixapi_LightInfoSphereEXT sphere{};
		sphere.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
		sphere.position = { fixturePositions[index][0], fixturePositions[index][1], fixturePositions[index][2] };
		sphere.radius = size;
		sphere.volumetricRadianceScale = 1.0f;

		remixapi_LightInfo info{};
		info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
		info.pNext = &sphere;
		info.hash = fixtureHashes[index];
		info.radiance = { colour[0] * scale, colour[1] * scale, colour[2] * scale };

		remixapi_LightHandle handle = nullptr;

		if (CreateCountedLight(&info, &handle) != REMIXAPI_ERROR_CODE_SUCCESS)
		{
			return nullptr;
		}

		return handle;
	}

	static void DestroyFixtureLights()
	{
		for (int i = 0; i < fixtureLightCount; ++i)
		{
			if (fixtureLights[i])
			{
				DestroyCountedLight(fixtureLights[i]);
				fixtureLights[i] = nullptr;
			}
		}

		fixtureLightCount = 0;
		fixtureLightsVersion = 0;
		stats.fixtureLights = 0;
	}

	static void RebuildDirtyFixtures()
	{
		if (dirtyFixtures.empty())
		{
			return;
		}

		for (int i = 0; i < fixtureLightCount; ++i)
		{
			if (dirtyFixtures.count(fixtureHashes[i]) == 0)
			{
				continue;
			}

			if (fixtureLights[i])
			{
				DestroyCountedLight(fixtureLights[i]);
			}

			fixtureLights[i] = CreateFixtureLight(i);
		}

		dirtyFixtures.clear();
	}

	static void SubmitFixtureLights()
	{
		const LookSettings look = GetLookSettings();

		if (!look.isFixtureLights)
		{
			if (fixtureLightCount > 0)
			{
				DestroyFixtureLights();
			}

			return;
		}

		const unsigned spots = GetSurfaceSpotsVersion();
		const unsigned version = GetFixtureVersion() + GetLampLightVersion() * 65536u + GetMapSettingsVersion() * 7919u
			+ (look.isMapZones ? 1u : 2u);
		const float brightness = settledFixtureBrightness * settledLightBrightness;
		const bool isStale = spots != fixtureLightsSpots || version != fixtureLightsVersion
			|| brightness != fixtureLightsBrightness || settledFixtureOffset != fixtureLightsOffset
			|| settledFixtureSize != fixtureLightsSize;

		if (isStale)
		{
			++stats.fixtureRebuilds;
			DestroyFixtureLights();
			fixtureLightsSpots = spots;
			fixtureLightsVersion = version;
			fixtureLightsBrightness = brightness;
			fixtureLightsOffset = settledFixtureOffset;
			fixtureLightsSize = settledFixtureSize;
			dirtyFixtures.clear();

			const float matchDistance = matchDistanceOfRadius * fixtureRange;
			fixtureRadianceScale = (matchDistance * matchDistance) / (sphereRadius * sphereRadius) * brightness;

			for (const FixtureSpot& spot : CollectFixtureSpots())
			{
				if (fixtureLightCount >= maxFixtureLights)
				{
					break;
				}

				const int index = fixtureLightCount;
				for (int axis = 0; axis < 3; ++axis)
				{
					fixturePositions[index][axis] = spot.position[axis] + spot.normal[axis] * settledFixtureOffset;
				}

				fixtureHashes[index] = FixtureHashAt(spot.position);
				fixtureCells[index] = FixtureCellAt(spot.position);
				fixtureLights[index] = CreateFixtureLight(index);
				++fixtureLightCount;
			}

			stats.fixtureLights = fixtureLightCount;
			stats.fixturesInCells = static_cast<int>(std::count_if(fixtureCells, fixtureCells + fixtureLightCount,
				[](int cell) { return cell >= 0; }));
		}

		RebuildDirtyFixtures();

		float camera[3];

		if (fixtureLightCount <= maxDrawnFixtureLights || !TryGetCameraOrigin(camera))
		{
			int drawnAll = 0;

			for (int i = 0; i < fixtureLightCount; ++i)
			{
				if (fixtureLights[i])
				{
					api->DrawLightInstance(fixtureLights[i]);
					++drawnAll;
				}
			}

			stats.drawnFixtureLights = drawnAll;
			return;
		}

		std::array<std::pair<float, int>, maxFixtureLights> byDistance{};
		int candidates = 0;

		for (int i = 0; i < fixtureLightCount; ++i)
		{
			if (!fixtureLights[i])
			{
				continue;
			}

			const float dx = fixturePositions[i][0] - camera[0];
			const float dy = fixturePositions[i][1] - camera[1];
			const float dz = fixturePositions[i][2] - camera[2];
			const float distanceSquared = dx * dx + dy * dy + dz * dz;

			if (distanceSquared <= maxFixtureLightDistance * maxFixtureLightDistance)
			{
				byDistance[candidates++] = { distanceSquared, i };
			}
		}

		const int drawn = std::min(candidates, maxDrawnFixtureLights);
		std::partial_sort(byDistance.begin(), byDistance.begin() + drawn, byDistance.begin() + candidates);

		for (int i = 0; i < drawn; ++i)
		{
			api->DrawLightInstance(fixtureLights[byDistance[i].second]);
		}

		stats.drawnFixtureLights = drawn;
	}

	FixtureEdit GetFixtureEdit(std::uint64_t hash)
	{
		return EffectiveFixtureEdit(hash, FixtureCellOf(hash));
	}

	void SetFixtureEdit(std::uint64_t hash, const FixtureEdit& edit)
	{
		fixtureEdits[hash] = edit;
		dirtyFixtures.insert(hash);
	}

	void ForgetFixtureEdit(std::uint64_t hash)
	{
		fixtureEdits.erase(hash);
		dirtyFixtures.insert(hash);
	}

	bool HasLiveFixtureEdit(std::uint64_t hash)
	{
		return fixtureEdits.count(hash) != 0;
	}

	static void DestroyLaserLights()
	{
		for (int i = 0; i < laserLightCount; ++i)
		{
			DestroyCountedLight(laserLights[i]);
		}

		laserLightCount = 0;
	}

	static void SubmitLaserLights()
	{
		DestroyLaserLights();
		stats.laserLights = 0;

		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (!GetLookSettings().isLaserLights || build.g_postLightCount == 0 || build.g_postLightInfo == 0)
		{
			return;
		}

		const std::uint32_t count = std::min(ReadAt<std::uint32_t>(Engine::RebaseAddress(build.g_postLightCount)),
			Iw4::maxPostLights);
		const std::uintptr_t entries = Engine::RebaseAddress(build.g_postLightInfo);

		for (std::uint32_t i = 0; i < count && laserLightCount < maxLaserLights; ++i)
		{
			const std::uintptr_t entry = entries + i * Iw4::postLightStride;
			float begin[3];
			float end[3];
			std::memcpy(begin, reinterpret_cast<const void*>(entry + Iw4::postLightBegin), sizeof(begin));
			std::memcpy(end, reinterpret_cast<const void*>(entry + Iw4::postLightEnd), sizeof(end));
			const float range = ReadAt<float>(entry + Iw4::postLightRadius);

			const float axis[3] = { end[0] - begin[0], end[1] - begin[1], end[2] - begin[2] };
			const float length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);

			if (length < 0.01f || !(range > 0.0f))
			{
				continue;
			}

			const float matchDistance = matchDistanceOfRadius * range;
			const float radianceScale = (matchDistance * matchDistance) / (laserBeamRadius * laserBeamRadius) * lightBrightness
				* dynamicLightBrightness;

			remixapi_LightInfoCylinderEXT cylinder{};
			cylinder.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_CYLINDER_EXT;
			cylinder.position = { (begin[0] + end[0]) * 0.5f, (begin[1] + end[1]) * 0.5f, (begin[2] + end[2]) * 0.5f };
			cylinder.radius = laserBeamRadius;
			cylinder.axis = { axis[0] / length, axis[1] / length, axis[2] / length };
			cylinder.axisLength = length;
			cylinder.volumetricRadianceScale = 1.0f;

			remixapi_LightInfo info{};
			info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
			info.pNext = &cylinder;
			info.hash = laserLightHashBase + static_cast<std::uint64_t>(laserLightCount);
			info.radiance = { laserColour[0] * radianceScale, laserColour[1] * radianceScale, laserColour[2] * radianceScale };

			remixapi_LightHandle handle = nullptr;

			if (CreateCountedLight(&info, &handle) != REMIXAPI_ERROR_CODE_SUCCESS || !handle)
			{
				continue;
			}

			laserLights[laserLightCount] = handle;
			++laserLightCount;
			api->DrawLightInstance(handle);
		}

		stats.laserLights = laserLightCount;
	}

	static void DestroyAuthoredLights()
	{
		for (AuthoredInstance& instance : authoredInstances)
		{
			if (instance.handle)
			{
				DestroyCountedLight(instance.handle);
				instance.handle = nullptr;
			}
		}

		authoredInstances.clear();
		stats.authoredLights = 0;
		stats.animatedLights = 0;
	}

	static void PlaceOnModel(const float* axis, float scale, const float* origin, const AuthoredLight& shape,
		AuthoredInstance& instance)
	{
		for (int component = 0; component < 3; ++component)
		{
			instance.position[component] = origin[component]
				+ (shape.position[0] * axis[component] + shape.position[1] * axis[3 + component]
					+ shape.position[2] * axis[6 + component]) * scale;

			instance.direction[component] = shape.direction[0] * axis[component]
				+ shape.direction[1] * axis[3 + component] + shape.direction[2] * axis[6 + component];
		}
	}

	static void CollectAuthoredLights(std::uintptr_t world, std::uint64_t mapSeed)
	{
		const MapSettings& mapSettings = GetMapSettings();

		for (const AuthoredLight& shape : mapSettings.addedLights)
		{
			if (authoredInstances.size() >= static_cast<std::size_t>(maxAuthoredLights))
			{
				break;
			}

			AuthoredInstance instance;
			instance.shape = shape;
			std::memcpy(instance.position, shape.position, sizeof(instance.position));
			std::memcpy(instance.direction, shape.direction, sizeof(instance.direction));

			const std::uint64_t index = authoredInstances.size();
			instance.hash = XXH3_64bits_withSeed(&index, sizeof(index), mapSeed ^ authoredLightHashBase);
			authoredInstances.push_back(instance);
		}

		if (mapSettings.attachedLights.empty())
		{
			return;
		}

		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);
		const auto modelCount = ReadAt<std::uint32_t>(world + Iw4::worldSModelCount);

		if (!drawInsts)
		{
			return;
		}

		for (std::uint32_t model = 0; model < modelCount; ++model)
		{
			const std::uintptr_t drawInst = drawInsts + model * Iw4::smodelDrawInstStride;
			const auto xModel = ReadAt<std::uintptr_t>(drawInst + Iw4::smodelDrawInstModel);

			if (!xModel)
			{
				continue;
			}

			const auto* modelName = ReadAt<const char*>(xModel + Iw4::xModelName);

			if (!modelName)
			{
				continue;
			}

			for (std::size_t rule = 0; rule < mapSettings.attachedLights.size(); ++rule)
			{
				const AuthoredLight& shape = mapSettings.attachedLights[rule];

				if (_stricmp(shape.model.c_str(), modelName) != 0)
				{
					continue;
				}

				if (authoredInstances.size() >= static_cast<std::size_t>(maxAuthoredLights))
				{
					return;
				}

				float origin[3];
				float axis[9];
				std::memcpy(origin, reinterpret_cast<const void*>(drawInst + Iw4::smodelDrawInstOrigin), sizeof(origin));
				std::memcpy(axis, reinterpret_cast<const void*>(drawInst + Iw4::smodelDrawInstAxis), sizeof(axis));
				const float scale = ReadAt<float>(drawInst + Iw4::smodelDrawInstScale);

				AuthoredInstance instance;
				instance.shape = shape;
				PlaceOnModel(axis, scale, origin, shape, instance);

				const std::uint64_t key[2] = { static_cast<std::uint64_t>(model), static_cast<std::uint64_t>(rule) };
				instance.hash = XXH3_64bits_withSeed(key, sizeof(key), mapSeed ^ attachedLightHashBase);
				authoredInstances.push_back(instance);
			}
		}
	}

	static void EvaluateKeyframes(const AuthoredLight& shape, float seconds, float& intensity, float* colour)
	{
		intensity = shape.intensity;
		std::memcpy(colour, shape.colour, sizeof(float) * 3);

		const std::vector<LightKeyframe>& keyframes = shape.keyframes;

		if (keyframes.size() < 2)
		{
			return;
		}

		const float length = keyframes.back().timeSeconds;

		if (length <= 0.0f)
		{
			return;
		}

		float time = std::min(seconds, length);

		if (shape.isLooped)
		{
			time = std::fmod(seconds, length);
		}

		std::size_t next = 1;

		while (next < keyframes.size() - 1 && keyframes[next].timeSeconds < time)
		{
			++next;
		}

		const LightKeyframe& before = keyframes[next - 1];
		const LightKeyframe& after = keyframes[next];
		const float span = after.timeSeconds - before.timeSeconds;
		float blend = 1.0f;

		if (span > 0.0f)
		{
			blend = std::clamp((time - before.timeSeconds) / span, 0.0f, 1.0f);
		}

		intensity *= before.intensity + (after.intensity - before.intensity) * blend;

		for (int channel = 0; channel < 3; ++channel)
		{
			float from = shape.colour[channel];
			float to = shape.colour[channel];

			if (before.hasColour)
			{
				from = before.colour[channel];
			}

			if (after.hasColour)
			{
				to = after.colour[channel];
			}

			colour[channel] = from + (to - from) * blend;
		}
	}

	static bool TryCreateAuthoredLight(AuthoredInstance& instance, float intensity, const float* colour)
	{
		const AuthoredLight& shape = instance.shape;

		remixapi_LightInfoSphereEXT sphere{};
		sphere.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
		sphere.position = { instance.position[0], instance.position[1], instance.position[2] };
		sphere.radius = std::max(shape.radius, 0.1f);
		sphere.volumetricRadianceScale = shape.volumetric;

		const float length = std::sqrt(instance.direction[0] * instance.direction[0]
			+ instance.direction[1] * instance.direction[1] + instance.direction[2] * instance.direction[2]);

		if (shape.hasDirection && length > 0.001f)
		{
			sphere.shaping_hasvalue = 1;
			sphere.shaping_value.direction = {
				instance.direction[0] / length,
				instance.direction[1] / length,
				instance.direction[2] / length,
			};
			sphere.shaping_value.coneAngleDegrees = std::clamp(shape.coneDeg, 1.0f, 179.0f);
			sphere.shaping_value.coneSoftness = std::clamp(shape.softness, 0.0f, 1.0f);
			sphere.shaping_value.focusExponent = std::max(shape.focus, 0.0f);
		}

		const float scale = intensity * settledLightBrightness;

		remixapi_LightInfo info{};
		info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
		info.pNext = &sphere;
		info.hash = instance.hash;
		info.radiance = { colour[0] * scale, colour[1] * scale, colour[2] * scale };

		remixapi_LightHandle handle = nullptr;

		if (CreateCountedLight(&info, &handle) != REMIXAPI_ERROR_CODE_SUCCESS || !handle)
		{
			return false;
		}

		instance.handle = handle;
		return true;
	}

	static void SubmitAuthoredLights(std::uintptr_t world)
	{
		const std::uintptr_t mapIdentity = GetMapIdentity();
		const bool isStale = authoredSettingsVersion != GetMapSettingsVersion() || authoredWorld != mapIdentity
			|| authoredBrightness != settledLightBrightness;

		if (isStale)
		{
			DestroyAuthoredLights();
			authoredSettingsVersion = GetMapSettingsVersion();
			authoredWorld = mapIdentity;
			authoredBrightness = settledLightBrightness;
			authoredStartMs = GetTickCount64();

			const std::string mapName = GetMapName();
			const std::uint64_t mapSeed = XXH3_64bits_withSeed(mapName.data(), mapName.size(), mapLightHashBase);
			CollectAuthoredLights(world, mapSeed);

			if (!authoredInstances.empty())
			{
				Log() << "remix: " << authoredInstances.size() << " lights added by the map settings for " << mapName;
			}
		}

		const float seconds = static_cast<float>(GetTickCount64() - authoredStartMs) / 1000.0f;
		int drawn = 0;
		int animated = 0;

		for (AuthoredInstance& instance : authoredInstances)
		{
			if (!instance.shape.trigger.empty() && !IsTriggerActive(instance.shape.trigger))
			{
				continue;
			}

			const bool isAnimated = instance.shape.keyframes.size() >= 2;

			if (isAnimated && instance.handle)
			{
				DestroyCountedLight(instance.handle);
				instance.handle = nullptr;
			}

			if (!instance.handle)
			{
				float intensity = 0.0f;
				float colour[3];
				EvaluateKeyframes(instance.shape, seconds, intensity, colour);

				if (!TryCreateAuthoredLight(instance, intensity, colour))
				{
					continue;
				}
			}

			api->DrawLightInstance(instance.handle);
			++drawn;

			if (isAnimated)
			{
				++animated;
			}
		}

		stats.authoredLights = drawn;
		stats.animatedLights = animated;
	}

	// The menu's own 3D scene (the lobby draws Kino with no map loaded): the map file's sun and [[lights.primary]]
	// through the same conversion as in a match.
	void SubmitMenuSceneLights(IDirect3DDevice9* device)
	{
		static constexpr std::uintptr_t menuSceneIdentity = 1;
		static constexpr float menuColourScale = 1.0f;

		api = TryGetApi(device);

		if (!api || GetMapIdentity() != 0)
		{
			return;
		}

		SettleBrightness(GetLookSettings());
		SubmitSun(0, 0, menuColourScale);

		if (mapLightsWorld != menuSceneIdentity || mapLightsBrightness != settledLightBrightness
			|| mapLightsSettingsVersion != GetMapSettingsVersion() || mapLightsTuningVersion != tuningVersion)
		{
			BuildMapLights(0, 0, menuColourScale, menuSceneIdentity);
		}

		for (int i = 0; i < mapLightCount; ++i)
		{
			api->DrawLightInstance(mapLights[i]);
		}

		++stats.submittedFrames;
	}

	void SubmitSceneLights(IDirect3DDevice9* device, const void* viewInfo)
	{
		api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		if (!stats.isApiReady)
		{
			stats.isApiReady = true;
			Log() << "remix: the map's sun and lights go to Remix as real lights through its API";
		}

		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.backEndData == 0 || build.gfxWorld == 0 || build.r_diffuseColorScaleDvar == 0)
		{
			return;
		}

		const auto backEnd = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.backEndData));
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));
		const auto colourScaleDvar = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.r_diffuseColorScaleDvar));

		if (!backEnd || !world || !colourScaleDvar)
		{
			return;
		}

		const float colourScale = ReadAt<float>(colourScaleDvar + Iw4::dvarValueOffset);

		SettleBrightness(GetLookSettings());
		SubmitSun(backEnd, world, colourScale);

		if (GetMapIdentity() != mapLightsWorld || mapLightsBrightness != settledLightBrightness
			|| mapLightsColourScale != colourScale || mapLightsSettingsVersion != GetMapSettingsVersion()
			|| mapLightsTuningVersion != tuningVersion)
		{
			BuildMapLights(backEnd, world, colourScale, GetMapIdentity());
		}

		for (int i = 0; i < mapLightCount; ++i)
		{
			api->DrawLightInstance(mapLights[i]);
		}

		SubmitFixtureLights();
		SubmitLaserLights();
		SubmitAuthoredLights(world);

		if (viewInfo)
		{
			SubmitDynamicLights(backEnd, world, reinterpret_cast<std::uintptr_t>(viewInfo), colourScale);
		}

		++stats.submittedFrames;
	}

	void ReleaseLights()
	{
		stats.hasSun = false;

		if (!api)
		{
			ForgetApiDevice();
			return;
		}

		if (sunLight)
		{
			DestroyCountedLight(sunLight);
			sunLight = nullptr;
		}

		DestroyMapLights();
		DestroyDynamicLights();
		DestroyFixtureLights();
		DestroyLaserLights();
		DestroyAuthoredLights();
		authoredSettingsVersion = 0;

		ForgetApiDevice();
		stats.hasSun = false;
	}

	void SetLightTuning(const LightTuning& settings)
	{
		LightTuning clamped = settings;
		clamped.spotBrightness = std::clamp(settings.spotBrightness, 0.0f, 1000.0f);
		clamped.omniBrightness = std::clamp(settings.omniBrightness, 0.0f, 1000.0f);
		clamped.sphereRadius = std::clamp(settings.sphereRadius, 0.5f, 1000.0f);
		clamped.coneOffsetDeg = std::clamp(settings.coneOffsetDeg, -45.0f, 45.0f);
		clamped.softnessOffset = std::clamp(settings.softnessOffset, -1.0f, 1.0f);
		clamped.focusExponent = std::clamp(settings.focusExponent, 0.0f, 100.0f);
		clamped.volumetric = std::clamp(settings.volumetric, 0.0f, 1000.0f);
		clamped.dimLightCutoff = std::clamp(settings.dimLightCutoff, 0.0f, 100000.0f);

		if (std::memcmp(&clamped, &tuning, sizeof(tuning)) != 0)
		{
			tuning = clamped;
			++tuningVersion;
		}
	}

	LightTuning GetLightTuning()
	{
		return tuning;
	}

	std::vector<MapLightInfo> GetMapLightInfo()
	{
		std::vector<MapLightInfo> result = mapLightInfo;

		for (const AuthoredInstance& instance : authoredInstances)
		{
			MapLightInfo info;
			info.hash = instance.hash;
			std::memcpy(info.origin, instance.position, sizeof(info.origin));
			info.radius = instance.shape.radius;
			info.isSpot = instance.shape.hasDirection;
			info.isAuthored = true;
			result.push_back(info);
		}

		for (int i = 0; i < fixtureLightCount; ++i)
		{
			const FixtureEdit edit = EffectiveFixtureEdit(fixtureHashes[i], fixtureCells[i]);

			MapLightInfo info;
			info.hash = fixtureHashes[i];
			std::memcpy(info.origin, fixturePositions[i], sizeof(info.origin));
			info.radius = fixtureRange;
			info.isFixture = true;
			info.isIgnored = !edit.isOn;
			info.isChanged = edit.isOn && (edit.brightness != 1.0f || edit.hasColour || edit.size != sphereRadius);
			result.push_back(info);
		}

		return result;
	}

	void SetLightBrightness(float brightness)
	{
		lightBrightness = std::clamp(brightness, minimumLightBrightness, maximumLightBrightness);
	}

	float GetLightBrightness()
	{
		return lightBrightness;
	}

	void SetSunSettings(const SunSettings& settings)
	{
		sunSettings = settings;
		sunSettings.brightness = std::clamp(settings.brightness, 0.0f, 1000.0f);
		sunSettings.maxRadiance = std::clamp(settings.maxRadiance, 0.0f, 10000.0f);
		sunSettings.pitchDeg = std::clamp(settings.pitchDeg, -10.0f, 90.0f);

		for (int channel = 0; channel < 3; ++channel)
		{
			sunSettings.tint[channel] = std::clamp(settings.tint[channel], 0.0f, 1.0f);
		}
	}

	SunSettings GetSunSettings()
	{
		return sunSettings;
	}

	void SetAllEffectLights(bool isEnabled)
	{
		isAllEffectLights = isEnabled;
	}

	bool IsAllEffectLights()
	{
		return isAllEffectLights;
	}

	LightStats GetLightStats()
	{
		LightStats result = stats;
		result.lightsCreated = lightsCreated;
		result.lightsDestroyed = lightsDestroyed;
		return result;
	}
}
