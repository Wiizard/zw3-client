#pragma once

#include "Controller/Types.hpp"

#include "Controller/Device/Identity.hpp"
#include "Controller/Sample/Axis.hpp"
#include "Controller/Sample/Motion.hpp"
#include "Controller/Sample/Trigger.hpp"

namespace Controller::Calibration
{
	enum class ValueSource : std::uint8_t
	{
		BuiltIn,
		Measured,
		User,
	};

	struct StickCalibration
	{
		float centerX = 0.0f;
		float centerY = 0.0f;
		float rangeX = 1.0f;
		float rangeY = 1.0f;

		float driftThreshold = 0.0f;
	};

	struct TriggerCalibration
	{
		float min = 0.0f;
		float max = 1.0f;
	};

	struct MotionCalibration
	{
		SensorVector gyroBias{};
		SensorVector accelBias{};
		float gyroScale = 1.0f;
		float accelScale = 1.0f;
	};

	struct Profile
	{
		static constexpr std::uint16_t currentVersion = 1;

		std::uint16_t version = currentVersion;
		Controller::Family family = Family::Unknown;
		std::optional<std::uint64_t> deviceKey;
		ValueSource source = ValueSource::BuiltIn;

		std::array<StickCalibration, stickCount> sticks{};
		std::array<TriggerCalibration, triggerCount> triggers{};
		MotionCalibration motion{};

		float smoothing = 0.0f;
	};

	Profile DefaultProfile(Controller::Family family) noexcept;
}
