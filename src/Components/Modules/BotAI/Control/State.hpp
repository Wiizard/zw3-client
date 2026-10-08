#pragma once

#include "Components/Modules/BotAI/Iw4.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Control/Identity.hpp"
#include "Components/Modules/BotAI/Control/Skill.hpp"
#include "Components/Modules/BotAI/Control/Personality.hpp"
#include "Components/Modules/BotAI/Control/WeaponData.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"

namespace Components::BotAI
{
	static constexpr float radToDeg     = 57.295776f;

	static constexpr int   hurtWindowMs     = 1000;
	static constexpr int   flinchMs         = 500;

	static constexpr float reactNearUnits   = 250.0f;
	static constexpr float reactFarUnits    = 2000.0f;
	static constexpr float reactNearScale   = 0.5f;
	static constexpr float reactFarScale    = 1.5f;
	static constexpr float surpriseRangeSq  = 300.0f * 300.0f;
	static constexpr float surpriseScale    = 2.0f;
	static constexpr int   aimBodySkillMax  = 2;
	static constexpr float shinRise         = 12.0f;

	static constexpr float swapRangeSq      = 400.0f * 400.0f;
	static constexpr int   swapBackMs       = 2500;
	static constexpr int   swapGiveUpMs     = 2000;
	static constexpr int   lowHealth        = 35;
	static constexpr float coverSearchSq    = 900.0f * 900.0f;
	static constexpr int   coverCandidates  = 12;
	static constexpr float coverEyeRise     = 60.0f;
	static constexpr int   coverHoldMs      = 3000;
	static constexpr int   sniperCoverHoldMs = 9000;
	static constexpr int   coverCooldownMs  = 8000;

	static constexpr float coverMinMoveUnits = 150.0f;
	static constexpr int   topUpRounds      = 18;

	static constexpr int   postKillMinMs    = 250;
	static constexpr int   postKillMaxMs    = 500;
	static constexpr float postKillSweepDeg = 3.0f;
	static constexpr int   revengeHuntMinMs  = 14000;
	static constexpr int   revengeHuntSpanMs = 12000;
	static constexpr int   helpHuntMs       = 15000;

	static constexpr int   alertMs            = 4000;
	static constexpr int   noSprintAfterMs    = 700;

	static constexpr int   maxPathNodes    = 320;

	static constexpr float navTurnFraction = 0.2f;
	static constexpr float casualTurnMaxDeg = 12.0f;
	static constexpr float turnEaseStartDeg = 8.0f;
	static constexpr float turnEaseMin = 0.4f;
	static constexpr float turnEaseStep = 0.3f;
	static constexpr float turnEaseDecay = 0.1f;
	static constexpr float turnMinDeg      = 0.75f;

	static constexpr float offGraphDistSq       = 200.0f * 200.0f;
	static constexpr int   huntPauseMs          = 8000;
	static constexpr int   huntRetargetMinMs    = 20000;

	static constexpr float followStandoffUnits  = 220.0f;
	static constexpr float followResumeUnits    = 360.0f;
	static constexpr float followCalmUnits      = 500.0f;

	static constexpr int   worldHurtRepeats     = 3;
	static constexpr int   worldHurtGapMs       = 1600;
	static constexpr int   worldHurtMaxDamage   = 60;

	static constexpr float huntMaxUnits         = 3000.0f;
	static constexpr int   huntShotMemoryMs     = 8000;
	static constexpr int   calloutDelayMinMs    = 1500;
	static constexpr int   calloutDelayMaxMs    = 3000;
	static constexpr float calloutFuzzMinUnits  = 100.0f;
	static constexpr float calloutFuzzMaxUnits  = 300.0f;
	static constexpr int   intelMaxAgeMs        = 20000;
	static constexpr float radarFuzzUnits       = 150.0f;
	static constexpr float huntDriftMaxUnits    = 500.0f;
	static constexpr int   huntDriftFullMs      = 20000;
	static constexpr float huntDryRangeSq       = 300.0f * 300.0f;
	static constexpr int   huntDryMinAgeMs      = 1500;
	static constexpr int   contactMemoryMs      = 20000;
	static constexpr float contactRangeSq       = 1200.0f * 1200.0f;

