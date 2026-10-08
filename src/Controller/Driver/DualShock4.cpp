#include "STDInclude.hpp"

#include "Controller/Driver/DualShock4.hpp"
#include "Controller/Driver/Decode.hpp"
#include "Controller/Driver/OutputReport.hpp"
#include "Controller/Driver/PlayStation.hpp"

namespace Controller::Driver
{
	static constexpr std::uint8_t ds4ReportUsb = 0x01;
	static constexpr std::uint8_t ds4ReportBluetooth = 0x11;
	static constexpr std::size_t ds4SizeUsb = 64;
	static constexpr std::size_t ds4SizeBluetooth = 78;
	static constexpr std::size_t ds4CommonUsb = 1;
	static constexpr std::size_t ds4CommonBluetooth = 3;
	static constexpr std::size_t ds4MaxTouchUsb = 3;
	static constexpr std::size_t ds4MaxTouchBluetooth = 4;
	static constexpr std::uint8_t ds4BatteryFull = 11;

	struct Ds4Frame
	{
		std::size_t common;
		std::size_t maxTouch;
	};

	static std::optional<Ds4Frame> TryResolveDs4Frame(std::span<const std::byte> report, Connection link) noexcept
	{
		if (link == Connection::Usb)
		{
			if (report.size() < ds4SizeUsb || ReadU8(report, 0) != ds4ReportUsb)
			{
				return std::nullopt;
			}

			return Ds4Frame{ ds4CommonUsb, ds4MaxTouchUsb };
		}

		if (link == Connection::Bluetooth)
		{
			if (report.size() < ds4SizeBluetooth || ReadU8(report, 0) != ds4ReportBluetooth)
			{
				return std::nullopt;
			}

			const std::uint32_t crc = ReadLe32(report, ds4SizeBluetooth - 4);

			if (!IsPsCrc32Valid(psInputCrcSeed, report.first(ds4SizeBluetooth - 4), crc))
			{
				return std::nullopt;
			}

			return Ds4Frame{ ds4CommonBluetooth, ds4MaxTouchBluetooth };
		}

		return std::nullopt;
	}

	static TouchPoint DecodeDs4Point(std::span<const std::byte> report, std::size_t offset) noexcept
	{
		const auto point = DecodeTouchPoint(report.subspan(offset, 4));
		return { point.isActive, point.id, point.x, point.y };
	}

	bool TryDecodeDualShock4(std::span<const std::byte> report, Connection link, RawSample& raw, CanonicalSample& canonical)
	{
		const auto frame = TryResolveDs4Frame(report, link);

		if (!frame)
		{
			return false;
		}

		const std::size_t base = frame->common;

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

		const std::uint8_t buttons0 = ReadU8(report, base + 4);
		const std::uint8_t buttons1 = ReadU8(report, base + 5);
		const std::uint8_t buttons2 = ReadU8(report, base + 6);

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

		canonical.buttons = buttons;
		raw.buttons = static_cast<std::uint32_t>(buttons0)
			| (static_cast<std::uint32_t>(buttons1) << 8)
			| (static_cast<std::uint32_t>(buttons2) << 16);

		const std::uint8_t leftTrigger = ReadU8(report, base + 7);
		const std::uint8_t rightTrigger = ReadU8(report, base + 8);
		raw.triggers[static_cast<std::size_t>(TriggerSide::Left)] = leftTrigger;
		raw.triggers[static_cast<std::size_t>(TriggerSide::Right)] = rightTrigger;
		canonical.triggers[static_cast<std::size_t>(TriggerSide::Left)] = { leftTrigger, static_cast<float>(leftTrigger) / 255.0f };
		canonical.triggers[static_cast<std::size_t>(TriggerSide::Right)] = { rightTrigger, static_cast<float>(rightTrigger) / 255.0f };

		MotionSample motion;
		motion.gyro.angularVelocity = { static_cast<float>(ReadLe16Signed(report, base + 12)), static_cast<float>(ReadLe16Signed(report, base + 14)), static_cast<float>(ReadLe16Signed(report, base + 16)) };
		motion.accel.acceleration = { static_cast<float>(ReadLe16Signed(report, base + 18)), static_cast<float>(ReadLe16Signed(report, base + 20)), static_cast<float>(ReadLe16Signed(report, base + 22)) };
		motion.deviceTimestamp = ReadLe16(report, base + 9);
		raw.motion = motion;

		const std::uint8_t status0 = ReadU8(report, base + 29);
		const auto capacity = static_cast<std::uint8_t>(status0 & 0x0Fu);
		const bool isCabled = (status0 & 0x10u) != 0;

		BatteryState battery;

		if (isCabled && capacity >= ds4BatteryFull)
		{
			battery.state = BatteryState::Status::Full;
			battery.percent = std::uint8_t{ 100 };
		}
		else
		{
			battery.state = BatteryState::Status::Discharging;

			if (isCabled)
			{
				battery.state = BatteryState::Status::Charging;
			}

			battery.percent = static_cast<std::uint8_t>(std::min<int>(capacity * 10, 100));
		}

		canonical.battery = battery;
		raw.battery = battery;

		const std::uint8_t touchCount = ReadU8(report, base + 32);

		if (touchCount >= 1 && touchCount <= frame->maxTouch)
		{
			Touchpad touchpad;
			touchpad.points[0] = DecodeDs4Point(report, base + 34);
			touchpad.points[1] = DecodeDs4Point(report, base + 38);
			canonical.touch = touchpad;
			raw.touch = touchpad;
		}
		else
		{
			canonical.touch.reset();
		}

		canonical.motion.reset();

		Capabilities caps = Capability::Gyroscope | Capability::Accelerometer;
		caps.Add(Capability::Touchpad).Add(Capability::Battery).Add(Capability::Rumble).Add(Capability::LightBar);
		canonical.caps = caps;

		return true;
	}

