#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Elevators : public Component
	{
	public:
		Elevators();

	private:
		enum ElevatorSettings { DISABLED, ENABLED, EASY };
	};
}
