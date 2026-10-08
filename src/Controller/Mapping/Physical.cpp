#include "STDInclude.hpp"

#include "Controller/Mapping/Physical.hpp"

namespace Controller::Mapping
{
	EngineKey ToEngineKey(const ApadInput& input) noexcept
	{
		if (input.which == Stick::Right)
		{
			switch (input.direction)
			{
			case StickDirection::Up:
				return EngineKey::RStickUp;
			case StickDirection::Down:
				return EngineKey::RStickDown;
			case StickDirection::Left:
				return EngineKey::RStickLeft;
			case StickDirection::Right:
				return EngineKey::RStickRight;
			}

			return EngineKey::RStickUp;
		}

		switch (input.direction)
		{
		case StickDirection::Up:
			return EngineKey::ApadUp;
		case StickDirection::Down:
			return EngineKey::ApadDown;
		case StickDirection::Left:
			return EngineKey::ApadLeft;
		case StickDirection::Right:
			return EngineKey::ApadRight;
		}

		return EngineKey::ApadUp;
	}

	bool IsAxisDeflected(float value, bool isPositive, bool wasDown, const AxisThreshold& threshold) noexcept
	{
		float limit = threshold.pressed + threshold.hysteresis;

		if (wasDown)
		{
			limit = threshold.pressed - threshold.hysteresis;
		}

		if (isPositive)
		{
			return value > limit;
		}

		return value < -limit;
	}
}
