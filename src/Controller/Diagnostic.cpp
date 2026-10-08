#include "STDInclude.hpp"

#include "Controller/Diagnostic.hpp"

#include "Components/Modules/Logger.hpp"

namespace Controller
{
	const char* ToString(Facility facility) noexcept
	{
		switch (facility)
		{
		case Facility::Runtime:
			return "runtime";
		case Facility::Discovery:
			return "discovery";
		case Facility::Transport:
			return "transport";
		case Facility::Driver:
			return "driver";
		case Facility::Decode:
			return "decode";
		case Facility::Sample:
			return "sample";
		case Facility::Calibration:
			return "calibration";
		case Facility::Mapping:
			return "mapping";
		case Facility::Aim:
			return "aim";
		case Facility::Steam:
			return "steam";
		case Facility::Engine:
			return "engine";
		case Facility::Debug:
			return "debug";
		}

		return "runtime";
	}

	void LoggingSink::Consume(const Diagnostic& diagnostic)
	{
		std::string line = std::format("controller: {}: {}", ToString(diagnostic.origin), diagnostic.message);

		if (diagnostic.code != ErrorCode::None)
		{
			line += std::format(" [{}]", ToString(diagnostic.code));
		}

		if (diagnostic.device)
		{
			line += std::format(" device({})", diagnostic.device.Value());
		}

		switch (diagnostic.level)
		{
		case Severity::Info:
			Components::Logger::Print("{}\n", line);
			break;
		case Severity::Warning:
			Components::Logger::Warning("{}\n", line);
			break;
		case Severity::Error:
			Components::Logger::Error("{}\n", line);
			break;
		}
	}
}
