#include "STDInclude.hpp"

#include "Controller/Driver/XInput.hpp"

namespace Controller::Driver
{
	static constexpr std::uint16_t xinputGamepadGuide = 0x0400;

	static constexpr float thumbFullScale = 32767.0f;

	static StickVector NormalizeStick(std::int16_t rawX, std::int16_t rawY) noexcept
	{
		float x = std::clamp(static_cast<float>(rawX) / thumbFullScale, -1.0f, 1.0f);
		float y = std::clamp(static_cast<float>(rawY) / thumbFullScale, -1.0f, 1.0f);

		const float magnitude = std::sqrt(x * x + y * y);

		if (magnitude > 1.0f)
		{
			x /= magnitude;
			y /= magnitude;
		}

		return { x, y };
	}

	static TriggerSample DecodeTrigger(std::uint8_t raw) noexcept
	{
		return { raw, static_cast<float>(raw) / 255.0f };
	}

	static ButtonSet DecodeButtons(std::uint16_t pressed, bool hasGuide) noexcept
	{
		ButtonSet buttons;

		buttons.Set(Button::FaceSouth, (pressed & XINPUT_GAMEPAD_A) != 0);
		buttons.Set(Button::FaceEast, (pressed & XINPUT_GAMEPAD_B) != 0);
		buttons.Set(Button::FaceWest, (pressed & XINPUT_GAMEPAD_X) != 0);
		buttons.Set(Button::FaceNorth, (pressed & XINPUT_GAMEPAD_Y) != 0);

		buttons.Set(Button::DpadUp, (pressed & XINPUT_GAMEPAD_DPAD_UP) != 0);
		buttons.Set(Button::DpadDown, (pressed & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
		buttons.Set(Button::DpadLeft, (pressed & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
		buttons.Set(Button::DpadRight, (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);

		buttons.Set(Button::L1, (pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
		buttons.Set(Button::R1, (pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
		buttons.Set(Button::L3, (pressed & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
		buttons.Set(Button::R3, (pressed & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);

		buttons.Set(Button::Start, (pressed & XINPUT_GAMEPAD_START) != 0);
		buttons.Set(Button::Back, (pressed & XINPUT_GAMEPAD_BACK) != 0);

		if (hasGuide)
		{
			buttons.Set(Button::Guide, (pressed & xinputGamepadGuide) != 0);
		}

		return buttons;
	}

	static WORD ScaleMotor(float value) noexcept
	{
		return static_cast<WORD>(std::clamp(value, 0.0f, 1.0f) * 65535.0f);
	}

	void DecodeXInput(const XINPUT_GAMEPAD& gamepad, bool hasGuide, RawSample& raw, CanonicalSample& canonical) noexcept
	{
		raw.sticks[static_cast<std::size_t>(Stick::Left)] = { gamepad.sThumbLX, gamepad.sThumbLY };
		raw.sticks[static_cast<std::size_t>(Stick::Right)] = { gamepad.sThumbRX, gamepad.sThumbRY };
		raw.triggers[static_cast<std::size_t>(TriggerSide::Left)] = gamepad.bLeftTrigger;
		raw.triggers[static_cast<std::size_t>(TriggerSide::Right)] = gamepad.bRightTrigger;
		raw.buttons = gamepad.wButtons;

		ButtonSet buttons = DecodeButtons(gamepad.wButtons, hasGuide);

		buttons.Set(Button::L2, gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		buttons.Set(Button::R2, gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

		canonical.buttons = buttons;

		auto& left = canonical.sticks[static_cast<std::size_t>(Stick::Left)];
		auto& right = canonical.sticks[static_cast<std::size_t>(Stick::Right)];
		left = {};
		right = {};
		left.raw = { gamepad.sThumbLX, gamepad.sThumbLY };
		left.normalized = NormalizeStick(gamepad.sThumbLX, gamepad.sThumbLY);
		right.raw = { gamepad.sThumbRX, gamepad.sThumbRY };
		right.normalized = NormalizeStick(gamepad.sThumbRX, gamepad.sThumbRY);

		canonical.triggers[static_cast<std::size_t>(TriggerSide::Left)] = DecodeTrigger(gamepad.bLeftTrigger);
		canonical.triggers[static_cast<std::size_t>(TriggerSide::Right)] = DecodeTrigger(gamepad.bRightTrigger);

		canonical.touch.reset();
		canonical.motion.reset();
		canonical.battery.reset();

		canonical.caps = Capability::Rumble;
	}

	XInputDriver::XInputDriver(const Context& context, const Transport::XInputModule& xinput, DeviceId device, UserIndex index)
		: context(context),
		xinput(xinput),
		device(device),
		index(index)
	{
	}

	bool XInputDriver::TryPoll(RawSample& raw, CanonicalSample& canonical)
	{
		XINPUT_STATE state{};

		if (this->xinput.GetState(this->index.Value(), state) != ERROR_SUCCESS)
		{
			this->hasPacket = false;
			return false;
		}

		if (this->hasPacket && state.dwPacketNumber == this->lastPacket)
		{
			return false;
		}

		this->hasPacket = true;
		this->lastPacket = state.dwPacketNumber;

		DecodeXInput(state.Gamepad, this->xinput.HasGuideButton(), raw, canonical);
		return true;
	}

	void XInputDriver::Submit(const OutputRequest& request)
	{
		if (const auto* rumble = std::get_if<RumbleRequest>(&request))
		{
			XINPUT_VIBRATION vibration{};
			vibration.wLeftMotorSpeed = ScaleMotor(rumble->lowFrequency);
			vibration.wRightMotorSpeed = ScaleMotor(rumble->highFrequency);

			if (this->xinput.SetState(this->index.Value(), vibration) != ERROR_SUCCESS)
			{
				this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::OutputRejected, this->device, "XInput rumble output failed");
			}

			return;
		}

		if (this->hasReportedUnsupported)
		{
			return;
		}

		this->hasReportedUnsupported = true;

		this->context.Report(Severity::Info, Facility::Driver, ErrorCode::OutputRejected, this->device, "XInput driver ignores a non-rumble output request");
	}

	std::string XInputDriver::Diagnostics() const
	{
		return "haptics: unavailable, because an XInput controller exposes nothing but its two rumble motors";
	}
}
