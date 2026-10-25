#pragma once

#include <string_view>

namespace Components::Remix
{
	inline bool IsEyeEffectName(const std::string_view name)
	{
		// The imported normal-zombie eyes have a hashed asset name instead of an eye-related prefix.
		return name == "smc_t6/zm_prison/fx/c85ed9d7224fff72062bb1d6bdb279fc"
			|| name.starts_with("misc/aircraft_light_wingtip")
			|| name.starts_with("zombie/eye_glow")
			|| (name.starts_with("zw3/") && name.find("eye") != std::string_view::npos);
	}

	inline bool IsProtectedScriptEffectName(const std::string_view name)
	{
		return IsEyeEffectName(name) || name.starts_with("zw3/")
			|| name.starts_with("misc/glow_weapon")
			|| name == "maps/mp_maps/fx_mp_ash_falling_large";
	}
}
