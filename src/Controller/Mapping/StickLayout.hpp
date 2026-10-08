#pragma once

#include "Controller/Types.hpp"

#include "Controller/Sample/Axis.hpp"

namespace Controller::Mapping
{
	enum class VirtualAxis : std::uint8_t
	{
		Side,
		Forward,
		Yaw,
		Pitch,
	};

	enum class StickLayout : std::uint8_t
	{
		Standard,
		Southpaw,
		Legacy,
		LegacySouthpaw,
	};

	StickLayout StickLayoutFromName(std::string_view name);

	struct ResolvedAxes
	{
		float side = 0.0f;
		float forward = 0.0f;
		float yaw = 0.0f;
		float pitch = 0.0f;
	};

	ResolvedAxes Resolve(StickLayout layout, StickVector left, StickVector right) noexcept;
}
