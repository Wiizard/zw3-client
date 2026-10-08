#pragma once

#include "Controller/Types.hpp"

#include "Controller/Aim/Assist.hpp"
#include "Controller/Aim/Graph.hpp"
#include "Controller/Aim/Types.hpp"

namespace Controller::Aim
{
	struct AimSettings
	{
		AimProfile hip;
		AimProfile ads;
		TurnIntegrator::Limits accel;
		std::optional<std::vector<Knot>> graphKnots;
		bool isGraphMonotonic = true;
	};

	class AimCalibration
	{
	public:
		static std::optional<AimCalibration> TryMake(const AimSettings& settings, std::string& why);

		AimProcessor::Config ProcessorConfig() const noexcept;

	private:
		AimCalibration() = default;

		AimProfile hip;
		AimProfile ads;
		TurnIntegrator::Limits accel;
		std::optional<AimGraph> graph;
	};
}
