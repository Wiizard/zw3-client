#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/Control/Objectives.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <cmath>

namespace Components::BotAI
{
	static constexpr float behindPenaltySq = 80.0f * 80.0f;
	static constexpr float behindUnits     = 10.0f;
	static constexpr float pursuitMaxDistSq     = 700.0f * 700.0f;
	static constexpr int   pursuitMs            = 2500;

	static constexpr float huntNearUnits        = 1000.0f;

	static constexpr int   behindHuntWeightPercent  = 35;
	static constexpr float behindDot            = -0.2f;

	static constexpr float huntReplanMoveSq     = 900.0f * 900.0f;

	static constexpr float roamMinSq            = 500.0f * 500.0f;
	static constexpr float roamMaxSq            = 2000.0f * 2000.0f;
	static constexpr int   roamGoalTries        = 64;
	static constexpr float roamShareRadiusSq    = 300.0f * 300.0f;
	static constexpr int   openingMs            = 15000;
	static constexpr float openingSpreadSq      = 800.0f * 800.0f;
	static constexpr int   openingSpreadTries   = 48;
	static constexpr float holdFrontConeDeg     = 90.0f;
	static constexpr int   coverThreatPenetrateType = 2;
	static constexpr float coverShareMax        = 0.2f;
	static constexpr float coverWallReachUnits  = 64.0f;
	static constexpr int   coverWallShareMax    = 64;
	static constexpr float lowCoverOpenUnits    = 256.0f;
	static constexpr float exposureCost         = 40.0f;
	static constexpr int   exposureMemoryMs     = 10000;
	static constexpr float threatLookMinUnits   = 300.0f;
	static constexpr float threatLookMaxUnits   = 2500.0f;
	static constexpr float threatKnownRangeSq   = 600.0f * 600.0f;
	static constexpr float threatKnownScale     = 4.0f;
	static constexpr float lookThreatWeight     = 2.0f;
	static constexpr float sniperSightMinUnits  = 1000.0f;
	static constexpr float sniperSightMaxUnits  = 8000.0f;
	static constexpr float holdCoverBonus       = 1.3f;
	static constexpr float holdMinAreaScore     = 20.0f;
	static constexpr int   clientAreaRefreshMs  = 250;
	static constexpr float flankNodeCost        = 30.0f;
	static constexpr float attackNodeCost       = 20.0f;
	static constexpr float flankFreeUnits       = 600.0f;
	static constexpr int   flankerFlankPercent  = 80;
	static constexpr int   holdPoolSize         = 32;
	static constexpr float watchConeDeg         = 60.0f;
	static constexpr float watchedAreaShare     = 0.35f;
	static constexpr float searchRadiusUnits    = 900.0f;
	static constexpr int   searchMs             = 20000;
	static constexpr float memoryLookWeight     = 1.0f;
	static constexpr float memoryHoldWeight     = 0.5f;
	static constexpr float dangerRouteCost      = 25.0f;
	static constexpr int   seeingNodeCandidates = 8;
	static constexpr float relocateSpacingUnits = 400.0f;
	static constexpr int   sneakHeardMs         = 3000;
	static constexpr int   sneakIntelMs         = 4000;
	static constexpr float seeingNodeReachUnits = 400.0f;
	static constexpr float seeingNodeEyeRise    = 40.0f;
	static constexpr float hiddenSpotSpacingUnits = 250.0f;
	static constexpr float hiddenSpotRiseUnits  = 120.0f;
	static constexpr float startRiseMaxUnits    = 24.0f;
	static constexpr int   lookYawSamples       = 8;
	static constexpr float lookOpenFarUnits     = 600.0f;
	static constexpr float lookOpenNearUnits    = 250.0f;
	static constexpr float lookFrontWeight      = 2.0f;
	static constexpr float playableProbeUnits   = 400.0f;
	static constexpr float playableNodeSq       = 250.0f * 250.0f;
	static constexpr float playableNodeDz       = 300.0f;
	static constexpr float stairRiseUnits       = 18.0f;
	static constexpr float stairSlope           = 0.35f;
	static constexpr int   holdMinLinks         = 5;

	static constexpr int   laneLinks            = 4;
	static constexpr int   laneGoalLinks        = 3;
	static constexpr float narrowLinkPenalty    = 70.0f;

	static constexpr int   openLinks            = 7;
	static constexpr float wallHugPenalty       = 90.0f;
	static constexpr float neighbourHugShare    = 0.6f;
	static constexpr float deadEndPenalty       = 300.0f;
	static constexpr float crouchNodePenalty    = 120.0f;
	static constexpr float proneNodePenalty     = 300.0f;
	static constexpr float climbNodePenalty     = 90.0f;

	static constexpr float ledgeNodePenalty     = 260.0f;

	static constexpr float followGoalMaxUnits   = 500.0f;
	static constexpr float followGoalRiseUnits  = 80.0f;


	static constexpr int   holdCandidates    = 8;
	static constexpr int   holdSamples       = 4;
	static constexpr int   holdSampleTries   = 14;
	static constexpr float holdSampleMinUnits = 500.0f;
	static constexpr float holdSampleMaxUnits = 1400.0f;
	static constexpr int   holdCandidateTries = 40;

	static constexpr float followReplanMoveSq = 600.0f * 600.0f;

	static constexpr int   holdMinSeen        = 2;

	static constexpr float stuckHeatAmount               = 1500.0f;
	static constexpr float stuckHeatDecayPerSecond = 0.98f;

	static constexpr int   linkTrapCap   = 64;
	static constexpr int   deadLinkFailures = 4;
	static constexpr float deadLinkPenalty = 1000000.0f;
	static constexpr int   deadNodeLinks = 2;
	static constexpr float deadNodePenalty = 1000000.0f;
	static constexpr float linkTrapEscalation = 1.5f;
	static constexpr float linkTrapFloor = 50.0f;


	float GraphDistanceSq(const float* feet, const float* nodeOrigin)
	{
		const float dx = nodeOrigin[0] - feet[0];
		const float dy = nodeOrigin[1] - feet[1];
		const float dz = (nodeOrigin[2] - feet[2]) * 3.0f;
		return dx * dx + dy * dy + dz * dz;
	}


	static float nodeHeat[2][Waypoints::nodeCap] = {};
	static float scaledHeat[Waypoints::nodeCap] = {};
	static float stuckHeat[Waypoints::nodeCap] = {};
	static float lanePenalty[Waypoints::nodeCap] = {};
	static Waypoints::LinkTrap linkTraps[linkTrapCap] = {};
	static int linkTrapCount = 0;
	static bool deadNode[Waypoints::nodeCap] = {};
	static bool hasStuckHeat = false;
	static bool hasLanePenalty = false;
	static int heatDecayTick = -1;
	static int heatBuiltTick = -1;
	static int heatBuiltTeam = -1;
	static int heatBuiltScale = -1;


	bool IsLaneNode(int node)
	{
		return Waypoints::ChildCount(node) >= laneGoalLinks && Waypoints::TypeOf(node) == Waypoints::NodeStand;
	}


	static void BuildLanePenalty(int count)
	{
		hasLanePenalty = true;
		for (int n = 0; n < count; ++n)
		{
			float penalty = 0.0f;
			const int links = Waypoints::ChildCount(n);
			if (links < laneLinks)
			{
				penalty += static_cast<float>(laneLinks - links) * narrowLinkPenalty;
			}
			if (links < openLinks)
			{
				penalty += static_cast<float>(openLinks - links) * wallHugPenalty;
			}
			if (links <= 1)
			{
				penalty += deadEndPenalty;
			}
			switch (Waypoints::TypeOf(n))
			{
			case Waypoints::NodeCrouch:
				penalty += crouchNodePenalty;
				break;
			case Waypoints::NodeProne:
				penalty += proneNodePenalty;
				break;
			case Waypoints::NodeClimb:
				penalty += climbNodePenalty;
				break;
			default:
				break;
			}

			for (int i = 0; i < links; ++i)
			{
				if (Waypoints::KindOf(n, Waypoints::ChildAt(n, i)) == Waypoints::LinkDrop)
				{
					penalty += ledgeNodePenalty;
					break;
				}
			}
			lanePenalty[n] = penalty;
		}

		static float ringPenalty[Waypoints::nodeCap];
		for (int ring = 0; ring < 2; ++ring)
		{
			const float share = ring == 0 ? neighbourHugShare : neighbourHugShare * 0.5f;
			for (int n = 0; n < count; ++n)
			{
				ringPenalty[n] = lanePenalty[n];
			}
			for (int n = 0; n < count; ++n)
			{
				const int links = Waypoints::ChildCount(n);
				if (links == 0)
				{
					continue;
				}
				float neighbourSum = 0.0f;
				for (int i = 0; i < links; ++i)
				{
					neighbourSum += ringPenalty[Waypoints::ChildAt(n, i)];
				}
				lanePenalty[n] += share * neighbourSum / static_cast<float>(links);
			}
		}
	}

	static int HeatLane(int team)
	{
		return team == 2 ? 1 : 0;
	}

	void AddHeat(int team, int node, float amount)
	{
		if (node >= 0 && node < Waypoints::nodeCap)
		{
			nodeHeat[HeatLane(team)][node] += amount;
		}
	}

	void AddStuckHeat(int node)
	{
		if (node >= 0 && node < Waypoints::nodeCap)
		{
			stuckHeat[node] += stuckHeatAmount;
			hasStuckHeat = true;
		}
	}

	static void OnLinkDied(int from, int to)
	{
		int deadInto = 1;
		for (int t = 0; t < linkTrapCount; ++t)
		{
			if (linkTraps[t].to == to && linkTraps[t].from != from && linkTraps[t].failures >= deadLinkFailures)
			{
				++deadInto;
			}
		}
		if (deadInto >= deadNodeLinks && to < Waypoints::nodeCap && !deadNode[to])
		{
			deadNode[to] = true;
			BotLog("trap node %d dead for the match: %d ways onto it failed", to, deadInto);
		}
	}


