#include "STDInclude.hpp"

#include "Controller/Driver/DualSense.hpp"
#include "Controller/Driver/Decode.hpp"
#include "Controller/Driver/OutputReport.hpp"
#include "Controller/Driver/PlayStation.hpp"

namespace Controller::Driver
{
	static constexpr std::uint8_t dsReportUsb = 0x01;
	static constexpr std::uint8_t dsReportBluetooth = 0x31;
	static constexpr std::size_t dsSizeUsb = 64;
	static constexpr std::size_t dsSizeBluetooth = 78;
	static constexpr std::size_t dsCommonUsb = 1;
	static constexpr std::size_t dsCommonBluetooth = 2;

	static bool TryResolveFrame(std::span<const std::byte> report, Connection link, std::size_t& common) noexcept
	{
		if (link == Connection::Usb)
		{
			if (report.size() < dsSizeUsb || ReadU8(report, 0) != dsReportUsb)
			{
				return false;
			}

			common = dsCommonUsb;
			return true;
		}

		if (link == Connection::Bluetooth)
		{
			if (report.size() < dsSizeBluetooth || ReadU8(report, 0) != dsReportBluetooth)
			{
				return false;
			}

			const std::uint32_t crc = ReadLe32(report, dsSizeBluetooth - 4);

			if (!IsPsCrc32Valid(psInputCrcSeed, report.first(dsSizeBluetooth - 4), crc))
			{
				return false;
			}

			common = dsCommonBluetooth;
			return true;
		}

		return false;
	}

	static TouchPoint DecodePoint(std::span<const std::byte> report, std::size_t offset) noexcept
	{
		const auto point = DecodeTouchPoint(report.subspan(offset, 4));
		return { point.isActive, point.id, point.x, point.y };
	}

	bool TryDecodeDualSense(std::span<const std::byte> report, Connection link, RawSample& raw, CanonicalSample& canonical, bool isEdge)
	{
		std::size_t base = 0;

		if (!TryResolveFrame(report, link, base))
		{
			return false;
		}

		const std::uint8_t leftX = ReadU8(report, base + 0);
		const std::uint8_t leftY = ReadU8(report, base + 1);
		const std::uint8_t rightX = ReadU8(report, base + 2);
		const std::uint8_t rightY = ReadU8(report, base + 3);

		raw.sticks[static_cast<std::size_t>(Stick::Left)] = { leftX, leftY };
		raw.sticks[static_cast<std::size_t>(Stick::Right)] = { rightX, rightY };

		auto& left = canonical.sticks[static_cast<std::size_t>(Stick::Left)];
		auto& right = canonical.sticks[static_cast<std::size_t>(Stick::Right)];
		left = {};
		right = {};
		left.raw = { leftX, leftY };
		left.normalized = NormalizePsStick(leftX, leftY);
		right.raw = { rightX, rightY };
		right.normalized = NormalizePsStick(rightX, rightY);

		const std::uint8_t leftTrigger = ReadU8(report, base + 4);
		const std::uint8_t rightTrigger = ReadU8(report, base + 5);
		raw.triggers[static_cast<std::size_t>(TriggerSide::Left)] = leftTrigger;
		raw.triggers[static_cast<std::size_t>(TriggerSide::Right)] = rightTrigger;
		canonical.triggers[static_cast<std::size_t>(TriggerSide::Left)] = { leftTrigger, static_cast<float>(leftTrigger) / 255.0f };
		canonical.triggers[static_cast<std::size_t>(TriggerSide::Right)] = { rightTrigger, static_cast<float>(rightTrigger) / 255.0f };

		const std::uint8_t buttons0 = ReadU8(report, base + 7);
		const std::uint8_t buttons1 = ReadU8(report, base + 8);
		const std::uint8_t buttons2 = ReadU8(report, base + 9);
		const std::uint8_t buttons3 = ReadU8(report, base + 10);

		ButtonSet buttons;
		buttons.Set(Button::FaceWest, (buttons0 & 0x10u) != 0);
		buttons.Set(Button::FaceSouth, (buttons0 & 0x20u) != 0);
		buttons.Set(Button::FaceEast, (buttons0 & 0x40u) != 0);
		buttons.Set(Button::FaceNorth, (buttons0 & 0x80u) != 0);
		ApplyHat(buttons, static_cast<std::uint8_t>(buttons0 & 0x0Fu));

		buttons.Set(Button::L1, (buttons1 & 0x01u) != 0);
		buttons.Set(Button::R1, (buttons1 & 0x02u) != 0);
		buttons.Set(Button::L2, (buttons1 & 0x04u) != 0);
		buttons.Set(Button::R2, (buttons1 & 0x08u) != 0);
		buttons.Set(Button::Back, (buttons1 & 0x10u) != 0);
		buttons.Set(Button::Start, (buttons1 & 0x20u) != 0);
		buttons.Set(Button::L3, (buttons1 & 0x40u) != 0);
		buttons.Set(Button::R3, (buttons1 & 0x80u) != 0);

		buttons.Set(Button::Guide, (buttons2 & 0x01u) != 0);
		buttons.Set(Button::Touchpad, (buttons2 & 0x02u) != 0);
		buttons.Set(Button::Mute, (buttons2 & 0x04u) != 0);

		if (isEdge)
		{
			buttons.Set(Button::EdgeFnLeft, (buttons2 & 0x10u) != 0);
			buttons.Set(Button::EdgeFnRight, (buttons2 & 0x20u) != 0);
			buttons.Set(Button::EdgePaddleLeft, (buttons2 & 0x40u) != 0);
			buttons.Set(Button::EdgePaddleRight, (buttons2 & 0x80u) != 0);
		}

		canonical.buttons = buttons;
		raw.buttons = static_cast<std::uint32_t>(buttons0)
			| (static_cast<std::uint32_t>(buttons1) << 8)
			| (static_cast<std::uint32_t>(buttons2) << 16)
			| (static_cast<std::uint32_t>(buttons3) << 24);

		MotionSample motion;
		motion.gyro.angularVelocity = { static_cast<float>(ReadLe16Signed(report, base + 15)), static_cast<float>(ReadLe16Signed(report, base + 17)), static_cast<float>(ReadLe16Signed(report, base + 19)) };
		motion.accel.acceleration = { static_cast<float>(ReadLe16Signed(report, base + 21)), static_cast<float>(ReadLe16Signed(report, base + 23)), static_cast<float>(ReadLe16Signed(report, base + 25)) };
		motion.deviceTimestamp = ReadLe32(report, base + 27);
		raw.motion = motion;

		Touchpad touchpad;
		touchpad.points[0] = DecodePoint(report, base + 32);
		touchpad.points[1] = DecodePoint(report, base + 36);
		canonical.touch = touchpad;
		raw.touch = touchpad;

		const std::uint8_t status0 = ReadU8(report, base + 52);
		const auto capacity = static_cast<std::uint8_t>(status0 & 0x0Fu);
		const auto charging = static_cast<std::uint8_t>((status0 >> 4) & 0x0Fu);

		BatteryState battery;

		switch (charging)
		{
		case 0x0:
			battery.state = BatteryState::Status::Discharging;
			battery.percent = static_cast<std::uint8_t>(std::min(capacity * 10 + 5, 100));
			break;

		case 0x1:
			battery.state = BatteryState::Status::Charging;
			battery.percent = static_cast<std::uint8_t>(std::min(capacity * 10 + 5, 100));
			break;

		case 0x2:
			battery.state = BatteryState::Status::Full;
			battery.percent = std::uint8_t{ 100 };
			break;

		default:
			battery.state = BatteryState::Status::Unknown;
			break;
		}

		canonical.battery = battery;
		raw.battery = battery;

		canonical.motion.reset();

		Capabilities caps = Capability::Gyroscope | Capability::Accelerometer;
		caps.Add(Capability::Touchpad).Add(Capability::Battery).Add(Capability::Rumble).Add(Capability::AdaptiveTriggers);
		caps.Add(Capability::LightBar).Add(Capability::PlayerLeds).Add(Capability::MicrophoneButton);

		if (link == Connection::Usb || link == Connection::Bluetooth)
		{
			caps.Add(Capability::Haptics);
		}

		if (isEdge)
		{
			caps.Add(Capability::BackButtons);
		}

		canonical.caps = caps;
		return true;
	}

