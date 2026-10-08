#include "Components/Modules/BotAI/Control/Ai.hpp"
#include <cmath>
#include <cstring>

namespace Components::BotAI
{
	static constexpr float teammateLineDot      = 0.9994f;
	static constexpr int   meleeGapMinMs        = 900;
	static constexpr int   meleeGapMaxMs        = 1400;
	static constexpr int   meleeGapSkillMs      = 50;
	static constexpr int   breakOffPercent      = 20;
	static constexpr int   flinchCrouchPercent  = 8;
	static constexpr int   flinchCrouchMinMs    = 400;
	static constexpr int   flinchCrouchMaxMs    = 900;
	static constexpr int   flinchFlipPercent    = 50;
	static constexpr int   dropShotBeatMinMs    = 150;
	static constexpr int   dropShotBeatMaxMs    = 300;
	static constexpr int   jumpShotBeatMinMs    = 100;
	static constexpr int   jumpShotBeatMaxMs    = 200;
	static constexpr int   strafeFlipPercent    = 70;
	static constexpr float strafeAmountMinShare = 0.45f;
	static constexpr int   fightStepPercent     = 12;
	static constexpr float closeInOffShare      = 0.8f;
	static constexpr float backOffOffShare      = 1.4f;
	static constexpr int   fightStepMinFrames   = 4;
	static constexpr int   fightStepMaxFrames   = 10;
	static constexpr signed char fightStepAmount = 48;
	static constexpr float sightedGrenadeRangeSq = 800.0f * 800.0f;
	static constexpr int   sightedGrenadePerMille = 3;
	static constexpr float knifeLungeSq         = 110.0f * 110.0f;
	static constexpr float knifeChargeRangeSq   = 700.0f * 700.0f;
	static constexpr int   rushSprintAggressionMin = 45;
	static constexpr float knifeSprintRangeSq   = 250.0f * 250.0f;
	static constexpr int   chargeStallLimit     = 20;
	static constexpr int   chargeRouteMs        = 4000;
	static constexpr float tubeMinRangeSq       = 450.0f * 450.0f;
	static constexpr float tubeMaxRangeSq       = 1400.0f * 1400.0f;
	static constexpr int   tubeHoldMs           = 3500;
	static constexpr int   tubeGapMinMs         = 2500;
	static constexpr int   tubeGapMaxMs         = 6000;
	static constexpr int   tubeAfterShotMs      = 400;
	static constexpr int   omaIdle              = 0;
	static constexpr int   omaSwitching         = 1;
	static constexpr int   omaChanging          = 2;
	static constexpr int   omaAnswerMinMs       = 400;
	static constexpr int   omaAnswerMaxMs       = 900;
	static constexpr int   omaSwitchGiveUpMs    = 4000;
	static constexpr int   omaChangeMs          = 7000;
	static constexpr int   omaRetryMs           = 10000;
	static constexpr float lobGateDeg           = 2.5f;
	static constexpr float lobMaxPitchDeg       = 45.0f;
	static constexpr float lobFeetRise          = 8.0f;
	static constexpr float lobLeadShare         = 0.7f;
	static constexpr float lobMaxLeadSec        = 1.0f;
	static constexpr float adsOnDot             = 0.985f;
	static constexpr float adsOffDot            = 0.906f;
	static constexpr float closeHipRangeSq      = 350.0f * 350.0f;
	static constexpr float closeHipRangeRifleSq = 180.0f * 180.0f;
	static constexpr float aimErrorVerticalShare = 0.35f;
	static constexpr float proneEyeRise         = 12.0f;
	static constexpr int   lostHoldSpanMs       = 800;
	static constexpr float throwReleaseErrorDeg = 5.0f;
	static constexpr int   throwAimWaitMax      = 8;
	static constexpr float adsFireReadyMin      = 0.85f;
	static constexpr float prefireCloseRangeSq  = 400.0f * 400.0f;
	static constexpr int   fightStyleStrafe     = 0;
	static constexpr int   fightStylePlant      = 1;
	static constexpr int   fightStylePush       = 2;
	static constexpr int   fightStyleProne      = 3;
	static constexpr int   fightStyleHide       = 4;
	static constexpr int   fightStyleMinMs      = 2500;
	static constexpr int   fightStyleMaxMs      = 5000;
	static constexpr float fightProneMinRangeSq = 300.0f * 300.0f;
	static constexpr int   hideHealth           = 70;
	static constexpr int   seeThroughProbeMask  = 0x21FCF97B;
	static constexpr float bloomBodyMulti       = 2.0f;
	static constexpr float bloomLowSkillShare   = 0.25f;
	static constexpr int   panicPercent         = 35;
	static constexpr float panicRangeSq         = 500.0f * 500.0f;
	static constexpr int   panicMinMs           = 150;
	static constexpr int   panicMaxMs           = 350;

	static constexpr float proneYawLimitDeg = 45.0f;

	static constexpr float ledgeDropUnits      = 60.0f;
	static constexpr int   strafeRampFrames    = 3;

	static constexpr int   tacticalCooldownMs  = 10000;
	static constexpr int   tacticalChancePercent   = 8;
	static constexpr int   tacticalHoldFrames  = 6;
	static constexpr float tacticalMinRangeSq  = 300.0f * 300.0f;
	static constexpr float tacticalMaxRangeSq  = 900.0f * 900.0f;

	static constexpr int   yyPeriodFrameCount      = 6;
	static constexpr int   yyRepeatsMin        = 1;
	static constexpr int   yyRepeatsMax        = 2;
	static constexpr int   yyGapMinMs          = 35000;
	static constexpr int   yyGapMaxMs          = 65000;

	static constexpr int   yyAfterShotMinMs    = 1300;
	static constexpr int   yyAfterShotMaxMs    = 1900;

	static constexpr int   reloadSafeMs        = 1500;
	static constexpr int   reloadAfterHurtMs   = 1000;
	static constexpr int   reloadLostDryMs     = 500;
	static constexpr int   lowClipSniper       = 3;
	static constexpr int   lowClipPistol       = 5;
	static constexpr int   lowClipShotgun      = 3;

	static constexpr float maxLeadSec       = 0.08f;

	static constexpr int   lostLookMaxMs    = 1200;

	static constexpr float noticeShare      = 0.4f;

	static constexpr int   reloadGuardMs    = 2600;

	static constexpr float walkingFightOffShare = 0.7f;
	static constexpr int   skipCamStartMs   = 1200;
	static constexpr int   skipCamStartJitterMs = 1300;
	static constexpr int   skipCamPercent   = 70;
	static constexpr int   watchCamMs       = 7000;
	static constexpr int   kickStepMs       = 5;
	static constexpr float kickReturnShare  = 0.06f;
	static constexpr float kickClampDeg     = 10.0f;
	static constexpr float kickAdsThreshold = 0.5f;

	static constexpr int   burstOverRounds  = 1;
	static constexpr int   burstFingerMinMs = 80;
	static constexpr int   burstFingerMaxMs = 220;
	static constexpr int   swapEdgeMs       = 300;
	static constexpr int   swapSlopMs       = 500;
	static constexpr float swapReachMulti   = 2.0f;
	static constexpr int   bloomCrouchPercent   = 15;
	static constexpr float stoppingPowerMulti = 1.4f;
	static constexpr float playerHealth     = 100.0f;
	static constexpr int   yyPeriodMinFrames = 3;
	static constexpr int   yyPeriodMaxFrames = 8;
	static constexpr int   adsHoldSlopMs    = 500;

	static constexpr float lowClipShare     = 0.3f;
	static constexpr int   reloadMarginMs   = 300;
	static constexpr int   boltMarginMs     = 300;

	static constexpr float bloomBodyHalfWidth = 15.0f;
	static constexpr float bloomOverRest    = 1.5f;
	static constexpr float proneViewHeight  = 11.0f;
	static constexpr float duckedViewHeight = 40.0f;
	static constexpr float standViewHeight  = 60.0f;
	static constexpr int   lostSpotGraceMs    = 300;
	static constexpr float wallbangMaxRangeSq = 1600.0f * 1600.0f;
	static constexpr float wallbangMinShare = 0.3f;
	static constexpr float wallbangOpenShare = 0.99f;
	static constexpr int   sniperSettleMaxFrames = 3;
	static constexpr int   sightFlickerMs = 100;
	static constexpr int   lostAdsSettleMs = 300;
	static constexpr int   walkingFightNoSprintMs = 300;
	static constexpr float sniperStillSpeed = 20.0f;
	static constexpr int   wallbangMinMs = 500;
	static constexpr int   wallbangMaxMs = 1200;
	static constexpr int   wallbangPercentBySkill[7] = { 10, 20, 30, 40, 55, 70, 85 };
	static constexpr float throwHiddenUnits = 350.0f;
	static constexpr float throwPointRise = 20.0f;
	static constexpr int   closetSeenMax = 8;
	static constexpr int   closetThrowScale = 2;
	static constexpr int   heardWallbangFreshMs = 400;
	static constexpr int   heardWallbangCooldownMs = 4000;
	static constexpr float heardWallbangRangeSq = 1000.0f * 1000.0f;
	static constexpr float heardWallbangTableMin = 0.3f;
	static constexpr float heardWallbangFuzzUnits = 24.0f;

	static constexpr int   lostAdsMaxMs       = 1000;

	static constexpr int   strafeFramesMin = 14;

	static constexpr int   serverFrameMs    = 50;

	static constexpr float adsAimSpeedMulti = 0.5f;

	static constexpr float fireConeDotHip   = 0.95f;
	static constexpr float fireConeDotAds   = 0.995f;

	static constexpr float quickscopeScopeInDot  = 0.996f;
	static constexpr int   quickscopeReactMinMs  = 350;
	static constexpr int   quickscopeScopeMs     = 700;
	static constexpr int   quickscopeScopeMaxMs  = 900;
	static constexpr int   quickscopeBeatMinMs   = 600;
	static constexpr int   quickscopeBeatMaxMs   = 1200;
	static constexpr int   quickscopeBeatSkillMs = 50;
	static constexpr int   quickscopeMissBeatMs  = 400;
	static constexpr int   quickscopeMissMax     = 3;

	static constexpr int   quickscopeBeatFloorMs = 500;
	static constexpr float quickscopeDownAmount  = 0.1f;

	static constexpr int   quickscopeLostGraceMs = 400;

	static constexpr int   strafeStallLimit     = 6;

	static constexpr int   strafeFlipMinMs       = 800;
	static constexpr int   strafeRestFrames      = 30;

	static constexpr int   boltCycleMs           = 1400;

	static constexpr float quickscopeJumpRangeSq = 300.0f * 300.0f;

	static constexpr float chestDrop             = 24.0f;

	static constexpr int   scopedStrafeDivisor   = 3;
	static constexpr float quickscopeCloseRange  = 150.0f;
	static constexpr float quickscopeFarRange    = 1000.0f;

	static constexpr float sniperBodyHalfWidth   = 15.0f;

	static constexpr int   grenadeMinLostMs   = 400;
	static constexpr int   grenadeCooldownMs  = 6000;
	static constexpr int   grenadeChancePercent   = 5;
	static constexpr int   grenadeHoldFrames  = 14;

	static constexpr int   grenadeFollowFrames = 4;
	static constexpr float grenadeMinRangeSq  = 350.0f * 350.0f;
	static constexpr float grenadeMaxRangeSq  = 1000.0f * 1000.0f;
	static constexpr int   grenadeMaxLostMs   = 3000;
	static constexpr int   lostFootworkHoldMs = 500;
	static constexpr int   throwGapMinMs      = 4000;
	static constexpr int   throwGapMaxMs      = 8000;
	static constexpr float throwMateClearSq   = 350.0f * 350.0f;
	static constexpr float sightedGrenadeMaxSpeedSq = 60.0f * 60.0f;
	static constexpr float grenadeUpMin       = 12.0f;
	static constexpr float grenadeUpMax       = 45.0f;
	static constexpr float grenadeUpPerUnit   = 0.02f;
	static constexpr float grenadeUpPerHeight = 0.03f;

	static constexpr float aimDropNear        = 40.0f;
	static constexpr float aimDropFar         = 55.0f;
	static constexpr float aimDropFarSq       = 800.0f * 800.0f;
	static constexpr float aimDropLowSkill    = 10.0f;

	static constexpr float shellshockAimTime  = 1.0f;

	static constexpr int   reloadCancelPercent    = 20;
	static constexpr int   reloadCancelSkillMin = 3;


	static void AimAndFire(int clientNum, const PlayerView& self, const PlayerView& target,
						   bool hasSight, BotInput& input);
	static bool TryGrenade(int clientNum, const PlayerView& self, const char* playerState, BotInput& input,
						   bool allowSeen = false);
	static bool TryTactical(int clientNum, const PlayerView& self, const char* playerState, BotInput& input);
	static void TryStartHeardWallbang(int clientNum, const PlayerView& self, const Personality& personality, int now);

	static bool FinishThrow(int clientNum, const PlayerView& self, const char* playerState, BotInput& input)
	{
		const BotState& bot = bots[clientNum];
		if (bot.grenadeThrowFrames > 0 && TryGrenade(clientNum, self, playerState, input))
		{
			return true;
		}
		return bot.tacticalThrowFrames > 0 && TryTactical(clientNum, self, playerState, input);
	}


	static constexpr float shooterTimeScaleFloor = 0.3f;
	static constexpr int   reacquireBlindMs = 500;
	static constexpr int   throwOverrunMaxFrames = 20;
	static constexpr float trueGateBodyUnits = 60.0f;
	static constexpr float tapRangeSq = 1800.0f * 1800.0f;
	static constexpr int   tapDisciplineMin = 50;
	static constexpr int   tapRoundsMin = 1;
	static constexpr int   tapRoundsMax = 3;
	static constexpr int   reloadCancelDelayMinMs = 50;
	static constexpr int   reloadCancelDelayMaxMs = 300;
	static constexpr float fireGateBodyUnits = 15.0f;
	static constexpr float fireGateScaleBySkill[7] = { 3.0f, 2.8f, 2.6f, 2.4f, 2.2f, 2.1f, 2.0f };
	static constexpr float wobbleMovingSpeedSq = 100.0f * 100.0f;
	static constexpr float wobbleMovingScale = 1.5f;
	static constexpr float wobbleAdsScale = 0.6f;
	static constexpr float coverThreatUnits = 600.0f;
	static constexpr float reacquireConeDot = 0.5f;

	static float RangeTimeScale(const BotSkill& skill, float distanceSq, float distMulti)
	{
		const float startSq = (skill.distStart * distMulti) * (skill.distStart * distMulti);
		const float maxSq = (skill.distMax * distMulti) * (skill.distMax * distMulti);

		if (distanceSq > maxSq)
		{
			return 0.0f;
		}
		if (distanceSq <= startSq)
		{
			return 1.0f;
		}
		return 1.0f - ((distanceSq - startSq) / (maxSq - startSq));
	}


	static void ReadVelocity(int clientNum, float out[3]);

	unsigned short PreferredHeldWeapon(const Personality& personality, const char* playerState)
	{
		if (!personality.isMeleeOnly)
		{
			return FindShotgun(playerState);
		}
		const unsigned short sidearm = FindSidearm(playerState);
		if (sidearm)
		{
			return sidearm;
		}
		return FindSecondGun(playerState, FirstHeldRealWeapon(playerState));
	}

