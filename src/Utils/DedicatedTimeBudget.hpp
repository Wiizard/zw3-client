#pragma once

#include <cstdint>
#include <limits>

namespace Utils::DedicatedTimeBudget
{
	// The native worker can advance on its timeout while the main thread is
	// busy. Adopt that progress without creating another budget for the same
	// elapsed time. target + residual must remain unchanged. A negative
	// residual records time already consumed by the worker; future main-frame
	// elapsed time repays it before scheduling another simulation tick.
	inline void Reconcile(int worker, int& target, int& residual)
	{
		if (worker <= target) return;
		const auto remainder = static_cast<std::int64_t>(residual) + target - worker;
		if (remainder < std::numeric_limits<int>::min()) return;
		target = worker;
		residual = static_cast<int>(remainder);
	}
}
