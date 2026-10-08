#pragma once

#include "Controller/Types.hpp"

#include "Controller/Calibration/Profile.hpp"

namespace Controller::Calibration
{
	bool IsValid(const Profile& profile, std::string& why);
}
