#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	struct Clock
	{
		using Base = std::chrono::steady_clock;
		using TimePoint = Base::time_point;
		using Duration = Base::duration;

		static_assert(Base::is_steady, "the subsystem clock must be monotonic");

		static TimePoint Now() noexcept
		{
			return Base::now();
		}
	};

	using Timestamp = Clock::TimePoint;

	using Seconds = std::chrono::duration<float>;

	struct LatencySpan
	{
		Timestamp acquired{};
		Timestamp consumed{};
	};
}