	void AddLinkTrap(int from, int to)
	{
		if (from < 0 || to < 0 || from == to)
		{
			return;
		}

		for (int t = 0; t < linkTrapCount; ++t)
		{
			if (linkTraps[t].from == from && linkTraps[t].to == to)
			{
				++linkTraps[t].failures;
				if (linkTraps[t].failures >= deadLinkFailures)
				{
					if (linkTraps[t].penalty < deadLinkPenalty)
					{
						BotLog("trap link %d -> %d dead for the match after %d failures", from, to, linkTraps[t].failures);
						OnLinkDied(from, to);
					}
					linkTraps[t].penalty = deadLinkPenalty;
					return;
				}
				linkTraps[t].penalty += stuckHeatAmount * static_cast<float>(linkTraps[t].failures) * linkTrapEscalation;
				return;
			}
		}

		int slot = linkTrapCount;
		if (linkTrapCount < linkTrapCap)
		{
			++linkTrapCount;
		}
		else
		{
			slot = 0;
			for (int t = 1; t < linkTrapCount; ++t)
			{
				if (linkTraps[t].penalty < linkTraps[slot].penalty)
				{
					slot = t;
				}
			}
		}
		linkTraps[slot].from = from;
		linkTraps[slot].to = to;
		linkTraps[slot].penalty = stuckHeatAmount;
		linkTraps[slot].failures = 1;
	}

	bool IsRouteDead(const short* path, int length)
	{
		for (int n = 1; n < length; ++n)
		{
			const int from = path[n - 1];
			const int to = path[n];
			if (to >= 0 && to < Waypoints::nodeCap && deadNode[to])
			{
				return true;
			}
			for (int t = 0; t < linkTrapCount; ++t)
			{
				if (linkTraps[t].from == from && linkTraps[t].to == to && linkTraps[t].failures >= deadLinkFailures)
				{
					return true;
				}
			}
		}
		return false;
	}

	const Waypoints::LinkTrap* LinkTraps(int* outCount)
	{
		*outCount = linkTrapCount;
		return linkTraps;
	}

	void ClearRouteHeat()
	{
		for (int lane = 0; lane < 2; ++lane)
		{
			for (int n = 0; n < Waypoints::nodeCap; ++n)
			{
				nodeHeat[lane][n] = 0.0f;
			}
		}
		for (int n = 0; n < Waypoints::nodeCap; ++n)
		{
			stuckHeat[n] = 0.0f;
			lanePenalty[n] = 0.0f;
			deadNode[n] = false;
		}
		linkTrapCount = 0;
		hasStuckHeat = false;
		hasLanePenalty = false;
		heatDecayTick = -1;
		heatBuiltTick = -1;
	}


	void DropPath(BotState& bot)
	{
		bot.pathLength = 0;
		bot.pathIndex = 0;
		bot.goalNode = -1;
		bot.smoothNode = -1;
		bot.lookNode = -1;
		bot.pathTaskSerial = -1;
		bot.pathTaskKind = TaskNone;
	}


	static float heatBuiltLaneScale = 0.0f;
	static float heatBuiltExposureScale = 0.0f;
	static float areaExposure[Waypoints::maxAreas] = {};
	static bool BuildExposure(int team, int now);

	const float* HeatPenalty(int team, float laneScale, float exposureScale, const RouteWatch* watch)
	{
		if (!Waypoints::IsLoaded())
		{
			return nullptr;
		}

		const int count = Waypoints::Count();
		if (!hasLanePenalty)
		{
			BuildLanePenalty(count);
		}
		if (heatDecayTick < 0 || debugTick - heatDecayTick >= 20)
		{
			heatDecayTick = debugTick;
			for (int lane = 0; lane < 2; ++lane)
			{
				for (int n = 0; n < count; ++n)
				{
					nodeHeat[lane][n] *= heatDecayPerSecond;
				}
			}
			if (hasStuckHeat)
			{
				for (int n = 0; n < count; ++n)
				{
					stuckHeat[n] *= stuckHeatDecayPerSecond;
				}
			}

			int kept = 0;
			for (int t = 0; t < linkTrapCount; ++t)
			{
				if (linkTraps[t].failures < deadLinkFailures)
				{
					linkTraps[t].penalty *= stuckHeatDecayPerSecond;
				}
				if (linkTraps[t].penalty >= linkTrapFloor)
				{
					linkTraps[kept] = linkTraps[t];
					++kept;
				}
			}
			linkTrapCount = kept;
		}

		const int scale = tuning.spread;
		const bool hasWatch = watch != nullptr && watch->area >= 0 && watch->area < Waypoints::AreaCount();
		if (heatBuiltTick == debugTick && heatBuiltTeam == team && heatBuiltScale == scale
			&& heatBuiltLaneScale == laneScale && heatBuiltExposureScale == exposureScale && !hasWatch)
		{
			return scaledHeat;
		}
		heatBuiltTick = debugTick;
		heatBuiltTeam = team;
		heatBuiltScale = scale;
		heatBuiltLaneScale = laneScale;
		heatBuiltExposureScale = exposureScale;
		if (hasWatch)
		{
			heatBuiltTick = -1;
		}

		const float* lane = nodeHeat[HeatLane(team)];
		const float factor = scale > 0 ? static_cast<float>(scale) / 100.0f : 0.0f;
		const bool hasExposure = exposureScale > 0.0f && BuildExposure(team, ServerTimeMs());
		const bool hasMemory = exposureScale > 0.0f && HasMapMemory();
		const float exposureUnits = exposureCost * exposureScale;
		const float dangerUnits = dangerRouteCost * exposureScale;
		float watchFreeSq = 0.0f;
		if (hasWatch)
		{
			watchFreeSq = watch->freeUnits * watch->freeUnits;
		}
		for (int n = 0; n < count; ++n)
		{
			scaledHeat[n] = lane[n] * factor + stuckHeat[n] + lanePenalty[n] * laneScale;
			if (deadNode[n])
			{
				scaledHeat[n] += deadNodePenalty;
			}
			if (!hasExposure && !hasWatch && !hasMemory)
			{
				continue;
			}
			const int area = Waypoints::AreaOf(n);
			if (area < 0)
			{
				continue;
			}
			if (hasExposure)
			{
				scaledHeat[n] += areaExposure[area] * exposureUnits;
			}
			if (hasMemory)
			{
				scaledHeat[n] += AreaDanger(area) * dangerUnits;
			}
			if (hasWatch && Waypoints::AreasSee(watch->area, area)
				&& DistanceSq2D(Waypoints::Origin(n), watch->goal) > watchFreeSq)
			{
				scaledHeat[n] += watch->nodeCost;
			}
		}
		return scaledHeat;
	}