	static constexpr float heatVisit            = 60.0f;
	static constexpr float heatPlan             = 30.0f;
	static constexpr float heatDecayPerSecond   = 0.93f;
	static constexpr int   huntGoalChoices      = 5;
	static constexpr float huntGoalSpreadSq     = 250.0f * 250.0f;

	static constexpr float sprayRangeSq     = 300.0f * 300.0f;
	static constexpr float burstFarRangeSq  = 800.0f * 800.0f;
	static constexpr int   burstFarMinMs    = 150;
	static constexpr int   burstFarMaxMs    = 300;
	static constexpr int   burstMidMinMs    = 300;
	static constexpr int   burstMidMaxMs    = 600;
	static constexpr int   burstGapMinMs    = 200;
	static constexpr int   burstGapMaxMs    = 450;
	static constexpr int   confirmReactionMsBySkill[7] = { 420, 380, 340, 300, 260, 220, 190 };
	static constexpr int   confirmJitterMs = 120;
	static constexpr int   sniperRescopeMs  = 2000;
	static constexpr float adsFovMulti      = 0.3f;
	static constexpr int   avoidNodeSlots = 4;
	static constexpr int   turnNoteSlots = 4;
	static constexpr int   avoidNodeMs = 20000;
	static constexpr float noticeWidthScale = 1.0f;
	static constexpr float fenceSeeThroughSq = 500.0f * 500.0f;
	static constexpr float noticeHeightShare = 0.62f;
	static constexpr float noticeRunnerScale = 1.5f;
	static constexpr float noticeRunnerSpeedSq = 150.0f * 150.0f;
	static constexpr float noticeCrouchScale = 0.6f;
	static constexpr float noticeProneScale = 0.4f;
	static constexpr float crouchViewHeightMax = 50.0f;
	static constexpr float proneViewHeightMax = 25.0f;
	static constexpr int   adsHoldMs        = 1000;

	static constexpr float dropshotRangeSq  = 700.0f * 700.0f;
	static constexpr float jumpshotRangeSq  = 500.0f * 500.0f;
	static constexpr int   dropHoldMs       = 2500;
	static constexpr int   jumpshotFrameCount   = 3;
	static constexpr int   strafeAmount     = 110;

	static constexpr int   pitchHoldMinMs   = 500;
	static constexpr int   pitchHoldMaxMs   = 1000;
	static constexpr int   crouchChancePercent  = 4;
	static constexpr int   crouchHoldMinMs  = 700;
	static constexpr int   crouchHoldMaxMs  = 1600;
	static constexpr float meleeRangeSq     = 64.0f * 64.0f;
	static constexpr int   meleeChancePercent   = 40;
	static constexpr float strafeProbe      = 26.0f;
	static constexpr float strafeMinSpeedSq = 400.0f;
	static constexpr int   reloadChancePercent  = 20;

	static constexpr float gravityUnits            = 800.0f;
	static constexpr float grenadeLiftMinDeg  = 3.0f;
	static constexpr float grenadeLiftMaxDeg  = 8.0f;
	static constexpr float grenadeSightUnits  = 384.0f;
	static constexpr int   grenadeBlockedMs   = 2000;
	static constexpr int   throwBackPercent       = 90;
	static constexpr int   throwBackDelayMinMs    = 300;
	static constexpr int   throwBackDelayMaxMs    = 800;

	struct BotInput
	{
		bool isActive;
		int buttons;
		signed char forward;
		signed char right;
		unsigned short weapon;
		unsigned short altWeapon;
		unsigned short offhandWeapon;
		float angles[3];
		float meleeYaw;
		unsigned char meleeDist;
		signed char selectedLocation[3];
		signed char remoteAngles[2];
		int serverTimeBias;
	};

