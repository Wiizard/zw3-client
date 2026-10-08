#pragma once

#include "Controller/Types.hpp"

#include "Controller/Driver/DualSense.hpp"

namespace Controller::Driver
{
	class DualSenseEdgeDriver : public DualSenseDriver
	{
	public:
		using DualSenseDriver::DualSenseDriver;

		Controller::Family Family() const noexcept override
		{
			return Controller::Family::DualSenseEdge;
		}

		bool TryPoll(RawSample& raw, CanonicalSample& canonical) override;
	};
}
