#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	enum class ErrorCode : std::uint8_t
	{
		None,

		DeviceUnavailable,
		AmbiguousIdentity,
		UnsupportedDevice,

		TransportFailure,

		ReportMalformed,
		ReportTruncated,
		ChecksumMismatch,

		OutputRejected,

		CalibrationInvalid,
		CalibrationVersion,

		GraphInvalid,

		BindingInvalid,

		SteamUnavailable,
		SteamUnsuitable,

		HookFailed,
		DvarRegistration,
	};

	const char* ToString(ErrorCode code) noexcept;
}
