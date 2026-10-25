#include "Components/Modules/WeaponAssetPolicy.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
	try
	{
		using Components::WeaponAssetPolicy::ShouldRestrict;
		for (const std::string_view weapon : {
			"cz75_upgraded_zm", "ithaca_zm", "dsr50_zm", "m1911_upgraded_zm", "fg42_bipod", "kar98k",
			"870mcs_zm", "hamr_zm", "mg08_zm", "pdw57_zm", "srm1216_zm", "svu_zm" })
		{
			for (const std::string_view zone : { "zw3_assets", "zw3_assets_0", "zw3_assets_1", "zw3_assets_pack", "ZW3_ASSETS_2" })
				if (ShouldRestrict(weapon, zone, 276))
					throw std::runtime_error("The supplied weapon bundles still reject " + std::string(weapon));
			if (!ShouldRestrict(weapon, "other_zone", 276))
				throw std::runtime_error("The bundle exception leaked into unrelated zones");
			for (const auto version : { 316u, 319u, 332u, 359u, 360u, 365u })
				for (const std::string_view zone : { "zw3_assets", "zw3_assets_0", "zw3_assets_extra" })
					if (!ShouldRestrict(weapon, zone, version))
						throw std::runtime_error("An incompatible IW4x weapon layout bypassed the existing restriction");
		}

		for (const std::string_view weapon : { "ak47_mp", "none", "destructible_car" })
		{
			if (ShouldRestrict(weapon, "common_mp", 276) || ShouldRestrict(weapon, "zw3_assets", 276))
				throw std::runtime_error("An existing supported weapon was rejected");
			if (!ShouldRestrict(weapon, "zw3_assets", 316))
				throw std::runtime_error("The IW4x format restriction must apply even to multiplayer names");
		}
		if (!ShouldRestrict("", "zw3_assets", 276)
			|| !ShouldRestrict("ithaca_zm", "other_zw3_assets", 276)
			|| !ShouldRestrict("ithaca_zm", "zw3", 276))
			throw std::runtime_error("Only asset bundles with the expected prefix may bypass the weapon-name filter");

		std::cout << "Weapon asset policy: all 12 exported weapons, native multiplayer assets and IW4x restrictions passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
