#pragma once

#include <cstdint>

namespace Components::Remix
{
	struct SkyStats
	{
		bool hasSky = false;
		bool didReadSky = false;
		float skyLuminance = 0.0f;
		float sunElevation = 0.0f;
		float hazeShare = 0.0f;
		float brightness = 1.0f;
		int submissions = 0;
	};

	void NoteSkyMaterial(std::uintptr_t material);

	void ForgetSkyMaterial();

	void UpdateSkyBrightness(IDirect3DDevice9* device);

	void SetSkyScale(float scale);

	float GetSkyScale();

	SkyStats GetSkyStats();
}
