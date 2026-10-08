#include "STDInclude.hpp"

#include "Controller/Driver/DualSenseEdge.hpp"

namespace Controller::Driver
{
	bool DualSenseEdgeDriver::TryPoll(RawSample& raw, CanonicalSample& canonical)
	{
		this->FlushRumble();

		return this->TryReadAndDecode(raw, canonical, true);
	}
}
