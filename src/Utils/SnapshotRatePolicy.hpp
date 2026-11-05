#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Utils::SnapshotRatePolicy
{
	// Retain only the rounding error of the normal 50 ms simulation tick.
	// Do not bank time across a hitch, reconnect, map restart or pause, and
	// never make a second snapshot eligible in the same simulation frame.
	inline int Deadline(int now, int previous, int nativeNext)
	{
		const auto late = static_cast<std::int64_t>(now) - previous;
		const auto delay = static_cast<std::int64_t>(nativeNext) - now;
		if (previous <= 0 || late < 0 || late >= 50 || delay <= 0
			|| nativeNext == std::numeric_limits<int>::max())
		{
			return nativeNext;
		}
		// Native byte-rate division truncates. Round conservatively before
		// carrying tick credit so repeated sends do not exceed that budget.
		return nativeNext + 1 - static_cast<int>(std::min(late, delay));
	}
}
