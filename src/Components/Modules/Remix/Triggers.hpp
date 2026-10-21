#pragma once

#include <string>
#include <vector>

namespace Components::Remix
{
	struct TriggerState
	{
		std::string name;
		bool isActive = false;
		bool isGlobal = false;
		unsigned long long activations = 0;
	};

	struct TriggerStats
	{
		bool isSoundHooked = false;
		unsigned long long soundsHeard = 0;
		std::string visionName;
		bool isCameraOutdoors = false;
		bool hasOutdoorMap = false;
	};

	bool InstallTriggerHooks();

	void UpdateTriggers();

	bool IsTriggerActive(const std::string& name);

	std::vector<TriggerState> GetTriggerStates();

	std::vector<std::string> GetRecentSounds();

	TriggerStats GetTriggerStats();
}
