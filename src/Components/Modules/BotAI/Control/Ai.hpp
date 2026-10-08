#pragma once

#include "Components/Modules/BotAI/Control/State.hpp"
#include "Components/Modules/BotAI/Control/Tasks.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"

namespace Components::BotAI
{
	bool IsFiniteVec(const float* value);
	PlayerView ReadPlayerView(int clientNum);
	bool IsEnemy(const PlayerView& self, const PlayerView& other);
	bool IsHuman(int clientNum);
	void FeetOf(const PlayerView& view, float out[3]);
	float AngleDelta(float from, float to);
	void NoteTurn(BotState& bot, const char* tag, float yaw);
	void RemoveViewKick(int clientNum, BotInput& input);
	void ApplyViewKick(int clientNum, BotInput& input);
	float TurnStep(float delta, float share, float rateDeg);
	void TurnView(int clientNum, BotInput& input, float yaw, float pitch, const char* tag);
	void MoveTowards(float travelYaw, float viewYaw, BotInput& input);
	void KeepMoveHeading(BotInput& input, float yawStepDeg);
	float ConeDot(const float* from, const float* to, float viewYaw, float viewPitch);
	bool HasSubstring(const char* text, const char* needle);
	float ViewYawOf(int clientNum);
	bool IsShellshocked(int clientNum, int now);
	float DistanceSq2D(const float* a, const float* b);

	WeaponInfo ReadWeapon(const char* playerState);
	bool IsInRange(const WeaponInfo& weapon, float distanceSq);
	bool CanAds(const WeaponInfo& weapon, float distanceSq, float hipScale = 1.0f);
	bool HasLethalOffhand(const char* playerState);
	bool HasTacticalOffhand(const char* playerState);
	int FindLethalOffhand(const char* playerState);
	int FindTacticalOffhand(const char* playerState);
	unsigned short FindSidearm(const char* playerState);
	unsigned short FindShotgun(const char* playerState);
	int ClipRoundsOf(const char* playerState, int weaponIndex);
	int StockRoundsOf(const char* playerState, int weaponIndex);
	bool IsDryGun(const char* playerState, int weaponIndex);

	int SightOn(int clientNum, const PlayerView& self, const PlayerView& other, bool useTraces);

	void ReadHurt(int clientNum, const PlayerView& self, int now);
	int SelectTarget(int clientNum, const PlayerView& self, const WeaponInfo& selfWeapon,
					 const BotInput& input, bool useTraces, int now, PlayerView* outTarget);
	bool HurtLook(int clientNum, BotInput& input, int now);
	bool LastShotOf(int clientNum, float outFeet[3], int* outTime);

	enum GoalKind
	{
		GoalNone = 0,
		GoalNode,
		GoalPursuit,
	};
	GoalKind ChooseGoal(int clientNum, const PlayerView& self, const float* feet, int start,
						int* outGoal, const char** outKind);
	bool ReplanTask(int clientNum, const PlayerView& self, int now);
	bool HuntTargetFeet(int clientNum, const PlayerView& self, float out[3]);
	int PickHuntTarget(int clientNum, const PlayerView& self);
	int PickStartNode(const float* feet, float forwardYaw, bool preferAhead);
	int PickRoamGoal(int clientNum, const PlayerView& self, const float* feet, int start);
	int PickHoldNode(int clientNum, const float* around, float minUnits, float maxUnits, float* outLookYaw);
	int PickDefendNode(int clientNum, const float* objective, float minUnits, float maxUnits, float* outLookYaw);
	struct RouteWatch
	{
		int area = -1;
		float nodeCost = 0.0f;
		float freeUnits = 0.0f;
		float goal[3] = {};
	};
	const float* HeatPenalty(int team, float laneScale = 1.0f, float exposureScale = 0.0f,
							 const RouteWatch* watch = nullptr);
	bool TryGetRouteWatch(int clientNum, int goal, RouteWatch* out);
	bool TryGetThreatLookYaw(int clientNum, const PlayerView& self, float* outYaw);
	int AreaOfClient(int clientNum);
	bool IsKnownEnemyNear(int clientNum, const PlayerView& self, float rangeUnits);
	bool IsWalledOff(int clientNum, int other);
	int FindHiddenAreas(int fromArea, const float* around, float maxUnits, int* outAreas, int maxCount);

	bool HasMapMemory();
	float AreaDanger(int area);
	float AreaStrength(int area);
	bool IsLaneNode(int node);
	void AddHeat(int team, int node, float amount);
	void AddStuckHeat(int node);
	void AddLinkTrap(int from, int to);
	void ReportSighting(int spotterTeam, int enemy, const float* enemyFeet, int now);
	bool PickOpeningPoint(const float* feet, float out[3]);
	int ForcedTargetFor(int clientNum, const PlayerView& self);
	float PickLookYaw(int clientNum, const PlayerView& self);
	bool IsViewOpen(int clientNum, const PlayerView& self, float yawDeg, float range);
	bool TryGetFrontYaw(int team, const float* from, float* outYaw);
	bool IsPlayableToward(const PlayerView& self, float yawDeg);
	const Waypoints::LinkTrap* LinkTraps(int* outCount);
	bool IsRouteDead(const short* path, int length);
	float GraphDistanceSq(const float* feet, const float* nodeOrigin);

	bool RequestCover(int clientNum, const PlayerView& self, const float* threatEye);

	void OnTargetLost(int clientNum, int lostClient, const float* lastSeenPos);
	bool TryContinueSearch(int clientNum);

	void Idle(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn = true);

	void RunCombatAi(int clientNum, BotInput& input, bool useTraces);
	unsigned short PreferredHeldWeapon(const Personality& personality, const char* playerState);

	bool RunKillstreakAi(int clientNum, BotInput& input);
}
