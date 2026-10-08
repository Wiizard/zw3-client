#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Driver/Driver.hpp"
#include "Controller/Transport/XInputModule.hpp"

namespace Controller::Driver
{
	void DecodeXInput(const XINPUT_GAMEPAD& gamepad, bool hasGuide, RawSample& raw, CanonicalSample& canonical) noexcept;

	class XInputDriver : public Driver
	{
	public:
		XInputDriver(const Context& context, const Transport::XInputModule& xinput, DeviceId device, UserIndex index);

		Controller::Family Family() const noexcept override
		{
			return Controller::Family::Xbox;
		}

		DeviceId Device() const noexcept override
		{
			return this->device;
		}

		bool TryPoll(RawSample& raw, CanonicalSample& canonical) override;
		void Submit(const OutputRequest& request) override;
		std::string Diagnostics() const override;

	private:
		const Context& context;
		const Transport::XInputModule& xinput;
		DeviceId device;
		UserIndex index;

		DWORD lastPacket = 0;
		bool hasPacket = false;

		bool hasReportedUnsupported = false;
	};
}
