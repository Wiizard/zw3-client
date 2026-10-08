#pragma once

#include "Controller/Types.hpp"

namespace Controller::Engine
{
	struct MoveDelta
	{
		std::int8_t forward = 0;
		std::int8_t right = 0;
	};

	MoveDelta UnpackMove(std::uint16_t packed, int key) noexcept;
}
