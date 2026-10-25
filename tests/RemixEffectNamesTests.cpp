#include "Components/Modules/Remix/EffectNames.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
	try
	{
		using namespace Components::Remix;
		for (const std::string_view name : {
			"smc_t6/zm_prison/fx/c85ed9d7224fff72062bb1d6bdb279fc",
			"zw3/redeye", "zw3/blueeye", "zw3/purpleeye", "zw3/greeneye", "zw3/orangeeye",
			"misc/aircraft_light_wingtip_red", "misc/aircraft_light_wingtip_green",
			"zombie/eye_glow_red_single", "zombie/eye_glow_blue_single", "zombie/eye_glow_lightyellow" })
		{
			if (!IsEyeEffectName(name) || !IsProtectedScriptEffectName(name))
				throw std::runtime_error("An eye colour lost its protected particle path: " + std::string(name));
		}
		for (const std::string_view name : { "maps/mp_maps/fx_mp_ash_falling_large", "zw3/snow", "misc/glow_weapon_fx" })
		{
			if (!IsProtectedScriptEffectName(name) || IsEyeEffectName(name))
				throw std::runtime_error("A protected non-eye effect was lost or received eye size/brightness adjustments");
		}
		for (const std::string_view name : { "", "misc/lamp_flare", "maps/ambient_smoke",
			"smc_t6/zm_prison/fx/unrelated", "maps/mp_maps/fx_mp_ash_falling_large_unrelated" })
		{
			if (IsEyeEffectName(name) || IsProtectedScriptEffectName(name))
				throw std::runtime_error("An unrelated effect bypassed its normal filtering");
		}
		std::cout << "Remix effects: imported red eyes, all eye colours, ash/snow particles and unrelated-effect filtering passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
