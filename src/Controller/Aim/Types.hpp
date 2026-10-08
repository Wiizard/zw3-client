#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"

namespace Controller::Aim
{
	inline constexpr float pi = 3.14159265358979323846f;

	struct Degrees
	{
		float value = 0.0f;

		friend constexpr Degrees operator+(Degrees left, Degrees right) noexcept
		{
			return { left.value + right.value };
		}

		friend constexpr Degrees operator-(Degrees degrees) noexcept
		{
			return { -degrees.value };
		}

		friend constexpr Degrees operator*(Degrees degrees, float scale) noexcept
		{
			return { degrees.value * scale };
		}
	};

	struct DegreesPerSecond
	{
		float value = 0.0f;

		friend constexpr DegreesPerSecond operator*(DegreesPerSecond rate, float scale) noexcept
		{
			return { rate.value * scale };
		}
	};

	struct DegreesPerSecondSquared
	{
		float value = 0.0f;
	};

	struct Magnitude
	{
		float value = 0.0f;
	};

	struct WorldVector
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	inline constexpr float Dot(WorldVector left, WorldVector right) noexcept
	{
		return left.x * right.x + left.y * right.y + left.z * right.z;
	}
}
