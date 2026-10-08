#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	struct SensorVector
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct GyroSample
	{
		SensorVector angularVelocity{};
	};

	struct AccelSample
	{
		SensorVector acceleration{};
	};

	struct MotionSample
	{
		GyroSample gyro{};
		AccelSample accel{};
		std::optional<std::uint32_t> deviceTimestamp;
	};
}
