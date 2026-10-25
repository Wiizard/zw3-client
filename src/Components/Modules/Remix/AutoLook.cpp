#include "STDInclude.hpp"

#include "AutoLook.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "Core.hpp"
#include "Engine.hpp"
#include "Fog.hpp"
#include "Iw4.hpp"
#include "Lights.hpp"
#include "Read.hpp"
#include "Scene.hpp"
#include "Settings.hpp"
#include "Sky.hpp"
#include "Weather.hpp"

namespace Components::Remix
{
	// Frames after a map loads before the look is picked anyway, if the sun or sky never report (about 10 seconds).
	static constexpr unsigned maximumWaitFrames = 600;
	static constexpr float nightSkyLuminance = 0.04f;
	static constexpr float nightSunLuminance = 0.15f;
	static constexpr float duskSkyLuminance = 0.15f;
	// Sine of 15 degrees: a sun lower than this is dusk or dawn.
	static constexpr float duskSunHeight = 0.26f;
	static constexpr float indoorShare = 0.35f;
	static constexpr float foggyOpacity = 0.5f;
	static constexpr float foggyHalfway = 2000.0f;

	static MapTraits traits;
	static std::uintptr_t trackedWorld = 0;
	static unsigned waitedFrames = 0;
	static bool isDecided = false;

	// Share of the map's cells that have sun lit surfaces (GfxWorld cellHasSunLitSurfsBits, one bit per cell, sized
	// (cellCount + 31) >> 5 words as IGfxWorld saves it), or -1 when the map has none.
	static float ReadOutdoorShare()
	{
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!world)
		{
			return -1.0f;
		}

		const int cellCount = ReadAt<int>(world + Iw4::worldCellCount);
		const auto bits = ReadAt<std::uintptr_t>(world + Iw4::worldCellHasSunLitSurfsBits);

		if (cellCount <= 0 || !bits)
		{
			return -1.0f;
		}

		int lit = 0;

		for (int cell = 0; cell < cellCount; ++cell)
		{
			const auto word = ReadAt<std::uint32_t>(bits + static_cast<std::uintptr_t>(cell >> 5) * sizeof(std::uint32_t));
			lit += (word >> (cell & 31)) & 1;
		}

		return static_cast<float>(lit) / static_cast<float>(cellCount);
	}

	static void Classify()
	{
		const LightStats lights = GetLightStats();
		const SkyStats sky = GetSkyStats();
		const FogStats fog = GetFogStats();

		traits = {};
		traits.isKnown = true;
		traits.sunLuminance = lights.hasSun ? 0.2126f * lights.mapSunRadiance[0] + 0.7152f * lights.mapSunRadiance[1]
			+ 0.0722f * lights.mapSunRadiance[2] : 0.0f;
		// sunDirection is the way the light travels, so a high sun points down.
		traits.sunHeight = lights.hasSun ? -lights.sunDirection[2] : 0.0f;
		traits.skyLuminance = sky.hasSky ? sky.skyLuminance : 0.0f;
		traits.fogHalfway = fog.halfway;
		traits.fogOpacity = fog.maxOpacity;
		traits.outdoorShare = ReadOutdoorShare();
		traits.isRaining = IsRaining();

		const bool isDarkSky = sky.didReadSky && traits.skyLuminance < nightSkyLuminance;
		const bool isWeakSun = !lights.hasSun || traits.sunLuminance < nightSunLuminance;

		if (isDarkSky && isWeakSun)
		{
			traits.time = MapTime::Night;
		}
		else if ((sky.didReadSky && traits.skyLuminance < duskSkyLuminance) || (lights.hasSun && traits.sunHeight < duskSunHeight))
		{
			traits.time = MapTime::Dusk;
		}

		traits.isIndoor = traits.outdoorShare >= 0.0f && traits.outdoorShare < indoorShare;
		traits.isFoggy = fog.hasMapFog && fog.maxOpacity >= foggyOpacity && fog.halfway > 0.0f && fog.halfway < foggyHalfway;

		static constexpr const char* timeNames[] = { "day", "dusk", "night" };
		char text[256];
		std::snprintf(text, sizeof(text), "%s, %s%s%s (sun %.2f at %.0f deg, sky %.3f, %.0f%% sun lit cells)",
			timeNames[static_cast<int>(traits.time)], traits.isIndoor ? "mostly indoors" : "mostly outdoors",
			traits.isFoggy ? ", foggy" : "", traits.isRaining ? ", rain" : "", traits.sunLuminance,
			std::asin(std::clamp(traits.sunHeight, -1.0f, 1.0f)) * 57.29578f, traits.skyLuminance,
			std::max(traits.outdoorShare, 0.0f) * 100.0f);
		traits.summary = text;
	}

	// Starting points per kind of map, on top of remix.ini. Exposure target is Remix Plus's auto exposure middle grey
	// (higher is brighter); shadow lift and outdoor EV are zw3's. Values follow the hand tuned Kino file.
	static void ApplyTraits(Core::Config& config)
	{
		switch (traits.time)
		{
		case MapTime::Night:
			config.remixPlusExposureTarget = 0.35f;
			config.remixShadowLift = std::max(config.remixShadowLift, 0.05f);
			config.remixBloomThreshold = std::min(config.remixBloomThreshold, 0.6f);
			break;

		case MapTime::Dusk:
			config.remixPlusExposureTarget = 0.25f;
			config.remixShadowLift = std::max(config.remixShadowLift, 0.03f);
			break;

		case MapTime::Day:
			config.remixPlusExposureTarget = 0.18f;
			break;
		}

		// Mostly indoor maps: outdoors is the bright exception, so it is brought down less and rooms get more lift.
		if (traits.isIndoor)
		{
			config.remixOutdoorEv = traits.time == MapTime::Day ? 0.5f : 1.0f;
			config.remixShadowLift = std::max(config.remixShadowLift, 0.04f);
		}

		if (traits.isFoggy)
		{
			config.remixHaze = true;
			config.remixMapFog = true;
		}

		if (config.remixWetMode == 0)
		{
			config.remixWetMode = 1;
		}
	}

	void UpdateAutoLook()
	{
		const std::uintptr_t world = GetMapIdentity();

		if (world != trackedWorld)
		{
			trackedWorld = world;
			waitedFrames = 0;
			isDecided = false;
			traits = {};
		}

		if (!world || isDecided)
		{
			return;
		}

		const bool isReady = GetSkyStats().submissions > 0 && GetLightStats().submittedFrames > 0;

		if (!isReady && ++waitedFrames < maximumWaitFrames)
		{
			return;
		}

		isDecided = true;
		Classify();

		const Core::Config& current = Core::GetConfig();

		if (Core::HasMapConfig() || !current.remixAutoLook)
		{
			Log() << "remix: " << Core::GetActiveMap() << " reads as " << traits.summary << "; its own settings are kept";
			return;
		}

		Core::Config& config = Core::MutableConfig();
		ApplyTraits(config);
		ApplyConfig(config);
		traits.isApplied = true;

		Log() << "remix: " << Core::GetActiveMap() << " has no map file and reads as " << traits.summary
			  << ", starting from a matching look (Save for this map keeps it)";
	}

	MapTraits GetMapTraits()
	{
		return traits;
	}
}
