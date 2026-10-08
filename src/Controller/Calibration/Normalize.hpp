#pragma once

#include "Controller/Types.hpp"

#include "Controller/Calibration/Profile.hpp"
#include "Controller/Sample/Sample.hpp"

namespace Controller::Calibration
{
	void ApplyProfile(const Profile& profile, const RawSample& raw, CanonicalSample& canonical) noexcept;
}
