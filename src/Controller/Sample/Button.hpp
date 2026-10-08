#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	enum class Button : std::uint8_t
	{
		FaceSouth,
		FaceEast,
		FaceWest,
		FaceNorth,

		DpadUp,
		DpadDown,
		DpadLeft,
		DpadRight,

		L1,
		R1,
		L2,
		R2,
		L3,
		R3,

		Start,
		Back,
		Guide,

		Touchpad,
		Mute,

		EdgePaddleLeft,
		EdgePaddleRight,
		EdgeFnLeft,
		EdgeFnRight,

		Count,
	};

	static_assert(static_cast<std::size_t>(Button::Count) <= 32, "ButtonSet stores buttons in a 32-bit mask");

	class ButtonSet
	{
	public:
		constexpr ButtonSet() = default;

		constexpr bool IsDown(Button button) const noexcept
		{
			return (this->bits & Mask(button)) != 0;
		}

		constexpr void Set(Button button, bool isDown) noexcept
		{
			if (isDown)
			{
				this->bits |= Mask(button);
			}
			else
			{
				this->bits &= ~Mask(button);
			}
		}

		friend constexpr bool operator==(ButtonSet, ButtonSet) noexcept = default;

	private:
		static constexpr std::uint32_t Mask(Button button) noexcept
		{
			return std::uint32_t{ 1 } << static_cast<std::uint32_t>(button);
		}

		std::uint32_t bits = 0;
	};
}
