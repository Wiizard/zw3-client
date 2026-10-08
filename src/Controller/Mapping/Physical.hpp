#pragma once

#include "Controller/Types.hpp"

#include "Controller/Mapping/Key.hpp"
#include "Controller/Sample/Axis.hpp"

namespace Controller::Mapping
{
	enum class StickDirection : std::uint8_t
	{
		Up,
		Down,
		Left,
		Right,
	};

	struct ApadInput
	{
		Stick which = Stick::Left;
		StickDirection direction = StickDirection::Up;
	};

	EngineKey ToEngineKey(const ApadInput& input) noexcept;

	struct AxisThreshold
	{
		float pressed;
		float hysteresis;
	};

	bool IsAxisDeflected(float value, bool isPositive, bool wasDown, const AxisThreshold& threshold) noexcept;
}
