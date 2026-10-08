#pragma once

#include "Controller/Types.hpp"

#include "Controller/Diagnostic.hpp"
#include "Controller/Error.hpp"
#include "Controller/Device/Id.hpp"

namespace Controller
{
	class Context
	{
	public:
		explicit Context(DiagnosticSink& sink) noexcept : sink(sink)
		{
		}

		void Report(Severity level, Facility origin, ErrorCode code, DeviceId device, std::string message) const;
		void Report(Severity level, Facility origin, ErrorCode code, std::string message) const;

	private:
		DiagnosticSink& sink;
	};
}
