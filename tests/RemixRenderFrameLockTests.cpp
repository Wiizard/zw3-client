#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/RenderFrameLock.hpp"

#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

int main()
{
	try
	{
		Components::Remix::Device::RenderFrameLock gate;
		std::promise<bool> timedAttempt;
		std::unique_lock outer(gate);
		std::unique_lock inner(gate);
		auto buffer = std::async(std::launch::async, [&]
		{
			const bool acquired = gate.TryLockFor(10ms);
			if (acquired) gate.unlock();
			timedAttempt.set_value(acquired);
			const std::lock_guard lock(gate);
			return gate.OwnsCurrentThread();
		});
		const bool timedOut = !timedAttempt.get_future().get();
		inner.unlock();
		const bool outerStillOwned = gate.OwnsCurrentThread();
		// Exceed the old 50 ms escape while the outer frame is still active.
		const bool bufferStillBlocked = buffer.wait_for(100ms) == std::future_status::timeout;
		outer.unlock();
		const bool bufferOwned = buffer.get();
		Require(timedOut, "A foreign query acquired the active render frame");
		Require(outerStillOwned && bufferStillBlocked,
			"Nested frame release or the old buffer timeout allowed concurrent device submission");
		Require(bufferOwned && !gate.OwnsCurrentThread(), "Buffer ownership was not transferred and released");

		unsigned sharedWrites = 0;
		std::vector<std::future<void>> workers;
		for (unsigned i = 0; i < 6; ++i)
			workers.push_back(std::async(std::launch::async, [&]
			{
				for (unsigned n = 0; n < 3000; ++n)
				{
					const std::lock_guard first(gate);
					const std::lock_guard nested(gate);
					++sharedWrites;
				}
			}));
		for (auto& worker : workers) worker.get();
		Require(sharedWrites == 18000, "Concurrent frames or buffer calls overlapped");
		Require(gate.TryLockFor(10ms), "Frame remained locked after contention");
		gate.unlock();
		std::cout << "Render frame lock: nested frames, waits beyond 50 ms, query timeout and contention passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
