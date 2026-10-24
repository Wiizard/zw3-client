#include "STDInclude.hpp"

#include "AreaExposure.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Engine.hpp"
#include "Iw4.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	// GfxWorld lightGrid (GfxLightGrid, asserted in Structs.hpp): entries are { colorsIndex u16, primaryLightIndex u8,
	// needsTrace u8 }, colours are 56 rgb byte samples (168 bytes) per set.
	static constexpr std::uintptr_t worldLightGrid = 0x108;
	static constexpr std::uintptr_t gridLastSunPrimaryLightIndex = 0x4;
	static constexpr std::uintptr_t gridColorCount = 0x48;
	static constexpr std::uintptr_t gridColors = 0x50;
	static constexpr std::size_t gridColourStride = 168;
	static constexpr int gridColourSamples = 56;
	// The engine's light grid row lookup (called by its model lighting, sub_140045130): reads the grid's row data and
	// returns the four entries at a cell and the cell above, then the next column. Pure: no traces, no writes but
	// its outputs. Cells are (floor(x) + 131072) >> 5, (floor(y) + 131072) >> 5 and (floor(z) + 131072) >> 6.
	static constexpr std::uintptr_t lightGridRowLookupFunction = 0x140044DB0;
	static constexpr std::uintptr_t gridRowAxis = 0x14;
	static constexpr float smoothingSeconds = 0.8f;
	static constexpr float minimumGridLuminance = 1.0f / 1024.0f;
	static constexpr float lowestEv = -8.0f;
	static constexpr float highestEv = 16.0f;

	using LightGridRowLookup = void(__fastcall*)(std::uintptr_t grid, const int* cell, std::uintptr_t* entries,
		std::uint32_t* isInside);

	static AreaExposureState state;
	static std::uintptr_t measuredWorld = 0;
	static ULONGLONG lastTick = 0;
	static bool hasSmoothed = false;
	static float smoothedLog = 0.0f;
	static float smoothedSunlit = 0.0f;

	static float SrgbByteToLinear(std::uint8_t value)
	{
		return std::pow(static_cast<float>(value) / 255.0f, 2.2f);
	}

	static float ColourSetLog(std::uintptr_t colours, std::uint32_t index)
	{
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(colours + index * gridColourStride);
		float sum = 0.0f;

		for (int sample = 0; sample < gridColourSamples; ++sample)
		{
			sum += 0.2126f * SrgbByteToLinear(bytes[sample * 3]) + 0.7152f * SrgbByteToLinear(bytes[sample * 3 + 1])
				+ 0.0722f * SrgbByteToLinear(bytes[sample * 3 + 2]);
		}

		return std::log2(std::max(sum / gridColourSamples, minimumGridLuminance));
	}

	// Percentiles of every colour set in the grid, so each map's own dark and bright areas set the scale.
	static void MeasureMap(std::uintptr_t world)
	{
		measuredWorld = world;
		hasSmoothed = false;
		state = {};

		const std::uintptr_t grid = world + worldLightGrid;
		const auto count = ReadAt<std::uint32_t>(grid + gridColorCount);
		const auto colours = ReadAt<std::uintptr_t>(grid + gridColors);

		if (count == 0 || !colours)
		{
			return;
		}

		std::vector<float> logs(count);

		for (std::uint32_t index = 0; index < count; ++index)
		{
			logs[index] = ColourSetLog(colours, index);
		}

		std::sort(logs.begin(), logs.end());

		const auto at = [&logs](float share)
		{
			return logs[std::min(static_cast<std::size_t>(share * static_cast<float>(logs.size())), logs.size() - 1)];
		};

		state.hasGrid = true;
		state.mapDarkLog = at(0.1f);
		state.mapMedianLog = at(0.5f);
		state.mapBrightLog = at(0.9f);
	}

	static bool TryLookUp(std::uintptr_t grid, const int* cell, std::uintptr_t* entries)
	{
		__try
		{
			const auto lookup = reinterpret_cast<LightGridRowLookup>(Engine::RebaseAddress(lightGridRowLookupFunction));
			std::uint32_t isInside = 1;
			lookup(grid, cell, entries, &isInside);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	// Brightness (log2 of linear luminance) of the light grid around a point and the share of its entries the sun
	// reaches, from the eight surrounding entries as the engine gathers them.
	static bool TrySampleArea(std::uintptr_t world, const float* point, float& areaLog, float& sunlitShare)
	{
		const std::uintptr_t grid = world + worldLightGrid;
		const auto colours = ReadAt<std::uintptr_t>(grid + gridColors);
		const auto colourCount = ReadAt<std::uint32_t>(grid + gridColorCount);
		const auto lastSun = ReadAt<std::uint32_t>(grid + gridLastSunPrimaryLightIndex);
		const auto rowAxis = ReadAt<std::uint32_t>(grid + gridRowAxis);

		if (!colours || colourCount == 0 || rowAxis > 2)
		{
			return false;
		}

		int cell[3] = {
			(static_cast<int>(std::floor(point[0])) + 0x20000) >> 5,
			(static_cast<int>(std::floor(point[1])) + 0x20000) >> 5,
			(static_cast<int>(std::floor(point[2])) + 0x20000) >> 6,
		};

		std::uintptr_t entries[8] = {};

		if (!TryLookUp(grid, cell, entries))
		{
			return false;
		}

		++cell[rowAxis];

		if (!TryLookUp(grid, cell, entries + 4))
		{
			return false;
		}

		float logSum = 0.0f;
		int found = 0;
		int sunlit = 0;

		for (const std::uintptr_t entry : entries)
		{
			if (!entry)
			{
				continue;
			}

			const auto colourIndex = ReadAt<std::uint16_t>(entry);
			const auto primaryLight = ReadAt<std::uint8_t>(entry + 2);

			if (colourIndex >= colourCount)
			{
				continue;
			}

			logSum += ColourSetLog(colours, colourIndex);
			++found;

			if (primaryLight != 0 && primaryLight <= lastSun)
			{
				++sunlit;
			}
		}

		if (found == 0)
		{
			return false;
		}

		areaLog = logSum / static_cast<float>(found);
		sunlitShare = static_cast<float>(sunlit) / static_cast<float>(found);
		return true;
	}

	bool TryGetAreaExposure(const AreaExposureSettings& settings, float rangeMinEv, float rangeMaxEv, float& minEv,
		float& maxEv)
	{
		state.isActive = false;

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!settings.isEnabled || !world || !GetMapIdentity())
		{
			return false;
		}

		if (world != measuredWorld)
		{
			MeasureMap(world);
		}

		float camera[3];
		float areaLog = 0.0f;
		float sunlitShare = 0.0f;

		if (!state.hasGrid || !TryGetCameraOrigin(camera) || !TrySampleArea(world, camera, areaLog, sunlitShare))
		{
			return false;
		}

		const ULONGLONG now = GetTickCount64();
		const float seconds = lastTick == 0 ? 0.0f : std::min(static_cast<float>(now - lastTick) / 1000.0f, 0.25f);
		lastTick = now;

		if (!hasSmoothed)
		{
			smoothedLog = areaLog;
			smoothedSunlit = sunlitShare;
			hasSmoothed = true;
		}

		const float blend = 1.0f - std::exp(-seconds / smoothingSeconds);
		smoothedLog += (areaLog - smoothedLog) * blend;
		smoothedSunlit += (sunlitShare - smoothedSunlit) * blend;

		// Scene EV rises with brightness: a brighter area than the map's median moves the window up (darker
		// exposure), a darker one moves it down, by the strength's share of the difference.
		const float rangeMiddle = 0.5f * (rangeMinEv + rangeMaxEv);
		const float centre = std::clamp(rangeMiddle + settings.strength * (smoothedLog - state.mapMedianLog)
			+ smoothedSunlit * settings.sunlitEv, lowestEv, highestEv);
		const float window = std::max(settings.windowEv, 0.05f);

		// Rounded so the option is not sent every frame for a hair of difference.
		minEv = std::round((centre - window) * 20.0f) / 20.0f;
		maxEv = std::round((centre + window) * 20.0f) / 20.0f;

		state.isActive = true;
		state.areaLog = smoothedLog;
		state.sunlitShare = smoothedSunlit;
		state.centreEv = centre;
		state.minEv = minEv;
		state.maxEv = maxEv;
		return true;
	}

	AreaExposureState GetAreaExposureState()
	{
		return state;
	}
}
