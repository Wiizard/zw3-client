#pragma once

#include "Components/Modules/BotAI/Iw4.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"

namespace Components::BotAI::Navgen
{
	using SV_Trace_t      = void (__cdecl*)(void* results, const float* start, const float* end,
											const float* bounds, const int* ignoreParams,
											int contentmask, int locational,
											unsigned char* priorityMap, int staticmodels);

	using SL_GetString_t  = unsigned short (__cdecl*)(const char* name, unsigned int user);

	using SL_ConvertToString_t = const char* (__cdecl*)(unsigned short id);

	using FS_ReadFile_t   = int (__cdecl*)(const char* path, char** out);

	using FS_FreeFile_t   = void (__cdecl*)(char* buffer);

	using FS_OpenWrite_t  = int (__cdecl*)(const char* path, const char* folder);

	using FS_Printf_t     = void (__cdecl*)(int handle, const char* format, ...);

	using FS_CloseFile_t  = void (__cdecl*)(int handle);

	using SV_GetPlayerstate_t = char* (__cdecl*)(int clientNum);

	using Sys_Milliseconds_t = int (__cdecl*)();

	using Cbuf_AddText_t = void (__cdecl*)(int localClient, const char* text);


	static constexpr int generatorRevision = 35;

	static constexpr float gridStep       = 40.0f;
	static constexpr float segmentLength  = 20.0f;
	static constexpr float trueNormalMin  = 0.7f;
	static constexpr float stepUpMax      = 18.0f;
	static constexpr float walkDropMax    = 40.0f;
	static constexpr float arriveSlack    = 18.0f;
	static constexpr float dropLinkMin    = 40.0f;
	static constexpr float dropStepMin    = stepUpMax + 2.0f;
	static constexpr float dropLinkMax    = 150.0f;
	static constexpr float dropDeepMax    = 250.0f;
	static constexpr float mantleUpMax    = 60.0f;
	static constexpr float jumpHeight     = 39.0f;
	static constexpr float mantleJumpMax  = mantleUpMax + jumpHeight;
	static constexpr float mantleJumpReliableMax = 80.0f;
	static constexpr float hopRiseMax     = 36.0f;
	static constexpr float walkableNormal = 0.7f;
	static constexpr float faceNormalMax  = 0.5f;
	static constexpr float dedupRadius    = 24.0f;
	static constexpr float dedupHeight    = 44.0f;
	static constexpr float ledgeDedupHeight = 18.0f;
	static constexpr float latticeSpacing[6] = { 40.0f, 80.0f, 120.0f, 160.0f, 200.0f, 240.0f };
	static constexpr float latticeSpanScale  = 1.45f;

	static constexpr float standHalfHeight  = 35.0f;
	static constexpr float crouchHalfHeight = 25.0f;
	static constexpr float proneHalfHeight  = 15.0f;

	static constexpr float walkSegment   = 12.0f;
	static constexpr float groundLift    = 0.5f;
	static constexpr float shoulderShift = 4.0f;
	static constexpr float snapReachUp   = 56.0f;
	static constexpr float boxHalfWidth  = 15.0f;

	static constexpr float mantleDetourMin   = 400.0f;
	static constexpr float mantleDetourMulti = 3.0f;
	static constexpr int   pocketMax         = 8;
	static constexpr int   vantageMin        = 24;
	static constexpr float crouchDetourMin   = 200.0f;
	static constexpr float crouchDetourMulti = 3.0f;
	static constexpr float mantleApproachStep = 8.0f;
	static constexpr float mantleApproachHalfWidth = 8.0f;

	static constexpr int   slideReachSteps = 4;

	static constexpr int   bridgeRimLinks   = 5;
	static constexpr float bridgeReach      = 240.0f;
	static constexpr float bridgeRiseMax    = 100.0f;
	static constexpr int   islandMin        = 16;

	static constexpr float ladderProbeReach = 60.0f;
	static constexpr float ladderGrabMid  = 34.0f;
	static constexpr float ladderGrabHalf = 26.0f;
	static constexpr float ladderTopSearch  = 120.0f;
	static constexpr float ladderStandOff   = 20.0f;

