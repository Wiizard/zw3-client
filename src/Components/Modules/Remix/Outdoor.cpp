#include "STDInclude.hpp"

#include "Outdoor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <vector>

#include "Core.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Dxt.hpp"
#include "Read.hpp"

namespace Components::Remix
{
	static constexpr int cloudSampleCount = 9;
	static constexpr int outdoorSamplesToDraw = 5;
	static constexpr float cloudSampleOffset = 0.5f;
	static constexpr std::size_t maxLoggedTechniqueSets = 32;

	static OutdoorStats stats;
	static std::uintptr_t loadedWorld = 0;
	static std::uintptr_t loadedImage = 0;
	static std::vector<float> heights;
	static float lookup[16] = {};
	static std::unordered_set<std::uintptr_t> seenMaterials;
	static std::unordered_set<std::string> loggedTechniqueSets;

	static float HalfToFloat(std::uint16_t half)
	{
		const std::uint32_t sign = (half & 0x8000u) << 16;
		const std::uint32_t exponent = (half >> 10) & 0x1Fu;
		const std::uint32_t mantissa = half & 0x3FFu;
		std::uint32_t bits = sign;

		if (exponent == 0x1Fu)
		{
			bits |= 0x7F800000u | (mantissa << 13);
		}
		else if (exponent != 0)
		{
			bits |= ((exponent + 112u) << 23) | (mantissa << 13);
		}
		else if (mantissa != 0)
		{
			float value = static_cast<float>(mantissa) / 16777216.0f;

			if (sign != 0)
			{
				value = -value;
			}

			return value;
		}

		float value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	static bool TryReadSingleChannel(const unsigned char* row, D3DFORMAT format, UINT x, float& height)
	{
		switch (format)
		{
		case D3DFMT_L8:
		case D3DFMT_A8:
			height = static_cast<float>(row[x]) / 255.0f;
			return true;
		case D3DFMT_A8L8:
			height = static_cast<float>(row[x * 2]) / 255.0f;
			return true;
		case D3DFMT_L16:
		{
			std::uint16_t texel;
			std::memcpy(&texel, row + x * 2, sizeof(texel));
			height = static_cast<float>(texel) / 65535.0f;
			return true;
		}
		case D3DFMT_R16F:
		{
			std::uint16_t texel;
			std::memcpy(&texel, row + x * 2, sizeof(texel));
			height = HalfToFloat(texel);
			return true;
		}
		case D3DFMT_R32F:
			std::memcpy(&height, row + x * 4, sizeof(height));
			return true;
		default:
			return false;
		}
	}

	static bool TryReadHeights(const D3DLOCKED_RECT& locked, const D3DSURFACE_DESC& desc)
	{
		const auto* bits = static_cast<const unsigned char*>(locked.pBits);
		heights.resize(static_cast<std::size_t>(desc.Width) * desc.Height);

		if (IsDecodableFormat(desc.Format))
		{
			DecodedImage image;

			if (!TryDecodeImage(bits, static_cast<UINT>(locked.Pitch), desc.Format, desc.Width, desc.Height, image))
			{
				return false;
			}

			for (std::size_t i = 0; i < heights.size(); ++i)
			{
				heights[i] = static_cast<float>(Channel(image.texels[i], 16)) / 255.0f;
			}

			return true;
		}

		for (UINT y = 0; y < desc.Height; ++y)
		{
			const unsigned char* row = bits + static_cast<std::size_t>(y) * locked.Pitch;

			for (UINT x = 0; x < desc.Width; ++x)
			{
				if (!TryReadSingleChannel(row, desc.Format, x, heights[static_cast<std::size_t>(y) * desc.Width + x]))
				{
					return false;
				}
			}
		}

		return true;
	}

	static void LoadOutdoorMap(std::uintptr_t world)
	{
		heights.clear();
		stats.hasMap = false;
		stats.width = 0;
		stats.height = 0;
		stats.format = 0;
		stats.cloudsTested = 0;
		stats.cloudsCulled = 0;
		stats.largestScale = 0.0f;

		const auto image = ReadAt<std::uintptr_t>(world + Iw4::worldOutdoorImage);

		if (!image)
		{
			Log() << "remix: this map has no outdoor map, rain and snow draw everywhere";
			return;
		}

		auto* texture = ReadAt<IDirect3DTexture9*>(image + Iw4::gfxImageTexture);
		D3DSURFACE_DESC desc{};

		if (!texture || FAILED(texture->GetLevelDesc(0, &desc)))
		{
			Log() << "remix: the outdoor map has no texture, rain and snow draw everywhere";
			return;
		}

		stats.width = static_cast<int>(desc.Width);
		stats.height = static_cast<int>(desc.Height);
		stats.format = static_cast<unsigned>(desc.Format);

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		{
			Log() << "remix: the outdoor map (" << desc.Width << "x" << desc.Height << ", format " << desc.Format
				  << ", pool " << desc.Pool << ") cannot be read, rain and snow draw everywhere";
			return;
		}

		const bool didRead = TryReadHeights(locked, desc);
		texture->UnlockRect(0);

		if (!didRead)
		{
			heights.clear();
			Log() << "remix: the outdoor map format " << desc.Format << " is not one we read, rain and snow draw"
					 " everywhere";
			return;
		}

		std::memcpy(lookup, reinterpret_cast<const void*>(world + Iw4::worldOutdoorLookupMatrix), sizeof(lookup));

		stats.heightSign = 1.0f;

		if (lookup[10] < 0.0f)
		{
			stats.heightSign = -1.0f;
		}

		const auto [lowest, highest] = std::minmax_element(heights.begin(), heights.end());
		stats.hasMap = true;

		Log() << "remix: outdoor map " << desc.Width << "x" << desc.Height << " format " << desc.Format << ", heights "
			  << *lowest << " to " << *highest << ", lookup z per unit " << lookup[10] << "; rain and snow clouds"
				 " under cover are skipped";
	}

	static void EnsureOutdoorMap()
	{
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!world)
		{
			return;
		}

		const auto image = ReadAt<std::uintptr_t>(world + Iw4::worldOutdoorImage);
		float currentLookup[16];
		std::memcpy(currentLookup, reinterpret_cast<const void*>(world + Iw4::worldOutdoorLookupMatrix),
			sizeof(currentLookup));

		const bool isSameMap = world == loadedWorld && image == loadedImage
			&& std::memcmp(currentLookup, lookup, sizeof(lookup)) == 0;

		if (isSameMap)
		{
			return;
		}

		loadedWorld = world;
		loadedImage = image;
		std::memcpy(lookup, currentLookup, sizeof(lookup));
		LoadOutdoorMap(world);
	}