	enum TaskKind
	{
		TaskNone = 0,
		TaskRoam,
		TaskHunt,
		TaskPush,
		TaskCover,
		TaskHold,
		TaskFollow,
		TaskCapture,
		TaskDefend,
		TaskKindCount,
	};

	enum TaskPriority
	{
		PriorityNone  = 0,
		PriorityPlan  = 1,
		PriorityHear  = 2,
		PriorityHelp  = 3,
		PriorityLost  = 4,
		PriorityCover = 5,
	};

	struct Task
	{
		int kind;
		int priority;
		int client = -1;
		int node = -1;
		int objective = -1;
		float point[3];
		float lookYaw;
		int until;
		int stage;
		int serial;
		const char* source;
	};

	static constexpr int taskQueue = 4;
	static constexpr int searchSpots = 3;

	struct BotIdentity
	{
		int ping;
		int pingBase;
		int pingNextTick;
		int ordinal;
		Identity identity;
		unsigned short lastHeldWeapon;
		unsigned short lastAskedWeapon;
		bool knifeForced;

		int skillIndex;
		int baseSkillIndex;
		int skillSettingSeen;
		BotSkill skillRow;
		float reactionScale;
		float aimTimeScale;
		float aimErrorScale;
		int killsAtLastDeath;
		bool isTraced;
		float traceLastYaw;
		float traceLastPitch;
		int deathsWithoutKill;
		int dropshotPercent;
		int jumpshotPercent;
		Personality personality;
		Loadout loadout;
		int personalitySettingSeen;

		int lastAttacker = -1;
		bool hasDeathPos;
		float deathPos[3];

		int helpCooldownTime;
		int coverCooldownTime;
		int grenadeCooldownTime;
		int tacticalCooldownTime;
		int taskSerial;
		int leaveAtTime;

		bool isFillBot;
		bool isZw3Bot;
		unsigned short remotePort;
		int joinedAtTime;

		int spawnStage;
		int stageCountdown;
		int unassignedFrames;
		bool hasSpawnedOnce;
		bool wasAlive;
	};

	struct BotLife
	{
		BotInput input;

		short path[maxPathNodes];
		int pathLength;
		int pathIndex;
		float lastOrigin[2];
		int stuckFrames;
		int strafeDirection;
		int strafeCountdown;
		float approachBestSq;
		int approachStall;
		int avoidNodes[avoidNodeSlots];
		int avoidNodeTimes[avoidNodeSlots];
		int avoidNodeNext;
		float lastStuckPos[2];
		int lastStuckTime;
		int stuckRepeat;
		bool wasWalking;
		bool wasProne;
		int stanceGraceUntilTick;
		int repathNotBeforeTick;
		int worldHurtCount;
		int worldHurtTime;

		bool isOffGraph;
		int offGraphNode;
		int offGraphRecheckTick;
		int steerSide;
		int steerFrames;
		int jumpPulseUntilTick;
		int knifeFaceUntilTick;
		float knifeYaw;
		float knifePitch;
		int knifePiece = -1;
		float pursuitPoint[3];
		int pursuitUntilTime;
		bool isRoamer;
		int pathTaskSerial;
		int pathTaskKind;
		int huntReplanTime;
		int huntPauseUntilTime;
		float followMatePos[3];
		int goalNode = -1;
		int glanceNextTime;
		int glanceUntilTime;
		float glanceYawDeg;

		Task task;
		Task interrupted;
		Task queue[taskQueue];
		int queueCount;
		bool lifePlanned;
		int taskStartTime;
		int taskPickNotBeforeTime;
		int holdSweepStartTime;
		int holdGlanceUntilTime;
		float holdGlanceYawDeg;
		int holdGlanceNextTime;
		bool holdCrouched;
		int grenadesThrown;
		int tacticalsThrown;

