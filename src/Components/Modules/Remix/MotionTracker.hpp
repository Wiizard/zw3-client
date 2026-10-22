#pragma once

namespace Components::Remix
{
	void SetLampFlaresHidden(bool isHidden);

	bool AreLampFlaresHidden();

	bool InstallMotionTrackerHooks();

	void SetTrackerBrightness(float brightness);

	float GetTrackerBrightness();
}
