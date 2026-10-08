#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include <cmath>

namespace Components::BotAI::Navgen
{
	RawNode rawNodes[maxRawNodes];
	int statNodeCapHits = 0;
	int statLinkCapHits = 0;
	int rawCount = 0;

	struct RefusedLink
	{
		float from[3];
		float to[3];
		unsigned char kind;
	};
	static constexpr int maxRefusedLinks = 64;
	static RefusedLink refusedLinks[maxRefusedLinks];
	static int refusedLinkCount = 0;

	int RefusedLinkCount()
	{
		return refusedLinkCount;
	}

	bool RefusedLinkAt(int index, float outFrom[3], float outTo[3], unsigned char* outKind)
	{
		if (index < 0 || index >= refusedLinkCount)
		{
			return false;
		}
		for (int axis = 0; axis < 3; ++axis)
		{
			outFrom[axis] = refusedLinks[index].from[axis];
			outTo[axis] = refusedLinks[index].to[axis];
		}
		*outKind = refusedLinks[index].kind;
		return true;
	}

	static constexpr int cellBuckets = 16384;
	static constexpr float cellSize = 128.0f;
	static int cellHead[cellBuckets];
	static int cellNext[maxRawNodes];

	static int CellIndex(float x, float y)
	{
		const int cx = static_cast<int>(std::floor(x / cellSize));
		const int cy = static_cast<int>(std::floor(y / cellSize));
		return ((cx * 73856093) ^ (cy * 19349663)) & (cellBuckets - 1);
	}

	static int frontier[maxRawNodes + 1];
	static int frontierHead = 0;
	static int frontierTail = 0;


	unsigned char removeStage = RemovedNever;

	const char* RemoveStageName(unsigned char stage)
	{
		static const char* const nameTable[RemoveStageCount] = {
			"alive", "contracted", "pocket-trimmed", "spur-trimmed",
			"island-culled", "trap-culled", "stranded-culled", "hurt-volume",
		};
		if (stage >= RemoveStageCount)
		{
			return "unknown";
		}
		return nameTable[stage];
	}


	void ResetGraph()
	{
		rawCount = 0;
		removeStage = RemovedNever;
		statNodeCapHits = 0;
		statLinkCapHits = 0;
		refusedLinkCount = 0;
		frontierHead = 0;
		frontierTail = 0;
		for (int i = 0; i < cellBuckets; ++i)
		{
			cellHead[i] = -1;
		}
	}


	bool PopFrontier(int* outNode)
	{
		if (frontierHead == frontierTail)
		{
			return false;
		}
		*outNode = frontier[frontierHead];
		frontierHead = (frontierHead + 1) % (maxRawNodes + 1);
		return true;
	}


	bool HasLink(int from, int to)
	{
		for (int i = 0; i < rawNodes[from].linkCount; ++i)
		{
			if (rawNodes[from].links[i] == to)
			{
				return true;
			}
		}
		return false;
	}

	void AddLink(int from, int to, Waypoints::LinkKind kind, unsigned char flags)
	{
		if (from == to || HasLink(from, to))
		{
			return;
		}
		if (rawNodes[from].linkCount >= maxNodeLinks)
		{
			++statLinkCapHits;
			if (refusedLinkCount < maxRefusedLinks)
			{
				RefusedLink& refused = refusedLinks[refusedLinkCount++];
				for (int axis = 0; axis < 3; ++axis)
				{
					refused.from[axis] = rawNodes[from].origin[axis];
					refused.to[axis] = rawNodes[to].origin[axis];
				}
				refused.kind = kind;
			}
			return;
		}
		rawNodes[from].linkKind[rawNodes[from].linkCount] = kind;
		rawNodes[from].linkFlags[rawNodes[from].linkCount] = flags;
		rawNodes[from].links[rawNodes[from].linkCount++] = static_cast<unsigned short>(to);
	}

