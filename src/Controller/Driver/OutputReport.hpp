#pragma once

#include "Controller/Types.hpp"

#include "Controller/Device/Id.hpp"
#include "Controller/Driver/Output.hpp"
#include "Controller/Haptic/Effect.hpp"

namespace Controller::Driver
{
	inline constexpr std::size_t ds4OutputUsbSize = 32;
	inline constexpr std::size_t ds4OutputBluetoothSize = 78;
	inline constexpr std::size_t dsOutputUsbSize = 63;
	inline constexpr std::size_t dsOutputBluetoothSize = 78;

	std::optional<std::size_t> TryEncodeDualShock4Output(const OutputRequest& request, Connection link, std::span<std::byte> out) noexcept;

	std::optional<std::size_t> TryEncodeDualSenseOutput(const OutputRequest& request, Connection link, std::uint8_t& bluetoothSequence, std::span<std::byte> out) noexcept;

	inline constexpr std::uint32_t hapticSampleRate = 3000;
	inline constexpr std::size_t hapticFramesPerReport = 32;
	inline constexpr std::size_t dsHapticReportSize = 142;

	std::optional<std::size_t> TryEncodeDualSenseHaptics(std::span<const Haptic::Frame> frames, std::uint8_t& counter, std::span<std::byte> out) noexcept;
}
