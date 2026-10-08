#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Device/Identity.hpp"
#include "Controller/Sample/Sample.hpp"

namespace Controller
{
	struct InputFrame
	{
		DeviceId device{};
		Controller::Family family = Family::Unknown;
		Connection link = Connection::Unknown;
		std::uint64_t sequence = 0;
		LatencySpan timing{};
		CanonicalSample state{};
	};
}