	DualShock4Driver::DualShock4Driver(const Context& context, Transport::HidDevice& hid, DeviceId device)
		: context(context),
		hid(hid),
		device(device),
		link(hid.Link())
	{
	}

	void DualShock4Driver::Configure(const OutputPolicy& outputPolicy)
	{
		this->policy = outputPolicy;
	}

	void DualShock4Driver::QueueRumble(const RumbleRequest& request)
	{
		this->pendingRumble = request;
		this->isRumblePending = true;

		this->FlushRumble();
	}

	void DualShock4Driver::FlushRumble()
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

	bool DualShock4Driver::TryPoll(RawSample& raw, CanonicalSample& canonical)
	{
		this->FlushRumble();

		std::array<std::byte, ds4SizeBluetooth> buffer;
		bool isDecoded = false;

		for (std::size_t i = 0; i != maxReportsPerPoll; ++i)
		{
			const auto length = this->hid.TryRead(buffer);

			if (!length || *length == 0)
			{
				break;
			}

			const std::span<const std::byte> report(buffer.data(), *length);

			if (TryDecodeDualShock4(report, this->link, raw, canonical))
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
						"DualShock 4 is still sending minimal Bluetooth reports and produces no input; the controller did not switch to its extended report");
				}

				continue;
			}

			this->context.Report(Severity::Warning, Facility::Decode, ErrorCode::ReportMalformed, this->device, "DualShock 4 report failed framing or CRC validation and was dropped");
		}

		return isDecoded;
	}

	void DualShock4Driver::Submit(const OutputRequest& request)
	{
		if (const auto* rumble = std::get_if<RumbleRequest>(&request))
		{
			this->QueueRumble(*rumble);
			return;
		}

		this->SubmitReport(request);
	}

	std::string DualShock4Driver::Diagnostics() const
	{
		return "haptics: unavailable, because a DualShock 4 exposes nothing but its two rumble motors";
	}

	void DualShock4Driver::SubmitReport(const OutputRequest& request)
	{
		std::array<std::byte, ds4OutputBluetoothSize> buffer;

		const auto length = TryEncodeDualShock4Output(request, this->link, buffer);

		if (!length)
		{
			if (!this->hasReportedUnencodable)
			{
				this->hasReportedUnencodable = true;

				this->context.Report(Severity::Info, Facility::Driver, ErrorCode::OutputRejected, this->device, "DualShock 4 driver ignores an output request it cannot encode for this connection");
			}

			return;
		}

		if (!this->hid.TryWrite(std::span<const std::byte>(buffer.data(), *length)))
		{
			this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::OutputRejected, this->device, "DualShock 4 output report write failed");
		}
	}
}
