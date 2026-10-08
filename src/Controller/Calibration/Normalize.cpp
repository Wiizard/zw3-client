#include "STDInclude.hpp"

#include "Controller/Calibration/Normalize.hpp"

namespace Controller::Calibration
{
	void ApplyProfile(const Profile& profile, const RawSample& raw, CanonicalSample& canonical) noexcept
	{
		for (std::size_t i = 0; i < stickCount; ++i)
		{
			const auto& stickCalibration = profile.sticks[i];
			auto& stick = canonical.sticks[i];

			float x = std::clamp((stick.normalized.x - stickCalibration.centerX) / stickCalibration.rangeX, -1.0f, 1.0f);
			float y = std::clamp((stick.normalized.y - stickCalibration.centerY) / stickCalibration.rangeY, -1.0f, 1.0f);

			float magnitude = std::sqrt(x * x + y * y);

			if (magnitude > 1.0f)
			{
				x /= magnitude;
				y /= magnitude;
				magnitude = 1.0f;
			}

			if (magnitude <= stickCalibration.driftThreshold)
			{
				x = 0.0f;
				y = 0.0f;
			}

			stick.calibrated = { x, y };
		}

		for (std::size_t i = 0; i < triggerCount; ++i)
		{
			const auto& triggerCalibration = profile.triggers[i];
			auto& trigger = canonical.triggers[i];

			const float range = triggerCalibration.max - triggerCalibration.min;
			float value = trigger.normalized;

			if (range > 0.0f)
			{
				value = (trigger.normalized - triggerCalibration.min) / range;
			}

			trigger.normalized = std::clamp(value, 0.0f, 1.0f);
		}

		if (!raw.motion)
		{
			return;
		}

		const auto& motionCalibration = profile.motion;
		MotionSample motion = *raw.motion;

		const SensorVector gyro = motion.gyro.angularVelocity;
		motion.gyro.angularVelocity =
		{
			(gyro.x - motionCalibration.gyroBias.x) * motionCalibration.gyroScale,
			(gyro.y - motionCalibration.gyroBias.y) * motionCalibration.gyroScale,
			(gyro.z - motionCalibration.gyroBias.z) * motionCalibration.gyroScale,
		};

		const SensorVector accel = motion.accel.acceleration;
		motion.accel.acceleration =
		{
			(accel.x - motionCalibration.accelBias.x) * motionCalibration.accelScale,
			(accel.y - motionCalibration.accelBias.y) * motionCalibration.accelScale,
			(accel.z - motionCalibration.accelBias.z) * motionCalibration.accelScale,
		};

		canonical.motion = motion;
	}
}
