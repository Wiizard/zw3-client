#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	enum class Stick : std::uint8_t
	{
		Left,
		Right,
	};

	inline constexpr std::size_t stickCount = 2;

	struct StickVector
	{
		float x = 0.0f;
		float y = 0.0f;

		float Magnitude() const noexcept;
	};

	struct StickRaw
	{
		std::int32_t x = 0;
		std::int32_t y = 0;
	};

	struct StickSample
	{
		StickRaw raw{};
		StickVector normalized{};
		StickVector calibrated{};
	};
}