		const char* aiMode;
		const char* navMode;
		int lastAssistLogNode = -1;
		int scanEnemies;
		int scanInCone;
		int scanVisible;
		int scanRotation;

		int targetClient = -1;
		int traceTime;
		int noTraceTime;
		int fireHoldTime;
		bool wasFrozen;
		float lastSeenPos[3];
		float lastSeenFeetZ;
		float aimOffsetBase[3];
		int semiCooldown;
		int lastFireTime;
		int trackSampleTime;
		float trackSamplePos[3];
		int wideGateUntilTime;
		bool hasOpenedFire;
		int aimBreakNextTime;
		int aimBreakUntilTime;
		int flickUntilTime;
		float reactionJitter = 1.0f;
		int reloadCancelAtTime;
		float turnEase;
		float listenPitch;
		int sentButtons;
		int adsChangeTime;
		float lastCmdYaw;
		int crouchChangeTime;
		int proneChangeTime;
		int throwOverrunFrames;
		int throwAimWaitFrames;
		int stopgapGoal = -1;
		int confirmUntilTime;
		int flinchDamage;
		int lastFlinchTime;
		int adsFlipTimes[3];
		int adsFlipNext;
		int adsFlipLogTime;
		bool isInNarrowGap;
		const char* turnTags[turnNoteSlots];
		float turnYaws[turnNoteSlots];
		int turnCount;
		float gapCorrectionDeg;
		int pendingHelpTime;
		int pendingHelpAttacker;
		int pendingHelpFrom;
		float pendingHelpPoint[3];
		float flickOverYaw;
		float flickOverPitch;
		int meleeNextTime;
		int strafeTarget;
		int fightStepFrames;
		signed char fightStepForward;
		float kickComp[2];
		bool flinchPending;
		bool grenadeAtSeen;
		bool panicRolled;
		int panicUntilTime;
		float huntKnownPos[3];
		int huntKnownTime;
		int huntKnownClient = -1;
		float huntDriftYaw;
		int pauseUntilTime;
		int pauseNextTime;
		int cornerSlowUntilTick;
		int cornerPathIndex = -1;
		float walkScale;
		int walkScaleNextTime;
		int arriveLookUntilTime;
		int arriveLookStartTime;
		float arriveLookYaw;
		bool wasAlert;
		float lanePenaltyScale;
		int tubeUntilTime;
		int tubeCooldownTime;
		int tubeStartTime;
		int omaStage;
		int omaAnswerTime;
		int omaGiveUpTime;
		int omaDoneTime;
		int omaNextTime;
		int nextThrowTime;
		int hipCloseTarget = -1;
		int fightStyle;
		int yieldUntilTime;
		float followLookYaw;
		int followLookNextTime;
		int threatFaceClient = -1;
		bool facesThreat;
		float ladderFacingYaw;
		int ladderFacingTime;
		float ladderBestZ;
		int ladderStallFrames;
		int ladderShuffleSide;
		int ladderShuffleUntilTime;
		float yieldYaw;
		bool isInHall;
		int fightStyleTarget = -1;
		int fightStyleUntilTime;
		int lostHoldMs;
		bool prefersHipClose;
		int footworkUntilTime;
		signed char footworkForward;
		signed char footworkRight;
		int chargeStallFrames;
		int chargeRouteUntilTime;
		float aimDrop;
		int pitchEndTime;
		int crouchEndTime;
		int grenadeThrowFrames;
		float grenadeYaw;
		float grenadePitch;
		int lastThrowBackOwner = -1;
		int throwBackRollOwner = -1;
		bool isThrowingBack;
		int throwBackPressTime;
		int prevClipRounds;
		int reloadGuardUntilTime;
		int prevClipWeapon;
		int kickLoggedWeapon;
		float kickAngle[2];
		float kickVel[2];
		float kickAdsCenter;
		float kickHipCenter;
		bool isKickLifted;
		int deadSince;
		int skipCamNextTime;
		int skipCamTapFrames;
		int skipCamTaps;
		bool isBloomBlocked;
		int yyPeriodFrames;
		float walkPitchDeg;
		int walkPitchNextTime;
		int tacticalThrowFrames;
		int strafeRight;
		int strafeStallFrames;
		int strafeFlipTime;
		int smoothNode = -1;
		int smoothRecheckTick;
		int lookNode = -1;
		int lookRecheckTick;
		int yyFrames;
		int yyNextTime;
		unsigned short yyWeapon;
		unsigned short yySidearm;

