#pragma once

#include "Controller/Types.hpp"

#include "Controller/Sample/Axis.hpp"
#include "Controller/Sample/Button.hpp"
#include "Controller/Sample/Motion.hpp"
#include "Controller/Sample/Touch.hpp"
#include "Controller/Sample/Trigger.hpp"

#include "Controller/Device/Capability.hpp"

namespace Controller
{
	struct BatteryState
	{
		enum class Status : std::uint8_t
		{
			Unknown,
			Discharging,
			Charging,
			Full,
		};

		Status state = Status::Unknown;
		std::optional<std::uint8_t> percent;
	};

	struct RawSample
	{
		std::array<StickRaw, stickCount> sticks{};
		std::array<std::uint16_t, triggerCount> triggers{};
		std::uint32_t buttons = 0;
		std::optional<MotionSample> motion;
		std::optional<Touchpad> touch;
		std::optional<BatteryState> battery;
	};

	struct CanonicalSample
	{
		ButtonSet buttons{};
		std::array<StickSample, stickCount> sticks{};
		std::array<TriggerSample, triggerCount> triggers{};
		std::optional<Touchpad> touch;
		std::optional<MotionSample> motion;
		std::optional<BatteryState> battery;
		Capabilities caps{};
	};
}
