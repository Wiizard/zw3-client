#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Discovery : public Component
	{
	public:
		Discovery();

		static void Perform();

	private:
		static std::atomic_bool isPerforming;
		static std::jthread thread;
		static std::string challenge;

		static Dvar::Var net_discoveryPortRangeMin;
		static Dvar::Var net_discoveryPortRangeMax;
	};
}
