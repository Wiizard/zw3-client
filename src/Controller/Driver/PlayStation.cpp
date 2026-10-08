#include "STDInclude.hpp"

#include "Controller/Driver/PlayStation.hpp"
#include "Controller/Driver/Decode.hpp"

namespace Controller::Driver
{
	static constexpr std::uint8_t psFeatureCalibration = 0x05;
	static constexpr std::size_t psFeatureCalibrationSize = 41;

	static constexpr std::uint8_t psReportBluetoothMinimal = 0x01;

	static constexpr std::size_t maxFeatureSize = 128;

	bool TryEnableExtendedReports(const Context& context, Transport::HidDevice& hid, DeviceId device)
	{
		const std::size_t length = std::clamp(hid.FeatureReportLength(), psFeatureCalibrationSize, maxFeatureSize);

		std::array<std::byte, maxFeatureSize> buffer{};
		buffer[0] = static_cast<std::byte>(psFeatureCalibration);

		if (!hid.TryGetFeature(std::span<std::byte>(buffer.data(), length)))
		{
			context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, device,
				"unable to read the calibration feature report over Bluetooth; the controller may keep sending minimal reports and produce no input");
			return false;
		}

		return true;
	}

	bool IsMinimalBluetoothReport(std::span<const std::byte> report, Connection link) noexcept
	{
		return link == Connection::Bluetooth && !report.empty() && ReadU8(report, 0) == psReportBluetoothMinimal;
	}
}