		int lastDamageEvent;
		bool hasDamageBaseline;
		int hurtUntilTime;
		bool hurtScanPending;
		bool hasHurtBearing;
		float hurtBearingYaw;
		int hurtLookUntilTime;
		float listenYaw;
		int listenUntilTime;
		int listenNextTime;
		int flinchUntilTime;
		int hearCooldownTime;
		int switchNotBeforeTime;
		int shooterSwitchNotBeforeTime;

		int alertUntilTime;
		int alertSightedUntilTime;
		int alertCheckTick;
		int lostSpotOccludedSince;
		bool lostSpotDropped;
		int wallbangUntilTime;
		int wallbangTarget = -1;
		bool hasWallbangRoll;
		int flankSerial = -1;
		bool isFlanking;
		int searchAreas[searchSpots];
		int searchCount;
		int searchIndex;
		int searchSerial = -1;
		int searchUntilTime;
		bool hasRevealPoint;
		float revealPoint[3];
		int heardClient = -1;
		int heardTime;
		float heardBangPoint[3];
		int heardBangUntilTime;
		int heardBangNextTime;
		int settleFrames;
		bool wantsStillShot;
		bool isFollowWaiting;
		bool isNearFollowMate;
		signed char fightMoveForward;
		signed char fightMoveRight;
		int escapeUntilTime;
		float escapeYaw;
		bool escapeJumps;
		int holdKills;
		int holdKillsNode = -1;
		float relocateFrom[3];
		bool hasRelocateFrom;
		bool isSneaking;
		bool hasSneakRoll;
		int coastUntilTick;
		float lastTravelYaw;
		int sneakCheckTime;
		int noSprintUntilTime;
		int sprintWindowEndTime;
		bool wantSprint;
		bool wasSlow;

		bool stanceDecided;
		int dropUntilTime;
		int jumpshotFrames;

		int burstEndTime;
		bool isBurstFiring;
		int lingerUntilTime;
		int adsHoldUntilTime;
		int scopeUpTime;
		bool isClosingIn;
		bool isBackingOff;
		int lastFootworkSign;
		int footworkLogTime;
		int ladderJumpTime;
		int scopeDownUntilTime;
		int scopeMisses;
		bool sightAtFeet;

		unsigned short swapWeapon;
		bool swapIsSidearm;
		int swapGiveUpTime;
		int swapBackTime;

		int postKillUntilTime;
		bool isWalkingFight;
		float sweepSign;
		bool deathRecorded;

		int ksPhase;
		int ksTimer;
		int ksCooldown;
		int ksCategory;
		int ksRemoteBaseline;
		int ksBoostFrames;
		unsigned short ksWeapon;
	};

	struct BotState : BotIdentity, BotLife
	{
	};

	static constexpr int defaultTimeBias = -200;

	static constexpr int spawnFrameGap = 10;

	static constexpr int burstFrameCount = 200;

	static constexpr int menuStepFrames = 20;
	static constexpr int stageTeam  = 1;
	static constexpr int stageClass = 2;

	static constexpr int unassignedRetryFrames = 100;
	static constexpr int lostTeamRetryFrames = 200;
	static constexpr int gametypeWaitFrames = 60;

