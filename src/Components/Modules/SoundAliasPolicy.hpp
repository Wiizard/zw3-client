#pragma once

#include <cstdint>
#include <string_view>

namespace Components::SoundPolicy
{
	inline std::uint32_t MapAmbienceFlags(const std::string_view name, const std::uint32_t flags)
	{
		constexpr std::uint32_t channelMask = 0x3Fu << 9;
		constexpr std::uint32_t effectsChannel = 19;
		constexpr std::uint32_t distanceAmbienceChannel = 1;
		const auto storage = (flags >> 7) & 3;
		if (!name.starts_with("sac_map_") || (!name.ends_with("_loop") && !name.ends_with("_point"))
			|| (flags & (1u << 6)) == 0 || (storage != 1 && storage != 2)
			|| ((flags & channelMask) >> 9) != effectsChannel)
		{
			return flags;
		}

		// Imported ambience must not outrank round cues or compete with weapons as effects1.
		// ambdist1 has lower priority and a bounded voice count in the native sound mixer.
		return (flags & ~channelMask) | (distanceAmbienceChannel << 9);
	}
}
