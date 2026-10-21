#include "STDInclude.hpp"

#include "Sky.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>

#include "Core.hpp"
#include "Device.hpp"
#include "Iw4.hpp"
#include "Api.hpp"
#include "Options.hpp"
#include "Dxt.hpp"
#include "Fog.hpp"
#include "Lights.hpp"
#include "Look.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	static constexpr float defaultSkyLuminance = 0.35f;
	static constexpr float clearHazeShare = 0.2f;
	static constexpr float hazeSharePerFogOpacity = 0.3f;
	static constexpr float maximumHazeShare = 0.45f;
	static constexpr float minimumSunElevation = 0.1f;
	static constexpr float minimumBrightness = 0.5f;
	static constexpr float maximumBrightness = 4.0f;
	static constexpr UINT sampledFaceSize = 8;
	static constexpr int faceCount = 6;
	static constexpr int brightFaces = 5;

	static SkyStats stats;
	static std::uintptr_t skyMaterial = 0;
	static std::uintptr_t computedMaterial = 0;
	static float computedSunRadiance = 0.0f;
	static float skyScale = 1.0f;
	static float computedSkyScale = 1.0f;
	static constexpr float noSunBrightness = 1.0f;
	static bool isNoSunSent = false;
	static std::uintptr_t skyMapIdentity = 0;

	static float SrgbToLinear(float channel)
	{
		if (channel <= 0.04045f)
		{
			return channel / 12.92f;
		}

		return std::pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	static IDirect3DCubeTexture9* FindSkyCube(std::uintptr_t material)
	{
		const auto textureCount = ReadAt<std::uint8_t>(material + Iw4::materialTextureCount);
		const auto textures = ReadAt<std::uintptr_t>(material + Iw4::materialTextureTable);

		if (!textures)
		{
			return nullptr;
		}

		for (std::uint8_t i = 0; i < textureCount; ++i)
		{
			const auto image = ReadAt<std::uintptr_t>(textures + i * Iw4::textureDefStride + Iw4::textureDefImage);

			if (!image)
			{
				continue;
			}

			auto* texture = Device::Unwrap(ReadAt<IDirect3DBaseTexture9*>(image + Iw4::gfxImageTexture));

			if (texture && texture->GetType() == D3DRTYPE_CUBETEXTURE)
			{
				return static_cast<IDirect3DCubeTexture9*>(texture);
			}
		}

		return nullptr;
	}

	static bool TryReadFaceLuminance(IDirect3DCubeTexture9* cube, D3DCUBEMAP_FACES face, UINT level, float& luminance)
	{
		D3DSURFACE_DESC desc{};

		if (FAILED(cube->GetLevelDesc(level, &desc)))
		{
			return false;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(cube->LockRect(face, level, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		DecodedImage image;
		const bool didDecode = TryDecodeImage(static_cast<const unsigned char*>(locked.pBits), static_cast<UINT>(locked.Pitch),
			desc.Format, desc.Width, desc.Height, image);
		cube->UnlockRect(face, level);

		if (!didDecode || image.texels.empty())
		{
			return false;
		}

		double sum = 0.0;

		for (const std::uint32_t texel : image.texels)
		{
			sum += 0.2126 * SrgbToLinear(Channel(texel, 16) / 255.0f) + 0.7152 * SrgbToLinear(Channel(texel, 8) / 255.0f)
				+ 0.0722 * SrgbToLinear(Channel(texel, 0) / 255.0f);
		}

		luminance = static_cast<float>(sum / static_cast<double>(image.texels.size()));
		return true;
	}

	static bool TryMeasureSky(std::uintptr_t material, float& luminance)
	{
		IDirect3DCubeTexture9* cube = FindSkyCube(material);

		if (!cube)
		{
			return false;
		}

		UINT level = 0;
		const UINT levels = cube->GetLevelCount();

		for (UINT candidate = 0; candidate < levels; ++candidate)
		{
			D3DSURFACE_DESC desc{};

			if (SUCCEEDED(cube->GetLevelDesc(candidate, &desc)) && desc.Width >= sampledFaceSize)
			{
				level = candidate;
			}
		}

		float faces[faceCount] = {};

		for (int face = 0; face < faceCount; ++face)
		{
			if (!TryReadFaceLuminance(cube, static_cast<D3DCUBEMAP_FACES>(face), level, faces[face]))
			{
				return false;
			}
		}

		std::sort(std::begin(faces), std::end(faces), std::greater<float>());

		float sum = 0.0f;

		for (int face = 0; face < brightFaces; ++face)
		{
			sum += faces[face];
		}

		luminance = sum / static_cast<float>(brightFaces);
		return luminance > 0.001f;
	}

	void NoteSkyMaterial(std::uintptr_t material)
	{
		skyMaterial = material;
	}

	void ForgetSkyMaterial()
	{
		skyMaterial = 0;
		computedMaterial = 0;
		isNoSunSent = false;
	}

	void UpdateSkyBrightness(IDirect3DDevice9* device)
	{
		const std::uintptr_t mapIdentity = GetMapIdentity();

		if (mapIdentity != skyMapIdentity)
		{
			skyMapIdentity = mapIdentity;
			ForgetSkyMaterial();
		}

		const LightStats lightStats = GetLightStats();

		if (!skyMaterial || !lightStats.hasSun)
		{
			const LookSettings look = GetLookSettings();
			float noSunTarget = noSunBrightness * skyScale;

			if (look.isNight)
			{
				noSunTarget = look.nightSky * skyScale;
			}

			if (isNoSunSent && noSunTarget == stats.brightness)
			{
				return;
			}

			const remixapi_Interface* noSunApi = TryGetApi(device);

			if (!noSunApi)
			{
				return;
			}

			char noSunText[32];
			std::snprintf(noSunText, sizeof(noSunText), "%.3f", noSunTarget);
			SendOption(noSunApi, "rtx.skyBrightness", noSunText);

			computedSkyScale = skyScale;
			computedMaterial = 0;
			isNoSunSent = true;
			stats.brightness = noSunTarget;
			return;
		}

		const float sunRadiance = 0.2126f * lightStats.sunRadiance[0] + 0.7152f * lightStats.sunRadiance[1]
			+ 0.0722f * lightStats.sunRadiance[2];

		if (skyMaterial == computedMaterial && std::fabs(sunRadiance - computedSunRadiance) < 0.01f
			&& skyScale == computedSkyScale)
		{
			return;
		}

		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		computedMaterial = skyMaterial;
		computedSunRadiance = sunRadiance;
		computedSkyScale = skyScale;
		isNoSunSent = false;

		float skyLuminance = defaultSkyLuminance;
		stats.didReadSky = TryMeasureSky(skyMaterial, skyLuminance);

		if (!stats.didReadSky)
		{
			skyLuminance = defaultSkyLuminance;
		}

		const FogStats fogStats = GetFogStats();
		float hazeShare = clearHazeShare;

		if (fogStats.hasMapFog)
		{
			hazeShare = std::min(clearHazeShare + hazeSharePerFogOpacity * fogStats.maxOpacity, maximumHazeShare);
		}

		const float sunElevation = std::max(-lightStats.sunDirection[2], minimumSunElevation);
		const float brightness = std::clamp(hazeShare / (1.0f - hazeShare) * sunRadiance * sunElevation / skyLuminance,
			minimumBrightness, maximumBrightness) * skyScale;

		char text[32];
		std::snprintf(text, sizeof(text), "%.3f", brightness);
		SendOption(api, "rtx.skyBrightness", text);

		stats.hasSky = true;
		stats.skyLuminance = skyLuminance;
		stats.sunElevation = sunElevation;
		stats.hazeShare = hazeShare;
		stats.brightness = brightness;
		++stats.submissions;

		Log() << "remix: sky brightness " << brightness << " so the map's own sky gives about " << hazeShare * 100.0f
			  << "% of outdoor light (sky luminance " << skyLuminance << ", read " << stats.didReadSky << ", sun "
			  << sunRadiance << " at elevation " << sunElevation << ")";
	}

	void SetSkyScale(float scale)
	{
		skyScale = std::clamp(scale, 0.0f, 4.0f);
	}

	float GetSkyScale()
	{
		return skyScale;
	}

	SkyStats GetSkyStats()
	{
		return stats;
	}
}
