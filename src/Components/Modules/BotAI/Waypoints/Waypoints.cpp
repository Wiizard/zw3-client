#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"

#include <cmath>
#include <cstring>
#include <vector>

namespace Components::BotAI::Waypoints
{
	extern const MapData* const knownMaps[];
	extern const int knownMapCount;

	static constexpr int maxNodes = 32768;
	static constexpr int maxGeneratedChildren = 262144;

	struct Graph
	{
		const char* map;
		const Node* nodes;
		const unsigned int* children;
		int count;
	};

	static const Graph* active = nullptr;
	static int graphVersion = 0;

	static bool hasLinkKinds = false;

	static Node generatedNodes[maxNodes];
	static unsigned int generatedChildren[maxGeneratedChildren];
	static Graph generatedData;
	static Objective objectives[maxObjectives];
	static int objectiveCount = 0;

	static bool AnyLinkFlags(const Graph* data)
	{
		for (int i = 0; i < data->count; ++i)
		{
			const Node& node = data->nodes[i];
			for (int c = 0; c < node.childCount; ++c)
			{
				if (data->children[node.firstChild + c] & ~linkIndexMask)
				{
					return true;
				}
			}
		}
		return false;
	}

	static float Distance(const float* from, const float* to)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float dz = to[2] - from[2];
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	bool Load(const char* mapName)
	{
		active = nullptr;
		++graphVersion;
		objectiveCount = 0;
		ClearAnnotations();
		if (!mapName || !*mapName)
		{
			return false;
		}

		for (int i = 0; i < knownMapCount; ++i)
		{
			if (std::strcmp(knownMaps[i]->map, mapName) == 0)
			{
				const MapData* baked = knownMaps[i];
				if (baked->count > 0 && baked->count <= maxNodes)
				{
					int written = 0;
					bool fits = true;
					for (int n = 0; n < baked->count && fits; ++n)
					{
						const Node& node = baked->nodes[n];
						if (written + node.childCount > maxGeneratedChildren)
						{
							fits = false;
							break;
						}
						generatedNodes[n] = node;
						generatedNodes[n].firstChild = static_cast<unsigned int>(written);
						for (int c = 0; c < node.childCount; ++c)
						{
							generatedChildren[written++] = baked->children[node.firstChild + c];
						}
					}
					if (fits)
					{
						generatedData.map = baked->map;
						generatedData.nodes = generatedNodes;
						generatedData.children = generatedChildren;
						generatedData.count = baked->count;
						hasLinkKinds = false;
						active = &generatedData;
					}
				}
				break;
			}
		}

		return active != nullptr;
	}

	bool InstallGenerated(const Node* nodes, int nodeCount,
						  const unsigned int* children, int childCount)
	{
		if (nodeCount <= 0 || nodeCount > maxNodes
			|| childCount < 0 || childCount > maxGeneratedChildren)
		{
			return false;
		}

		active = nullptr;
		++graphVersion;
		objectiveCount = 0;
		ClearAnnotations();
		std::memcpy(generatedNodes, nodes, sizeof(Node) * nodeCount);
		std::memcpy(generatedChildren, children, sizeof(unsigned int) * childCount);

		generatedData.map = "generated";
		generatedData.nodes = generatedNodes;
		generatedData.children = generatedChildren;
		generatedData.count = nodeCount;
		hasLinkKinds = AnyLinkFlags(&generatedData);
		active = &generatedData;
		return true;
	}

	void SetObjectives(const Objective* source, int count)
	{
		objectiveCount = 0;
		if (!active)
		{
			return;
		}
		for (int i = 0; i < count && objectiveCount < maxObjectives; ++i)
		{
			Objective& objective = objectives[objectiveCount++];
			objective = source[i];
			objective.node = Nearest(objective.origin);
		}
	}

	int ObjectiveCount()
	{
		return objectiveCount;
	}

	const Objective& ObjectiveAt(int index)
	{
		return objectives[index];
	}

	bool IsLoaded()
	{
		return active != nullptr;
	}

	int Count()
	{
		return active ? active->count : 0;
	}

