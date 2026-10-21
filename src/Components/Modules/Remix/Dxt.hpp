#pragma once

#include <cstdint>
#include <vector>

namespace Components::Remix
{
	struct DecodedImage
	{
		UINT width = 0;
		UINT height = 0;
		std::vector<std::uint32_t> texels;
	};

	std::uint32_t PackColour(int red, int green, int blue, int alpha);

	int Channel(std::uint32_t texel, int shift);

	bool IsDecodableFormat(D3DFORMAT format);

	bool TryDecodeImage(const unsigned char* bits, UINT pitch, D3DFORMAT format, UINT width, UINT height,
		DecodedImage& image);
}
