#pragma once

#include <cstdint>
#include <string_view>

#include "AssetBundleZones.hpp"

namespace Components::WeaponAssetPolicy
{
	inline bool ShouldRestrict(std::string_view name, std::string_view zone, std::uint32_t version)
	{
		// IW4x weapon layouts are still handled by the separate weapon-file loader.
		if (name.empty() || version >= 316) return true;

		// Shared native-format bundles intentionally contain zombie and ported weapon names.
		if (AssetBundleZones::IsZoneName(zone)) return false;

		return name.find("_mp") == std::string_view::npos && name != "none" && name != "destructible_car";
	}
}
