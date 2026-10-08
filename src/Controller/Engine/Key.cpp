#include "STDInclude.hpp"

#include "Controller/Engine/Key.hpp"
#include "Controller/Engine/Engine.hpp"
#include "Controller/Aim/Deadzone.hpp"
#include "Controller/Mapping/Physical.hpp"

#include "Components/Modules/Command.hpp"

namespace Controller::Engine
{
	using Mapping::EngineKey;

	static constexpr int lastPlusBinding = 65;

	static constexpr unsigned int adsSprintHoldMs = 600;

	struct ButtonKey
	{
		Button physical;
		EngineKey key;
	};

	static constexpr ButtonKey buttonKeys[] =
	{
		{ Button::FaceWest, EngineKey::ButtonX },
		{ Button::FaceSouth, EngineKey::ButtonA },
		{ Button::FaceEast, EngineKey::ButtonB },
		{ Button::FaceNorth, EngineKey::ButtonY },
		{ Button::L2, EngineKey::ButtonLTrigger },
		{ Button::R2, EngineKey::ButtonRTrigger },
		{ Button::L1, EngineKey::ButtonLShoulder },
		{ Button::R1, EngineKey::ButtonRShoulder },
		{ Button::Start, EngineKey::ButtonStart },
		{ Button::Back, EngineKey::ButtonBack },
		{ Button::L3, EngineKey::ButtonLStick },
		{ Button::R3, EngineKey::ButtonRStick },
		{ Button::DpadUp, EngineKey::DpadUp },
		{ Button::DpadDown, EngineKey::DpadDown },
		{ Button::DpadLeft, EngineKey::DpadLeft },
		{ Button::DpadRight, EngineKey::DpadRight },
	};

	struct MenuKey
	{
		EngineKey controller;
		int keyboard;
	};

	static constexpr MenuKey menuKeys[] =
	{
		{ EngineKey::ButtonA, Game::K_ENTER },
		{ EngineKey::ButtonStart, Game::K_ENTER },
		{ EngineKey::ButtonB, Game::K_ESCAPE },
		{ EngineKey::ButtonBack, Game::K_ESCAPE },
		{ EngineKey::DpadUp, Game::K_UPARROW },
		{ EngineKey::ApadUp, Game::K_UPARROW },
		{ EngineKey::RStickUp, Game::K_UPARROW },
		{ EngineKey::DpadDown, Game::K_DOWNARROW },
		{ EngineKey::ApadDown, Game::K_DOWNARROW },
		{ EngineKey::RStickDown, Game::K_DOWNARROW },
		{ EngineKey::DpadLeft, Game::K_LEFTARROW },
		{ EngineKey::ApadLeft, Game::K_LEFTARROW },
		{ EngineKey::RStickLeft, Game::K_LEFTARROW },
		{ EngineKey::DpadRight, Game::K_RIGHTARROW },
		{ EngineKey::ApadRight, Game::K_RIGHTARROW },
		{ EngineKey::RStickRight, Game::K_RIGHTARROW },
	};

	static bool IsDpad(EngineKey key) noexcept
	{
		return key >= EngineKey::DpadUp && key <= EngineKey::DpadRight;
	}

	static bool IsStickKey(EngineKey key) noexcept
	{
		const bool isApad = key >= EngineKey::ApadUp && key <= EngineKey::ApadRight;
		const bool isRightStick = key >= EngineKey::RStickUp && key <= EngineKey::RStickRight;

		return isApad || isRightStick;
	}

	static bool IsScrollKey(EngineKey key) noexcept
	{
		return IsDpad(key) || IsStickKey(key);
	}

	static bool RepeatsWhileHeld(Button button) noexcept
	{
		switch (button)
		{
		case Button::DpadUp:
		case Button::DpadDown:
		case Button::DpadLeft:
		case Button::DpadRight:
		case Button::L2:
		case Button::R2:
			return true;

		default:
			return false;
		}
	}

	static int BindingFor(EngineKey key) noexcept
	{
		return Game::playerKeys[localClient].keys[static_cast<int>(key)].binding;
	}

	static bool IsSprint(int binding)
	{
		static const int breathSprintBinding = Game::Key_GetBindingForCmd("+breath_sprint");
		static const int sprintBinding = Game::Key_GetBindingForCmd("+sprint");

		return binding != 0 && (binding == breathSprintBinding || binding == sprintBinding);
	}

