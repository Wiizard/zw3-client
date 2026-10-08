#include "STDInclude.hpp"

#include "Controller/Aim/Calibration.hpp"

namespace Controller::Aim
{
	std::optional<AimCalibration> AimCalibration::TryMake(const AimSettings& settings, std::string& why)
	{
		std::string reason;

		if (!IsValid(settings.hip.deadzone, reason))
		{
			why = "hip deadzone: " + reason;
			return std::nullopt;
		}

		if (!IsValid(settings.ads.deadzone, reason))
		{
			why = "ADS deadzone: " + reason;
			return std::nullopt;
		}

		std::optional<AimGraph> built;

		if (settings.graphKnots)
		{
			built = AimGraph::TryMake(std::span<const Knot>(settings.graphKnots->data(), settings.graphKnots->size()), settings.isGraphMonotonic, reason);

			if (!built)
			{
				why = "aim graph: " + reason;
				return std::nullopt;
			}
		}

		AimCalibration calibration;
		calibration.hip = settings.hip;
		calibration.ads = settings.ads;
		calibration.accel = settings.accel;
		calibration.graph = built;
		return calibration;
	}

	AimProcessor::Config AimCalibration::ProcessorConfig() const noexcept
	{
		const AimGraph* graphPointer = nullptr;

		if (this->graph)
		{
			graphPointer = &*this->graph;
		}

		return { this->hip, this->ads, this->accel, graphPointer };
	}
}
