#pragma once

namespace Components::BotAI::Waypoints
{
	enum NodeType : unsigned char
	{
		NodeStand = 0,
		NodeCrouch,
		NodeProne,
		NodeClimb,
		NodeClaymore,
		NodeGrenade,
		NodeJavelin,
	};

	struct Node
	{
		float origin[3];
		unsigned int firstChild;
		unsigned char childCount;
		unsigned char type;
		unsigned char region;
	};

	constexpr unsigned int linkIndexMask = 0x00FFFFFFu;
	constexpr unsigned int linkMantle    = 0x80000000u;
	constexpr unsigned int linkDrop      = 0x40000000u;
	constexpr unsigned int linkCrouch    = 0x20000000u;
	constexpr unsigned int linkGlass     = 0x10000000u;
	constexpr unsigned int linkLadder    = 0x08000000u;
	constexpr unsigned int linkJump      = 0x04000000u;
	constexpr unsigned int linkKindMask  = linkMantle | linkDrop | linkLadder | linkJump;

	enum LinkKind : unsigned char
	{
		LinkWalk = 0,
		LinkMantle,
		LinkDrop,
		LinkLadder,
		LinkJump,
	};

	struct MapData
	{
		const char* map;
		const Node* nodes;
		const unsigned short* children;
		int count;
	};

	bool Load(const char* mapName);

	bool InstallGenerated(const Node* nodes, int nodeCount,
						  const unsigned int* children, int childCount);

	struct Objective
	{
		float origin[3];
		char name[32];
		char gametype[16];
		int node;
	};

	constexpr int maxObjectives = 64;

	void SetObjectives(const Objective* objectives, int count);
	int ObjectiveCount();
	const Objective& ObjectiveAt(int index);

	bool IsLoaded();
	int Count();
	const float* Origin(int node);
	unsigned char TypeOf(int node);
	unsigned char RegionOf(int node);

	int ChildCount(int node);
	int ChildAt(int node, int index);
	unsigned int ChildFlags(int node, int index);
	bool HasChild(int node, int target);

	LinkKind KindOf(int from, int to);
	const char* KindName(int kind);
	void SetClimbCostScale(float scale);
	unsigned int FlagsOf(int from, int to);
	bool HasLinkKinds();
	LinkKind KindFromFlags(unsigned int flags);

	int Nearest(const float* position);

	bool EditDeleteNode(int node);
	bool EditRemoveLink(int a, int b);
	bool EditAddLink(int a, int b);
	bool EditAddLinkKind(int a, int b, unsigned int flagsAToB, unsigned int flagsBToA);
	bool EditSetType(int node, unsigned char type);
	bool EditMoveNode(int node, const float* origin);
	int  EditAddNode(const float* origin);

	constexpr int nodeCap = 32768;

	constexpr int maxFileLinks = 32;

	struct LinkTrap
	{
		int from;
		int to;
		float penalty;
		int failures;
	};

	int FindPath(int from, int to, short* path, int maxPath, const float* nodePenalty = nullptr,
				 const LinkTrap* traps = nullptr, int trapCount = 0);
	void SetLadderDescent(bool isAllowed);

	constexpr int wallDirections = 16;
	constexpr float wallDirectionDeg = 360.0f / wallDirections;
	constexpr float wallReachUnit = 4.0f;
	constexpr int wallReachOpen = 255;
	constexpr int wallShareFull = 255;
	constexpr float standEyeRise = 60.0f;
	constexpr float crouchEyeRise = 40.0f;
	constexpr int maxAreas = 4096;

	struct NodeWalls
	{
		unsigned char standReach[wallDirections];
		unsigned char crouchReach[wallDirections];
		unsigned char standShare[wallDirections];
		unsigned char crouchShare[wallDirections];
	};

	struct AreaInfo
	{
		int node;
		int farNode;
		int memberCount;
	};

	constexpr int AreaVisBytes(int areaCount)
	{
		return static_cast<int>((static_cast<long long>(areaCount) * (areaCount - 1) / 2 + 7) / 8);
	}

	constexpr int maxWallbangPairs = 65536;

	struct AreaWallbang
	{
		unsigned short low;
		unsigned short high;
		unsigned char share;
		unsigned char pad;
	};

	bool InstallAnnotations(const NodeWalls* walls, const unsigned short* nodeAreas, int nodeCount,
							const AreaInfo* areas, int areaCount, const unsigned char* visBits);
	bool InstallWallbangs(const AreaWallbang* pairs, int count);
	int AnnotationSerial();
	int WallbangCount();
	const AreaWallbang* WallbangAt(int index);
	float WallbangShare(int areaA, int areaB);
	void ClearAnnotations();
	bool HasWalls();
	const NodeWalls* WallsOf(int node);
	int AreaCount();
	int AreaOf(int node);
	const AreaInfo* AreaAt(int area);
	bool AreasSee(int areaA, int areaB);
	int WallDirectionOf(float yawDeg);
}
