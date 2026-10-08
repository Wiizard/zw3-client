#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	struct TouchPoint
	{
		bool isActive = false;
		std::uint8_t id = 0;
		std::uint16_t x = 0;
		std::uint16_t y = 0;
	};

	struct Touchpad
	{
		static constexpr std::size_t maxPoints = 2;

		std::array<TouchPoint, maxPoints> points{};
	};
}
