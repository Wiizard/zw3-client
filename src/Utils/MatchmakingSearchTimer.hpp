#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>

namespace Utils
{
	class MatchmakingSearchTimer
	{
	public:
		using Clock = std::chrono::steady_clock;

		void SetSearching(const bool searching, const Clock::time_point now)
		{
			if (searching && !this->searching_)
			{
				this->startedAt_ = now;
			}
			this->searching_ = searching;
			if (!searching) this->startedAt_ = {};
		}

		int ElapsedSeconds(const Clock::time_point now) const
		{
			if (!this->searching_ || now <= this->startedAt_) return 0;
			const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - this->startedAt_).count();
			return static_cast<int>(std::min(seconds, static_cast<decltype(seconds)>(std::numeric_limits<int>::max())));
		}

		std::string Text(const Clock::time_point now) const
		{
			const auto elapsed = this->ElapsedSeconds(now);
			const auto seconds = elapsed % 60;
			return std::to_string(elapsed / 60) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
		}

	private:
		bool searching_ = false;
		Clock::time_point startedAt_{};
	};
}
