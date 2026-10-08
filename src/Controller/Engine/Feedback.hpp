#pragma once

#include "Controller/Types.hpp"

#include "Controller/Driver/Output.hpp"
#include "Controller/Engine/Dvar.hpp"

namespace Controller::Engine
{
	bool TryEvaluateTriggerFeedback(const Dvars& dvars, int client, Driver::AdaptiveTriggerRequest& left, Driver::AdaptiveTriggerRequest& right);
}
