#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/Control/Objectives.hpp"

namespace Components::BotAI
{
	static constexpr int lifeTimerSpreadMs = 4000;
	static constexpr int sprintSeedMaxMs = 1000;
	static constexpr int openingSprintPercent = 90;
	static constexpr int openingSprintMinMs = 3000;
	static constexpr int openingSprintMaxMs = 6000;

	static bool IsFrozen(int clientNum)
	{
		const char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
		const char* gclient = *reinterpret_cast<char* const*>(entity + gentityClient);
		if (!gclient)
		{
			return false;
		}
		return (*reinterpret_cast<const int*>(gclient + gclientFlags) & gclientFlagFrozen) != 0;
	}


	static void ResetLife(int clientNum)
	{
		BotState& bot = bots[clientNum];
		static_cast<BotLife&>(bot) = BotLife{};

		bot.input.serverTimeBias = defaultTimeBias;
		const float* viewAngles = reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psViewAngles);
		bot.input.angles[0] = viewAngles[0];
		bot.input.angles[1] = viewAngles[1];
		bot.input.angles[2] = viewAngles[2];

		if (SkillSetting() != bot.skillSettingSeen)
		{
			RollSkill(clientNum);
		}

		bot.fireHoldTime = ServerTimeMs() + IrandMs(300, 700);
		bot.yyNextTime = ServerTimeMs() + IrandMs(1500, 4000);

