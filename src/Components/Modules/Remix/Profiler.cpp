#include "STDInclude.hpp"

#include "Profiler.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "Core.hpp"
#include "FixedFunction.hpp"
#include "Lights.hpp"
#include "Look.hpp"
#include "Weather.hpp"

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
		"rain",
		"particle clouds",
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

	void LogPerformance()
	{
		static constexpr double reportMs = 10000.0;
		static std::vector<float> frameMs;
		static std::int64_t lastFrameTicks = 0;
		static std::int64_t reportStartTicks = 0;
		static WorldStats lastWorld{};
		static WeatherStats lastWeather{};
		static std::array<float, profileSectionCount> sectionMsSum{};

		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);

		if (lastFrameTicks != 0)
		{
			frameMs.push_back(static_cast<float>(static_cast<double>(now.QuadPart - lastFrameTicks) / TicksPerMs()));
		}

		lastFrameTicks = now.QuadPart;

		for (int index = 0; index < profileSectionCount; ++index)
		{
			sectionMsSum[index] += sections[index].timing.lastMs;
		}

		if (reportStartTicks == 0)
		{
			reportStartTicks = now.QuadPart;
			lastWorld = GetWorldStats();
			lastWeather = GetWeatherStats();
			return;
		}

		if (static_cast<double>(now.QuadPart - reportStartTicks) / TicksPerMs() < reportMs || frameMs.empty())
		{
			return;
		}

		const double frames = static_cast<double>(frameMs.size());
		double totalMs = 0.0;

		for (const float ms : frameMs)
		{
			totalMs += ms;
		}

		std::vector<float> sorted = frameMs;
		std::sort(sorted.begin(), sorted.end());
		const float worstPercent = sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(frames * 0.99))];
		const float worst = sorted.back();

		const WorldStats world = GetWorldStats();
		const WeatherStats weather = GetWeatherStats();
		const LightStats lights = GetLightStats();
		const auto perFrame = [frames](unsigned long long now, unsigned long long before)
		{
			return static_cast<double>(now - before) / frames;
		};

		std::string sectionText;

		for (int index = 0; index < profileSectionCount; ++index)
		{
			const double averageMs = sectionMsSum[index] / frames;

			if (averageMs >= 0.05)
			{
				char part[64];
				std::snprintf(part, sizeof(part), "%s%s %.2f", sectionText.empty() ? "" : ", ", sectionNames[index], averageMs);
				sectionText += part;
			}

			sectionMsSum[index] = 0.0f;
		}

		char text[768];
		std::snprintf(text, sizeof(text),
			"perf %s: %.1f fps, frame %.2f ms (worst 1%% %.2f, worst %.2f); zw3 per frame: %s; drawn per frame: world"
			" batches %.0f, prop batches %.0f (chunks %.0f drawn, %.0f past the draw distance), entity lists %.0f, skinned lists %.0f, rain and cloud quads %.0f (%.0f cloud"
			" particles left out by the budget); lights %d map"
			" %d fixtures %d dynamic",
			Core::GetActiveMap().empty() ? "menu" : Core::GetActiveMap().c_str(), 1000.0 * frames / totalMs, totalMs / frames,
			worstPercent, worst, sectionText.empty() ? "-" : sectionText.c_str(), perFrame(world.batchDraws, lastWorld.batchDraws),
			perFrame(world.staticBatchDraws, lastWorld.staticBatchDraws), perFrame(world.chunksDrawn, lastWorld.chunksDrawn),
			perFrame(world.chunksBeyondDrawDistance, lastWorld.chunksBeyondDrawDistance),
			perFrame(world.entityModelLists, lastWorld.entityModelLists),
			perFrame(world.skinnedModelLists, lastWorld.skinnedModelLists),
			perFrame(weather.particlesDrawn, lastWeather.particlesDrawn),
			perFrame(weather.particlesOffscreen + weather.particlesThinned, lastWeather.particlesOffscreen + lastWeather.particlesThinned),
			lights.mapLights, lights.drawnFixtureLights,
			lights.dynamicLights);
		Log() << text;

		frameMs.clear();
		reportStartTicks = now.QuadPart;
		lastWorld = world;
		lastWeather = weather;
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