	static bool IsNodeIndex(int node)
	{
		return active && node >= 0 && node < active->count;
	}

	const float* Origin(int node)
	{
		static const float noOrigin[3] = {};
		if (!IsNodeIndex(node))
		{
			return noOrigin;
		}
		return active->nodes[node].origin;
	}

	unsigned char TypeOf(int node)
	{
		if (!IsNodeIndex(node))
		{
			return NodeStand;
		}
		return active->nodes[node].type;
	}

	unsigned char RegionOf(int node)
	{
		if (!IsNodeIndex(node))
		{
			return 0;
		}
		return active->nodes[node].region;
	}

	int ChildCount(int node)
	{
		if (!IsNodeIndex(node))
		{
			return 0;
		}
		return active->nodes[node].childCount;
	}

	int ChildAt(int node, int index)
	{
		if (!IsNodeIndex(node) || index < 0 || index >= active->nodes[node].childCount)
		{
			return 0;
		}
		return active->children[active->nodes[node].firstChild + index] & linkIndexMask;
	}

	unsigned int ChildFlags(int node, int index)
	{
		if (!IsNodeIndex(node) || index < 0 || index >= active->nodes[node].childCount)
		{
			return 0;
		}
		return active->children[active->nodes[node].firstChild + index] & ~linkIndexMask;
	}

	bool HasChild(int node, int target)
	{
		if (!IsNodeIndex(node))
		{
			return false;
		}
		const Node& parent = active->nodes[node];
		for (int i = 0; i < parent.childCount; ++i)
		{
			if ((active->children[parent.firstChild + i] & linkIndexMask) == target)
			{
				return true;
			}
		}
		return false;
	}

	bool HasLinkKinds()
	{
		return hasLinkKinds;
	}

	static constexpr float stepUpMax   = 18.0f;
	static constexpr float walkDropMax = 40.0f;

	static LinkKind GuessKind(int from, int to)
	{
		if (HasChild(to, from))
		{
			return LinkWalk;
		}
		const float rise = active->nodes[to].origin[2] - active->nodes[from].origin[2];
		if (rise > stepUpMax)
		{
			return LinkMantle;
		}
		if (rise < -walkDropMax)
		{
			return LinkDrop;
		}
		return LinkWalk;
	}

	LinkKind KindFromFlags(unsigned int flags)
	{
		if (flags & linkMantle)
		{
			return LinkMantle;
		}
		if (flags & linkDrop)
		{
			return LinkDrop;
		}
		if (flags & linkLadder)
		{
			return LinkLadder;
		}
		if (flags & linkJump)
		{
			return LinkJump;
		}
		return LinkWalk;
	}

	LinkKind KindOf(int from, int to)
	{
		if (!IsNodeIndex(from) || !IsNodeIndex(to))
		{
			return LinkWalk;
		}
		const Node& parent = active->nodes[from];
		for (int i = 0; i < parent.childCount; ++i)
		{
			const unsigned int entry = active->children[parent.firstChild + i];
			if ((entry & linkIndexMask) != to)
			{
				continue;
			}
			return hasLinkKinds ? KindFromFlags(entry & ~linkIndexMask) : GuessKind(from, to);
		}
		return LinkWalk;
	}

	const char* KindName(int kind)
	{
		static const char* const names[5] = { "walk", "mantle", "drop", "ladder", "jump" };
		if (kind < 0 || kind >= 5)
		{
			return "link";
		}
		return names[kind];
	}

	unsigned int FlagsOf(int from, int to)
	{
		if (!IsNodeIndex(from) || !IsNodeIndex(to))
		{
			return 0;
		}
		const Node& parent = active->nodes[from];
		for (int i = 0; i < parent.childCount; ++i)
		{
			const unsigned int entry = active->children[parent.firstChild + i];
			if ((entry & linkIndexMask) == static_cast<unsigned int>(to))
			{
				return entry & ~linkIndexMask;
			}
		}
		return 0;
	}

	static constexpr float nearestFloorBand = 72.0f;
	static constexpr float nearestFloorReachSq = 500.0f * 500.0f;

