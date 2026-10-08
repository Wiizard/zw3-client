#pragma once

#include "Controller/Types.hpp"

#include "Controller/Error.hpp"
#include "Controller/Device/Id.hpp"

namespace Controller
{
	enum class Facility : std::uint8_t
	{
		Runtime,
		Discovery,
		Transport,
		Driver,
		Decode,
		Sample,
		Calibration,
		Mapping,
		Aim,
		Steam,
		Engine,
		Debug,
	};

	const char* ToString(Facility facility) noexcept;

	enum class Severity : std::uint8_t
	{
		Info,
		Warning,
		Error,
	};

	struct Diagnostic
	{
		Severity level = Severity::Info;
		Facility origin = Facility::Runtime;
		ErrorCode code = ErrorCode::None;
		DeviceId device{};
		std::string message;
	};

	class DiagnosticSink
	{
	public:
		virtual ~DiagnosticSink() = default;

		virtual void Consume(const Diagnostic& diagnostic) = 0;
	};

	class LoggingSink : public DiagnosticSink
	{
	public:
		void Consume(const Diagnostic& diagnostic) override;
	};
}
