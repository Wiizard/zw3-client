#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <cmath>

namespace Components::BotAI
{
	static constexpr int   maxTracesPerScan = 3;
	static constexpr int   retargetFrames   = 5;


	static constexpr int   rescanFrames     = 10;

	static constexpr float switchRatio      = 0.25f;

	static constexpr int   switchHiddenMs   = 1000;
	static constexpr int   switchCooldownMs = 1500;

	static constexpr float hurtBearingConeDeg   = 45.0f;
	static constexpr float damageBearingFlipDeg = 180.0f;
	static constexpr int   hurtLookMs           = 1200;
	static constexpr int   listenMs             = 1000;
	static constexpr int   listenCooldownMs     = 4000;
	static constexpr float listenFuzzDeg        = 20.0f;
	static constexpr float listenOpenUnits      = 350.0f;
	static constexpr float listenPitchMaxDeg    = 8.0f;
	static constexpr int   listenSearchSteps    = 3;
	static constexpr float listenSearchStepDeg  = 20.0f;

	static constexpr float plainSwitchNearSq = 250.0f * 250.0f;

	static constexpr int   helpPerShout     = 2;
	static constexpr int   helpCooldownMs   = 10000;
	static constexpr int   helpDelayMinMs   = 500;
	static constexpr int   helpDelayMaxMs   = 2000;

	static constexpr int   gunfireMs           = 600;
	static constexpr int   worldHurtShotMs     = 500;

	static constexpr float gunfireRangeMulti   = 3.0f;

	static constexpr int   shooterSwitchCooldownMs = 1500;

	static constexpr float heardSpeedSq     = 40000.0f;

	static constexpr int   hearCooldownMs   = 7000;
	static constexpr float hearPushMaxSq    = 1200.0f * 1200.0f;
	static constexpr float hearFuzzUnits    = 250.0f;
	static constexpr float footstepWallShare = 0.5f;
	static constexpr float gunfireWallShare = 0.7f;


	static int lastClipRounds[maxClients] = {};
	static int lastClipWeapon[maxClients] = {};
	static int firedTime[maxClients] = {};
	static bool firedSilenced[maxClients] = {};
	static float firedPos[maxClients][3] = {};
	static int gunfireTick = -1;