	static constexpr float nearestCellUnits = 256.0f;
	static constexpr int nearestCellReach = 2;
	static constexpr float nearestGridProofSq = nearestCellUnits * nearestCellReach * nearestCellUnits * nearestCellReach;
	static std::vector<int> nearestCellFirst;
	static std::vector<int> nearestCellItems;
	static float nearestGridOrigin[2] = { 0.0f, 0.0f };
	static int nearestGridColumns = 0;
	static int nearestGridRows = 0;
	static int nearestGridVersion = -1;
	static int nearestGridCount = -1;

	static void BuildNearestGrid()
	{
		nearestGridVersion = graphVersion;
		nearestGridCount = active->count;
		float low[2] = { 0.0f, 0.0f };
		float high[2] = { 0.0f, 0.0f };
		for (int i = 0; i < active->count; ++i)
		{
			const float* origin = active->nodes[i].origin;
			for (int axis = 0; axis < 2; ++axis)
			{
				if (i == 0 || origin[axis] < low[axis])
				{
					low[axis] = origin[axis];
				}
				if (i == 0 || origin[axis] > high[axis])
				{
					high[axis] = origin[axis];
				}
			}
		}
		nearestGridOrigin[0] = low[0];
		nearestGridOrigin[1] = low[1];
		nearestGridColumns = static_cast<int>((high[0] - low[0]) / nearestCellUnits) + 1;
		nearestGridRows = static_cast<int>((high[1] - low[1]) / nearestCellUnits) + 1;
		const int cellCount = nearestGridColumns * nearestGridRows;
		nearestCellFirst.assign(static_cast<std::size_t>(cellCount) + 1, 0);
		nearestCellItems.assign(static_cast<std::size_t>(active->count), 0);
		std::vector<int> cellOf(static_cast<std::size_t>(active->count), 0);
		for (int i = 0; i < active->count; ++i)
		{
			const float* origin = active->nodes[i].origin;
			const int column = static_cast<int>((origin[0] - nearestGridOrigin[0]) / nearestCellUnits);
			const int row = static_cast<int>((origin[1] - nearestGridOrigin[1]) / nearestCellUnits);
			cellOf[i] = row * nearestGridColumns + column;
			++nearestCellFirst[cellOf[i] + 1];
		}
		for (int c = 0; c < cellCount; ++c)
		{
			nearestCellFirst[c + 1] += nearestCellFirst[c];
		}
		std::vector<int> fill(nearestCellFirst.begin(), nearestCellFirst.end() - 1);
		for (int i = 0; i < active->count; ++i)
		{
			nearestCellItems[fill[cellOf[i]]++] = i;
		}
	}

	static int NearestFullScan(const float* position);

	int Nearest(const float* position)
	{
		if (!active)
		{
			return -1;
		}
		if (nearestGridVersion != graphVersion || nearestGridCount != active->count)
		{
			BuildNearestGrid();
		}

		const int centreColumn = static_cast<int>(std::floor((position[0] - nearestGridOrigin[0]) / nearestCellUnits));
		const int centreRow = static_cast<int>(std::floor((position[1] - nearestGridOrigin[1]) / nearestCellUnits));
		int best = -1;
		float bestDistance = 0.0f;
		int bestOnFloor = -1;
		float bestOnFloorDistance = 0.0f;
		for (int row = centreRow - nearestCellReach; row <= centreRow + nearestCellReach; ++row)
		{
			if (row < 0 || row >= nearestGridRows)
			{
				continue;
			}
			for (int column = centreColumn - nearestCellReach; column <= centreColumn + nearestCellReach; ++column)
			{
				if (column < 0 || column >= nearestGridColumns)
				{
					continue;
				}
				const int cell = row * nearestGridColumns + column;
				for (int k = nearestCellFirst[cell]; k < nearestCellFirst[cell + 1]; ++k)
				{
					const int i = nearestCellItems[k];
					if (active->nodes[i].childCount == 0)
					{
						continue;
					}
					const float* origin = active->nodes[i].origin;
					const float dx = origin[0] - position[0];
					const float dy = origin[1] - position[1];
					const float rise = origin[2] - position[2];
					const float dz = rise * 3.0f;
					const float distance = dx * dx + dy * dy + dz * dz;
					if (best < 0 || distance < bestDistance || (distance == bestDistance && i < best))
					{
						best = i;
						bestDistance = distance;
					}
					const float flatDistance = dx * dx + dy * dy;
					const bool isOnFloor = rise > -nearestFloorBand && rise < nearestFloorBand;
					if (isOnFloor && (bestOnFloor < 0 || flatDistance < bestOnFloorDistance
									  || (flatDistance == bestOnFloorDistance && i < bestOnFloor)))
					{
						bestOnFloor = i;
						bestOnFloorDistance = flatDistance;
					}
				}
			}
		}
		if (bestOnFloor >= 0 && bestOnFloorDistance <= nearestFloorReachSq)
		{
			return bestOnFloor;
		}
		if (best >= 0 && bestDistance < nearestGridProofSq)
		{
			return best;
		}
		return NearestFullScan(position);
	}

