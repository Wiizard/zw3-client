#pragma once

#include <charconv>
#include <chrono>
#include <mutex>
#include <optional>
#include <string_view>
#include <json.hpp>

namespace Utils::ManagedRuntimeQuery
{
	inline std::optional<std::string_view> ReadChallenge(std::string_view data)
	{
		if (data.size() > 66) return std::nullopt;
		if (data.ends_with('\n')) data.remove_suffix(1);
		if (data.ends_with('\r')) data.remove_suffix(1);
		if (data.size() < 24 || data.size() > 64) return std::nullopt;
		for (const auto character : data)
			if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return std::nullopt;
		return data;
	}

	inline std::optional<int> ReadInteger(const std::string_view text, const int maximum)
	{
		if (text.empty() || text.size() > 10 || (text.size() > 1 && text.front() == '0')
			|| text.front() < '0' || text.front() > '9') return std::nullopt;
		int value = 0;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < 0 || value > maximum)
			return std::nullopt;
		return value;
	}

	inline nlohmann::json IntegerObservation(const char* type, const std::optional<int> value)
	{
		return { { "valid", value.has_value() }, { "type", type },
			{ "value", value ? nlohmann::json(*value) : nlohmann::json(nullptr) } };
	}

	class RateLimit
	{
	public:
		using Clock = std::chrono::steady_clock;
		bool Allow(const Clock::time_point now)
		{
			std::lock_guard lock(this->mutex);
			if (this->last && now - *this->last < std::chrono::seconds(1)) return false;
			this->last = now;
			return true;
		}
	private:
		std::mutex mutex;
		std::optional<Clock::time_point> last;
	};
}
