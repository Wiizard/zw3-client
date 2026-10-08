#include "STDInclude.hpp"

#include "Controller/Error.hpp"

namespace Controller
{
	const char* ToString(ErrorCode code) noexcept
	{
		switch (code)
		{
		case ErrorCode::None:
			return "none";
		case ErrorCode::DeviceUnavailable:
			return "device-unavailable";
		case ErrorCode::AmbiguousIdentity:
			return "ambiguous-identity";
		case ErrorCode::UnsupportedDevice:
			return "unsupported-device";
		case ErrorCode::TransportFailure:
			return "transport-failure";
		case ErrorCode::ReportMalformed:
			return "report-malformed";
		case ErrorCode::ReportTruncated:
			return "report-truncated";
		case ErrorCode::ChecksumMismatch:
			return "checksum-mismatch";
		case ErrorCode::OutputRejected:
			return "output-rejected";
		case ErrorCode::CalibrationInvalid:
			return "calibration-invalid";
		case ErrorCode::CalibrationVersion:
			return "calibration-version";
		case ErrorCode::GraphInvalid:
			return "graph-invalid";
		case ErrorCode::BindingInvalid:
			return "binding-invalid";
		case ErrorCode::SteamUnavailable:
			return "steam-unavailable";
		case ErrorCode::SteamUnsuitable:
			return "steam-unsuitable";
		case ErrorCode::HookFailed:
			return "hook-failed";
		case ErrorCode::DvarRegistration:
			return "dvar-registration";
		}

		return "none";
	}
}