	static constexpr float seedCoverage = 60.0f;
	static constexpr float seedRingRadius[2] = { 20.0f, 40.0f };

	static constexpr int maxRawNodes     = 65536;
	static constexpr int maxNodeLinks    = 16;
	static constexpr int maxFinalNodes   = 32768;
	static constexpr int maxFinalChildren = 262144;

	enum RawLinkFlag : unsigned char
	{
		RawLinkCrouch = 1,
		RawLinkGlass  = 2,
		RawLinkOver   = 4,
		RawLinkJumpFirst = 8,
	};

	struct RawNode
	{
		float origin[3];
		unsigned char type;
		bool removed;
		bool isSeed;
		bool isPlayerSeed;
		unsigned char linkCount;
		unsigned short links[maxNodeLinks];
		unsigned char linkKind[maxNodeLinks];
		unsigned char linkFlags[maxNodeLinks];
		unsigned char removedBy;
		unsigned char fillLinks;
		unsigned char region;
	};

	enum RemoveStage : unsigned char
	{
		RemovedNever = 0,
		RemovedContract,
		RemovedPocket,
		RemovedSpur,
		RemovedIsland,
		RemovedTrap,
		RemovedStranded,
		RemovedHurt,
		RemoveStageCount,
	};

	struct GroundHit
	{
		bool isValid;
		bool needsCrouch;
		bool isSlope;
		bool isRayStartSolid;
		float z;
		float rayZ;
		float normalZ;
		float rayNormalZ;
	};

	struct TraceHit
	{
		float fraction;
		float normalZ;
		float normal[3];
		int hitType;
		int surfaceFlags;
		int contents;
		bool isStartSolid;
	};

	struct MantleGeometry
	{
		bool hasFace;
		bool isFlagged;
		bool isOver;
		bool isValid;
		bool needsCrouch;
		bool needsJump;
		bool hasFooting;
		float faceDistance;
		float faceNormalZ;
		float ledgeDepth;
		float ledgeZ;
		float ledgeNormalZ;
		float landing[3];
		const char* refusal;
	};

	struct LadderHit
	{
		bool isValid;
		bool needsJump;
		float bottom[3];
		float top[3];
		float normal[2];
	};


	void Report(const char* format, ...);
	void ReportFile(const char* format, ...);
	void ReportCapped(int line, int consoleCap, const char* format, ...);
	void ReportBegin(const char* mapName, bool append);
	void ReportEnd();
	int  NowMs();

	extern bool probeVerbose;
	extern int traceCount;
	extern int statRayMiss;
	extern int statRayStartSolid;
	extern int statRaySlope;
	extern int statRayZero;
	extern int statMantleFaces;
	extern int statMantleUnflagged;
	extern int statMantleSlopes;
	extern int statMantleNoApproach;
	extern int statMantleWalled;
	extern int statMantleLinks;
	extern int statMantleOver;
	extern int statHopLinks;
	extern int statSlideLinks;
	extern int statLadderLinks;
	extern int statLadderFaces;
	extern int statLadderNoTop;
	extern int statLadderJumpLinks;
	void ResetTraceStats();
	float TraceWorldHit(const float* start, const float* end, const float* bounds,
						float* outNormalZ, int* outHitType);
	float TraceWorld(const float* start, const float* end, const float* bounds, float* outNormalZ);
	TraceHit TraceFull(const float* start, const float* end, const float* bounds, int contentmask);
	bool GroundRay(float x, float y, float fromZ, float depth, float* outZ, float* outNormalZ);
	bool GroundRayBox(float x, float y, float fromZ, float depth, float halfWidth, float* outZ, float* outNormalZ);
	bool IsInsideSolid(float x, float y, float z);
	GroundHit SnapBesideSolid(float x, float y, float fromZ, const float* parent, float* outX, float* outY);
	bool ClearThroughGlass(const float* start, const float* end, const float* bounds);
	void SuppressGlass(bool suppress);
	GroundHit SnapToGround(float x, float y, float fromZ, float depth);
	GroundHit GroundFitNoGlass(float x, float y, float ledgeZ);
	bool HasShoulderRoom(float x, float y, float z, bool crouch, float* outPushX = nullptr, float* outPushY = nullptr);
	bool HasSolidFooting(float x, float y, float z);
	bool HasPathFooting(float x, float y, float z);
	bool CanWalk(const float* from, const float* to);
	bool CanWalkCrouched(const float* from, const float* to);
	bool CanDrop(const float* from, const float* to);
	bool CanDropDeep(const float* from, const float* to);
	bool MarchFromNode(const float* start, int label, float x, float y, float z, char* outLine, int outSize);
	bool CanSlideDown(const float* from, const float* to);
	bool CanHop(const float* from, const float* to);
	MantleGeometry ProbeMantle(const float* from, float dirX, float dirY);
	bool FindLadder(const float* from, float dirX, float dirY, LadderHit* out);
	bool IsInsidePlayerClip(float x, float y, float z);
	int  SurfaceFlagsUnder(float x, float y, float z);

