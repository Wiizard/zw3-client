#pragma once

namespace Components::BotAI
{
	struct BotSkill
	{
		float aimTime;
		int reactionTime;
		int noTraceLookTime;
		int noTraceAdsTime;
		int rememberTime;
		float fov;
		float distStart;
		float distMax;
		float aimOffsetTime;
		float aimOffsetAmount;
		float semiTime;
		int strafeChance;
		int shootAfterMs;
	};


	static const BotSkill skills[] = {
		{ 0.60f, 1000,  600,  500,  750, 0.940f,  1000.0f,  2500.0f, 1.50f, 4.0f, 0.90f,  0, 1000 },
		{ 0.55f,  800, 1250, 1000, 1500, 0.930f,  1500.0f,  3000.0f, 1.00f, 3.0f, 0.75f, 10,  750 },
		{ 0.40f,  500, 1500, 1000, 2000, 0.920f,  2250.0f,  4000.0f, 0.75f, 2.5f, 0.65f, 20,  650 },
		{ 0.30f,  400, 2000, 1500, 3000, 0.906f,  3350.0f,  5000.0f, 0.50f, 2.0f, 0.50f, 30,  500 },
		{ 0.25f,  300, 3000, 2500, 4000, 0.891f,  5000.0f,  7500.0f, 0.35f, 1.5f, 0.40f, 40,  350 },
		{ 0.20f,  150, 4000, 2500, 5000, 0.879f,  7500.0f, 10000.0f, 0.25f, 1.0f, 0.25f, 50,  250 },
		{ 0.10f,   50, 4000, 2500, 7500, 0.866f, 10000.0f, 15000.0f, 0.00f, 0.0f, 0.10f, 65,    0 },
	};

	static constexpr int skillRandom = 9;
	static constexpr int skillAdaptive = 8;
	static constexpr int skillAdaptiveHard = 10;
	static constexpr int skillCount = sizeof(skills) / sizeof(skills[0]);


	static constexpr int dropshotPercentBySkill[7] = { 0, 0, 3, 5, 6, 8, 10 };
	static constexpr int jumpshotPercentBySkill[7] = { 0, 0, 2, 3, 3, 4, 5 };

	static constexpr float turnRateDegBySkill[7] = { 7.0f, 8.0f, 10.0f, 13.0f, 17.0f, 22.0f, 30.0f };
	static constexpr float wobbleDegBySkill[7]  = { 0.9f, 0.75f, 0.6f, 0.45f, 0.35f, 0.25f, 0.12f };
	static constexpr float kickControlBySkill[7] = { 0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f };
	static constexpr int   kickControlLagMsBySkill[7] = { 320, 290, 260, 230, 200, 180, 160 };
	static constexpr float kickYawControlShare = 0.25f;
	static constexpr int   closeHipPercentBySkill[7] = { 0, 5, 10, 20, 35, 50, 60 };
	static constexpr int   threatGlancePercentBySkill[7] = { 5, 10, 20, 30, 40, 50, 60 };
	static constexpr int   huntFacePercentBySkill[7] = { 0, 0, 0, 5, 15, 25, 35 };
	static constexpr int   lostHoldMinMsBySkill[7] = { 600, 700, 800, 900, 1000, 1100, 1200 };
	static constexpr float grenadeTurnBySkill[7] = { 0.15f, 0.18f, 0.21f, 0.25f, 0.3f, 0.35f, 0.4f };

	static constexpr int   trackLagMsBySkill[7] = { 180, 150, 130, 110, 90, 60, 30 };
	static constexpr float traceBleedBySkill[7] = { 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.35f };
	static constexpr int   wideBurstMsBySkill[7] = { 450, 380, 320, 260, 200, 140, 80 };
	static constexpr float prefireShareBySkill[7] = { 0.0f, 0.0f, 0.4f, 0.7f, 1.0f, 1.2f, 1.4f };
	static constexpr float hearScaleBySkill[7] = { 0.6f, 0.75f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f };
	static constexpr int   disciplineBySkill[7] = { 10, 20, 35, 50, 65, 80, 95 };
	static constexpr int   errorBreakMsBySkill[7] = { 1200, 1500, 1800, 2200, 2600, 3200, 4500 };
	static constexpr float flickOverShareBySkill[7] = { 0.14f, 0.12f, 0.10f, 0.08f, 0.06f, 0.05f, 0.03f };
	static constexpr float semiFingerBySkill[7] = { 2.2f, 1.9f, 1.6f, 1.4f, 1.25f, 1.1f, 1.0f };
	static constexpr int   strafeChanceFloor = 15;
	static constexpr float aimErrorDegBySkill[7] = { 4.0f, 3.2f, 2.5f, 1.9f, 1.4f, 1.0f, 0.6f };
	static constexpr int   aimSettleMsBySkill[7] = { 1500, 1100, 850, 650, 500, 380, 280 };
	static constexpr float slipDegBySkill[7] = { 1.5f, 1.3f, 1.1f, 0.9f, 0.8f, 0.6f, 0.5f };
	static constexpr float driftTurnShareBySkill[7] = { 0.15f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.6f };
	static constexpr float acquireRampFloorBySkill[7] = { 0.3f, 0.35f, 0.4f, 0.45f, 0.55f, 0.65f, 0.75f };
	static constexpr float acquireRampMsBySkill[7] = { 400.0f, 350.0f, 300.0f, 250.0f, 180.0f, 130.0f, 90.0f };
	static constexpr float noticeEdgeScaleBySkill[7] = { 0.25f, 0.3f, 0.35f, 0.4f, 0.5f, 0.6f, 0.7f };
	static constexpr float skillJitterMin = 0.85f;
	static constexpr float skillJitterMax = 1.2f;
	static constexpr float reactionJitterMin = 0.75f;
	static constexpr float reactionJitterMax = 1.5f;
	static constexpr int   reactionFloorNearMs = 170;
	static constexpr int   reactionFloorFarMs = 220;
	static constexpr float flinchPitchMinDeg = 0.6f;
	static constexpr float flinchPitchMaxDeg = 3.0f;
	static constexpr float flinchYawMaxDeg = 1.5f;
	static constexpr float flinchDegPerDamage = 0.08f;
	static constexpr int   flinchGapMs = 150;
	static constexpr float prefireReadyMin = 0.3f;
	static constexpr int   aimBreakMs = 300;
	static constexpr int   flickMs = 200;
	static constexpr float slowTurnStartDeg = 100.0f;
	static constexpr float slowTurnMinShare = 0.25f;
	static constexpr float adaptiveKdTarget = 1.0f;
	static constexpr float adaptiveHardKdTarget = 0.75f;
	static constexpr int   adaptiveShiftMax = 3;
}
