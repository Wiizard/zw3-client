#include "Components/Modules/SoundAliasPolicy.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void Require(const bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using Components::SoundPolicy::MapAmbienceFlags;
		// These are the actual streamed ambience flags observed on Theater.
		Require(MapAmbienceFlags("sac_map_21cc0b3969336d517bb2bb33_loop", 0x2741) == 0x341,
			"Theater's looping ambience still uses the effects channel that blocks round cues");
		Require(MapAmbienceFlags("sac_map_fbce591031eb6b9a47e3a784_point", 0x2740) == 0x340,
			"Theater's random ambience still competes with weapon streams");
		Require(MapAmbienceFlags("sac_map_water_loop", 0x26C1) == 0x2C1,
			"Loaded ambience lost its storage class or looping flag");
		Require(MapAmbienceFlags("sac_map_water_loop", 0x8000275F) == 0x8000035F,
			"Channel correction changed unrelated alias flags");
		Require(MapAmbienceFlags("mp_killstreak_choppergunner", 0x1518) == 0x1518,
			"The round start alias was changed");
		Require(MapAmbienceFlags("mp_killstreak_ac130", 0x1518) == 0x1518,
			"The round end alias was changed");
		Require(MapAmbienceFlags("weap_olympia_fire_player", 0x2CC0) == 0x2CC0,
			"The weapon's sound flags were changed");
		for (const auto name : { "sac_map_water", "sac_map_water_loop_extra", "stock_water_loop", "sac_weapon_loop" })
			Require(MapAmbienceFlags(name, 0x2741) == 0x2741, "A non-ambience alias was changed");
		for (const auto flags : { 0x2701u, 0x2641u, 0x27C1u, 0x341u, 0x2B41u })
			Require(MapAmbienceFlags("sac_map_water_loop", flags) == flags,
				"A 2D, unsupported-storage or already configured alias was changed");
		Require(MapAmbienceFlags("sac_map_water_loop", MapAmbienceFlags("sac_map_water_loop", 0x2741)) == 0x341,
			"Reloading an alias changed its channel again");
		std::cout << "Sound alias policy: Theater loop/point priority, loaded/streamed storage, round cues, weapons and flag preservation passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
