#pragma once

namespace Components::BotAI
{
	enum Archetype
	{
		ArchetypeRusher = 0,
		ArchetypeSlayer,
		ArchetypeAnchor,
		ArchetypeFlanker,
		ArchetypeSniper,
		ArchetypeSupport,
		ArchetypeTuber,
		ArchetypeKnifer,
		ArchetypeShotgunner,
		ArchetypeCount,
	};


	struct Loadout
	{
		const char* primary;
		const char* primaryAttachment;
		const char* secondary;
		const char* secondaryAttachment;
		const char* equipment;
		const char* perk1;
		const char* perk2;
		const char* perk3;
		const char* deathstreak;
		const char* tactical;
		const char* killstreaks[3];
	};


	struct Personality
	{
		int archetype;

		int aggression;
		int caution;
		int objectiveDrive;
		int teamplay;
		int tricks;
		int grenadeHabit;
		int focus;
		int sightDiscipline;

		int sprintPercent;
		float hipScale;
		bool isQuickscoper;
		float closeRange;
		float farRange;
		float wobblePhase;
		float wobbleYawRate;
		float wobblePitchRate;
		float turnShare;
		float turnRateScale;
		int grenadesPerLife;
		bool holdsSecondary;
		bool isMeleeOnly;
		bool isTuber;
	};


	const char* ArchetypeName(int archetype);

	void RollPersonality(int clientNum);
	bool PersonalitySettingChanged(int clientNum);
	void RerollLoadout(int clientNum);

	inline int ScaleByTrait(int percent, int trait)
	{
		const int scaled = percent * trait / 50;
		if (scaled > 100)
		{
			return 100;
		}
		return scaled;
	}
}
