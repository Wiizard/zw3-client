#pragma once

#include <cstdint>
#include <vector>

namespace Components::Remix
{
	enum class ProfileSection
	{
		MapSettings,
		Triggers,
		Culling,
		Options,
		Lights,
		DebugDraw,
		Markers,
		TextureTags,
		PbrMaterials,
	};

	inline constexpr int profileSectionCount = 9;

	struct SectionTiming
	{
		const char* name = "";
		float lastMs = 0.0f;
		float medianMs = 0.0f;
		float peakMs = 0.0f;
		float spikeMs = 0.0f;
		unsigned long long spikes = 0;
	};

	class ProfileScope
	{
	public:
		explicit ProfileScope(ProfileSection section);
		~ProfileScope();

		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;

	private:
		ProfileSection section;
		std::int64_t startTicks;
	};

	void EndProfileFrame();

	std::vector<SectionTiming> GetProfile();
}
