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
		Rain,
		ParticleClouds,
	};

	inline constexpr int profileSectionCount = 11;

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

	// Every 10 seconds a line in zw3_remix_log.txt: frame time (average, worst 1%, worst), zw3's own sections and the
	// per-frame load zw3 hands Remix, so a slow map can be read back after the session.
	void LogPerformance();
}
