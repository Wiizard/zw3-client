#include "STDInclude.hpp"

#include "Profiler.hpp"

#include <algorithm>
#include <array>

namespace Components::Remix
{
	static constexpr int historyFrames = 120;
	static constexpr float spikeRatio = 4.0f;
	static constexpr float spikeFloorMs = 0.5f;

	static constexpr const char* sectionNames[profileSectionCount] = {
		"map settings",
		"triggers",
		"culling",
		"Remix options",
		"lights",
		"debug shapes",
		"markers",
		"texture tags",
		"pbr materials",
	};

	struct SectionHistory
	{
		float frameMs = 0.0f;
		std::array<float, historyFrames> samples{};
		int written = 0;
		SectionTiming timing;
	};

	static std::array<SectionHistory, profileSectionCount> sections;

	static double TicksPerMs()
	{
		static double ticksPerMs = 0.0;

		if (ticksPerMs == 0.0)
		{
			LARGE_INTEGER frequency{};
			QueryPerformanceFrequency(&frequency);
			ticksPerMs = static_cast<double>(frequency.QuadPart) / 1000.0;
		}

		return ticksPerMs;
	}

	ProfileScope::ProfileScope(ProfileSection profiled) : section(profiled), startTicks(0)
	{
		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);
		startTicks = now.QuadPart;
	}

	ProfileScope::~ProfileScope()
	{
		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);
		sections[static_cast<int>(section)].frameMs += static_cast<float>(static_cast<double>(now.QuadPart - startTicks)
			/ TicksPerMs());
	}

	void EndProfileFrame()
	{
		for (int index = 0; index < profileSectionCount; ++index)
		{
			SectionHistory& history = sections[index];
			SectionTiming& timing = history.timing;

			timing.name = sectionNames[index];
			timing.lastMs = history.frameMs;
			history.samples[history.written % historyFrames] = history.frameMs;
			++history.written;
			history.frameMs = 0.0f;

			const int count = std::min(history.written, historyFrames);
			std::array<float, historyFrames> sorted = history.samples;
			std::nth_element(sorted.begin(), sorted.begin() + count / 2, sorted.begin() + count);
			timing.medianMs = sorted[count / 2];
			timing.peakMs = *std::max_element(history.samples.begin(), history.samples.begin() + count);

			const bool isSpike = timing.lastMs > spikeFloorMs && timing.lastMs > timing.medianMs * spikeRatio;

			if (isSpike)
			{
				++timing.spikes;
				timing.spikeMs = timing.lastMs;
			}
		}
	}

	std::vector<SectionTiming> GetProfile()
	{
		std::vector<SectionTiming> result;

		for (const SectionHistory& history : sections)
		{
			result.push_back(history.timing);
		}

		return result;
	}
}
