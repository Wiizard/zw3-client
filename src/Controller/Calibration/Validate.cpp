#include "STDInclude.hpp"

#include "Controller/Calibration/Validate.hpp"

namespace Controller::Calibration
{
	static bool IsFinite(float value) noexcept
	{
		return std::isfinite(value);
	}

	static bool IsFinite(const SensorVector& vector) noexcept
	{
		return IsFinite(vector.x) && IsFinite(vector.y) && IsFinite(vector.z);
	}

	bool IsValid(const Profile& profile, std::string& why)
	{
		if (profile.version == 0 || profile.version > Profile::currentVersion)
		{
			why = "unsupported calibration profile version";
			return false;
		}

		for (const auto& stick : profile.sticks)
		{
			const bool isFinite = IsFinite(stick.centerX) && IsFinite(stick.centerY) && IsFinite(stick.rangeX) && IsFinite(stick.rangeY) && IsFinite(stick.driftThreshold);

			if (!isFinite)
			{
				why = "stick calibration has a non-finite value";
				return false;
			}

			if (stick.rangeX <= 0.0f || stick.rangeY <= 0.0f)
			{
				why = "stick calibration range must be strictly positive";
				return false;
			}

			if (stick.driftThreshold < 0.0f || stick.driftThreshold >= 1.0f)
			{
				why = "stick drift threshold must be in [0, 1)";
				return false;
			}
		}

		for (const auto& trigger : profile.triggers)
		{
			if (!IsFinite(trigger.min) || !IsFinite(trigger.max))
			{
				why = "trigger calibration has a non-finite value";
				return false;
			}

			if (trigger.max <= trigger.min)
			{
				why = "trigger calibration max must exceed min";
				return false;
			}
		}

		const auto& motion = profile.motion;

		if (!IsFinite(motion.gyroBias) || !IsFinite(motion.accelBias) || !IsFinite(motion.gyroScale) || !IsFinite(motion.accelScale))
		{
			why = "motion calibration has a non-finite value";
			return false;
		}

		if (!IsFinite(profile.smoothing) || profile.smoothing < 0.0f)
		{
			why = "smoothing time constant must be finite and non-negative";
			return false;
		}

		return true;
	}
}
