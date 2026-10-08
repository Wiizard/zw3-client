#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <cmath>
#include <cstring>

namespace Components::BotAI
{
	static constexpr float smoothReachSq       = 200.0f * 200.0f;
	static constexpr float lineHoldUnits       = 18.0f;
	static constexpr int   startCandidateCount = 6;
	static constexpr float startSearchSq       = 300.0f * 300.0f;
	static constexpr float lineLookAheadUnits  = 48.0f;
	static constexpr int   smoothLookNodes     = 3;
	static constexpr int   smoothRecheckFrames = 4;
	static constexpr int   alertCheckFrames   = 3;

	static constexpr float alertAheadDot      = 0.9f;

	static constexpr int   alertHoldMs        = 2500;

	static constexpr float alertWalkScale     = 0.75f;

	static constexpr int   sprintWindowMinMs  = 2500;
	static constexpr int   sprintWindowMaxMs  = 5000;
	static constexpr int   sprintRestMinMs    = 500;
	static constexpr int   sprintRestMaxMs    = 1100;
	static constexpr float sprintHeadWanderDeg = 3.0f;

	static constexpr float sprintStraightDot  = 0.5f;

	static constexpr float arriveRadiusSq  = 48.0f * 48.0f;
	static constexpr float ladderFootArriveSq = 16.0f * 16.0f;
	static constexpr float climbArriveDz = 16.0f;
	static constexpr float stairArriveDz = 14.0f;
	static constexpr float stairRiseMin = 18.0f;
	static constexpr float stairSlopeMin = 0.35f;

	static constexpr float stuckHeatMinUnits = 90.0f;

	static constexpr int   stanceGraceFrames = 14;

	static constexpr float stuckMoveSq      = 4.0f * 4.0f;
	static constexpr float proneStuckMoveSq = 0.5f * 0.5f;
	static constexpr float exposureCautionDivisor = 60.0f;
	static constexpr float belowNodeDz = 18.0f;
	static constexpr float belowNodeSlope = 0.5f;
	static constexpr float fellBelowRouteDz = 40.0f;

	static constexpr float slowStuckMoveSq  = 1.5f * 1.5f;
	static constexpr int   stuckFrameLimit = 20;

	static constexpr int   rescueRepeats         = 10;
	static constexpr int   rescueRepeatsOffGraph = 20;
	static constexpr float rescueSpotSq          = 120.0f * 120.0f;
	static constexpr int   rescueWindowMs        = 8000;
	static constexpr float ladderDownSlack       = 8.0f;
	static constexpr int   sneakCheckMs          = 500;
	static constexpr float sneakRangeUnits       = 700.0f;
	static constexpr int   sneakBasePercent      = 40;
	static constexpr int   coastFrames           = 3;
	static constexpr int   escapeRepeatMin       = 2;
	static constexpr int   escapeDirections      = 8;
	static constexpr int   escapeMs              = 700;
	static constexpr float escapeProbeUnits      = 96.0f;
	static constexpr float escapeProbeHeight     = 30.0f;
	static constexpr int   offGraphRecheckFrames = 10;
	static constexpr float pursuitArriveSq      = 60.0f * 60.0f;
	static constexpr float steerAngleDeg        = 35.0f;
	static constexpr int   steerFramesMin       = 16;
	static constexpr int   steerFramesSpan      = 24;

	static constexpr int   repathGapFrames      = 4;

	static constexpr int   pathSteerFrames      = 10;

	static constexpr float bodyAheadDistSq      = 56.0f * 56.0f;

	static constexpr float bodyAheadDot         = 0.6f;

	static constexpr int   sidestepFrames       = 6;

	static constexpr int   glanceGapMinMs       = 3000;
	static constexpr int   glanceGapMaxMs       = 7000;
	static constexpr int   glanceHoldMinMs      = 300;
	static constexpr int   glanceHoldMaxMs      = 600;
	static constexpr float glanceYawMinDeg      = 25.0f;
	static constexpr float glanceYawMaxDeg      = 45.0f;
	static constexpr float glanceOpenUnits      = 350.0f;
	static constexpr float followOpenUnits      = 300.0f;
	static constexpr int   followLookMinMs      = 3000;
	static constexpr int   followLookMaxMs      = 6000;
	static constexpr float threatGlanceMinDeg   = 10.0f;
	static constexpr float threatGlanceMaxDeg   = 110.0f;
	static constexpr float threatGlanceOpenUnits = 500.0f;
	static constexpr int   threatGlanceHoldMinMs = 700;
	static constexpr int   threatGlanceHoldMaxMs = 1500;
	static constexpr int   huntFaceKnownMs      = 4000;
	static constexpr float huntFaceRangeSq      = 900.0f * 900.0f;
	static constexpr float huntFaceMaxOffDeg    = 100.0f;
	static constexpr float huntFaceOpenUnits    = 250.0f;
	static constexpr float yieldStillSpeedSq    = 60.0f * 60.0f;
	static constexpr float yieldRangeSq         = 90.0f * 90.0f;
	static constexpr float yieldHeightUnits     = 60.0f;
	static constexpr float yieldMateSpeed       = 80.0f;
	static constexpr float yieldHeadingDot      = 0.7f;
	static constexpr float yieldStepUnits       = 48.0f;
	static constexpr int   yieldMs              = 700;

	static constexpr float leadMaxDeg           = 40.0f;
	static constexpr float walkLookLeadDeg      = 30.0f;

	static constexpr int   lookAheadNodes       = 14;
	static constexpr float lookMaxSq            = 800.0f * 800.0f;
	static constexpr int   lookRecheckFrames    = 5;
	static constexpr float lookConeDeg = 60.0f;
	static constexpr int   lookTraces           = 4;

	static constexpr float walkPitchMinDeg    = -1.0f;
	static constexpr float walkPitchMaxDeg    = 4.0f;
	static constexpr float lookPitchDeadbandDeg = 3.0f;
	static constexpr int   walkPitchGapMinMs  = 3000;
	static constexpr int   walkPitchGapMaxMs  = 6000;


	using SV_TraceBox_t = void (__cdecl*)(void* results, const float* start, const float* end,
										  const float* bounds, const int* ignoreParams,
										  int contentmask, int locational,
										  unsigned char* priorityMap, int staticmodels);

	static constexpr float probeReach = 44.0f;
	static constexpr float lowFaceProbeHeight = 24.0f;
	static constexpr float climbReachMargin = 16.0f;
	static constexpr float climbFaceReachSq = 160.0f * 160.0f;
	static constexpr float ladderCenterUnits = 20.0f;
	static constexpr float gapProbeHeight = 40.0f;
	static constexpr float gapProbeReach = 96.0f;
	static constexpr float narrowGapWidth = 90.0f;
	static constexpr float hallWidth = 160.0f;
	static constexpr float wallKeepUnits = 48.0f;
	static constexpr float wallLookAheadUnits = 80.0f;
	static constexpr float wallMaxCorrectionDeg = 20.0f;
	static constexpr float wallKeepNodeSq = 64.0f * 64.0f;
	static constexpr float gapLookAheadUnits = 40.0f;
	static constexpr float queueWalkScale = 0.3f;
	static constexpr float smoothFlatDz = 12.0f;

	static constexpr int   sprintLookNodes = 3;
	static constexpr float sprintAlignDeg  = 15.0f;

	static constexpr int   alertCautionMin = 65;
	static constexpr int   knifeSprintPercent = 95;

	static constexpr int   pauseGapMinMs        = 10000;
	static constexpr int   pauseGapMaxMs        = 25000;
	static constexpr int   pauseMinMs           = 350;
	static constexpr int   pauseMaxMs           = 700;
	static constexpr int   pauseChancePercent   = 12;
	static constexpr float pauseGlanceMinDeg    = 30.0f;
	static constexpr float pauseGlanceMaxDeg    = 70.0f;
	static constexpr int   cornerSlowFrames     = 5;
	static constexpr int   cornerSlowPercent    = 35;
	static constexpr float cornerWalkScale      = 0.65f;
	static constexpr float cornerLeadDeg        = 70.0f;
	static constexpr float turnFirstLookDeg     = 75.0f;
	static constexpr float turnFirstStopDeg     = 110.0f;
	static constexpr float turnFirstSlowDeg     = 70.0f;
	static constexpr float turnFirstWalkScale   = 0.35f;
	static constexpr int   revealLookNodes      = 6;
	static constexpr float revealMinUnits       = 150.0f;
	static constexpr float revealMaxUnits       = 1500.0f;
	static constexpr float revealConeDeg        = 75.0f;
	static constexpr int   revealMinMembers     = 3;
	static constexpr int   shoulderCheckPercent = 8;
	static constexpr float shoulderFrontSafeDeg = 70.0f;
	static constexpr float shoulderYawMinDeg    = 100.0f;
	static constexpr float shoulderYawMaxDeg    = 160.0f;
	static constexpr int   shoulderHoldMinMs    = 400;
	static constexpr int   shoulderHoldMaxMs    = 700;
	static constexpr int   walkScaleGapMinMs    = 1200;
	static constexpr int   walkScaleGapMaxMs    = 3000;
	static constexpr float walkScaleMin         = 0.92f;
	static constexpr float arriveSweepDeg       = 60.0f;
	static constexpr int   arriveSweepPeriodMs  = 2400;

	static void ProbeLine(const PlayerView& self, float travelYaw, float heightAboveFeet,
						  float outStart[3], float outEnd[3])
	{
		const float rad = travelYaw / radToDeg;
		outStart[0] = self.eye[0];
		outStart[1] = self.eye[1];
		outStart[2] = self.feetZ + heightAboveFeet;
		outEnd[0] = outStart[0] + std::cos(rad) * probeReach;
		outEnd[1] = outStart[1] + std::sin(rad) * probeReach;
		outEnd[2] = outStart[2];
	}


	static bool BlockedAheadAt(const PlayerView& self, float travelYaw, float heightAboveFeet)
	{
		float start[3];
		float end[3];
		ProbeLine(self, travelYaw, heightAboveFeet, start, end);

		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 6.0f, 6.0f, 6.0f };
		const int ignore[4] = { -1, -1, 0, 0 };

