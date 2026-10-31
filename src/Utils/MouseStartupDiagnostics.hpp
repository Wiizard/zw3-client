#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace Utils
{
	class MouseStartupDiagnostics
	{
	public:
		static constexpr std::uint64_t durationMs = 60000;
		enum State : unsigned int
		{
			rawInput = 1, focused = 2, ui = 4, loading = 8, engineMouseActive = 16
		};
		struct Sample
		{
			unsigned int frames = 0;
			unsigned int rawMessages = 0;
			unsigned int registrations = 0;
			unsigned int registrationFailures = 0;
			unsigned int warps = 0;
			unsigned int states = 0;
			std::uint64_t maximumFrameGapMs = 0;
			std::uint32_t maximumMessageAgeMs = 0;
		};
		using Samples = std::array<Sample, durationMs / 1000>;

		void Start(const std::uint64_t now)
		{
			*this = {};
			started = now;
			active = true;
		}
		bool Frame(const std::uint64_t now, const unsigned int state)
		{
			auto* sample = At(now);
			if (!sample) return false;
			++sample->frames;
			sample->states |= state;
			if (hasFrame) sample->maximumFrameGapMs = std::max(sample->maximumFrameGapMs, now - lastFrame);
			lastFrame = now;
			hasFrame = true;
			return true;
		}
		void RawMessage(const std::uint64_t now, const std::uint32_t messageAgeMs)
		{
			if (auto* sample = At(now))
			{
				++sample->rawMessages;
				sample->maximumMessageAgeMs = std::max(sample->maximumMessageAgeMs, messageAgeMs);
			}
		}
		void Registration(const std::uint64_t now, const bool successful)
		{
			if (auto* sample = At(now))
			{
				++sample->registrations;
				if (!successful) ++sample->registrationFailures;
			}
		}
		void Warp(const std::uint64_t now)
		{
			if (auto* sample = At(now)) ++sample->warps;
		}
		bool IsActive() const { return active; }
		bool IsFinished(const std::uint64_t now) const { return active && now >= started && now - started >= durationMs; }
		Samples Stop()
		{
			active = false;
			return samples;
		}

	private:
		Sample* At(const std::uint64_t now)
		{
			if (!active || now < started || now - started >= durationMs) return nullptr;
			return &samples[static_cast<std::size_t>((now - started) / 1000)];
		}
		Samples samples{};
		std::uint64_t started = 0;
		std::uint64_t lastFrame = 0;
		bool active = false;
		bool hasFrame = false;
	};
}