	static EngineKey StickKey(Stick which, bool isHorizontal, bool isPositive) noexcept
	{
		Mapping::StickDirection direction = Mapping::StickDirection::Down;

		if (isHorizontal && isPositive)
		{
			direction = Mapping::StickDirection::Right;
		}
		else if (isHorizontal)
		{
			direction = Mapping::StickDirection::Left;
		}
		else if (isPositive)
		{
			direction = Mapping::StickDirection::Up;
		}

		return Mapping::ToEngineKey(Mapping::ApadInput{ which, direction });
	}

	static float AdsLerp() noexcept
	{
		const auto& aimAssist = Game::aaGlobArray[localClient];

		if (!aimAssist.initialized)
		{
			return 0.0f;
		}

		return aimAssist.adsLerp;
	}

	static Aim::DeadzoneParams StickDeadzone(const Dvars& dvars) noexcept
	{
		return Aim::DeadzoneParams
		{
			Aim::Magnitude{ Read(dvars.stickDeadzoneMin, 0.2f) },
			Aim::Magnitude{ Read(dvars.stickDeadzoneMax, 0.01f) },
			Aim::Magnitude{ Read(dvars.stickAntiDeadzone, 0.0f) },
		};
	}

	KeyDispatcher::KeyDispatcher(const Context& context, const Dvars& dvars)
		: context(context),
		dvars(dvars)
	{
	}

	void KeyDispatcher::SetInUse(bool isNowInUse)
	{
		if (this->isInUse == isNowInUse)
		{
			return;
		}

		this->isInUse = isNowInUse;

		if (this->dvars.inUse != nullptr)
		{
			Game::Dvar_SetBool(this->dvars.inUse, isNowInUse);
		}

		if (isNowInUse)
		{
			this->context.Report(Severity::Info, Facility::Engine, ErrorCode::None, "input source: controller");
		}
		else
		{
			this->context.Report(Severity::Info, Facility::Engine, ErrorCode::None, "input source: keyboard and mouse");
		}
	}

	unsigned int KeyDispatcher::ReleaseDelay() const noexcept
	{
		constexpr unsigned int longestDelayMs = 2000;

		if (!Read(this->dvars.releaseDelayEnabled, true))
		{
			return 0;
		}

		const auto floorMs = static_cast<unsigned int>(std::max(0, Read(this->dvars.releaseDelay, 50)));

		const float scale = Read(this->dvars.releaseDelayScale, 3.5f);

		if (!(scale > 0.0f))
		{
			return floorMs;
		}

		const int ping = std::max(0, SnapPing(localClient));
		const auto scaledMs = static_cast<unsigned int>(static_cast<float>(ping) * scale);

		return std::min(std::max(floorMs, scaledMs), longestDelayMs);
	}

	bool KeyDispatcher::DefersRelease(EngineKey key) const noexcept
	{
		if (this->ReleaseDelay() == 0 && Read(this->dvars.releaseGrace, 75) <= 0)
		{
			return false;
		}

		const bool isSprint = IsSprint(BindingFor(key));

		if (isSprint && IsSprintButtonUpRequired(localClient))
		{
			return false;
		}

		if (!Read(this->dvars.releaseDelaySprintOnly, true))
		{
			return true;
		}

		return isSprint;
	}

	void KeyDispatcher::NoteOtherInput()
	{
		this->SetInUse(false);
	}

