#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Driver/Driver.hpp"
#include "Controller/Haptic/Stream.hpp"
#include "Controller/Transport/Hid.hpp"

namespace Controller::Driver
{
	bool TryDecodeDualSense(std::span<const std::byte> report, Connection link, RawSample& raw, CanonicalSample& canonical, bool isEdge);

	class DualSenseDriver : public Driver
	{
	public:
		DualSenseDriver(const Context& context, Transport::HidDevice& hid, DeviceId device);

		Controller::Family Family() const noexcept override
		{
			return Controller::Family::DualSense;
		}

		DeviceId Device() const noexcept override
		{
			return this->device;
		}

		bool TryPoll(RawSample& raw, CanonicalSample& canonical) override;
		void Submit(const OutputRequest& request) override;
		void Configure(const OutputPolicy& outputPolicy) override;
		void StopHaptic(std::uint32_t tag) override;
		std::string Diagnostics() const override;

	protected:
		void SubmitReport(const OutputRequest& request);
		void QueueRumble(const RumbleRequest& request);
		void FlushRumble();

		bool TryStartHaptics();
		bool TryPlayWaveform(const RumbleRequest& request);
		bool TryPlayEffect(const Haptic::Effect& effect);

		bool TryReadAndDecode(RawSample& raw, CanonicalSample& canonical, bool isEdge);

		const Context& context;
		Transport::HidDevice& hid;
		DeviceId device;
		Connection link;

		std::uint8_t bluetoothOutputSequence = 0;

		bool hasReportedMinimal = false;

		OutputPolicy policy{};

		RumbleRequest pendingRumble{};
		bool isRumblePending = false;
		bool hasSentRumble = false;
		Timestamp lastRumble{};

		std::unique_ptr<Haptic::Stream> haptics;

		bool haveHapticsFailed = false;

		bool hasReportedFallback = false;
		bool hasReportedEffectDrop = false;

		bool isEmulating = false;
	};
}
