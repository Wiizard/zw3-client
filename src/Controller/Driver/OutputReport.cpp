#include "STDInclude.hpp"

#include "Controller/Driver/OutputReport.hpp"
#include "Controller/Driver/Decode.hpp"

namespace Controller::Driver
{
	static constexpr std::uint8_t ds4Flag0Motor = 0x01;
	static constexpr std::uint8_t ds4Flag0Led = 0x02;
	static constexpr std::uint8_t ds4HardwareControlCrc32 = 0x40;
	static constexpr std::uint8_t ds4HardwareControlHid = 0x80;

	static constexpr std::uint8_t dsFlag0CompatibleVibration = 0x01;
	static constexpr std::uint8_t dsFlag0HapticsSelect = 0x02;
	static constexpr std::uint8_t dsFlag1LightbarEnable = 0x04;

	static constexpr std::uint8_t dsFlag0RightTriggerEffect = 0x04;
	static constexpr std::uint8_t dsFlag0LeftTriggerEffect = 0x08;

	static constexpr std::size_t dsRightTriggerEffectOffset = 10;
	static constexpr std::size_t dsLeftTriggerEffectOffset = 21;
	static constexpr std::size_t dsTriggerEffectSize = 11;

	static constexpr std::uint8_t dsTriggerOff = 0x00;
	static constexpr std::uint8_t dsTriggerFeedback = 0x21;
	static constexpr std::uint8_t dsTriggerWeapon = 0x22;

	static constexpr std::uint8_t dsTriggerMaxStrength = 8;
	static constexpr std::uint8_t dsTriggerMinWeaponStart = 2;
	static constexpr std::uint8_t dsTriggerMaxWeaponStart = 7;
	static constexpr std::uint8_t dsTriggerMaxWeaponEnd = 8;

	static constexpr std::uint8_t dsOutputTag = 0x10;

	static constexpr std::uint8_t dsHapticReportId = 0x32;
	static constexpr std::uint8_t dsPacketSized = 0x80;

	static constexpr std::uint8_t dsHapticControlId = 0x11;
	static constexpr std::uint8_t dsHapticControlLength = 7;

	static constexpr std::uint8_t dsHapticControl[dsHapticControlLength] = { 0xFE, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00 };

	static constexpr std::uint8_t dsHapticSamplesId = 0x12;
	static constexpr std::uint8_t dsHapticSamplesLength = static_cast<std::uint8_t>(hapticFramesPerReport * 2);

	static constexpr std::size_t dsHapticControlOffset = 2;
	static constexpr std::size_t dsHapticControlData = dsHapticControlOffset + 2;
	static constexpr std::size_t dsHapticCounter = dsHapticControlData + dsHapticControlLength - 1;
	static constexpr std::size_t dsHapticSamplesOffset = dsHapticControlData + dsHapticControlLength;
	static constexpr std::size_t dsHapticSamplesData = dsHapticSamplesOffset + 2;
	static constexpr std::size_t dsHapticCrc = dsHapticReportSize - 4;

	static_assert(dsHapticSamplesData + dsHapticSamplesLength <= dsHapticCrc, "the packets and the checksum must fit inside one report");

	static constexpr std::uint8_t dsHapticSilence = 0x80;

	static std::uint8_t ToByte(float value) noexcept
	{
		return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
	}

	static void Put(std::span<std::byte> report, std::size_t index, std::uint8_t value) noexcept
	{
		assert(index < report.size());
		report[index] = static_cast<std::byte>(value);
	}

	static void SealBluetoothCrc(std::span<std::byte> report) noexcept
	{
		assert(report.size() >= 4);

		const auto seed = static_cast<std::byte>(psOutputCrcSeed);

		std::uint32_t crc = Crc32Le(0xFFFFFFFFu, std::span<const std::byte>(&seed, 1));
		crc = ~Crc32Le(crc, report.subspan(0, report.size() - 4));

		const std::size_t offset = report.size() - 4;
		Put(report, offset + 0, static_cast<std::uint8_t>(crc));
		Put(report, offset + 1, static_cast<std::uint8_t>(crc >> 8));
		Put(report, offset + 2, static_cast<std::uint8_t>(crc >> 16));
		Put(report, offset + 3, static_cast<std::uint8_t>(crc >> 24));
	}

	static std::uint8_t ClampTo(std::uint8_t value, std::uint8_t limit) noexcept
	{
		if (value > limit)
		{
			return limit;
		}

		return value;
	}

