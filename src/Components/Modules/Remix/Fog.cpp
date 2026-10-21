#include "STDInclude.hpp"

#include "Fog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "Core.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Device.hpp"
#include "Api.hpp"
#include "MapSettings.hpp"
#include "Options.hpp"
#include "Lights.hpp"
#include "Look.hpp"
#include "Read.hpp"

namespace Components::Remix
{
	struct MapFog
	{
		float start;
		float density;
		float maxOpacity;
		float colour[3];
		bool hasSunFog;
		float sunFadeDegrees;
	};

	struct VolumeSettings
	{
		float transmittance;
		float distanceMeters;
		float albedo[3];
		float anisotropy;
	};

	static constexpr VolumeSettings baseline = { 0.993f, 10.0f, { 0.999f, 0.999f, 0.999f }, 0.3f };
	static constexpr float farOpacityCap = 0.15f;
	static constexpr float minimumFarOpacity = 0.02f;
	static constexpr float maximumFarOpacity = 0.9f;
	static constexpr float maximumFogAlbedo = 0.75f;
	static constexpr float minimumFogAlbedo = 0.25f;
	static constexpr float fogBrightnessScale = 0.8f;
	static constexpr float fogSaturation = 0.5f;
	static constexpr float unitsPerRemixMeter = 100.0f;
	static constexpr float minimumStrength = 0.0f;
	static constexpr float maximumStrength = 6.0f;
	static constexpr ULONGLONG submitIntervalMs = 250;
	static constexpr int sunLobeCount = 4;
	static constexpr float sunLobeDegrees[sunLobeCount] = { 23.0f, 31.0f, 43.0f, 60.0f };
	static constexpr float sunLobeAnisotropy[sunLobeCount] = { 0.6f, 0.5f, 0.4f, 0.3f };
	static constexpr float godRayAnisotropy = 0.65f;

	static FogStats stats;
	static VolumeSettings submitted = baseline;
	static bool hasSubmitted = false;
	static ULONGLONG lastSubmitTick = 0;

	static float SrgbToLinear(float channel)
	{
		if (channel <= 0.03928f)
		{
			return channel / 12.92f;
		}

		return std::pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	static float LinearToSrgb(float channel)
	{
		if (channel <= 0.0031308f)
		{
			return channel * 12.92f;
		}

		return 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
	}

	static float AnisotropyForSunFade(float fadeDegrees)
	{
		if (fadeDegrees <= sunLobeDegrees[0])
		{
			return sunLobeAnisotropy[0];
		}

		for (int i = 1; i < sunLobeCount; ++i)
		{
			if (fadeDegrees <= sunLobeDegrees[i])
			{
				const float blend = (fadeDegrees - sunLobeDegrees[i - 1]) / (sunLobeDegrees[i] - sunLobeDegrees[i - 1]);
				return sunLobeAnisotropy[i - 1] + (sunLobeAnisotropy[i] - sunLobeAnisotropy[i - 1]) * blend;
			}
		}

		return sunLobeAnisotropy[sunLobeCount - 1];
	}

	static bool TryReadMapFog(MapFog& fog)
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.fogSettingsCurrent == 0 || build.fogIndex == 0 || build.r_fogDvar == 0)
		{
			return false;
		}

