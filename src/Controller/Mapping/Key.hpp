#pragma once

#include "Controller/Types.hpp"

namespace Controller::Mapping
{
	enum class EngineKey : int
	{
		ButtonA = 0x01,
		ButtonB = 0x02,
		ButtonX = 0x03,
		ButtonY = 0x04,
		ButtonLShoulder = 0x05,
		ButtonRShoulder = 0x06,

		ButtonStart = 0x0E,
		ButtonBack = 0x0F,
		ButtonLStick = 0x10,
		ButtonRStick = 0x11,
		ButtonLTrigger = 0x12,
		ButtonRTrigger = 0x13,

		DpadUp = 0x14,
		DpadDown = 0x15,
		DpadLeft = 0x16,
		DpadRight = 0x17,

		ApadUp = 0x1C,
		ApadDown = 0x1D,
		ApadLeft = 0x1E,
		ApadRight = 0x1F,

		RStickUp = 0xE0,
		RStickDown = 0xE1,
		RStickLeft = 0xE2,
		RStickRight = 0xE3,
	};

	inline constexpr std::size_t engineKeyCount = 24;

	static_assert(static_cast<int>(EngineKey::RStickRight) < 256, "controller keys must fit the engine's key state array");

	std::span<const EngineKey> Keys() noexcept;

	std::size_t KeyIndex(EngineKey key) noexcept;

	bool IsControllerKey(int keyNum) noexcept;

	const char* KeyName(EngineKey key) noexcept;
}
