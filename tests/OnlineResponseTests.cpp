#include "Utils/OnlineResponse.hpp"

#include <iostream>
#include <stdexcept>

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

int main()
{
	try
	{
		using Clock = Utils::OnlineMetric::Clock;
		const auto start = Clock::time_point{};
		Utils::OnlineMetric online;
		Utils::OnlineMetric running;
		Require(!online.Current(start), "unknown metric must not become zero");
		const nlohmann::json legacy = {{"online_players", 7}};
		online.Observe(Utils::ReadOnlineMetric(legacy, "online_players"), start);
		running.Observe(Utils::ReadOnlineMetric(legacy, "running_games"), start);
		Require(online.Current(start) == 7 && !running.Current(start), "legacy backend must preserve online count independently");
		running.Observe(2, start);
		online.Observe(9, start + std::chrono::seconds(25));
		Require(!running.Current(start + std::chrono::seconds(31)), "other metric refresh must not keep running count fresh");
		Require(online.Current(start + std::chrono::seconds(31)) == 9, "fresh online metric lost");
		Require(!online.Current(start + std::chrono::seconds(56)), "stale online metric must be unavailable");
		Require(Utils::ReadOnlineMetric({{"online_players", 0}}, "online_players") == 0, "confirmed zero must be displayed");
		for (const auto& bad : {nlohmann::json(-1), nlohmann::json(1.5), nlohmann::json("5"), nlohmann::json(nullptr), nlohmann::json(UINT64_MAX)})
		{
			Require(!Utils::ReadOnlineMetric({{"online_players", bad}}, "online_players"), "invalid metric accepted");
		}
		Require(!Utils::ReadOnlineMetric({{"error", {{"code", "UNAVAILABLE"}}}, {"online_players", 0}}, "online_players"), "error payload accepted as count");
		for (const auto* code : {"PARTY_FULL", "PLAYER_BLOCKED", "INVITE_INVALID", "INVITE_REQUIRED", "SESSION_INVALID"})
		{
			const auto response = Utils::ParseSocialResponse(nlohmann::json({{"error", {{"code", code}}}}).dump(), false);
			Require(!response.value && response.error == code, "HTTP rejection lost its backend reason");
		}
		Require(!Utils::ParseSocialResponse("{\"id\":\"pty_test\"}", false).value, "HTTP error accepted as a successful join");
		Require(Utils::ParseSocialResponse("not json", false).error == "REQUEST_UNAVAILABLE", "invalid response incorrectly resolves invite");
		Require(Utils::ParseSocialResponse("{\"ok\":false}", true).error == "REQUEST_FAILED", "application rejection accepted");
		Require(Utils::ParseSocialResponse("{\"id\":\"pty_test\",\"members\":[]}", true).value.has_value(), "successful party join lost");
		std::cout << "Online response regression tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
