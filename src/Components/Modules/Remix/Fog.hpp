#pragma once

namespace Components::Remix
{
	struct FogStats
	{
		bool isEnabled = true;
		float strength = 1.0f;
		bool hasMapFog = false;
		float start = 0.0f;
		float halfway = 0.0f;
		float maxOpacity = 0.0f;
		float colour[3] = {};
		bool hasSunFog = false;
		float farOpacity = 0.0f;
		float distanceMeters = 0.0f;
		float albedo[3] = {};
		float anisotropy = 0.0f;
		int submissions = 0;
	};

	void UpdateMapFog(IDirect3DDevice9* device);

	void SetMapFogEnabled(bool isEnabled);

	void SetMapFogStrength(float strength);

	FogStats GetFogStats();
}
