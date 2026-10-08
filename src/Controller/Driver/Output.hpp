#pragma once

#include "Controller/Types.hpp"

#include "Controller/Haptic/Effect.hpp"
#include "Controller/Sample/Trigger.hpp"

namespace Controller::Driver
{
	enum class HapticMode : std::uint8_t
	{
		Waveform,
		Emulated,
	};

	struct RumbleRequest
	{
		float lowFrequency = 0.0f;
		float highFrequency = 0.0f;
	};

	struct OutputPolicy
	{
		bool isRumbleEnabled = true;
		HapticMode hapticMode = HapticMode::Waveform;
		unsigned int outputIntervalMs = 4;
	};

	struct LightBarRequest
	{
		std::uint8_t red = 0;
		std::uint8_t green = 0;
		std::uint8_t blue = 0;
	};

	enum class TriggerEffect : std::uint8_t
	{
		Off,
		Feedback,
		Weapon,
	};

	inline constexpr std::size_t triggerZoneCount = 10;

	using TriggerProfile = std::array<std::uint8_t, triggerZoneCount>;

	struct AdaptiveTriggerRequest
	{
		TriggerSide side = TriggerSide::Left;
		TriggerEffect effect = TriggerEffect::Off;

		TriggerProfile zones{};

		std::uint8_t startPosition = 0;
		std::uint8_t endPosition = 0;
		std::uint8_t strength = 0;
	};

	using OutputRequest = std::variant<RumbleRequest, Haptic::Effect, LightBarRequest, AdaptiveTriggerRequest>;
}