	DualSenseDriver::DualSenseDriver(const Context& context, Transport::HidDevice& hid, DeviceId device)
		: context(context),
		hid(hid),
		device(device),
		link(hid.Link())
	{
	}

	bool DualSenseDriver::TryReadAndDecode(RawSample& raw, CanonicalSample& canonical, bool isEdge)
	{
		std::array<std::byte, dsSizeBluetooth> buffer;
		bool isDecoded = false;

		for (std::size_t i = 0; i != maxReportsPerPoll; ++i)
		{
			const auto length = this->hid.TryRead(buffer);

			if (!length || *length == 0)
			{
				break;
			}

			const std::span<const std::byte> report(buffer.data(), *length);

			if (TryDecodeDualSense(report, this->link, raw, canonical, isEdge))
			{
				isDecoded = true;
				continue;
			}

			if (IsMinimalBluetoothReport(report, this->link))
			{
				if (!this->hasReportedMinimal)
				{
					this->hasReportedMinimal = true;

					this->context.Report(Severity::Info, Facility::Decode, ErrorCode::None, this->device,
						"DualSense is still sending minimal Bluetooth reports and produces no input; the controller did not switch to its extended report");
				}

				continue;
			}

			this->context.Report(Severity::Warning, Facility::Decode, ErrorCode::ReportMalformed, this->device, "DualSense report failed framing or CRC validation and was dropped");
		}

		return isDecoded;
	}

	bool DualSenseDriver::TryPoll(RawSample& raw, CanonicalSample& canonical)
	{
		this->FlushRumble();

		return this->TryReadAndDecode(raw, canonical, false);
	}

	void DualSenseDriver::QueueRumble(const RumbleRequest& request)
	{
		this->pendingRumble = request;
		this->isRumblePending = true;

		this->FlushRumble();
	}

