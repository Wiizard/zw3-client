#pragma once

#include "Controller/Types.hpp"

#include "Controller/Aim/Types.hpp"
#include "Controller/Sample/Axis.hpp"

namespace Controller::Aim
{
	struct DeadzoneParams
	{
		Magnitude inner{ 0.0f };
		Magnitude outer{ 0.0f };
		Magnitude anti{ 0.0f };
	};

	bool IsValid(const DeadzoneParams& params, std::string& why);

	StickVector ApplyDeadzone(const DeadzoneParams& params, StickVector stick) noexcept;
}
