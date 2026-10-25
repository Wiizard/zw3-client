#pragma once

#include <cstdint>

namespace Components::Remix
{
	struct WeatherStats
	{
		unsigned long long cloudsDrawn = 0;
		unsigned long long cloudsPassedToEngine = 0;
		unsigned long long particlesDrawn = 0;
		unsigned long long particlesUnderCover = 0;
		unsigned long long particlesFaded = 0;
		unsigned long long particlesOffscreen = 0;
		unsigned long long particlesThinned = 0;
	};

	bool TryDrawWeatherCloud(IDirect3DDevice9* device, std::uintptr_t cloud, std::uintptr_t material, bool isOutdoorMasked);

	void DrawRainEverywhere(IDirect3DDevice9* device);

	void DrawScreenDrops(IDirect3DDevice9* device);

	bool IsRaining();

	void ReleaseWeather();

	WeatherStats GetWeatherStats();
}
