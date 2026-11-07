#pragma once

#include <string_view>

namespace Components::Remix
{
	inline bool IsCutoutDecalState(int region, int sortKey, int blend, bool alphaTested)
	{
		// Converted cut-out overlays also use the opaque camera region.
		return (region == 0 || region == 1) && sortKey >= 6 && sortKey <= 16
			&& blend == 0x012 && alphaTested;
	}

	inline bool IsCutoutDecalName(std::string_view name)
	{
		name = name.substr(name.rfind('/') == std::string_view::npos ? 0 : name.rfind('/') + 1);
		return name.find("decal") != std::string_view::npos || name.ends_with("_dec")
			|| name.find("_dec_") != std::string_view::npos || name.find("paper") != std::string_view::npos
			|| name.find("poster") != std::string_view::npos || name.find("streetline") != std::string_view::npos
			|| name.find("crosswalk") != std::string_view::npos || name.find("rug") != std::string_view::npos;
	}

	inline bool IsConvertedDecalAlias(std::string_view name, int region, int sortKey, int blend, bool alphaTested)
	{
		if (!name.starts_with("sac_bo1_local/") && !name.starts_with("generated/"))
		{
			return false;
		}

		// An alias is only a candidate: a named decal sharing its GPU texture must confirm it.
		return IsCutoutDecalState(region, sortKey, blend, alphaTested)
			|| ((region == 0 || region == 1) && sortKey >= 6 && sortKey <= 16
				&& (blend == 0x162 || blend == 0x165));
	}

	inline bool HasNonDecalGeometry(bool hasGeometry, bool hasConvertedAlias, bool hasNamedDecal)
	{
		// Genuine opaque users always protect a shared wall/floor texture from decal tagging.
		return hasGeometry || (hasConvertedAlias && !hasNamedDecal);
	}
}
