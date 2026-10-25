#pragma once

namespace Components::Remix
{
	struct AreaExposureSettings
	{
		bool isEnabled = true;
		// How much of the brightness difference between areas the exposure makes up (0 none, 1 all of it).
		float strength = 0.7f;
		// EV added where the map's sun reaches the spot you stand on.
		float sunlitEv = 1.5f;
		// How far Remix's own auto exposure may still move around the area's value, in EV either way.
		float windowEv = 0.5f;
	};

	struct AreaExposureState
	{
		bool hasGrid = false;
		bool isActive = false;
		float mapDarkLog = 0.0f;
		float mapMedianLog = 0.0f;
		float mapBrightLog = 0.0f;
		float areaLog = 0.0f;
		float sunlitShare = 0.0f;
		float centreEv = 0.0f;
		float minEv = 0.0f;
		float maxEv = 0.0f;
	};

	// Auto exposure bounds for the spot the player stands on, from the map's baked light grid (the same per position
	// lighting the game puts on models), so where you look no longer swings the exposure. The middle of the
	// configured range is the map's median area; darker and brighter areas move the window from there.
	// Returns false when the map has no light grid or it is off, and the configured range applies as it is.
	bool TryGetAreaExposure(const AreaExposureSettings& settings, float rangeMinEv, float rangeMaxEv, float& minEv,
		float& maxEv);

	AreaExposureState GetAreaExposureState();
}
