#pragma once

#include <cstdint>

namespace Components::Remix
{
	struct OutdoorStats
	{
		bool isEnabled = true;
		bool hasMap = false;
		int width = 0;
		int height = 0;
		unsigned format = 0;
		float heightSign = 1.0f;
		int maskedMaterials = 0;
		unsigned long long cloudsTested = 0;
		unsigned long long cloudsCulled = 0;
		float largestScale = 0.0f;
	};

	bool IsOutdoorMaskedMaterial(std::uintptr_t material);

	bool IsCloudOutdoors(std::uintptr_t cloud);

	bool IsWeatherPointOutdoors(const float* point);

	bool TryTestPointOutdoors(const float* point, bool& isOutdoors);

	void SetRainMaskEnabled(bool isEnabled);

	OutdoorStats GetOutdoorStats();
}