	static bool IsPointOutdoors(const float* point)
	{
		const float u = point[0] * lookup[0] + point[1] * lookup[4] + point[2] * lookup[8] + lookup[12];
		const float v = point[0] * lookup[1] + point[1] * lookup[5] + point[2] * lookup[9] + lookup[13];
		const float depth = point[0] * lookup[2] + point[1] * lookup[6] + point[2] * lookup[10] + lookup[14];

		const int x = static_cast<int>(std::clamp(u, 0.0f, 1.0f) * static_cast<float>(stats.width - 1));
		const int y = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * static_cast<float>(stats.height - 1));

		return (depth - heights[static_cast<std::size_t>(y) * stats.width + x]) * stats.heightSign >= 0.0f;
	}

	static void QuatToAxis(const float* quat, float axis[3][3])
	{
		const float x = quat[0];
		const float y = quat[1];
		const float z = quat[2];
		const float w = quat[3];

		axis[0][0] = 1.0f - 2.0f * (y * y + z * z);
		axis[0][1] = 2.0f * (x * y + w * z);
		axis[0][2] = 2.0f * (x * z - w * y);

		axis[1][0] = 2.0f * (x * y - w * z);
		axis[1][1] = 1.0f - 2.0f * (x * x + z * z);
		axis[1][2] = 2.0f * (y * z + w * x);

		axis[2][0] = 2.0f * (x * z + w * y);
		axis[2][1] = 2.0f * (y * z - w * x);
		axis[2][2] = 1.0f - 2.0f * (x * x + y * y);
	}

	bool IsOutdoorMaskedMaterial(std::uintptr_t material)
	{
		if (!material)
		{
			return false;
		}

		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (!techniqueSet)
		{
			return false;
		}

		const char* name = ReadAt<const char*>(techniqueSet);

		if (!name)
		{
			return false;
		}

		// Only rain and snow are kept outdoors. Other clouds with an outdoor technique (dust, embers, motes, fog wisps)
		// are also placed indoors by maps, and the height map test removed them there.
		const char* materialName = ReadAt<const char*>(material);
		bool isPrecipitation = false;

		for (const char* word : { "rain", "snow", "sleet", "drizzle", "hail", "blizzard" })
		{
			isPrecipitation = isPrecipitation || (materialName && std::strstr(materialName, word) != nullptr);
		}

		const bool isMasked = std::strstr(name, "outdoor") != nullptr && isPrecipitation;

		if (seenMaterials.insert(material).second)
		{
			if (isMasked)
			{
				++stats.maskedMaterials;
			}

			if (loggedTechniqueSets.size() < maxLoggedTechniqueSets && loggedTechniqueSets.insert(name).second)
			{
				Log() << "remix: particle cloud technique set " << name << " (" << (materialName ? materialName : "?")
					  << "), outdoor mask " << isMasked;
			}
		}

		return isMasked;
	}

	bool IsCloudOutdoors(std::uintptr_t cloud)
	{
		if (!stats.isEnabled)
		{
			return true;
		}

		EnsureOutdoorMap();

		if (!stats.hasMap)
		{
			return true;
		}

		float quat[4];
		float origin[3];
		std::memcpy(quat, reinterpret_cast<const void*>(cloud + Iw4::particleCloudQuat), sizeof(quat));
		std::memcpy(origin, reinterpret_cast<const void*>(cloud + Iw4::particleCloudOrigin), sizeof(origin));
		const float scale = ReadAt<float>(cloud + Iw4::particleCloudScale);

		float axis[3][3];
		QuatToAxis(quat, axis);

		int outdoorSamples = 0;

		if (IsPointOutdoors(origin))
		{
			++outdoorSamples;
		}

		for (int corner = 0; corner < cloudSampleCount - 1; ++corner)
		{
			float local[3];

			for (int component = 0; component < 3; ++component)
			{
				local[component] = cloudSampleOffset;

				if ((corner >> component) & 1)
				{
					local[component] = -cloudSampleOffset;
				}
			}

			float point[3];

			for (int component = 0; component < 3; ++component)
			{
				point[component] = origin[component] + scale * (local[0] * axis[0][component]
					+ local[1] * axis[1][component] + local[2] * axis[2][component]);
			}

			if (IsPointOutdoors(point))
			{
				++outdoorSamples;
			}
		}

		++stats.cloudsTested;
		stats.largestScale = std::max(stats.largestScale, scale);

		if (outdoorSamples >= outdoorSamplesToDraw)
		{
			return true;
		}

		++stats.cloudsCulled;
		return false;
	}

	bool IsWeatherPointOutdoors(const float* point)
	{
		if (!stats.isEnabled)
		{
			return true;
		}

		EnsureOutdoorMap();

		if (!stats.hasMap)
		{
			return true;
		}

		return IsPointOutdoors(point);
	}

	bool TryTestPointOutdoors(const float* point, bool& isOutdoors)
	{
		EnsureOutdoorMap();

		if (!stats.hasMap)
		{
			return false;
		}

		isOutdoors = IsPointOutdoors(point);
		return true;
	}

	void SetRainMaskEnabled(bool isEnabled)
	{
		stats.isEnabled = isEnabled;
	}

	OutdoorStats GetOutdoorStats()
	{
		return stats;
	}
}