		unsigned char trace[128] = {};
		SV_Trace(trace, start, end, bounds, ignore,
												 maskPlayerSolidNoBodies, 0, nullptr, 1);
		return *reinterpret_cast<const float*>(trace + traceFraction) < 1.0f;
	}


	static float ClearanceAt(const PlayerView& self, float yawDeg, float heightAboveFeet, float reach)
	{
		const float rad = yawDeg / radToDeg;
		const float start[3] = { self.eye[0], self.eye[1], self.feetZ + heightAboveFeet };
		const float end[3] = { start[0] + std::cos(rad) * reach, start[1] + std::sin(rad) * reach, start[2] };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 4.0f, 4.0f, 4.0f };
		const int ignore[4] = { -1, -1, 0, 0 };
		unsigned char trace[128] = {};
		SV_Trace(trace, start, end, bounds, ignore, maskPlayerSolidNoBodies, 0, nullptr, 1);
		return *reinterpret_cast<const float*>(trace + traceFraction) * reach;
	}


	static bool BlockedAhead(const PlayerView& self, float travelYaw)
	{
		return BlockedAheadAt(self, travelYaw, 40.0f);
	}


	static int PickSteerSide(const PlayerView& self, float travelYaw, int lastSide)
	{
		const bool leftClear = !BlockedAhead(self, travelYaw + steerAngleDeg);
		const bool rightClear = !BlockedAhead(self, travelYaw - steerAngleDeg);
		if (leftClear != rightClear)
		{
			return leftClear ? 1 : -1;
		}
		if (lastSide == 0)
		{
			return (NextRand() & 1u) ? 1 : -1;
		}
		return leftClear ? lastSide : -lastSide;
	}


	static bool LowBlockerAhead(const PlayerView& self, float travelYaw)
	{
		const bool lowBlocked = BlockedAheadAt(self, travelYaw, 24.0f) || BlockedAheadAt(self, travelYaw, 40.0f);
		return lowBlocked && !BlockedAheadAt(self, travelYaw, 62.0f);
	}


	static bool MantleFaceAhead(const PlayerView& self, float travelYaw)
	{
		float start[3];
		float end[3];
		ProbeLine(self, travelYaw, 0.0f, start, end);
		return Navgen::MantleFaceAhead(start, end, nullptr);
	}

	static constexpr float ladderClimbPitchDeg = -50.0f;
	static constexpr int   ladderRegrabMs = 500;
	static constexpr float ladderClimbMinRise = 2.0f;
	static constexpr int   ladderStallLimit = 8;
	static constexpr int   ladderShuffleMs = 800;
	static constexpr int   ladderExitMs = 600;
	static constexpr float ladderHeadroomUnits = 56.0f;
	static constexpr float ladderShuffleOffsets[3] = { 16.0f, 32.0f, 48.0f };
	static constexpr int   ladderJumpGapMs     = 1500;
	static constexpr int   ladderApproachNoSprintMs = 400;


	static int PickLadderShuffleSide(const PlayerView& self, float facingYaw)
	{
		const float rad = facingYaw / radToDeg;
		const float rightX = std::sin(rad);
		const float rightY = -std::cos(rad);
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 12.0f, 12.0f, 4.0f };
		const int ignore[4] = { -1, -1, 0, 0 };
		for (const float offset : ladderShuffleOffsets)
		{
			for (int side = 1; side >= -1; side -= 2)
			{
				const float shift = offset * static_cast<float>(side);
				const float start[3] = { self.eye[0] + rightX * shift, self.eye[1] + rightY * shift, self.eye[2] };
				const float end[3] = { start[0], start[1], start[2] + ladderHeadroomUnits };
				unsigned char trace[128] = {};
				SV_Trace(trace, start, end, bounds, ignore, maskPlayerSolidNoBodies, 0, nullptr, 1);
				if (*reinterpret_cast<const float*>(trace + traceFraction) >= 1.0f)
				{
					return side;
				}
			}
		}
		return 0;
	}


	static bool ClearWalkTo(const PlayerView& self, const float* point);
	static bool IsNodeAvoided(const BotState& bot, int node, int now);


	static int PickWalkableStart(BotState& bot, const PlayerView& self, const float* feet, int preferred, int now)
	{
		if (ClearWalkTo(self, Waypoints::Origin(preferred)))
		{
			return preferred;
		}
		int candidates[startCandidateCount];
		float candidateSq[startCandidateCount];
		int count = 0;
		for (int n = 0; n < Waypoints::Count(); ++n)
		{
			if (n == preferred || Waypoints::ChildCount(n) == 0 || IsNodeAvoided(bot, n, now))
			{
				continue;
			}
			const float distanceSq = GraphDistanceSq(feet, Waypoints::Origin(n));
			if (distanceSq > startSearchSq)
			{
				continue;
			}
			if (count == startCandidateCount && distanceSq >= candidateSq[count - 1])
			{
				continue;
			}
			int slot = count;
			if (count == startCandidateCount)
			{
				slot = startCandidateCount - 1;
			}
			while (slot > 0 && candidateSq[slot - 1] > distanceSq)
			{
				candidates[slot] = candidates[slot - 1];
				candidateSq[slot] = candidateSq[slot - 1];
				--slot;
			}
			candidates[slot] = n;
			candidateSq[slot] = distanceSq;
			if (count < startCandidateCount)
			{
				++count;
			}
		}
		for (int k = 0; k < count; ++k)
		{
			if (ClearWalkTo(self, Waypoints::Origin(candidates[k])))
			{
				return candidates[k];
			}
		}
		return preferred;
	}


	static bool ClearWalkTo(const PlayerView& self, const float* point)
	{
		static const float bodyHeights[3] = { 24.0f, 44.0f, 62.0f };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 18.0f, 18.0f, 8.0f };
		const int ignore[4] = { -1, -1, 0, 0 };

		for (const float height : bodyHeights)
		{
			const float start[3] = { self.eye[0], self.eye[1], self.feetZ + height };
			const float end[3] = { point[0], point[1], point[2] + height };
			unsigned char trace[128] = {};
			SV_Trace(trace, start, end, bounds, ignore,
													 maskPlayerSolidNoBodies, 0, nullptr, 1);
			if (*reinterpret_cast<const float*>(trace + traceFraction) < 1.0f)
			{
				return false;
			}
		}
		return true;
	}


	static int GlassAhead(const PlayerView& self, float travelYaw, float* outHeight)
	{
		static const float bodyHeights[3] = { 24.0f, 40.0f, 62.0f };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 6.0f, 6.0f, 6.0f };

		for (const float height : bodyHeights)
		{
			float start[3];
			float end[3];
			ProbeLine(self, travelYaw, height, start, end);
			const int hitId = Navgen::GlassHitId(start, end, bounds);
			if (hitId)
			{
				*outHeight = height;
				return hitId;
			}
		}
		return 0;
	}


	static bool KnifeGlassAhead(int clientNum, const PlayerView& self, float travelYaw, BotInput& input)
	{
		BotState& bot = bots[clientNum];
		if (debugTick < bot.knifeFaceUntilTick)
		{
			NoteTurn(bot, "glass", bot.knifeYaw);
			input.angles[0] = bot.knifePitch;
			input.angles[1] = bot.knifeYaw;
			input.angles[2] = 0.0f;
			return true;
		}

		const bool mayLook = ((debugTick + clientNum) % 8) == 0 || debugTick == bot.knifeFaceUntilTick;
		if (!mayLook)
		{
			return false;
		}

		float paneHeight = 0.0f;
		const int hitId = GlassAhead(self, travelYaw, &paneHeight);
		if (!hitId)
		{
			if (bot.knifePiece >= 0)
			{
				BotLog("knife client %d piece %d opened, damage %d", clientNum, bot.knifePiece,
					Navgen::GlassPieceDamage(bot.knifePiece));
				bot.knifePiece = -1;
			}
			return false;
		}

		const float drop = self.eye[2] - (self.feetZ + paneHeight);
		bot.knifePitch = std::atan2(drop, probeReach) * radToDeg;
		bot.knifeYaw = travelYaw;
		bot.knifePiece = hitId - 1;
		bot.knifeFaceUntilTick = debugTick + 8;

		BotLog("knife client %d at %.0f %.0f %.0f piece %d damage %d height %.0f pitch %.0f", clientNum,
			self.eye[0], self.eye[1], self.feetZ, hitId - 1, Navgen::GlassPieceDamage(hitId - 1),
			paneHeight, bot.knifePitch);

		NoteTurn(bot, "glass", bot.knifeYaw);
		input.angles[0] = bot.knifePitch;
		input.angles[1] = bot.knifeYaw;
		input.angles[2] = 0.0f;
		input.buttons |= cmdButtonMelee;
		return true;
	}


	static void ApplyNodeStance(int node, BotInput& input)
	{
		switch (Waypoints::TypeOf(node))
		{
		case Waypoints::NodeCrouch:
			input.buttons |= cmdButtonCrouch;
			break;
		case Waypoints::NodeProne:
			input.buttons |= cmdButtonProne;
			break;
		case Waypoints::NodeClimb:
			input.buttons |= cmdButtonUp;
			break;
		default:
			break;
		}
	}


	static bool PickLookPoint(int clientNum, const PlayerView& self, float travelYaw, float out[3])
	{
		BotState& bot = bots[clientNum];
		bool keepsLook = false;
		if (debugTick >= bot.lookRecheckTick && bot.lookNode > bot.pathIndex && bot.lookNode < bot.pathLength)
		{
			const float* current = Waypoints::Origin(bot.path[bot.lookNode]);
			const float currentHead[3] = { current[0], current[1], current[2] + coverEyeRise };
			const float currentYaw = std::atan2(current[1] - self.eye[1], current[0] - self.eye[0]) * radToDeg;
			keepsLook = std::fabs(AngleDelta(travelYaw, currentYaw)) < lookConeDeg
				&& SightLine(self.eye, currentHead, clientNum);
			if (keepsLook)
			{
				bot.lookRecheckTick = debugTick + lookRecheckFrames;
			}
		}
		if (!keepsLook
			&& (debugTick >= bot.lookRecheckTick || bot.lookNode <= bot.pathIndex || bot.lookNode >= bot.pathLength))
		{
			bot.lookRecheckTick = debugTick + lookRecheckFrames;
			bot.lookNode = -1;

			const int low = bot.pathIndex + 1;
			int high = bot.pathIndex + lookAheadNodes;
			if (high > bot.pathLength - 1)
			{
				high = bot.pathLength - 1;
			}
			while (high > low && DistanceSq2D(self.eye, Waypoints::Origin(bot.path[high])) > lookMaxSq)
			{
				--high;
			}
			for (int attempt = 0; attempt < lookTraces && high >= low; ++attempt)
			{
				const float* origin = Waypoints::Origin(bot.path[high]);
				const float head[3] = { origin[0], origin[1], origin[2] + coverEyeRise };
				const float toYaw = std::atan2(origin[1] - self.eye[1], origin[0] - self.eye[0]) * radToDeg;
				const bool isAlongRoute = std::fabs(AngleDelta(travelYaw, toYaw)) < lookConeDeg;
				if (isAlongRoute && SightLine(self.eye, head, clientNum))
				{
					bot.lookNode = high;
					break;
				}
				if (high == low)
				{
					break;
				}
				high = low + (high - low) / 2;
			}
		}

		if (bot.lookNode < 0)
		{
			return false;
		}
		const float* origin = Waypoints::Origin(bot.path[bot.lookNode]);
		out[0] = origin[0];
		out[1] = origin[1];
		out[2] = origin[2] + coverEyeRise;
		return true;
	}


	static bool TryFindRevealPoint(int clientNum, float out[3])
	{
		const BotState& bot = bots[clientNum];
		const int here = AreaOfClient(clientNum);
		if (here < 0 || bot.pathIndex + 1 >= bot.pathLength)
		{
			return false;
		}
		int beyondArea = -1;
		for (int k = bot.pathIndex; k < bot.pathLength && k <= bot.pathIndex + revealLookNodes; ++k)
		{
			const int area = Waypoints::AreaOf(bot.path[k]);
			if (area >= 0 && area != here)
			{
				beyondArea = area;
				break;
			}
		}
		if (beyondArea < 0)
		{
			return false;
		}

		const float* corner = Waypoints::Origin(bot.path[bot.pathIndex]);
		const float* ahead = Waypoints::Origin(bot.path[bot.pathIndex + 1]);
		const float cornerYaw = std::atan2(ahead[1] - corner[1], ahead[0] - corner[0]) * radToDeg;
		const float minSq = revealMinUnits * revealMinUnits;
		const float maxSq = revealMaxUnits * revealMaxUnits;
		const int areaCount = Waypoints::AreaCount();
		int best = -1;
		float bestSq = 0.0f;
		for (int area = 0; area < areaCount; ++area)
		{
			if (!Waypoints::AreasSee(beyondArea, area) || Waypoints::AreasSee(here, area))
			{
				continue;
			}
			const Waypoints::AreaInfo* info = Waypoints::AreaAt(area);
			if (info->memberCount < revealMinMembers)
			{
				continue;
			}
			const float* origin = Waypoints::Origin(info->node);
			const float distanceSq = DistanceSq2D(corner, origin);
			if (distanceSq < minSq || distanceSq > maxSq || (best >= 0 && distanceSq >= bestSq))
			{
				continue;
			}
			const float yaw = std::atan2(origin[1] - corner[1], origin[0] - corner[0]) * radToDeg;
			if (std::fabs(AngleDelta(cornerYaw, yaw)) > revealConeDeg)
			{
				continue;
			}
			best = area;
			bestSq = distanceSq;
		}
		if (best < 0)
		{
			return false;
		}
		const float* origin = Waypoints::Origin(Waypoints::AreaAt(best)->node);
		out[0] = origin[0];
		out[1] = origin[1];
		out[2] = origin[2];
		return true;
	}


	static float GlanceYaw(int clientNum, const PlayerView& self, float baseYaw, int now)
	{
		BotState& bot = bots[clientNum];
		if (now < bot.glanceUntilTime)
		{
			return bot.glanceYawDeg;
		}
		if (now < bot.glanceNextTime)
		{
			return 0.0f;
		}

		bot.glanceNextTime = now + IrandMs(glanceGapMinMs, glanceGapMaxMs);
		if (!RollPercent(ScaleByTrait(tuning.glance, 100 - bot.personality.sightDiscipline)))
		{
			return 0.0f;
		}

		float threatYaw = 0.0f;
		if (RollPercent(threatGlancePercentBySkill[bot.skillIndex])
			&& (TryGetThreatLookYaw(clientNum, self, &threatYaw) || TryGetFrontYaw(self.team, self.eye, &threatYaw)))
		{
			const float threatOffset = AngleDelta(baseYaw, threatYaw);
			if (std::fabs(threatOffset) > threatGlanceMinDeg && std::fabs(threatOffset) < threatGlanceMaxDeg
				&& IsViewOpen(clientNum, self, threatYaw, threatGlanceOpenUnits) && IsPlayableToward(self, threatYaw))
			{
				bot.glanceYawDeg = threatOffset;
				bot.glanceUntilTime = now + IrandMs(threatGlanceHoldMinMs, threatGlanceHoldMaxMs);
				return bot.glanceYawDeg;
			}
		}

		float side = 1.0f;
		if ((NextRand() & 1u) != 0)
		{
			side = -1.0f;
		}
		bool isShoulder = RollPercent(ScaleByTrait(shoulderCheckPercent, disciplineBySkill[bot.skillIndex]));
		float frontYaw = 0.0f;
		if (isShoulder && self.team != 0 && TryGetFrontYaw(self.team, self.eye, &frontYaw)
			&& std::fabs(AngleDelta(baseYaw, frontYaw)) < shoulderFrontSafeDeg)
		{
			isShoulder = false;
		}
		float amount = Flrand(glanceYawMinDeg, glanceYawMaxDeg);
		int holdMs = IrandMs(glanceHoldMinMs, glanceHoldMaxMs);
		if (isShoulder)
		{
			amount = Flrand(shoulderYawMinDeg, shoulderYawMaxDeg);
			holdMs = IrandMs(shoulderHoldMinMs, shoulderHoldMaxMs);
		}
		if (!IsViewOpen(clientNum, self, baseYaw + side * amount, glanceOpenUnits) || !IsPlayableToward(self, baseYaw + side * amount))
		{
			side = -side;
			if (!IsViewOpen(clientNum, self, baseYaw + side * amount, glanceOpenUnits) || !IsPlayableToward(self, baseYaw + side * amount))
			{
				return 0.0f;
			}
		}
		bot.glanceYawDeg = side * amount;
		bot.glanceUntilTime = now + holdMs;
		if (isShoulder)
		{
			BotLog("glance client %d checks its shoulder", clientNum);
		}
		return bot.glanceYawDeg;
	}


	static bool IsAlert(int clientNum, const PlayerView& self, int now)
	{
		BotState& bot = bots[clientNum];
		const Personality& personality = bot.personality;

		if (personality.caution < alertCautionMin)
		{
			return false;
		}
		const int alertUnits = tuning.adsWalk * (75 + personality.caution / 2) / 100;
		if (alertUnits <= 0)
		{
			return false;
		}
		if (debugTick < bot.alertCheckTick)
		{
			return now < bot.alertSightedUntilTime;
		}
		bot.alertCheckTick = debugTick + alertCheckFrames;

		float spot[3] = { 0.0f, 0.0f, 0.0f };
		bool hasSpot = false;
		if (bot.task.kind == TaskHunt && bot.pathTaskKind == TaskHunt && bot.task.client >= 0)
		{
			if (bot.huntKnownClient == bot.task.client && bot.huntKnownTime != 0 && now - bot.huntKnownTime < alertMs)
			{
				spot[0] = bot.huntKnownPos[0];
				spot[1] = bot.huntKnownPos[1];
				spot[2] = bot.huntKnownPos[2];
				hasSpot = true;
			}
		}
		else if (bot.task.kind == TaskPush && bot.pathTaskKind == TaskPush && bot.task.source
				 && std::strcmp(bot.task.source, "opening") != 0 && std::strcmp(bot.task.source, "hazard") != 0)
		{
			spot[0] = bot.task.point[0];
			spot[1] = bot.task.point[1];
			spot[2] = bot.task.point[2];
			hasSpot = true;
		}
		else if (now < bot.alertUntilTime)
		{
			spot[0] = bot.lastSeenPos[0];
			spot[1] = bot.lastSeenPos[1];
			spot[2] = bot.lastSeenPos[2];
			hasSpot = true;
		}
		if (!hasSpot)
		{
			return now < bot.alertSightedUntilTime;
		}

		const float alertSq = static_cast<float>(alertUnits) * static_cast<float>(alertUnits);
		if (DistanceSq2D(self.eye, spot) > alertSq
			|| ConeDot(self.eye, spot, bot.input.angles[1], bot.input.angles[0]) < alertAheadDot)
		{
			return now < bot.alertSightedUntilTime;
		}

		static const float spotRises[2] = { 0.0f, coverEyeRise };
		for (const float rise : spotRises)
		{
			const float probe[3] = { spot[0], spot[1], spot[2] + rise };
			if (SightLine(self.eye, probe, clientNum))
			{
				bot.alertSightedUntilTime = now + alertHoldMs;
				break;
			}
		}
		return now < bot.alertSightedUntilTime;
	}


	static void WalkAimed(int clientNum, BotInput& input)
	{
		if (bots[clientNum].personality.isQuickscoper)
		{
			return;
		}
		const char* playerState = PlayerStateOf(clientNum);
		const WeaponInfo weapon = ReadWeapon(playerState);

		input.forward = static_cast<signed char>(static_cast<float>(input.forward) * alertWalkScale);
		input.right = static_cast<signed char>(static_cast<float>(input.right) * alertWalkScale);
		input.buttons &= ~cmdButtonSprint;
		bots[clientNum].wasSlow = true;

		if (weapon.index && CanAds(weapon, 1.0e9f))
		{
			input.buttons |= cmdButtonAds;
		}
	}


	static bool MaySprint(int clientNum, int now, bool isStraight)
	{
		BotState& bot = bots[clientNum];
		if (bot.personality.isMeleeOnly && now >= bot.noSprintUntilTime)
		{
			if (now >= bot.sprintWindowEndTime)
			{
				bot.wantSprint = RollPercent(knifeSprintPercent);
				bot.sprintWindowEndTime = now + IrandMs(sprintWindowMinMs, sprintWindowMaxMs);
			}
			return bot.wantSprint;
		}
		const bool isSprinting = (*reinterpret_cast<const int*>(PlayerStateOf(clientNum) + psPmFlags) & pmFlagSprint) != 0;
		if ((!isStraight && !isSprinting) || now < bot.noSprintUntilTime)
		{
			return false;
		}
		if (now >= bot.sprintWindowEndTime)
		{
			bot.wantSprint = RollPercent(bot.personality.sprintPercent);
			bot.sprintWindowEndTime = now + (bot.wantSprint
				? IrandMs(sprintWindowMinMs, sprintWindowMaxMs)
				: IrandMs(sprintRestMinMs, sprintRestMaxMs));
		}
		return bot.wantSprint;
	}


	static int BodySide(int clientNum, const PlayerView& self, float travelYaw)
	{
		const float rad = travelYaw / radToDeg;
		const float forwardX = std::cos(rad);
		const float forwardY = std::sin(rad);

		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			if (i == clientNum)
			{
				continue;
			}
			const PlayerView other = ReadPlayerView(i);
			if (!other.isPlaying)
			{
				continue;
			}

			const float dx = other.eye[0] - self.eye[0];
			const float dy = other.eye[1] - self.eye[1];
			const float distSq = dx * dx + dy * dy;
			if (distSq > bodyAheadDistSq || distSq < 1.0f || std::fabs(other.feetZ - self.feetZ) > 72.0f)
			{
				continue;
			}

			const float dist = std::sqrt(distSq);
			if ((dx * forwardX + dy * forwardY) / dist < bodyAheadDot)
			{
				continue;
			}

			const float cross = forwardX * dy - forwardY * dx;
			return cross > 0.0f ? -1 : 1;
		}
		return 0;
	}


	static void SteerToward(int clientNum, const PlayerView& self, const float* point,
							BotInput& input, bool allowTurn)
	{
		BotState& bot = bots[clientNum];
		const float dx = point[0] - self.eye[0];
		const float dy = point[1] - self.eye[1];
		const float directYaw = std::atan2(dy, dx) * radToDeg;
		float yaw = directYaw;

		if (allowTurn && KnifeGlassAhead(clientNum, self, directYaw, input))
		{
			bot.stuckFrames = 0;
			MoveTowards(directYaw, input.angles[1], input);
			bot.wasWalking = true;
			bot.wasProne = false;
			return;
		}

		const int bodySide = BodySide(clientNum, self, directYaw);
		if (bodySide != 0)
		{
			bot.steerSide = bodySide;
			bot.steerFrames = sidestepFrames;
			bot.stuckFrames = 0;
		}

		const bool mayProbe = ((debugTick + clientNum) % 3) == 0;
		if (bot.steerFrames > 0)
		{
			--bot.steerFrames;
			yaw += static_cast<float>(bot.steerSide) * steerAngleDeg;
		}
		else if (mayProbe && BlockedAhead(self, directYaw))
		{
			bot.steerSide = PickSteerSide(self, directYaw, bot.steerSide);
			bot.steerFrames = steerFramesMin
				+ static_cast<int>(NextRand() % static_cast<unsigned int>(steerFramesSpan));
			yaw += static_cast<float>(bot.steerSide) * steerAngleDeg;
		}

		if (mayProbe)
		{
			if (LowBlockerAhead(self, yaw))
			{
				if (debugTick >= bot.jumpPulseUntilTick)
				{
					BotLog("hop client %d at %.0f %.0f %.0f yaw %.0f", clientNum,
						self.eye[0], self.eye[1], self.feetZ, yaw);
				}
				bot.jumpPulseUntilTick = debugTick + 12;
			}
		}

		if (allowTurn)
		{
			TurnView(clientNum, input, directYaw, 0.0f, "steer");
		}
		MoveTowards(yaw, input.angles[1], input);

		const int now = ServerTimeMs();
		if (debugTick < bot.jumpPulseUntilTick)
		{
			if (((debugTick + clientNum) % 12) < 6)
			{
				input.buttons |= cmdButtonUp;
			}
		}
		else if (allowTurn && IsAlert(clientNum, self, now))
		{
			WalkAimed(clientNum, input);
		}
		else if (MaySprint(clientNum, now, bot.steerFrames == 0))
		{
			input.buttons |= cmdButtonSprint;
		}

		bot.wasWalking = true;
		bot.wasProne = false;
	}


	static void AvoidNode(BotState& bot, int node, int now)
	{
		bot.avoidNodes[bot.avoidNodeNext] = node;
		bot.avoidNodeTimes[bot.avoidNodeNext] = now + avoidNodeMs;
		bot.avoidNodeNext = (bot.avoidNodeNext + 1) % avoidNodeSlots;
	}


	static bool IsNodeAvoided(const BotState& bot, int node, int now)
	{
		for (int k = 0; k < avoidNodeSlots; ++k)
		{
			if (bot.avoidNodes[k] == node && now < bot.avoidNodeTimes[k])
			{
				return true;
			}
		}
		return false;
	}


	static const char* LinkLabel(int from, int to)
	{
		if (from < 0)
		{
			return "offlink";
		}
		return Waypoints::KindName(Waypoints::KindOf(from, to));
	}


	static bool HandleStuck(int clientNum, const PlayerView& self, BotInput& input)
	{
		BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();

		const float sdx = self.eye[0] - bot.lastStuckPos[0];
		const float sdy = self.eye[1] - bot.lastStuckPos[1];
		if ((sdx * sdx + sdy * sdy) < rescueSpotSq && now - bot.lastStuckTime < rescueWindowMs)
		{
			++bot.stuckRepeat;
		}
		else
		{
			bot.stuckRepeat = 0;
		}
		bot.lastStuckPos[0] = self.eye[0];
		bot.lastStuckPos[1] = self.eye[1];
		bot.lastStuckTime = now;
		bot.stuckFrames = 0;

		const bool isPursuing = bot.pursuitUntilTime > now;
		const bool isPathless = bot.isOffGraph || isPursuing;
		float hopYaw = input.angles[1];
		const int rescueBar = isPathless ? rescueRepeatsOffGraph : rescueRepeats;
		if (bot.stuckRepeat >= rescueBar)
		{
			BotLog("rescue client %d trapped at %.0f %.0f %.0f%s", clientNum,
				self.eye[0], self.eye[1], self.feetZ, bot.isOffGraph ? " offgraph" : "");
			RescueBot(clientNum);
			return false;
		}

		if (!isPathless && bot.pathIndex < bot.pathLength)
		{
			const int target = bot.path[bot.pathIndex];
			const float* origin = Waypoints::Origin(target);
			hopYaw = std::atan2(origin[1] - self.eye[1], origin[0] - self.eye[0]) * radToDeg;
			AvoidNode(bot, target, now);

			const float toNodeX = origin[0] - self.eye[0];
			const float toNodeY = origin[1] - self.eye[1];
			if (toNodeX * toNodeX + toNodeY * toNodeY > stuckHeatMinUnits * stuckHeatMinUnits)
			{
				AddStuckHeat(target);
			}

			int from = -1;
			if (bot.pathIndex > 0)
			{
				from = bot.path[bot.pathIndex - 1];
				AddLinkTrap(from, target);
			}
			BotLog("stuck client %d at %.0f %.0f %.0f node %d at %.0f %.0f %.0f %s from %d repeat %d", clientNum,
				self.eye[0], self.eye[1], self.feetZ, target, origin[0], origin[1], origin[2],
				LinkLabel(from, target), from, bot.stuckRepeat);
		}
		else
		{
			BotLog("stuck client %d at %.0f %.0f %.0f %s repeat %d", clientNum,
				self.eye[0], self.eye[1], self.feetZ,
				bot.isOffGraph ? "offgraph" : (isPursuing ? "pursuit" : "parked"), bot.stuckRepeat);
		}

		if (isPursuing)
		{
			bot.pursuitUntilTime = 0;
			bot.huntPauseUntilTime = now + JitterMs(huntPauseMs);
		}

		bot.steerSide = bot.steerSide >= 0 ? -1 : 1;
		bot.steerFrames = steerFramesMin;

		DropPath(bot);
		if (LowBlockerAhead(self, hopYaw))
		{
			input.buttons |= cmdButtonUp;
		}
		if (bot.stuckRepeat >= escapeRepeatMin)
		{
			float bestClearance = -1.0f;
			for (int k = 0; k < escapeDirections; ++k)
			{
				const float yaw = hopYaw + 180.0f + static_cast<float>(k) * (360.0f / escapeDirections);
				float clearance = ClearanceAt(self, yaw, escapeProbeHeight, escapeProbeUnits);
				if (std::fabs(AngleDelta(hopYaw, yaw)) < 45.0f)
				{
					clearance *= 0.5f;
				}
				if (clearance > bestClearance)
				{
					bestClearance = clearance;
					bot.escapeYaw = yaw;
				}
			}
			bot.escapeUntilTime = now + escapeMs;
			bot.escapeJumps = (bot.stuckRepeat % 2) == 0;
			const char* jumpNote = "";
			if (bot.escapeJumps)
			{
				jumpNote = " jumping";
			}
			BotLog("escape client %d backs out toward %.0f (%.0f clear)%s", clientNum, bot.escapeYaw, bestClearance, jumpNote);
		}
		return true;
	}


	static bool CoastOrWait(BotState& bot, BotInput& input)
	{
		if (debugTick >= bot.coastUntilTick)
		{
			return false;
		}
		MoveTowards(bot.lastTravelYaw, input.angles[1], input);
		bot.wasWalking = true;
		bot.navMode = "coast";
		return true;
	}

	static bool RunNavigation(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn)
	{
		BotState& bot = bots[clientNum];
		if (!Waypoints::IsLoaded())
		{
			bot.navMode = "nograph";
			return false;
		}

		const int now = ServerTimeMs();
		float feet[3];
		FeetOf(self, feet);

		const float movedX = self.eye[0] - bot.lastOrigin[0];
		const float movedY = self.eye[1] - bot.lastOrigin[1];
		bot.lastOrigin[0] = self.eye[0];
		bot.lastOrigin[1] = self.eye[1];

		const bool wasWalking = bot.wasWalking;
		const bool wasSlow = bot.wasSlow;
		bot.wasWalking = false;
		bot.wasSlow = false;
		float stuckBarSq = stuckMoveSq;
		if (bot.wasProne)
		{
			stuckBarSq = proneStuckMoveSq;
		}
		else if (wasSlow)
		{
			stuckBarSq = slowStuckMoveSq;
		}
		const bool isStunned = IsShellshocked(clientNum, now);
		if (!wasWalking || self.isOnLadder || self.isMantling || isStunned)
		{
			bot.stuckFrames = 0;
		}
		else if ((movedX * movedX + movedY * movedY) < stuckBarSq)
		{
			++bot.stuckFrames;
		}
		else
		{
			bot.stuckFrames = 0;
		}

		if (debugTick < bot.stanceGraceUntilTick)
		{
			bot.stuckFrames = 0;
		}

		if (bot.stuckFrames > stuckFrameLimit && !HandleStuck(clientNum, self, input))
		{
			return false;
		}

		if (now < bot.escapeUntilTime && !self.isOnLadder)
		{
			bot.navMode = "escape";
			MoveTowards(bot.escapeYaw, input.angles[1], input);
			if (bot.escapeJumps && ((debugTick + clientNum) % 4) == 0)
			{
				input.buttons |= cmdButtonUp;
			}
			bot.wasWalking = true;
			return true;
		}

		if (IsHolding(clientNum))
		{
			bot.navMode = "hold";
			return false;
		}

		bot.isNearFollowMate = false;
		if (bot.task.kind == TaskFollow && bot.task.client >= 0)
		{
			const PlayerView mate = ReadPlayerView(bot.task.client);
			const float mateDistSq = DistanceSq2D(self.eye, mate.eye);
			bot.isNearFollowMate = mate.isPlaying && mateDistSq < followCalmUnits * followCalmUnits;
			if (mate.isPlaying && mateDistSq < followStandoffUnits * followStandoffUnits)
			{
				DropPath(bot);
				bot.isFollowWaiting = true;
				bot.navMode = "wait";
				return false;
			}
		}

		if (bot.pursuitUntilTime > now)
		{
			if (DistanceSq2D(self.eye, bot.pursuitPoint) > pursuitArriveSq)
			{
				bot.navMode = "pursue";
				SteerToward(clientNum, self, bot.pursuitPoint, input, allowTurn);
				return true;
			}
			BotLog("pursuit client %d arrived at %.0f %.0f %.0f", clientNum,
				self.eye[0], self.eye[1], self.feetZ);
			bot.pursuitUntilTime = 0;
		}
		else if (bot.pursuitUntilTime != 0)
		{
			BotLog("pursuit client %d timed out at %.0f %.0f %.0f", clientNum,
				self.eye[0], self.eye[1], self.feetZ);
			bot.pursuitUntilTime = 0;
		}

		if (bot.isOffGraph)
		{
			if (debugTick >= bot.offGraphRecheckTick)
			{
				bot.offGraphRecheckTick = debugTick + offGraphRecheckFrames;
				const int nearest = Waypoints::Nearest(feet);
				if (nearest >= 0)
				{
					bot.offGraphNode = nearest;
					if (GraphDistanceSq(feet, Waypoints::Origin(nearest)) <= offGraphDistSq)
					{
						BotLog("ongraph client %d at %.0f %.0f %.0f node %d", clientNum,
							self.eye[0], self.eye[1], self.feetZ, nearest);
						bot.isOffGraph = false;
						DropPath(bot);
					}
				}
			}
			if (bot.isOffGraph)
			{
				bot.navMode = "offgraph";
				SteerToward(clientNum, self, Waypoints::Origin(bot.offGraphNode), input, allowTurn);
				return true;
			}
		}

		if (ReplanTask(clientNum, self, now) && wasWalking)
		{
			bot.coastUntilTick = debugTick + coastFrames;
		}

		const bool needsPath = bot.pathLength <= 0 || bot.pathIndex >= bot.pathLength;
		if (needsPath)
		{
			bot.navMode = "wait";
			if (debugTick < bot.repathNotBeforeTick)
			{
				return CoastOrWait(bot, input);
			}

			if (pathBuiltThisFrame)
			{
				return CoastOrWait(bot, input);
			}
			pathBuiltThisFrame = true;
			bot.repathNotBeforeTick = debugTick + repathGapFrames;

			int start = PickStartNode(feet, input.angles[1], true);
			if (start < 0)
			{
				return false;
			}
			start = PickWalkableStart(bot, self, feet, start, now);

			if (GraphDistanceSq(feet, Waypoints::Origin(start)) > offGraphDistSq)
			{
				bot.isOffGraph = true;
				bot.offGraphNode = start;
				bot.offGraphRecheckTick = debugTick + offGraphRecheckFrames;
				const float* origin = Waypoints::Origin(start);
				BotLog("offgraph client %d at %.0f %.0f %.0f nearest node %d at %.0f %.0f %.0f", clientNum,
					self.eye[0], self.eye[1], self.feetZ, start, origin[0], origin[1], origin[2]);
				bot.navMode = "offgraph";
				SteerToward(clientNum, self, origin, input, allowTurn);
				return true;
			}

			if (IsNodeAvoided(bot, start, now))
			{
				int alternate = -1;
				float alternateBest = 0.0f;
				for (int n = 0; n < Waypoints::Count(); ++n)
				{
					if (Waypoints::ChildCount(n) == 0 || IsNodeAvoided(bot, n, now))
					{
						continue;
					}
					const float distanceSq = GraphDistanceSq(feet, Waypoints::Origin(n));
					if (alternate < 0 || distanceSq < alternateBest)
					{
						alternate = n;
						alternateBest = distanceSq;
					}
				}
				if (alternate >= 0)
				{
					start = alternate;
				}
			}

			int goal = -1;
			const char* goalKind = "roam";
			const GoalKind goalChoice = ChooseGoal(clientNum, self, feet, start, &goal, &goalKind);
			if (goalChoice == GoalPursuit)
			{
				bot.navMode = "pursue";
				SteerToward(clientNum, self, bot.pursuitPoint, input, allowTurn);
				return true;
			}
			if (goalChoice == GoalNone || goal < 0)
			{
				return false;
			}

			const float exposureScale = static_cast<float>(bot.personality.caution) / exposureCautionDivisor;
			RouteWatch watch;
			const bool hasWatch = TryGetRouteWatch(clientNum, goal, &watch);
			const float* heatPenalty = HeatPenalty(self.team, bot.lanePenaltyScale, exposureScale, &watch);
			int trapCount = 0;
			const Waypoints::LinkTrap* traps = LinkTraps(&trapCount);
			bot.pathLength = Waypoints::FindPath(start, goal, bot.path, maxPathNodes, heatPenalty, traps, trapCount);
			++pathFinds;
			if (bot.pathLength > 0 && IsRouteDead(bot.path, bot.pathLength))
			{
				BotLog("deadroute client %d %s from node %d to node %d: only a dead link reaches it", clientNum, goalKind,
					start, goal);
				bot.pathLength = 0;
			}
			if (bot.pathLength <= 0)
			{
				Waypoints::SetLadderDescent(true);
				bot.pathLength = Waypoints::FindPath(start, goal, bot.path, maxPathNodes, heatPenalty, traps, trapCount);
				Waypoints::SetLadderDescent(false);
				++pathFinds;
				if (bot.pathLength > 0 && IsRouteDead(bot.path, bot.pathLength))
				{
					bot.pathLength = 0;
				}
				if (bot.pathLength > 0)
				{
					BotLog("ladderdown client %d %s from node %d: the only way on is down a ladder", clientNum, goalKind,
						start);
				}
			}

			if (bot.pathLength <= 0)
			{
				BotLog("noroute client %d %s from node %d to node %d", clientNum, goalKind, start, goal);
				OnGoalFailed(clientNum, "no route");
			}
			for (int attempt = 0; attempt < 2 && bot.pathLength <= 0; ++attempt)
			{
				goal = PickRoamGoal(clientNum, self, feet, start);
				goalKind = "roam";
				bot.pathLength = 0;
				if (goal >= 0)
				{
					bot.pathLength = Waypoints::FindPath(start, goal, bot.path, maxPathNodes, heatPenalty, traps, trapCount);
				}
				if (attempt == 0 && bot.pathLength > 0 && IsRouteDead(bot.path, bot.pathLength))
				{
					bot.pathLength = 0;
				}
				++pathFinds;
			}

			for (int trim = 0; trim < 3 && bot.pathLength >= 2; ++trim)
			{
				const float* first = Waypoints::Origin(bot.path[0]);
				const float* second = Waypoints::Origin(bot.path[1]);
				const float linkX = second[0] - first[0];
				const float linkY = second[1] - first[1];
				const float linkLengthSq = linkX * linkX + linkY * linkY;
				if (linkLengthSq < 1.0f)
				{
					break;
				}
				const float along = ((feet[0] - first[0]) * linkX + (feet[1] - first[1]) * linkY) / linkLengthSq;
				if (along <= 0.05f || std::fabs(second[2] - self.feetZ) > 40.0f)
				{
					break;
				}
				if (Waypoints::KindOf(bot.path[0], bot.path[1]) != Waypoints::LinkWalk)
				{
					break;
				}
				for (int n = 1; n < bot.pathLength; ++n)
				{
					bot.path[n - 1] = bot.path[n];
				}
				--bot.pathLength;
			}

			for (int n = 0; n < bot.pathLength; ++n)
			{
				AddHeat(self.team, bot.path[n], heatPlan);
			}

			bot.pathIndex = 0;
			bot.approachBestSq = 0.0f;
			bot.approachStall = 0;
			bot.pathTaskSerial = bot.task.serial;
			bot.pathTaskKind = bot.task.kind;
			bot.goalNode = bot.pathLength > 0 ? goal : -1;

			bot.huntReplanTime = now + tuning.huntReplan + clientNum * 137;

			if (bot.pathLength <= 0)
			{
				bot.navMode = "noroute";
				if (((debugTick + clientNum) % 20) == 0)
				{
					BotLog("noroute client %d at %.0f %.0f %.0f from node %d", clientNum,
						self.eye[0], self.eye[1], self.feetZ, start);
				}
				return false;
			}

			if (hasWatch)
			{
				int inSight = 0;
				for (int n = 0; n < bot.pathLength; ++n)
				{
					if (Waypoints::AreasSee(watch.area, Waypoints::AreaOf(bot.path[n])))
					{
						++inSight;
					}
				}
				BotLog("path client %d from node %d to %d len %d %s, around the sight of area %d (%d nodes in it)",
					clientNum, start, goal, bot.pathLength, goalKind, watch.area, inSight);
			}
			else
			{
				BotLog("path client %d from node %d to %d len %d %s", clientNum, start, goal,
					bot.pathLength, goalKind);
			}
		}

		int node = 0;
		float dx = 0.0f;
		float dy = 0.0f;
		float dz = 0.0f;
		float nodeDistSq = 0.0f;
		for (;;)
		{
			if (bot.pathIndex >= bot.pathLength)
			{
				OnGoalReached(clientNum);
				DropPath(bot);
				bot.navMode = IsHolding(clientNum) ? "hold" : "parked";
				if (wasWalking && !IsHolding(clientNum) && now >= bot.arriveLookUntilTime && bot.task.kind != TaskNone)
				{
					bot.coastUntilTick = debugTick + coastFrames;
					return CoastOrWait(bot, input);
				}
				return false;
			}

			node = bot.path[bot.pathIndex];
			const float* nodeOrigin = Waypoints::Origin(node);
			dx = nodeOrigin[0] - self.eye[0];
			dy = nodeOrigin[1] - self.eye[1];

			dz = nodeOrigin[2] - self.feetZ;
			nodeDistSq = dx * dx + dy * dy;
			const bool leadsToLadder = bot.pathIndex + 1 < bot.pathLength
				&& Waypoints::KindOf(node, bot.path[bot.pathIndex + 1]) == Waypoints::LinkLadder;

			bool isClimbEntry = false;
			bool isPastClimbMiddle = true;
			float arriveDz = 40.0f;
			if (bot.pathIndex > 0)
			{
				const int previousNode = bot.path[bot.pathIndex - 1];
				const Waypoints::LinkKind entryKind = Waypoints::KindOf(previousNode, node);
				if (entryKind == Waypoints::LinkMantle || entryKind == Waypoints::LinkJump)
				{
					isClimbEntry = true;
					const float* previous = Waypoints::Origin(previousNode);
					arriveDz = std::fabs(nodeOrigin[2] - previous[2]) * 0.5f;
					if (arriveDz < climbArriveDz)
					{
						arriveDz = climbArriveDz;
					}
					if (arriveDz > 40.0f)
					{
						arriveDz = 40.0f;
					}
					const float linkX = nodeOrigin[0] - previous[0];
					const float linkY = nodeOrigin[1] - previous[1];
					const float linkLengthSq = linkX * linkX + linkY * linkY;
					if (linkLengthSq > 1.0f)
					{
						const float fromX = self.eye[0] - previous[0];
						const float fromY = self.eye[1] - previous[1];
						isPastClimbMiddle = (fromX * linkX + fromY * linkY) / linkLengthSq > 0.5f;
					}
				}
			}

			float arriveSq = arriveRadiusSq;
			if (leadsToLadder)
			{
				arriveSq = ladderFootArriveSq;
			}
			bool isStairStep = false;
			if (bot.pathIndex + 1 < bot.pathLength
				&& Waypoints::KindOf(node, bot.path[bot.pathIndex + 1]) == Waypoints::LinkWalk)
			{
				const float* stairNext = Waypoints::Origin(bot.path[bot.pathIndex + 1]);
				const float rise = std::fabs(stairNext[2] - nodeOrigin[2]);
				const float run = std::sqrt(DistanceSq2D(nodeOrigin, stairNext));
				isStairStep = rise > stairRiseMin && rise > run * stairSlopeMin;
			}
			if (isStairStep && arriveDz > stairArriveDz)
			{
				arriveDz = stairArriveDz;
			}
			const bool isBelowNode = dz > belowNodeDz + std::sqrt(nodeDistSq) * belowNodeSlope;
			bool arrived = nodeDistSq < arriveSq && std::fabs(dz) < arriveDz && isPastClimbMiddle && !isBelowNode;

			if (!arrived && !leadsToLadder && isPastClimbMiddle && !isBelowNode && bot.pathIndex + 1 < bot.pathLength)
			{
				const float* next = Waypoints::Origin(bot.path[bot.pathIndex + 1]);
				const bool isOnClimbTop = (!isClimbEntry && !isStairStep) || std::fabs(dz) < arriveDz;
				if (std::fabs(next[2] - self.feetZ) < 100.0f && isOnClimbTop)
				{
					arrived = DistanceSq2D(self.eye, next) < DistanceSq2D(nodeOrigin, next)
						&& ClearWalkTo(self, next);
				}
			}

			if (!arrived && !isBelowNode && bot.pathIndex > 0 && std::fabs(dz) < arriveDz)
			{
				const float* previous = Waypoints::Origin(bot.path[bot.pathIndex - 1]);
				const float linkX = nodeOrigin[0] - previous[0];
				const float linkY = nodeOrigin[1] - previous[1];
				const float linkLengthSq = linkX * linkX + linkY * linkY;
				if (linkLengthSq > 1.0f)
				{
					const float fromX = self.eye[0] - previous[0];
					const float fromY = self.eye[1] - previous[1];
					const float along = (fromX * linkX + fromY * linkY) / linkLengthSq;
					const float lateralSq = fromX * fromX + fromY * fromY - along * along * linkLengthSq;
					arrived = along > 1.0f && lateralSq < (100.0f * 100.0f);
				}
			}

			if (!arrived)
			{
				break;
			}
			AddHeat(self.team, node, heatVisit);
			++bot.pathIndex;
			bot.approachBestSq = 0.0f;
			bot.approachStall = 0;
		}

		const float* nodeOrigin = Waypoints::Origin(node);
		bot.navMode = "path";

		if (bot.pathIndex > 0 && dz > fellBelowRouteDz && !self.isOnLadder)
		{
			const int previousNode = bot.path[bot.pathIndex - 1];
			const float* previousOrigin = Waypoints::Origin(previousNode);
			if (Waypoints::KindOf(previousNode, node) == Waypoints::LinkWalk
				&& previousOrigin[2] - self.feetZ > fellBelowRouteDz)
			{
				BotLog("fell client %d at %.0f %.0f %.0f under the walk %d -> %d, replanning", clientNum,
					self.eye[0], self.eye[1], self.feetZ, previousNode, node);
				DropPath(bot);
				return false;
			}
		}

		float progressSq = nodeDistSq;
		if (self.isOnLadder)
		{
			progressSq += dz * dz;
		}
		int stallLimit = 40;
		if (self.isOnLadder
			|| (bot.pathIndex > 0 && Waypoints::KindOf(bot.path[bot.pathIndex - 1], node) == Waypoints::LinkLadder))
		{
			stallLimit = 80;
		}
		if (bot.approachBestSq == 0.0f || progressSq < bot.approachBestSq - 16.0f)
		{
			bot.approachBestSq = progressSq;
			bot.approachStall = 0;
		}
		else if (isStunned || self.isMantling)
		{
			bot.approachStall = 0;
		}
		else if (++bot.approachStall > stallLimit)
		{
			AvoidNode(bot, node, now);
			if (nodeDistSq > stuckHeatMinUnits * stuckHeatMinUnits)
			{
				AddStuckHeat(node);
			}

			int from = -1;
			if (bot.pathIndex > 0)
			{
				from = bot.path[bot.pathIndex - 1];
				AddLinkTrap(from, node);
			}
			BotLog("stall client %d at %.0f %.0f %.0f node %d at %.0f %.0f %.0f %s from %d", clientNum,
				self.eye[0], self.eye[1], self.feetZ, node, nodeOrigin[0], nodeOrigin[1], nodeOrigin[2],
				LinkLabel(from, node), from);
			bot.approachStall = 0;
			bot.approachBestSq = 0.0f;
			DropPath(bot);
			if (LowBlockerAhead(self, std::atan2(dy, dx) * radToDeg))
			{
				input.buttons |= cmdButtonUp;
			}
			bot.wasWalking = true;
			return true;
		}

		bool isFirstNode = bot.pathIndex == 0;
		Waypoints::LinkKind linkKind = isFirstNode
			? Waypoints::LinkWalk
			: Waypoints::KindOf(bot.path[bot.pathIndex - 1], node);
		bool isClimbLink = linkKind != Waypoints::LinkWalk;

		float steerX = dx;
		float steerY = dy;
		bool isSmoothSteer = false;
		if (tuning.smooth != 0 && !isClimbLink)
		{
			if (debugTick >= bot.smoothRecheckTick)
			{
				bot.smoothRecheckTick = debugTick + smoothRecheckFrames;
				bot.smoothNode = -1;
				int last = bot.pathIndex + smoothLookNodes;
				if (last > bot.pathLength - 1)
				{
					last = bot.pathLength - 1;
				}
				for (int k = bot.pathIndex + 1; k <= last; ++k)
				{
					const float* candidate = Waypoints::Origin(bot.path[k]);
					if (Waypoints::KindOf(bot.path[k - 1], bot.path[k]) != Waypoints::LinkWalk
						|| (Waypoints::FlagsOf(bot.path[k - 1], bot.path[k]) & Waypoints::linkCrouch) != 0
						|| std::fabs(candidate[2] - self.feetZ) > smoothFlatDz
						|| DistanceSq2D(self.eye, candidate) > smoothReachSq
						|| !ClearWalkTo(self, candidate))
					{
						break;
					}
					bot.smoothNode = k;
				}
			}
			if (bot.smoothNode > bot.pathIndex && bot.smoothNode < bot.pathLength)
			{
				const float* smoothOrigin = Waypoints::Origin(bot.path[bot.smoothNode]);
				const float steerDistSq = DistanceSq2D(self.eye, smoothOrigin);
				bool advanced = false;
				while (bot.pathIndex < bot.smoothNode
					   && steerDistSq < DistanceSq2D(Waypoints::Origin(bot.path[bot.pathIndex]), smoothOrigin))
				{
					++bot.pathIndex;
					bot.approachBestSq = 0.0f;
					bot.approachStall = 0;
					advanced = true;
				}
				if (advanced)
				{
					node = bot.path[bot.pathIndex];
					nodeOrigin = Waypoints::Origin(node);
					dx = nodeOrigin[0] - self.eye[0];
					dy = nodeOrigin[1] - self.eye[1];
					dz = nodeOrigin[2] - self.feetZ;
					nodeDistSq = dx * dx + dy * dy;
					isFirstNode = false;
					linkKind = Waypoints::LinkWalk;
					isClimbLink = false;
				}
				steerX = smoothOrigin[0] - self.eye[0];
				steerY = smoothOrigin[1] - self.eye[1];
				isSmoothSteer = true;
			}
		}

		float lineOffset = 0.0f;
		if (!isSmoothSteer && !isClimbLink && bot.pathIndex > 0)
		{
			const float* lineFrom = Waypoints::Origin(bot.path[bot.pathIndex - 1]);
			const float segX = nodeOrigin[0] - lineFrom[0];
			const float segY = nodeOrigin[1] - lineFrom[1];
			const float segLength = std::sqrt(segX * segX + segY * segY);
			if (segLength > 1.0f)
			{
				const float relX = self.eye[0] - lineFrom[0];
				const float relY = self.eye[1] - lineFrom[1];
				const float along = (relX * segX + relY * segY) / segLength;
				lineOffset = std::fabs(relX * segY - relY * segX) / segLength;
				if (lineOffset > lineHoldUnits)
				{
					float carrot = along + lineLookAheadUnits;
					if (carrot > segLength)
					{
						carrot = segLength;
					}
					if (carrot < 0.0f)
					{
						carrot = 0.0f;
					}
					steerX = lineFrom[0] + segX / segLength * carrot - self.eye[0];
					steerY = lineFrom[1] + segY / segLength * carrot - self.eye[1];
				}
			}
		}
		const float yaw = std::atan2(steerY, steerX) * radToDeg;

		float ladderAxisYaw = yaw;
		float ladderLateral = 0.0f;
		bool hasLadderAxis = false;
		if (linkKind == Waypoints::LinkLadder && bot.pathIndex > 0)
		{
			const float* foot = Waypoints::Origin(bot.path[bot.pathIndex - 1]);
			const float axisX = nodeOrigin[0] - foot[0];
			const float axisY = nodeOrigin[1] - foot[1];
			const float axisLength = std::sqrt(axisX * axisX + axisY * axisY);
			if (axisLength > 1.0f)
			{
				const float unitX = axisX / axisLength;
				const float unitY = axisY / axisLength;
				ladderLateral = (self.eye[0] - foot[0]) * -unitY + (self.eye[1] - foot[1]) * unitX;
				ladderAxisYaw = std::atan2(unitY, unitX) * radToDeg;
				hasLadderAxis = true;
			}
		}

		bool isLadderUp = true;
		if (bot.pathIndex > 0)
		{
			isLadderUp = nodeOrigin[2] >= Waypoints::Origin(bot.path[bot.pathIndex - 1])[2];
		}
		else if (self.isOnLadder)
		{
			isLadderUp = nodeOrigin[2] >= self.feetZ - ladderDownSlack;
		}
		if (self.isOnLadder)
		{
			const float* ladderVec = reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psLadderVec);
			if (ladderVec[0] * ladderVec[0] + ladderVec[1] * ladderVec[1] > 0.01f)
			{
				if (now - bot.ladderFacingTime > ladderRegrabMs)
				{
					bot.ladderBestZ = self.feetZ;
					bot.ladderStallFrames = 0;
					bot.ladderShuffleUntilTime = 0;
				}
				bot.ladderFacingYaw = std::atan2(-ladderVec[1], -ladderVec[0]) * radToDeg;
				bot.ladderFacingTime = now;
				ladderAxisYaw = bot.ladderFacingYaw;
				ladderLateral = 0.0f;
				hasLadderAxis = true;
				if (std::fabs(self.feetZ - bot.ladderBestZ) > ladderClimbMinRise)
				{
					bot.ladderBestZ = self.feetZ;
					bot.ladderStallFrames = 0;
				}
				else if (++bot.ladderStallFrames > ladderStallLimit && now >= bot.ladderShuffleUntilTime && isLadderUp)
				{
					bot.ladderStallFrames = 0;
					bot.ladderShuffleSide = PickLadderShuffleSide(self, bot.ladderFacingYaw);
					if (bot.ladderShuffleSide != 0)
					{
						bot.ladderShuffleUntilTime = now + ladderShuffleMs;
						BotLog("ladder client %d blocked at %.0f %.0f %.0f, shuffles %s", clientNum, self.eye[0],
							self.eye[1], self.feetZ, bot.ladderShuffleSide > 0 ? "right" : "left");
					}
				}
			}
		}
		else if (linkKind == Waypoints::LinkLadder && isLadderUp && now - bot.ladderFacingTime < ladderExitMs)
		{
			ladderAxisYaw = bot.ladderFacingYaw;
			ladderLateral = 0.0f;
			hasLadderAxis = true;
		}

		if (((debugTick + clientNum) % 3) == 0)
		{
			bot.isInNarrowGap = false;
			bot.isInHall = false;
			bot.gapCorrectionDeg = 0.0f;
			if (!isClimbLink && !self.isOnLadder)
			{
				const float left = ClearanceAt(self, yaw + 90.0f, gapProbeHeight, gapProbeReach);
				const float right = ClearanceAt(self, yaw - 90.0f, gapProbeHeight, gapProbeReach);
				const bool isWalledBothSides = left < gapProbeReach && right < gapProbeReach;
				if (isWalledBothSides && left + right < narrowGapWidth)
				{
					bot.isInNarrowGap = true;
					bot.gapCorrectionDeg = std::atan2((left - right) * 0.5f, gapLookAheadUnits) * radToDeg;
				}
				else if (isWalledBothSides && left + right < hallWidth)
				{
					bot.isInHall = true;
					bot.gapCorrectionDeg = std::atan2((left - right) * 0.5f, gapLookAheadUnits) * radToDeg;
				}
				else if (std::fabs(dz) <= smoothFlatDz && nodeDistSq > wallKeepNodeSq)
				{
					if (right < wallKeepUnits && left > right)
					{
						bot.gapCorrectionDeg = std::atan2(wallKeepUnits - right, wallLookAheadUnits) * radToDeg;
					}
					else if (left < wallKeepUnits && right > left)
					{
						bot.gapCorrectionDeg = -std::atan2(wallKeepUnits - left, wallLookAheadUnits) * radToDeg;
					}
				}
				if (!bot.isInNarrowGap && bot.gapCorrectionDeg > wallMaxCorrectionDeg)
				{
					bot.gapCorrectionDeg = wallMaxCorrectionDeg;
				}
				if (!bot.isInNarrowGap && bot.gapCorrectionDeg < -wallMaxCorrectionDeg)
				{
					bot.gapCorrectionDeg = -wallMaxCorrectionDeg;
				}
			}
		}

		const int bodySide = BodySide(clientNum, self, yaw);
		const bool isQueuing = bodySide != 0 && bot.isInNarrowGap;
		if (bodySide != 0)
		{
			bot.stuckFrames = 0;
			if (!isQueuing)
			{
				bot.steerSide = bodySide;
				bot.steerFrames = sidestepFrames;
			}
		}

		float travelYaw = yaw;
		if (self.isOnLadder)
		{
			bot.steerFrames = 0;
		}
		else if (bot.steerFrames > 0)
		{
			--bot.steerFrames;
			travelYaw = yaw + static_cast<float>(bot.steerSide) * steerAngleDeg;
		}
		else if (!isClimbLink && std::fabs(dz) <= 20.0f + 0.6f * std::sqrt(nodeDistSq)
				 && ((debugTick + clientNum) % 3) == 0 && BlockedAhead(self, yaw))
		{
			bot.steerSide = PickSteerSide(self, yaw, bot.steerSide);
			bot.steerFrames = pathSteerFrames;
			travelYaw = yaw + static_cast<float>(bot.steerSide) * steerAngleDeg;
		}
		const bool isSidestepping = travelYaw != yaw;
		if (hasLadderAxis)
		{
			travelYaw = ladderAxisYaw - std::atan2(ladderLateral, ladderCenterUnits) * radToDeg;
			if (self.isOnLadder && now < bot.ladderShuffleUntilTime)
			{
				travelYaw = ladderAxisYaw - 90.0f * static_cast<float>(bot.ladderShuffleSide);
			}
		}
		else if (!isSidestepping && (bot.isInNarrowGap || bot.isInHall || lineOffset < lineHoldUnits))
		{
			travelYaw += bot.gapCorrectionDeg;
		}

		const bool isAlert = IsAlert(clientNum, self, now);
		const bool isKnifing = allowTurn && KnifeGlassAhead(clientNum, self, yaw, input);

		bool isStraight = true;
		int aheadIndex = bot.pathIndex + sprintLookNodes;
		if (aheadIndex > bot.pathLength - 1)
		{
			aheadIndex = bot.pathLength - 1;
		}
		if (aheadIndex > bot.pathIndex)
		{
			const float* farNode = Waypoints::Origin(bot.path[aheadIndex]);
			const float farX = farNode[0] - self.eye[0];
			const float farY = farNode[1] - self.eye[1];
			const float farLength = std::sqrt(farX * farX + farY * farY);
			const float nodeLength = std::sqrt(nodeDistSq);
			if (farLength > 1.0f && nodeLength > 1.0f)
			{
				isStraight = (farX * dx + farY * dy) / (farLength * nodeLength) >= sprintStraightDot;
			}
		}
		if (now >= bot.sneakCheckTime)
		{
			bot.sneakCheckTime = now + sneakCheckMs;
			const bool isEnemyNear = !bot.personality.isMeleeOnly && IsKnownEnemyNear(clientNum, self, sneakRangeUnits);
			if (isEnemyNear && !bot.isSneaking && !bot.hasSneakRoll)
			{
				bot.hasSneakRoll = true;
				bot.isSneaking = RollPercent(sneakBasePercent + bot.personality.caution / 2);
				if (bot.isSneaking)
				{
					BotLog("sneak client %d walks: a known enemy is within %.0f", clientNum, sneakRangeUnits);
				}
			}
			if (!isEnemyNear)
			{
				bot.isSneaking = false;
				bot.hasSneakRoll = false;
			}
		}
		const bool wantsSprint = !isAlert && !isKnifing && !isClimbLink && !self.isOnLadder
			&& !isSidestepping && !isQueuing && debugTick >= bot.cornerSlowUntilTick && now >= bot.pauseUntilTime
			&& !bot.isNearFollowMate && !bot.isSneaking && MaySprint(clientNum, now, isStraight);

		const bool isClimbing = self.isOnLadder || linkKind == Waypoints::LinkLadder;
		const bool isAtClimbUp = (linkKind == Waypoints::LinkMantle || linkKind == Waypoints::LinkJump)
			&& nodeDistSq < climbFaceReachSq && bot.turnCount == 0;
		bool isRouteLook = false;
		float routeLookYaw = 0.0f;
		if (isKnifing)
		{
			bot.stuckFrames = 0;
		}
		else if ((allowTurn || isAtClimbUp) && !isClimbing)
		{
			float lookYaw = yaw;
			float lookPitch = 0.0f;
			const bool mayLead = !isClimbLink && bot.pathIndex + 1 < bot.pathLength;
			if (mayLead)
			{
				float lookPoint[3];
				if (PickLookPoint(clientNum, self, yaw, lookPoint))
				{
					const float lookX = lookPoint[0] - self.eye[0];
					const float lookY = lookPoint[1] - self.eye[1];
					const float lookZ = lookPoint[2] - self.eye[2];
					lookYaw = std::atan2(lookY, lookX) * radToDeg;
					lookPitch = -std::atan2(lookZ, std::sqrt(lookX * lookX + lookY * lookY)) * radToDeg;
					if (std::fabs(lookPitch) < lookPitchDeadbandDeg)
					{
						lookPitch = 0.0f;
					}
					float lookLead = AngleDelta(yaw, lookYaw);
					if (lookLead > walkLookLeadDeg)
					{
						lookLead = walkLookLeadDeg;
					}
					if (lookLead < -walkLookLeadDeg)
					{
						lookLead = -walkLookLeadDeg;
					}
					lookYaw = yaw + lookLead;
				}
				else
				{
					const float* ahead = Waypoints::Origin(bot.path[bot.pathIndex + 1]);
					if (bot.cornerPathIndex != bot.pathIndex)
					{
						bot.cornerPathIndex = bot.pathIndex;
						bot.hasRevealPoint = false;
						const int cornerChance = ScaleByTrait(
							ScaleByTrait(cornerSlowPercent, disciplineBySkill[bot.skillIndex]), bot.personality.caution);
						if (!bot.personality.isQuickscoper && RollPercent(cornerChance))
						{
							bot.cornerSlowUntilTick = debugTick + cornerSlowFrames + bot.personality.caution / 20;
							bot.hasRevealPoint = TryFindRevealPoint(clientNum, bot.revealPoint);
							if (bot.hasRevealPoint)
							{
								BotLog("corner client %d slows at node %d and pre-aims where %.0f %.0f %.0f comes into view",
									clientNum, node, bot.revealPoint[0], bot.revealPoint[1], bot.revealPoint[2]);
							}
							else
							{
								BotLog("corner client %d slows and pre-aims at node %d", clientNum, node);
							}
						}
					}
					const bool isCornerSlow = debugTick < bot.cornerSlowUntilTick;
					const float* aimAt = ahead;
					if (isCornerSlow && bot.hasRevealPoint)
					{
						aimAt = bot.revealPoint;
					}
					const float aheadX = aimAt[0] - self.eye[0];
					const float aheadY = aimAt[1] - self.eye[1];
					const float aheadZ = aimAt[2] - self.feetZ;
					const float aheadYaw = std::atan2(aheadY, aheadX) * radToDeg;
					float leadLimit = leadMaxDeg;
					if (isCornerSlow)
					{
						leadLimit = cornerLeadDeg;
					}
					float lead = AngleDelta(yaw, aheadYaw);
					if (lead > leadLimit)
					{
						lead = leadLimit;
					}
					if (lead < -leadLimit)
					{
						lead = -leadLimit;
					}
					lookYaw = yaw + lead;
					lookPitch = -std::atan2(aheadZ, std::sqrt(aheadX * aheadX + aheadY * aheadY)) * radToDeg;
				}

				if (wantsSprint)
				{
					float offset = AngleDelta(travelYaw, lookYaw)
						+ std::sin(static_cast<float>(now) * 0.0018f + bot.personality.wobblePhase) * sprintHeadWanderDeg;
					const float limit = sprintAlignDeg * 0.6f;
					if (offset > limit)
					{
						offset = limit;
					}
					if (offset < -limit)
					{
						offset = -limit;
					}
					lookYaw = travelYaw + offset;
				}
				if (!isAlert && !wantsSprint)
				{
					lookYaw += GlanceYaw(clientNum, self, lookYaw, now);

					if (now >= bot.walkPitchNextTime)
					{
						bot.walkPitchNextTime = now + IrandMs(walkPitchGapMinMs, walkPitchGapMaxMs);
						const float steadiness = static_cast<float>(100 - bot.personality.sightDiscipline) / 100.0f;
						bot.walkPitchDeg = Flrand(walkPitchMinDeg, walkPitchMaxDeg) * steadiness;
					}
					lookPitch += bot.walkPitchDeg;
				}
			}

			if (!isAlert && !wantsSprint && bot.task.kind == TaskHunt && bot.huntKnownClient == bot.task.client
				&& bot.huntKnownTime != 0 && now - bot.huntKnownTime < huntFaceKnownMs)
			{
				if (bot.threatFaceClient != bot.task.client)
				{
					bot.threatFaceClient = bot.task.client;
					bot.facesThreat = RollPercent(huntFacePercentBySkill[bot.skillIndex]);
				}
				const float threatX = bot.huntKnownPos[0] - self.eye[0];
				const float threatY = bot.huntKnownPos[1] - self.eye[1];
				const float threatYaw = std::atan2(threatY, threatX) * radToDeg;
				if (bot.facesThreat && threatX * threatX + threatY * threatY < huntFaceRangeSq
					&& std::fabs(AngleDelta(travelYaw, threatYaw)) < huntFaceMaxOffDeg
					&& IsViewOpen(clientNum, self, threatYaw, huntFaceOpenUnits))
				{
					lookYaw = threatYaw;
					lookPitch = 0.0f;
				}
			}
			isRouteLook = true;
			routeLookYaw = lookYaw;
			TurnView(clientNum, input, lookYaw, lookPitch, "route");
		}

		if (isClimbing && (allowTurn || self.isOnLadder))
		{
			float climbPitch = -std::atan2(dz, std::sqrt(nodeDistSq)) * radToDeg;
			if (self.isOnLadder)
			{
				climbPitch = ladderClimbPitchDeg;
				if (!isLadderUp)
				{
					climbPitch = -ladderClimbPitchDeg;
				}
			}
			TurnView(clientNum, input, ladderAxisYaw, climbPitch, "climb");
		}

		if (now >= bot.pauseNextTime)
		{
			bot.pauseNextTime = now + IrandMs(pauseGapMinMs, pauseGapMaxMs);
			if (allowTurn && !isAlert && !isKnifing && !isClimbing && !bot.personality.isQuickscoper
				&& !bot.isInNarrowGap && !bot.isInHall && std::fabs(dz) <= smoothFlatDz
				&& RollPercent(ScaleByTrait(pauseChancePercent, 100 - bot.personality.aggression)))
			{
				bot.pauseUntilTime = now + IrandMs(pauseMinMs, pauseMaxMs);
				if (now >= bot.glanceUntilTime)
				{
					const float side = (NextRand() & 1u) ? 1.0f : -1.0f;
					bot.glanceYawDeg = side * Flrand(pauseGlanceMinDeg, pauseGlanceMaxDeg);
					bot.glanceUntilTime = bot.pauseUntilTime;
				}
				BotLog("pause client %d stops for %d ms at %.0f %.0f %.0f", clientNum, bot.pauseUntilTime - now,
					self.eye[0], self.eye[1], self.feetZ);
			}
		}
		if (now < bot.pauseUntilTime)
		{
			input.forward = 0;
			input.right = 0;
			input.buttons &= ~cmdButtonSprint;
			bot.wasWalking = false;
			bot.navMode = "pause";
			return true;
		}

		MoveTowards(travelYaw, input.angles[1], input);
		bot.lastTravelYaw = travelYaw;
		bool isTurningInPlace = false;
		const float viewOffTravelDeg = std::fabs(AngleDelta(input.angles[1], travelYaw));
		if (isRouteLook && !isAlert && !isClimbLink && !self.isOnLadder && !isSidestepping
			&& std::fabs(AngleDelta(routeLookYaw, travelYaw)) < turnFirstLookDeg)
		{
			if (viewOffTravelDeg > turnFirstStopDeg)
			{
				input.forward = 0;
				input.right = 0;
				isTurningInPlace = true;
			}
			else if (viewOffTravelDeg > turnFirstSlowDeg)
			{
				input.forward = static_cast<signed char>(static_cast<float>(input.forward) * turnFirstWalkScale);
				input.right = static_cast<signed char>(static_cast<float>(input.right) * turnFirstWalkScale);
				bot.wasSlow = true;
			}
		}
		if (isQueuing)
		{
			input.forward = static_cast<signed char>(static_cast<float>(input.forward) * queueWalkScale);
			input.right = static_cast<signed char>(static_cast<float>(input.right) * queueWalkScale);
		}
		if (now >= bot.walkScaleNextTime)
		{
			bot.walkScaleNextTime = now + IrandMs(walkScaleGapMinMs, walkScaleGapMaxMs);
			bot.walkScale = Flrand(walkScaleMin, 1.0f);
		}
		if (bot.walkScale > 0.0f)
		{
			input.forward = static_cast<signed char>(static_cast<float>(input.forward) * bot.walkScale);
			input.right = static_cast<signed char>(static_cast<float>(input.right) * bot.walkScale);
		}
		if (debugTick < bot.cornerSlowUntilTick)
		{
			input.forward = static_cast<signed char>(static_cast<float>(input.forward) * cornerWalkScale);
			input.right = static_cast<signed char>(static_cast<float>(input.right) * cornerWalkScale);
			bot.wasSlow = true;
		}
		if ((debugTick < bot.cornerSlowUntilTick || bot.isSneaking) && !isClimbLink && !self.isOnLadder)
		{
			const WeaponInfo heldWeapon = ReadWeapon(PlayerStateOf(clientNum));
			if (heldWeapon.index && (heldWeapon.weapClass == weapClassRifle || heldWeapon.weapClass == weapClassMg))
			{
				input.buttons |= cmdButtonAds;
				bot.wasSlow = true;
			}
		}

		ApplyNodeStance(node, input);
		if (bot.pathIndex > 0)
		{
			ApplyNodeStance(bot.path[bot.pathIndex - 1], input);

			if ((Waypoints::FlagsOf(bot.path[bot.pathIndex - 1], node) & Waypoints::linkCrouch) != 0)
			{
				input.buttons |= cmdButtonCrouch;
			}
		}
		if ((input.buttons & cmdButtonCrouch) != 0)
		{
			bot.wasSlow = true;
		}

		if (isAlert)
		{
			if (allowTurn)
			{
				WalkAimed(clientNum, input);
			}
			if (!bot.wasAlert && bot.crouchEndTime <= now
				&& RollPercent(ScaleByTrait(crouchChancePercent, bot.personality.caution)))
			{
				bot.crouchEndTime = now + IrandMs(crouchHoldMinMs, crouchHoldMaxMs);
				BotLog("alert client %d creeps crouched", clientNum);
			}
			if (bot.crouchEndTime > now)
			{
				input.buttons |= cmdButtonCrouch;
			}
		}
		else if (wantsSprint && (input.buttons & (cmdButtonCrouch | cmdButtonProne | cmdButtonUp)) == 0
				 && viewOffTravelDeg < turnFirstSlowDeg)
		{
			input.buttons |= cmdButtonSprint;
			if (viewOffTravelDeg < sprintAlignDeg)
			{
				input.forward = 127;
				input.right = 0;
			}
		}
		bot.wasAlert = isAlert;

		const bool isClimbUpLink = linkKind == Waypoints::LinkMantle || linkKind == Waypoints::LinkJump;
		const bool mayJump = !self.isOnLadder && (isFirstNode || isClimbUpLink);
		bool wantsClimb = dz > 20.0f;
		float jumpReach = 80.0f;
		if (isClimbUpLink)
		{
			wantsClimb = true;
			const float* previous = Waypoints::Origin(bot.path[bot.pathIndex - 1]);
			const float linkLength = std::sqrt(DistanceSq2D(previous, nodeOrigin));
			if (linkLength + climbReachMargin > jumpReach)
			{
				jumpReach = linkLength + climbReachMargin;
			}
		}
		if (mayJump && wantsClimb && nodeDistSq < jumpReach * jumpReach
			&& (BlockedAheadAt(self, yaw, lowFaceProbeHeight) || BlockedAhead(self, yaw))
			&& (linkKind != Waypoints::LinkMantle || MantleFaceAhead(self, yaw)))
		{
			if (bot.lastAssistLogNode != node)
			{
				bot.lastAssistLogNode = node;
				const char* climbLabel = "mantle";
				if (linkKind == Waypoints::LinkJump)
				{
					climbLabel = "jump";
				}
				BotLog("%s client %d at %.0f %.0f %.0f node %d dz %.0f", climbLabel, clientNum,
					self.eye[0], self.eye[1], self.feetZ, node, dz);
			}
			if (((debugTick + clientNum) % 12) < 6)
			{
				input.buttons |= cmdButtonUp;
				input.buttons &= ~(cmdButtonCrouch | cmdButtonProne);
			}
			input.buttons &= ~cmdButtonSprint;
		}

		const bool mayHop = !self.isOnLadder && (isFirstNode || linkKind == Waypoints::LinkDrop);
		if (mayHop && dz < -40.0f && nodeDistSq < (160.0f * 160.0f)
			&& ((debugTick + clientNum) % 8) == 0
			&& (BlockedAhead(self, yaw) || BlockedAheadAt(self, yaw, 20.0f) || bot.approachStall > 8))
		{
			if (bot.lastAssistLogNode != node)
			{
				bot.lastAssistLogNode = node;
				BotLog("drop client %d at %.0f %.0f %.0f node %d dz %.0f", clientNum,
					self.eye[0], self.eye[1], self.feetZ, node, dz);
			}
			input.buttons |= cmdButtonUp;
			input.buttons &= ~(cmdButtonCrouch | cmdButtonProne);
		}

		if (bot.pathIndex + 1 < bot.pathLength
			&& Waypoints::KindOf(node, bot.path[bot.pathIndex + 1]) == Waypoints::LinkLadder)
		{
			bot.noSprintUntilTime = now + ladderApproachNoSprintMs;
			input.buttons &= ~cmdButtonSprint;
		}
		if (isClimbing)
		{
			input.buttons &= ~(cmdButtonCrouch | cmdButtonProne | cmdButtonSprint);
			bot.noSprintUntilTime = now + ladderApproachNoSprintMs;
			if (self.isOnLadder)
			{
				input.buttons &= ~cmdButtonUp;
			}
			else if (bot.approachStall > 30 && dz > 20.0f && nodeDistSq < (120.0f * 120.0f)
					 && BlockedAhead(self, yaw) && now - bot.ladderJumpTime > ladderJumpGapMs)
			{
				bot.ladderJumpTime = now;
				input.buttons |= cmdButtonUp;
				BotLog("ladder client %d jumps for the grab at node %d, %.0f up", clientNum, node, dz);
			}
		}

		bot.wasWalking = !isTurningInPlace;
		const bool isProne = (input.buttons & cmdButtonProne) != 0;
		if (isProne != bot.wasProne)
		{
			bot.stanceGraceUntilTick = debugTick + stanceGraceFrames;
		}
		bot.wasProne = isProne;
		return true;
	}


	static bool TryYield(int clientNum, const PlayerView& self, BotInput& input, int now)
	{
		BotState& bot = bots[clientNum];
		if (now < bot.yieldUntilTime)
		{
			MoveTowards(bot.yieldYaw, input.angles[1], input);
			input.buttons &= ~cmdButtonSprint;
			bot.navMode = "yield";
			return true;
		}
		if (self.team == 0)
		{
			return false;
		}
		const float* selfVelocity = reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psVelocity);
		if (selfVelocity[0] * selfVelocity[0] + selfVelocity[1] * selfVelocity[1] > yieldStillSpeedSq)
		{
			return false;
		}

		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int other = 0; other < numClients; ++other)
		{
			if (other == clientNum)
			{
				continue;
			}
			const PlayerView mate = ReadPlayerView(other);
			if (!mate.isPlaying || mate.team != self.team || std::fabs(mate.feetZ - self.feetZ) > yieldHeightUnits
				|| DistanceSq2D(mate.eye, self.eye) > yieldRangeSq)
			{
				continue;
			}
			const float* mateVelocity = reinterpret_cast<const float*>(PlayerStateOf(other) + psVelocity);
			const float mateSpeed = std::sqrt(mateVelocity[0] * mateVelocity[0] + mateVelocity[1] * mateVelocity[1]);
			if (mateSpeed < yieldMateSpeed)
			{
				continue;
			}
			const float toUsX = self.eye[0] - mate.eye[0];
			const float toUsY = self.eye[1] - mate.eye[1];
			const float toUsLength = std::sqrt(toUsX * toUsX + toUsY * toUsY);
			if (toUsLength < 1.0f)
			{
				continue;
			}
			const float heading = (mateVelocity[0] * toUsX + mateVelocity[1] * toUsY) / (mateSpeed * toUsLength);
			if (heading < yieldHeadingDot)
			{
				continue;
			}

			const float travelYaw = std::atan2(mateVelocity[1], mateVelocity[0]) * radToDeg;
			float side = -1.0f;
			if (mateVelocity[0] * toUsY - mateVelocity[1] * toUsX >= 0.0f)
			{
				side = 1.0f;
			}
			float yieldYaw = travelYaw + side * 90.0f;
			if (ClearanceAt(self, yieldYaw, gapProbeHeight, yieldStepUnits) < yieldStepUnits)
			{
				yieldYaw = travelYaw - side * 90.0f;
				if (ClearanceAt(self, yieldYaw, gapProbeHeight, yieldStepUnits) < yieldStepUnits)
				{
					yieldYaw = travelYaw;
				}
			}
			bot.yieldYaw = yieldYaw;
			bot.yieldUntilTime = now + yieldMs;
			BotLog("yield client %d steps aside for client %d at %.0f %.0f %.0f", clientNum, other,
				self.eye[0], self.eye[1], self.feetZ);
			MoveTowards(bot.yieldYaw, input.angles[1], input);
			input.buttons &= ~cmdButtonSprint;
			bot.navMode = "yield";
			return true;
		}
		return false;
	}


	void Idle(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn)
	{
		BotState& bot = bots[clientNum];
		const int now = ServerTimeMs();
		if (TryYield(clientNum, self, input, now))
		{
			return;
		}
		if (allowTurn && now < bot.arriveLookUntilTime && !IsHolding(clientNum))
		{
			input.forward = 0;
			input.right = 0;
			input.buttons &= ~cmdButtonSprint;
			bot.wasWalking = false;
			bot.navMode = "look";
			const float phase = static_cast<float>(now - bot.arriveLookStartTime)
				* (6.2831853f / static_cast<float>(arriveSweepPeriodMs));
			TurnView(clientNum, input, bot.arriveLookYaw + bot.sweepSign * std::sin(phase) * arriveSweepDeg, 0.0f, "arrive");
			return;
		}
		if (RunNavigation(clientNum, self, input, allowTurn))
		{
			return;
		}
		input.forward = 0;
		input.right = 0;
		if (HoldView(clientNum, self, input, allowTurn))
		{
			return;
		}

		const Task& task = bot.task;
		if (allowTurn && task.kind == TaskFollow && task.client >= 0 && ReadPlayerView(task.client).isPlaying)
		{
			if (now >= bot.followLookNextTime)
			{
				bot.followLookNextTime = now + IrandMs(followLookMinMs, followLookMaxMs);
				bot.followLookYaw = PickLookYaw(clientNum, self);
			}
			TurnView(clientNum, input, bot.followLookYaw, 0.0f, "follow");
		}
	}

	int NearestBotPath(const float* origin, const short** outPath, int* outLength, int* outIndex)
	{
		int best = -1;
		float bestSq = 0.0f;
		const int numClients = *reinterpret_cast<int*>(svs_numClients);

		for (int i = 0; i < numClients && i < maxClients; ++i)
		{
			if (bots[i].pathLength <= 0 || bots[i].pathIndex >= bots[i].pathLength)
			{
				continue;
			}

			const PlayerView view = ReadPlayerView(i);

			if (!view.isPlaying)
			{
				continue;
			}

			const float distanceSq = DistanceSq2D(view.eye, origin);

			if (best < 0 || distanceSq < bestSq)
			{
				best = i;
				bestSq = distanceSq;
			}
		}

		if (best < 0)
		{
			return -1;
		}

		*outPath = bots[best].path;
		*outLength = bots[best].pathLength;
		*outIndex = bots[best].pathIndex;
		return best;
	}
}
