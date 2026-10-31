#include "Utils/MouseStartupDiagnostics.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
	try
	{
		using Diagnostics = Utils::MouseStartupDiagnostics;
		const auto require = [](const bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		};
		Diagnostics recorder;
		recorder.RawMessage(0, 999);
		require(!recorder.Frame(0, Diagnostics::ui), "Capture ran before initialization");
		recorder.Start(5000);
		recorder.Frame(5000, Diagnostics::focused | Diagnostics::rawInput);
		recorder.Frame(5016, Diagnostics::focused | Diagnostics::ui | Diagnostics::rawInput);
		for (unsigned int i = 0; i < 8000; ++i) recorder.RawMessage(5020, i % 300);
		recorder.Registration(5030, true);
		recorder.Registration(5031, false);
		recorder.Warp(5032);
		recorder.Frame(7020, Diagnostics::focused | Diagnostics::ui);
		recorder.RawMessage(7021, 2000);
		require(!recorder.IsFinished(64999) && recorder.IsFinished(65000), "Capture deadline was wrong");
		require(!recorder.Frame(65000, Diagnostics::ui), "Capture exceeded its fixed memory window");
		recorder.RawMessage(65001, 90000);
		recorder.Warp(4999);
		const auto samples = recorder.Stop();
		require(samples[0].frames == 2 && samples[0].maximumFrameGapMs == 16, "Normal input-frame timing was lost");
		require(samples[0].rawMessages == 8000 && samples[0].maximumMessageAgeMs == 299, "High polling rate or message ages were lost");
		require(samples[0].registrations == 2 && samples[0].registrationFailures == 1 && samples[0].warps == 1,
			"Input registration changes or cursor warps were lost");
		require(samples[0].states == (Diagnostics::focused | Diagnostics::ui | Diagnostics::rawInput), "Input state transitions were lost");
		require(samples[2].frames == 1 && samples[2].maximumFrameGapMs == 2004 && samples[2].maximumMessageAgeMs == 2000,
			"A startup stall and delayed mouse messages were not distinguished from normal input");
		require(!recorder.IsActive(), "Capture did not stop");
		recorder.Start(100000);
		recorder.Frame(100010, Diagnostics::ui);
		require(recorder.Stop()[0].maximumFrameGapMs == 0, "Restarted capture retained an old frame or counters");
		std::cout << "Mouse startup diagnostics: bounded capture, high polling rates, message backlog, frame stalls, state changes and reset passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
