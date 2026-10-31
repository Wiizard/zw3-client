#pragma once

#include <string_view>

#include "EffectSurfaces.hpp"

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

	inline ScriptEffectKind ClassifyScriptEffectName(const std::string_view name)
	{
		// This oriented sprite lies on the floor. Soft-particle intersection fading
		// erases the poison pool precisely where it touches that floor.
		if (name == "zw3/poison_lake")
		{
			return ScriptEffectKind::Ground;
		}

		if (IsEyeEffectName(name))
		{
			return ScriptEffectKind::Eye;
		}

		return IsProtectedScriptEffectName(name) ? ScriptEffectKind::Particle : ScriptEffectKind::None;
	}
}