	static bool IsMovingFaster(int clientNum, float speed)
	{
		float velocity[3];
		ReadVelocity(clientNum, velocity);
		return velocity[0] * velocity[0] + velocity[1] * velocity[1] > speed * speed;
	}

	static void ReadVelocity(int clientNum, float out[3])
	{
		const char* ps = PlayerStateOf(clientNum);
		const float* v = reinterpret_cast<const float*>(ps + psVelocity);
		out[0] = v[0];
		out[1] = v[1];
		out[2] = v[2];
	}


	static float NoticeScale(const BotState& bot, const PlayerView& self, const PlayerView& target, const BotInput& input)
	{
		const int targetClient = bot.targetClient;
		const float edgeDeg = std::acos(bot.skillRow.fov) * radToDeg * noticeWidthScale;
		float coneDot = ConeDot(self.eye, target.eye, input.angles[1], input.angles[0]);
		if (coneDot > 1.0f)
		{
			coneDot = 1.0f;
		}
		if (coneDot < -1.0f)
		{
			coneDot = -1.0f;
		}
		const float offDeg = std::acos(coneDot) * radToDeg;
		float offShare = offDeg / edgeDeg;
		if (offShare > 1.0f)
		{
			offShare = 1.0f;
		}
		const float edgeScale = noticeEdgeScaleBySkill[bot.skillIndex];
		float scale = 1.0f - (1.0f - edgeScale) * offShare;

		float velocity[3];
		ReadVelocity(targetClient, velocity);
		if (velocity[0] * velocity[0] + velocity[1] * velocity[1] > noticeRunnerSpeedSq)
		{
			scale *= noticeRunnerScale;
		}

		const float viewHeight = target.eye[2] - target.feetZ;
		if (viewHeight < proneViewHeightMax)
		{
			scale *= noticeProneScale;
		}
		else if (viewHeight < crouchViewHeightMax)
		{
			scale *= noticeCrouchScale;
		}
		return scale;
	}


	static void DriveSwap(int clientNum, const char* playerState, BotInput& input, int now)
	{
		BotState& b = bots[clientNum];
		const unsigned short held = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		if (b.swapWeapon)
		{
			if (held == b.swapWeapon || now >= b.swapGiveUpTime)
			{
				if (held == b.swapWeapon && b.swapIsSidearm)
				{
					b.swapBackTime = now + swapBackMs;
				}
				b.swapWeapon = 0;
				input.weapon = 0;
				return;
			}
			input.weapon = b.swapWeapon;
			return;
		}

		input.weapon = 0;
		if (b.swapBackTime && now >= b.swapBackTime && b.targetClient < 0)
		{
			b.swapBackTime = 0;
			const unsigned short primary = FirstHeldRealWeapon(playerState);
			if (primary && primary != held)
			{
				b.swapWeapon = primary;
				b.swapIsSidearm = false;
				b.swapGiveUpTime = now + swapGiveUpMs;
				input.weapon = primary;
				BotLog("swap client %d back to its primary", clientNum);
			}
		}
	}


	static bool IsLobGunName(const char* name)
	{
		return name && (std::strncmp(name, "gl_", 3) == 0 || std::strncmp(name, "m79", 3) == 0);
	}


	static void DriveTube(int clientNum, const char* playerState, BotInput& input, int now)
	{
		BotState& bot = bots[clientNum];
		const unsigned short held = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		const int bagIndex = FindHeldWeapon(playerState, [](int index)
		{
			const char* name = WeaponNameOf(index);
			return name && std::strncmp(name, "onemanarmy", 10) == 0;
		});

		if (bot.omaStage == omaChanging)
		{
			if (now >= bot.omaDoneTime)
			{
				bot.omaStage = omaIdle;
				bot.omaNextTime = now + omaRetryMs;
			}
			return;
		}

		if (bagIndex && held == bagIndex)
		{
			if (bot.omaStage == omaIdle)
			{
				bot.omaStage = omaSwitching;
				bot.omaGiveUpTime = now + omaSwitchGiveUpMs;
			}
			if (bot.omaAnswerTime == 0)
			{
				bot.omaAnswerTime = now + IrandMs(omaAnswerMinMs, omaAnswerMaxMs);
			}
			if (now >= bot.omaAnswerTime)
			{
				char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
				SendMenuResponse(entity, "custom1", "onemanarmy");
				bot.omaStage = omaChanging;
				bot.omaAnswerTime = 0;
				bot.omaDoneTime = now + omaChangeMs;
				BotLog("oma client %d picks its class again", clientNum);
			}
			return;
		}

		if (bot.omaStage == omaSwitching)
		{
			if (now >= bot.omaGiveUpTime)
			{
				bot.omaStage = omaIdle;
				bot.omaAnswerTime = 0;
				bot.omaNextTime = now + omaRetryMs;
				BotLog("oma client %d never got the bag out", clientNum);
			}
			return;
		}

		if (bot.swapWeapon || bot.yyFrames != 0 || bot.grenadeThrowFrames != 0 || bot.targetClient >= 0)
		{
			return;
		}

		const int tubeIndex = FindHeldWeapon(playerState, [](int index)
		{
			const char* name = WeaponNameOf(index);
			return name && std::strncmp(name, "gl_", 3) == 0;
		});
		const bool areTubesDry = tubeIndex != 0
			&& ClipRoundsOf(playerState, tubeIndex) == 0 && StockRoundsOf(playerState, tubeIndex) == 0;
		if (bagIndex && areTubesDry && now >= bot.omaNextTime)
		{
			bot.swapWeapon = static_cast<unsigned short>(bagIndex);
			bot.swapIsSidearm = false;
			bot.swapGiveUpTime = now + omaSwitchGiveUpMs;
			input.weapon = bot.swapWeapon;
			bot.omaStage = omaSwitching;
			bot.omaGiveUpTime = now + omaSwitchGiveUpMs;
			BotLog("oma client %d tubes dry, takes the bag", clientNum);
			return;
		}

		const char* heldName = nullptr;
		if (held)
		{
			heldName = WeaponNameOf(held);
		}
		if (!IsLobGunName(heldName))
		{
			return;
		}

		unsigned short backTo = 0;
		const char* completeDef = BG_GetWeaponCompleteDef(held);
		if (std::strncmp(heldName, "gl_", 3) == 0 && completeDef)
		{
			backTo = static_cast<unsigned short>(*reinterpret_cast<const int*>(completeDef + weaponCompleteAltWeaponIndex));
		}
		if (!backTo)
		{
			backTo = FirstHeldRealWeapon(playerState);
		}
		if (!backTo || backTo == held)
		{
			return;
		}
		bot.swapWeapon = backTo;
		bot.swapIsSidearm = false;
		bot.swapGiveUpTime = now + swapGiveUpMs;
		input.weapon = backTo;
		bot.tubeUntilTime = 0;
		bot.tubeStartTime = 0;
		BotLog("tube client %d fight over, back to %s", clientNum, WeaponNameOf(backTo));
	}


	static void KickView(int clientNum, const char* playerState, int weaponIndex)
	{
		BotState& bot = bots[clientNum];
		const char* weaponDef = WeaponDefOf(weaponIndex);
		const char* completeDef = BG_GetWeaponCompleteDef(weaponIndex);
		if (!weaponDef || !completeDef)
		{
			return;
		}
		const float adsAmount = *reinterpret_cast<const float*>(playerState + psAdsAmount);
		const bool isAdsKick = adsAmount >= 1.0f;
		const float* pitchRange = reinterpret_cast<const float*>(
			weaponDef + (isAdsKick ? weaponDefAdsViewKickPitchMin : weaponDefHipViewKickPitchMin));
		const float* yawRange = reinterpret_cast<const float*>(
			weaponDef + (isAdsKick ? weaponDefAdsViewKickYawMin : weaponDefHipViewKickYawMin));

		float scale = static_cast<float>(tuning.kick) / 100.0f;
		const int restrictMs = *reinterpret_cast<const int*>(playerState + psWeaponRestrictKickTime);
		float reducedPercent = 100.0f;
		if (restrictMs > 0)
		{
			reducedPercent = *reinterpret_cast<const float*>(
				weaponDef + (isAdsKick ? weaponDefAdsGunKickReducedKickPercent : weaponDefHipGunKickReducedKickPercent));
			scale *= reducedPercent / 100.0f;
		}
		bot.kickVel[0] = -Flrand(pitchRange[0], pitchRange[1]) * scale;
		bot.kickVel[1] = Flrand(yawRange[0], yawRange[1]) * scale;
		bot.kickAdsCenter = *reinterpret_cast<const float*>(completeDef + weaponCompleteAdsViewKickCenterSpeed);
		bot.kickHipCenter = *reinterpret_cast<const float*>(completeDef + weaponCompleteHipViewKickCenterSpeed);

		if (bot.kickLoggedWeapon != weaponIndex)
		{
			bot.kickLoggedWeapon = weaponIndex;
			BotLog("kick client %d %s ads %.2f: pitch %.0f..%.0f yaw %.0f..%.0f a second, centre %.0f/%.0f, reduced to %.0f%% for %d ms",
				clientNum, WeaponNameOf(weaponIndex), adsAmount, pitchRange[0], pitchRange[1], yawRange[0], yawRange[1],
				bot.kickAdsCenter, bot.kickHipCenter, reducedPercent, restrictMs);
		}
	}


	void RemoveViewKick(int clientNum, BotInput& input)
	{
		BotState& bot = bots[clientNum];
		input.angles[0] -= bot.kickAngle[0];
		input.angles[1] -= bot.kickAngle[1];
		bot.isKickLifted = true;
	}


	void ApplyViewKick(int clientNum, BotInput& input)
	{
		BotState& bot = bots[clientNum];

		if (!bot.isKickLifted)
		{
			bot.kickAngle[0] = 0.0f;
			bot.kickAngle[1] = 0.0f;
			bot.kickVel[0] = 0.0f;
			bot.kickVel[1] = 0.0f;
			return;
		}
		bot.isKickLifted = false;

		const bool isStill = bot.kickAngle[0] == 0.0f && bot.kickAngle[1] == 0.0f
			&& bot.kickVel[0] == 0.0f && bot.kickVel[1] == 0.0f;
		if (isStill)
		{
			return;
		}

		const float adsAmount = *reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psAdsAmount);
		float center = bot.kickHipCenter;
		if (adsAmount > kickAdsThreshold)
		{
			center = bot.kickAdsCenter;
		}

		int remainingMs = serverFrameMs;
		while (remainingMs > 0)
		{
			int stepMs = remainingMs;
			if (stepMs > kickStepMs)
			{
				stepMs = kickStepMs;
			}
			remainingMs -= stepMs;
			const float dt = static_cast<float>(stepMs) * 0.001f;

			for (int axis = 0; axis < 2; ++axis)
			{
				float angle = bot.kickAngle[axis];
				float vel = bot.kickVel[axis];
				if (angle == 0.0f && vel == 0.0f)
				{
					continue;
				}
				if (angle > 0.0f)
				{
					vel -= center * dt;
				}
				else if (angle < 0.0f)
				{
					vel += center * dt;
				}

				float step = vel * dt;
				if (angle * step < 0.0f)
				{
					step *= kickReturnShare;
				}
				const float next = angle + step;
				if (angle * next < 0.0f)
				{
					angle = 0.0f;
					vel = 0.0f;
				}
				else
				{
					angle = next;
					if (angle > kickClampDeg)
					{
						angle = kickClampDeg;
						vel = 0.0f;
					}
					if (angle < -kickClampDeg)
					{
						angle = -kickClampDeg;
						vel = 0.0f;
					}
				}
				bot.kickAngle[axis] = angle;
				bot.kickVel[axis] = vel;
			}
		}

