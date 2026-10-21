#include "STDInclude.hpp"

#include "Camo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Core.hpp"
#include "Device.hpp"
#include "Dxt.hpp"

namespace Components::Remix
{
	static bool TryDecodeTopLevel(IDirect3DTexture9* texture, DecodedImage& image)
	{
		D3DSURFACE_DESC description{};

		if (FAILED(texture->GetLevelDesc(0, &description)))
		{
			return false;
		}

		const D3DFORMAT format = description.Format;

		if (!IsDecodableFormat(format))
		{
			return false;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		const bool didDecode = TryDecodeImage(static_cast<const unsigned char*>(locked.pBits),
			static_cast<UINT>(locked.Pitch), format, description.Width, description.Height, image);

		texture->UnlockRect(0);

		return didDecode;
	}

	static void SampleWrapped(const DecodedImage& image, float u, float v, float* rgb)
	{
		const float x = u * static_cast<float>(image.width) - 0.5f;
		const float y = v * static_cast<float>(image.height) - 0.5f;
		const float floorX = std::floor(x);
		const float floorY = std::floor(y);
		const float blendX = x - floorX;
		const float blendY = y - floorY;

		const int width = static_cast<int>(image.width);
		const int height = static_cast<int>(image.height);
		const int left = ((static_cast<int>(floorX) % width) + width) % width;
		const int top = ((static_cast<int>(floorY) % height) + height) % height;
		const int right = (left + 1) % width;
		const int bottom = (top + 1) % height;

		const std::uint32_t topLeft = image.texels[top * width + left];
		const std::uint32_t topRight = image.texels[top * width + right];
		const std::uint32_t bottomLeft = image.texels[bottom * width + left];
		const std::uint32_t bottomRight = image.texels[bottom * width + right];

		for (int channel = 0; channel < 3; ++channel)
		{
			const int shift = 16 - 8 * channel;
			const float upperLeft = static_cast<float>(Channel(topLeft, shift));
			const float lowerLeft = static_cast<float>(Channel(bottomLeft, shift));
			const float upper = upperLeft + (static_cast<float>(Channel(topRight, shift)) - upperLeft) * blendX;
			const float lower = lowerLeft + (static_cast<float>(Channel(bottomRight, shift)) - lowerLeft) * blendX;

			rgb[channel] = upper + (lower - upper) * blendY;
		}
	}

	static int ClampChannel(float value)
	{
		return static_cast<int>(std::clamp(value + 0.5f, 0.0f, 255.0f));
	}

	static void Composite(const DecodedImage& colour, const DecodedImage& camo, CamoScale scale,
		std::vector<std::uint32_t>& output)
	{
		output.resize(colour.texels.size());

		for (UINT y = 0; y < colour.height; ++y)
		{
			const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(colour.height) * scale.v;

			for (UINT x = 0; x < colour.width; ++x)
			{
				const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(colour.width) * scale.u;
				const std::uint32_t texel = colour.texels[y * colour.width + x];
				const float mask = static_cast<float>(Channel(texel, 24)) / 255.0f;

				if (mask <= 0.0f)
				{
					output[y * colour.width + x] = texel | 0xFF000000u;
					continue;
				}

				float detail[3];
				SampleWrapped(camo, u, v, detail);

				const int red = ClampChannel(Channel(texel, 16) + mask * (detail[0] - 127.5f));
				const int green = ClampChannel(Channel(texel, 8) + mask * (detail[1] - 127.5f));
				const int blue = ClampChannel(Channel(texel, 0) + mask * (detail[2] - 127.5f));

				output[y * colour.width + x] = PackColour(red, green, blue, 255);
			}
		}
	}

	static void HalveLevel(const std::vector<std::uint32_t>& source, UINT width, UINT height,
		std::vector<std::uint32_t>& destination)
	{
		const UINT halfWidth = std::max(1u, width / 2);
		const UINT halfHeight = std::max(1u, height / 2);

		destination.resize(static_cast<std::size_t>(halfWidth) * halfHeight);

		for (UINT y = 0; y < halfHeight; ++y)
		{
			const UINT top = std::min(y * 2, height - 1);
			const UINT bottom = std::min(y * 2 + 1, height - 1);

			for (UINT x = 0; x < halfWidth; ++x)
			{
				const UINT left = std::min(x * 2, width - 1);
				const UINT right = std::min(x * 2 + 1, width - 1);

				const std::uint32_t corners[4] = {
					source[top * width + left],
					source[top * width + right],
					source[bottom * width + left],
					source[bottom * width + right],
				};

				int sums[4] = {};

				for (const std::uint32_t corner : corners)
				{
					sums[0] += Channel(corner, 24);
					sums[1] += Channel(corner, 16);
					sums[2] += Channel(corner, 8);
					sums[3] += Channel(corner, 0);
				}

				destination[y * halfWidth + x] = PackColour((sums[1] + 2) / 4, (sums[2] + 2) / 4, (sums[3] + 2) / 4,
					(sums[0] + 2) / 4);
			}
		}
	}

	static bool TryWriteLevel(IDirect3DTexture9* texture, UINT level, const std::vector<std::uint32_t>& texels,
		UINT width, UINT height)
	{
		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(level, &locked, nullptr, 0)))
		{
			return false;
		}

		auto* bits = static_cast<unsigned char*>(locked.pBits);

