#include "Components/Modules/BotAI/Control/Tasks.hpp"
#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/Control/Objectives.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Components::BotAI
{
	static constexpr int   huntRetargetSpanMs   = 20000;


	static constexpr int   holdMinMs           = 3500;
	static constexpr int   holdMaxMs           = 7000;
	static constexpr int   sniperHoldMinMs     = 6000;
	static constexpr int   sniperHoldMaxMs     = 12000;
	static constexpr float holdSearchMinUnits  = 400.0f;
	static constexpr float holdSearchMaxUnits  = 1600.0f;
	static constexpr float holdSpreadUnits     = 300.0f;
	static constexpr int   relocateKills       = 2;
	static constexpr int   relocateKillsSniper = 3;
	static constexpr int   relocateBasePercent = 50;
	static constexpr int   relocateDelayMinMs  = 400;
	static constexpr int   relocateDelayMaxMs  = 1200;
	static constexpr float holdSweepDeg        = 20.0f;
	static constexpr float holdScopedSweepDeg  = 8.0f;
	static constexpr float captureSweepDeg     = 60.0f;
	static constexpr int   holdSweepPeriodMs   = 9000;
	static constexpr int   holdGlanceGapMinMs  = 2500;
	static constexpr int   holdGlanceGapMaxMs  = 5000;
	static constexpr int   holdGlanceHoldMinMs = 500;
	static constexpr int   holdGlanceHoldMaxMs = 1000;
	static constexpr float holdGlanceYawMinDeg = 30.0f;
	static constexpr float holdGlanceYawMaxDeg = 50.0f;
	static constexpr float holdGlanceOpenUnits = 300.0f;

	static constexpr int   quickscoperHoldPercent = 15;
	static constexpr int   anchorHoldBasePercent = 25;
	static constexpr int   holdPickBasePercent = 20;
	static constexpr int   arrivalHoldPercent  = 30;
	static constexpr int   arrivalHoldMinMs    = 2500;
	static constexpr int   arrivalHoldMaxMs    = 6000;
	static constexpr float carryOnMinSq = 600.0f * 600.0f;
	static constexpr float stopgapKeepSq = 300.0f * 300.0f;
	static constexpr int   forcedHuntMs = 30000;

	static constexpr int   followMinMs         = 15000;
	static constexpr int   followMaxMs         = 30000;
	static constexpr float followPickUnits     = 1500.0f;

	static constexpr float flankRoamMinUnits   = 1500.0f;
	static constexpr float flankRoamMaxUnits   = 3000.0f;
	static constexpr int   flankRoamTries      = 16;

	static constexpr int   stopgapRoamMs       = 3000;
	static constexpr int   taskRestMs          = 2000;
	static constexpr int   overdueMs           = 90000;

	static constexpr int   captureHoldMs       = 15000;
	static constexpr int   defendHoldMs        = 40000;
	static constexpr float defendSpotMinUnits  = 100.0f;
	static constexpr float defendSpotMaxUnits  = 500.0f;

	static constexpr int roamBiasByArchetype[ArchetypeCount] = { 50, 80, 120, 150, 100, 100, 90, 40, 50 };

	static constexpr float laneScaleMin        = 0.8f;
	static constexpr float laneScaleMax        = 1.3f;
	static constexpr int   arriveLookMinMs     = 500;
	static constexpr int   arriveLookMaxMs     = 1200;
	static constexpr int   arriveLookPercent   = 60;
	static constexpr float holdSweepHarmonic   = 2.7f;
	static constexpr float holdSweepHarmonicShare = 0.3f;


	const char* TaskName(int kind)
	{
		static const char* const nameTable[TaskKindCount] = {
			"none", "roam", "hunt", "push", "cover", "hold", "follow", "capture", "defend",
		};
		if (kind < 0 || kind >= TaskKindCount)
		{
			return "?";
		}
		return nameTable[kind];
	}


	static Task MakeTask(int kind, int priority, const char* source)
	{
		Task task = {};
		task.kind = kind;
		task.priority = priority;
		task.source = source;
		return task;
	}


	static bool TimesFromArrival(int kind)
	{
		return kind == TaskHold || kind == TaskDefend || kind == TaskCapture || kind == TaskCover;
	}


	static void LogTask(int clientNum, const Task& task, const char* how)
	{
		const int now = ServerTimeMs();
		switch (task.kind)
		{
		case TaskHunt:
		case TaskFollow:
			BotLog("task client %d %s %s client %d for %d ms (%s)", clientNum, how, TaskName(task.kind),
				task.client, task.until ? task.until - now : 0, task.source);
			break;
		case TaskPush:
		case TaskCover:
			BotLog("task client %d %s %s at %.0f %.0f %.0f (%s)", clientNum, how, TaskName(task.kind),
				task.point[0], task.point[1], task.point[2], task.source);
			break;
		case TaskHold:
		case TaskDefend:
			BotLog("task client %d %s %s node %d look %.0f for %d ms (%s)", clientNum, how, TaskName(task.kind),
				task.node, task.lookYaw, task.until, task.source);
			break;
		case TaskCapture:
			BotLog("task client %d %s capture %s node %d (%s)", clientNum, how,
				task.objective >= 0 ? ObjectiveAt(task.objective).name : "?", task.node, task.source);
			break;
		default:
			BotLog("task client %d %s %s%s (%s)", clientNum, how, TaskName(task.kind),
				task.node >= 0 ? " to a chosen node" : "", task.source);
			break;
		}
	}


	static void SetTask(int clientNum, const Task& task, const char* how)
	{
		BotState& bot = bots[clientNum];
		bot.task = task;
		bot.task.serial = ++bot.taskSerial;
		bot.task.stage = 0;
		bot.taskStartTime = ServerTimeMs();
		if (task.kind == TaskFollow)
		{
			bot.followLookNextTime = 0;
		}
		LogTask(clientNum, bot.task, how);
	}


	static void EndTask(int clientNum, const char* why)
	{
		BotState& bot = bots[clientNum];
		if (bot.task.kind == TaskNone)
		{
			return;
		}
		BotLog("task client %d ends %s (%s)", clientNum, TaskName(bot.task.kind), why);
		bot.task = Task{};

		if (bot.interrupted.kind == TaskNone)
		{
			return;
		}
		Task resumed = bot.interrupted;
		bot.interrupted = Task{};
		const int now = ServerTimeMs();
		const bool hasTime = resumed.until == 0 || resumed.stage == 0 || resumed.until > now + 3000;
		if (hasTime)
		{
			if (resumed.stage == 1 && TimesFromArrival(resumed.kind))
			{
				resumed.until = resumed.until - now;
			}
			SetTask(clientNum, resumed, "resumes");
		}
	}


	static void Queue(BotState& bot, const Task& task)
	{
		if (bot.queueCount < taskQueue)
		{
			bot.queue[bot.queueCount] = task;
			++bot.queueCount;
		}
	}


	static bool PopQueue(int clientNum)
	{
		BotState& bot = bots[clientNum];
		if (bot.queueCount == 0)
		{
			return false;
		}

		Task next = bot.queue[0];
		for (int i = 1; i < bot.queueCount; ++i)
		{
			bot.queue[i - 1] = bot.queue[i];
		}
		--bot.queueCount;

		if (next.until > 0 && !TimesFromArrival(next.kind))
		{
			next.until += ServerTimeMs();
		}
		SetTask(clientNum, next, "starts");
		return true;
	}


	static int HoldDurationMs(const Personality& personality, bool isSniper)
	{
		const int base = isSniper ? IrandMs(sniperHoldMinMs, sniperHoldMaxMs) : IrandMs(holdMinMs, holdMaxMs);
		return base * (50 + personality.caution) / 100;
	}


	static bool MakeHoldTask(int clientNum, const float* around, float minUnits, float maxUnits,
							 int durationMs, const char* source, Task* out)
	{
		float lookYaw = 0.0f;
		const int node = PickHoldNode(clientNum, around, minUnits, maxUnits, &lookYaw);
		if (node < 0)
		{
			return false;
		}
		*out = MakeTask(TaskHold, PriorityPlan, source);
		out->node = node;
		out->lookYaw = lookYaw;
		out->until = durationMs;
		return true;
	}


	static int PickFollowTarget(int clientNum, const PlayerView& self)
	{
		if (self.team == 0)
		{
			return -1;
		}
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		const float pickSq = followPickUnits * followPickUnits;
		int candidates[maxClients];
		int weights[maxClients];
		int count = 0;
		int totalWeight = 0;
		for (int k = 0; k < numClients && count < maxClients; ++k)
		{
			if (k == clientNum)
			{
				continue;
			}
			const PlayerView mate = ReadPlayerView(k);
			if (!mate.isPlaying || mate.team != self.team || DistanceSq2D(self.eye, mate.eye) > pickSq)
			{
				continue;
			}

			if (bots[k].task.kind == TaskFollow && bots[k].task.client == clientNum)
			{
				continue;
			}
			candidates[count] = k;
			weights[count] = IsHuman(k) ? 3 : 1;
			totalWeight += weights[count];
			++count;
		}
		if (count == 0)
		{
			return -1;
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


	static int DrawNodeInBand(const PlayerView& self, const float* feet, float minUnits, float maxUnits, int tries)
	{
		if (!Waypoints::IsLoaded() || Waypoints::Count() == 0)
		{
			return -1;
		}
		const float* heat = HeatPenalty(self.team);
		const float minSq = minUnits * minUnits;
		const float maxSq = maxUnits * maxUnits;
		int best = -1;
		float bestHeat = 0.0f;
		for (int attempt = 0; attempt < tries; ++attempt)
		{
			const int node = static_cast<int>(NextRand() % static_cast<unsigned int>(Waypoints::Count()));
			if (!IsLaneNode(node))
			{
				continue;
			}
			const float distanceSq = DistanceSq2D(feet, Waypoints::Origin(node));
			if (distanceSq < minSq || distanceSq > maxSq)
			{
				continue;
			}
			const float nodeHeat = heat ? heat[node] : 0.0f;
			if (best < 0 || nodeHeat < bestHeat)
			{
				best = node;
				bestHeat = nodeHeat;
			}
		}
		return best;
	}


	void PlanLife(int clientNum, const PlayerView& self)
	{
		BotState& bot = bots[clientNum];
		const Personality& personality = bot.personality;
		const int now = ServerTimeMs();
		float feet[3];
		FeetOf(self, feet);

		bot.queueCount = 0;
		bot.task = Task{};
		bot.interrupted = Task{};
		bot.taskPickNotBeforeTime = 0;
		bot.lanePenaltyScale = Flrand(laneScaleMin, laneScaleMax);

		const int roamBias = roamBiasByArchetype[personality.archetype];
		bot.isRoamer = tuning.type == 1 && RollPercent(tuning.roam * roamBias / 100);

		float openingPoint[3];
		const bool hasOpening = IsOpening(now) && PickOpeningPoint(feet, openingPoint);
		if (hasOpening)
		{
			Task opening = MakeTask(TaskPush, PriorityPlan, "opening");
			opening.point[0] = openingPoint[0];
			opening.point[1] = openingPoint[1];
			opening.point[2] = openingPoint[2];
			Queue(bot, opening);
		}

		if (bot.hasDeathPos && bot.lastAttacker >= 0 && RollPercent(ScaleByTrait(tuning.revenge, personality.aggression)))
		{
			const int killer = bot.lastAttacker;
			if (IsEnemy(self, ReadPlayerView(killer)))
			{
				if (tuning.type == 1 && !bot.isRoamer)
				{
					Task hunt = MakeTask(TaskHunt, PriorityPlan, "revenge");
					hunt.client = killer;
					hunt.until = revengeHuntMinMs
						+ static_cast<int>(NextRand() % static_cast<unsigned int>(revengeHuntSpanMs));
					Queue(bot, hunt);
					bot.huntKnownClient = killer;
					bot.huntKnownPos[0] = bot.deathPos[0];
					bot.huntKnownPos[1] = bot.deathPos[1];
					bot.huntKnownPos[2] = bot.deathPos[2];
					bot.huntKnownTime = now;
					bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
				}
				else
				{
					Task push = MakeTask(TaskPush, PriorityPlan, "revenge");
					push.point[0] = bot.deathPos[0];
					push.point[1] = bot.deathPos[1];
					push.point[2] = bot.deathPos[2];
					Queue(bot, push);
				}
				BotLog("revenge client %d goes after client %d", clientNum, killer);
			}
		}
		bot.hasDeathPos = false;
		bot.lastAttacker = -1;

		if (IsObjectiveMode() && ObjectiveCount() > 0 && RollPercent(personality.objectiveDrive))
		{
			const int target = NearestObjectiveNotOwned(feet, self.team);
			if (target >= 0 && ObjectiveAt(target).node >= 0)
			{
				Task capture = MakeTask(TaskCapture, PriorityPlan, "plan");
				capture.objective = target;
				capture.node = ObjectiveAt(target).node;
				capture.until = captureHoldMs;
				Queue(bot, capture);
			}
			const int own = NearestObjectiveOwned(feet, self.team);
			if (own >= 0)
			{
				float lookYaw = 0.0f;
				const int node = PickDefendNode(clientNum, ObjectiveAt(own).origin, defendSpotMinUnits, defendSpotMaxUnits,
												&lookYaw);
				if (node >= 0)
				{
					Task defend = MakeTask(TaskDefend, PriorityPlan, "plan");
					defend.node = node;
					defend.lookYaw = lookYaw;
					defend.until = defendHoldMs;
					defend.objective = own;
					Queue(bot, defend);
				}
			}
		}

		switch (personality.archetype)
		{
		case ArchetypeAnchor:
		{
			const float* holdAround = feet;
			if (hasOpening)
			{
				holdAround = openingPoint;
			}
			Task hold;
			if (MakeHoldTask(clientNum, holdAround, holdSearchMinUnits, holdSearchMaxUnits,
							 HoldDurationMs(personality, false), "plan", &hold))
			{
				Queue(bot, hold);
			}
			break;
		}
		case ArchetypeFlanker:
		{
			const int farNode = DrawNodeInBand(self, feet, flankRoamMinUnits, flankRoamMaxUnits, flankRoamTries);
			if (farNode >= 0)
			{
				Task flank = MakeTask(TaskRoam, PriorityPlan, "flank");
				flank.node = farNode;
				Queue(bot, flank);
			}
			break;
		}
		case ArchetypeSniper:
		{
			if (!personality.isQuickscoper)
			{
				Task hold;
				if (MakeHoldTask(clientNum, feet, holdSearchMinUnits, holdSearchMaxUnits,
								 HoldDurationMs(personality, true), "plan", &hold))
				{
					Queue(bot, hold);
				}
			}
			break;
		}
		case ArchetypeSupport:
		{
			const int mate = PickFollowTarget(clientNum, self);
			if (mate >= 0)
			{
				Task follow = MakeTask(TaskFollow, PriorityPlan, "plan");
				follow.client = mate;
				follow.until = IrandMs(followMinMs, followMaxMs);
				Queue(bot, follow);
			}
			break;
		}
		default:
			break;
		}

		if (debugOn)
		{
			char plan[128] = "";
			for (int i = 0; i < bot.queueCount; ++i)
			{
				_snprintf_s(plan + std::strlen(plan), sizeof(plan) - std::strlen(plan), _TRUNCATE, "%s%s",
					i ? ", " : "", TaskName(bot.queue[i].kind));
			}
			BotLog("plan client %d %s%s lane %.2f: %s", clientNum, ArchetypeName(personality.archetype),
				bot.isRoamer ? " roamer" : "", bot.lanePenaltyScale, bot.queueCount ? plan : "the picker's draw");
		}
		(void)now;
	}


	static void PickTask(int clientNum, const PlayerView& self)
	{
		BotState& bot = bots[clientNum];
		const Personality& personality = bot.personality;
		const int now = ServerTimeMs();
		float feet[3];
		FeetOf(self, feet);

		const bool isSniper = personality.archetype == ArchetypeSniper;
		const bool holdsLong = isSniper && !personality.isQuickscoper;
		int holdChance = holdPickBasePercent + personality.caution / 3;
		if (personality.isQuickscoper)
		{
			holdChance = 0;
			if (isSniper)
			{
				holdChance = quickscoperHoldPercent;
			}
		}
		if (personality.archetype == ArchetypeAnchor)
		{
			holdChance = anchorHoldBasePercent + personality.caution / 2;
		}
		if (RollPercent(holdChance))
		{
			Task hold;
			if (MakeHoldTask(clientNum, feet, holdSearchMinUnits, holdSearchMaxUnits,
							 HoldDurationMs(personality, holdsLong), "picked", &hold))
			{
				SetTask(clientNum, hold, "starts");
				return;
			}
		}

		if (RollPercent(personality.teamplay / 5))
		{
			const int mate = PickFollowTarget(clientNum, self);
			if (mate >= 0)
			{
				Task follow = MakeTask(TaskFollow, PriorityPlan, "picked");
				follow.client = mate;
				follow.until = now + IrandMs(followMinMs, followMaxMs);
				SetTask(clientNum, follow, "starts");
				return;
			}
		}

		const bool mayHunt = tuning.type == 1 && !bot.isRoamer && now >= bot.huntPauseUntilTime;
		if (mayHunt)
		{
			const int enemy = PickHuntTarget(clientNum, self);
			if (enemy >= 0)
			{
				Task hunt = MakeTask(TaskHunt, PriorityPlan, "picked");
				hunt.client = enemy;
				hunt.until = now + huntRetargetMinMs
					+ static_cast<int>(NextRand() % static_cast<unsigned int>(huntRetargetSpanMs));
				SetTask(clientNum, hunt, "starts");
				BotLog("hunt client %d picks client %d%s", clientNum, enemy, IsHuman(enemy) ? " (player)" : "");
				return;
			}

			Task stopgap = MakeTask(TaskRoam, PriorityPlan, "stopgap");
			stopgap.until = now + JitterMs(stopgapRoamMs);
			if (bot.stopgapGoal >= 0 && bot.stopgapGoal < Waypoints::Count()
				&& DistanceSq2D(self.eye, Waypoints::Origin(bot.stopgapGoal)) > stopgapKeepSq)
			{
				stopgap.node = bot.stopgapGoal;
			}
			SetTask(clientNum, stopgap, "starts");
			return;
		}

		Task roam = MakeTask(TaskRoam, PriorityPlan, "picked");
		SetTask(clientNum, roam, "starts");
	}


	void TickTasks(int clientNum, const PlayerView& self)
	{
		BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();

		if (!bot.lifePlanned)
		{
			bot.lifePlanned = true;
			PlanLife(clientNum, self);
		}

		if (bot.pendingHelpTime != 0 && now >= bot.pendingHelpTime)
		{
			bot.pendingHelpTime = 0;
			const int attacker = bot.pendingHelpAttacker;
			if (attacker >= 0 && IsEnemy(self, ReadPlayerView(attacker)))
			{
				bool isAnswered = false;
				if (tuning.type == 1 && !bot.isRoamer)
				{
					isAnswered = RequestHunt(clientNum, attacker, now + helpHuntMs, PriorityHelp, "help");
					if (isAnswered)
					{
						bot.huntKnownClient = attacker;
						bot.huntKnownPos[0] = bot.pendingHelpPoint[0];
						bot.huntKnownPos[1] = bot.pendingHelpPoint[1];
						bot.huntKnownPos[2] = bot.pendingHelpPoint[2];
						bot.huntKnownTime = now;
						bot.huntDriftYaw = Flrand(0.0f, 6.2831853f);
					}
				}
				else
				{
					isAnswered = RequestPush(clientNum, bot.pendingHelpPoint, PriorityHelp, "help");
				}
				if (isAnswered)
				{
					BotLog("help client %d answers client %d against client %d", clientNum, bot.pendingHelpFrom, attacker);
				}
			}
		}

		const int forced = ForcedTargetFor(clientNum, self);
		if (forced >= 0 && !(bot.task.kind == TaskHunt && bot.task.client == forced))
		{
			RequestHunt(clientNum, forced, now + forcedHuntMs, PriorityHelp, "forced");
		}

		Task& task = bot.task;

		switch (task.kind)
		{
		case TaskHunt:
		{
			const PlayerView hunted = task.client >= 0 ? ReadPlayerView(task.client) : PlayerView{};
			const bool isGone = task.client < 0 || task.client == clientNum || !IsEnemy(self, hunted)
				|| (tuning.ignoreHumans != 0 && IsHuman(task.client));
			if (!isGone && now < task.until)
			{
				break;
			}

			const bool hasKnownSpot = bot.huntKnownClient == task.client && bot.huntKnownTime != 0
				&& now - bot.huntKnownTime < intelMaxAgeMs && DistanceSq2D(self.eye, bot.huntKnownPos) > carryOnMinSq;
			const float knownSpot[3] = { bot.huntKnownPos[0], bot.huntKnownPos[1], bot.huntKnownPos[2] };
			if (isGone)
			{
				EndTask(clientNum, "target gone");
			}
			else
			{
				EndTask(clientNum, "re-roll");
			}
			if (hasKnownSpot && bot.task.kind == TaskNone)
			{
				Task carryOn = MakeTask(TaskPush, PriorityPlan, "carry on");
				carryOn.point[0] = knownSpot[0];
				carryOn.point[1] = knownSpot[1];
				carryOn.point[2] = knownSpot[2];
				SetTask(clientNum, carryOn, "starts");
			}
			break;
		}
		case TaskFollow:
		{
			const PlayerView mate = task.client >= 0 ? ReadPlayerView(task.client) : PlayerView{};
			if (task.client < 0 || task.client == clientNum || !mate.isPlaying
				|| (self.team != 0 && mate.team != self.team))
			{
				EndTask(clientNum, "teammate gone");
			}
			else if (task.client < clientNum && bots[task.client].task.kind == TaskFollow
					 && bots[task.client].task.client == clientNum)
			{
				EndTask(clientNum, "mutual follow");
			}
			else if (now >= task.until)
			{
				EndTask(clientNum, "time");
			}
			break;
		}
		case TaskRoam:
		case TaskPush:
			if (task.until != 0 && now >= task.until)
			{
				EndTask(clientNum, "time");
			}
			break;
		case TaskCover:
		case TaskHold:
		case TaskDefend:
		case TaskCapture:
			if ((task.kind == TaskCapture || task.kind == TaskDefend)
				&& (task.objective < 0 || task.objective >= ObjectiveCount() || !ObjectiveAt(task.objective).isActive))
			{
				EndTask(clientNum, "objective gone");
			}
			else if (task.stage == 1 && now >= task.until)
			{
				EndTask(clientNum, "time");
			}
			else if (task.stage == 0 && now - bot.taskStartTime > overdueMs)
			{
				EndTask(clientNum, "overdue");
			}
			break;
		default:
			break;
		}

		if (bot.task.kind != TaskNone || now < bot.taskPickNotBeforeTime)
		{
			return;
		}
		if (!PopQueue(clientNum))
		{
			PickTask(clientNum, self);
		}
	}


	bool RequestTask(int clientNum, const Task& task)
	{
		BotState& bot = bots[clientNum];
		const Task& current = bot.task;
		if (current.kind != TaskNone && task.priority < current.priority)
		{
			return false;
		}

		const bool isSpell = current.kind == TaskHold || current.kind == TaskFollow
			|| current.kind == TaskCapture || current.kind == TaskDefend;
		if (current.kind != TaskNone && current.priority == PriorityPlan && task.priority > PriorityPlan && isSpell)
		{
			bot.interrupted = current;
		}
		SetTask(clientNum, task, "takes over");
		return true;
	}


	bool RequestPush(int clientNum, const float* point, int priority, const char* source)
	{
		Task push = MakeTask(TaskPush, priority, source);
		push.point[0] = point[0];
		push.point[1] = point[1];
		push.point[2] = point[2];
		return RequestTask(clientNum, push);
	}


	bool RequestCoverAt(int clientNum, const float* point, float lookYaw)
	{
		Task cover = MakeTask(TaskCover, PriorityCover, "cover");
		cover.point[0] = point[0];
		cover.point[1] = point[1];
		cover.point[2] = point[2];
		cover.lookYaw = lookYaw;
		cover.until = bots[clientNum].personality.archetype == ArchetypeSniper
			? sniperCoverHoldMs
			: coverHoldMs;
		return RequestTask(clientNum, cover);
	}


	bool RequestHunt(int clientNum, int client, int untilTime, int priority, const char* source)
	{
		BotState& bot = bots[clientNum];
		if (bot.task.kind == TaskHunt && bot.task.client == client)
		{
			if (untilTime > bot.task.until)
			{
				bot.task.until = untilTime;
			}
			return true;
		}
		Task hunt = MakeTask(TaskHunt, priority, source);
		hunt.client = client;
		hunt.until = untilTime;
		return RequestTask(clientNum, hunt);
	}


	void OnGoalReached(int clientNum)
	{
		BotState& bot = bots[clientNum];
		Task& task = bot.task;
		const int now = ServerTimeMs();
		if (task.stage == 1)
		{
			return;
		}

		switch (task.kind)
		{
		case TaskRoam:
		case TaskPush:
			if (task.kind == TaskRoam)
			{
				bot.stopgapGoal = -1;
			}
			if (TryContinueSearch(clientNum))
			{
				break;
			}
			if (bot.goalNode >= 0 && !bot.personality.isMeleeOnly
				&& RollPercent(arrivalHoldPercent + bot.personality.caution / 4))
			{
				task.kind = TaskHold;
				task.node = bot.goalNode;
				task.stage = 1;
				task.source = "arrival";
				task.until = now + IrandMs(arrivalHoldMinMs, arrivalHoldMaxMs) * (50 + bot.personality.caution) / 100;
				task.lookYaw = PickLookYaw(clientNum, ReadPlayerView(clientNum));
				bot.holdCrouched = RollPercent(bot.personality.caution / 2);
				bot.holdSweepStartTime = now;
				bot.holdGlanceNextTime = now + IrandMs(holdGlanceGapMinMs, holdGlanceGapMaxMs);
				bot.holdGlanceUntilTime = 0;
				BotLog("hold client %d stops at node %d for %d ms on arrival", clientNum, task.node, task.until - now);
				break;
			}
			if (RollPercent(arriveLookPercent))
			{
				bot.arriveLookUntilTime = now + IrandMs(arriveLookMinMs, arriveLookMaxMs);
				bot.arriveLookStartTime = now;
				bot.arriveLookYaw = PickLookYaw(clientNum, ReadPlayerView(clientNum));
				bot.sweepSign = (NextRand() & 1u) ? 1.0f : -1.0f;
			}
			EndTask(clientNum, "arrived");
			break;
		case TaskCover:
			task.stage = 1;
			task.until = now + (task.until > 0 ? task.until : coverHoldMs);
			BotLog("cover client %d holds at %.0f %.0f %.0f", clientNum, task.point[0], task.point[1], task.point[2]);
			break;
		case TaskHold:
		case TaskDefend:
		case TaskCapture:
			task.stage = 1;
			task.until = now + (task.until > 0 ? task.until : holdMinMs);
			bot.holdCrouched = task.kind != TaskCapture && RollPercent(bot.personality.caution);
			bot.holdSweepStartTime = now;
			bot.holdGlanceNextTime = now + IrandMs(holdGlanceGapMinMs, holdGlanceGapMaxMs);
			bot.holdGlanceUntilTime = 0;
			BotLog("%s client %d at node %d for %d ms%s", TaskName(task.kind), clientNum, task.node,
				task.until - now, bot.holdCrouched ? " crouched" : "");
			break;
		default:
			break;
		}
	}


	void OnKillConfirmed(int clientNum)
	{
		BotState& bot = bots[clientNum];
		Task& task = bot.task;
		if ((task.kind != TaskHold && task.kind != TaskDefend) || task.stage != 1 || task.node < 0)
		{
			return;
		}
		if (bot.holdKillsNode != task.node)
		{
			bot.holdKillsNode = task.node;
			bot.holdKills = 0;
		}
		++bot.holdKills;
		int killLimit = relocateKills;
		if (bot.personality.archetype == ArchetypeSniper)
		{
			killLimit = relocateKillsSniper;
		}
		if (bot.holdKills < killLimit || !RollPercent(relocateBasePercent + bot.personality.caution / 2))
		{
			return;
		}
		const float* origin = Waypoints::Origin(task.node);
		bot.relocateFrom[0] = origin[0];
		bot.relocateFrom[1] = origin[1];
		bot.relocateFrom[2] = origin[2];
		bot.hasRelocateFrom = true;
		bot.holdKills = 0;
		const int now = ServerTimeMs();
		task.until = now + IrandMs(relocateDelayMinMs, relocateDelayMaxMs);
		BotLog("relocate client %d leaves node %d after %d kills from it", clientNum, task.node, killLimit);
	}

	void OnGoalFailed(int clientNum, const char* reason)
	{
		BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();
		if (bot.task.kind == TaskRoam)
		{
			bot.stopgapGoal = -1;
		}
		if (bot.task.kind == TaskHunt)
		{
			bot.huntPauseUntilTime = now + JitterMs(huntPauseMs);
		}
		EndTask(clientNum, reason);
		bot.taskPickNotBeforeTime = now + JitterMs(taskRestMs);
	}


	bool IsHolding(int clientNum)
	{
		const Task& task = bots[clientNum].task;
		if (task.stage != 1)
		{
			return false;
		}
		return task.kind == TaskCover || task.kind == TaskHold || task.kind == TaskDefend || task.kind == TaskCapture;
	}


	bool HoldView(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn)
	{
		if (!IsHolding(clientNum))
		{
			return false;
		}
		BotState& bot = bots[clientNum];
		const Task& task = bot.task;
		const Personality& personality = bot.personality;
		const int now = ServerTimeMs();

		if (task.kind == TaskCover)
		{
			if (allowTurn)
			{
				TurnView(clientNum, input, task.lookYaw, 0.0f, "cover");
			}
			input.buttons |= cmdButtonCrouch;
			return true;
		}

		const WeaponInfo weapon = ReadWeapon(PlayerStateOf(clientNum));
		const bool isSightGun = (weapon.weapClass == weapClassSniper && !personality.isQuickscoper)
			|| weapon.weapClass == weapClassRifle || weapon.weapClass == weapClassMg;
		const bool isScoped = task.kind != TaskCapture && weapon.index && isSightGun;
		float sweep = holdSweepDeg;
		if (task.kind == TaskCapture)
		{
			sweep = captureSweepDeg;
		}
		else if (isScoped)
		{
			sweep = holdScopedSweepDeg;
		}
		const float phase = static_cast<float>(now - bot.holdSweepStartTime) * (6.2831853f / static_cast<float>(holdSweepPeriodMs));
		float yaw = task.lookYaw + std::sin(phase) * sweep
			+ std::sin(phase * holdSweepHarmonic + personality.wobblePhase) * sweep * holdSweepHarmonicShare;
		if (!isScoped)
		{
			if (now < bot.holdGlanceUntilTime)
			{
				yaw += bot.holdGlanceYawDeg;
			}
			else if (now >= bot.holdGlanceNextTime)
			{
				bot.holdGlanceNextTime = now + IrandMs(holdGlanceGapMinMs, holdGlanceGapMaxMs);
				if (task.kind != TaskCapture)
				{
					bot.holdCrouched = RollPercent(personality.caution);
				}
				if (RollPercent(100 - personality.sightDiscipline))
				{
					float side = 1.0f;
					if ((NextRand() & 1u) != 0)
					{
						side = -1.0f;
					}
					const float amount = Flrand(holdGlanceYawMinDeg, holdGlanceYawMaxDeg);
					if (!IsViewOpen(clientNum, self, task.lookYaw + side * amount, holdGlanceOpenUnits))
					{
						side = -side;
					}
					if (IsViewOpen(clientNum, self, task.lookYaw + side * amount, holdGlanceOpenUnits))
					{
						bot.holdGlanceYawDeg = side * amount;
						bot.holdGlanceUntilTime = now + IrandMs(holdGlanceHoldMinMs, holdGlanceHoldMaxMs);
					}
				}
			}
		}

		if (allowTurn)
		{
			TurnView(clientNum, input, yaw, 0.0f, "hold");
		}
		if (bot.holdCrouched)
		{
			input.buttons |= cmdButtonCrouch;
		}
		if (isScoped)
		{
			input.buttons |= cmdButtonAds;
		}
		(void)self;
		return true;
	}


	int TaskHuntClient(int clientNum)
	{
		const Task& task = bots[clientNum].task;
		return task.kind == TaskHunt ? task.client : -1;
	}


	int HuntersOf(int clientNum, int enemy)
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		int hunters = 0;
		for (int k = 0; k < numClients; ++k)
		{
			if (k == clientNum || !bots[k].input.isActive || bots[k].isRoamer)
			{
				continue;
			}
			if (TaskHuntClient(k) == enemy)
			{
				++hunters;
			}
		}
		return hunters;
	}


	bool IsHoldSpotTaken(int clientNum, const float* origin)
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		const float spreadSq = holdSpreadUnits * holdSpreadUnits;
		for (int k = 0; k < numClients; ++k)
		{
			if (k == clientNum || !bots[k].input.isActive)
			{
				continue;
			}
			const Task& other = bots[k].task;
			if ((other.kind != TaskHold && other.kind != TaskDefend) || other.node < 0)
			{
				continue;
			}
			if (DistanceSq2D(Waypoints::Origin(other.node), origin) < spreadSq)
			{
				return true;
			}
		}
		return false;
	}


	const char* TaskLabel(int clientNum)
	{
		static char label[48];
		const Task& task = bots[clientNum].task;
		switch (task.kind)
		{
		case TaskNone:
			return "-";
		case TaskHunt:
			_snprintf_s(label, sizeof(label), _TRUNCATE, "hunt:%d", task.client);
			return label;
		case TaskFollow:
			_snprintf_s(label, sizeof(label), _TRUNCATE, "follow:%d", task.client);
			return label;
		case TaskHold:
		case TaskDefend:
		case TaskCapture:
			_snprintf_s(label, sizeof(label), _TRUNCATE, "%s:%d%s", TaskName(task.kind), task.node, task.stage ? "*" : "");
			return label;
		case TaskCover:
			return task.stage ? "cover*" : "cover";
		default:
			return TaskName(task.kind);
		}
	}
}