	static int PickHuntGoal(const float* enemyFeet)
	{
		int candidates[huntGoalChoices];
		float candidateSq[huntGoalChoices];
		int count = 0;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				continue;
			}
			const float distanceSq = GraphDistanceSq(enemyFeet, Waypoints::Origin(n));
			if (count == huntGoalChoices && distanceSq >= candidateSq[count - 1])
			{
				continue;
			}
			int slot = count < huntGoalChoices ? count : huntGoalChoices - 1;
			while (slot > 0 && candidateSq[slot - 1] > distanceSq)
			{
				candidates[slot] = candidates[slot - 1];
				candidateSq[slot] = candidateSq[slot - 1];
				--slot;
			}
			candidates[slot] = n;
			candidateSq[slot] = distanceSq;
			if (count < huntGoalChoices)
			{
				++count;
			}
		}
		if (count == 0)
		{
			return -1;
		}

		int usable = 1;
		while (usable < count && candidateSq[usable] - candidateSq[0] < huntGoalSpreadSq)
		{
			++usable;
		}
		const float target[3] = { enemyFeet[0], enemyFeet[1], enemyFeet[2] + seeingNodeEyeRise };
		int seeing[huntGoalChoices];
		int seeingCount = 0;
		for (int k = 0; k < usable; ++k)
		{
			const float* origin = Waypoints::Origin(candidates[k]);
			const float eye[3] = { origin[0], origin[1], origin[2] + seeingNodeEyeRise };
			if (G_LocationalTracePassed(eye, target, entityNumNone, entityNumNone, maskSight, nullptr) != 0
				&& !Navgen::IsRenderBlocked(eye, target))
			{
				seeing[seeingCount] = candidates[k];
				++seeingCount;
			}
		}
		if (seeingCount > 0)
		{
			return seeing[NextRand() % static_cast<unsigned int>(seeingCount)];
		}
		return candidates[NextRand() % static_cast<unsigned int>(usable)];
	}


	struct EnemyIntel
	{
		float pendingFeet[3];
		int pendingSeenTime;
		int pendingUsableTime;
		bool isPending;
		float knownFeet[3];
		int knownTime;
		int knownArea = -1;
	};
	static EnemyIntel teamIntel[2][maxClients] = {};

	static void PublishDueIntel(EnemyIntel& entry, int now)
	{
		if (!entry.isPending || now < entry.pendingUsableTime)
		{
			return;
		}
		entry.knownFeet[0] = entry.pendingFeet[0];
		entry.knownFeet[1] = entry.pendingFeet[1];
		entry.knownFeet[2] = entry.pendingFeet[2];
		entry.knownTime = entry.pendingSeenTime;
		entry.knownArea = Waypoints::AreaOf(Waypoints::Nearest(entry.knownFeet));
		entry.isPending = false;
	}

	void ReportSighting(int spotterTeam, int enemy, const float* enemyFeet, int now)
	{
		if (spotterTeam == 0 || enemy < 0 || enemy >= maxClients)
		{
			return;
		}
		EnemyIntel& entry = teamIntel[HeatLane(spotterTeam)][enemy];
		PublishDueIntel(entry, now);
		if (entry.isPending)
		{
			return;
		}

		const float bearing = Flrand(0.0f, 6.2831853f);
		const float fuzz = Flrand(calloutFuzzMinUnits, calloutFuzzMaxUnits);
		entry.pendingFeet[0] = enemyFeet[0] + std::cos(bearing) * fuzz;
		entry.pendingFeet[1] = enemyFeet[1] + std::sin(bearing) * fuzz;
		entry.pendingFeet[2] = enemyFeet[2];
		entry.pendingSeenTime = now;
		entry.pendingUsableTime = now + IrandMs(calloutDelayMinMs, calloutDelayMaxMs);
		entry.isPending = true;
	}

	static bool BuildExposure(int team, int now)
	{
		const int areaCount = Waypoints::AreaCount();
		if (areaCount == 0 || team == 0)
		{
			return false;
		}
		for (int area = 0; area < areaCount; ++area)
		{
			areaExposure[area] = 0.0f;
		}
		bool hasThreat = false;
		const int lane = HeatLane(team);
		for (int enemy = 0; enemy < maxClients; ++enemy)
		{
			EnemyIntel& entry = teamIntel[lane][enemy];
			PublishDueIntel(entry, now);
			if (entry.knownTime == 0 || now < entry.knownTime || now - entry.knownTime > exposureMemoryMs
				|| entry.knownArea < 0)
			{
				continue;
			}
			const float weight = 1.0f - static_cast<float>(now - entry.knownTime) / static_cast<float>(exposureMemoryMs);
			for (int area = 0; area < areaCount; ++area)
			{
				if (Waypoints::AreasSee(entry.knownArea, area))
				{
					areaExposure[area] += weight;
					hasThreat = true;
				}
			}
		}
		return hasThreat;
	}

	static int clientArea[maxClients] = {};
	static int clientAreaTime[maxClients] = {};

	int AreaOfClient(int clientNum)
	{
		if (clientNum < 0 || clientNum >= maxClients || Waypoints::AreaCount() == 0)
		{
			return -1;
		}
		const int now = ServerTimeMs();
		const int age = now - clientAreaTime[clientNum];
		if (clientAreaTime[clientNum] == 0 || age < 0 || age >= clientAreaRefreshMs
			|| clientArea[clientNum] >= Waypoints::AreaCount())
		{
			float feet[3];
			FeetOf(ReadPlayerView(clientNum), feet);
			clientArea[clientNum] = Waypoints::AreaOf(Waypoints::Nearest(feet));
			clientAreaTime[clientNum] = now;
		}
		return clientArea[clientNum];
	}

	bool IsWalledOff(int clientNum, int other)
	{
		const int here = AreaOfClient(clientNum);
		const int there = AreaOfClient(other);
		if (here < 0 || there < 0)
		{
			return false;
		}
		return !Waypoints::AreasSee(here, there);
	}

	int FindHiddenAreas(int fromArea, const float* around, float maxUnits, int* outAreas, int maxCount)
	{
		const int areaCount = Waypoints::AreaCount();
		if (areaCount == 0 || fromArea < 0)
		{
			return 0;
		}
		const float maxSq = maxUnits * maxUnits;
		const float spacingSq = hiddenSpotSpacingUnits * hiddenSpotSpacingUnits;
		int found = 0;
		while (found < maxCount)
		{
			int best = -1;
			float bestSq = 0.0f;
			for (int area = 0; area < areaCount; ++area)
			{
				if (Waypoints::AreasSee(fromArea, area))
				{
					continue;
				}
				const float* origin = Waypoints::Origin(Waypoints::AreaAt(area)->node);
				if (std::fabs(origin[2] - around[2]) > hiddenSpotRiseUnits)
				{
					continue;
				}
				const float distanceSq = DistanceSq2D(around, origin);
				if (distanceSq > maxSq || (best >= 0 && distanceSq >= bestSq))
				{
					continue;
				}
				bool isNearChosen = false;
				for (int k = 0; k < found; ++k)
				{
					const float* chosen = Waypoints::Origin(Waypoints::AreaAt(outAreas[k])->node);
					if (outAreas[k] == area || DistanceSq2D(chosen, origin) < spacingSq)
					{
						isNearChosen = true;
					}
				}
				if (isNearChosen)
				{
					continue;
				}
				best = area;
				bestSq = distanceSq;
			}
			if (best < 0)
			{
				break;
			}
			outAreas[found] = best;
			++found;
		}
		return found;
	}

	static int NearestSeeingNode(const float* point)
	{
		int candidates[seeingNodeCandidates];
		float candidateSq[seeingNodeCandidates];
		int count = 0;
		const float reachSq = seeingNodeReachUnits * seeingNodeReachUnits;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				continue;
			}
			const float distanceSq = GraphDistanceSq(point, Waypoints::Origin(n));
			if (distanceSq > reachSq || (count == seeingNodeCandidates && distanceSq >= candidateSq[count - 1]))
			{
				continue;
			}
			int slot = count;
			if (count == seeingNodeCandidates)
			{
				slot = seeingNodeCandidates - 1;
			}
			while (slot > 0 && candidateSq[slot - 1] > distanceSq)
			{
				candidates[slot] = candidates[slot - 1];
				candidateSq[slot] = candidateSq[slot - 1];
				--slot;
			}
			candidates[slot] = n;
			candidateSq[slot] = distanceSq;
			if (count < seeingNodeCandidates)
			{
				++count;
			}
		}
		const float target[3] = { point[0], point[1], point[2] + seeingNodeEyeRise };
		for (int k = 0; k < count; ++k)
		{
			const float* origin = Waypoints::Origin(candidates[k]);
			const float eye[3] = { origin[0], origin[1], origin[2] + seeingNodeEyeRise };
			if (G_LocationalTracePassed(eye, target, entityNumNone, entityNumNone, maskSight, nullptr) != 0
				&& !Navgen::IsRenderBlocked(eye, target))
			{
				return candidates[k];
			}
		}
		return Waypoints::Nearest(point);
	}

	bool TryGetRouteWatch(int clientNum, int goal, RouteWatch* out)
	{
		BotState& bot = bots[clientNum];
		const Task& task = bot.task;
		if (Waypoints::AreaCount() == 0 || goal < 0)
		{
			return false;
		}
		const int goalArea = Waypoints::AreaOf(goal);
		if (goalArea < 0)
		{
			return false;
		}

		float nodeCost = 0.0f;
		if (task.kind == TaskCapture)
		{
			nodeCost = attackNodeCost * (0.5f + static_cast<float>(bot.personality.caution) / 100.0f);
		}
		else if (task.kind == TaskHunt)
		{
			if (bot.flankSerial != task.serial)
			{
				bot.flankSerial = task.serial;
				int flankChance = (bot.personality.caution + 100 - bot.personality.aggression) / 3;
				if (bot.personality.archetype == ArchetypeFlanker)
				{
					flankChance = flankerFlankPercent;
				}
				bot.isFlanking = RollPercent(flankChance);
			}
			if (!bot.isFlanking)
			{
				return false;
			}
			nodeCost = flankNodeCost;
		}
		else
		{
			return false;
		}

		const float* origin = Waypoints::Origin(goal);
		out->area = goalArea;
		out->nodeCost = nodeCost;
		out->freeUnits = flankFreeUnits;
		out->goal[0] = origin[0];
		out->goal[1] = origin[1];
		out->goal[2] = origin[2];
		return true;
	}

	static void ForgetIntel(int enemy)
	{
		for (int lane = 0; lane < 2; ++lane)
		{
			teamIntel[lane][enemy] = EnemyIntel{};
		}
	}

	static bool TryGetTeamIntel(int team, int enemy, int now, float outFeet[3], int* outTime)
	{
		if (team == 0 || enemy < 0 || enemy >= maxClients)
		{
			return false;
		}
		EnemyIntel& entry = teamIntel[HeatLane(team)][enemy];
		PublishDueIntel(entry, now);
		if (entry.knownTime == 0 || now < entry.knownTime || now - entry.knownTime > intelMaxAgeMs)
		{
			return false;
		}
		outFeet[0] = entry.knownFeet[0];
		outFeet[1] = entry.knownFeet[1];
		outFeet[2] = entry.knownFeet[2];
		*outTime = entry.knownTime;
		return true;
	}

	bool IsKnownEnemyNear(int clientNum, const PlayerView& self, float rangeUnits)
	{
		const BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();
		const float rangeSq = rangeUnits * rangeUnits;
		if (bot.heardClient >= 0 && now - bot.heardTime < sneakHeardMs)
		{
			const PlayerView heard = ReadPlayerView(bot.heardClient);
			if (IsEnemy(self, heard) && DistanceSq2D(self.eye, heard.eye) < rangeSq)
			{
				return true;
			}
		}
		if (self.team == 0)
		{
			return false;
		}
		for (int enemy = 0; enemy < maxClients; ++enemy)
		{
			float known[3];
			int knownTime = 0;
			if (enemy != clientNum && TryGetTeamIntel(self.team, enemy, now, known, &knownTime)
				&& now - knownTime < sneakIntelMs && DistanceSq2D(self.eye, known) < rangeSq)
			{
				return true;
			}
		}
		return false;
	}

	static bool TryGetEnemyIntel(int clientNum, const PlayerView& self, int enemy, int now, float outFeet[3])
	{
		const BotState& bot = bots[clientNum];
		if (bot.targetClient == enemy && bot.noTraceTime == 0)
		{
			FeetOf(ReadPlayerView(enemy), outFeet);
			return true;
		}
		if (bot.huntKnownClient == enemy && bot.huntKnownTime != 0 && now - bot.huntKnownTime < intelMaxAgeMs)
		{
			outFeet[0] = bot.huntKnownPos[0];
			outFeet[1] = bot.huntKnownPos[1];
			outFeet[2] = bot.huntKnownPos[2];
			return true;
		}
		int intelTime = 0;
		if (TryGetTeamIntel(self.team, enemy, now, outFeet, &intelTime))
		{
			return true;
		}
		int shotTime = 0;
		return LastShotOf(enemy, outFeet, &shotTime) && now - shotTime < huntShotMemoryMs;
	}


	int ForcedTargetFor(int clientNum, const PlayerView& self)
	{
		const int forced = tuning.forceTarget;
		if (forced < 0 || forced >= maxClients || forced == clientNum)
		{
			return -1;
		}
		if (!IsEnemy(self, ReadPlayerView(forced)))
		{
			return -1;
		}
		return forced;
	}

	static int openingEndTime = 0;

	void StartOpening(int now)
	{
		if (openingEndTime != 0)
		{
			return;
		}
		openingEndTime = now + openingMs;
		BotLog("opening: bots head for the middle until %d", openingEndTime);
	}

	void ResetOpening()
	{
		openingEndTime = 0;
	}

	bool IsOpening(int now)
	{
		return openingEndTime != 0 && now < openingEndTime;
	}

	static float cachedMiddle[3] = {};
	static int cachedMiddleCount = -1;

	static bool GetMapMiddle(float out[3])
	{
		const int count = Waypoints::Count();
		if (!Waypoints::IsLoaded() || count <= 0)
		{
			return false;
		}
		if (count == cachedMiddleCount)
		{
			out[0] = cachedMiddle[0];
			out[1] = cachedMiddle[1];
			out[2] = cachedMiddle[2];
			return true;
		}
		double sumX = 0.0;
		double sumY = 0.0;
		double sumZ = 0.0;
		for (int n = 0; n < count; ++n)
		{
			const float* origin = Waypoints::Origin(n);
			sumX += origin[0];
			sumY += origin[1];
			sumZ += origin[2];
		}
		out[0] = static_cast<float>(sumX / count);
		out[1] = static_cast<float>(sumY / count);
		out[2] = static_cast<float>(sumZ / count);
		cachedMiddle[0] = out[0];
		cachedMiddle[1] = out[1];
		cachedMiddle[2] = out[2];
		cachedMiddleCount = count;
		return true;
	}

	bool PickOpeningPoint(const float* feet, float out[3])
	{
		const int count = Waypoints::Count();
		if (!Waypoints::IsLoaded() || count <= 0)
		{
			return false;
		}

		if (IsObjectiveMode() && ObjectiveCount() > 0)
		{
			int best = -1;
			float bestSq = 0.0f;
			for (int i = 0; i < ObjectiveCount(); ++i)
			{
				const ObjectivePoint& point = ObjectiveAt(i);
				if (!point.isActive)
				{
					continue;
				}
				const float distanceSq = DistanceSq2D(feet, point.origin);
				if (best < 0 || distanceSq < bestSq)
				{
					best = i;
					bestSq = distanceSq;
				}
			}
			if (best >= 0)
			{
				out[0] = ObjectiveAt(best).origin[0];
				out[1] = ObjectiveAt(best).origin[1];
				out[2] = ObjectiveAt(best).origin[2];
				return true;
			}
		}

		float middle[3];
		if (!GetMapMiddle(middle))
		{
			return false;
		}

		for (int attempt = 0; attempt < openingSpreadTries; ++attempt)
		{
			const int node = static_cast<int>(NextRand() % static_cast<unsigned int>(count));
			if (!IsLaneNode(node) || DistanceSq2D(Waypoints::Origin(node), middle) > openingSpreadSq)
			{
				continue;
			}
			const float* origin = Waypoints::Origin(node);
			out[0] = origin[0];
			out[1] = origin[1];
			out[2] = origin[2];
			return true;
		}

		const int nearest = Waypoints::Nearest(middle);
		if (nearest < 0)
		{
			return false;
		}
		const float* origin = Waypoints::Origin(nearest);
		out[0] = origin[0];
		out[1] = origin[1];
		out[2] = origin[2];
		return true;
	}


	int PickHuntTarget(int clientNum, const PlayerView& self)
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		const int maxFollow = tuning.maxFollow;
		int candidates[maxClients];
		int weights[maxClients];
		int count = 0;
		int totalWeight = 0;
		const float viewYaw = ViewYawOf(clientNum);
		const int now = ServerTimeMs();
		int nearestFar = -1;
		float nearestFarDistance = 0.0f;

		const int forced = ForcedTargetFor(clientNum, self);
		if (forced >= 0)
		{
			return forced;
		}

		for (int i = 0; i < numClients && count < maxClients; ++i)
		{
			if (i == clientNum)
			{
				continue;
			}
			const PlayerView other = ReadPlayerView(i);
			if (!other.isPlaying)
			{
				ForgetIntel(i);
				continue;
			}
			if (!IsEnemy(self, other))
			{
				continue;
			}
			if (tuning.ignoreHumans != 0 && IsHuman(i))
			{
				continue;
			}
			if (maxFollow > 0 && HuntersOf(clientNum, i) >= maxFollow)
			{
				continue;
			}

			float knownFeet[3];
			if (!TryGetEnemyIntel(clientNum, self, i, now, knownFeet))
			{
				continue;
			}

			const float distance = std::sqrt(DistanceSq2D(self.eye, knownFeet));
			if (distance > huntMaxUnits)
			{
				if (nearestFar < 0 || distance < nearestFarDistance)
				{
					nearestFar = i;
					nearestFarDistance = distance;
				}
				continue;
			}
			const float nearness = huntNearUnits / (huntNearUnits + distance);
			int rangeWeight = static_cast<int>(100.0f * nearness * nearness);
			if (ConeDot(self.eye, knownFeet, viewYaw, 0.0f) < behindDot)
			{
				rangeWeight = rangeWeight * behindHuntWeightPercent / 100;
			}
			if (rangeWeight < 1)
			{
				rangeWeight = 1;
			}

			candidates[count] = i;
			weights[count] = rangeWeight;
			totalWeight += weights[count];
			++count;
		}

		if (count == 0)
		{
			if (nearestFar >= 0)
			{
				BotLog("hunt client %d has nobody within %.0f, reaches for client %d at %.0f", clientNum,
					huntMaxUnits, nearestFar, nearestFarDistance);
			}
			return nearestFar;
		}

		int roll = static_cast<int>(NextRand() % static_cast<unsigned int>(totalWeight));
		for (int k = 0; k < count; ++k)
		{
			roll -= weights[k];
			if (roll < 0)
			{
				return candidates[k];
			}
		}
		return candidates[count - 1];
	}


	static float contactPoint[2][3] = {};
	static int contactTime[2] = {};

	static bool GetTeamFront(int team, int now, float out[3])
	{
		const int lane = HeatLane(team);
		if (contactTime[lane] != 0 && now >= contactTime[lane] && now - contactTime[lane] < contactMemoryMs)
		{
			out[0] = contactPoint[lane][0];
			out[1] = contactPoint[lane][1];
			out[2] = contactPoint[lane][2];
			return true;
		}
		return GetMapMiddle(out);
	}

	bool IsPlayableToward(const PlayerView& self, float yawDeg)
	{
		const float rad = yawDeg / radToDeg;
		const float point[3] = { self.eye[0] + std::cos(rad) * playableProbeUnits,
								 self.eye[1] + std::sin(rad) * playableProbeUnits, self.feetZ };
		const int node = Waypoints::Nearest(point);
		if (node < 0)
		{
			return true;
		}
		const float* origin = Waypoints::Origin(node);
		return DistanceSq2D(origin, point) < playableNodeSq && std::fabs(origin[2] - point[2]) < playableNodeDz;
	}

	bool TryGetFrontYaw(int team, const float* from, float* outYaw)
	{
		float front[3];
		if (!GetTeamFront(team, ServerTimeMs(), front))
		{
			return false;
		}
		*outYaw = std::atan2(front[1] - from[1], front[0] - from[0]) * radToDeg;
		return true;
	}

	bool TryGetThreatLookYaw(int clientNum, const PlayerView& self, float* outYaw)
	{
		const int areaCount = Waypoints::AreaCount();
		if (areaCount == 0)
		{
			return false;
		}
		const int here = AreaOfClient(clientNum);
		if (here < 0)
		{
			return false;
		}
		const int now = ServerTimeMs();
		float front[3];
		const bool hasFront = GetTeamFront(self.team, now, front);
		float frontYaw = 0.0f;
		if (hasFront)
		{
			frontYaw = std::atan2(front[1] - self.eye[1], front[0] - self.eye[0]) * radToDeg;
		}

		float known[maxClients][3];
		int knownCount = 0;
		if (self.team != 0)
		{
			for (int enemy = 0; enemy < maxClients; ++enemy)
			{
				int knownTime = 0;
				if (enemy != clientNum && TryGetTeamIntel(self.team, enemy, now, known[knownCount], &knownTime)
					&& now - knownTime < exposureMemoryMs)
				{
					++knownCount;
				}
			}
		}

		const float lookMinSq = threatLookMinUnits * threatLookMinUnits;
		const float lookMaxSq = threatLookMaxUnits * threatLookMaxUnits;
		float bestScore = 0.0f;
		float bestYaw = 0.0f;
		for (int area = 0; area < areaCount; ++area)
		{
			if (area == here || !Waypoints::AreasSee(here, area))
			{
				continue;
			}
			const Waypoints::AreaInfo* info = Waypoints::AreaAt(area);
			const float* target = Waypoints::Origin(info->node);
			const float distanceSq = DistanceSq2D(self.eye, target);
			if (distanceSq < lookMinSq || distanceSq > lookMaxSq)
			{
				continue;
			}
			const float yaw = std::atan2(target[1] - self.eye[1], target[0] - self.eye[0]) * radToDeg;
			float score = static_cast<float>(info->memberCount) * (1.0f + AreaDanger(area));
			if (hasFront)
			{
				score *= 1.0f + std::cos(AngleDelta(frontYaw, yaw) / radToDeg);
			}
			for (int k = 0; k < knownCount; ++k)
			{
				if (DistanceSq2D(known[k], target) < threatKnownRangeSq)
				{
					score *= threatKnownScale;
				}
			}
			if (score > bestScore)
			{
				bestScore = score;
				bestYaw = yaw;
			}
		}
		if (bestScore <= 0.0f)
		{
			return false;
		}
		*outYaw = bestYaw;
		return true;
	}

	float PickLookYaw(int clientNum, const PlayerView& self)
	{
		const int now = ServerTimeMs();
		const float viewYaw = ViewYawOf(clientNum);
		float front[3];
		const bool hasFront = GetTeamFront(self.team, now, front);
		float frontYaw = viewYaw;
		if (hasFront)
		{
			frontYaw = std::atan2(front[1] - self.eye[1], front[0] - self.eye[0]) * radToDeg;
		}
		float threatYaw = 0.0f;
		const bool hasThreatYaw = TryGetThreatLookYaw(clientNum, self, &threatYaw);

		float bestYaw = viewYaw;
		float bestScore = -1.0e9f;
		for (int k = 0; k < lookYawSamples; ++k)
		{
			const float yaw = viewYaw + static_cast<float>(k) * (360.0f / static_cast<float>(lookYawSamples));
			float score = 0.0f;
			if (!IsPlayableToward(self, yaw))
			{
				score -= 2.0f;
			}
			else if (IsViewOpen(clientNum, self, yaw, lookOpenFarUnits))
			{
				score += 2.0f;
			}
			else if (IsViewOpen(clientNum, self, yaw, lookOpenNearUnits))
			{
				score += 1.0f;
			}
			if (hasFront)
			{
				score += std::cos(AngleDelta(frontYaw, yaw) / radToDeg) * lookFrontWeight;
			}
			if (hasThreatYaw)
			{
				score += std::cos(AngleDelta(threatYaw, yaw) / radToDeg) * lookThreatWeight;
			}
			if (score > bestScore)
			{
				bestScore = score;
				bestYaw = yaw;
			}
		}
		return bestYaw;
	}

	void ForgetTeamKnowledge()
	{
		for (int lane = 0; lane < 2; ++lane)
		{
			for (int enemy = 0; enemy < maxClients; ++enemy)
			{
				teamIntel[lane][enemy] = EnemyIntel{};
			}
			contactPoint[lane][0] = 0.0f;
			contactPoint[lane][1] = 0.0f;
			contactPoint[lane][2] = 0.0f;
			contactTime[lane] = 0;
		}
	}


	bool HuntTargetFeet(int clientNum, const PlayerView& self, float out[3])
	{
		BotState& bot = bots[clientNum];
		const int hunted = TaskHuntClient(clientNum);
		if (hunted < 0 || hunted == clientNum)
		{
			return false;
		}
		const PlayerView target = ReadPlayerView(hunted);
		if (!IsEnemy(self, target))
		{
			return false;
		}
		const int now = ServerTimeMs();
		if (bot.huntKnownClient != hunted)
		{
			bot.huntKnownClient = hunted;
			bot.huntKnownTime = 0;
		}

		const char* source = nullptr;
		if ((bot.targetClient == hunted && bot.noTraceTime == 0) || hunted == ForcedTargetFor(clientNum, self))
		{
			FeetOf(target, bot.huntKnownPos);
			bot.huntKnownTime = now;
			bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
		}
		else
		{
			float intelFeet[3];
			int intelTime = 0;
			if (TryGetTeamIntel(self.team, hunted, now, intelFeet, &intelTime) && intelTime > bot.huntKnownTime)
			{
				bot.huntKnownPos[0] = intelFeet[0];
				bot.huntKnownPos[1] = intelFeet[1];
				bot.huntKnownPos[2] = intelFeet[2];
				bot.huntKnownTime = intelTime;
				bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
				source = "callout";
			}
			float shotFeet[3];
			int shotTime = 0;
			if (source == nullptr && LastShotOf(hunted, shotFeet, &shotTime) && now - shotTime < huntShotMemoryMs
				&& shotTime > bot.huntKnownTime)
			{
				const float bearing = Flrand(0.0f, 6.2831853f);
				const float fuzz = Flrand(0.0f, radarFuzzUnits);
				bot.huntKnownPos[0] = shotFeet[0] + std::cos(bearing) * fuzz;
				bot.huntKnownPos[1] = shotFeet[1] + std::sin(bearing) * fuzz;
				bot.huntKnownPos[2] = shotFeet[2];
				bot.huntKnownTime = shotTime;
				bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
				source = "gunfire";
			}
		}
		if (bot.huntKnownTime == 0)
		{
			return false;
		}
		if (source != nullptr)
		{
			BotLog("hunt client %d places client %d by %s at %.0f %.0f %.0f", clientNum, hunted, source,
				bot.huntKnownPos[0], bot.huntKnownPos[1], bot.huntKnownPos[2]);
		}

		out[0] = bot.huntKnownPos[0];
		out[1] = bot.huntKnownPos[1];
		out[2] = bot.huntKnownPos[2];
		const int age = now - bot.huntKnownTime;
		const int rememberMs = bot.skillRow.rememberTime;
		if (age > rememberMs)
		{
			float share = static_cast<float>(age - rememberMs) / static_cast<float>(huntDriftFullMs);
			if (share > 1.0f)
			{
				share = 1.0f;
			}
			const float drift = huntDriftMaxUnits * share;
			out[0] += std::cos(bot.huntDriftYaw) * drift;
			out[1] += std::sin(bot.huntDriftYaw) * drift;
		}
		return true;
	}


	void OnTargetLost(int clientNum, int lostClient, const float* lastSeenPos)
	{
		BotState& bot = bots[clientNum];
		const Personality& personality = bot.personality;
		const int now = ServerTimeMs();
		const int lane = HeatLane(ReadPlayerView(clientNum).team);
		contactPoint[lane][0] = lastSeenPos[0];
		contactPoint[lane][1] = lastSeenPos[1];
		contactPoint[lane][2] = lastSeenPos[2];
		contactTime[lane] = now;

		const float lastSeenFeet[3] = { lastSeenPos[0], lastSeenPos[1], bot.lastSeenFeetZ };
		if (lostClient >= 0)
		{
			bot.huntKnownClient = lostClient;
			bot.huntKnownPos[0] = lastSeenFeet[0];
			bot.huntKnownPos[1] = lastSeenFeet[1];
			bot.huntKnownPos[2] = lastSeenFeet[2];
			bot.huntKnownTime = now;
			bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
		}

		if (tuning.type == 1 && lostClient >= 0 && !bot.isRoamer)
		{
			const bool onSpell = bot.task.kind == TaskHold || bot.task.kind == TaskFollow
				|| bot.task.kind == TaskDefend || bot.task.kind == TaskCapture;
			if (!onSpell || RollPercent(personality.aggression))
			{
				RequestHunt(clientNum, lostClient, now + huntRetargetMinMs, PriorityPlan, "lost");
			}
		}

		bot.alertUntilTime = now + alertMs;
		bot.noSprintUntilTime = now + noSprintAfterMs;

		if (!RollPercent(ScaleByTrait(tuning.push, personality.aggression)))
		{
			return;
		}

		const Task lostHunt = bot.task;
		int searchAreas[searchSpots];
		const int searchCount = FindHiddenAreas(AreaOfClient(clientNum), lastSeenFeet, searchRadiusUnits, searchAreas,
												searchSpots);
		float pushPoint[3] = { lastSeenFeet[0], lastSeenFeet[1], lastSeenFeet[2] };
		if (searchCount > 0)
		{
			const float* spot = Waypoints::Origin(Waypoints::AreaAt(searchAreas[0])->node);
			pushPoint[0] = spot[0];
			pushPoint[1] = spot[1];
			pushPoint[2] = spot[2];
		}
		if (RequestPush(clientNum, pushPoint, PriorityLost, "lost"))
		{
			if (lostHunt.kind == TaskHunt && lostHunt.client == lostClient && bot.interrupted.kind == TaskNone)
			{
				bot.interrupted = lostHunt;
			}
			for (int k = 0; k < searchCount; ++k)
			{
				bot.searchAreas[k] = searchAreas[k];
			}
			bot.searchCount = searchCount;
			bot.searchIndex = 0;
			bot.searchSerial = bot.task.serial;
			bot.searchUntilTime = now + searchMs;
			BotLog("push client %d after losing client %d at %.0f %.0f %.0f toward %.0f %.0f %.0f (%d hidden spots)",
				clientNum, lostClient, lastSeenPos[0], lastSeenPos[1], lastSeenPos[2], pushPoint[0], pushPoint[1],
				pushPoint[2], searchCount);
		}
	}

	bool TryContinueSearch(int clientNum)
	{
		BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();
		if (bot.task.kind != TaskPush || bot.task.serial != bot.searchSerial || bot.targetClient >= 0
			|| now >= bot.searchUntilTime || bot.searchIndex + 1 >= bot.searchCount)
		{
			return false;
		}
		++bot.searchIndex;
		const int area = bot.searchAreas[bot.searchIndex];
		if (area < 0 || area >= Waypoints::AreaCount())
		{
			return false;
		}
		const float* spot = Waypoints::Origin(Waypoints::AreaAt(area)->node);
		if (!RequestPush(clientNum, spot, bot.task.priority, "search"))
		{
			return false;
		}
		bot.searchSerial = bot.task.serial;
		BotLog("search client %d checks hidden spot %d of %d at %.0f %.0f %.0f", clientNum, bot.searchIndex + 1,
			bot.searchCount, spot[0], spot[1], spot[2]);
		return true;
	}


	int PickRoamGoal(int clientNum, const PlayerView& self, const float* feet, int start)
	{
		const int maxFollow = tuning.maxFollow;
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		const float* heat = HeatPenalty(self.team);
		const float viewYaw = ViewYawOf(clientNum);
		const int now = ServerTimeMs();
		const int lane = HeatLane(self.team);
		const bool hasContact = contactTime[lane] != 0 && now - contactTime[lane] < contactMemoryMs;
		int fallback = -1;
		int best = -1;
		float bestHeat = 0.0f;
		bool bestIsAhead = false;
		bool bestNearContact = false;
		bool bestTowardFront = false;
		float front[3];
		const bool hasFront = GetTeamFront(self.team, now, front);
		float selfToFrontSq = 0.0f;
		if (hasFront)
		{
			selfToFrontSq = DistanceSq2D(feet, front);
		}

		for (int attempt = 0; attempt < roamGoalTries; ++attempt)
		{
			const int node = static_cast<int>(NextRand() % static_cast<unsigned int>(Waypoints::Count()));
			if (node == start || !IsLaneNode(node))
			{
				continue;
			}

			const float* origin = Waypoints::Origin(node);
			const float distanceSq = DistanceSq2D(feet, origin);
			if (distanceSq > roamMaxSq)
			{
				continue;
			}

			fallback = node;
			if (distanceSq < roamMinSq)
			{
				continue;
			}
			const bool isAhead = ConeDot(feet, origin, viewYaw, 0.0f) >= behindDot;

			int sharing = 0;
			for (int k = 0; maxFollow > 0 && k < numClients; ++k)
			{
				if (k == clientNum || !bots[k].input.isActive || bots[k].goalNode < 0)
				{
					continue;
				}
				if (DistanceSq2D(Waypoints::Origin(bots[k].goalNode), origin) > roamShareRadiusSq)
				{
					continue;
				}
				const PlayerView other = ReadPlayerView(k);
				if (other.isPlaying && (self.team == 0 || other.team == self.team))
				{
					++sharing;
				}
			}
			if (maxFollow > 0 && sharing >= maxFollow)
			{
				continue;
			}

			const float nodeHeatHere = heat ? heat[node] : 0.0f;
			const bool nearContact = hasContact && DistanceSq2D(origin, contactPoint[lane]) < contactRangeSq;
			const bool towardFront = !hasFront || DistanceSq2D(origin, front) < selfToFrontSq;
			bool beatsBest = best < 0;
			if (!beatsBest && towardFront != bestTowardFront)
			{
				beatsBest = towardFront;
			}
			else if (!beatsBest)
			{
				beatsBest = (isAhead && !bestIsAhead)
					|| (isAhead == bestIsAhead
						&& ((nearContact && !bestNearContact) || (nearContact == bestNearContact && nodeHeatHere < bestHeat)));
			}
			if (beatsBest)
			{
				best = node;
				bestHeat = nodeHeatHere;
				bestNearContact = nearContact;
				bestIsAhead = isAhead;
				bestTowardFront = towardFront;
			}
		}
		return best >= 0 ? best : fallback;
	}


	int PickStartNode(const float* feet, float forwardYaw, bool preferAhead)
	{
		if (!preferAhead)
		{
			return Waypoints::Nearest(feet);
		}

		const float rad = forwardYaw / radToDeg;
		const float forwardX = std::cos(rad);
		const float forwardY = std::sin(rad);
		int best = -1;
		float bestCost = 0.0f;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				continue;
			}
			const float* origin = Waypoints::Origin(n);
			float cost = GraphDistanceSq(feet, origin);
			const float ahead = (origin[0] - feet[0]) * forwardX + (origin[1] - feet[1]) * forwardY;
			if (ahead < -behindUnits)
			{
				cost += behindPenaltySq;
			}
			if (std::fabs(origin[2] - feet[2]) > startRiseMaxUnits)
			{
				cost += behindPenaltySq;
			}
			if (best < 0 || cost < bestCost)
			{
				best = n;
				bestCost = cost;
			}
		}
		return best;
	}


	static bool IsStairNode(int node)
	{
		const float* origin = Waypoints::Origin(node);
		const int links = Waypoints::ChildCount(node);
		for (int k = 0; k < links; ++k)
		{
			const float* next = Waypoints::Origin(Waypoints::ChildAt(node, k));
			const float rise = std::fabs(next[2] - origin[2]);
			const float run = std::sqrt(DistanceSq2D(origin, next));
			if (rise > stairRiseUnits && rise > run * stairSlope)
			{
				return true;
			}
		}
		return false;
	}


	static bool IsWallCover(const unsigned char* reach, const unsigned char* share, int direction)
	{
		return reach[direction] != Waypoints::wallReachOpen
			&& static_cast<float>(reach[direction]) * Waypoints::wallReachUnit <= coverWallReachUnits
			&& share[direction] <= coverWallShareMax;
	}

	static bool HasCrouchCover(int node, float yawDeg)
	{
		const Waypoints::NodeWalls* walls = Waypoints::WallsOf(node);
		if (!walls)
		{
			return false;
		}
		const int direction = Waypoints::WallDirectionOf(yawDeg);
		for (int offset = -1; offset <= 1; ++offset)
		{
			const int k = (direction + offset + Waypoints::wallDirections) % Waypoints::wallDirections;
			if (IsWallCover(walls->crouchReach, walls->crouchShare, k))
			{
				return true;
			}
		}
		return false;
	}

	static bool HasLowCover(int node, float yawDeg)
	{
		const Waypoints::NodeWalls* walls = Waypoints::WallsOf(node);
		if (!walls)
		{
			return false;
		}
		const int k = Waypoints::WallDirectionOf(yawDeg);
		const float standUnits = static_cast<float>(walls->standReach[k]) * Waypoints::wallReachUnit;
		const bool isStandOpen = walls->standReach[k] == Waypoints::wallReachOpen || standUnits >= lowCoverOpenUnits;
		return isStandOpen && IsWallCover(walls->crouchReach, walls->crouchShare, k);
	}

	static bool isAreaWatched[Waypoints::maxAreas] = {};

	static int MarkWatchedAreas(int clientNum, int team)
	{
		const int areaCount = Waypoints::AreaCount();
		for (int area = 0; area < areaCount; ++area)
		{
			isAreaWatched[area] = false;
		}
		if (team == 0)
		{
			return 0;
		}
		int watchers = 0;
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int k = 0; k < numClients && k < maxClients; ++k)
		{
			if (k == clientNum || !bots[k].input.isActive)
			{
				continue;
			}
			const Task& other = bots[k].task;
			if ((other.kind != TaskHold && other.kind != TaskDefend) || other.node < 0 || ReadPlayerView(k).team != team)
			{
				continue;
			}
			const int mateArea = Waypoints::AreaOf(other.node);
			if (mateArea < 0)
			{
				continue;
			}
			++watchers;
			const float* origin = Waypoints::Origin(other.node);
			for (int area = 0; area < areaCount; ++area)
			{
				if (isAreaWatched[area] || !Waypoints::AreasSee(mateArea, area))
				{
					continue;
				}
				const float* target = Waypoints::Origin(Waypoints::AreaAt(area)->node);
				const float yaw = std::atan2(target[1] - origin[1], target[0] - origin[0]) * radToDeg;
				if (std::fabs(AngleDelta(other.lookYaw, yaw)) <= watchConeDeg)
				{
					isAreaWatched[area] = true;
				}
			}
		}
		return watchers;
	}

	static int PickHoldNodeByAreas(int clientNum, const float* around, float minUnits, float maxUnits, int watchArea,
								   float* outLookYaw)
	{
		const int areaCount = Waypoints::AreaCount();
		if (areaCount == 0)
		{
			return -1;
		}
		const BotState& bot = bots[clientNum];
		const bool isSniper = bot.personality.archetype == ArchetypeSniper;
		float sightMin = holdSampleMinUnits;
		float sightMax = holdSampleMaxUnits;
		if (isSniper)
		{
			sightMin = sniperSightMinUnits;
			sightMax = sniperSightMaxUnits;
		}
		const float sightMinSq = sightMin * sightMin;
		const float sightMaxSq = sightMax * sightMax;
		const float minSq = minUnits * minUnits;
		const float maxSq = maxUnits * maxUnits;

		int pool[holdPoolSize];
		int poolCount = 0;
		int eligible = 0;
		const int nodeCount = Waypoints::Count();
		for (int node = 0; node < nodeCount; ++node)
		{
			if (Waypoints::ChildCount(node) < holdMinLinks || Waypoints::TypeOf(node) != Waypoints::NodeStand)
			{
				continue;
			}
			const float distanceSq = DistanceSq2D(around, Waypoints::Origin(node));
			if (distanceSq < minSq || distanceSq > maxSq)
			{
				continue;
			}
			if (bot.hasRelocateFrom
				&& DistanceSq2D(bot.relocateFrom, Waypoints::Origin(node)) < relocateSpacingUnits * relocateSpacingUnits)
			{
				continue;
			}
			const int area = Waypoints::AreaOf(node);
			if (area < 0 || (watchArea >= 0 && !Waypoints::AreasSee(area, watchArea)))
			{
				continue;
			}
			++eligible;
			if (poolCount < holdPoolSize)
			{
				pool[poolCount] = node;
				++poolCount;
				continue;
			}
			const int slot = static_cast<int>(NextRand() % static_cast<unsigned int>(eligible));
			if (slot < holdPoolSize)
			{
				pool[slot] = node;
			}
		}
		for (int k = poolCount - 1; k > 0; --k)
		{
			const int swapWith = static_cast<int>(NextRand() % static_cast<unsigned int>(k + 1));
			const int kept = pool[k];
			pool[k] = pool[swapWith];
			pool[swapWith] = kept;
		}

		const int team = ReadPlayerView(clientNum).team;
		const int watchers = MarkWatchedAreas(clientNum, team);
		float front[3];
		const bool hasFront = GetTeamFront(team, ServerTimeMs(), front);
		int bestNode = -1;
		float bestScore = 0.0f;
		float bestYaw = 0.0f;
		bool bestHasCover = false;
		int candidates = 0;
		for (int pick = 0; pick < poolCount && candidates < holdCandidates; ++pick)
		{
			const int node = pool[pick];
			const float* origin = Waypoints::Origin(node);
			if (IsStairNode(node) || IsHoldSpotTaken(clientNum, origin))
			{
				continue;
			}
			++candidates;
			const int area = Waypoints::AreaOf(node);

			const float frontYaw = std::atan2(front[1] - origin[1], front[0] - origin[0]) * radToDeg;
			float rawScore = 0.0f;
			float score = 0.0f;
			float lookWeight = 0.0f;
			float lookYaw = 0.0f;
			for (int other = 0; other < areaCount; ++other)
			{
				if (other == area || !Waypoints::AreasSee(area, other))
				{
					continue;
				}
				const Waypoints::AreaInfo* info = Waypoints::AreaAt(other);
				const float* target = Waypoints::Origin(info->node);
				const float sightSq = DistanceSq2D(origin, target);
				if (sightSq < sightMinSq || sightSq > sightMaxSq)
				{
					continue;
				}
				const float yaw = std::atan2(target[1] - origin[1], target[0] - origin[0]) * radToDeg;
				if (hasFront && std::fabs(AngleDelta(frontYaw, yaw)) > holdFrontConeDeg)
				{
					continue;
				}
				float weight = static_cast<float>(info->memberCount);
				if (isSniper)
				{
					weight *= std::sqrt(sightSq) / sightMin;
				}
				weight *= 1.0f + memoryLookWeight * AreaDanger(other);
				rawScore += weight;
				if (isAreaWatched[other])
				{
					weight *= watchedAreaShare;
				}
				score += weight;
				if (weight > lookWeight)
				{
					lookWeight = weight;
					lookYaw = yaw;
				}
			}
			if (rawScore < holdMinAreaScore)
			{
				continue;
			}
			score *= 1.0f + memoryHoldWeight * (AreaStrength(area) - AreaDanger(area));
			const bool hasCover = HasLowCover(node, lookYaw);
			if (hasCover)
			{
				score *= holdCoverBonus;
			}
			if (score > bestScore)
			{
				bestNode = node;
				bestScore = score;
				bestYaw = lookYaw;
				bestHasCover = hasCover;
			}
		}
		if (bestNode < 0)
		{
			return -1;
		}
		bots[clientNum].hasRelocateFrom = false;
		*outLookYaw = bestYaw;
		const char* coverNote = "";
		if (bestHasCover)
		{
			coverNote = " over low cover";
		}
		const char* watchNote = "";
		if (watchArea >= 0)
		{
			watchNote = ", in sight of the objective";
		}
		BotLog("holdspot client %d node %d watches %.0f of open floor, look %.0f%s%s (%d spots, %d teammates holding)",
			clientNum, bestNode, bestScore, bestYaw, coverNote, watchNote, eligible, watchers);
		return bestNode;
	}

	int PickDefendNode(int clientNum, const float* objective, float minUnits, float maxUnits, float* outLookYaw)
	{
		if (!Waypoints::IsLoaded() || Waypoints::Count() == 0)
		{
			return -1;
		}
		if (Waypoints::AreaCount() > 0)
		{
			const int objectiveArea = Waypoints::AreaOf(Waypoints::Nearest(objective));
			if (objectiveArea >= 0)
			{
				const int node = PickHoldNodeByAreas(clientNum, objective, minUnits, maxUnits, objectiveArea, outLookYaw);
				if (node >= 0)
				{
					return node;
				}
			}
		}
		return PickHoldNode(clientNum, objective, minUnits, maxUnits, outLookYaw);
	}

	int PickHoldNode(int clientNum, const float* around, float minUnits, float maxUnits, float* outLookYaw)
	{
		if (!Waypoints::IsLoaded() || Waypoints::Count() == 0)
		{
			return -1;
		}
		if (Waypoints::AreaCount() > 0)
		{
			return PickHoldNodeByAreas(clientNum, around, minUnits, maxUnits, -1, outLookYaw);
		}
		const float minSq = minUnits * minUnits;
		const float maxSq = maxUnits * maxUnits;
		const float sampleMinSq = holdSampleMinUnits * holdSampleMinUnits;
		const float sampleMaxSq = holdSampleMaxUnits * holdSampleMaxUnits;

		int bestNode = -1;
		int bestScore = -1;
		float bestReach = 0.0f;
		float bestYaw = 0.0f;
		int candidates = 0;

		float front[3];
		const bool hasFront = GetTeamFront(ReadPlayerView(clientNum).team, ServerTimeMs(), front);
		for (int attempt = 0; attempt < holdCandidateTries && candidates < holdCandidates; ++attempt)
		{
			const int node = static_cast<int>(NextRand() % static_cast<unsigned int>(Waypoints::Count()));
			if (Waypoints::ChildCount(node) < holdMinLinks || Waypoints::TypeOf(node) != Waypoints::NodeStand
				|| IsStairNode(node))
			{
				continue;
			}
			const float* origin = Waypoints::Origin(node);
			const float distanceSq = DistanceSq2D(around, origin);
			if (distanceSq < minSq || distanceSq > maxSq || IsHoldSpotTaken(clientNum, origin))
			{
				continue;
			}
			++candidates;

			const float head[3] = { origin[0], origin[1], origin[2] + coverEyeRise };
			int score = 0;
			float reach = 0.0f;
			float yaw = 0.0f;
			int samples = 0;
			const float frontYaw = std::atan2(front[1] - origin[1], front[0] - origin[0]) * radToDeg;
			for (int tries = 0; tries < holdSampleTries && samples < holdSamples; ++tries)
			{
				const int sample = static_cast<int>(NextRand() % static_cast<unsigned int>(Waypoints::Count()));
				if (sample == node || Waypoints::ChildCount(sample) == 0)
				{
					continue;
				}
				const float* sampleOrigin = Waypoints::Origin(sample);
				const float sampleSq = DistanceSq2D(origin, sampleOrigin);
				if (sampleSq < sampleMinSq || sampleSq > sampleMaxSq)
				{
					continue;
				}
				const float sampleYaw = std::atan2(sampleOrigin[1] - origin[1], sampleOrigin[0] - origin[0]) * radToDeg;
				if (hasFront && std::fabs(AngleDelta(frontYaw, sampleYaw)) > holdFrontConeDeg)
				{
					continue;
				}
				++samples;
				const float target[3] = { sampleOrigin[0], sampleOrigin[1], sampleOrigin[2] + coverEyeRise };
				if (!SightLine(head, target, entityNumNone))
				{
					continue;
				}
				++score;
				if (sampleSq > reach)
				{
					reach = sampleSq;
					yaw = std::atan2(sampleOrigin[1] - origin[1], sampleOrigin[0] - origin[0]) * radToDeg;
				}
			}

			if (score > bestScore || (score == bestScore && reach > bestReach))
			{
				bestNode = node;
				bestScore = score;
				bestReach = reach;
				bestYaw = yaw;
			}
		}

		if (bestNode < 0 || bestScore < holdMinSeen)
		{
			BotLog("holdspot client %d none worth holding (best sees %d/%d)", clientNum, bestScore, holdSamples);
			return -1;
		}
		*outLookYaw = bestYaw;
		BotLog("holdspot client %d node %d sees %d/%d look %.0f", clientNum, bestNode, bestScore, holdSamples, bestYaw);
		return bestNode;
	}


	static int PickFollowGoal(const float* feet, const float* mateFeet)
	{
		const float minSq = followStandoffUnits * followStandoffUnits;
		const float maxSq = followGoalMaxUnits * followGoalMaxUnits;

		float trail[3] = { mateFeet[0], mateFeet[1], mateFeet[2] };
		const float toBotX = feet[0] - mateFeet[0];
		const float toBotY = feet[1] - mateFeet[1];
		const float toBotLength = std::sqrt(toBotX * toBotX + toBotY * toBotY);
		if (toBotLength > 1.0f)
		{
			trail[0] += toBotX / toBotLength * followStandoffUnits;
			trail[1] += toBotY / toBotLength * followStandoffUnits;
		}

		int best = -1;
		float bestSq = 0.0f;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (!IsLaneNode(n))
			{
				continue;
			}
			const float* origin = Waypoints::Origin(n);
			const float mateSq = DistanceSq2D(mateFeet, origin);
			if (mateSq < minSq || mateSq > maxSq || std::fabs(origin[2] - mateFeet[2]) > followGoalRiseUnits)
			{
				continue;
			}
			const float trailSq = DistanceSq2D(trail, origin);
			if (best < 0 || trailSq < bestSq)
			{
				best = n;
				bestSq = trailSq;
			}
		}
		if (best < 0)
		{
			return Waypoints::Nearest(mateFeet);
		}
		return best;
	}


	bool RequestCover(int clientNum, const PlayerView& self, const float* threatEye)
	{
		if (!Waypoints::IsLoaded())
		{
			return false;
		}

		float feet[3];
		FeetOf(self, feet);

		int candidates[coverCandidates];
		float candidateSq[coverCandidates];
		int count = 0;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				continue;
			}
			const float distanceSq = GraphDistanceSq(feet, Waypoints::Origin(n));
			if (distanceSq > coverSearchSq || (count == coverCandidates && distanceSq >= candidateSq[count - 1]))
			{
				continue;
			}

			int slot = count < coverCandidates ? count : coverCandidates - 1;
			while (slot > 0 && candidateSq[slot - 1] > distanceSq)
			{
				candidates[slot] = candidates[slot - 1];
				candidateSq[slot] = candidateSq[slot - 1];
				--slot;
			}
			candidates[slot] = n;
			candidateSq[slot] = distanceSq;
			if (count < coverCandidates)
			{
				++count;
			}
		}

		int passes = 1;
		if (Waypoints::HasWalls())
		{
			passes = 2;
		}
		for (int pass = 0; pass < passes; ++pass)
		{
			const bool needsWall = passes == 2 && pass == 0;
			for (int k = 0; k < count; ++k)
			{
				const float* origin = Waypoints::Origin(candidates[k]);
				if (DistanceSq2D(feet, origin) < coverMinMoveUnits * coverMinMoveUnits)
				{
					continue;
				}
				const float lookYaw = std::atan2(threatEye[1] - origin[1], threatEye[0] - origin[0]) * radToDeg;
				if (needsWall && !HasCrouchCover(candidates[k], lookYaw))
				{
					continue;
				}
				const float head[3] = { origin[0], origin[1], origin[2] + coverEyeRise };
				if (SightLine(threatEye, head, entityNumNone))
				{
					continue;
				}
				if (Navgen::PenetrationShare(threatEye, head, coverThreatPenetrateType, 1.0f) > coverShareMax)
				{
					continue;
				}

				if (!RequestCoverAt(clientNum, origin, lookYaw))
				{
					return false;
				}
				const char* coverKind = "out of sight";
				if (needsWall)
				{
					coverKind = "behind a wall";
				}
				BotLog("cover client %d runs for node %d at %.0f %.0f %.0f, %s", clientNum, candidates[k],
					origin[0], origin[1], origin[2], coverKind);
				return true;
			}
		}
		return false;
	}


	GoalKind ChooseGoal(int clientNum, const PlayerView& self, const float* feet, int start,
						int* outGoal, const char** outKind)
	{
		BotState& bot = bots[clientNum];
		Task& task = bot.task;
		const int now = ServerTimeMs();
		*outGoal = -1;
		*outKind = "roam";

		switch (task.kind)
		{
		case TaskRoam:
		{
			int goal = -1;
			if (task.node >= 0 && Waypoints::ChildCount(task.node) > 0)
			{
				goal = task.node;
				*outKind = task.source;
			}
			else
			{
				goal = PickRoamGoal(clientNum, self, feet, start);
				if (goal >= 0)
				{
					task.node = goal;
					task.source = "roam";
					bot.stopgapGoal = goal;
				}
			}
			if (goal < 0)
			{
				return GoalNone;
			}
			if (goal == start)
			{
				OnGoalReached(clientNum);
				return GoalNone;
			}
			*outGoal = goal;
			return GoalNode;
		}

		case TaskHunt:
		{
			float enemyFeet[3];
			if (!HuntTargetFeet(clientNum, self, enemyFeet))
			{
				if (bot.huntKnownClient == task.client && bot.huntKnownTime == 0)
				{
					OnGoalFailed(clientNum, "no intel");
				}
				return GoalNone;
			}
			if (now - bot.huntKnownTime > huntDryMinAgeMs && bot.targetClient != task.client
				&& DistanceSq2D(feet, enemyFeet) < huntDryRangeSq)
			{
				BotLog("hunt client %d finds nobody where it thought client %d was", clientNum, task.client);
				OnGoalFailed(clientNum, "trail cold");
				return GoalNone;
			}
			const int goal = PickHuntGoal(enemyFeet);
			*outKind = "hunt";
			if (goal < 0)
			{
				OnGoalFailed(clientNum, "no node near the enemy");
				return GoalNone;
			}
			if (goal != start)
			{
				*outGoal = goal;
				return GoalNode;
			}

			const float enemyDistSq = DistanceSq2D(feet, enemyFeet);
			if (enemyDistSq < pursuitMaxDistSq)
			{
				BotLog("pursue client %d from node %d enemy at %.0f %.0f %.0f dist %.0f", clientNum,
					start, enemyFeet[0], enemyFeet[1], enemyFeet[2], std::sqrt(enemyDistSq));
				bot.pursuitPoint[0] = enemyFeet[0];
				bot.pursuitPoint[1] = enemyFeet[1];
				bot.pursuitPoint[2] = enemyFeet[2];
				bot.pursuitUntilTime = now + pursuitMs;
				return GoalPursuit;
			}
			BotLog("hunt client %d paused at node %d: enemy %.0f away with no nearer node", clientNum,
				start, std::sqrt(enemyDistSq));
			OnGoalFailed(clientNum, "enemy off the graph");
			return GoalNone;
		}

		case TaskPush:
		case TaskCover:
		{
			if (task.node < 0 || Waypoints::ChildCount(task.node) == 0)
			{
				task.node = NearestSeeingNode(task.point);
			}
			const int goal = task.node;
			*outKind = task.kind == TaskCover ? "cover" : "push";
			if (goal < 0 || goal == start)
			{
				OnGoalReached(clientNum);
				return GoalNone;
			}
			*outGoal = goal;
			return GoalNode;
		}

		case TaskHold:
		case TaskDefend:
		case TaskCapture:
		{
			const int goal = task.node;
			*outKind = TaskName(task.kind);
			if (goal < 0 || Waypoints::ChildCount(goal) == 0)
			{
				OnGoalFailed(clientNum, "no goal node");
				return GoalNone;
			}
			if (goal == start)
			{
				OnGoalReached(clientNum);
				return GoalNone;
			}
			*outGoal = goal;
			return GoalNode;
		}

		case TaskFollow:
		{
			const PlayerView mate = task.client >= 0 ? ReadPlayerView(task.client) : PlayerView{};
			if (!mate.isPlaying)
			{
				return GoalNone;
			}
			float mateFeet[3];
			FeetOf(mate, mateFeet);
			const float mateDistSq = DistanceSq2D(feet, mateFeet);
			if (mateDistSq < followStandoffUnits * followStandoffUnits)
			{
				bot.isFollowWaiting = true;
			}
			if (bot.isFollowWaiting && mateDistSq < followResumeUnits * followResumeUnits)
			{
				return GoalNone;
			}
			bot.isFollowWaiting = false;
			const int goal = PickFollowGoal(feet, mateFeet);
			*outKind = "follow";
			if (goal < 0 || goal == start)
			{
				return GoalNone;
			}
			bot.followMatePos[0] = mateFeet[0];
			bot.followMatePos[1] = mateFeet[1];
			bot.followMatePos[2] = mateFeet[2];
			*outGoal = goal;
			return GoalNode;
		}

		default:
		{
			const int goal = PickRoamGoal(clientNum, self, feet, start);
			if (goal < 0 || goal == start)
			{
				return GoalNone;
			}
			*outGoal = goal;
			return GoalNode;
		}
		}
	}


	bool ReplanTask(int clientNum, const PlayerView& self, int now)
	{
		BotState& bot = bots[clientNum];
		if (bot.pathLength <= 0)
		{
			return false;
		}

		if (bot.pathTaskSerial != bot.task.serial)
		{
			BotLog("replan client %d: %s replaced the path's task", clientNum, TaskLabel(clientNum));
			DropPath(bot);
			return true;
		}

		if (bot.task.kind == TaskHunt)
		{
			const PlayerView hunted = bot.task.client >= 0 ? ReadPlayerView(bot.task.client) : PlayerView{};
			if (!IsEnemy(self, hunted))
			{
				BotLog("replan client %d: hunted client %d is gone", clientNum, bot.task.client);
				DropPath(bot);
				return true;
			}
			if (now < bot.huntReplanTime || bot.goalNode < 0)
			{
				return false;
			}
			bot.huntReplanTime = now + tuning.huntReplan;

			float huntedFeet[3];
			if (!HuntTargetFeet(clientNum, self, huntedFeet))
			{
				DropPath(bot);
				return true;
			}
			const float movedSq = DistanceSq2D(huntedFeet, Waypoints::Origin(bot.goalNode));
			if (movedSq < huntReplanMoveSq || Waypoints::Nearest(huntedFeet) == bot.goalNode)
			{
				return false;
			}
			BotLog("replan client %d: client %d moved %.0f from node %d", clientNum, bot.task.client,
				std::sqrt(movedSq), bot.goalNode);
			DropPath(bot);
			return true;
		}

		if (bot.task.kind == TaskFollow)
		{
			const PlayerView mate = bot.task.client >= 0 ? ReadPlayerView(bot.task.client) : PlayerView{};
			if (!mate.isPlaying)
			{
				DropPath(bot);
				return true;
			}
			if (now < bot.huntReplanTime || bot.goalNode < 0)
			{
				return false;
			}
			bot.huntReplanTime = now + tuning.huntReplan;

			float mateFeet[3];
			FeetOf(mate, mateFeet);
			const float movedSq = DistanceSq2D(mateFeet, bot.followMatePos);
			if (movedSq < followReplanMoveSq)
			{
				return false;
			}
			BotLog("replan client %d: teammate %d walked %.0f from where the route was built", clientNum,
				bot.task.client, std::sqrt(movedSq));
			DropPath(bot);
			return true;
		}

		return false;
	}
}