	float RenderBlockedFraction(const float* from, const float* to);
	unsigned long LoadedStamp();
	static constexpr int annotationRevision = 2;
	static constexpr int annotationRevisionNoWallbangs = 1;
	int SightTriangleCount();
	bool SightTriangleAt(int index, float outCorners[3][3]);
	int SightBoxCount();
	bool SightBoxAt(int index, float outCorners[8][3], bool* outIsSoft);
	int RenderBlockedCount();

	static constexpr int maxHurtVolumes = 64;
	static constexpr int maxObjectives  = 64;
	static constexpr int maxDynamicSolids = 64;
	void ScanWorldEntities();
	int  HurtVolumeCount();
	bool IsInHurtVolume(const float* point);
	int  ObjectiveCount();
	const float* ObjectivePosition(int index);
	const char* ObjectiveName(int index);
	const char* ObjectiveGametype(int index);
	int  DynamicSolidCount();
	void SuppressDynamicSolids(bool suppress);
	const char* ClassnameText(unsigned short id);

	extern RawNode rawNodes[maxRawNodes];
	extern int rawCount;
	void ResetGraph();
	bool PopFrontier(int* outNode);
	bool HasLink(int from, int to);
	void AddLink(int from, int to, Waypoints::LinkKind kind = Waypoints::LinkWalk, unsigned char flags = 0);
	void RemoveLink(int from, int to);
	Waypoints::LinkKind LinkKindOf(int from, int to);
	unsigned char LinkFlagsOf(int from, int to);
	void SetLinkFlags(int from, int to, unsigned char flags);
	bool LinkIfWalkable(int a, int b);
	int FindNearbyNode(float x, float y, float z);
	int FindNearbyNodeWithin(float x, float y, float z, float heightTolerance);
	int FindOtherNearbyNode(float x, float y, float z, int excludedNode);
	int CollectNodesWithin(float x, float y, float radius, int* out, int maxOut);
	int AddNode(const GroundHit& ground, float x, float y);
	void RemoveNode(int nodeIndex);
	void SweepDanglingLinks();
	float Distance3(const float* from, const float* to);
	float Distance2(const float* from, const float* to);
	extern int statNodeCapHits;
	extern int statLinkCapHits;
	int  RefusedLinkCount();
	bool RefusedLinkAt(int index, float outFrom[3], float outTo[3], unsigned char* outKind);
	extern unsigned char removeStage;
	const char* RemoveStageName(unsigned char stage);

	static constexpr int maxSeeds = 512;
	extern float seedPositions[maxSeeds][3];
	extern bool seedIsObjective[maxSeeds];
	extern int seedCount;
	extern int playerSeedCount;
	void SeedGraph();
	void ExpandNode(int nodeIndex);
	void ReportFillStats();

	int TrimMantleShortcuts();
	int TrimMantleDuplicates();
	int TrimMantlePockets();
	int TrimCrouchShortcuts();
	int PruneGraph();
	int BridgeStrandedRegions();
	bool IsProtectedRaw(int nodeIndex);

	void FileNameFor(const char* mapName, char* out, int outSize);
	void SaveActiveGraph(const char* mapName);
	int  LoadedRevision();
	int  HoleCount();
	bool HoleAt(int index, float outCentre[3], int* outCells, int* outKind);
	int  RegionCount();
}
