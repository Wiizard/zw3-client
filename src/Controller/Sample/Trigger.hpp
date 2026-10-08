#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	enum class TriggerSide : std::uint8_t
	{
		Left,
		Right,
	};

	inline constexpr std::size_t triggerCount = 2;

	struct TriggerSample
	{
		std::uint16_t raw = 0;
		float normalized = 0.0f;
	};
}
