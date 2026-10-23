#include "Utils/MatchmakingSearchTimer.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void Require(const bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using Timer = Utils::MatchmakingSearchTimer;
		const auto bootedLongAgo = Timer::Clock::time_point{} + std::chrono::hours(24 * 100);
		Timer timer;
		Require(timer.Text(bootedLongAgo) == "0:00", "Idle must not display process uptime");
		timer.SetSearching(true, bootedLongAgo);
		Require(timer.Text(bootedLongAgo) == "0:00", "A new search starts at zero regardless of uptime");
		Require(timer.Text(bootedLongAgo + std::chrono::milliseconds(999)) == "0:00", "Seconds must not round up");
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(9)) == "0:09", "Seconds retain their leading zero");
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(59)) == "0:59", "Last second of the minute is correct");
		timer.SetSearching(true, bootedLongAgo + std::chrono::seconds(59));
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(60)) == "1:00", "Polling and SEARCH_STARTING to SEARCHING must not restart the clock");
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(65)) == "1:05", "Minute rollover formats correctly");
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(6001)) == "100:01", "Long searches must not wrap to uptime or a missing digit");
		Require(timer.Text(bootedLongAgo - std::chrono::seconds(5)) == "0:00", "A clock discontinuity must not show negative values");
		// No menu-close operation mutates the stopwatch: reopening a menu for the
		// same ongoing search must show the genuine elapsed time, not zero.
		Require(timer.Text(bootedLongAgo + std::chrono::seconds(90)) == "1:30", "A hidden menu does not reset the active search");
		Require(timer.ElapsedSeconds(bootedLongAgo + std::chrono::seconds(3'000'000'000LL)) == std::numeric_limits<int>::max(),
			"Unrepresentable elapsed seconds must saturate rather than wrap negative");
		for (const auto phase : { "cancel", "error", "ready", "vote", "starting", "joining", "idle", "offline" })
		{
			timer.SetSearching(false, bootedLongAgo + std::chrono::seconds(100));
			Require(timer.Text(bootedLongAgo + std::chrono::seconds(150)) == "0:00", phase);
			timer.SetSearching(true, bootedLongAgo + std::chrono::seconds(160));
			Require(timer.Text(bootedLongAgo + std::chrono::seconds(162)) == "0:02", "A later search must have its own start");
		}
		std::cout << "Matchmaking search timer start/reset/repeated-state/rollover/long-uptime tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