	void KeyDispatcher::Dispatch(const CanonicalSample& sample)
	{
		const auto time = static_cast<unsigned int>(Game::Sys_Milliseconds());

		Aim::DeadzoneParams deadzone = StickDeadzone(this->dvars);

		std::string why;

		if (!Aim::IsValid(deadzone, why))
		{
			if (!this->hasReportedDeadzone)
			{
				this->hasReportedDeadzone = true;
				this->context.Report(Severity::Warning, Facility::Mapping, ErrorCode::CalibrationInvalid,
					std::format("gpad_stick_deadzone_min/max rejected ({}); using defaults for the analog pad keys", why));
			}

			deadzone = Aim::DeadzoneParams{ Aim::Magnitude{ 0.2f }, Aim::Magnitude{ 0.01f }, Aim::Magnitude{ 0.0f } };
		}

		const StickVector left = Aim::ApplyDeadzone(deadzone, sample.sticks[static_cast<std::size_t>(Stick::Left)].calibrated);
		const StickVector right = Aim::ApplyDeadzone(deadzone, sample.sticks[static_cast<std::size_t>(Stick::Right)].calibrated);

		const float triggerDeadzone = Read(this->dvars.buttonDeadzone, 0.13f);
		const float triggerMargin = Read(this->dvars.buttonDeadzoneHysteresis, defaultTriggerReleaseMargin);

		const auto isTriggerDown = [this, triggerDeadzone, triggerMargin](TriggerSide side, float value)
		{
			const auto index = static_cast<std::size_t>(side);

			const float engageAt = std::max(triggerDeadzone, this->engage[index]);
			const float releaseAt = std::max(triggerDeadzone, engageAt - triggerMargin);

			float threshold = engageAt;

			if (this->isTriggerHeld[index])
			{
				threshold = releaseAt;
			}

			this->isTriggerHeld[index] = value > 0.0f && value >= threshold;
			return this->isTriggerHeld[index];
		};

		const bool isLeftTriggerDown = isTriggerDown(TriggerSide::Left, sample.triggers[static_cast<std::size_t>(TriggerSide::Left)].normalized);
		const bool isRightTriggerDown = isTriggerDown(TriggerSide::Right, sample.triggers[static_cast<std::size_t>(TriggerSide::Right)].normalized);

		ButtonSet current = sample.buttons;
		current.Set(Button::L2, isLeftTriggerDown);
		current.Set(Button::R2, isRightTriggerDown);

		const bool isStickMoved = left.x != 0.0f || left.y != 0.0f || right.x != 0.0f || right.y != 0.0f;

		if (isStickMoved || isLeftTriggerDown || isRightTriggerDown)
		{
			this->SetInUse(true);
		}

		const float axes[axisCount] = { right.x, right.y, left.x, left.y };

		const Mapping::AxisThreshold threshold{ Read(this->dvars.stickPressed, 0.4f), Read(this->dvars.stickPressedHysteresis, 0.1f) };

		for (std::size_t i = 0; i != axisCount; ++i)
		{
			for (std::size_t end = 0; end != 2; ++end)
			{
				const bool isPositive = end == 1;

				this->wasDeflected[i][end] = this->isDeflected[i][end];
				this->isDeflected[i][end] = Mapping::IsAxisDeflected(axes[i], isPositive, this->wasDeflected[i][end], threshold);
			}
		}

		this->DispatchApad(time);
		this->DispatchButtons(current, time);

		this->buttons = current;
	}

	void KeyDispatcher::Tick()
	{
		this->DispatchButtons(this->buttons, static_cast<unsigned int>(Game::Sys_Milliseconds()));
	}

	void KeyDispatcher::DispatchApad(unsigned int time)
	{
		for (std::size_t i = 0; i != axisCount; ++i)
		{
			Stick which = Stick::Left;

			if (i < 2)
			{
				which = Stick::Right;
			}

			const bool isHorizontal = (i % 2) == 0;

			const EngineKey positiveKey = StickKey(which, isHorizontal, true);
			const EngineKey negativeKey = StickKey(which, isHorizontal, false);

			const bool isPositiveNow = this->isDeflected[i][1];
			const bool isNegativeNow = this->isDeflected[i][0];
			const bool wasPositive = this->wasDeflected[i][1];
			const bool wasNegative = this->wasDeflected[i][0];

			if (isPositiveNow)
			{
				KeyEvent event = KeyEvent::Pressed;

				if (wasPositive)
				{
					event = KeyEvent::Repeated;
				}

				this->Emit(positiveKey, event, time);
			}
			else if (isNegativeNow)
			{
				KeyEvent event = KeyEvent::Pressed;

				if (wasNegative)
				{
					event = KeyEvent::Repeated;
				}

				this->Emit(negativeKey, event, time);
			}
			else if (wasPositive)
			{
				this->Emit(positiveKey, KeyEvent::Released, time);
			}
			else if (wasNegative)
			{
				this->Emit(negativeKey, KeyEvent::Released, time);
			}
		}
	}

	void KeyDispatcher::UpdateAds() noexcept
	{
		const float ads = AdsLerp();

		bool isLowering = this->isAdsLowering;

		if (ads != this->adsLerp)
		{
			isLowering = ads < this->adsLerp;
		}

		this->isAdsLowering = ads > 0.0f && isLowering;
		this->adsLerp = ads;
	}

