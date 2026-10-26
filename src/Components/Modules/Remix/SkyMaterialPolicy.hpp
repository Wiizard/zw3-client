#pragma once

#include <string_view>

namespace Components::Remix
{
	inline bool IsSkyTextureMaterial(const std::string_view name, const std::string_view techniqueSet,
		const int sortKey)
	{
		if (techniqueSet == "sky" || techniqueSet.ends_with("_sky"))
		{
			return true;
		}

		const bool isUnlit = techniqueSet.find("unlit") != std::string_view::npos;
		const bool hasSkyName = name.find("sky") != std::string_view::npos
			|| name.find("dome") != std::string_view::npos || name.find("pano") != std::string_view::npos;

		// Converted maps also use sort 2 for lit walls. Sorting alone must never remove their occlusion.
		return isUnlit && ((sortKey == 2 && hasSkyName) || name.starts_with("wc/bg_facades"));
	}
}