		for (UINT y = 0; y < height; ++y)
		{
			std::memcpy(bits + y * static_cast<UINT>(locked.Pitch), &texels[static_cast<std::size_t>(y) * width],
				width * sizeof(std::uint32_t));
		}

		texture->UnlockRect(level);

		return true;
	}

	static int EncodeDisplayed(float linear)
	{
		return ClampChannel(std::pow(linear, 1.0f / 2.2f) * 255.0f);
	}

	IDirect3DTexture9* TryBuildReticleTexture(IDirect3DDevice9* device, IDirect3DTexture9* colorMap,
		IDirect3DTexture9* detailMap, std::uint64_t& hash)
	{
		DecodedImage colour;
		DecodedImage detail;

		if (!TryDecodeTopLevel(colorMap, colour) || !TryDecodeTopLevel(detailMap, detail) || detail.texels.empty())
		{
			return nullptr;
		}

		double detailSum = 0.0;
		int detailPeak = 0;

		for (const std::uint32_t texel : detail.texels)
		{
			detailSum += Channel(texel, 16);
			detailPeak = std::max(detailPeak, Channel(texel, 16));
		}

		const float detailMean = static_cast<float>(detailSum / (255.0 * static_cast<double>(detail.texels.size())));
		const float detailBrightest = static_cast<float>(detailPeak) / 255.0f;
		std::vector<std::uint32_t> texels(colour.texels.size());
		std::size_t opaqueTexels = 0;

		for (std::size_t i = 0; i < texels.size(); ++i)
		{
			const float mask = static_cast<float>(Channel(colour.texels[i], 16)) / 255.0f * detailBrightest;
			const float red = std::clamp(2.0f * mask, 0.0f, 1.0f);
			const float grey = std::clamp(1.2f * mask - 0.24f, 0.0f, 1.0f);
			const UINT column = static_cast<UINT>(i % colour.width);
			const UINT row = static_cast<UINT>(i / colour.width);
			const bool isBorder = column == 0 || row == 0 || column + 1 == colour.width || row + 1 == colour.height;
			int alpha = 0;

			if (red >= 0.25f && !isBorder)
			{
				alpha = 255;
				++opaqueTexels;
			}

			texels[i] = PackColour(EncodeDisplayed(red), EncodeDisplayed(grey), EncodeDisplayed(grey), alpha);
		}

		Log() << "remix: reticle bake " << colour.width << "x" << colour.height << ", detail mean " << detailMean
			  << " brightest " << detailBrightest << ", " << opaqueTexels << " of " << texels.size() << " texels visible";

		D3DSURFACE_DESC description{};
		description.Format = D3DFMT_A8R8G8B8;
		description.Width = colour.width;
		description.Height = colour.height;

		if (!Device::TryHashTopLevel(texels.data(), description, hash))
		{
			return nullptr;
		}

		IDirect3DTexture9* texture = nullptr;

		if (FAILED(device->CreateTexture(colour.width, colour.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture,
				nullptr)))
		{
			return nullptr;
		}

		if (!TryWriteLevel(texture, 0, texels, colour.width, colour.height))
		{
			texture->Release();
			return nullptr;
		}

		return texture;
	}

	IDirect3DTexture9* TryBuildSolidTexture(IDirect3DDevice9* device, std::uint32_t colour, std::uint64_t& hash)
	{
		static constexpr UINT size = 8;
		const std::vector<std::uint32_t> texels(static_cast<std::size_t>(size) * size, colour);

		D3DSURFACE_DESC description{};
		description.Format = D3DFMT_A8R8G8B8;
		description.Width = size;
		description.Height = size;

		if (!Device::TryHashTopLevel(texels.data(), description, hash))
		{
			return nullptr;
		}

		IDirect3DTexture9* texture = nullptr;

		if (FAILED(device->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
		{
			return nullptr;
		}

		if (!TryWriteLevel(texture, 0, texels, size, size))
		{
			texture->Release();
			return nullptr;
		}

		return texture;
	}

	IDirect3DTexture9* TryBuildCamoTexture(IDirect3DDevice9* device, IDirect3DTexture9* colorMap,
		IDirect3DTexture9* camoMap, CamoScale scale, std::uint64_t& hash)
	{
		DecodedImage colour;
		DecodedImage camo;

		if (!TryDecodeTopLevel(colorMap, colour) || !TryDecodeTopLevel(camoMap, camo))
		{
			return nullptr;
		}

		const UINT levels = colorMap->GetLevelCount();
		IDirect3DTexture9* texture = nullptr;

		if (FAILED(device->CreateTexture(colour.width, colour.height, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
				&texture, nullptr)))
		{
			return nullptr;
		}

		std::vector<std::uint32_t> level;
		std::vector<std::uint32_t> nextLevel;
		Composite(colour, camo, scale, level);

		D3DSURFACE_DESC description{};
		description.Format = D3DFMT_A8R8G8B8;
		description.Width = colour.width;
		description.Height = colour.height;

		if (!Device::TryHashTopLevel(level.data(), description, hash))
		{
			hash = 0;
		}

		UINT width = colour.width;
		UINT height = colour.height;

		for (UINT index = 0; index < levels; ++index)
		{
			if (!TryWriteLevel(texture, index, level, width, height))
			{
				texture->Release();
				return nullptr;
			}

			HalveLevel(level, width, height, nextLevel);
			level.swap(nextLevel);
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}

		return texture;
	}
}
