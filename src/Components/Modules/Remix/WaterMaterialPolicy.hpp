#pragma once

#include <string_view>

namespace Components::Remix
{
	inline bool IsScrollingWaterMaterial(const std::string_view name, const std::string_view techniqueSet,
		const std::string_view imageName, const int cameraRegion)
	{
		const auto contains = [](const std::string_view text, const std::string_view part)
		{
			return text.find(part) != std::string_view::npos;
		};

		// Converted ocean and river materials have no native water texture semantic.
		// Foam and falling-water sprites still need their original alpha coverage.
		if (cameraRegion != 1 || !contains(techniqueSet, "_ua") || contains(name, "foam")
			|| contains(imageName, "foam") || contains(name, "waterfall") || contains(imageName, "waterfall"))
		{
			return false;
		}

		return contains(name, "water") || contains(name, "ocean") || contains(imageName, "water_ocean")
			|| contains(imageName, "water_solid") || contains(imageName, "water_scrolling");
	}
}