	static void PutZones(std::span<std::byte> effect, std::uint16_t active, std::uint32_t force) noexcept
	{
		Put(effect, 1, static_cast<std::uint8_t>(active));
		Put(effect, 2, static_cast<std::uint8_t>(active >> 8));
		Put(effect, 3, static_cast<std::uint8_t>(force));
		Put(effect, 4, static_cast<std::uint8_t>(force >> 8));
		Put(effect, 5, static_cast<std::uint8_t>(force >> 16));
		Put(effect, 6, static_cast<std::uint8_t>(force >> 24));
	}

	static void EncodeFeedbackZones(const TriggerProfile& zones, std::span<std::byte> effect) noexcept
	{
		std::uint16_t active = 0;
		std::uint32_t force = 0;

		for (std::size_t i = 0; i != triggerZoneCount; ++i)
		{
			const std::uint8_t strength = ClampTo(zones[i], dsTriggerMaxStrength);

			if (strength == 0)
			{
				continue;
			}

			active = static_cast<std::uint16_t>(active | (1u << i));
			force |= static_cast<std::uint32_t>((strength - 1) & 0x07) << (3 * i);
		}

		if (active == 0)
		{
			Put(effect, 0, dsTriggerOff);
			return;
		}

		Put(effect, 0, dsTriggerFeedback);
		PutZones(effect, active, force);
	}

	static bool TryEncodeTriggerEffect(const AdaptiveTriggerRequest& trigger, std::span<std::byte> effect) noexcept
	{
		assert(effect.size() >= dsTriggerEffectSize);

		std::fill_n(effect.data(), dsTriggerEffectSize, std::byte{});

		const std::uint8_t strength = ClampTo(trigger.strength, dsTriggerMaxStrength);

		switch (trigger.effect)
		{
		case TriggerEffect::Off:
			Put(effect, 0, dsTriggerOff);
			return true;

		case TriggerEffect::Feedback:
			EncodeFeedbackZones(trigger.zones, effect);
			return true;

		case TriggerEffect::Weapon:
		{
			if (strength == 0)
			{
				Put(effect, 0, dsTriggerOff);
				return true;
			}

			std::uint8_t start = trigger.startPosition;

			if (start < dsTriggerMinWeaponStart)
			{
				start = dsTriggerMinWeaponStart;
			}

			const std::uint8_t begin = ClampTo(start, dsTriggerMaxWeaponStart);

			std::uint8_t end = static_cast<std::uint8_t>(begin + 1);

			if (trigger.endPosition > begin)
			{
				end = trigger.endPosition;
			}

			end = ClampTo(end, dsTriggerMaxWeaponEnd);

			if (end <= begin)
			{
				return false;
			}

			const auto bounds = static_cast<std::uint16_t>((1u << begin) | (1u << end));

			Put(effect, 0, dsTriggerWeapon);
			Put(effect, 1, static_cast<std::uint8_t>(bounds));
			Put(effect, 2, static_cast<std::uint8_t>(bounds >> 8));
			Put(effect, 3, static_cast<std::uint8_t>(strength - 1));
			return true;
		}
		}

		return false;
	}

	static std::uint8_t ToSample(float value) noexcept
	{
		const float clamped = std::clamp(value, -1.0f, 1.0f);
		return static_cast<std::uint8_t>(std::lround(clamped * 127.0f) + static_cast<long>(dsHapticSilence));
	}

	std::optional<std::size_t> TryEncodeDualShock4Output(const OutputRequest& request, Connection link, std::span<std::byte> out) noexcept
	{
		const bool isBluetooth = link == Connection::Bluetooth;

		if (link != Connection::Usb && !isBluetooth)
		{
			return std::nullopt;
		}

		std::size_t size = ds4OutputUsbSize;
		std::size_t base = 1;

		if (isBluetooth)
		{
			size = ds4OutputBluetoothSize;
			base = 3;
		}

		if (out.size() < size)
		{
			return std::nullopt;
		}

		std::fill_n(out.data(), size, std::byte{});

		if (isBluetooth)
		{
			Put(out, 0, 0x11);
			Put(out, 1, ds4HardwareControlHid | ds4HardwareControlCrc32);
		}
		else
		{
			Put(out, 0, 0x05);
		}

		if (const auto* rumble = std::get_if<RumbleRequest>(&request))
		{
			Put(out, base + 0, ds4Flag0Motor);
			Put(out, base + 3, ToByte(rumble->highFrequency));
			Put(out, base + 4, ToByte(rumble->lowFrequency));
		}
		else if (const auto* lightBar = std::get_if<LightBarRequest>(&request))
		{
			Put(out, base + 0, ds4Flag0Led);
			Put(out, base + 5, lightBar->red);
			Put(out, base + 6, lightBar->green);
			Put(out, base + 7, lightBar->blue);
		}
		else
		{
			return std::nullopt;
		}

		if (isBluetooth)
		{
			SealBluetoothCrc(out.subspan(0, size));
		}

		return size;
	}

