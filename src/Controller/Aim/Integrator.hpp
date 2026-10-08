#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"
#include "Controller/Aim/Types.hpp"

namespace Controller::Aim
{
	class TurnIntegrator
	{
	public:
		struct Limits
		{
			DegreesPerSecondSquared accel{ 0.0f };
			DegreesPerSecondSquared decel{ 0.0f };
		};

		Degrees Advance(DegreesPerSecond target, const Limits& limits, Seconds deltaTime) noexcept;

		void Reset() noexcept
		{
			this->current = DegreesPerSecond{ 0.0f };
		}

	private:
		DegreesPerSecond current{ 0.0f };
	};
}
