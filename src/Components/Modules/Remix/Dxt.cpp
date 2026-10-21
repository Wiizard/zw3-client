#include "STDInclude.hpp"

#include "Dxt.hpp"

#include <algorithm>
#include <cstring>

namespace Components::Remix
{
	std::uint32_t PackColour(int red, int green, int blue, int alpha)
	{
		return (static_cast<std::uint32_t>(alpha) << 24) | (static_cast<std::uint32_t>(red) << 16)
			| (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(blue);
	}

	static void ExpandColour565(std::uint16_t packed, int* rgb)
	{
		const int red = (packed >> 11) & 31;
		const int green = (packed >> 5) & 63;
		const int blue = packed & 31;

		rgb[0] = (red << 3) | (red >> 2);
		rgb[1] = (green << 2) | (green >> 4);
		rgb[2] = (blue << 3) | (blue >> 2);
	}

	static void DecodeColourPalette(const unsigned char* block, bool isFourColourOnly, std::uint32_t* palette)
	{
		const std::uint16_t first = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
		const std::uint16_t second = static_cast<std::uint16_t>(block[2] | (block[3] << 8));

		int a[3];
		int b[3];
		ExpandColour565(first, a);
		ExpandColour565(second, b);

		palette[0] = PackColour(a[0], a[1], a[2], 255);
		palette[1] = PackColour(b[0], b[1], b[2], 255);

		if (isFourColourOnly || first > second)
		{
			palette[2] = PackColour((2 * a[0] + b[0]) / 3, (2 * a[1] + b[1]) / 3, (2 * a[2] + b[2]) / 3, 255);
			palette[3] = PackColour((a[0] + 2 * b[0]) / 3, (a[1] + 2 * b[1]) / 3, (a[2] + 2 * b[2]) / 3, 255);
		}
		else
		{
			palette[2] = PackColour((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2, 255);
			palette[3] = 0;
		}
	}

	static void DecodeAlphaPalette(const unsigned char* block, int* palette)
	{
		const int first = block[0];
		const int second = block[1];

		palette[0] = first;
		palette[1] = second;

		if (first > second)
		{
			for (int i = 1; i < 7; ++i)
			{
				palette[1 + i] = ((7 - i) * first + i * second) / 7;
			}
		}
		else
		{
			for (int i = 1; i < 5; ++i)
			{
				palette[1 + i] = ((5 - i) * first + i * second) / 5;
			}

			palette[6] = 0;
			palette[7] = 255;
		}
	}

	static std::uint64_t ReadLittleEndian(const unsigned char* bytes, int count)
	{
		std::uint64_t value = 0;

		for (int i = count - 1; i >= 0; --i)
		{
			value = (value << 8) | bytes[i];
		}

		return value;
	}

	static void DecodeBlock(const unsigned char* block, D3DFORMAT format, std::uint32_t* texels)
	{
		const unsigned char* colourBlock = block;

		if (format != D3DFMT_DXT1)
		{
			colourBlock = block + 8;
		}

		std::uint32_t palette[4];
		DecodeColourPalette(colourBlock, format != D3DFMT_DXT1, palette);

		const std::uint32_t indices = static_cast<std::uint32_t>(ReadLittleEndian(colourBlock + 4, 4));

		for (int i = 0; i < 16; ++i)
		{
			texels[i] = palette[(indices >> (2 * i)) & 3];
		}

		if (format == D3DFMT_DXT3)
		{
			const std::uint64_t alphaBits = ReadLittleEndian(block, 8);

			for (int i = 0; i < 16; ++i)
			{
				const auto alpha = static_cast<std::uint32_t>((alphaBits >> (4 * i)) & 0xF) * 17u;
				texels[i] = (texels[i] & 0x00FFFFFFu) | (alpha << 24);
			}
		}

		if (format == D3DFMT_DXT5)
		{
			int alphaPalette[8];
			DecodeAlphaPalette(block, alphaPalette);

			const std::uint64_t alphaIndices = ReadLittleEndian(block + 2, 6);

			for (int i = 0; i < 16; ++i)
			{
				const auto alpha = static_cast<std::uint32_t>(alphaPalette[(alphaIndices >> (3 * i)) & 7]);
				texels[i] = (texels[i] & 0x00FFFFFFu) | (alpha << 24);
			}
		}
	}

	static void DecodeBlocks(const unsigned char* bits, UINT pitch, D3DFORMAT format, DecodedImage& image)
	{
		const UINT blocksWide = std::max(1u, (image.width + 3) / 4);
		const UINT blocksHigh = std::max(1u, (image.height + 3) / 4);
		UINT blockBytes = 16;

		if (format == D3DFMT_DXT1)
		{
			blockBytes = 8;
		}

		std::uint32_t texels[16];

		for (UINT blockY = 0; blockY < blocksHigh; ++blockY)
		{
			const unsigned char* row = bits + blockY * pitch;

			for (UINT blockX = 0; blockX < blocksWide; ++blockX)
			{
				DecodeBlock(row + blockX * blockBytes, format, texels);

				for (UINT y = 0; y < 4; ++y)
				{
					const UINT imageY = blockY * 4 + y;

					if (imageY >= image.height)
					{
						break;
					}

					for (UINT x = 0; x < 4; ++x)
					{
						const UINT imageX = blockX * 4 + x;

						if (imageX < image.width)
						{
							image.texels[imageY * image.width + imageX] = texels[y * 4 + x];
						}
					}
				}
			}
		}
	}

	int Channel(std::uint32_t texel, int shift)
	{
		return static_cast<int>((texel >> shift) & 0xFFu);
	}

	bool IsDecodableFormat(D3DFORMAT format)
	{
		return format == D3DFMT_DXT1 || format == D3DFMT_DXT3 || format == D3DFMT_DXT5 || format == D3DFMT_A8R8G8B8
			|| format == D3DFMT_X8R8G8B8;
	}

	bool TryDecodeImage(const unsigned char* bits, UINT pitch, D3DFORMAT format, UINT width, UINT height,
		DecodedImage& image)
	{
		if (!bits || width == 0 || height == 0 || !IsDecodableFormat(format))
		{
			return false;
		}

		image.width = width;
		image.height = height;
		image.texels.assign(static_cast<std::size_t>(width) * height, 0);

		if (format == D3DFMT_DXT1 || format == D3DFMT_DXT3 || format == D3DFMT_DXT5)
		{
			DecodeBlocks(bits, pitch, format, image);
			return true;
		}

		for (UINT y = 0; y < height; ++y)
		{
			std::memcpy(&image.texels[static_cast<std::size_t>(y) * width], bits + y * pitch, width * sizeof(std::uint32_t));
		}

		if (format == D3DFMT_X8R8G8B8)
		{
			for (std::uint32_t& texel : image.texels)
			{
				texel |= 0xFF000000u;
			}
		}

		return true;
	}
}