	void RemoveLink(int from, int to)
	{
		RawNode& node = rawNodes[from];
		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.links[i] == to)
			{
				--node.linkCount;
				node.links[i] = node.links[node.linkCount];
				node.linkKind[i] = node.linkKind[node.linkCount];
				node.linkFlags[i] = node.linkFlags[node.linkCount];
				return;
			}
		}
	}

	unsigned char LinkFlagsOf(int from, int to)
	{
		const RawNode& node = rawNodes[from];
		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.links[i] == to)
			{
				return node.linkFlags[i];
			}
		}
		return 0;
	}

	void SetLinkFlags(int from, int to, unsigned char flags)
	{
		RawNode& node = rawNodes[from];
		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.links[i] == to)
			{
				node.linkFlags[i] = flags;
				return;
			}
		}
	}

	Waypoints::LinkKind LinkKindOf(int from, int to)
	{
		const RawNode& node = rawNodes[from];
		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.links[i] == to)
			{
				return static_cast<Waypoints::LinkKind>(node.linkKind[i]);
			}
		}
		return Waypoints::LinkWalk;
	}

	bool LinkIfWalkable(int a, int b)
	{
		const float* from = rawNodes[a].origin;
		const float* to = rawNodes[b].origin;
		if (CanWalk(from, to))
		{
			AddLink(a, b);
			return true;
		}
		if (CanWalkCrouched(from, to))
		{
			AddLink(a, b, Waypoints::LinkWalk, RawLinkCrouch);
			return true;
		}
		return false;
	}

	int FindNearbyNodeWithin(float x, float y, float z, float heightTolerance)
	{
		const float half = cellSize * 0.5f;
		for (int ox = -1; ox <= 1; ++ox)
		{
			for (int oy = -1; oy <= 1; ++oy)
			{
				for (int i = cellHead[CellIndex(x + ox * half, y + oy * half)]; i >= 0; i = cellNext[i])
				{
					const float dx = rawNodes[i].origin[0] - x;
					const float dy = rawNodes[i].origin[1] - y;
					const float dz = rawNodes[i].origin[2] - z;
					if ((dx * dx + dy * dy) < (dedupRadius * dedupRadius)
						&& std::fabs(dz) < heightTolerance)
					{
						return i;
					}
				}
			}
		}
		return -1;
	}

	int FindNearbyNode(float x, float y, float z)
	{
		return FindNearbyNodeWithin(x, y, z, dedupHeight);
	}

	int FindOtherNearbyNode(float x, float y, float z, int excludedNode)
	{
		const float half = cellSize * 0.5f;
		for (int ox = -1; ox <= 1; ++ox)
		{
			for (int oy = -1; oy <= 1; ++oy)
			{
				for (int i = cellHead[CellIndex(x + ox * half, y + oy * half)]; i >= 0; i = cellNext[i])
				{
					if (i == excludedNode)
					{
						continue;
					}
					const float dx = rawNodes[i].origin[0] - x;
					const float dy = rawNodes[i].origin[1] - y;
					const float dz = rawNodes[i].origin[2] - z;
					if ((dx * dx + dy * dy) < (dedupRadius * dedupRadius) && std::fabs(dz) < dedupHeight)
					{
						return i;
					}
				}
			}
		}
		return -1;
	}

	int CollectNodesWithin(float x, float y, float radius, int* out, int maxOut)
	{
		const int reach = static_cast<int>(radius / cellSize) + 1;
		const float radiusSq = radius * radius;
		int count = 0;
		for (int ox = -reach; ox <= reach; ++ox)
		{
			for (int oy = -reach; oy <= reach; ++oy)
			{
				const int cell = CellIndex(x + ox * cellSize, y + oy * cellSize);
				for (int i = cellHead[cell]; i >= 0 && count < maxOut; i = cellNext[i])
				{
					const float dx = rawNodes[i].origin[0] - x;
					const float dy = rawNodes[i].origin[1] - y;
					if (dx * dx + dy * dy <= radiusSq)
					{
						out[count++] = i;
					}
				}
			}
		}
		return count;
	}

	int AddNode(const GroundHit& ground, float x, float y)
	{
		if (rawCount >= maxRawNodes)
		{
			++statNodeCapHits;
			return -1;
		}

		RawNode& node = rawNodes[rawCount];
		node = {};
		node.origin[0] = x;
		node.origin[1] = y;
		node.origin[2] = ground.z;
		node.type = ground.needsCrouch ? Waypoints::NodeCrouch : Waypoints::NodeStand;

		const int cell = CellIndex(x, y);
		cellNext[rawCount] = cellHead[cell];
		cellHead[cell] = rawCount;

		frontier[frontierTail] = rawCount;
		frontierTail = (frontierTail + 1) % (maxRawNodes + 1);
		return rawCount++;
	}


	void RemoveNode(int nodeIndex)
	{
		RawNode& node = rawNodes[nodeIndex];
		node.removed = true;
		node.removedBy = removeStage;
		for (int i = 0; i < node.linkCount; ++i)
		{
			RemoveLink(node.links[i], nodeIndex);
		}
		node.linkCount = 0;

		for (int ox = -2; ox <= 2; ++ox)
		{
			for (int oy = -2; oy <= 2; ++oy)
			{
				const int cell = CellIndex(node.origin[0] + ox * cellSize, node.origin[1] + oy * cellSize);
				for (int i = cellHead[cell]; i >= 0; i = cellNext[i])
				{
					if (i != nodeIndex && !rawNodes[i].removed)
					{
						RemoveLink(i, nodeIndex);
					}
				}
			}
		}
	}

	void SweepDanglingLinks()
	{
		for (int i = 0; i < rawCount; ++i)
		{
			RawNode& node = rawNodes[i];
			if (node.removed)
			{
				continue;
			}
			for (int c = node.linkCount - 1; c >= 0; --c)
			{
				if (rawNodes[node.links[c]].removed)
				{
					RemoveLink(i, node.links[c]);
				}
			}
		}
	}


	float Distance3(const float* from, const float* to)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float dz = to[2] - from[2];
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	float Distance2(const float* from, const float* to)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		return std::sqrt(dx * dx + dy * dy);
	}
}
