#include "Components/Modules/BotAI/Control/Objectives.hpp"
#include "Components/Modules/BotAI/Control/State.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <cstdio>
#include <cstring>

namespace Components::BotAI
{
	using Dvar_FindVar_t = void* (__cdecl*)(const char* name);

	static constexpr int maxObjectivePoints = 16;

	static constexpr int gametypeRecheckFrames = 20;

	static Gametype currentGametype = GametypeUnknown;
	static char gametypeName[16] = "";
	static void* gametypeDvar = nullptr;
	static int gametypeRecheckTick = -1;
	static ObjectivePoint points[maxObjectivePoints] = {};
	static int pointCount = 0;
	static bool pointsLoaded = false;


	struct GametypeEntry
	{
		const char* name;
		Gametype gametype;
	};

	static const GametypeEntry gametypeNames[] = {
		{ "dm", GametypeDm },
		{ "war", GametypeWar },
		{ "dom", GametypeDom },
		{ "sd", GametypeSd },
		{ "sab", GametypeSab },
		{ "ctf", GametypeCtf },
		{ "koth", GametypeKoth },
		{ "dd", GametypeDd },
		{ "oneflag", GametypeOneflag },
		{ "arena", GametypeArena },
		{ "gtnw", GametypeGtnw },
		{ "vip", GametypeVip },
	};


	static void ReadGametype()
	{
		if (!gametypeDvar)
		{
			gametypeDvar = Dvar_FindVar("g_gametype");
		}
		if (!gametypeDvar)
		{
			return;
		}

		const char* name = *reinterpret_cast<const char**>(reinterpret_cast<char*>(gametypeDvar) + dvarCurrent);
		if (!name || std::strcmp(name, gametypeName) == 0)
		{
			return;
		}

		_snprintf_s(gametypeName, sizeof(gametypeName), _TRUNCATE, "%s", name);
		currentGametype = GametypeUnknown;
		for (const GametypeEntry& entry : gametypeNames)
		{
			if (std::strcmp(entry.name, name) == 0)
			{
				currentGametype = entry.gametype;
				break;
			}
		}
		BotLog("gametype %s%s", gametypeName, IsObjectiveMode() ? " (objective mode)" : " (kills)");
	}


	static bool NameBelongsTo(const char* name, Gametype gametype)
	{
		switch (gametype)
		{
		case GametypeDom:
			return std::strstr(name, "flag_") != nullptr;
		case GametypeSd:
			return std::strstr(name, "bombzone") != nullptr || std::strstr(name, "sd_bomb") != nullptr;
		case GametypeDd:
			return std::strstr(name, "dd_bombzone") != nullptr;
		case GametypeSab:
			return std::strstr(name, "sab_bomb") != nullptr;
		case GametypeCtf:
		case GametypeOneflag:
			return std::strstr(name, "ctf_flag") != nullptr;
		case GametypeKoth:
			return std::strstr(name, "hq_hardpoint") != nullptr;
		case GametypeGtnw:
			return std::strstr(name, "gtnw_zone") != nullptr;
		default:
			return false;
		}
	}


	static int LoadObjectivesFromGraph()
	{
		int count = 0;
		for (int i = 0; i < Waypoints::ObjectiveCount() && count < maxObjectivePoints; ++i)
		{
			const Waypoints::Objective& baked = Waypoints::ObjectiveAt(i);
			const bool isLabelled = baked.gametype[0] != 0 && std::strcmp(baked.gametype, "-") != 0;
			const bool isWanted = isLabelled
				? std::strcmp(baked.gametype, gametypeName) == 0
				: NameBelongsTo(baked.name, currentGametype);
			if (!isWanted)
			{
				continue;
			}

			ObjectivePoint& point = points[count];
			++count;
			std::strncpy(point.name, baked.name, sizeof(point.name) - 1);
			point.name[sizeof(point.name) - 1] = 0;
			point.origin[0] = baked.origin[0];
			point.origin[1] = baked.origin[1];
			point.origin[2] = baked.origin[2];
			point.node = baked.node;
			point.ownerTeam = 0;
			point.isActive = true;
			BotLog("objective %s (%s) at %.0f %.0f %.0f node %d", point.name, baked.gametype,
				point.origin[0], point.origin[1], point.origin[2], point.node);
		}
		return count;
	}


	void RefreshObjectives()
	{
		if (gametypeRecheckTick < 0 || debugTick - gametypeRecheckTick >= gametypeRecheckFrames)
		{
			gametypeRecheckTick = debugTick;
			ReadGametype();
		}

		if (!pointsLoaded && Waypoints::IsLoaded())
		{
			pointsLoaded = true;
			pointCount = LoadObjectivesFromGraph();
			BotLog("objectives: %d point(s) for %s", pointCount, gametypeName);
		}
	}


	void ForgetObjectives()
	{
		pointCount = 0;
		pointsLoaded = false;
		gametypeName[0] = 0;
		currentGametype = GametypeUnknown;
		gametypeRecheckTick = -1;
	}


	Gametype CurrentGametype()
	{
		return currentGametype;
	}


	const char* GametypeName()
	{
		return gametypeName[0] ? gametypeName : "?";
	}


	bool IsTeamBased()
	{
		return currentGametype != GametypeDm;
	}


	bool IsObjectiveMode()
	{
		switch (currentGametype)
		{
		case GametypeDom:
		case GametypeSd:
		case GametypeSab:
		case GametypeCtf:
		case GametypeKoth:
		case GametypeDd:
		case GametypeOneflag:
		case GametypeGtnw:
			return true;
		default:
			return false;
		}
	}


	int ObjectiveCount()
	{
		return pointCount;
	}


	const ObjectivePoint& ObjectiveAt(int index)
	{
		return points[index];
	}


	static int NearestPoint(const float* feet, int team, bool wantOwned)
	{
		int best = -1;
		float bestSq = 0.0f;
		for (int i = 0; i < pointCount; ++i)
		{
			const ObjectivePoint& point = points[i];
			if (!point.isActive)
			{
				continue;
			}
			const bool owned = team != 0 && point.ownerTeam == team;
			if (owned != wantOwned)
			{
				continue;
			}
			const float dx = point.origin[0] - feet[0];
			const float dy = point.origin[1] - feet[1];
			const float distanceSq = dx * dx + dy * dy;
			if (best < 0 || distanceSq < bestSq)
			{
				best = i;
				bestSq = distanceSq;
			}
		}
		return best;
	}


	int NearestObjectiveNotOwned(const float* feet, int team)
	{
		return NearestPoint(feet, team, false);
	}


	int NearestObjectiveOwned(const float* feet, int team)
	{
		return NearestPoint(feet, team, true);
	}
}