		const auto fogDvar = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.r_fogDvar));
		const int fogIndex = ReadAt<int>(Engine::RebaseAddress(build.fogIndex));

		if (!fogDvar || fogIndex == 0 || ReadAt<std::uint8_t>(fogDvar + Iw4::dvarValueOffset) == 0)
		{
			return false;
		}

		const std::uintptr_t settings = Engine::RebaseAddress(build.fogSettingsCurrent);
		const auto packedColour = ReadAt<std::uint32_t>(settings + Iw4::fogColor);

		fog.start = ReadAt<float>(settings + Iw4::fogStart);
		fog.density = ReadAt<float>(settings + Iw4::fogDensity);
		fog.maxOpacity = ReadAt<float>(settings + Iw4::fogMaxOpacity);
		fog.colour[0] = static_cast<float>((packedColour >> 16) & 0xFF) / 255.0f;
		fog.colour[1] = static_cast<float>((packedColour >> 8) & 0xFF) / 255.0f;
		fog.colour[2] = static_cast<float>(packedColour & 0xFF) / 255.0f;
		fog.hasSunFog = ReadAt<std::uint8_t>(settings + Iw4::fogSunEnabled) != 0;
		fog.sunFadeDegrees = 0.5f * (ReadAt<float>(settings + Iw4::fogSunBeginFade)
			+ ReadAt<float>(settings + Iw4::fogSunEndFade));

		return fog.density > 0.0f && fog.maxOpacity > 0.0f;
	}

	static VolumeSettings VolumeForFog(const MapFog& fog, float strength)
	{
		const float halfway = std::log(2.0f) / fog.density;
		const float matchOpacity = std::min(fog.maxOpacity, 0.5f);
		const float matchDistance = std::max(fog.start + halfway, 1.0f);
		const float extinction = -std::log(1.0f - matchOpacity) / matchDistance * strength;
		const float farOpacity = std::clamp(std::min(fog.maxOpacity, farOpacityCap) * strength, minimumFarOpacity,
			maximumFarOpacity);
		const float distanceUnits = -std::log(1.0f - farOpacity) / (5.0f * extinction);

		VolumeSettings volume{};
		volume.transmittance = LinearToSrgb(std::pow(1.0f - farOpacity, 0.2f));
		volume.distanceMeters = distanceUnits / unitsPerRemixMeter;

		const LightStats lightStats = GetLightStats();
		float tint[3];
		float peak = 0.0f;

		for (int channel = 0; channel < 3; ++channel)
		{
			tint[channel] = SrgbToLinear(fog.colour[channel]);

			if (lightStats.hasSun && lightStats.mapSunRadiance[channel] > 0.001f)
			{
				tint[channel] /= lightStats.mapSunRadiance[channel];
			}

			peak = std::max(peak, tint[channel]);
		}

		const float luminance = 0.2126f * fog.colour[0] + 0.7152f * fog.colour[1] + 0.0722f * fog.colour[2];
		const float brightness = std::clamp(luminance * fogBrightnessScale, minimumFogAlbedo, maximumFogAlbedo);

		for (int channel = 0; channel < 3; ++channel)
		{
			float hue = 1.0f;

			if (peak > 0.0001f)
			{
				hue = tint[channel] / peak;
			}

			volume.albedo[channel] = brightness * (1.0f - fogSaturation * (1.0f - hue));
		}

		if (fog.hasSunFog)
		{
			volume.anisotropy = AnisotropyForSunFade(fog.sunFadeDegrees);
		}
		else
		{
			volume.anisotropy = baseline.anisotropy;
		}

		return volume;
	}

	static float FarOpacityOf(const VolumeSettings& volume)
	{
		return 1.0f - std::pow(SrgbToLinear(volume.transmittance), 5.0f);
	}

	static bool IsSameVolume(const VolumeSettings& left, const VolumeSettings& right)
	{
		if (std::fabs(left.transmittance - right.transmittance) > 0.001f)
		{
			return false;
		}

		if (std::fabs(left.distanceMeters - right.distanceMeters) > 0.01f * right.distanceMeters)
		{
			return false;
		}

		for (int channel = 0; channel < 3; ++channel)
		{
			if (std::fabs(left.albedo[channel] - right.albedo[channel]) > 0.005f)
			{
				return false;
			}
		}

		return std::fabs(left.anisotropy - right.anisotropy) <= 0.005f;
	}

	static void SubmitVolume(const remixapi_Interface* api, const VolumeSettings& volume)
	{
		char text[96];

		std::snprintf(text, sizeof(text), "%.4f, %.4f, %.4f", volume.transmittance, volume.transmittance,
			volume.transmittance);
		SendOption(api, "rtx.volumetrics.transmittanceColor", text);

		std::snprintf(text, sizeof(text), "%.3f", volume.distanceMeters);
		SendOption(api, "rtx.volumetrics.transmittanceMeasurementDistanceMeters", text);

		std::snprintf(text, sizeof(text), "%.4f, %.4f, %.4f", volume.albedo[0], volume.albedo[1], volume.albedo[2]);
		SendOption(api, "rtx.volumetrics.singleScatteringAlbedo", text);

		std::snprintf(text, sizeof(text), "%.3f", volume.anisotropy);
		SendOption(api, "rtx.volumetrics.anisotropy", text);
		Device::NoteFrameEvent("remix fog options");
	}

	void UpdateMapFog(IDirect3DDevice9* device)
	{
		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		MapFog fog{};
		const bool hasMapFog = TryReadMapFog(fog);
		VolumeSettings target = baseline;

		bool isEnabled = stats.isEnabled;
		float strength = stats.strength;
		const MapSettings& mapSettings = GetMapSettings();

		if (mapSettings.isFogEnabled)
		{
			isEnabled = *mapSettings.isFogEnabled;
		}

		if (mapSettings.fogStrength)
		{
			strength = std::clamp(*mapSettings.fogStrength, minimumStrength, maximumStrength);
		}

		const bool isMapFogApplied = hasMapFog && isEnabled && strength > 0.0f;

		if (isMapFogApplied)
		{
			target = VolumeForFog(fog, strength);
		}

		const LookSettings look = GetLookSettings();

		if (look.isGodRays)
		{
			target.transmittance = std::pow(target.transmittance, look.godRayStrength);
			target.anisotropy = std::max(target.anisotropy, godRayAnisotropy);
		}

		stats.hasMapFog = hasMapFog;

		if (hasMapFog)
		{
			stats.start = fog.start;
			stats.halfway = std::log(2.0f) / fog.density;
			stats.maxOpacity = fog.maxOpacity;
			stats.colour[0] = fog.colour[0];
			stats.colour[1] = fog.colour[1];
			stats.colour[2] = fog.colour[2];
			stats.hasSunFog = fog.hasSunFog;
		}

		const ULONGLONG now = GetTickCount64();

		if (hasSubmitted && (IsSameVolume(target, submitted) || now - lastSubmitTick < submitIntervalMs))
		{
			return;
		}

		SubmitVolume(api, target);

		submitted = target;
		hasSubmitted = true;
		lastSubmitTick = now;

		++stats.submissions;
		stats.farOpacity = FarOpacityOf(target);
		stats.distanceMeters = target.distanceMeters;
		stats.albedo[0] = target.albedo[0];
		stats.albedo[1] = target.albedo[1];
		stats.albedo[2] = target.albedo[2];
		stats.anisotropy = target.anisotropy;

		if (!isMapFogApplied)
		{
			Log() << "remix: no map fog in use, Remix gets our light haze (the rtx.conf we ship)";
			return;
		}

		Log() << "remix: map fog from " << fog.start << " units, halfway " << stats.halfway << " units, max opacity "
			  << fog.maxOpacity << ", colour " << fog.colour[0] << " " << fog.colour[1] << " " << fog.colour[2]
			  << ", sun fog " << fog.hasSunFog << " becomes Remix haze up to " << stats.farOpacity << " opacity over "
			  << target.distanceMeters << " m steps, albedo " << target.albedo[0] << " " << target.albedo[1] << " "
			  << target.albedo[2] << ", anisotropy " << target.anisotropy;
	}

	void SetMapFogEnabled(bool isEnabled)
	{
		stats.isEnabled = isEnabled;
		lastSubmitTick = 0;
	}

	void SetMapFogStrength(float strength)
	{
		stats.strength = std::clamp(strength, minimumStrength, maximumStrength);
	}

	FogStats GetFogStats()
	{
		return stats;
	}
}