	void KeyDispatcher::DispatchButtons(const ButtonSet& current, unsigned int time)
	{
		this->UpdateAds();

		const unsigned int delay = this->ReleaseDelay();
		const auto grace = static_cast<unsigned int>(std::max(0, Read(this->dvars.releaseGrace, 75)));

		for (const auto& buttonKey : buttonKeys)
		{
			const auto index = static_cast<std::size_t>(buttonKey.physical);

			const bool isDownNow = current.IsDown(buttonKey.physical);
			const bool wasDown = this->buttons.IsDown(buttonKey.physical);

			if (isDownNow && !wasDown)
			{
				this->deferred.Set(buttonKey.physical, false);
				this->pressedAt[index] = time;

				this->EmitButton(buttonKey.key, KeyEvent::Pressed, time);
				continue;
			}

			if (isDownNow && RepeatsWhileHeld(buttonKey.physical))
			{
				this->EmitButton(buttonKey.key, KeyEvent::Repeated, time);
				continue;
			}

			if (!isDownNow && wasDown)
			{
				if (this->DefersRelease(buttonKey.key))
				{
					this->releasedAt[index] = time;
					this->deferred.Set(buttonKey.physical, true);
					this->EmitButton(buttonKey.key, KeyEvent::Repeated, time);
					continue;
				}

				this->EmitButton(buttonKey.key, KeyEvent::Released, time);
				continue;
			}

			if (isDownNow || !this->deferred.IsDown(buttonKey.physical))
			{
				continue;
			}

			const bool isExpired = time - this->pressedAt[index] >= delay && time - this->releasedAt[index] >= grace;
			const bool isLowering = IsSprint(BindingFor(buttonKey.key)) && this->isAdsLowering && time - this->releasedAt[index] < adsSprintHoldMs;

			if (!this->DefersRelease(buttonKey.key) || (isExpired && !isLowering))
			{
				this->deferred.Set(buttonKey.physical, false);
				this->EmitButton(buttonKey.key, KeyEvent::Released, time);
			}
			else
			{
				this->EmitButton(buttonKey.key, KeyEvent::Repeated, time);
			}
		}
	}

	void KeyDispatcher::EmitButton(EngineKey key, KeyEvent event, unsigned int time)
	{
		this->SetInUse(true);

		if (Game::Key_IsCatcherActive(localClient, Game::KEYCATCH_UI))
		{
			this->ResetScroll(key, event == KeyEvent::Pressed, time);
		}

		this->Emit(key, event, time);
	}

	void KeyDispatcher::Emit(EngineKey key, KeyEvent event, unsigned int time)
	{
		static const int locationCancelBinding = Game::Key_GetBindingForCmd("+actionslot 4");
		static const int locationConfirmBinding = Game::Key_GetBindingForCmd("+attack");

		const int keyNum = static_cast<int>(key);
		const bool isDown = event != KeyEvent::Released;

		auto& keyState = Game::playerKeys[localClient];
		auto& padKey = keyState.keys[keyNum];

		padKey.down = 0;

		if (isDown)
		{
			padKey.down = 1;

			if (++padKey.repeats == 1)
			{
				++keyState.anyKeyDown;
			}
		}
		else if (padKey.repeats > 0)
		{
			padKey.repeats = 0;

			if (--keyState.anyKeyDown < 0)
			{
				keyState.anyKeyDown = 0;
			}
		}

		if (isDown && this->ShouldIgnoreRepeat(key, padKey.repeats, time))
		{
			return;
		}

		const int binding = padKey.binding;

		if (isDown && Game::Key_IsCatcherActive(localClient, Game::KEYCATCH_LOCATION_SELECTION))
		{
			if (key == EngineKey::ButtonB || (binding != 0 && binding == locationCancelBinding))
			{
				keyState.locSelInputState = Game::LOC_SEL_INPUT_CANCEL;
			}
			else if (key == EngineKey::ButtonA || (binding != 0 && binding == locationConfirmBinding))
			{
				keyState.locSelInputState = Game::LOC_SEL_INPUT_CONFIRM;
			}

			return;
		}

		if (Game::UI_GetActiveMenu(localClient) == Game::UIMENU_SCOREBOARD && event == KeyEvent::Pressed && this->TryScoreboardKeyEvent(key))
		{
			return;
		}

		keyState.locSelInputState = Game::LOC_SEL_INPUT_NONE;

		if (isDown)
		{
			if (Game::Key_IsCatcherActive(localClient, Game::KEYCATCH_UI))
			{
				this->MenuKeyEvent(key, true);
				return;
			}

			if (binding != 0)
			{
				Components::Command::ExecBinding(localClient, binding, keyNum);
			}

			return;
		}

		if (binding != 0 && binding < lastPlusBinding && (binding & 1) != 0)
		{
			Components::Command::ExecBinding(localClient, binding + 1, keyNum);
		}

		if (Game::Key_IsCatcherActive(localClient, Game::KEYCATCH_UI))
		{
			this->MenuKeyEvent(key, false);
		}
	}

