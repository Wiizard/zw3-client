#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Driver/Driver.hpp"
#include "Controller/Transport/Hid.hpp"

namespace Controller::Driver
{
	bool TryDecodeDualShock4(std::span<const std::byte> report, Connection link, RawSample& raw, CanonicalSample& canonical);

	class DualShock4Driver : public Driver
	{
	public:
		DualShock4Driver(const Context& context, Transport::HidDevice& hid, DeviceId device);

		Controller::Family Family() const noexcept override
		{
			return Controller::Family::DualShock4;
		}

		DeviceId Device() const noexcept override
		{
			return this->device;
		}

		bool TryPoll(RawSample& raw, CanonicalSample& canonical) override;
		void Submit(const OutputRequest& request) override;
		void Configure(const OutputPolicy& outputPolicy) override;
		std::string Diagnostics() const override;

	private:
		void SubmitReport(const OutputRequest& request);
		void QueueRumble(const RumbleRequest& request);
		void FlushRumble();

		const Context& context;
		Transport::HidDevice& hid;
		DeviceId device;
		Connection link;

		bool hasReportedMinimal = false;
		bool hasReportedUnencodable = false;

		OutputPolicy policy{};

		RumbleRequest pendingRumble{};
		bool isRumblePending = false;
		bool hasSentRumble = false;
		Timestamp lastRumble{};
	};
}
