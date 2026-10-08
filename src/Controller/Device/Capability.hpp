#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	enum class Capability : std::uint32_t
	{
		None = 0,

		Gyroscope = 1u << 0,
		Accelerometer = 1u << 1,
		Touchpad = 1u << 2,
		Battery = 1u << 3,
		MicrophoneButton = 1u << 4,
		BackButtons = 1u << 5,

		Rumble = 1u << 8,
		Haptics = 1u << 9,
		AdaptiveTriggers = 1u << 10,
		LightBar = 1u << 11,
		PlayerLeds = 1u << 12,
	};

	class Capabilities
	{
	public:
		using Bits = std::underlying_type_t<Capability>;

		constexpr Capabilities() = default;

		constexpr Capabilities(Capability capability) noexcept : bits(static_cast<Bits>(capability))
		{
		}

		constexpr bool Has(Capability capability) const noexcept
		{
			const auto mask = static_cast<Bits>(capability);
			return capability != Capability::None && (this->bits & mask) == mask;
		}

		constexpr Capabilities& Add(Capability capability) noexcept
		{
			this->bits |= static_cast<Bits>(capability);
			return *this;
		}

		friend constexpr bool operator==(Capabilities, Capabilities) noexcept = default;

		friend constexpr Capabilities operator|(Capabilities left, Capabilities right) noexcept
		{
			Capabilities combined;
			combined.bits = left.bits | right.bits;
			return combined;
		}

	private:
		Bits bits = 0;
	};

	constexpr Capabilities operator|(Capability left, Capability right) noexcept
	{
		return Capabilities(left) | Capabilities(right);
	}
}