	static int NearestFullScan(const float* position)
	{
		if (!active)
		{
			return -1;
		}

		int best = -1;
		float bestDistance = 0.0f;
		int bestOnFloor = -1;
		float bestOnFloorDistance = 0.0f;

		for (int i = 0; i < active->count; ++i)
		{
			if (active->nodes[i].childCount == 0)
			{
				continue;
			}
			const float* origin = active->nodes[i].origin;
			const float dx = origin[0] - position[0];
			const float dy = origin[1] - position[1];

			const float rise = origin[2] - position[2];
			const float dz = rise * 3.0f;
			const float distance = dx * dx + dy * dy + dz * dz;

			if (best < 0 || distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
			const float flatDistance = dx * dx + dy * dy;
			if (rise > -nearestFloorBand && rise < nearestFloorBand && (bestOnFloor < 0 || flatDistance < bestOnFloorDistance))
			{
				bestOnFloor = i;
				bestOnFloorDistance = flatDistance;
			}
		}

		if (bestOnFloor >= 0 && bestOnFloorDistance <= nearestFloorReachSq)
		{
			return bestOnFloor;
		}
		return best;
	}

	static unsigned int editAdj[maxNodes][maxFileLinks];
	static unsigned char editAdjCount[maxNodes];

	static bool MakeEditable()
	{
		return active != nullptr;
	}

	static void LoadAdjacency()
	{
		for (int i = 0; i < active->count; ++i)
		{
			editAdjCount[i] = 0;
			const Node& node = active->nodes[i];
			for (int c = 0; c < node.childCount && editAdjCount[i] < maxFileLinks; ++c)
			{
				editAdj[i][editAdjCount[i]++] = active->children[node.firstChild + c];
			}
		}
	}

	static bool RebuildChildren()
	{
		int written = 0;
		for (int i = 0; i < generatedData.count; ++i)
		{
			if (written + editAdjCount[i] > maxGeneratedChildren)
			{
				return false;
			}
			generatedNodes[i].firstChild = static_cast<unsigned int>(written);
			generatedNodes[i].childCount = editAdjCount[i];
			for (int c = 0; c < editAdjCount[i]; ++c)
			{
				generatedChildren[written++] = editAdj[i][c];
			}
		}
		return true;
	}

	static void AdjRemove(int fromNode, int target)
	{
		for (int c = 0; c < editAdjCount[fromNode]; ++c)
		{
			if ((editAdj[fromNode][c] & linkIndexMask) == target)
			{
				editAdj[fromNode][c] = editAdj[fromNode][--editAdjCount[fromNode]];
				return;
			}
		}
	}

	bool EditDeleteNode(int node)
	{
		if (!MakeEditable() || node < 0 || node >= active->count)
		{
			return false;
		}

		LoadAdjacency();
		editAdjCount[node] = 0;
		for (int i = 0; i < active->count; ++i)
		{
			AdjRemove(i, node);
		}
		return RebuildChildren();
	}

	bool EditRemoveLink(int a, int b)
	{
		if (!MakeEditable() || a < 0 || b < 0 || a >= active->count || b >= active->count)
		{
			return false;
		}

		LoadAdjacency();
		AdjRemove(a, b);
		AdjRemove(b, a);
		return RebuildChildren();
	}

	bool EditAddLink(int a, int b)
	{
		if (!MakeEditable() || a == b || a < 0 || b < 0
			|| a >= active->count || b >= active->count)
		{
			return false;
		}

		LoadAdjacency();
		if (editAdjCount[a] >= maxFileLinks || editAdjCount[b] >= maxFileLinks)
		{
			return false;
		}

		bool hasAToB = false;
		bool hasBToA = false;
		for (int c = 0; c < editAdjCount[a]; ++c) { if ((editAdj[a][c] & linkIndexMask) == b) { hasAToB = true; } }
		for (int c = 0; c < editAdjCount[b]; ++c) { if ((editAdj[b][c] & linkIndexMask) == a) { hasBToA = true; } }
		if (!hasAToB) { editAdj[a][editAdjCount[a]++] = static_cast<unsigned int>(b); }
		if (!hasBToA) { editAdj[b][editAdjCount[b]++] = static_cast<unsigned int>(a); }
		return RebuildChildren();
	}

	bool EditAddLinkKind(int a, int b, unsigned int flagsAToB, unsigned int flagsBToA)
	{
		if (!MakeEditable() || a == b || a < 0 || b < 0
			|| a >= active->count || b >= active->count)
		{
			return false;
		}

		LoadAdjacency();
		const int ends[2] = { a, b };
		const unsigned int words[2] = { flagsAToB, flagsBToA };
		for (int side = 0; side < 2; ++side)
		{
			if (words[side] == linkIndexMask)
			{
				continue;
			}
			const int from = ends[side];
			const int to = ends[1 - side];
			AdjRemove(from, to);
			if (editAdjCount[from] >= maxFileLinks)
			{
				return false;
			}
			editAdj[from][editAdjCount[from]++] = static_cast<unsigned int>(to) | (words[side] & ~linkIndexMask);
		}
		return RebuildChildren();
	}

	bool EditSetType(int node, unsigned char type)
	{
		if (!MakeEditable() || node < 0 || node >= active->count)
		{
			return false;
		}
		generatedNodes[node].type = type;
		return true;
	}

	bool EditMoveNode(int node, const float* origin)
	{
		if (!MakeEditable() || node < 0 || node >= active->count)
		{
			return false;
		}
		generatedNodes[node].origin[0] = origin[0];
		generatedNodes[node].origin[1] = origin[1];
		generatedNodes[node].origin[2] = origin[2];
		++graphVersion;
		return true;
	}

	int EditAddNode(const float* origin)
	{
		if (!MakeEditable() || generatedData.count >= maxNodes)
		{
			return -1;
		}

		const int nearest = Nearest(origin);
		if (nearest < 0)
		{
			return -1;
		}

		LoadAdjacency();
		const int added = generatedData.count;
		generatedNodes[added].origin[0] = origin[0];
		generatedNodes[added].origin[1] = origin[1];
		generatedNodes[added].origin[2] = origin[2];
		generatedNodes[added].type = NodeStand;
		generatedNodes[added].region = 0;
		editAdjCount[added] = 0;
		++generatedData.count;
		++graphVersion;

		if (editAdjCount[nearest] >= maxFileLinks)
		{
			--generatedData.count;
			return -1;
		}
		editAdj[added][editAdjCount[added]++] = static_cast<unsigned int>(nearest);
		editAdj[nearest][editAdjCount[nearest]++] = static_cast<unsigned int>(added);

		if (!RebuildChildren())
		{
			--generatedData.count;
			return -1;
		}
		return added;
	}


	static constexpr float gradeCharge      = 2.0f;
	static constexpr float oneWayPenalty    = 40.0f;
	static constexpr float dropPenalty      = 150.0f;
	static constexpr float dropFallCharge   = 20.0f;
	static constexpr float dropFallFree     = 90.0f;
	static constexpr float mantlePenalty    = 500.0f;
	static constexpr float mantleRiseCharge = 8.0f;
	static constexpr float jumpPenalty      = 300.0f;
	static constexpr float jumpRiseCharge   = 4.0f;
	static constexpr float ladderPenalty    = 400.0f;
	static constexpr float ladderRiseCharge = 2.0f;
	static constexpr float crouchPenalty    = 150.0f;
	static constexpr float glassPenalty     = 120.0f;

	static float climbCostScale = 1.0f;

	void SetClimbCostScale(float scale)
	{
		climbCostScale = scale;
	}

	static float LinkCost(int from, int to, unsigned int flags)
	{
		const Node& a = active->nodes[from];
		const Node& b = active->nodes[to];
		const float rise = b.origin[2] - a.origin[2];
		float cost = Distance(a.origin, b.origin);

		const LinkKind kind = hasLinkKinds ? KindFromFlags(flags) : GuessKind(from, to);
		if (kind == LinkWalk)
		{
			const float dx = b.origin[0] - a.origin[0];
			const float dy = b.origin[1] - a.origin[1];
			const float run = std::sqrt(dx * dx + dy * dy);
			if (rise > 0.0f && run > 1.0f)
			{
				cost *= 1.0f + gradeCharge * rise / run;
			}
			if (!HasChild(to, from))
			{
				cost += oneWayPenalty;
			}
		}
		else if (kind == LinkMantle)
		{
			cost += mantlePenalty * climbCostScale;
			if (rise > 0.0f)
			{
				cost += mantleRiseCharge * rise;
			}
		}
		else if (kind == LinkDrop)
		{
			cost += dropPenalty * climbCostScale;
			if (-rise > dropFallFree)
			{
				cost += dropFallCharge * (-rise - dropFallFree);
			}
		}
		else if (kind == LinkJump)
		{
			cost += jumpPenalty * climbCostScale;
			if (rise > 0.0f)
			{
				cost += jumpRiseCharge * rise;
			}
		}
		else if (kind == LinkLadder)
		{
			cost += ladderPenalty * climbCostScale;
			if (rise > 0.0f)
			{
				cost += ladderRiseCharge * rise;
			}
		}
		if (flags & linkCrouch)
		{
			cost += crouchPenalty;
		}
		if (flags & linkGlass)
		{
			cost += glassPenalty;
		}
		return cost;
	}


	static constexpr float unreached = 1.0e30f;
	static constexpr int heapCapacity = maxGeneratedChildren + maxNodes;
	static int heapNode[heapCapacity];
	static float heapKey[heapCapacity];
	static int heapCount = 0;

	static void HeapPush(int node, float key)
	{
		if (heapCount >= heapCapacity)
		{
			return;
		}
		int slot = heapCount++;
		while (slot > 0)
		{
			const int parent = (slot - 1) / 2;
			if (heapKey[parent] <= key)
			{
				break;
			}
			heapKey[slot] = heapKey[parent];
			heapNode[slot] = heapNode[parent];
			slot = parent;
		}
		heapKey[slot] = key;
		heapNode[slot] = node;
	}

	static int HeapPop()
	{
		const int top = heapNode[0];
		--heapCount;
		if (heapCount > 0)
		{
			const float key = heapKey[heapCount];
			const int node = heapNode[heapCount];
			int slot = 0;
			while (true)
			{
				int child = 2 * slot + 1;
				if (child >= heapCount)
				{
					break;
				}
				if (child + 1 < heapCount && heapKey[child + 1] < heapKey[child])
				{
					++child;
				}
				if (heapKey[child] >= key)
				{
					break;
				}
				heapKey[slot] = heapKey[child];
				heapNode[slot] = heapNode[child];
				slot = child;
			}
			heapKey[slot] = key;
			heapNode[slot] = node;
		}
		return top;
	}

	static bool isLadderDescentAllowed = false;

	void SetLadderDescent(bool isAllowed)
	{
		isLadderDescentAllowed = isAllowed;
	}

	int FindPath(int from, int to, short* path, int maxPath, const float* nodePenalty,
				 const LinkTrap* traps, int trapCount)
	{
		if (!active || from < 0 || to < 0 || from >= active->count || to >= active->count || maxPath <= 0)
		{
			return 0;
		}

		const unsigned char goalRegion = active->nodes[to].region;
		if (goalRegion != 0 && goalRegion != active->nodes[from].region)
		{
			return 0;
		}

		static float costFromStart[maxNodes];
		static short cameFrom[maxNodes];
		static bool isClosed[maxNodes];
		static bool isTrapFrom[maxNodes];

		const int count = active->count;
		for (int i = 0; i < count; ++i)
		{
			costFromStart[i] = unreached;
			cameFrom[i] = -1;
			isClosed[i] = false;
			isTrapFrom[i] = false;
		}
		for (int t = 0; t < trapCount; ++t)
		{
			if (traps[t].from >= 0 && traps[t].from < count)
			{
				isTrapFrom[traps[t].from] = true;
			}
		}

		heapCount = 0;
		costFromStart[from] = 0.0f;
		HeapPush(from, Distance(active->nodes[from].origin, active->nodes[to].origin));

		bool reached = false;
		while (heapCount > 0)
		{
			const int current = HeapPop();
			if (isClosed[current])
			{
				continue;
			}
			if (current == to)
			{
				reached = true;
				break;
			}
			isClosed[current] = true;

			const Node& node = active->nodes[current];
			for (int c = 0; c < node.childCount; ++c)
			{
				const unsigned int entry = active->children[node.firstChild + c];
				const int child = entry & linkIndexMask;
				if (child >= count || isClosed[child])
				{
					continue;
				}

				if (hasLinkKinds && !isLadderDescentAllowed && (entry & linkLadder) != 0
					&& active->nodes[child].origin[2] < active->nodes[current].origin[2])
				{
					continue;
				}

				float cost = costFromStart[current] + LinkCost(current, child, entry & ~linkIndexMask);
				if (nodePenalty)
				{
					cost += nodePenalty[child];
				}
				if (isTrapFrom[current])
				{
					for (int t = 0; t < trapCount; ++t)
					{
						if (traps[t].from == current && traps[t].to == child)
						{
							cost += traps[t].penalty;
						}
					}
				}
				if (cost >= costFromStart[child])
				{
					continue;
				}

				cameFrom[child] = static_cast<short>(current);
				costFromStart[child] = cost;
				HeapPush(child, cost + Distance(active->nodes[child].origin, active->nodes[to].origin));
			}
		}
		if (!reached)
		{
			return 0;
		}

		static short reversed[maxNodes];
		int length = 0;
		for (int node = to; node >= 0 && length < maxNodes; node = cameFrom[node])
		{
			reversed[length++] = static_cast<short>(node);
			if (node == from)
			{
				break;
			}
		}

		const int pathCount = length < maxPath ? length : maxPath;
		for (int i = 0; i < pathCount; ++i)
		{
			path[i] = reversed[length - 1 - i];
		}

		return pathCount;
	}


	static NodeWalls nodeWalls[maxNodes];
	static unsigned short nodeAreas[maxNodes];
	static AreaInfo areaInfos[maxAreas];
	static unsigned char areaVis[AreaVisBytes(maxAreas)];
	static int annotatedNodeCount = 0;
	static int areaCount = 0;
	static AreaWallbang wallbangs[maxWallbangPairs];
	static int wallbangCount = 0;
	static int annotationSerial = 0;

	void ClearAnnotations()
	{
		annotatedNodeCount = 0;
		areaCount = 0;
		wallbangCount = 0;
		++annotationSerial;
	}

	int AnnotationSerial()
	{
		return annotationSerial;
	}

	bool InstallWallbangs(const AreaWallbang* pairs, int count)
	{
		wallbangCount = 0;
		if (count < 0 || count > maxWallbangPairs || areaCount == 0)
		{
			return false;
		}
		for (int k = 0; k < count; ++k)
		{
			const bool isOrdered = k == 0 || pairs[k - 1].low < pairs[k].low
				|| (pairs[k - 1].low == pairs[k].low && pairs[k - 1].high < pairs[k].high);
			if (pairs[k].low >= pairs[k].high || pairs[k].high >= areaCount || !isOrdered)
			{
				return false;
			}
		}
		if (count > 0)
		{
			std::memcpy(wallbangs, pairs, sizeof(AreaWallbang) * count);
		}
		wallbangCount = count;
		return true;
	}

	int WallbangCount()
	{
		return wallbangCount;
	}

	const AreaWallbang* WallbangAt(int index)
	{
		if (index < 0 || index >= wallbangCount)
		{
			return nullptr;
		}
		return &wallbangs[index];
	}

	float WallbangShare(int areaA, int areaB)
	{
		if (areaA == areaB || areaA < 0 || areaB < 0)
		{
			return 0.0f;
		}
		int low = areaA;
		int high = areaB;
		if (low > high)
		{
			low = areaB;
			high = areaA;
		}
		int first = 0;
		int last = wallbangCount - 1;
		while (first <= last)
		{
			const int middle = (first + last) / 2;
			const AreaWallbang& pair = wallbangs[middle];
			if (pair.low == low && pair.high == high)
			{
				return static_cast<float>(pair.share) / 255.0f;
			}
			const bool isBefore = pair.low < low || (pair.low == low && pair.high < high);
			if (isBefore)
			{
				first = middle + 1;
			}
			else
			{
				last = middle - 1;
			}
		}
		return 0.0f;
	}

	bool InstallAnnotations(const NodeWalls* walls, const unsigned short* areas, int nodeCount,
							const AreaInfo* infos, int count, const unsigned char* visBits)
	{
		ClearAnnotations();
		if (!active || nodeCount != active->count || nodeCount > maxNodes || count < 0 || count > maxAreas)
		{
			return false;
		}
		std::memcpy(nodeWalls, walls, sizeof(NodeWalls) * nodeCount);
		std::memcpy(nodeAreas, areas, sizeof(unsigned short) * nodeCount);
		if (count > 0)
		{
			std::memcpy(areaInfos, infos, sizeof(AreaInfo) * count);
			std::memcpy(areaVis, visBits, AreaVisBytes(count));
		}
		areaCount = count;
		annotatedNodeCount = nodeCount;
		return true;
	}

	bool HasWalls()
	{
		return annotatedNodeCount > 0;
	}

	const NodeWalls* WallsOf(int node)
	{
		if (!IsNodeIndex(node) || node >= annotatedNodeCount)
		{
			return nullptr;
		}
		return &nodeWalls[node];
	}

	int AreaCount()
	{
		return areaCount;
	}

	int AreaOf(int node)
	{
		if (!IsNodeIndex(node) || node >= annotatedNodeCount)
		{
			return -1;
		}
		const int area = nodeAreas[node];
		if (area >= areaCount)
		{
			return -1;
		}
		return area;
	}

	const AreaInfo* AreaAt(int area)
	{
		if (area < 0 || area >= areaCount)
		{
			return nullptr;
		}
		return &areaInfos[area];
	}

	bool AreasSee(int areaA, int areaB)
	{
		if (areaA < 0 || areaB < 0 || areaA >= areaCount || areaB >= areaCount)
		{
			return false;
		}
		if (areaA == areaB)
		{
			return true;
		}
		int low = areaA;
		int high = areaB;
		if (low > high)
		{
			low = areaB;
			high = areaA;
		}
		const long long bit = static_cast<long long>(low) * areaCount - static_cast<long long>(low) * (low + 1) / 2
			+ (high - low - 1);
		return (areaVis[bit >> 3] & (1 << (bit & 7))) != 0;
	}

	int WallDirectionOf(float yawDeg)
	{
		float turns = yawDeg / wallDirectionDeg;
		int direction = static_cast<int>(std::floor(turns + 0.5f)) % wallDirections;
		if (direction < 0)
		{
			direction += wallDirections;
		}
		return direction;
	}
}