		const int now = ServerTimeMs();
		bot.pauseNextTime = now + IrandMs(0, lifeTimerSpreadMs);
		bot.glanceNextTime = now + IrandMs(0, lifeTimerSpreadMs);
		bot.walkPitchNextTime = now + IrandMs(0, lifeTimerSpreadMs);
		bot.walkScaleNextTime = now + IrandMs(0, lifeTimerSpreadMs);
		bot.sprintWindowEndTime = now + IrandMs(0, sprintSeedMaxMs);
		bot.input.isActive = true;
	}


	static void DriveBotInner(int clientNum, bool wantFrozen)
	{
		BotInput& input = bots[clientNum].input;
		bots[clientNum].turnCount = 0;
		bots[clientNum].aiMode = "idle";
		bots[clientNum].navMode = "-";

		const PlayerView self = ReadPlayerView(clientNum);
		if (!input.isActive && bots[clientNum].hasSpawnedOnce && !self.isPlaying)
		{
			bots[clientNum].aiMode = "dead";
			return;
		}

		if (!input.isActive)
		{
			ResetLife(clientNum);
		}

		{
			const float* viewAngles = reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psViewAngles);
			if (IsFiniteVec(viewAngles))
			{
				input.angles[0] = viewAngles[0];
				input.angles[1] = viewAngles[1];
			}
		}
		RemoveViewKick(clientNum, input);

		if (wantFrozen || IsFrozen(clientNum))
		{
			input.buttons = 0;
			input.forward = 0;
			input.right = 0;
			bots[clientNum].grenadeThrowFrames = 0;
			bots[clientNum].tacticalThrowFrames = 0;
			bots[clientNum].throwOverrunFrames = 0;
			bots[clientNum].wasFrozen = true;
			bots[clientNum].aiMode = "frozen";
			if (!wantFrozen && self.isPlaying && bots[clientNum].personality.holdsSecondary)
			{
				const unsigned short preferred = PreferredHeldWeapon(bots[clientNum].personality,
																	 PlayerStateOf(clientNum));
				if (preferred)
				{
					input.weapon = preferred;
				}
			}
			return;
		}

		if (bots[clientNum].wasFrozen)
		{
			bots[clientNum].wasFrozen = false;
			StartOpening(ServerTimeMs());
			bots[clientNum].wantSprint = RollPercent(openingSprintPercent);
			bots[clientNum].sprintWindowEndTime = ServerTimeMs() + IrandMs(openingSprintMinMs, openingSprintMaxMs);
			bots[clientNum].fireHoldTime = ServerTimeMs() + IrandMs(400, 900);
			bots[clientNum].yyNextTime = ServerTimeMs() + IrandMs(1500, 4000);
		}

		if (self.isPlaying)
		{
			TickTasks(clientNum, self);
		}

		if (RunKillstreakAi(clientNum, input))
		{
			bots[clientNum].aiMode = "killstreak";
			return;
		}

		RunCombatAi(clientNum, input, true);

		{
			BotState& bot = bots[clientNum];
			const int now = ServerTimeMs();
			const bool isJumping = (input.buttons & cmdButtonUp) != 0;
			if (bot.targetClient >= 0 && !isJumping && !self.isOnLadder)
			{
				if (bot.dropUntilTime > now)
				{
					input.buttons |= cmdButtonProne;
					input.buttons &= ~(cmdButtonCrouch | cmdButtonSprint);
				}
				else if (bot.crouchEndTime > now && (input.buttons & cmdButtonProne) == 0)
				{
					input.buttons |= cmdButtonCrouch;
					input.buttons &= ~cmdButtonSprint;
				}
			}
		}

		if (bots[clientNum].yyFrames > 0)
		{
			input.buttons &= ~cmdButtonAds;
		}

		{
			BotState& bot = bots[clientNum];
			const int throwBackOwner = *reinterpret_cast<const int*>(PlayerStateOf(clientNum) + psThrowBackOwner);
			if (throwBackOwner == entityNumNone)
			{
				bot.throwBackRollOwner = -1;
			}
			else
			{
				const int now = ServerTimeMs();
				if (bot.throwBackRollOwner != throwBackOwner)
				{
					bot.throwBackRollOwner = throwBackOwner;
					bot.isThrowingBack = RollPercent(throwBackPercent);
					bot.throwBackPressTime = now + IrandMs(throwBackDelayMinMs, throwBackDelayMaxMs);
				}
				if (bot.isThrowingBack && now >= bot.throwBackPressTime)
				{
					input.buttons &= ~(cmdButtonAttack | cmdButtonAds | cmdButtonSprint);
					input.buttons |= cmdButtonFrag;
					if (bot.lastThrowBackOwner != throwBackOwner)
					{
						bot.lastThrowBackOwner = throwBackOwner;
						BotLog("throwback client %d grenade %d", clientNum, throwBackOwner);
					}
				}
			}
		}
	}


	void DriveBot(int clientNum, bool wantFrozen)
	{
		DriveBotInner(clientNum, wantFrozen);
		ApplyViewKick(clientNum, bots[clientNum].input);

		if (!debugOn || ((debugTick + clientNum * 5) % 100) != 0)
		{
			return;
		}

		const BotState& bot = bots[clientNum];
		const PlayerView self = ReadPlayerView(clientNum);
		const int weaponIndex = *reinterpret_cast<const unsigned short*>(PlayerStateOf(clientNum) + psWeapon);
		const char* weaponName = weaponIndex ? WeaponNameOf(weaponIndex) : nullptr;
		BotLog("state client %d at %.0f %.0f %.0f %s/%s task %s node %d/%d goal %d team %d target %d%s sight %d scan %d/%d/%d skill %d/%d %s%s %s %s",
			clientNum, self.eye[0], self.eye[1], self.feetZ, bot.aiMode, bot.navMode, TaskLabel(clientNum),
			bot.pathIndex, bot.pathLength, bot.goalNode,
			self.team, bot.targetClient, bot.isRoamer ? " roamer" : "",
			(bot.targetClient >= 0 && bot.noTraceTime == 0) ? 1 : 0,
			bot.scanEnemies, bot.scanInCone, bot.scanVisible,
			bot.skillIndex + 1, bot.baseSkillIndex + 1, ArchetypeName(bot.personality.archetype),
			bot.personality.isQuickscoper ? "(qs)" : "",
			weaponName ? weaponName : "none", GametypeName());
	}
}
