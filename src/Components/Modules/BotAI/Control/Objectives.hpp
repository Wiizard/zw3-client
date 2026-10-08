#pragma once

namespace Components::BotAI
{
	enum Gametype
	{
		GametypeUnknown = 0,
		GametypeDm,
		GametypeWar,
		GametypeDom,
		GametypeSd,
		GametypeSab,
		GametypeCtf,
		GametypeKoth,
		GametypeDd,
		GametypeOneflag,
		GametypeArena,
		GametypeGtnw,
		GametypeVip,
		GametypeCount,
	};


	struct ObjectivePoint
	{
		char name[32];
		float origin[3];
		int node;
		int ownerTeam;
		bool isActive;
	};


	void RefreshObjectives();
	void ForgetObjectives();

	Gametype CurrentGametype();
	const char* GametypeName();

	bool IsObjectiveMode();

	bool IsTeamBased();

	int ObjectiveCount();
	const ObjectivePoint& ObjectiveAt(int index);

	int NearestObjectiveNotOwned(const float* feet, int team);

	int NearestObjectiveOwned(const float* feet, int team);
}