	void DualSenseDriver::FlushRumble()
	{
		if (!this->isRumblePending)
		{
			return;
		}

		const Timestamp now = Clock::Now();

		const bool isThrottled = this->policy.outputIntervalMs != 0 && this->hasSentRumble
			&& now - this->lastRumble < std::chrono::milliseconds(this->policy.outputIntervalMs);

		if (isThrottled)
		{
			return;
		}

		this->isRumblePending = false;
		this->hasSentRumble = true;
		this->lastRumble = now;

		this->SubmitReport(this->pendingRumble);
	}

	bool DualSenseDriver::TryStartHaptics()
	{
		if (this->haveHapticsFailed || (this->link != Connection::Usb && this->link != Connection::Bluetooth))
		{
			return false;
		}

		if (this->haptics == nullptr)
		{
			try
			{
				this->haptics = std::make_unique<Haptic::Stream>(this->context, this->device, this->link, this->hid);
			}
			catch (const std::exception& exception)
			{
				this->haveHapticsFailed = true;

				this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::TransportFailure, this->device,
					std::format("controller haptics could not be started: {}", exception.what()));
				return false;
			}
		}

		return this->haptics->IsRunning();
	}

	bool DualSenseDriver::TryPlayWaveform(const RumbleRequest& request)
	{
		if (!this->TryStartHaptics())
		{
			return false;
		}

		this->haptics->SetRumble(request.lowFrequency, request.highFrequency);
		return true;
	}

	bool DualSenseDriver::TryPlayEffect(const Haptic::Effect& effect)
	{
		return this->TryStartHaptics() && this->haptics->TryPlay(effect);
	}

	void DualSenseDriver::Submit(const OutputRequest& request)
	{
		if (const auto* effect = std::get_if<Haptic::Effect>(&request))
		{
			if (this->policy.isRumbleEnabled && this->TryPlayEffect(*effect))
			{
				this->hasReportedEffectDrop = false;
				return;
			}

			if (!this->hasReportedEffectDrop)
			{
				this->hasReportedEffectDrop = true;

				this->context.Report(Severity::Info, Facility::Driver, ErrorCode::OutputRejected, this->device, "DualSense dropped a haptic effect: its actuators are not being driven");
			}

			return;
		}

		if (const auto* rumble = std::get_if<RumbleRequest>(&request))
		{
			if (this->policy.hapticMode == HapticMode::Waveform)
			{
				if (this->isEmulating)
				{
					this->isEmulating = false;
					this->QueueRumble(RumbleRequest{});
				}

				if (this->TryPlayWaveform(*rumble))
				{
					this->hasReportedFallback = false;
					return;
				}

				if (!this->hasReportedFallback)
				{
					this->hasReportedFallback = true;

					this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::OutputRejected, this->device,
						"controller haptics are enabled but the actuators are not being driven, so rumble is going through the firmware's motor emulation instead; controller_status says why");
				}
			}

			this->isEmulating = rumble->lowFrequency > 0.0f || rumble->highFrequency > 0.0f;

			this->QueueRumble(*rumble);
			return;
		}

		this->SubmitReport(request);
	}

	void DualSenseDriver::Configure(const OutputPolicy& outputPolicy)
	{
		this->policy = outputPolicy;

		const bool isWanted = outputPolicy.isRumbleEnabled && outputPolicy.hapticMode == HapticMode::Waveform;

		if (isWanted)
		{
			this->TryStartHaptics();
		}

		if (!isWanted && this->haptics != nullptr)
		{
			this->haptics.reset();
			this->hasReportedFallback = false;
			this->hasReportedEffectDrop = false;
		}

		if (this->haptics != nullptr)
		{
			this->haptics->PollDiagnostics();
		}
	}

	void DualSenseDriver::StopHaptic(std::uint32_t tag)
	{
		if (this->haptics != nullptr)
		{
			this->haptics->Stop(tag);
		}
	}

	std::string DualSenseDriver::Diagnostics() const
	{
		if (this->haptics != nullptr)
		{
			return "haptics: " + this->haptics->Status();
		}

		if (this->link != Connection::Usb && this->link != Connection::Bluetooth)
		{
			return "haptics: unavailable, because this link's framing is unknown";
		}

		if (!this->policy.isRumbleEnabled)
		{
			return "haptics: off, because rumble is off (gpad_rumble)";
		}

		if (this->policy.hapticMode != HapticMode::Waveform)
		{
			return "haptics: off, playing rumble through the motor emulation (gpad_haptics)";
		}

		return "haptics: not started (no rumble has been asked for yet)";
	}

	void DualSenseDriver::SubmitReport(const OutputRequest& request)
	{
		std::array<std::byte, dsOutputBluetoothSize> buffer;

		const auto length = TryEncodeDualSenseOutput(request, this->link, this->bluetoothOutputSequence, buffer);

		if (!length)
		{
			this->context.Report(Severity::Info, Facility::Driver, ErrorCode::OutputRejected, this->device, "DualSense driver dropped an output request it cannot encode for this link");
			return;
		}

		if (!this->hid.TryWrite(std::span<const std::byte>(buffer.data(), *length)))
		{
			this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::OutputRejected, this->device, "DualSense output report write failed");
		}
	}
}