	bool KeyDispatcher::ShouldIgnoreRepeat(EngineKey key, int repeats, unsigned int time)
	{
		if (Game::Key_IsCatcherActive(localClient, Game::KEYCATCH_UI) && IsScrollKey(key))
		{
			const int delayFirst = Read(this->dvars.menuScrollDelayFirst, 420);
			const int delayRest = Read(this->dvars.menuScrollDelayRest, 210);
			const int delayLeast = Read(this->dvars.menuScrollDelayMin, 50);
			const int accelTime = Read(this->dvars.menuScrollAccelTime, 1500);

			if (repeats == 1)
			{
				this->nextScroll = time + static_cast<unsigned int>(delayFirst);
				return false;
			}

			if (time > this->nextScroll)
			{
				int delay = delayRest;

				if (IsDpad(key) && accelTime > 0 && delayRest > delayLeast)
				{
					const auto elapsed = static_cast<int>(time - this->scrollHoldStart);
					const int heldTime = std::min(elapsed, accelTime);

					delay = delayRest - (delayRest - delayLeast) * heldTime / accelTime;
				}

				this->nextScroll = time + static_cast<unsigned int>(delay);
				return false;
			}
		}

		return repeats > 1;
	}

	void KeyDispatcher::ResetScroll(EngineKey key, bool isDown, unsigned int time)
	{
		if (!isDown)
		{
			this->scrollHoldKey.reset();
			return;
		}

		if (!IsScrollKey(key))
		{
			return;
		}

		if (IsDpad(key) && this->scrollHoldKey != key)
		{
			this->scrollHoldStart = time;
			this->scrollHoldKey = key;
		}

		this->nextScroll = time + static_cast<unsigned int>(Read(this->dvars.menuScrollDelayFirst, 420));
	}

	void KeyDispatcher::MenuKeyEvent(EngineKey key, bool isDown)
	{
		int down = 0;

		if (isDown)
		{
			down = 1;
		}

		if (*Game::g_waitingForKey != 0)
		{
			Game::UI_KeyEvent(localClient, static_cast<int>(key), down);
			return;
		}

		for (const auto& menuKey : menuKeys)
		{
			if (menuKey.controller == key)
			{
				Game::UI_KeyEvent(localClient, menuKey.keyboard, down);
				return;
			}
		}
	}

	bool KeyDispatcher::TryScoreboardKeyEvent(EngineKey key)
	{
		if (key == EngineKey::DpadUp)
		{
			Game::Scoreboard_HandleInput(localClient, Game::K_PGUP);
			return true;
		}

		if (key == EngineKey::DpadDown)
		{
			Game::Scoreboard_HandleInput(localClient, Game::K_PGDN);
			return true;
		}

		return false;
	}

	void KeyDispatcher::SetTriggerEngage(float left, float right) noexcept
	{
		this->engage[static_cast<std::size_t>(TriggerSide::Left)] = left;
		this->engage[static_cast<std::size_t>(TriggerSide::Right)] = right;
	}

	void KeyDispatcher::ReleaseAll()
	{
		const auto time = static_cast<unsigned int>(Game::Sys_Milliseconds());
		const auto& keyState = Game::playerKeys[localClient];

		for (const auto key : Mapping::Keys())
		{
			if (keyState.keys[static_cast<int>(key)].down != 0)
			{
				this->Emit(key, KeyEvent::Released, time);
			}
		}

		this->buttons = ButtonSet();
		this->deferred = ButtonSet();
		this->isTriggerHeld = {};
		this->adsLerp = 0.0f;
		this->isAdsLowering = false;
		this->pressedAt = {};
		this->releasedAt = {};
		this->isDeflected = {};
		this->wasDeflected = {};
		this->scrollHoldKey.reset();
	}
}
