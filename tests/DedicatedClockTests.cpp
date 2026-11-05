#include <cstdint>
#include <iostream>
#include <stdexcept>

struct FrameResult
{
	int milliseconds;
	float seconds;
	float fraction;
};

static int serverTime = 0;
static int targetTime = 0;

extern "C"
{
	void DedicatedClockTest_Resume();
	void DedicatedClockTest_Run(int milliseconds, float seconds, float fraction, FrameResult* result);
	std::uintptr_t Com_Frame_DedicatedMsecResume = reinterpret_cast<std::uintptr_t>(DedicatedClockTest_Resume);
	std::uintptr_t Com_Frame_WaitResume = 0;
	int Threading_ComFrameWait(int milliseconds) { return milliseconds; }
	void DedicatedClockTest_FrameRun();
	void DedicatedClockTest_FrameSkip();
	int DedicatedClockTest_Worker(int frameRequested);
	std::uintptr_t SV_DedicatedFrameRun = reinterpret_cast<std::uintptr_t>(DedicatedClockTest_FrameRun);
	std::uintptr_t SV_DedicatedFrameSkip = reinterpret_cast<std::uintptr_t>(DedicatedClockTest_FrameSkip);
	std::uintptr_t SV_DedicatedTime = reinterpret_cast<std::uintptr_t>(&serverTime);
	std::uintptr_t SV_DedicatedTargetTime = reinterpret_cast<std::uintptr_t>(&targetTime);
}

int main()
{
	try
	{
		const auto sample = [](int milliseconds)
		{
			FrameResult result{};
			const float seconds = static_cast<float>(milliseconds) * 0.001f;
			DedicatedClockTest_Run(milliseconds, seconds, 0.75f, &result);
			if (result.milliseconds != milliseconds || result.seconds != seconds || result.fraction != 0.0f)
				throw std::runtime_error("Dedicated frame hand-off changed elapsed time or lost the native register contract");
			return result.milliseconds;
		};
		for (const int duration : {0, 1, 3, 8, 16, 39, 40, 41, 50, 150, 200, 5000}) sample(duration);

		// Alternating waits and short frames cross the native 40 ms smoothing
		// boundary. A server must neither replay nor discard those milliseconds.
		std::int64_t wall = 0;
		std::int64_t simulation = 0;
		for (int frame = 0; frame < 10000; ++frame)
		{
			const int duration = frame % 97 == 0 ? 150 : (frame % 2 ? 3 : 49);
			wall += duration;
			simulation += sample(duration);
		}
		if (simulation != wall) throw std::runtime_error("Dedicated clock drifts after long waits");
		const auto expectWorker = [](int current, int target, int requested, int expected)
		{
			serverTime = current;
			targetTime = target;
			if (DedicatedClockTest_Worker(requested) != expected)
				throw std::runtime_error("Worker exceeded its time budget, lost a due frame, or corrupted registers");
		};
		expectWorker(0, 0, 1, 1);
		expectWorker(50, 0, 1, 1);
		expectWorker(0, 0, 0, 0);
		expectWorker(1000, 1050, 0, 0);
		expectWorker(1000, 1050, 1, 1);
		expectWorker(1050, 1050, 1, 0);
		expectWorker(1100, 1050, 1, 0);
		serverTime = 1000;
		targetTime = 1000;
		for (int frame = 0; frame < 10000; ++frame)
		{
			targetTime += 50;
			// A timer wakeup and a main-thread notification may target the same tick.
			for (int wake = 0; wake < 2; ++wake)
			{
				const int due = DedicatedClockTest_Worker(1);
				if (due != (wake == 0 ? 1 : 0)) throw std::runtime_error("Duplicate wakeup advanced the simulation");
				serverTime += due * 50;
			}
		}
		if (serverTime != targetTime) throw std::runtime_error("Server worker clock drifted");
		targetTime += 200;
		int catchupFrames = 0;
		while (DedicatedClockTest_Worker(1) == 1)
		{
			serverTime += 50;
			if (++catchupFrames > 4) throw std::runtime_error("Catch-up exceeded its budget");
		}
		if (catchupFrames != 4) throw std::runtime_error("Lost legitimate catch-up frames");
		std::cout << "Dedicated timing ABI and clock conservation passed (" << wall << " ms).\n";
		std::cout << "Worker wakeup races and catch-up budget passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