		input.angles[0] += bot.kickAngle[0];
		input.angles[1] += bot.kickAngle[1];
		KeepMoveHeading(input, bot.kickAngle[1]);
	}


	static float DamageAt(const GunStats& stats, float distance, const Loadout& loadout)
	{
		float damage = static_cast<float>(stats.damage);
		if (distance > stats.minDamageRange)
		{
			damage = static_cast<float>(stats.minDamage);
		}
		else if (distance > stats.maxDamageRange && stats.minDamageRange > stats.maxDamageRange)
		{
			const float share = (distance - stats.maxDamageRange) / (stats.minDamageRange - stats.maxDamageRange);
			damage += (static_cast<float>(stats.minDamage) - damage) * share;
		}
		if (loadout.perk2 && HasSubstring(loadout.perk2, "bulletdamage"))
		{
			damage *= stoppingPowerMulti;
		}
		return damage;
	}


	static int ShotsToKill(const GunStats& stats, float distance, const Loadout& loadout)
	{
		const float damage = DamageAt(stats, distance, loadout);
		if (damage <= 0.0f)
		{
			return 1;
		}
		return static_cast<int>(std::ceil(playerHealth / damage));
	}


	static int SwapMs(const WeaponInfo& weapon, int toIndex)
	{
		int raiseMs = weapon.stats.raiseTimeMs;
		GunStats to = {};
		if (TryGetGunStats(WeaponNameOf(toIndex), &to))
		{
			raiseMs = to.raiseTimeMs;
		}
		return weapon.stats.dropTimeMs + raiseMs;
	}


	static int SwapGiveUpMs(const WeaponInfo& weapon, int toIndex)
	{
		if (!weapon.hasStats)
		{
			return swapGiveUpMs;
		}
		return SwapMs(weapon, toIndex) + swapSlopMs;
	}


	static int YyPeriodFrames(const WeaponInfo& weapon)
	{
		if (!weapon.hasStats)
		{
			return yyPeriodFrameCount;
		}
		int frames = weapon.stats.quickDropTimeMs / serverFrameMs + 1 + IrandMs(0, 1);
		if (frames < yyPeriodMinFrames)
		{
			frames = yyPeriodMinFrames;
		}
		if (frames > yyPeriodMaxFrames)
		{
			frames = yyPeriodMaxFrames;
		}
		return frames;
	}


	static int ReloadGuardMs(const Loadout& loadout, const WeaponInfo& weapon)
	{
		if (!weapon.hasStats)
		{
			return reloadGuardMs;
		}
		int reloadMs = weapon.stats.reloadTimeMs;
		if (weapon.clipRounds == 0)
		{
			reloadMs = weapon.stats.reloadEmptyTimeMs;
		}
		if (loadout.perk1 && HasSubstring(loadout.perk1, "fastreload"))
		{
			reloadMs /= 2;
		}
		return reloadMs + reloadMarginMs;
	}


	static int ShotCycleMs(const WeaponInfo& weapon)
	{
		int cycleMs = weapon.stats.fireTimeMs;
		if (weapon.isBurst)
		{
			cycleMs *= 3;
		}
		const char* weaponDef = WeaponDefOf(weapon.index);
		if (weaponDef && *reinterpret_cast<const bool*>(weaponDef + weaponDefNeedsRechamber))
		{
			cycleMs += weapon.stats.rechamberTimeMs;
		}
		return cycleMs;
	}


	static int SemiGapMs(const BotSkill& skill, int skillIndex, const WeaponInfo& weapon)
	{
		if (!weapon.hasStats)
		{
			return static_cast<int>(skill.semiTime * 1000.0f);
		}
		const int cycleMs = ShotCycleMs(weapon);
		int gapMs = static_cast<int>(static_cast<float>(cycleMs) * semiFingerBySkill[skillIndex] * Flrand(0.85f, 1.25f));
		if (gapMs < cycleMs)
		{
			gapMs = cycleMs;
		}
		return gapMs;
	}


	static int BoltMs(const WeaponInfo& weapon)
	{
		if (!weapon.hasStats)
		{
			return boltCycleMs;
		}
		return ShotCycleMs(weapon) + boltMarginMs;
	}


	static void SpreadRangeDeg(const WeaponInfo& weapon, const char* playerState, float* minDeg, float* maxDeg)
	{
		const HipSpread& hip = weapon.stats.hip;
		const float viewHeight = *reinterpret_cast<const float*>(playerState + psViewHeight);
		if (viewHeight <= duckedViewHeight)
		{
			const float share = (viewHeight - proneViewHeight) / (duckedViewHeight - proneViewHeight);
			*minDeg = hip.proneMin + (hip.duckedMin - hip.proneMin) * share;
			*maxDeg = hip.proneMax + (hip.duckedMax - hip.proneMax) * share;
		}
		else
		{
			const float share = (viewHeight - duckedViewHeight) / (standViewHeight - duckedViewHeight);
			*minDeg = hip.duckedMin + (hip.standMin - hip.duckedMin) * share;
			*maxDeg = hip.duckedMax + (hip.standMax - hip.duckedMax) * share;
		}

		const float adsAmount = *reinterpret_cast<const float*>(playerState + psAdsAmount);
		if (adsAmount >= 1.0f)
		{
			*minDeg = weapon.stats.adsSpread;
		}
	}


	static int LowClipRounds(const WeaponInfo& weapon)
	{
		if (weapon.hasStats)
		{
			const int low = static_cast<int>(std::ceil(static_cast<float>(weapon.stats.clipSize) * lowClipShare));
			if (low < 1)
			{
				return 1;
			}
			return low;
		}
		switch (weapon.weapClass)
		{
		case weapClassSniper:
			return lowClipSniper;
		case weapClassPistol:
			return lowClipPistol;
		case weapClassSpread:
			return lowClipShotgun;
		default:
			return topUpRounds;
		}
	}


	static bool IsQuiet(const BotState& bot, int now)
	{
		return bot.scanInCone == 0 && now >= bot.hurtUntilTime + reloadAfterHurtMs;
	}


	static bool IsSafeToReload(const BotState& bot, int now)
	{
		return IsQuiet(bot, now) && now - bot.lastFireTime >= reloadSafeMs;
	}


	static unsigned short FlickPartner(const char* playerState, int current)
	{
		const unsigned short sidearm = FindSidearm(playerState);
		if (sidearm && sidearm != current)
		{
			return sidearm;
		}
		return FindSecondGun(playerState, static_cast<unsigned short>(current));
	}


	static void FlipStrafe(BotState& bot, int now)
	{
		if (now - bot.strafeFlipTime < strafeFlipMinMs)
		{
			bot.strafeDirection = 0;
			bot.strafeCountdown = strafeRestFrames;
		}
		else
		{
			bot.strafeDirection = -bot.strafeDirection;
		}
		bot.strafeFlipTime = now;
	}


	static void IdleHoldingProne(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn, int now)
	{
		const bool holdsProne = bots[clientNum].dropUntilTime > now;
		if (holdsProne)
		{
			input.buttons |= cmdButtonProne;
		}
		Idle(clientNum, self, input, allowTurn);
		if (holdsProne)
		{
			input.buttons &= ~(cmdButtonCrouch | cmdButtonSprint | cmdButtonUp);
		}
	}


	static bool IsTeammateInLine(int clientNum, const PlayerView& self, float distance, const BotInput& input)
	{
		if (self.team == 0)
		{
			return false;
		}
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		const float distanceSq = distance * distance;
		for (int k = 0; k < numClients; ++k)
		{
			if (k == clientNum)
			{
				continue;
			}
			const PlayerView mate = ReadPlayerView(k);
			if (!mate.isPlaying || mate.team != self.team)
			{
				continue;
			}
			const float dx = mate.eye[0] - self.eye[0];
			const float dy = mate.eye[1] - self.eye[1];
			const float dz = mate.eye[2] - self.eye[2];
			if (dx * dx + dy * dy + dz * dz > distanceSq)
			{
				continue;
			}
			if (ConeDot(self.eye, mate.eye, input.angles[1], input.angles[0]) > teammateLineDot)
			{
				return true;
			}
		}
		return false;
	}


	void RunCombatAi(int clientNum, BotInput& input, bool useTraces)
	{
		BotState& bot = bots[clientNum];
		const BotSkill& skill = bot.skillRow;
		const PlayerView self = ReadPlayerView(clientNum);
		const char* playerState = PlayerStateOf(clientNum);
		const WeaponInfo selfWeapon = ReadWeapon(playerState);
		const bool knifeForced = selfWeapon.index != 0 && IsDryGun(playerState, selfWeapon.index)
			&& FindSecondGun(playerState, selfWeapon.index) == 0;
		if (knifeForced != bot.knifeForced)
		{
			bot.knifeForced = knifeForced;
			if (knifeForced)
			{
				BotLog("knife client %d goes knife only: %s has no rounds left", clientNum, BG_GetWeaponName(selfWeapon.index));
			}
			else
			{
				BotLog("knife client %d has rounds again", clientNum);
			}
		}
		Personality personality = bot.personality;
		if (knifeForced)
		{
			personality.isMeleeOnly = true;
		}
		const float distMulti = selfWeapon.distMulti;

		input.buttons = 0;
		input.forward = 0;
		input.right = 0;
		input.meleeYaw = 0.0f;
		input.meleeDist = 0;

		if (bot.semiCooldown > 0)
		{
			bot.semiCooldown -= serverFrameMs;
		}

		if (!self.isPlaying)
		{
			bot.aiMode = bot.hasSpawnedOnce ? "dead" : "menu";
			bot.targetClient = -1;
			bot.hasDamageBaseline = false;
			if (bot.hasSpawnedOnce)
			{
				if (!bot.deathRecorded)
				{
					bot.deathRecorded = true;
					bot.hasDeathPos = true;
					FeetOf(self, bot.deathPos);
				}
				input.isActive = false;

				if (bot.deadSince == 0)
				{
					bot.deadSince = ServerTimeMs();
					if (RollPercent(skipCamPercent))
					{
						bot.skipCamNextTime = bot.deadSince + skipCamStartMs + IrandMs(0, skipCamStartJitterMs);
					}
					else
					{
						bot.skipCamNextTime = bot.deadSince + watchCamMs;
						BotLog("killcam client %d watches it", clientNum);
					}
				}
			}
			return;
		}

		const int now = ServerTimeMs();
		DriveSwap(clientNum, playerState, input, now);
		if (personality.isTuber)
		{
			DriveTube(clientNum, playerState, input, now);
		}
		if (personality.holdsSecondary && !bot.swapWeapon && bot.yyFrames == 0)
		{
			const unsigned short preferred = PreferredHeldWeapon(personality, playerState);
			if (preferred && selfWeapon.index != preferred)
			{
				bot.swapWeapon = preferred;
				bot.swapIsSidearm = false;
				bot.swapGiveUpTime = now + swapGiveUpMs;
				input.weapon = preferred;
				BotLog("hand client %d takes out %s", clientNum, WeaponNameOf(preferred));
			}
		}

		{
			const bool roundsLanded = selfWeapon.index && selfWeapon.index == bot.prevClipWeapon
				&& selfWeapon.clipRounds > bot.prevClipRounds;
			const bool roundFired = selfWeapon.index && selfWeapon.index == bot.prevClipWeapon
				&& selfWeapon.clipRounds < bot.prevClipRounds;
			bot.prevClipRounds = selfWeapon.clipRounds;
			bot.prevClipWeapon = selfWeapon.index;
			if (roundsLanded)
			{
				bot.reloadGuardUntilTime = 0;
			}
			if (roundFired)
			{
				KickView(clientNum, playerState, selfWeapon.index);
			}
			if (roundsLanded && bot.yyFrames == 0 && !bot.swapWeapon && !bot.swapBackTime
				&& bot.skillIndex >= reloadCancelSkillMin
				&& RollPercent(ScaleByTrait(reloadCancelPercent, personality.tricks)))
			{
				bot.reloadCancelAtTime = now + IrandMs(reloadCancelDelayMinMs, reloadCancelDelayMaxMs);
			}
			if (bot.reloadCancelAtTime != 0 && now >= bot.reloadCancelAtTime)
			{
				bot.reloadCancelAtTime = 0;
				const unsigned short partner = FlickPartner(playerState, selfWeapon.index);
				if (partner && bot.yyFrames == 0 && !bot.swapWeapon && !bot.swapBackTime)
				{
					bot.yyWeapon = static_cast<unsigned short>(selfWeapon.index);
					bot.yySidearm = partner;
					bot.yyFrames = YyPeriodFrames(selfWeapon);
					bot.yyPeriodFrames = bot.yyFrames;
				}
			}
			if (bot.yyFrames > 0 && !bot.swapWeapon)
			{
				--bot.yyFrames;
				if (bot.yyPeriodFrames <= 0)
				{
					bot.yyPeriodFrames = yyPeriodFrameCount;
				}
				const int phase = bot.yyFrames % bot.yyPeriodFrames;
				if (phase == bot.yyPeriodFrames - 1)
				{
					input.weapon = bot.yySidearm;
				}
				else if (phase == bot.yyPeriodFrames - 2)
				{
					input.weapon = bot.yyWeapon;
				}
			}
		}

		ReadHurt(clientNum, self, now);

		PlayerView target = {};
		const int chosen = SelectTarget(clientNum, self, selfWeapon, input, useTraces, now, &target);
		bot.targetClient = chosen;
		ApplyFightSkill(clientNum, chosen);
		if (chosen >= 0 && now < bot.reloadGuardUntilTime && selfWeapon.clipRounds > 0 && bot.yyFrames == 0
			&& !bot.swapWeapon && !bot.swapBackTime && bot.skillIndex >= reloadCancelSkillMin
			&& DistanceSq2D(self.eye, target.eye) < swapRangeSq * swapReachMulti * swapReachMulti
			&& RollPercent(ScaleByTrait(reloadCancelPercent, personality.tricks)))
		{
			const unsigned short partner = FlickPartner(playerState, selfWeapon.index);
			if (partner)
			{
				bot.yyWeapon = static_cast<unsigned short>(selfWeapon.index);
				bot.yySidearm = partner;
				bot.yyFrames = YyPeriodFrames(selfWeapon);
				bot.yyPeriodFrames = bot.yyFrames;
				bot.reloadGuardUntilTime = 0;
				BotLog("reload client %d cancels for client %d with %d rounds", clientNum, chosen, selfWeapon.clipRounds);
			}
		}
		if (chosen < 0)
		{
			bot.aiMode = "idle";
			bot.flinchPending = false;
			if (FinishThrow(clientNum, self, playerState, input))
			{
				return;
			}

			const bool isConfirming = now < bot.confirmUntilTime;
			const bool isPostKill = now < bot.postKillUntilTime;
			if (!isConfirming && !isPostKill)
			{
				TryStartHeardWallbang(clientNum, self, personality, now);
			}
			const bool isHeardBanging = !isConfirming && !isPostKill && now < bot.heardBangUntilTime;
			const bool looksAtShot = !isPostKill && !isHeardBanging
				&& ((bot.hasHurtBearing && now < bot.hurtLookUntilTime) || now < bot.listenUntilTime);
			IdleHoldingProne(clientNum, self, input, !isConfirming && !isPostKill && !looksAtShot && !isHeardBanging, now);
			const bool hasTurned = bot.turnCount > 0;
			if (isConfirming)
			{
				const float toX = bot.lastSeenPos[0] - self.eye[0];
				const float toY = bot.lastSeenPos[1] - self.eye[1];
				const float toZ = bot.lastSeenPos[2] - chestDrop - self.eye[2];
				const float bodyYaw = std::atan2(toY, toX) * radToDeg;
				const float bodyPitch = -std::atan2(toZ, std::sqrt(toX * toX + toY * toY)) * radToDeg;
				if (!hasTurned)
				{
					TurnView(clientNum, input, bodyYaw, bodyPitch, "confirm");
				}
				input.buttons |= bot.sentButtons & cmdButtonAds;
			}
			else if (isPostKill)
			{
				if (!hasTurned)
				{
					TurnView(clientNum, input, input.angles[1] + bot.sweepSign * postKillSweepDeg, 0.0f, "postkill");
				}
			}
			else if (isHeardBanging)
			{
				const float toX = bot.heardBangPoint[0] - self.eye[0];
				const float toY = bot.heardBangPoint[1] - self.eye[1];
				const float toZ = bot.heardBangPoint[2] - self.eye[2];
				const float flatUnits = std::sqrt(toX * toX + toY * toY);
				if (!hasTurned)
				{
					TurnView(clientNum, input, std::atan2(toY, toX) * radToDeg, -std::atan2(toZ, flatUnits) * radToDeg,
						"wallbang");
				}
				const float bangDistSq = toX * toX + toY * toY + toZ * toZ;
				const bool isOnPoint = ConeDot(self.eye, bot.heardBangPoint, input.angles[1], input.angles[0]) > fireConeDotHip;
				if (selfWeapon.index && selfWeapon.hasAmmo && selfWeapon.clipRounds > 0 && isOnPoint
					&& !IsTeammateInLine(clientNum, self, std::sqrt(bangDistSq), input))
				{
					if (selfWeapon.isFullAuto)
					{
						input.buttons |= cmdButtonAttack;
						bot.lastFireTime = now;
					}
					else if (bot.semiCooldown <= 0)
					{
						input.buttons |= cmdButtonAttack;
						bot.semiCooldown = SemiGapMs(skill, bot.skillIndex, selfWeapon);
						bot.lastFireTime = now;
					}
				}
				if (CanAds(selfWeapon, bangDistSq, personality.hipScale))
				{
					input.buttons |= cmdButtonAds;
				}
			}
			else if (looksAtShot)
			{
				if (!hasTurned)
				{
					HurtLook(clientNum, input, now);
				}
				input.buttons &= ~cmdButtonAds;
			}

			if (now < bot.lingerUntilTime)
			{
				input.buttons |= cmdButtonAttack;
				input.buttons |= bot.sentButtons & cmdButtonAds;
			}

			bot.scopeMisses = 0;
			const float idleAds = *reinterpret_cast<const float*>(playerState + psAdsAmount);
			const bool isBolting = now - bot.lastFireTime < yyAfterShotMinMs;
			const bool isReloading = now < bot.reloadGuardUntilTime;
			if (personality.isQuickscoper && !bot.swapWeapon && !bot.swapBackTime && idleAds <= 0.0f && !isBolting && !isReloading)
			{
				if (bot.yyFrames == 0 && now >= bot.yyNextTime)
				{
					bot.yyNextTime = now + IrandMs(yyGapMinMs, yyGapMaxMs);
					const unsigned short partner = FlickPartner(playerState, selfWeapon.index);
					if (selfWeapon.weapClass == weapClassSniper && partner
						&& RollPercent(ScaleByTrait(tuning.yy, personality.tricks)))
					{
						bot.yyWeapon = static_cast<unsigned short>(selfWeapon.index);
						bot.yySidearm = partner;
						bot.yyPeriodFrames = YyPeriodFrames(selfWeapon);
						bot.yyFrames = IrandMs(yyRepeatsMin, yyRepeatsMax) * bot.yyPeriodFrames;
						input.weapon = partner;
						BotLog("yy client %d", clientNum);
					}
				}
			}

			const bool isHolding = IsHolding(clientNum);
			const bool isDry = selfWeapon.clipRounds == 0;
			const bool isLow = selfWeapon.clipRounds < LowClipRounds(selfWeapon);
			if (selfWeapon.index && !bot.swapWeapon
				&& (isHolding ? (isDry || isLow)
							  : ((isDry && IsQuiet(bot, now)) || (isLow && IsSafeToReload(bot, now)))))
			{
				input.buttons |= cmdButtonReload;
				if (now >= bot.reloadGuardUntilTime)
				{
					bot.reloadGuardUntilTime = now + ReloadGuardMs(bot.loadout, selfWeapon);
				}
			}

			const char* selfEntity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
			const int health = *reinterpret_cast<const int*>(selfEntity + gentityHealth);
			if (tuning.cover && health > 0 && health < lowHealth && now < bot.hurtUntilTime + 3000
				&& now >= bot.coverCooldownTime && bot.task.kind != TaskCover && !isHolding
				&& bot.lastAttacker >= 0 && RollPercent(50 + personality.caution / 2))
			{
				bot.coverCooldownTime = now + coverCooldownMs;
				float threat[3] = { bot.lastSeenPos[0], bot.lastSeenPos[1], bot.lastSeenPos[2] };
				if (bot.hasHurtBearing)
				{
					const float bearingRad = bot.hurtBearingYaw / radToDeg;
					threat[0] = self.eye[0] + std::cos(bearingRad) * coverThreatUnits;
					threat[1] = self.eye[1] + std::sin(bearingRad) * coverThreatUnits;
					threat[2] = self.eye[2];
				}
				RequestCover(clientNum, self, threat);
			}
			return;
		}

		const float dx = target.eye[0] - self.eye[0];
		const float dy = target.eye[1] - self.eye[1];
		const float dz = target.eye[2] - self.eye[2];
		const float distanceSq = dx * dx + dy * dy + dz * dz;

		int sight = SightOn(clientNum, self, target, useTraces);
		if (sight != 0 && bot.noTraceTime > reacquireBlindMs && now >= bot.hurtUntilTime
			&& ConeDot(self.eye, target.eye, input.angles[1], input.angles[0]) < reacquireConeDot)
		{
			sight = 0;
		}
		const bool hasSight = sight != 0;
		bot.sightAtFeet = sight == 2;

		if (hasSight)
		{
			float targetFeet[3];
			FeetOf(target, targetFeet);
			ReportSighting(self.team, bot.targetClient, targetFeet, now);
			bot.noTraceTime = 0;
			float rangeScale = RangeTimeScale(skill, distanceSq, distMulti)
				* NoticeScale(bot, self, target, input);
			if (bot.lastAttacker >= 0 && bot.lastAttacker == bot.targetClient && rangeScale < shooterTimeScaleFloor)
			{
				rangeScale = shooterTimeScaleFloor;
			}
			bot.traceTime += static_cast<int>(serverFrameMs * rangeScale);
			if (bot.traceTime < 0)
			{
				bot.traceTime = 0;
			}
			bot.lastSeenPos[0] = target.eye[0];
			bot.lastSeenPos[1] = target.eye[1];
			bot.lastSeenPos[2] = target.eye[2];
			bot.lastSeenFeetZ = target.feetZ;
		}
		else
		{
			bot.noTraceTime += serverFrameMs;
			bot.traceTime -= static_cast<int>(serverFrameMs * traceBleedBySkill[bot.skillIndex]);
			if (bot.traceTime < 0)
			{
				bot.traceTime = 0;
			}
			if (bot.noTraceTime > skill.rememberTime)
			{
				bot.aiMode = "idle";
				bot.targetClient = -1;
				OnTargetLost(clientNum, chosen, bot.lastSeenPos);
				if (FinishThrow(clientNum, self, playerState, input))
				{
					return;
				}
				Idle(clientNum, self, input);
				return;
			}
		}

		bot.aiMode = hasSight ? "combat" : "lost";
		AimAndFire(clientNum, self, target, hasSight, input);
		if (hasSight)
		{
			bot.fightMoveForward = input.forward;
			bot.fightMoveRight = input.right;
		}
		else if (bot.noTraceTime <= sightFlickerMs && input.forward == 0 && input.right == 0)
		{
			input.forward = bot.fightMoveForward;
			input.right = bot.fightMoveRight;
		}
		if (bot.wantsStillShot)
		{
			bot.wantsStillShot = false;
			input.forward = 0;
			input.right = 0;
			input.buttons &= ~cmdButtonSprint;
			bot.wasWalking = false;
		}
	}


	static bool CanSeeFromProne(int clientNum, const PlayerView& self, const PlayerView& target)
	{
		const float proneEye[3] = { self.eye[0], self.eye[1], self.feetZ + proneEyeRise };
		const float chest[3] = { target.eye[0], target.eye[1], target.eye[2] - chestDrop };
		return SightLine(proneEye, chest, clientNum);
	}


	static bool IsGrenadeInHand(const char* playerState)
	{
		const int state = *reinterpret_cast<const int*>(playerState + psWeaponStateHand0);
		return state >= weaponStateOffhandFirst && state <= weaponStateOffhandStart;
	}


	static bool IsTeammateNear(int clientNum, const PlayerView& self, const float* point)
	{
		if (self.team == 0)
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
			if (!mate.isPlaying || mate.team != self.team)
			{
				continue;
			}
			float feet[3];
			FeetOf(mate, feet);
			if (DistanceSq2D(feet, point) < throwMateClearSq)
			{
				return true;
			}
		}
		return false;
	}


	static void AimGrenadeAt(int clientNum, BotInput& input, float yaw, float pitch)
	{
		BotState& bot = bots[clientNum];
		NoteTurn(bot, "throw", yaw);
		const float share = grenadeTurnBySkill[bot.skillIndex];
		const float rate = turnRateDegBySkill[bot.skillIndex];
		const float yawStep = TurnStep(AngleDelta(input.angles[1], yaw), share, rate);
		input.angles[1] += yawStep;
		KeepMoveHeading(input, yawStep);
		input.angles[0] += TurnStep(AngleDelta(input.angles[0], pitch), share, rate);
		input.angles[2] = 0.0f;
	}


	static bool IsThrowAimOff(const BotInput& input, const BotState& bot)
	{
		return std::fabs(AngleDelta(input.angles[1], bot.grenadeYaw)) > throwReleaseErrorDeg
			|| std::fabs(AngleDelta(input.angles[0], bot.grenadePitch)) > throwReleaseErrorDeg;
	}


	static bool SolveThrow(int clientNum, const PlayerView& self, int weaponIndex, const float* target, float* outYaw,
						   float* outPitch)
	{
		const float dx = target[0] - self.eye[0];
		const float dy = target[1] - self.eye[1];
		const float dz = target[2] - self.eye[2];
		const float range = std::sqrt(dx * dx + dy * dy);
		*outYaw = std::atan2(dy, dx) * radToDeg;

		float pitchUpDeg = grenadeUpMin + range * grenadeUpPerUnit;
		if (pitchUpDeg > grenadeUpMax)
		{
			pitchUpDeg = grenadeUpMax;
		}
		pitchUpDeg += dz * grenadeUpPerHeight;

		const char* weaponDef = WeaponDefOf(weaponIndex);
		const float speed = weaponDef
			? static_cast<float>(*reinterpret_cast<const int*>(weaponDef + weaponDefProjectileSpeed))
			: 0.0f;
		if (speed > 1.0f && range > 1.0f)
		{
			const float speedSq = speed * speed;
			const float discriminant = speedSq * speedSq
				- gravityUnits * (gravityUnits * range * range + 2.0f * dz * speedSq);
			if (discriminant >= 0.0f)
			{
				const float solved = std::atan((speedSq - std::sqrt(discriminant)) / (gravityUnits * range)) * radToDeg;
				pitchUpDeg = solved + Flrand(grenadeLiftMinDeg, grenadeLiftMaxDeg);
			}
		}
		*outPitch = -pitchUpDeg;

		const float yawRad = *outYaw / radToDeg;
		const float pitchRad = *outPitch / radToDeg;
		const float end[3] = {
			self.eye[0] + std::cos(pitchRad) * std::cos(yawRad) * grenadeSightUnits,
			self.eye[1] + std::cos(pitchRad) * std::sin(yawRad) * grenadeSightUnits,
			self.eye[2] - std::sin(pitchRad) * grenadeSightUnits,
		};
		return SightLine(self.eye, end, clientNum);
	}


	static int SeenAreaCount(int area)
	{
		int seen = 0;
		const int areaCount = Waypoints::AreaCount();
		for (int other = 0; other < areaCount; ++other)
		{
			if (other != area && Waypoints::AreasSee(area, other))
			{
				++seen;
			}
		}
		return seen;
	}

	static int PickThrowPoint(int clientNum, bool isAtLastSeen, float out[3])
	{
		const BotState& b = bots[clientNum];
		out[0] = b.lastSeenPos[0];
		out[1] = b.lastSeenPos[1];
		out[2] = b.lastSeenPos[2];
		if (isAtLastSeen)
		{
			return -1;
		}
		const float lastFeet[3] = { b.lastSeenPos[0], b.lastSeenPos[1], b.lastSeenFeetZ };
		int area = -1;
		if (FindHiddenAreas(AreaOfClient(clientNum), lastFeet, throwHiddenUnits, &area, 1) == 0)
		{
			return -1;
		}
		const float* origin = Waypoints::Origin(Waypoints::AreaAt(area)->node);
		out[0] = origin[0];
		out[1] = origin[1];
		out[2] = origin[2] + throwPointRise;
		return area;
	}

	static bool TryGrenade(int clientNum, const PlayerView& self, const char* playerState, BotInput& input,
						   bool allowSeen)
	{
		BotState& b = bots[clientNum];
		const int now = ServerTimeMs();

		if (b.grenadeThrowFrames > 0)
		{
			--b.grenadeThrowFrames;
			if (b.grenadeThrowFrames == grenadeFollowFrames - 1 && IsThrowAimOff(input, b)
				&& b.throwAimWaitFrames < throwAimWaitMax)
			{
				++b.throwAimWaitFrames;
				b.grenadeThrowFrames = grenadeFollowFrames;
			}
			if (b.grenadeThrowFrames >= grenadeFollowFrames)
			{
				AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
				input.buttons |= cmdButtonFrag;
				input.forward = 0;
				input.right = 0;
				return true;
			}

			if (b.grenadeThrowFrames == 0 && IsGrenadeInHand(playerState) && b.throwOverrunFrames < throwOverrunMaxFrames)
			{
				b.grenadeThrowFrames = 1;
				++b.throwOverrunFrames;
			}
			IdleHoldingProne(clientNum, self, input, false, now);
			input.buttons &= ~(cmdButtonSprint | cmdButtonAds | cmdButtonAttack | cmdButtonMelee
							   | cmdButtonReload | cmdButtonUp);
			AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
			return true;
		}

		b.grenadeAtSeen = false;
		const int lethal = FindLethalOffhand(playerState);
		if (now < b.grenadeCooldownTime || (!allowSeen && b.noTraceTime < grenadeMinLostMs) || !lethal
			|| b.grenadesThrown >= b.personality.grenadesPerLife || b.tacticalThrowFrames > 0 || now < b.nextThrowTime
			|| (!allowSeen && b.noTraceTime > grenadeMaxLostMs))
		{
			return false;
		}

		float throwPoint[3];
		int throwArea = PickThrowPoint(clientNum, allowSeen, throwPoint);
		const float dx = throwPoint[0] - self.eye[0];
		const float dy = throwPoint[1] - self.eye[1];
		const float horizSq = dx * dx + dy * dy;
		if (horizSq < grenadeMinRangeSq || horizSq > grenadeMaxRangeSq)
		{
			return false;
		}
		int chance = ScaleByTrait(grenadeChancePercent, b.personality.grenadeHabit);
		bool isCloset = throwArea >= 0 && SeenAreaCount(throwArea) <= closetSeenMax;
		if (isCloset)
		{
			chance *= closetThrowScale;
		}
		if (!RollPercent(chance))
		{
			return false;
		}
		if (throwArea >= 0 && !SightLine(b.lastSeenPos, throwPoint, clientNum))
		{
			throwArea = PickThrowPoint(clientNum, true, throwPoint);
			isCloset = false;
		}
		if (IsTeammateNear(clientNum, self, throwPoint))
		{
			return false;
		}
		if (!SolveThrow(clientNum, self, lethal, throwPoint, &b.grenadeYaw, &b.grenadePitch))
		{
			b.grenadeCooldownTime = now + grenadeBlockedMs;
			return false;
		}

		b.grenadeThrowFrames = grenadeHoldFrames + grenadeFollowFrames;
		b.throwOverrunFrames = 0;
		b.throwAimWaitFrames = 0;
		b.grenadeCooldownTime = now + grenadeCooldownMs;
		b.nextThrowTime = now + IrandMs(throwGapMinMs, throwGapMaxMs);
		b.grenadeAtSeen = allowSeen;
		++b.grenadesThrown;
		AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
		input.buttons |= cmdButtonFrag;
		input.forward = 0;
		input.right = 0;
		const char* placeNote = "";
		if (allowSeen)
		{
			placeNote = ", in sight";
		}
		else if (isCloset)
		{
			placeNote = ", into the closed room beside where they vanished";
		}
		else if (throwArea >= 0)
		{
			placeNote = ", into the hidden area beside where they vanished";
		}
		BotLog("frag client %d at %.0f %.0f %.0f pitch %.0f (%d of %d this life%s)", clientNum, throwPoint[0],
			throwPoint[1], throwPoint[2], b.grenadePitch, b.grenadesThrown, b.personality.grenadesPerLife, placeNote);
		return true;
	}

	static bool BurstAllows(int clientNum, const WeaponInfo& weapon, const char* playerState, float distanceSq, int now)
	{
		BotState& b = bots[clientNum];
		if (tuning.burst == 0 || distanceSq < sprayRangeSq)
		{
			return true;
		}

		if (weapon.hasStats)
		{
			const float bloom = *reinterpret_cast<const float*>(playerState + psAimSpreadScale) / 255.0f;
			float restDeg = 0.0f;
			float maxDeg = 0.0f;
			SpreadRangeDeg(weapon, playerState, &restDeg, &maxDeg);
			const float spreadDeg = restDeg + (maxDeg - restDeg) * bloom;
			const float bodyDeg = std::atan(bloomBodyHalfWidth / std::sqrt(distanceSq)) * radToDeg;
			const float bodyAllowance = bloomBodyMulti * (1.0f + static_cast<float>(6 - b.skillIndex) * bloomLowSkillShare);
			if (spreadDeg > bodyDeg * bodyAllowance && spreadDeg > restDeg * bloomOverRest)
			{
				b.isBloomBlocked = true;
				return false;
			}

			if (now >= b.burstEndTime)
			{
				b.isBurstFiring = !b.isBurstFiring;
				if (!b.isBurstFiring)
				{
					int decayMs = 0;
					if (weapon.stats.hip.decayRate > 0.0f)
					{
						decayMs = static_cast<int>(bloom / weapon.stats.hip.decayRate * 1000.0f);
					}
					b.burstEndTime = now + decayMs + IrandMs(burstFingerMinMs, burstFingerMaxMs);
				}
				else
				{
					int rounds = ShotsToKill(weapon.stats, std::sqrt(distanceSq), b.loadout) + burstOverRounds
						+ IrandMs(0, 1 + (6 - b.skillIndex) / 2);
					if (distanceSq > tapRangeSq && b.personality.sightDiscipline >= tapDisciplineMin)
					{
						rounds = IrandMs(tapRoundsMin, tapRoundsMax);
					}
					b.burstEndTime = now + rounds * weapon.stats.fireTimeMs;
				}
			}
			return b.isBurstFiring;
		}

		if (now >= b.burstEndTime)
		{
			b.isBurstFiring = !b.isBurstFiring;
			if (!b.isBurstFiring)
			{
				b.burstEndTime = now + IrandMs(burstGapMinMs, burstGapMaxMs);
			}
			else if (distanceSq > burstFarRangeSq)
			{
				b.burstEndTime = now + IrandMs(burstFarMinMs, burstFarMaxMs);
			}
			else
			{
				b.burstEndTime = now + IrandMs(burstMidMinMs, burstMidMaxMs);
			}
		}
		return b.isBurstFiring;
	}


	static bool TryTactical(int clientNum, const PlayerView& self, const char* playerState, BotInput& input)
	{
		BotState& b = bots[clientNum];
		const int now = ServerTimeMs();

		if (b.tacticalThrowFrames > 0)
		{
			--b.tacticalThrowFrames;
			if (b.tacticalThrowFrames == grenadeFollowFrames - 1 && IsThrowAimOff(input, b)
				&& b.throwAimWaitFrames < throwAimWaitMax)
			{
				++b.throwAimWaitFrames;
				b.tacticalThrowFrames = grenadeFollowFrames;
			}
			if (b.tacticalThrowFrames >= grenadeFollowFrames)
			{
				AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
				input.buttons |= cmdButtonSpecial;
				input.forward = 0;
				input.right = 0;
				return true;
			}

			if (b.tacticalThrowFrames == 0 && IsGrenadeInHand(playerState) && b.throwOverrunFrames < throwOverrunMaxFrames)
			{
				b.tacticalThrowFrames = 1;
				++b.throwOverrunFrames;
			}
			IdleHoldingProne(clientNum, self, input, false, now);
			input.buttons &= ~(cmdButtonSprint | cmdButtonAds | cmdButtonAttack | cmdButtonMelee
							   | cmdButtonReload | cmdButtonUp);
			AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
			return true;
		}

		const int tactical = FindTacticalOffhand(playerState);
		if (tuning.tactical == 0 || tuning.sniperLobby != 0 || b.personality.isQuickscoper || now < b.tacticalCooldownTime
			|| b.noTraceTime < grenadeMinLostMs || b.noTraceTime > grenadeMaxLostMs
			|| !tactical || b.tacticalsThrown >= b.personality.grenadesPerLife
			|| b.grenadeThrowFrames > 0 || now < b.nextThrowTime)
		{
			return false;
		}

		float throwPoint[3];
		int throwArea = PickThrowPoint(clientNum, false, throwPoint);
		const float dx = throwPoint[0] - self.eye[0];
		const float dy = throwPoint[1] - self.eye[1];
		const float horizSq = dx * dx + dy * dy;
		if (horizSq < tacticalMinRangeSq || horizSq > tacticalMaxRangeSq)
		{
			return false;
		}
		int chance = ScaleByTrait(tacticalChancePercent, b.personality.grenadeHabit);
		if (throwArea >= 0 && SeenAreaCount(throwArea) <= closetSeenMax)
		{
			chance *= closetThrowScale;
		}
		if (!RollPercent(chance))
		{
			return false;
		}
		if (throwArea >= 0 && !SightLine(b.lastSeenPos, throwPoint, clientNum))
		{
			throwArea = PickThrowPoint(clientNum, true, throwPoint);
		}
		if (IsTeammateNear(clientNum, self, throwPoint))
		{
			return false;
		}
		if (!SolveThrow(clientNum, self, tactical, throwPoint, &b.grenadeYaw, &b.grenadePitch))
		{
			b.tacticalCooldownTime = now + grenadeBlockedMs;
			return false;
		}

		b.tacticalThrowFrames = tacticalHoldFrames + grenadeFollowFrames;
		b.throwOverrunFrames = 0;
		b.throwAimWaitFrames = 0;
		b.tacticalCooldownTime = now + tacticalCooldownMs;
		b.nextThrowTime = now + IrandMs(throwGapMinMs, throwGapMaxMs);
		++b.tacticalsThrown;
		AimGrenadeAt(clientNum, input, b.grenadeYaw, b.grenadePitch);
		input.buttons |= cmdButtonSpecial;
		input.forward = 0;
		input.right = 0;
		const char* placeNote = "";
		if (throwArea >= 0)
		{
			placeNote = ", into the hidden area beside where they vanished";
		}
		BotLog("tactical client %d at %.0f %.0f %.0f%s", clientNum, throwPoint[0], throwPoint[1], throwPoint[2], placeNote);
		return true;
	}


	static int QuickscopeBeatMs(int skillIndex, const WeaponInfo& weapon)
	{
		int beat = IrandMs(quickscopeBeatMinMs, quickscopeBeatMaxMs) - skillIndex * quickscopeBeatSkillMs;
		if (beat < quickscopeBeatFloorMs)
		{
			beat = quickscopeBeatFloorMs;
		}
		if (weapon.hasStats && beat < weapon.stats.adsOutTimeMs)
		{
			beat = weapon.stats.adsOutTimeMs;
		}
		return beat;
	}


	static float WallbangShareFor(int clientNum, const float* eye, int weaponIndex, const float* point)
	{
		const char* def = WeaponDefOf(weaponIndex);
		const char* complete = BG_GetWeaponCompleteDef(weaponIndex);
		if (!def || !complete)
		{
			return 0.0f;
		}
		const int penetrateType = *reinterpret_cast<const int*>(def + weaponDefPenetrateType);
		float depthScale = *reinterpret_cast<const float*>(complete + weaponCompletePenetrateScale);
		const unsigned char perks = *reinterpret_cast<const unsigned char*>(PlayerStateOf(clientNum) + psPerks);
		if ((perks & psPerkArmorPiercing) != 0 && perk_bulletPenetrationMultiplier)
		{
			const char* dvar = *reinterpret_cast<const char* const*>(perk_bulletPenetrationMultiplier);
			if (dvar)
			{
				const float multiplier = *reinterpret_cast<const float*>(dvar + dvarCurrentValue);
				if (multiplier > 0.0f)
				{
					depthScale *= multiplier;
				}
			}
		}
		return Navgen::PenetrationShare(eye, point, penetrateType, depthScale);
	}

	static bool IsWallbanging(const BotState& bot, int now)
	{
		return now < bot.wallbangUntilTime && bot.wallbangTarget == bot.targetClient;
	}

	static void TryStartWallbang(int clientNum, const PlayerView& self, const Personality& personality, int now)
	{
		BotState& bot = bots[clientNum];
		if (bot.hasWallbangRoll)
		{
			return;
		}
		bot.hasWallbangRoll = true;
		if (personality.isQuickscoper || personality.isMeleeOnly)
		{
			return;
		}
		const WeaponInfo weapon = ReadWeapon(PlayerStateOf(clientNum));
		if (!weapon.index || !weapon.hasAmmo || weapon.clipRounds <= 0 || weapon.weapClass == weapClassSpread
			|| weapon.weapClass == weapClassSniper || weapon.weapClass == weapClassGrenade)
		{
			return;
		}
		const float dx = bot.lastSeenPos[0] - self.eye[0];
		const float dy = bot.lastSeenPos[1] - self.eye[1];
		const float dz = bot.lastSeenPos[2] - self.eye[2];
		if (dx * dx + dy * dy + dz * dz > wallbangMaxRangeSq)
		{
			return;
		}
		if (!RollPercent(ScaleByTrait(wallbangPercentBySkill[bot.skillIndex], personality.aggression)))
		{
			return;
		}
		const float chest[3] = { bot.lastSeenPos[0], bot.lastSeenPos[1], bot.lastSeenPos[2] - chestDrop };
		const float share = WallbangShareFor(clientNum, self.eye, weapon.index, chest);
		if (share >= wallbangOpenShare || Navgen::IsRenderBlocked(self.eye, chest))
		{
			BotLog("wallbang client %d holds off client %d: nothing solid between, only drawn cover hides them", clientNum,
				bot.targetClient);
			return;
		}
		if (share < wallbangMinShare)
		{
			BotLog("wallbang client %d holds off client %d: %.2f of a round would get through", clientNum,
				bot.targetClient, share);
			return;
		}
		bot.wallbangUntilTime = now + IrandMs(wallbangMinMs, wallbangMaxMs);
		bot.wallbangTarget = bot.targetClient;
		BotLog("wallbang client %d fires through the wall at client %d, %.2f of each round gets through", clientNum,
			bot.targetClient, share);
	}

	static void TryStartHeardWallbang(int clientNum, const PlayerView& self, const Personality& personality, int now)
	{
		BotState& bot = bots[clientNum];
		if (bot.heardClient < 0 || now - bot.heardTime > heardWallbangFreshMs || now < bot.heardBangNextTime)
		{
			return;
		}
		bot.heardBangNextTime = now + heardWallbangCooldownMs;
		if (personality.isQuickscoper || personality.isMeleeOnly)
		{
			return;
		}
		const PlayerView heard = ReadPlayerView(bot.heardClient);
		if (!IsEnemy(self, heard))
		{
			return;
		}
		const float dx = heard.eye[0] - self.eye[0];
		const float dy = heard.eye[1] - self.eye[1];
		const float dz = heard.eye[2] - self.eye[2];
		if (dx * dx + dy * dy + dz * dz > heardWallbangRangeSq)
		{
			return;
		}
		const int here = AreaOfClient(clientNum);
		const int there = AreaOfClient(bot.heardClient);
		if (here < 0 || there < 0 || Waypoints::AreasSee(here, there)
			|| Waypoints::WallbangShare(here, there) < heardWallbangTableMin)
		{
			return;
		}
		const WeaponInfo weapon = ReadWeapon(PlayerStateOf(clientNum));
		if (!weapon.index || !weapon.hasAmmo || weapon.clipRounds <= 0 || weapon.weapClass == weapClassSpread
			|| weapon.weapClass == weapClassSniper || weapon.weapClass == weapClassGrenade)
		{
			return;
		}
		if (!RollPercent(ScaleByTrait(wallbangPercentBySkill[bot.skillIndex], personality.aggression)))
		{
			return;
		}
		const float bearing = Flrand(0.0f, 6.2831853f);
		const float fuzz = Flrand(0.0f, heardWallbangFuzzUnits);
		const float chest[3] = { heard.eye[0] + std::cos(bearing) * fuzz, heard.eye[1] + std::sin(bearing) * fuzz,
								 heard.eye[2] - chestDrop };
		const float share = WallbangShareFor(clientNum, self.eye, weapon.index, chest);
		if (share >= wallbangOpenShare || Navgen::IsRenderBlocked(self.eye, chest))
		{
			return;
		}
		if (share < wallbangMinShare)
		{
			BotLog("wallbang client %d holds off the sound of client %d: %.2f of a round would get through", clientNum,
				bot.heardClient, share);
			return;
		}
		bot.heardBangPoint[0] = chest[0];
		bot.heardBangPoint[1] = chest[1];
		bot.heardBangPoint[2] = chest[2];
		bot.heardBangUntilTime = now + IrandMs(wallbangMinMs, wallbangMaxMs);
		BotLog("wallbang client %d fires through the wall at the sound of client %d, %.2f of each round gets through",
			clientNum, bot.heardClient, share);
	}

	static void AimAndFire(int clientNum, const PlayerView& self, const PlayerView& target,
						   bool hasSight, BotInput& input)
	{
		BotState& bot = bots[clientNum];
		const BotSkill& skill = bot.skillRow;
		Personality personality = bot.personality;
		if (bot.knifeForced)
		{
			personality.isMeleeOnly = true;
		}
		const int now = ServerTimeMs();

		if (hasSight)
		{
			bot.lostSpotOccludedSince = 0;
			bot.lostSpotDropped = false;
			bot.hasWallbangRoll = false;
			bot.wallbangUntilTime = 0;
		}

		const char* playerState = PlayerStateOf(clientNum);
		const float adsAmount = *reinterpret_cast<const float*>(playerState + psAdsAmount);
		const bool isShellshocked = IsShellshocked(clientNum, now);
		const int heldIndex = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		const char* heldName = heldIndex ? WeaponNameOf(heldIndex) : nullptr;
		const bool isLobGun = IsLobGunName(heldName);

		float aimTime = skill.aimTime;
		if (adsAmount > 0.0f && !personality.isQuickscoper)
		{
			aimTime *= 1.0f + adsAimSpeedMulti * adsAmount;
		}
		if (isShellshocked && aimTime < shellshockAimTime)
		{
			aimTime = shellshockAimTime;
		}

		float predictSteps = aimTime * (1000.0f / serverFrameMs);
		if (predictSteps < 1.0f)
		{
			predictSteps = 1.0f;
		}

		float aimPos[3];
		if (hasSight)
		{
			if (now >= bot.trackSampleTime)
			{
				const int lagMs = trackLagMsBySkill[bot.skillIndex];
				bot.trackSamplePos[0] = target.eye[0];
				bot.trackSamplePos[1] = target.eye[1];
				bot.trackSamplePos[2] = target.eye[2];
				bot.trackSampleTime = now + IrandMs(lagMs * 7 / 10, lagMs * 13 / 10);
			}
			aimPos[0] = bot.trackSamplePos[0];
			aimPos[1] = bot.trackSamplePos[1];
			aimPos[2] = bot.trackSamplePos[2];

			if (bot.pitchEndTime <= now)
			{
				const float seenX = target.eye[0] - self.eye[0];
				const float seenY = target.eye[1] - self.eye[1];
				float maxDrop = (seenX * seenX + seenY * seenY) < aimDropFarSq ? aimDropNear : aimDropFar;
				if (bot.skillIndex <= aimBodySkillMax)
				{
					maxDrop += aimDropLowSkill;
				}
				bot.aimDrop = Flrand(0.0f, maxDrop);
				bot.pitchEndTime = now + IrandMs(pitchHoldMinMs, pitchHoldMaxMs);
			}
			aimPos[2] -= bot.aimDrop;
			if (aimPos[2] < target.feetZ + shinRise)
			{
				aimPos[2] = target.feetZ + shinRise;
			}

			if (bot.sightAtFeet)
			{
				FeetOf(target, aimPos);
				aimPos[2] += shinRise;
			}
		}
		else
		{
			int lookMs = skill.noTraceLookTime;
			if (lookMs > lostLookMaxMs)
			{
				lookMs = lostLookMaxMs;
			}
			if (bot.noTraceTime > lookMs)
			{
				if (FinishThrow(clientNum, self, playerState, input))
				{
					return;
				}
				Idle(clientNum, self, input);
				return;
			}

			if (SightLine(self.eye, bot.lastSeenPos, clientNum))
			{
				bot.lostSpotOccludedSince = 0;
			}
			else if (bot.lostSpotOccludedSince == 0)
			{
				bot.lostSpotOccludedSince = now;
				TryStartWallbang(clientNum, self, personality, now);
			}
			else if (now - bot.lostSpotOccludedSince >= lostSpotGraceMs && !IsWallbanging(bot, now))
			{
				bot.lostSpotDropped = true;
			}
			if (bot.lostSpotDropped)
			{
				if (TryGrenade(clientNum, self, playerState, input)
					|| TryTactical(clientNum, self, playerState, input))
				{
					return;
				}
				IdleHoldingProne(clientNum, self, input, true, now);
				return;
			}
			aimPos[0] = bot.lastSeenPos[0];
			aimPos[1] = bot.lastSeenPos[1];
			aimPos[2] = bot.lastSeenPos[2];
			if (IsWallbanging(bot, now))
			{
				aimPos[2] -= chestDrop;
			}
		}

		const float settleMs = static_cast<float>(aimSettleMsBySkill[bot.skillIndex]);
		float errorDeg = 0.0f;
		if (static_cast<float>(bot.traceTime) < settleMs)
		{
			errorDeg = aimErrorDegBySkill[bot.skillIndex] * bot.aimErrorScale
				* (1.0f - static_cast<float>(bot.traceTime) / settleMs);
		}

		if (hasSight)
		{
			const int breakMs = errorBreakMsBySkill[bot.skillIndex];
			if (bot.aimBreakNextTime == 0)
			{
				bot.aimBreakNextTime = now + IrandMs(breakMs * 7 / 10, breakMs * 13 / 10);
			}
			else if (now >= bot.aimBreakNextTime)
			{
				bot.aimBreakNextTime = now + IrandMs(breakMs * 7 / 10, breakMs * 13 / 10);
				bot.aimBreakUntilTime = now + aimBreakMs;
				for (int axis = 0; axis < 3; ++axis)
				{
					bot.aimOffsetBase[axis] = Flrand(-1.0f, 1.0f);
				}
				BotLog("aim client %d hand slips on client %d", clientNum, bot.targetClient);
			}
		}
		if (bot.aimBreakUntilTime > now)
		{
			const float slipDeg = slipDegBySkill[bot.skillIndex] * bot.aimErrorScale;
			if (errorDeg < slipDeg)
			{
				errorDeg = slipDeg;
			}
		}
		if (errorDeg > 0.0f)
		{
			const float errX = aimPos[0] - self.eye[0];
			const float errY = aimPos[1] - self.eye[1];
			const float errZ = aimPos[2] - self.eye[2];
			const float errorUnits = std::sqrt(errX * errX + errY * errY + errZ * errZ) * std::tan(errorDeg / radToDeg);
			const float flatUnits = std::sqrt(errX * errX + errY * errY);
			if (flatUnits > 1.0f)
			{
				aimPos[0] += -errY / flatUnits * bot.aimOffsetBase[0] * errorUnits;
				aimPos[1] += errX / flatUnits * bot.aimOffsetBase[0] * errorUnits;
			}
			aimPos[2] += bot.aimOffsetBase[2] * errorUnits * aimErrorVerticalShare;
		}

		if (hasSight)
		{
			const char* targetState = PlayerStateOf(bot.targetClient);
			const float* targetVelocity = reinterpret_cast<const float*>(targetState + psVelocity);
			float lead = (serverFrameMs / 1000.0f) * (predictSteps - 1.0f);
			if (lead > maxLeadSec)
			{
				lead = maxLeadSec;
			}
			for (int axis = 0; axis < 3; ++axis)
			{
				aimPos[axis] += targetVelocity[axis] * lead;
			}

			if (!SightLine(self.eye, aimPos, clientNum))
			{
				if (bot.sightAtFeet)
				{
					FeetOf(target, aimPos);
					aimPos[2] += shinRise;
				}
				else
				{
					aimPos[0] = target.eye[0];
					aimPos[1] = target.eye[1];
					aimPos[2] = target.eye[2];
				}
			}
		}

		if (isLobGun && hasSight)
		{
			aimPos[2] = target.feetZ + lobFeetRise;
		}
		float toX = aimPos[0] - self.eye[0];
		float toY = aimPos[1] - self.eye[1];
		const float toZ = aimPos[2] - self.eye[2];

		float yaw = std::atan2(toY, toX) * radToDeg;
		float pitch = -std::atan2(toZ, std::sqrt(toX * toX + toY * toY)) * radToDeg;
		if (isLobGun && hasSight)
		{
			const char* lobDef = WeaponDefOf(heldIndex);
			float speed = 0.0f;
			if (lobDef)
			{
				speed = static_cast<float>(*reinterpret_cast<const int*>(lobDef + weaponDefProjectileSpeed));
			}
			const float* targetVelocity = reinterpret_cast<const float*>(PlayerStateOf(bot.targetClient) + psVelocity);
			float lobPitchDeg = lobMaxPitchDeg;
			for (int pass = 0; pass < 2; ++pass)
			{
				const float range = std::sqrt(toX * toX + toY * toY);
				lobPitchDeg = lobMaxPitchDeg;
				if (speed > 1.0f && range > 1.0f)
				{
					const float speedSq = speed * speed;
					const float discriminant = speedSq * speedSq
						- gravityUnits * (gravityUnits * range * range + 2.0f * toZ * speedSq);
					if (discriminant >= 0.0f)
					{
						lobPitchDeg = std::atan((speedSq - std::sqrt(discriminant)) / (gravityUnits * range)) * radToDeg;
					}
				}
				if (pass == 1 || speed <= 1.0f)
				{
					break;
				}
				float flightSec = range / (speed * std::cos(lobPitchDeg / radToDeg));
				if (flightSec > lobMaxLeadSec)
				{
					flightSec = lobMaxLeadSec;
				}
				toX += targetVelocity[0] * flightSec * lobLeadShare;
				toY += targetVelocity[1] * flightSec * lobLeadShare;
			}
			yaw = std::atan2(toY, toX) * radToDeg;
			pitch = -lobPitchDeg;
		}

		const float kickControl = kickControlBySkill[bot.skillIndex];
		const float kickFollow = static_cast<float>(serverFrameMs)
			/ static_cast<float>(kickControlLagMsBySkill[bot.skillIndex] + serverFrameMs);
		bot.kickComp[0] += (bot.kickAngle[0] * kickControl - bot.kickComp[0]) * kickFollow;
		bot.kickComp[1] += (bot.kickAngle[1] * kickControl * kickYawControlShare - bot.kickComp[1]) * kickFollow;
		pitch -= bot.kickComp[0];
		yaw -= bot.kickComp[1];

		float wobble = wobbleDegBySkill[bot.skillIndex];
		float selfVelocity[3];
		ReadVelocity(clientNum, selfVelocity);
		if (selfVelocity[0] * selfVelocity[0] + selfVelocity[1] * selfVelocity[1] > wobbleMovingSpeedSq)
		{
			wobble *= wobbleMovingScale;
		}
		if (*reinterpret_cast<const float*>(playerState + psAdsAmount) > 0.5f)
		{
			wobble *= wobbleAdsScale;
		}
		const float wobbleTime = static_cast<float>(now);
		const float phase = personality.wobblePhase;
		const float yawWave = std::sin(wobbleTime * 0.0055f * personality.wobbleYawRate + phase) * 0.7f
			+ std::sin(wobbleTime * 0.0079f * personality.wobbleYawRate + phase * 1.7f) * 0.3f;
		const float pitchWave = std::sin(wobbleTime * 0.0043f * personality.wobblePitchRate + 1.0f + phase) * 0.7f
			+ std::sin(wobbleTime * 0.0067f * personality.wobblePitchRate + phase * 2.3f) * 0.3f;
		yaw += yawWave * wobble;
		pitch += pitchWave * wobble * 0.6f;

		float turn = 1.0f / predictSteps;
		const float noticeMs = static_cast<float>(skill.reactionTime) * noticeShare;
		const bool noticed = static_cast<float>(bot.traceTime) > noticeMs;
		if (!noticed)
		{
			turn *= driftTurnShareBySkill[bot.skillIndex];
		}
		else
		{
			float ramp = (static_cast<float>(bot.traceTime) - noticeMs) / acquireRampMsBySkill[bot.skillIndex];
			if (ramp > 1.0f)
			{
				ramp = 1.0f;
			}
			const float rampFloor = acquireRampFloorBySkill[bot.skillIndex];
			turn *= rampFloor + (1.0f - rampFloor) * ramp;
		}
		const float yawDelta = AngleDelta(input.angles[1], yaw);
		if (std::fabs(yawDelta) >= slowTurnStartDeg)
		{
			float slowShare = 1.0f - std::fabs(yawDelta) / 180.0f;
			if (slowShare < slowTurnMinShare)
			{
				slowShare = slowTurnMinShare;
			}
			turn *= slowShare;
		}
		if (noticed && hasSight && bot.flickUntilTime == 0)
		{
			const float overShare = flickOverShareBySkill[bot.skillIndex];
			bot.flickOverYaw = yawDelta * overShare;
			bot.flickOverPitch = AngleDelta(input.angles[0], pitch) * overShare;
			bot.flickUntilTime = now + flickMs;
		}
		if (bot.flickUntilTime > now)
		{
			const float flickShare = static_cast<float>(bot.flickUntilTime - now) / static_cast<float>(flickMs);
			yaw += bot.flickOverYaw * flickShare;
			pitch += bot.flickOverPitch * flickShare;
		}
		const float turnRate = turnRateDegBySkill[bot.skillIndex];
		const bool isMidThrow = bot.grenadeThrowFrames > 0 || bot.tacticalThrowFrames > 0;
		if (!isMidThrow)
		{
			NoteTurn(bot, "aim", yaw);
			input.angles[1] += TurnStep(AngleDelta(input.angles[1], yaw), turn, turnRate);
			input.angles[0] += TurnStep(AngleDelta(input.angles[0], pitch), turn, turnRate);
		}
		input.angles[2] = 0.0f;
		const bool lobOnTarget = std::fabs(AngleDelta(input.angles[1], yaw)) < lobGateDeg
			&& std::fabs(AngleDelta(input.angles[0], pitch)) < lobGateDeg;

		if (!hasSight)
		{
			bot.flinchPending = false;

			const bool keepsScope = personality.isQuickscoper && bot.scopeUpTime != 0
				&& bot.noTraceTime <= quickscopeLostGraceMs && now - bot.scopeUpTime <= quickscopeScopeMs;
			if (!keepsScope)
			{
				bot.scopeUpTime = 0;
			}

			if (TryGrenade(clientNum, self, playerState, input) || TryTactical(clientNum, self, playerState, input))
			{
				return;
			}

			const WeaponInfo pursuitWeapon = ReadWeapon(playerState);
			const bool wasFiringAtLoss = now - bot.lastFireTime < bot.noTraceTime + 300;
			const bool spraysAfter = pursuitWeapon.index && pursuitWeapon.hasAmmo && wasFiringAtLoss
				&& pursuitWeapon.weapClass != weapClassSniper
				&& bot.noTraceTime <= skill.shootAfterMs
				&& ConeDot(self.eye, aimPos, input.angles[1], input.angles[0]) > fireConeDotHip
				&& !Navgen::IsRenderBlocked(self.eye, aimPos);
			const bool isWallbanging = IsWallbanging(bot, now) && pursuitWeapon.index && pursuitWeapon.hasAmmo
				&& pursuitWeapon.clipRounds > 0
				&& ConeDot(self.eye, aimPos, input.angles[1], input.angles[0]) > fireConeDotHip;
			if (spraysAfter || isWallbanging)
			{
				if (pursuitWeapon.isFullAuto)
				{
					input.buttons |= cmdButtonAttack;
					bot.lastFireTime = now;
				}
				else if (bot.semiCooldown <= 0)
				{
					input.buttons |= cmdButtonAttack;
					bot.semiCooldown = SemiGapMs(skill, bot.skillIndex, pursuitWeapon);
					bot.lastFireTime = now;
				}
			}
			else if (pursuitWeapon.index
					 && ((pursuitWeapon.clipRounds == 0 && bot.noTraceTime >= reloadLostDryMs)
						 || (pursuitWeapon.clipRounds < LowClipRounds(pursuitWeapon)
							 && bot.noTraceTime >= reloadSafeMs && IsQuiet(bot, now)
							 && RollPercent(reloadChancePercent))))
			{
				input.buttons |= cmdButtonReload;
				if (now >= bot.reloadGuardUntilTime)
				{
					bot.reloadGuardUntilTime = now + ReloadGuardMs(bot.loadout, pursuitWeapon);
				}
			}

			const float lostDistSq = toX * toX + toY * toY + toZ * toZ;
			float lostAdsGateDot = adsOnDot;
			if ((bot.sentButtons & cmdButtonAds) != 0)
			{
				lostAdsGateDot = adsOffDot;
			}
			const bool isScopeHolderLost = pursuitWeapon.weapClass == weapClassSniper && !personality.isQuickscoper;
			int lostAdsMs = skill.noTraceAdsTime;
			if (!isScopeHolderLost && lostAdsMs > lostAdsMaxMs)
			{
				lostAdsMs = lostAdsMaxMs;
			}

			if (keepsScope)
			{
				input.buttons |= cmdButtonAds;
			}
			else if (bot.noTraceTime <= lostAdsSettleMs)
			{
				input.buttons |= bot.sentButtons & cmdButtonAds;
			}
			else if ((bot.noTraceTime <= lostAdsMs || IsWallbanging(bot, now)) && pursuitWeapon.index
					 && !personality.isQuickscoper
					 && pursuitWeapon.weapClass != weapClassSniper
					 && CanAds(pursuitWeapon, lostDistSq, personality.hipScale)
					 && ConeDot(self.eye, aimPos, input.angles[1], input.angles[0]) > lostAdsGateDot)
			{
				input.buttons |= cmdButtonAds;
			}

			int lostHoldMs = bot.lostHoldMs;
			if (bot.fightStyle == fightStylePush)
			{
				lostHoldMs /= 2;
			}
			if (bot.noTraceTime < lostHoldMs)
			{
				input.forward = 0;
				input.right = 0;
				return;
			}
			if (now < bot.footworkUntilTime)
			{
				input.forward = bot.footworkForward;
				input.right = bot.footworkRight;
				return;
			}
			IdleHoldingProne(clientNum, self, input, false, now);
			return;
		}

		const float distanceSq = toX * toX + toY * toY + toZ * toZ;
		const WeaponInfo weapon = ReadWeapon(playerState);
		bot.isBloomBlocked = false;

		if (bot.flinchPending)
		{
			bot.flinchPending = false;
			NoteTurn(bot, "flinch", input.angles[1]);
			float flinchPitch = static_cast<float>(bot.flinchDamage) * flinchDegPerDamage;
			if (flinchPitch < flinchPitchMinDeg)
			{
				flinchPitch = flinchPitchMinDeg;
			}
			if (flinchPitch > flinchPitchMaxDeg)
			{
				flinchPitch = flinchPitchMaxDeg;
			}
			input.angles[0] -= flinchPitch * Flrand(0.8f, 1.2f);
			input.angles[1] += Flrand(-flinchYawMaxDeg, flinchYawMaxDeg) * flinchPitch / flinchPitchMaxDeg;
			const char* selfEntity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
			const int health = *reinterpret_cast<const int*>(selfEntity + gentityHealth);
			if (tuning.cover && health > 0 && health < lowHealth && now >= bot.coverCooldownTime
				&& bot.task.kind != TaskCover && !personality.isQuickscoper
				&& RollPercent(breakOffPercent + personality.caution / 2))
			{
				bot.coverCooldownTime = now + coverCooldownMs;
				if (RequestCover(clientNum, self, target.eye))
				{
					BotLog("cover client %d breaks off from client %d at %d hp", clientNum, bot.targetClient, health);
					Idle(clientNum, self, input, false);
					return;
				}
			}
			if (bot.crouchEndTime <= now && RollPercent(flinchCrouchPercent + personality.caution / 4))
			{
				bot.crouchEndTime = now + IrandMs(flinchCrouchMinMs, flinchCrouchMaxMs);
				BotLog("flinch client %d ducks under fire from client %d", clientNum, bot.targetClient);
			}
			if (bot.strafeDirection != 0 && RollPercent(flinchFlipPercent))
			{
				FlipStrafe(bot, now);
			}
		}

		if (weapon.index && !weapon.hasAmmo && !personality.isMeleeOnly)
		{
			bool handled = bot.swapWeapon != 0;
			if (!handled && tuning.swap && weapon.weapClass != weapClassPistol && !bot.swapBackTime)
			{
				const unsigned short sidearm = FindSidearm(playerState);

				bool sidearmWins = distanceSq < swapRangeSq;
				if (!sidearmWins && sidearm && weapon.hasStats && distanceSq < swapRangeSq * swapReachMulti * swapReachMulti)
				{
					sidearmWins = weapon.stats.reloadAddTimeMs > SwapMs(weapon, sidearm) + swapEdgeMs;
				}
				if (sidearmWins && sidearm && sidearm != weapon.index)
				{
					bot.swapWeapon = sidearm;
					bot.swapIsSidearm = true;
					bot.swapGiveUpTime = now + SwapGiveUpMs(weapon, sidearm);
					input.weapon = sidearm;
					handled = true;
					BotLog("swap client %d to its sidearm, dry on client %d at %.0f", clientNum,
						bot.targetClient, std::sqrt(distanceSq));
				}
			}
			if (!handled && tuning.cover && now >= bot.coverCooldownTime && bot.task.kind != TaskCover)
			{
				bot.coverCooldownTime = now + coverCooldownMs;
				handled = RequestCover(clientNum, self, target.eye);
			}
			if (!handled)
			{
				input.buttons |= cmdButtonReload;
				if (now >= bot.reloadGuardUntilTime)
				{
					bot.reloadGuardUntilTime = now + ReloadGuardMs(bot.loadout, weapon);
				}
			}
		}

		const bool isSniper = weapon.weapClass == weapClassSniper;

		const bool isQuickscoper = isSniper && personality.isQuickscoper;

		const bool justFired = !isQuickscoper && bot.lastFireTime != 0
			&& (now - bot.lastFireTime) < sniperRescopeMs;

		const float coneDot = ConeDot(self.eye, aimPos, input.angles[1], input.angles[0]);
		const float distance = std::sqrt(distanceSq);
		float reactScale = reactNearScale
			+ (distance - reactNearUnits) / (reactFarUnits - reactNearUnits) * (reactFarScale - reactNearScale);
		if (reactScale < reactNearScale)
		{
			reactScale = reactNearScale;
		}
		if (reactScale > reactFarScale)
		{
			reactScale = reactFarScale;
		}
		float farShare = (distance - reactNearUnits) / (reactFarUnits - reactNearUnits);
		if (farShare < 0.0f)
		{
			farShare = 0.0f;
		}
		if (farShare > 1.0f)
		{
			farShare = 1.0f;
		}
		float reactMs = static_cast<float>(skill.reactionTime) * reactScale * bot.reactionJitter;
		const float reactFloorMs = static_cast<float>(reactionFloorNearMs)
			+ static_cast<float>(reactionFloorFarMs - reactionFloorNearMs) * farShare;
		if (reactMs < reactFloorMs)
		{
			reactMs = reactFloorMs;
		}
		const bool reacted = static_cast<float>(bot.traceTime) > reactMs;

		bool wantAds = !isLobGun && !personality.isMeleeOnly && CanAds(weapon, distanceSq, personality.hipScale)
			&& IsInRange(weapon, distanceSq);
		if (bot.hipCloseTarget != bot.targetClient)
		{
			bot.hipCloseTarget = bot.targetClient;
			bot.prefersHipClose = RollPercent(closeHipPercentBySkill[bot.skillIndex]);
			const int holdMinMs = lostHoldMinMsBySkill[bot.skillIndex];
			bot.lostHoldMs = IrandMs(holdMinMs, holdMinMs + lostHoldSpanMs) * (70 + personality.caution * 6 / 10) / 100;
		}
		const bool isHipCloseGun = weapon.weapClass == weapClassRifle || weapon.weapClass == weapClassSmg
			|| weapon.weapClass == weapClassMg;
		float hipCloseSq = closeHipRangeSq;
		if (weapon.weapClass == weapClassRifle || weapon.weapClass == weapClassMg)
		{
			hipCloseSq = closeHipRangeRifleSq;
		}
		if (bot.prefersHipClose && isHipCloseGun && distanceSq < hipCloseSq)
		{
			wantAds = false;
		}
		if (isQuickscoper)
		{
			const int scopeAgeMs = bot.scopeUpTime != 0 ? now - bot.scopeUpTime : 0;
			const bool nearlyOn = coneDot > fireConeDotAds && scopeAgeMs < quickscopeScopeMaxMs;
			if (bot.scopeUpTime != 0 && scopeAgeMs > quickscopeScopeMs && !nearlyOn)
			{
				bot.scopeUpTime = 0;
				if (bot.scopeMisses < quickscopeMissMax)
				{
					++bot.scopeMisses;
				}
				bot.scopeDownUntilTime = now + QuickscopeBeatMs(bot.skillIndex, weapon)
					+ bot.scopeMisses * quickscopeMissBeatMs;
			}
			const bool fullyReacted = reacted && bot.traceTime > quickscopeReactMinMs;
			const bool isUp = bot.scopeUpTime != 0;
			const bool isDown = adsAmount <= quickscopeDownAmount;
			wantAds = now >= bot.scopeDownUntilTime && fullyReacted
				&& (isUp || (isDown && coneDot > quickscopeScopeInDot));
			if (wantAds && !isUp)
			{
				bot.scopeUpTime = now;
			}
			if (!wantAds)
			{
				bot.scopeUpTime = 0;
			}
		}
		else if (wantAds)
		{
			int holdMs = adsHoldMs;
			if (weapon.hasStats && holdMs < weapon.stats.adsInTimeMs + weapon.stats.adsOutTimeMs + adsHoldSlopMs)
			{
				holdMs = weapon.stats.adsInTimeMs + weapon.stats.adsOutTimeMs + adsHoldSlopMs;
			}
			bot.adsHoldUntilTime = now + holdMs;
		}
		else if (now < bot.adsHoldUntilTime)
		{
			wantAds = true;
		}
		if (wantAds && isSniper && justFired)
		{
			wantAds = false;
			bot.adsHoldUntilTime = 0;
		}
		float adsKeepDot = adsOnDot;
		if ((bot.sentButtons & cmdButtonAds) != 0)
		{
			adsKeepDot = adsOffDot;
		}
		if (wantAds && !isSniper && coneDot < adsKeepDot)
		{
			wantAds = false;
		}

		if (wantAds)
		{
			input.buttons |= cmdButtonAds;
			if (isSniper && !isQuickscoper && adsAmount >= 1.0f)
			{
				input.buttons |= cmdButtonBreath;
			}
		}

		if (FinishThrow(clientNum, self, playerState, input))
		{
			return;
		}
		float targetVelocity[3];
		ReadVelocity(bot.targetClient, targetVelocity);
		const bool isTargetStill = targetVelocity[0] * targetVelocity[0] + targetVelocity[1] * targetVelocity[1]
			< sightedGrenadeMaxSpeedSq;
		if (distanceSq > sightedGrenadeRangeSq && !wantAds && bot.grenadeThrowFrames == 0 && isTargetStill
			&& static_cast<int>(NextRand() % 1000u) < sightedGrenadePerMille * personality.grenadeHabit / 50
			&& TryGrenade(clientNum, self, playerState, input, true))
		{
			return;
		}

		if (personality.isTuber && weapon.index)
		{
			const char* completeDef = BG_GetWeaponCompleteDef(weapon.index);
			const int altIndex = completeDef ? *reinterpret_cast<const int*>(completeDef + weaponCompleteAltWeaponIndex) : 0;
			if (isLobGun)
			{
				if (bot.tubeStartTime == 0)
				{
					bot.tubeStartTime = now;
				}
				const bool fired = bot.lastFireTime >= bot.tubeStartTime && now - bot.lastFireTime < tubeAfterShotMs;
				if ((fired || now >= bot.tubeUntilTime) && altIndex && !bot.swapWeapon)
				{
					bot.swapWeapon = static_cast<unsigned short>(altIndex);
					bot.swapIsSidearm = false;
					bot.swapGiveUpTime = now + swapGiveUpMs;
					input.weapon = bot.swapWeapon;
					bot.tubeUntilTime = 0;
					bot.tubeStartTime = 0;
					BotLog("tube client %d %s, back to %s", clientNum, fired ? "fired" : "gave up", WeaponNameOf(altIndex));
				}
			}
			else if (altIndex && bot.tubeCooldownTime <= now && bot.tubeUntilTime <= now && !bot.swapWeapon
					 && bot.yyFrames == 0 && reacted && distanceSq > tubeMinRangeSq && distanceSq < tubeMaxRangeSq
					 && FindHeldWeapon(playerState, [altIndex](int held) { return held == altIndex; }) != 0
					 && ClipRoundsOf(playerState, altIndex) > 0)
			{
				bot.swapWeapon = static_cast<unsigned short>(altIndex);
				bot.swapIsSidearm = false;
				bot.swapGiveUpTime = now + swapGiveUpMs;
				input.weapon = bot.swapWeapon;
				bot.tubeUntilTime = now + tubeHoldMs;
				bot.tubeCooldownTime = now + IrandMs(tubeGapMinMs, tubeGapMaxMs);
				bot.tubeStartTime = 0;
				BotLog("tube client %d takes the launcher for client %d at %.0f", clientNum, bot.targetClient, distance);
			}
		}

		float adsReady = 1.0f;
		if (!isSniper)
		{
			adsReady = 1.0f - (1.0f - static_cast<float>(tuning.prefire) / 100.0f) * prefireShareBySkill[bot.skillIndex];
			if (adsReady < prefireReadyMin)
			{
				adsReady = prefireReadyMin;
			}
			if (distanceSq > prefireCloseRangeSq && adsReady < adsFireReadyMin)
			{
				adsReady = adsFireReadyMin;
			}
		}
		bool scoped = !wantAds || adsAmount >= adsReady;
		float coneGate = wantAds ? fireConeDotAds : fireConeDotHip;
		const float sizeGateDot = std::cos(std::atan(fireGateBodyUnits * fireGateScaleBySkill[bot.skillIndex] / distance));
		if (sizeGateDot > coneGate)
		{
			coneGate = sizeGateDot;
		}
		if (reacted && !bot.hasOpenedFire)
		{
			bot.hasOpenedFire = true;
			const int wideMs = wideBurstMsBySkill[bot.skillIndex];
			bot.wideGateUntilTime = now + IrandMs(wideMs * 7 / 10, wideMs * 13 / 10);
			BotLog("fire client %d opens on client %d at %.0f, wide for %d ms", clientNum, bot.targetClient, distance,
				bot.wideGateUntilTime - now);
			if (debugOn)
			{
				const float bounds[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
				const int ignore[4] = { -1, -1, 0, 0 };
				unsigned char trace[128] = {};
				SV_Trace(trace, self.eye, target.eye, bounds, ignore, seeThroughProbeMask, 0, nullptr, 1);
				const float fraction = *reinterpret_cast<const float*>(trace + traceFraction);
				if (fraction < 1.0f)
				{
					const int contents = *reinterpret_cast<const int*>(trace + traceContents);
					const int surfaceFlags = *reinterpret_cast<const int*>(trace + traceSurfaceFlags);
					BotLog("seethrough client %d on client %d: contents 0x%x surface %d flags 0x%x at %.0f of %.0f",
						clientNum, bot.targetClient, contents, (surfaceFlags >> 20) & 0x1F, surfaceFlags,
						fraction * distance, distance);
				}
			}
		}
		if (!isSniper && bot.wideGateUntilTime > now)
		{
			coneGate = fireConeDotHip;
		}
		if (isSniper)
		{
			scoped = wantAds && adsAmount >= 1.0f;
			coneGate = fireConeDotAds;
			const float bodyDot = std::cos(std::atan(sniperBodyHalfWidth / distance));
			if (!isQuickscoper && bodyDot > coneGate)
			{
				coneGate = bodyDot;
			}
		}

		bool isWellSeen = true;
		if (isSniper)
		{
			const float chest[3] = { target.eye[0], target.eye[1], target.eye[2] - chestDrop };
			isWellSeen = !bot.sightAtFeet && SightLine(self.eye, chest, clientNum);
		}

		bool onTarget = coneDot > coneGate;
		if (isLobGun)
		{
			onTarget = lobOnTarget;
		}
		else
		{
			const float chest[3] = { target.eye[0], target.eye[1], target.eye[2] - chestDrop };
			const float trueDot = ConeDot(self.eye, chest, input.angles[1] + bot.kickAngle[1], input.angles[0] + bot.kickAngle[0]);
			const float trueGateDot = std::cos(std::atan(trueGateBodyUnits / distance));
			if (trueDot < trueGateDot)
			{
				onTarget = false;
			}
		}
		if (noticed && !bot.panicRolled)
		{
			bot.panicRolled = true;
			if (weapon.isFullAuto && !isSniper && distanceSq < panicRangeSq
				&& RollPercent(ScaleByTrait(panicPercent, personality.aggression)))
			{
				bot.panicUntilTime = now + IrandMs(panicMinMs, panicMaxMs);
				BotLog("panic client %d sprays at client %d before it has reacted", clientNum, bot.targetClient);
			}
		}
		const bool panicking = !reacted && now < bot.panicUntilTime && coneDot > fireConeDotHip;
		if (panicking)
		{
			onTarget = true;
			scoped = true;
		}
		const bool mayShoot = (reacted || panicking) && scoped && onTarget && isWellSeen && !personality.isMeleeOnly
			&& weapon.index && weapon.hasAmmo && IsInRange(weapon, distanceSq)
			&& now >= bot.fireHoldTime && !IsTeammateInLine(clientNum, self, distance, input);

		if (mayShoot)
		{
			if (weapon.isFullAuto)
			{
				if (BurstAllows(clientNum, weapon, playerState, distanceSq, now))
				{
					input.buttons |= cmdButtonAttack;
					bot.lastFireTime = now;
				}
			}
			else if (bot.semiCooldown <= 0 && isSniper && bot.settleFrames < sniperSettleMaxFrames
					 && IsMovingFaster(clientNum, sniperStillSpeed))
			{
				++bot.settleFrames;
				bot.wantsStillShot = true;
			}
			else if (bot.semiCooldown <= 0)
			{
				bot.settleFrames = 0;
				input.buttons |= cmdButtonAttack;
				bot.semiCooldown = SemiGapMs(skill, bot.skillIndex, weapon);
				bot.lastFireTime = now;
				const int boltMs = BoltMs(weapon);
				if (isSniper && bot.noSprintUntilTime < now + boltMs)
				{
					bot.noSprintUntilTime = now + boltMs;
				}
				if (isQuickscoper)
				{
					bot.scopeUpTime = 0;
					bot.scopeMisses = 0;
					bot.scopeDownUntilTime = now + QuickscopeBeatMs(bot.skillIndex, weapon);

					const int afterShotTime = now + IrandMs(yyAfterShotMinMs, yyAfterShotMaxMs);
					if (bot.yyNextTime < afterShotTime)
					{
						bot.yyNextTime = afterShotTime;
					}
				}
			}
		}
		else
		{
			bot.settleFrames = 0;
		}

		const float lungeSq = personality.isMeleeOnly ? knifeLungeSq : meleeRangeSq;
		if (reacted && now >= bot.meleeNextTime && distanceSq < lungeSq && RollPercent(meleeChancePercent))
		{
			input.buttons |= cmdButtonMelee;
			input.meleeYaw = input.angles[1];
			input.meleeDist = static_cast<unsigned char>(distance < 255.0f ? distance : 255.0f);
			bot.meleeNextTime = now + IrandMs(meleeGapMinMs, meleeGapMaxMs) - bot.skillIndex * meleeGapSkillMs;
			BotLog("melee client %d swings at client %d at %.0f", clientNum, bot.targetClient, distance);
		}

		const bool walksSlow = (input.buttons & (cmdButtonAds | cmdButtonCrouch)) != 0;
		if (bot.task.kind == TaskCover && !IsHolding(clientNum))
		{
			if (walksSlow)
			{
				bot.wasSlow = true;
			}
			Idle(clientNum, self, input, false);
			return;
		}

		float closeRange = personality.closeRange;
		float farRange = personality.farRange;
		if (isQuickscoper)
		{
			closeRange = quickscopeCloseRange;
			farRange = quickscopeFarRange;
		}
		const float closeRangeSq = closeRange * closeRange;
		const float farRangeSq = farRange * farRange;
		if (distanceSq > farRangeSq)
		{
			bot.isWalkingFight = true;
		}
		else if (distanceSq < farRangeSq * walkingFightOffShare * walkingFightOffShare)
		{
			bot.isWalkingFight = false;
		}
		if (personality.isMeleeOnly && distanceSq < knifeChargeRangeSq && now >= bot.chargeRouteUntilTime)
		{
			bot.isWalkingFight = false;
		}
		if (tuning.fightMove != 0 && bot.isWalkingFight && !(isSniper && (wantAds || isQuickscoper))
			&& bot.pathLength > 0 && bot.pathIndex < bot.pathLength)
		{
			bot.dropUntilTime = 0;
			if (hasSight && !personality.isMeleeOnly && IsInRange(weapon, distanceSq)
				&& bot.noSprintUntilTime < now + walkingFightNoSprintMs)
			{
				bot.noSprintUntilTime = now + walkingFightNoSprintMs;
			}
			if (walksSlow)
			{
				bot.wasSlow = true;
			}
			Idle(clientNum, self, input, false);
			return;
		}

		const bool isProneLocked = bot.fightStyle == fightStyleProne && bot.dropUntilTime > now;
		if (bot.fightStyleTarget != bot.targetClient || (now >= bot.fightStyleUntilTime && !isProneLocked))
		{
			const bool isNewFight = bot.fightStyleTarget != bot.targetClient;
			const int previousStyle = bot.fightStyle;
			bot.fightStyleTarget = bot.targetClient;
			bot.fightStyleUntilTime = now + IrandMs(fightStyleMinMs, fightStyleMaxMs);
			const char* selfEntity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
			const int health = *reinterpret_cast<const int*>(selfEntity + gentityHealth);
			const bool suitsProne = !isSniper && weapon.weapClass != weapClassSpread && distanceSq > fightProneMinRangeSq;
			int strafeWeight = 40;
			int plantWeight = 10 + personality.caution / 4;
			int pushWeight = personality.aggression / 2;
			int proneWeight = 0;
			int hideWeight = personality.caution / 10;
			if (suitsProne && isNewFight)
			{
				proneWeight = (personality.tricks + personality.caution) / 60;
			}
			if (health < hideHealth)
			{
				hideWeight = personality.caution / 4;
			}
			if (isSniper && !isQuickscoper)
			{
				plantWeight += 40;
				pushWeight = 0;
			}
			if (personality.isMeleeOnly)
			{
				strafeWeight = 0;
				plantWeight = 0;
				proneWeight = 0;
				hideWeight = 0;
				pushWeight = 100;
			}
			int roll = static_cast<int>(NextRand() % static_cast<unsigned int>(
				strafeWeight + plantWeight + pushWeight + proneWeight + hideWeight + 1));
			bot.fightStyle = fightStyleStrafe;
			if (roll < plantWeight)
			{
				bot.fightStyle = fightStylePlant;
			}
			else if ((roll -= plantWeight) < pushWeight)
			{
				bot.fightStyle = fightStylePush;
			}
			else if ((roll -= pushWeight) < proneWeight)
			{
				bot.fightStyle = fightStyleProne;
			}
			else if ((roll -= proneWeight) < hideWeight)
			{
				bot.fightStyle = fightStyleHide;
			}
			if (bot.fightStyle == fightStyleProne && (isShellshocked || !CanSeeFromProne(clientNum, self, target)))
			{
				bot.fightStyle = fightStylePlant;
			}
			if (bot.fightStyle == fightStyleProne)
			{
				bot.dropUntilTime = now + dropHoldMs;
			}
			else if (previousStyle == fightStyleProne && !isNewFight)
			{
				bot.dropUntilTime = 0;
			}
			if (bot.fightStyle == fightStyleHide && tuning.cover && bot.task.kind != TaskCover
				&& now >= bot.coverCooldownTime && !isQuickscoper)
			{
				bot.coverCooldownTime = now + coverCooldownMs;
				if (RequestCover(clientNum, self, target.eye))
				{
					BotLog("fight client %d hides from client %d at %d hp", clientNum, bot.targetClient, health);
					Idle(clientNum, self, input, false);
					return;
				}
				bot.fightStyle = fightStylePlant;
			}
			static const char* const styleNames[] = { "strafe", "plant", "push", "prone", "hide" };
			if (isNewFight)
			{
				BotLog("fight client %d on client %d: %s", clientNum, bot.targetClient, styleNames[bot.fightStyle]);
			}
		}

		if (--bot.strafeCountdown <= 0)
		{
			bot.strafeCountdown = strafeFramesMin + static_cast<int>(NextRand() % 20u);
			int strafeChance = skill.strafeChance;
			if (strafeChance < strafeChanceFloor)
			{
				strafeChance = strafeChanceFloor;
			}
			const bool wantStrafe = RollPercent(strafeChance);
			if (!wantStrafe)
			{
				bot.strafeDirection = 0;
			}
			else if (bot.strafeDirection == 0)
			{
				bot.strafeDirection = (NextRand() & 1u) ? 1 : -1;
			}
			else if (RollPercent(strafeFlipPercent))
			{
				bot.strafeDirection = -bot.strafeDirection;
			}
			bot.strafeTarget = static_cast<int>(static_cast<float>(strafeAmount) * Flrand(strafeAmountMinShare, 1.0f));
			if (bot.fightStepFrames == 0 && RollPercent(fightStepPercent))
			{
				bot.fightStepFrames = IrandMs(fightStepMinFrames, fightStepMaxFrames);
				bot.fightStepForward = (NextRand() & 1u) ? fightStepAmount : static_cast<signed char>(-fightStepAmount);
			}

			if (bot.isBloomBlocked)
			{
				bot.strafeDirection = 0;
				if (bot.crouchEndTime <= now && weapon.stats.hip.duckedDecay > 1.0f && RollPercent(bloomCrouchPercent))
				{
					bot.crouchEndTime = now + IrandMs(crouchHoldMinMs, crouchHoldMaxMs);
				}
			}

			if (bot.crouchEndTime <= now
				&& RollPercent(ScaleByTrait(crouchChancePercent, personality.caution)))
			{
				bot.crouchEndTime = now + IrandMs(crouchHoldMinMs, crouchHoldMaxMs);
			}

			if (distanceSq < jumpshotRangeSq && bot.jumpshotFrames == 0
				&& bot.dropUntilTime <= now && !isShellshocked && !isSniper
				&& RollPercent(ScaleByTrait(bot.jumpshotPercent * tuning.dropshot / 100, personality.tricks)))
			{
				float hopVelocity[3];
				ReadVelocity(clientNum, hopVelocity);
				if (std::fabs(hopVelocity[2]) < 10.0f)
				{
					bot.jumpshotFrames = jumpshotFrameCount;
				}
			}
		}

		if (bot.crouchEndTime > now)
		{
			input.buttons |= cmdButtonCrouch;
		}

		if (reacted && !bot.stanceDecided && !isShellshocked)
		{
			bot.stanceDecided = true;
			float velocity[3];
			ReadVelocity(clientNum, velocity);
			const bool isGrounded = std::fabs(velocity[2]) < 10.0f;
			const bool suits = !isSniper && weapon.weapClass != weapClassSpread;
			const bool suitsJump = suits || (isQuickscoper && distanceSq < quickscopeJumpRangeSq);
			const int scale = isQuickscoper ? tuning.dropshot / 2 : tuning.dropshot;
			if (isGrounded && suits && distanceSq < dropshotRangeSq
				&& RollPercent(ScaleByTrait(bot.dropshotPercent * scale / 100, personality.tricks))
				&& CanSeeFromProne(clientNum, self, target))
			{
				bot.dropUntilTime = now + dropHoldMs;
				bot.fireHoldTime = now + IrandMs(dropShotBeatMinMs, dropShotBeatMaxMs);
				BotLog("dropshot client %d on client %d at %.0f", clientNum, bot.targetClient,
					std::sqrt(distanceSq));
			}
			else if (isGrounded && suitsJump && distanceSq < jumpshotRangeSq
					 && RollPercent(ScaleByTrait(bot.jumpshotPercent * scale / 100, personality.tricks)))
			{
				bot.jumpshotFrames = jumpshotFrameCount;
				bot.fireHoldTime = now + IrandMs(jumpShotBeatMinMs, jumpShotBeatMaxMs);
				BotLog("jumpshot client %d on client %d at %.0f", clientNum, bot.targetClient,
					std::sqrt(distanceSq));
			}
		}

		if (bot.dropUntilTime > now)
		{
			const float* actualView = reinterpret_cast<const float*>(playerState + psViewAngles);
			if (std::fabs(AngleDelta(actualView[1], yaw)) > proneYawLimitDeg)
			{
				bot.dropUntilTime = 0;
			}
			else
			{
				bot.dropUntilTime = now + dropHoldMs;
				input.buttons |= cmdButtonProne;
				input.buttons &= ~cmdButtonCrouch;
			}
		}
		if (bot.jumpshotFrames > 0)
		{
			--bot.jumpshotFrames;
			if (!isShellshocked)
			{
				input.buttons |= cmdButtonUp;
			}
		}

		if (distanceSq > farRangeSq)
		{
			bot.isClosingIn = true;
		}
		else if (distanceSq < farRangeSq * closeInOffShare * closeInOffShare)
		{
			bot.isClosingIn = false;
		}
		if (distanceSq < closeRangeSq)
		{
			bot.isBackingOff = true;
		}
		else if (distanceSq > closeRangeSq * backOffOffShare * backOffOffShare)
		{
			bot.isBackingOff = false;
		}
		signed char forward = 0;
		if (bot.isClosingIn)
		{
			forward = 96;
			if (!personality.isMeleeOnly && !isSniper && (!hasSight || !IsInRange(weapon, distanceSq))
				&& personality.aggression >= rushSprintAggressionMin && !isShellshocked)
			{
				forward = 127;
				input.buttons |= cmdButtonSprint;
			}
			if (personality.isMeleeOnly)
			{
				float chargeVelocity[3];
				ReadVelocity(clientNum, chargeVelocity);
				if (chargeVelocity[0] * chargeVelocity[0] + chargeVelocity[1] * chargeVelocity[1] < strafeMinSpeedSq)
				{
					if (++bot.chargeStallFrames >= chargeStallLimit)
					{
						bot.chargeStallFrames = 0;
						bot.chargeRouteUntilTime = now + chargeRouteMs;
						BotLog("charge client %d is stuck on the way to client %d, takes the route", clientNum, bot.targetClient);
					}
				}
				else
				{
					bot.chargeStallFrames = 0;
				}
				if (distanceSq > knifeSprintRangeSq)
				{
					forward = 127;
					input.buttons |= cmdButtonSprint;
				}
			}
		}
		else if (bot.isBackingOff)
		{
			forward = -96;
		}
		else if (bot.fightStepFrames > 0)
		{
			--bot.fightStepFrames;
			forward = bot.fightStepForward;
		}
		if (!bot.isClosingIn && (bot.fightStyle == fightStylePlant || bot.fightStyle == fightStyleProne))
		{
			forward = 0;
			bot.strafeDirection = 0;
		}
		if (bot.fightStyle == fightStylePush && forward <= 0 && !personality.isMeleeOnly)
		{
			forward = 80;
		}
		const int footworkSign = forward > 0 ? 1 : (forward < 0 ? -1 : 0);
		if (footworkSign != 0 && footworkSign == -bot.lastFootworkSign && now - bot.footworkLogTime > 1000)
		{
			bot.footworkLogTime = now;
			BotLog("footwork client %d turns %s at %.0f units (close %.0f far %.0f, step %d)", clientNum,
				footworkSign > 0 ? "in" : "back", distance, closeRange, farRange, bot.fightStepFrames);
		}
		if (footworkSign != 0)
		{
			bot.lastFootworkSign = footworkSign;
		}

		if (isSniper && !personality.isQuickscoper && wantAds)
		{
			input.forward = 0;
			input.right = 0;
			return;
		}

		if (bot.strafeDirection != 0)
		{
			float vel[3];
			ReadVelocity(clientNum, vel);
			const float speedSq = vel[0] * vel[0] + vel[1] * vel[1];
			if (speedSq <= strafeMinSpeedSq)
			{
				if (++bot.strafeStallFrames > strafeStallLimit)
				{
					bot.strafeStallFrames = 0;
					FlipStrafe(bot, now);
				}
			}
			else
			{
				bot.strafeStallFrames = 0;
				const float scale = strafeProbe / std::sqrt(speedSq);
				const float probe[3] = { self.eye[0] + vel[0] * scale, self.eye[1] + vel[1] * scale, self.eye[2] };
				const bool wallAhead = !SightLine(self.eye, probe, clientNum);

				const float floorTop[3] = { probe[0], probe[1], self.feetZ + 20.0f };
				const float floorBottom[3] = { probe[0], probe[1], self.feetZ - ledgeDropUnits };
				const bool ledgeAhead = SightLine(floorTop, floorBottom, clientNum);

				if (wallAhead || ledgeAhead)
				{
					FlipStrafe(bot, now);
				}
			}
		}

		int wantRight = bot.strafeDirection * (bot.strafeTarget > 0 ? bot.strafeTarget : strafeAmount);
		if (isQuickscoper && adsAmount > 0.5f)
		{
			wantRight /= scopedStrafeDivisor;
		}
		const int rampStep = strafeAmount / strafeRampFrames;
		if (bot.strafeRight < wantRight)
		{
			bot.strafeRight += rampStep;
			if (bot.strafeRight > wantRight)
			{
				bot.strafeRight = wantRight;
			}
		}
		else if (bot.strafeRight > wantRight)
		{
			bot.strafeRight -= rampStep;
			if (bot.strafeRight < wantRight)
			{
				bot.strafeRight = wantRight;
			}
		}

		input.forward = forward;
		input.right = static_cast<signed char>(bot.strafeRight);
		if ((input.buttons & cmdButtonSprint) != 0)
		{
			input.right = 0;
		}
		bot.footworkForward = input.forward;
		bot.footworkRight = input.right;
		bot.footworkUntilTime = now + lostFootworkHoldMs;
	}
}