	struct PlayerView
	{
		bool isPlaying;
		int team;
		float eye[3];
		float feetZ;
		bool isOnLadder;
		bool isMantling;
	};

	static constexpr float maxShotgunDistance = 500.0f;

	struct WeaponInfo
	{
		int index;
		int weapClass;
		bool isFullAuto;
		bool isAkimbo;
		bool hasAmmo;
		int clipRounds;
		float distMulti;
		float hipRange;
		bool isSilenced;
		bool isBurst;
		bool hasStats;
		GunStats stats;
	};

	struct Tuning
	{
		int skill;
		int freeze;
		int pingMin;
		int pingMax;
		int botClass;
		int sniperLobby;
		int type;
		int roam;
		int maxFollow;
		int huntReplan;
		int push;
		int glance;
		int hear;
		int adsWalk;
		int burst;
		int prefire;
		int dropshot;
		int swap;
		int cover;
		int revenge;
		int help;
		int smooth;
		int tactical;
		int yy;
		int kick;
		int ignoreHumans;
		int forceTarget;
		int trace;
		int fightMove;
		int spread;
		int churn;
		int reserve;
		int settings;
	};

	extern BotState bots[maxClients];
	extern Tuning tuning;
	extern bool debugOn;
	extern int debugTick;
	extern bool pathBuiltThisFrame;

	inline int ServerTimeMs()
	{
		return *reinterpret_cast<int*>(svs_time);
	}

	inline const char* PlayerStateOf(int clientNum)
	{
		return SV_GetPlayerstateForClientNum(clientNum);
	}

	inline const char* WeaponDefOf(int index)
	{
		return BG_GetWeaponDef(index);
	}

	inline const char* WeaponNameOf(int index)
	{
		return BG_GetWeaponName(index);
	}

	extern int sightLineCalls;
	extern int pathFinds;

	inline bool SightLine(const float* from, const float* to, int passEntNum)
	{
		++sightLineCalls;
		if (G_LocationalTracePassed(from, to, passEntNum, entityNumNone, maskSightThroughGlass, nullptr) == 0)
		{
			return false;
		}
		if (Navgen::IsRenderBlocked(from, to))
		{
			return false;
		}
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float dz = to[2] - from[2];
		if (dx * dx + dy * dy + dz * dz < fenceSeeThroughSq)
		{
			return true;
		}
		++sightLineCalls;
		return G_LocationalTracePassed(from, to, passEntNum, entityNumNone, contentsMissileClip, nullptr) != 0;
	}

	template <typename Predicate>
	int FindHeldWeapon(const char* playerState, Predicate matches)
	{
		const int* held = reinterpret_cast<const int*>(playerState + psHeldWeapons);
		for (int i = 0; i < 15; ++i)
		{
			if (held[i] && matches(held[i]))
			{
				return held[i];
			}
		}
		return 0;
	}

	float Flrand(float low, float high);
	int IrandMs(int low, int high);
	int JitterMs(int ms);
	bool RollPercent(int percent);

	int ChooseSkill();
	bool IsSkillFixed();
	int SkillSetting();
	void RollSkill(int clientNum);
	bool IsSkillAdaptive();
	void ApplyFightSkill(int clientNum, int targetClient);
	unsigned short FirstHeldRealWeapon(const char* playerState);
	void SendMenuResponse(void* entity, const char* response, const char* menu);
	unsigned short FindSecondGun(const char* playerState, unsigned short first);
	void AppendBotLog(const char* line);
	void BotLog(const char* format, ...);
	void RescueBot(int clientNum);
	void DriveBot(int clientNum, bool wantFrozen);

	void DropPath(BotState& bot);
	void ClearRouteHeat();
	void StartOpening(int now);
	void ForgetTeamKnowledge();
	void ForgetGunfire();
	void ResetOpening();
	bool IsOpening(int now);

	void LoadMapMemory(const char* mapName);
	void TrackMapMemory();
}
