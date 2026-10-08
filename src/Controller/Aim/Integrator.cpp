#include "STDInclude.hpp"

#include "Controller/Aim/Integrator.hpp"

namespace Controller::Aim
{
	Degrees TurnIntegrator::Advance(DegreesPerSecond target, const Limits& limits, Seconds deltaTime) noexcept
	{
		const float seconds = deltaTime.count();

		if (seconds <= 0.0f)
		{
			return Degrees{ 0.0f };
		}

		const float targetRate = target.value;
		float rate = this->current.value;

		if (rate < targetRate)
		{
			float step = targetRate - rate;

			if (limits.accel.value > 0.0f)
			{
				step = limits.accel.value * seconds;
			}

			rate = std::min(rate + step, targetRate);
		}
		else if (rate > targetRate)
		{
			float step = rate - targetRate;

			if (limits.decel.value > 0.0f)
			{
				step = limits.decel.value * seconds;
			}

			rate = std::max(rate - step, targetRate);
		}

		this->current = DegreesPerSecond{ rate };

		return Degrees{ rate * seconds };
	}
}
