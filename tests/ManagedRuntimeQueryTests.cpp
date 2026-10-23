#include "Utils/ManagedRuntimeQuery.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using namespace Utils::ManagedRuntimeQuery;
		const std::string nonce = "0123456789abcdef0123456789abcdef";
		Require(ReadChallenge(nonce) == nonce && ReadChallenge(nonce + "\n") == nonce
			&& ReadChallenge(nonce + "\r\n") == nonce, "valid nonce rejected or changed");
		for (const auto& invalid : { std::string(), std::string(23, 'a'), std::string(65, 'a'), nonce + " x", nonce + std::string("\0x", 2),
			std::string(32, 'A'), nonce + "\n\n", " " + nonce })
			Require(!ReadChallenge(invalid), "invalid or injectable nonce accepted");
		Require(ReadInteger("0", 1) == 0 && ReadInteger("1", 1) == 1 && ReadInteger("9999", 9999) == 9999,
			"actual valid integer was not preserved");
		for (const auto* invalid : { "", "-1", "+1", "01", " 0", "0 ", "false", "true", "0.0", "1garbage", "2147483648" })
			Require(!ReadInteger(invalid, 9999), "unknown/malformed value was coerced to an integer");
		Require(!ReadInteger("2", 1) && !ReadInteger("10000", 9999), "out-of-range runtime flag accepted");
		const auto missing = IntegerObservation("unknown", std::nullopt);
		Require(missing.at("valid") == false && missing.at("type") == "unknown" && missing.at("value").is_null(),
			"missing DVAR fabricated zero");
		const auto zero = IntegerObservation("string", ReadInteger("0", 1));
		Require(zero.at("valid") == true && zero.at("type") == "string" && zero.at("value").is_number_integer() && zero.at("value") == 0,
			"real typed zero did not remain distinguishable from unknown");
		RateLimit limiter;
		const auto at = RateLimit::Clock::time_point{};
		Require(limiter.Allow(at) && !limiter.Allow(at) && !limiter.Allow(at + std::chrono::milliseconds(999))
			&& limiter.Allow(at + std::chrono::seconds(1)), "bounded local query rate limit failed");
		std::cout << "Managed runtime nonce, strict observation, unavailable-state and rate-limit tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
