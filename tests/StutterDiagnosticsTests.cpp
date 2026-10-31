#include "Utils/StutterDiagnostics.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
	using namespace Utils::StutterDiagnostics;
	try
	{
		const auto require = [](bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		};
		Recorder log;
		require(log.Token() == 0, "Diagnostics started enabled");
		log.Add(0, {0, 100}, 4);
		require(log.Stop().entries.empty(), "Disabled diagnostics captured data");
		log.Start(1000);
		const auto first = log.Token();
		log.Add(first, {1010, 3}, 4);
		log.Add(first, {1020, 4}, 4);
		const auto capture = log.Stop();
		require(capture.started == 1000 && capture.entries.size() == 1
			&& capture.entries.front().started == 1020, "Slow-operation threshold lost a boundary event");
		log.Add(first, {1030, 100}, 4);
		require(log.Stop().entries.size() == 1, "An operation finishing after stop changed the capture");
		log.Start(2000);
		const auto second = log.Token();
		require(second != first, "Restart reused a capture token");
		log.Add(first, {2010, 100}, 4);
		require(log.Stop().entries.empty(), "An operation from an old capture leaked into a new capture");
		log.Start(3000);
		for (std::size_t i = 0; i < Recorder::capacity + 7; ++i)
			log.Add(log.Token(), {static_cast<std::int64_t>(i), 10}, 4);
		const auto wrapped = log.Stop();
		require(wrapped.entries.size() == Recorder::capacity && wrapped.overwritten == 7
			&& wrapped.entries.front().started == 7
			&& wrapped.entries.back().started == Recorder::capacity + 6, "Bounded history did not retain the latest events");
		log.Start(4000);
		const auto concurrentToken = log.Token();
		std::array<std::thread, 4> writers;
		for (std::size_t writer = 0; writer < writers.size(); ++writer)
		{
			writers[writer] = std::thread([&, writer]
			{
				for (int i = 0; i < 1500; ++i)
					log.Add(concurrentToken, {i, 10, writer, "concurrent"}, 4);
			});
		}
		for (auto& writer : writers) writer.join();
		const auto concurrent = log.Stop();
		require(concurrent.entries.size() + concurrent.overwritten == 6000,
			"Concurrent game, renderer and background writers lost events");
		for (const auto& entry : concurrent.entries)
			require(entry.duration == 10 && entry.thread < writers.size() && entry.category == "concurrent",
				"Concurrent writers corrupted a timing record");

		recorder.Start(Now());
		{
			const Scope span("test_scope", 0);
		}
		const auto measured = recorder.Stop();
		require(measured.entries.size() == 1 && measured.entries.front().category == "test_scope"
			&& measured.entries.front().duration >= 0 && measured.entries.front().source.line() > 0,
			"Scoped measurement lost its duration or source location");
		{
			const Scope span("disabled_scope", 0);
		}
		require(recorder.Stop().entries.size() == 1, "Disabled scope changed the completed capture");
		std::cout << "Stutter diagnostics: disabled capture, thresholds, stop/restart isolation, bounded history and scoped timing passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
