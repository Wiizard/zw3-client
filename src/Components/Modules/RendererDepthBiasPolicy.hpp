#pragma once

#include <cstdint>
#include <string_view>

namespace Components::RendererDepthBiasPolicy
{
	inline bool IsConvertedWorldTechnique(const char* name)
	{
		return name && std::string_view(name).starts_with("wc_t7_");
	}

	inline bool UsesPreciseDecalBias(bool convertedWorld, std::uint32_t stateBits)
	{
		const auto offset = (stateBits >> 4) & 3u;
		return convertedWorld && (offset == 1 || offset == 2);
	}

	inline float DecalBias(std::uint32_t stateBits)
	{
		// Native decals use 16-bit depth units, doubled without slope-bias support.
		// At the church this pulls wall layers across glass several units in front.
		// Two D24 units separate coplanar layers without hiding nearby geometry.
		return -2.0f * static_cast<float>((stateBits >> 4) & 3u) / 16777216.0f;
	}
}
