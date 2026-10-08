#include "STDInclude.hpp"

#include "Controller/Context.hpp"

namespace Controller
{
	void Context::Report(Severity level, Facility origin, ErrorCode code, DeviceId device, std::string message) const
	{
		this->sink.Consume(Diagnostic{ level, origin, code, device, std::move(message) });
	}

	void Context::Report(Severity level, Facility origin, ErrorCode code, std::string message) const
	{
		this->sink.Consume(Diagnostic{ level, origin, code, noDevice, std::move(message) });
	}
}
