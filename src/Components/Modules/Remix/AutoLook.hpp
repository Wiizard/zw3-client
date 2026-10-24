#pragma once

#include <string>

namespace Components::Remix
{
	enum class MapTime
	{
		Day,
		Dusk,
		Night,
	};

	// What zw3 read about the loaded map to pick its starting look: sun, sky, fog and how much of it is sun lit.
	struct MapTraits
	{
		bool isKnown = false;
		bool isApplied = false;
		MapTime time = MapTime::Day;
		bool isIndoor = false;
		bool isFoggy = false;
		bool isRaining = false;
		float outdoorShare = -1.0f;
		float sunLuminance = 0.0f;
		float sunHeight = 0.0f;
		float skyLuminance = 0.0f;
		float fogHalfway = 0.0f;
		float fogOpacity = 0.0f;
		std::string summary;
	};

	// Once per map without its own zw3_remix\maps\<map>.ini, after the sun and sky are read, sets a starting look that
	// fits the map's time of day, indoor share and fog. A saved map file always wins.
	void UpdateAutoLook();

	MapTraits GetMapTraits();
}
