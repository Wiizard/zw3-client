#pragma once

#include "Controller/Types.hpp"

namespace Controller::Engine
{
	struct Dvars
	{
		Game::dvar_t* enabled = nullptr;
		Game::dvar_t* present = nullptr;
		Game::dvar_t* inUse = nullptr;
		Game::dvar_t* rumble = nullptr;
		Game::dvar_t* style = nullptr;

		Game::dvar_t* haptics = nullptr;
		Game::dvar_t* hapticIntensity = nullptr;
		Game::dvar_t* rumbleScaleLow = nullptr;
		Game::dvar_t* rumbleScaleHigh = nullptr;
		Game::dvar_t* adaptiveTriggers = nullptr;
		Game::dvar_t* outputInterval = nullptr;

		Game::dvar_t* adaptiveTriggerStrength = nullptr;
		Game::dvar_t* adaptiveTriggerLight = nullptr;
		Game::dvar_t* adaptiveTriggerHeavy = nullptr;
		Game::dvar_t* adaptiveTriggerLightStart = nullptr;
		Game::dvar_t* adaptiveTriggerLightEnd = nullptr;
		Game::dvar_t* adaptiveTriggerHeavyStart = nullptr;
		Game::dvar_t* adaptiveTriggerHeavyEnd = nullptr;
		Game::dvar_t* adaptiveTriggerAds = nullptr;

		Game::dvar_t* lightBar = nullptr;
		Game::dvar_t* lightBarBrightness = nullptr;
		Game::dvar_t* lightBarRed = nullptr;
		Game::dvar_t* lightBarGreen = nullptr;
		Game::dvar_t* lightBarBlue = nullptr;

		Game::dvar_t* stickDeadzoneMin = nullptr;
		Game::dvar_t* stickDeadzoneMax = nullptr;
		Game::dvar_t* stickAntiDeadzone = nullptr;
		Game::dvar_t* buttonDeadzone = nullptr;
		Game::dvar_t* buttonDeadzoneHysteresis = nullptr;
		Game::dvar_t* stickPressed = nullptr;
		Game::dvar_t* stickPressedHysteresis = nullptr;

		Game::dvar_t* buttonsConfig = nullptr;
		Game::dvar_t* sticksConfig = nullptr;

		Game::dvar_t* menuScrollDelayFirst = nullptr;
		Game::dvar_t* menuScrollDelayRest = nullptr;
		Game::dvar_t* menuScrollDelayMin = nullptr;
		Game::dvar_t* menuScrollAccelTime = nullptr;

		Game::dvar_t* useHoldTime = nullptr;

		Game::dvar_t* releaseDelayEnabled = nullptr;
		Game::dvar_t* releaseDelay = nullptr;
		Game::dvar_t* releaseDelayScale = nullptr;
		Game::dvar_t* releaseDelaySprintOnly = nullptr;
		Game::dvar_t* releaseGrace = nullptr;

		Game::dvar_t* invertPitch = nullptr;
		Game::dvar_t* viewSensitivity = nullptr;
		Game::dvar_t* aimAssistEnabled = nullptr;
		Game::dvar_t* turnRatePitch = nullptr;
		Game::dvar_t* turnRatePitchAds = nullptr;
		Game::dvar_t* turnRateYaw = nullptr;
		Game::dvar_t* turnRateYawAds = nullptr;
		Game::dvar_t* accelEnabled = nullptr;
		Game::dvar_t* accelRate = nullptr;
		Game::dvar_t* graphEnabled = nullptr;
		Game::dvar_t* graphIndex = nullptr;
		Game::dvar_t* scaleViewAxis = nullptr;

		Game::dvar_t* slowdownEnabled = nullptr;
		Game::dvar_t* gpadSlowdownEnabled = nullptr;
		Game::dvar_t* slowdownPitchScale = nullptr;
		Game::dvar_t* slowdownPitchScaleAds = nullptr;
		Game::dvar_t* slowdownYawScale = nullptr;
		Game::dvar_t* slowdownYawScaleAds = nullptr;
		Game::dvar_t* lockOnEnabled = nullptr;
		Game::dvar_t* gpadLockOnEnabled = nullptr;
		Game::dvar_t* lockOnDeflection = nullptr;
		Game::dvar_t* lockOnStrength = nullptr;
		Game::dvar_t* lockOnPitchStrength = nullptr;
		Game::dvar_t* aimAssistRangeScale = nullptr;
	};

	Dvars& RegisteredDvars() noexcept;

	void RegisterDvars();

	void PublishPresent(const Dvars& dvars, bool isPresent);

	inline bool Read(const Game::dvar_t* dvar, bool fallback) noexcept
	{
		if (dvar == nullptr)
		{
			return fallback;
		}

		return dvar->current.enabled;
	}

	inline float Read(const Game::dvar_t* dvar, float fallback) noexcept
	{
		if (dvar == nullptr)
		{
			return fallback;
		}

		return dvar->current.value;
	}

	inline int Read(const Game::dvar_t* dvar, int fallback) noexcept
	{
		if (dvar == nullptr)
		{
			return fallback;
		}

		return dvar->current.integer;
	}

	inline const char* Read(const Game::dvar_t* dvar, const char* fallback) noexcept
	{
		if (dvar == nullptr || dvar->current.string == nullptr)
		{
			return fallback;
		}

		return dvar->current.string;
	}
}