	static void UpdateGunfire(int now)
	{
		if (gunfireTick == debugTick || (debugTick & 1) != 0)
		{
			return;
		}
		gunfireTick = debugTick;

		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			const char* ps = PlayerStateOf(i);
			const WeaponInfo weapon = ReadWeapon(ps);
			if (weapon.index && weapon.index == lastClipWeapon[i] && weapon.clipRounds < lastClipRounds[i])
			{
				firedTime[i] = now;
				firedSilenced[i] = weapon.isSilenced;
				FeetOf(ReadPlayerView(i), firedPos[i]);
			}
			lastClipRounds[i] = weapon.clipRounds;
			lastClipWeapon[i] = weapon.index;
		}
	}


	static bool HasEnemyFiredLately(const PlayerView& self, int now)
	{
		UpdateGunfire(now);
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			if (firedTime[i] == 0 || now - firedTime[i] > worldHurtShotMs)
			{
				continue;
			}
			if (IsEnemy(self, ReadPlayerView(i)))
			{
				return true;
			}
		}
		return false;
	}


	void ForgetGunfire()
	{
		for (int i = 0; i < maxClients; ++i)
		{
			lastClipRounds[i] = 0;
			lastClipWeapon[i] = 0;
			firedTime[i] = 0;
			firedSilenced[i] = false;
		}
		gunfireTick = -1;
	}


	bool LastShotOf(int clientNum, float outFeet[3], int* outTime)
	{
		if (clientNum < 0 || clientNum >= maxClients || firedTime[clientNum] == 0 || firedSilenced[clientNum])
		{
			return false;
		}
		outFeet[0] = firedPos[clientNum][0];
		outFeet[1] = firedPos[clientNum][1];
		outFeet[2] = firedPos[clientNum][2];
		*outTime = firedTime[clientNum];
		return true;
	}


	static void ReadVelocity(int clientNum, float out[3])
	{
		const char* ps = PlayerStateOf(clientNum);
		const float* v = reinterpret_cast<const float*>(ps + psVelocity);
		out[0] = v[0];
		out[1] = v[1];
		out[2] = v[2];
	}


	static void CallForHelp(int clientNum, const PlayerView& self, int attacker, int now)
	{
		const int helpUnits = tuning.help;
		if (helpUnits <= 0 || self.team == 0)
		{
			return;
		}
		const float helpSq = static_cast<float>(helpUnits) * static_cast<float>(helpUnits);
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		float here[3];
		FeetOf(self, here);
		int answered = 0;
		for (int k = 0; k < numClients && answered < helpPerShout; ++k)
		{
			BotState& mate = bots[k];
			if (k == clientNum || !mate.input.isActive || mate.targetClient >= 0 || TaskHuntClient(k) == attacker
				|| now < mate.helpCooldownTime)
			{
				continue;
			}
			const PlayerView view = ReadPlayerView(k);
			if (!view.isPlaying || view.team != self.team)
			{
				continue;
			}
			if (DistanceSq2D(view.eye, self.eye) > helpSq)
			{
				continue;
			}
			if (!RollPercent(mate.personality.teamplay))
			{
				continue;
			}

			if (mate.pendingHelpTime != 0)
			{
				continue;
			}
			mate.pendingHelpTime = now + IrandMs(helpDelayMinMs, helpDelayMaxMs);
			mate.pendingHelpAttacker = attacker;
			mate.pendingHelpFrom = clientNum;
			mate.pendingHelpPoint[0] = here[0];
			mate.pendingHelpPoint[1] = here[1];
			mate.pendingHelpPoint[2] = here[2];
			mate.helpCooldownTime = now + JitterMs(helpCooldownMs);
			++answered;
		}
	}


	void ReadHurt(int clientNum, const PlayerView& self, int now)
	{
		BotState& bot = bots[clientNum];
		const char* playerState = PlayerStateOf(clientNum);
		const int damageEvent = *reinterpret_cast<const int*>(playerState + psDamageEvent);
		if (!bot.hasDamageBaseline)
		{
			bot.hasDamageBaseline = true;
			bot.lastDamageEvent = damageEvent;
			return;
		}
		if (damageEvent == bot.lastDamageEvent)
		{
			return;
		}
		bot.lastDamageEvent = damageEvent;

		const int yawByte = *reinterpret_cast<const int*>(playerState + psDamageYaw);
		const int pitchByte = *reinterpret_cast<const int*>(playerState + psDamagePitch);
		const int count = *reinterpret_cast<const int*>(playerState + psDamageCount);
		if (yawByte == 255 && pitchByte == 255)
		{
			BotLog("hurt client %d by the world for %d", clientNum, count);
			return;
		}

		bot.hurtUntilTime = now + hurtWindowMs;
		bot.hurtScanPending = true;
		bot.flinchUntilTime = now + flinchMs;
		if (now - bot.lastFlinchTime >= flinchGapMs)
		{
			bot.flinchPending = true;
			bot.flinchDamage = count;
			bot.lastFlinchTime = now;
		}
		bot.noSprintUntilTime = now + noSprintAfterMs;

		const float bearing = AngleDelta(0.0f, static_cast<float>(yawByte) * (360.0f / 256.0f) + damageBearingFlipDeg);
		bot.hasHurtBearing = true;
		bot.hurtBearingYaw = bearing;
		bot.hurtLookUntilTime = now + hurtLookMs;

		BotLog("hurt client %d for %d from bearing %.0f (yaw byte %d pitch byte %d) at %.0f %.0f %.0f", clientNum,
			count, bearing, yawByte, pitchByte, self.eye[0], self.eye[1], self.feetZ);

		const bool mayBeWorld = bot.targetClient < 0 && count < worldHurtMaxDamage && !HasEnemyFiredLately(self, now);
		if (!mayBeWorld || now - bot.worldHurtTime > worldHurtGapMs)
		{
			bot.worldHurtCount = mayBeWorld ? 1 : 0;
			bot.worldHurtTime = now;
			return;
		}

		bot.worldHurtTime = now;
		if (++bot.worldHurtCount < worldHurtRepeats)
		{
			return;
		}

		bot.worldHurtCount = 0;
		bot.coverCooldownTime = now + coverCooldownMs;

		float feet[3];
		FeetOf(self, feet);
		const int here = Waypoints::Nearest(feet);
		if (here >= 0)
		{
			AddStuckHeat(here);
		}
		BotLog("hazard client %d at %.0f %.0f %.0f node %d: hurt on a beat with nobody to blame",
			clientNum, self.eye[0], self.eye[1], self.feetZ, here);

		const int away = here >= 0 ? PickRoamGoal(clientNum, self, feet, here) : -1;
		if (away >= 0)
		{
			RequestPush(clientNum, Waypoints::Origin(away), PriorityLost, "hazard");
		}
	}


	static bool TryOpenYawToward(int clientNum, const PlayerView& self, float yawDeg, float range, float* outYaw)
	{
		for (int step = 0; step <= listenSearchSteps; ++step)
		{
			const float offset = static_cast<float>(step) * listenSearchStepDeg;
			if (IsViewOpen(clientNum, self, yawDeg + offset, range))
			{
				*outYaw = yawDeg + offset;
				return true;
			}
			if (step > 0 && IsViewOpen(clientNum, self, yawDeg - offset, range))
			{
				*outYaw = yawDeg - offset;
				return true;
			}
		}
		return false;
	}


	int SelectTarget(int clientNum, const PlayerView& self, const WeaponInfo& selfWeapon,
					 const BotInput& input, bool useTraces, int now, PlayerView* outTarget)
	{
		BotState& bot = bots[clientNum];
		const BotSkill& skill = bot.skillRow;
		const Personality& personality = bot.personality;
		const float distMulti = selfWeapon.distMulti;

		int chosen = bot.targetClient;
		PlayerView target = chosen >= 0 ? ReadPlayerView(chosen) : PlayerView{};
		if (chosen >= 0 && (!IsEnemy(self, target) || (tuning.ignoreHumans != 0 && IsHuman(chosen))))
		{
			if (now - bot.lastFireTime < 300)
			{
				const int confirmMs = bot.ping + confirmReactionMsBySkill[bot.skillIndex] + IrandMs(0, confirmJitterMs);
				if (selfWeapon.weapClass != weapClassSniper && selfWeapon.isFullAuto)
				{
					bot.lingerUntilTime = now + confirmMs;
				}
				bot.confirmUntilTime = now + confirmMs;
				bot.postKillUntilTime = bot.confirmUntilTime + IrandMs(postKillMinMs, postKillMaxMs);
				bot.sweepSign = (NextRand() & 1u) ? 1.0f : -1.0f;
				OnKillConfirmed(clientNum);
			}
			chosen = -1;
		}

		const bool isHurt = now < bot.hurtUntilTime;
		const int forcedTarget = ForcedTargetFor(clientNum, self);
		const bool isFightingBack = isHurt && chosen == bot.lastAttacker;
		if (chosen >= 0 && forcedTarget >= 0 && chosen != forcedTarget && !isFightingBack)
		{
			chosen = -1;
			target = PlayerView{};
		}

		const bool isPostKill = chosen < 0 && now < bot.confirmUntilTime && !isHurt;
		const bool mayScan = ((debugTick + clientNum) % retargetFrames) == 0;
		const bool mayRescan = ((debugTick + clientNum) % rescanFrames) == 0;
		const bool wantScan = bot.hurtScanPending || (!isPostKill && (chosen < 0 ? mayScan : mayRescan));
		if (wantScan)
		{
			const int numClients = *reinterpret_cast<int*>(svs_numClients);
			const float maxSq = (skill.distMax * distMulti) * (skill.distMax * distMulti);
			const float hearUnits = static_cast<float>(tuning.hear) * hearScaleBySkill[bot.skillIndex];
			const float hearDistSq = hearUnits * hearUnits;
			const float gunfireDistSq = hearDistSq * gunfireRangeMulti * gunfireRangeMulti;
			const bool ignoreHumans = tuning.ignoreHumans != 0;
			UpdateGunfire(now);

			const char* playerState = PlayerStateOf(clientNum);
			const float selfAds = *reinterpret_cast<const float*>(playerState + psAdsAmount);
			float noticeFov = skill.fov;
			if (selfAds > 0.0f)
			{
				noticeFov = std::cos(std::acos(skill.fov) * (1.0f - adsFovMulti * selfAds));
			}
			const float noticeYawHalfDeg = std::acos(noticeFov) * radToDeg * noticeWidthScale;
			const float noticePitchHalfDeg = noticeYawHalfDeg * noticeHeightShare;
			const int incumbent = chosen;
			const int forced = ForcedTargetFor(clientNum, self);
			bot.hurtScanPending = false;

			bool incumbentOnBearing = false;
			if (incumbent >= 0 && isHurt && bot.hasHurtBearing)
			{
				const float incumbentYaw = std::atan2(target.eye[1] - self.eye[1], target.eye[0] - self.eye[0]) * radToDeg;
				incumbentOnBearing = std::fabs(AngleDelta(incumbentYaw, bot.hurtBearingYaw)) < hurtBearingConeDeg;
			}
			const bool mayShooterSwitch = incumbent < 0
				|| (!incumbentOnBearing && now >= bot.shooterSwitchNotBeforeTime);

			float switchBar = 0.0f;
			if (incumbent >= 0)
			{
				const float idx = target.eye[0] - self.eye[0];
				const float idy = target.eye[1] - self.eye[1];
				const float idz = target.eye[2] - self.eye[2];
				const float incumbentDistSq = idx * idx + idy * idy + idz * idz;
				if (now < bot.switchNotBeforeTime)
				{
					switchBar = 0.0f;
				}
				else if (bot.noTraceTime > switchHiddenMs)
				{
					switchBar = 1.0e30f;
				}
				else
				{
					const float focusScale = static_cast<float>(150 - personality.focus) / 100.0f;
					switchBar = incumbentDistSq * switchRatio * focusScale;
				}
			}

			bot.scanEnemies = 0;
			bot.scanInCone = 0;
			bot.scanVisible = 0;

			struct ScanCandidate
			{
				int client;
				float effDistSq;
				float realDistSq;
				float bearingYaw;
				bool heard;
				bool onBearing;
				PlayerView view;
			};
			ScanCandidate candidates[maxClients];
			int candidateCount = 0;
			int heardUnseen = -1;
			float heardUnseenDistSq = 0.0f;
			PlayerView heardView = {};

			for (int i = 0; i < numClients && candidateCount < maxClients; ++i)
			{
				if (i == clientNum || i == incumbent)
				{
					continue;
				}

				const PlayerView other = ReadPlayerView(i);
				if (!IsEnemy(self, other) || (ignoreHumans && IsHuman(i)))
				{
					continue;
				}
				++bot.scanEnemies;

				const float dx = other.eye[0] - self.eye[0];
				const float dy = other.eye[1] - self.eye[1];
				const float dz = other.eye[2] - self.eye[2];
				const float realDistSq = dx * dx + dy * dy + dz * dz;

				const float effDistSq = realDistSq;
				const float bearingYaw = std::atan2(dy, dx) * radToDeg;
				const bool isForced = i == forced;
				const bool onBearing = isForced || (isHurt && bot.hasHurtBearing && mayShooterSwitch
					&& std::fabs(AngleDelta(bearingYaw, bot.hurtBearingYaw)) < hurtBearingConeDeg);
				if (forced >= 0 && !onBearing)
				{
					continue;
				}
				if (realDistSq > maxSq && !onBearing)
				{
					continue;
				}
				if (incumbent >= 0)
				{
					const bool beatsBar = effDistSq < switchBar
						&& (realDistSq < plainSwitchNearSq || switchBar >= 1.0e30f);
					if (!onBearing && !beatsBar)
					{
						continue;
					}
				}

				const float yawOffShare = AngleDelta(input.angles[1], bearingYaw) / noticeYawHalfDeg;
				const float pitchTo = -std::atan2(dz, std::sqrt(dx * dx + dy * dy)) * radToDeg;
				const float pitchOffShare = AngleDelta(input.angles[0], pitchTo) / noticePitchHalfDeg;
				const bool inCone = yawOffShare * yawOffShare + pitchOffShare * pitchOffShare <= 1.0f;
				bool heard = false;
				if (!inCone && !onBearing)
				{
					if (hearDistSq <= 0.0f)
					{
						continue;
					}

					float shotRangeSq = gunfireDistSq;
					if (firedSilenced[i])
					{
						shotRangeSq = gunfireDistSq * 0.25f;
					}
					float stepRangeSq = hearDistSq;
					if (IsWalledOff(clientNum, i))
					{
						shotRangeSq *= gunfireWallShare * gunfireWallShare;
						stepRangeSq *= footstepWallShare * footstepWallShare;
					}
					heard = now - firedTime[i] < gunfireMs && realDistSq < shotRangeSq;
					if (!heard && realDistSq <= stepRangeSq)
					{
						float velocity[3];
						ReadVelocity(i, velocity);
						heard = velocity[0] * velocity[0] + velocity[1] * velocity[1] > heardSpeedSq;
					}
					if (!heard)
					{
						continue;
					}

					if (now >= bot.listenNextTime)
					{
						bot.listenNextTime = now + listenCooldownMs;
						const float toX = other.eye[0] - self.eye[0];
						const float toY = other.eye[1] - self.eye[1];
						const float soundYaw = std::atan2(toY, toX) * radToDeg + Flrand(-listenFuzzDeg, listenFuzzDeg);
						float openRange = std::sqrt(toX * toX + toY * toY);
						if (openRange > listenOpenUnits)
						{
							openRange = listenOpenUnits;
						}
						float lookYaw = soundYaw;
						if (TryOpenYawToward(clientNum, self, soundYaw, openRange, &lookYaw))
						{
							float pitch = -std::atan2(other.eye[2] - self.eye[2], std::sqrt(toX * toX + toY * toY)) * radToDeg;
							if (pitch > listenPitchMaxDeg)
							{
								pitch = listenPitchMaxDeg;
							}
							if (pitch < -listenPitchMaxDeg)
							{
								pitch = -listenPitchMaxDeg;
							}
							bot.listenYaw = lookYaw;
							bot.listenPitch = pitch;
							bot.listenUntilTime = now + listenMs;
						}
					}
					if (heardUnseen < 0 || realDistSq < heardUnseenDistSq)
					{
						heardUnseen = i;
						heardUnseenDistSq = realDistSq;
						heardView = other;
					}
					continue;
				}
				++bot.scanInCone;

				int slot = candidateCount;
				while (slot > 0)
				{
					const ScanCandidate& before = candidates[slot - 1];
					const bool goesBefore = (onBearing && !before.onBearing)
						|| (onBearing == before.onBearing && effDistSq < before.effDistSq);
					if (!goesBefore)
					{
						break;
					}
					candidates[slot] = candidates[slot - 1];
					--slot;
				}
				candidates[slot] = { i, effDistSq, realDistSq, bearingYaw, heard, onBearing, other };
				++candidateCount;
			}

			int traces = 0;
			float chosenBearing = 0.0f;
			bool chosenOnBearing = false;
			++bot.scanRotation;
			for (int step = 0; step < candidateCount && traces < maxTracesPerScan; ++step)
			{
				int k = step;
				if (step > 0 && candidateCount > maxTracesPerScan)
				{
					k = 1 + (bot.scanRotation + step - 1) % (candidateCount - 1);
				}
				const ScanCandidate& candidate = candidates[k];
				++traces;
				if (SightOn(clientNum, self, candidate.view, useTraces) == 0)
				{
					if (candidate.heard && (heardUnseen < 0 || candidate.realDistSq < heardUnseenDistSq))
					{
						heardUnseen = candidate.client;
						heardUnseenDistSq = candidate.realDistSq;
						heardView = candidate.view;
					}
					continue;
				}
				++bot.scanVisible;

				chosen = candidate.client;
				target = candidate.view;
				chosenBearing = candidate.bearingYaw;
				chosenOnBearing = candidate.onBearing;
				break;
			}

			if (chosen >= 0 && chosen != bot.targetClient)
			{
				if (incumbent >= 0)
				{
					BotLog("switch client %d from client %d to client %d%s", clientNum, incumbent, chosen,
						chosenOnBearing ? " (shot)" : "");
					if (chosenOnBearing)
					{
						bot.shooterSwitchNotBeforeTime = now + shooterSwitchCooldownMs;
					}
				}
				else if (isHurt)
				{
					BotLog("hit client %d turns on client %d at bearing %.0f (shot from %.0f%s)", clientNum, chosen,
						chosenBearing, bot.hurtBearingYaw, chosenOnBearing ? ", on it" : "");
				}

				bot.traceTime = 0;
				bot.noTraceTime = 0;
				bot.isBurstFiring = false;
				bot.burstEndTime = 0;
				bot.switchNotBeforeTime = now + switchCooldownMs * personality.focus / 50;
				bot.stanceDecided = false;
				bot.hasOpenedFire = false;
				bot.wideGateUntilTime = 0;
				bot.trackSampleTime = 0;
				bot.aimBreakNextTime = 0;
				bot.aimBreakUntilTime = 0;
				bot.flickUntilTime = 0;
				bot.adsHoldUntilTime = 0;
				bot.panicRolled = false;
				bot.panicUntilTime = 0;
				bot.isWalkingFight = false;
				bot.isClosingIn = false;
				bot.isBackingOff = false;

				const float freshX = target.eye[0] - self.eye[0];
				const float freshY = target.eye[1] - self.eye[1];
				const float freshZ = target.eye[2] - self.eye[2];
				const float freshDistSq = freshX * freshX + freshY * freshY + freshZ * freshZ;
				const float errorScale = freshDistSq < surpriseRangeSq ? surpriseScale : 1.0f;
				for (int axis = 0; axis < 3; ++axis)
				{
					bot.aimOffsetBase[axis] = Flrand(-1.0f, 1.0f) * errorScale;
				}
				bot.reactionJitter = Flrand(reactionJitterMin, reactionJitterMax);

				if (isHurt)
				{
					bot.lastAttacker = chosen;
					bot.hurtLookUntilTime = 0;
					CallForHelp(clientNum, self, chosen, now);
				}
			}

			if (heardUnseen >= 0)
			{
				bot.heardClient = heardUnseen;
				bot.heardTime = now;
			}
			const bool onSpell = bot.task.kind == TaskHold || bot.task.kind == TaskFollow
				|| bot.task.kind == TaskCover || bot.task.kind == TaskCapture || bot.task.kind == TaskDefend;
			if (chosen < 0 && heardUnseen >= 0 && now >= bot.hearCooldownTime && bot.pursuitUntilTime <= now
				&& !onSpell && heardUnseenDistSq < hearPushMaxSq)
			{
				bot.hearCooldownTime = now + JitterMs(hearCooldownMs);
				if (RollPercent(personality.aggression))
				{
					float heardFeet[3];
					FeetOf(heardView, heardFeet);
					const float fuzzBearing = Flrand(0.0f, 6.2831853f);
					const float fuzz = Flrand(0.0f, hearFuzzUnits);
					heardFeet[0] += std::cos(fuzzBearing) * fuzz;
					heardFeet[1] += std::sin(fuzzBearing) * fuzz;
					if (RequestPush(clientNum, heardFeet, PriorityHear, "hear"))
					{
						BotLog("hear client %d walks toward client %d %.0f away", clientNum, heardUnseen,
							std::sqrt(heardUnseenDistSq));
					}
				}
			}
		}

		*outTarget = target;
		return chosen;
	}


	bool HurtLook(int clientNum, BotInput& input, int now)
	{
		BotState& bot = bots[clientNum];
		float lookYaw = bot.hurtBearingYaw;
		float lookPitch = 0.0f;
		if (!bot.hasHurtBearing || now >= bot.hurtLookUntilTime)
		{
			if (now >= bot.listenUntilTime)
			{
				return false;
			}
			lookYaw = bot.listenYaw;
			lookPitch = bot.listenPitch;
		}
		TurnView(clientNum, input, lookYaw, lookPitch, "hurt");
		return true;
	}
}
