#pragma once

#include "Controller/Types.hpp"

#include "Controller/Sample/Axis.hpp"
#include "Controller/Sample/Button.hpp"

namespace Controller::Driver
{
	std::uint8_t ReadU8(std::span<const std::byte> data, std::size_t offset) noexcept;
	std::uint16_t ReadLe16(std::span<const std::byte> data, std::size_t offset) noexcept;
	std::int16_t ReadLe16Signed(std::span<const std::byte> data, std::size_t offset) noexcept;
	std::uint32_t ReadLe32(std::span<const std::byte> data, std::size_t offset) noexcept;

	std::uint32_t Crc32Le(std::uint32_t crc, std::span<const std::byte> data) noexcept;

	inline constexpr std::uint8_t psInputCrcSeed = 0xA1;
	inline constexpr std::uint8_t psOutputCrcSeed = 0xA2;

	bool IsPsCrc32Valid(std::uint8_t seed, std::span<const std::byte> data, std::uint32_t expected) noexcept;

	struct PsTouchPoint
	{
		bool isActive;
		std::uint8_t id;
		std::uint16_t x;
		std::uint16_t y;
	};

	PsTouchPoint DecodeTouchPoint(std::span<const std::byte> point) noexcept;

	void ApplyHat(ButtonSet& buttons, std::uint8_t hat) noexcept;

	StickVector NormalizePsStick(std::uint8_t x, std::uint8_t y) noexcept;
}
