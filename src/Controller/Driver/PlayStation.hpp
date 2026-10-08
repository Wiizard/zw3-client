#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Transport/Hid.hpp"

namespace Controller::Driver
{
	bool TryEnableExtendedReports(const Context& context, Transport::HidDevice& hid, DeviceId device);

	bool IsMinimalBluetoothReport(std::span<const std::byte> report, Connection link) noexcept;
}