	std::optional<std::size_t> TryEncodeDualSenseOutput(const OutputRequest& request, Connection link, std::uint8_t& bluetoothSequence, std::span<std::byte> out) noexcept
	{
		const bool isBluetooth = link == Connection::Bluetooth;

		if (link != Connection::Usb && !isBluetooth)
		{
			return std::nullopt;
		}

		std::size_t size = dsOutputUsbSize;
		std::size_t base = 1;

		if (isBluetooth)
		{
			size = dsOutputBluetoothSize;
			base = 3;
		}

		if (out.size() < size)
		{
			return std::nullopt;
		}

		std::fill_n(out.data(), size, std::byte{});

		if (isBluetooth)
		{
			Put(out, 0, 0x31);

			Put(out, 1, static_cast<std::uint8_t>((bluetoothSequence & 0x0F) << 4));
			Put(out, 2, dsOutputTag);

			bluetoothSequence = static_cast<std::uint8_t>((bluetoothSequence + 1) & 0x0F);
		}
		else
		{
			Put(out, 0, 0x02);
		}

		if (const auto* rumble = std::get_if<RumbleRequest>(&request))
		{
			Put(out, base + 0, dsFlag0HapticsSelect | dsFlag0CompatibleVibration);
			Put(out, base + 2, ToByte(rumble->highFrequency));
			Put(out, base + 3, ToByte(rumble->lowFrequency));
		}
		else if (const auto* lightBar = std::get_if<LightBarRequest>(&request))
		{
			Put(out, base + 1, dsFlag1LightbarEnable);
			Put(out, base + 44, lightBar->red);
			Put(out, base + 45, lightBar->green);
			Put(out, base + 46, lightBar->blue);
		}
		else if (const auto* trigger = std::get_if<AdaptiveTriggerRequest>(&request))
		{
			const bool isRight = trigger->side == TriggerSide::Right;

			std::size_t effectOffset = base + dsLeftTriggerEffectOffset;
			std::uint8_t flag = dsFlag0LeftTriggerEffect;

			if (isRight)
			{
				effectOffset = base + dsRightTriggerEffectOffset;
				flag = dsFlag0RightTriggerEffect;
			}

			if (!TryEncodeTriggerEffect(*trigger, out.subspan(effectOffset, dsTriggerEffectSize)))
			{
				return std::nullopt;
			}

			Put(out, base + 0, flag);
		}
		else
		{
			return std::nullopt;
		}

		if (isBluetooth)
		{
			SealBluetoothCrc(out.subspan(0, size));
		}

		return size;
	}

	std::optional<std::size_t> TryEncodeDualSenseHaptics(std::span<const Haptic::Frame> frames, std::uint8_t& counter, std::span<std::byte> out) noexcept
	{
		if (frames.size() != hapticFramesPerReport || out.size() < dsHapticReportSize)
		{
			return std::nullopt;
		}

		std::fill_n(out.data(), dsHapticReportSize, std::byte{});

		Put(out, 0, dsHapticReportId);
		Put(out, 1, 0);

		Put(out, dsHapticControlOffset, dsHapticControlId | dsPacketSized);
		Put(out, dsHapticControlOffset + 1, dsHapticControlLength);

		for (std::size_t i = 0; i != dsHapticControlLength; ++i)
		{
			Put(out, dsHapticControlData + i, dsHapticControl[i]);
		}

		Put(out, dsHapticCounter, counter);
		++counter;

		Put(out, dsHapticSamplesOffset, dsHapticSamplesId | dsPacketSized);
		Put(out, dsHapticSamplesOffset + 1, dsHapticSamplesLength);

		for (std::size_t i = 0; i != frames.size(); ++i)
		{
			Put(out, dsHapticSamplesData + i * 2 + 0, ToSample(frames[i].left));
			Put(out, dsHapticSamplesData + i * 2 + 1, ToSample(frames[i].right));
		}

		SealBluetoothCrc(out.subspan(0, dsHapticReportSize));

		return dsHapticReportSize;
	}
}
