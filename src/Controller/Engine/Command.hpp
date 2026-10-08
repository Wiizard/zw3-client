#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"

namespace Controller
{
	class Runtime;
}

namespace Controller::Engine
{
	void RegisterCommands(const Context& context, Runtime& runtime);
}
