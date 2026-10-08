#include "STDInclude.hpp"

#include "Controller/Calibration/Profile.hpp"

namespace Controller::Calibration
{
	Profile DefaultProfile(Controller::Family family) noexcept
	{
		Profile profile;
		profile.version = Profile::currentVersion;
		profile.family = family;
		profile.source = ValueSource::BuiltIn;

		for (auto& stick : profile.sticks)
		{
			stick = StickCalibration{};
		}

		for (auto& trigger : profile.triggers)
		{
			trigger = TriggerCalibration{};
		}

		profile.motion = MotionCalibration{};
		profile.smoothing = 0.0f;
		return profile;
	}
}
