#pragma once

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <json.hpp>

namespace Utils
{
	inline std::optional<int> ReadOnlineMetric(const nlohmann::json& document, const char* field)
	{
		if (!document.is_object() || document.contains("error")) return std::nullopt;
		const auto value = document.find(field);
		if (value == document.end() || !value->is_number_integer()) return std::nullopt;
		if (value->is_number_unsigned())
		{
			const auto count = value->get<std::uint64_t>();
			if (count > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return std::nullopt;
			return static_cast<int>(count);
		}
		const auto count = value->get<std::int64_t>();
		if (count < 0 || count > std::numeric_limits<int>::max()) return std::nullopt;
		return static_cast<int>(count);
	}

	class OnlineMetric
	{
	public:
		using Clock = std::chrono::steady_clock;

		void Observe(std::optional<int> value, Clock::time_point now)
		{
			if (value)
			{
				count = value;
				observedAt = now;
			}
		}

		std::optional<int> Current(Clock::time_point now) const
		{
			if (!count || now < observedAt || now - observedAt > std::chrono::seconds(30)) return std::nullopt;
			return count;
		}

	private:
		std::optional<int> count;
		Clock::time_point observedAt{};
	};

	struct SocialResponse
	{
		std::optional<nlohmann::json> value;
		std::string error;
	};

	inline SocialResponse ParseSocialResponse(const std::string& body, bool httpSuccess)
	{
		const auto document = nlohmann::json::parse(body, nullptr, false);
		if (document.is_discarded()) return {std::nullopt, "REQUEST_UNAVAILABLE"};
		if (document.is_object() && document.contains("error"))
		{
			const auto& error = document.at("error");
			if (error.is_object() && error.contains("code") && error.at("code").is_string())
			{
				const auto code = error.at("code").get<std::string>();
				if (!code.empty() && code.size() <= 128) return {std::nullopt, code};
			}
			return {std::nullopt, "REQUEST_FAILED"};
		}
		if (!httpSuccess) return {std::nullopt, "REQUEST_UNAVAILABLE"};
		if (document.is_object() && document.contains("ok") && document.at("ok") == false)
		{
			return {std::nullopt, "REQUEST_FAILED"};
		}
		return {document, {}};
	}
}
