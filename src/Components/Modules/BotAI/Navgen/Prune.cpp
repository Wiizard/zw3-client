#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include <algorithm>
#include <cmath>

namespace Components::BotAI::Navgen
{
	static bool HasNearbyPath(int from, int to, int excluded, float budget, int depth)
	{
		if (from == to)
		{
			return true;
		}
		if (depth == 0)
		{
			return false;
		}

		const RawNode& node = rawNodes[from];
		for (int i = 0; i < node.linkCount; ++i)
		{
			const int next = node.links[i];
			if (next == excluded || node.linkKind[i] != Waypoints::LinkWalk)
			{
				continue;
			}

			const float hop = Distance3(node.origin, rawNodes[next].origin);
			if (hop > budget)
			{
				continue;
			}

			if (HasNearbyPath(next, to, excluded, budget - hop, depth - 1))
			{
				return true;
			}
		}
		return false;
	}


	static bool TryContract(int nodeIndex, float spanMax, float coverageRadius)
	{
		RawNode& node = rawNodes[nodeIndex];
		if (node.removed || node.linkCount < 2)
		{
			return false;
		}
		if (node.isSeed)
		{
			return false;
		}

		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.linkKind[i] != Waypoints::LinkWalk || !HasLink(node.links[i], nodeIndex)
				|| LinkKindOf(node.links[i], nodeIndex) != Waypoints::LinkWalk)
			{
				return false;
			}
		}

		float nearestSq = coverageRadius * coverageRadius + 1.0f;
		for (int i = 0; i < node.linkCount; ++i)
		{
			const float dx = rawNodes[node.links[i]].origin[0] - node.origin[0];
			const float dy = rawNodes[node.links[i]].origin[1] - node.origin[1];
			const float distanceSq = dx * dx + dy * dy;
			if (distanceSq < nearestSq)
			{
				nearestSq = distanceSq;
			}
		}
		if (nearestSq > coverageRadius * coverageRadius)
		{
			return false;
		}

		unsigned short pairA[66];
		unsigned short pairB[66];
		int neededCount = 0;
		int addedPerSlot[maxNodeLinks] = {};

		for (int i = 0; i < node.linkCount; ++i)
		{
			for (int j = i + 1; j < node.linkCount; ++j)
			{
				const int a = node.links[i];
				const int b = node.links[j];
				if (HasLink(a, b) && LinkKindOf(a, b) == Waypoints::LinkWalk
					&& HasLink(b, a) && LinkKindOf(b, a) == Waypoints::LinkWalk)
				{
					continue;
				}

				const float direct = Distance3(rawNodes[a].origin, rawNodes[b].origin);
				if (HasNearbyPath(a, b, nodeIndex, direct * 2.5f, 3)
					&& HasNearbyPath(b, a, nodeIndex, direct * 2.5f, 3))
				{
					continue;
				}

				const float dx = rawNodes[a].origin[0] - rawNodes[b].origin[0];
				const float dy = rawNodes[a].origin[1] - rawNodes[b].origin[1];
				if ((dx * dx + dy * dy) > (spanMax * spanMax))
				{
					return false;
				}

				++addedPerSlot[i];
				++addedPerSlot[j];
				if (rawNodes[a].linkCount - 1 + addedPerSlot[i] > maxNodeLinks
					|| rawNodes[b].linkCount - 1 + addedPerSlot[j] > maxNodeLinks)
				{
					return false;
				}

				if (!CanWalk(rawNodes[a].origin, rawNodes[b].origin)
					|| !CanWalk(rawNodes[b].origin, rawNodes[a].origin))
				{
					return false;
				}

				pairA[neededCount] = static_cast<unsigned short>(a);
				pairB[neededCount] = static_cast<unsigned short>(b);
				++neededCount;
			}
		}

		RemoveNode(nodeIndex);
		for (int i = 0; i < neededCount; ++i)
		{
			AddLink(pairA[i], pairB[i]);
			AddLink(pairB[i], pairA[i]);
		}
		return true;
	}

	static bool isLattice[maxRawNodes];

	static void SelectLattice(float spacing)
	{
		static int order[maxRawNodes];
		static int cellX[maxRawNodes];
		static int cellY[maxRawNodes];
		int count = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			isLattice[i] = false;
			if (rawNodes[i].removed)
			{
				continue;
			}
			cellX[i] = static_cast<int>(std::floor(rawNodes[i].origin[0] / spacing));
			cellY[i] = static_cast<int>(std::floor(rawNodes[i].origin[1] / spacing));
			order[count++] = i;
		}
		std::sort(order, order + count, [](int a, int b)
		{
			if (cellX[a] != cellX[b])
			{
				return cellX[a] < cellX[b];
			}
			if (cellY[a] != cellY[b])
			{
				return cellY[a] < cellY[b];
			}
			return rawNodes[a].origin[2] < rawNodes[b].origin[2];
		});

		int runStart = 0;
		while (runStart < count)
		{
			const int first = order[runStart];
			int runEnd = runStart;
			while (runEnd < count && cellX[order[runEnd]] == cellX[first] && cellY[order[runEnd]] == cellY[first])
			{
				++runEnd;
			}
			const float centreX = (static_cast<float>(cellX[first]) + 0.5f) * spacing;
			const float centreY = (static_cast<float>(cellY[first]) + 0.5f) * spacing;

			int levelStart = runStart;
			while (levelStart < runEnd)
			{
				const float levelZ = rawNodes[order[levelStart]].origin[2];
				int best = -1;
				float bestSq = 0.0f;
				int levelEnd = levelStart;
				while (levelEnd < runEnd && rawNodes[order[levelEnd]].origin[2] - levelZ <= dedupHeight)
				{
					const int node = order[levelEnd];
					const float dx = rawNodes[node].origin[0] - centreX;
					const float dy = rawNodes[node].origin[1] - centreY;
					const float distanceSq = dx * dx + dy * dy;
					if (best < 0 || distanceSq < bestSq)
					{
						best = node;
						bestSq = distanceSq;
					}
					++levelEnd;
				}
				isLattice[best] = true;
				levelStart = levelEnd;
			}
			runStart = runEnd;
		}
	}

	static bool IsContractionProof(int nodeIndex)
	{
		const RawNode& node = rawNodes[nodeIndex];
		if (node.isSeed || node.linkCount < 2)
		{
			return true;
		}
		for (int i = 0; i < node.linkCount; ++i)
		{
			if (node.linkKind[i] != Waypoints::LinkWalk || !HasLink(node.links[i], nodeIndex)
				|| LinkKindOf(node.links[i], nodeIndex) != Waypoints::LinkWalk)
			{
				return true;
			}
		}
		return false;
	}

	static int HeldCount(int* outLattice)
	{
		int held = 0;
		int lattice = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			if (isLattice[i])
			{
				++lattice;
			}
			if (isLattice[i] || IsContractionProof(i))
			{
				++held;
			}
		}
		*outLattice = lattice;
		return held;
	}


	static int ContractPass(float spanMax, float coverageRadius)
	{
		int removed = 0;
		int pass = 0;
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (int i = 0; i < rawCount; ++i)
			{
				if (isLattice[i])
				{
					continue;
				}
				if (TryContract(i, spanMax, coverageRadius))
				{
					++removed;
					changed = true;
				}
			}
			Report("navgen: prune pass %d, %d removed, %d traces...\n",
						   ++pass, removed, traceCount);
		}
		return removed;
	}

	static int LiveNodeCount()
	{
		int live = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (!rawNodes[i].removed)
			{
				++live;
			}
		}
		return live;
	}

	static bool IsMantleLink(int from, int to)
	{
		return LinkKindOf(from, to) == Waypoints::LinkMantle;
	}


	static bool HasWalkRoute(int from, int to, float budget, bool avoidCrouch)
	{
		static float bestCost[maxRawNodes];
		static int seenStamp[maxRawNodes];
		static int doneStamp[maxRawNodes];
		static int open[maxRawNodes];
		static int stamp = 0;
		++stamp;

		int openCount = 0;
		open[openCount++] = from;
		seenStamp[from] = stamp;
		bestCost[from] = 0.0f;

		while (openCount > 0)
		{
			int cheapest = 0;
			for (int i = 1; i < openCount; ++i)
			{
				if (bestCost[open[i]] < bestCost[open[cheapest]])
				{
					cheapest = i;
				}
			}
			const int current = open[cheapest];
			open[cheapest] = open[--openCount];
			if (current == to)
			{
				return true;
			}
			doneStamp[current] = stamp;

			const RawNode& node = rawNodes[current];
			for (int c = 0; c < node.linkCount; ++c)
			{
				const int next = node.links[c];
				if (rawNodes[next].removed || doneStamp[next] == stamp || IsMantleLink(current, next))
				{
					continue;
				}
				if (avoidCrouch && (node.linkFlags[c] & RawLinkCrouch) != 0)
				{
					continue;
				}

				const float cost = bestCost[current] + Distance3(node.origin, rawNodes[next].origin);
				if (cost > budget)
				{
					continue;
				}
				if (seenStamp[next] == stamp && cost >= bestCost[next])
				{
					continue;
				}
				if (seenStamp[next] != stamp)
				{
					seenStamp[next] = stamp;
					open[openCount++] = next;
				}
				bestCost[next] = cost;
			}
		}
		return false;
	}


	int TrimMantleShortcuts()
	{
		int linksRemoved = 0;
		int risesRemoved = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}

			for (int c = 0; c < rawNodes[i].linkCount;)
			{
				const int target = rawNodes[i].links[c];
				if (!IsMantleLink(i, target))
				{
					++c;
					continue;
				}

				float riseCap = mantleUpMax;
				if (rawNodes[i].linkFlags[c] & RawLinkJumpFirst)
				{
					riseCap = mantleJumpReliableMax;
				}
				if (rawNodes[target].origin[2] - rawNodes[i].origin[2] > riseCap)
				{
					RemoveLink(i, target);
					++risesRemoved;
					continue;
				}

				float budget = Distance3(rawNodes[i].origin, rawNodes[target].origin) * mantleDetourMulti;
				if (budget < mantleDetourMin)
				{
					budget = mantleDetourMin;
				}
				if (!HasWalkRoute(i, target, budget, false))
				{
					++c;
					continue;
				}
				RemoveLink(i, target);
				++linksRemoved;
			}
		}
		Report("navgen: mantle rise trim: %d link(s) rising past %.0f dropped\n", risesRemoved, mantleUpMax);
		return linksRemoved;
	}


	int TrimMantleDuplicates()
	{
		int linksRemoved = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount;)
			{
				const int target = rawNodes[i].links[c];
				if (!IsMantleLink(i, target))
				{
					++c;
					continue;
				}

				bool isDuplicate = false;
				for (int d = 0; d < rawNodes[i].linkCount && !isDuplicate; ++d)
				{
					const int other = rawNodes[i].links[d];
					if (d == c || !IsMantleLink(i, other))
					{
						continue;
					}
					if (Distance2(rawNodes[target].origin, rawNodes[other].origin) > 60.0f
						|| std::fabs(rawNodes[target].origin[2] - rawNodes[other].origin[2]) > 20.0f)
					{
						continue;
					}
					const float rise = rawNodes[target].origin[2] - rawNodes[i].origin[2];
					const float otherRise = rawNodes[other].origin[2] - rawNodes[i].origin[2];
					if (rise > otherRise || (rise == otherRise && target > other))
					{
						isDuplicate = true;
					}
				}
				if (!isDuplicate)
				{
					++c;
					continue;
				}
				RemoveLink(i, target);
				++linksRemoved;
			}
		}
		return linksRemoved;
	}


	int TrimCrouchShortcuts()
	{
		int linksRemoved = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount;)
			{
				const int target = rawNodes[i].links[c];
				if ((rawNodes[i].linkFlags[c] & RawLinkCrouch) == 0)
				{
					++c;
					continue;
				}

				float budget = Distance3(rawNodes[i].origin, rawNodes[target].origin) * crouchDetourMulti;
				if (budget < crouchDetourMin)
				{
					budget = crouchDetourMin;
				}
				if (!HasWalkRoute(i, target, budget, true))
				{
					++c;
					continue;
				}
				RemoveLink(i, target);
				++linksRemoved;
			}
		}
		return linksRemoved;
	}


	int TrimMantlePockets()
	{
		removeStage = RemovedPocket;
		static int regionOf[maxRawNodes];
		static int regionSize[maxRawNodes];
		static bool regionHasSeed[maxRawNodes];
		static bool regionHasEntry[maxRawNodes];
		static int regionMantleSource[maxRawNodes];
		static bool regionExitsElsewhere[maxRawNodes];
		for (int i = 0; i < rawCount; ++i)
		{
			regionOf[i] = i;
			regionSize[i] = 0;
			regionHasSeed[i] = false;
			regionHasEntry[i] = false;
			regionMantleSource[i] = -1;
			regionExitsElsewhere[i] = false;
		}
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount; ++c)
			{
				const int target = rawNodes[i].links[c];
				if (rawNodes[i].linkKind[c] != Waypoints::LinkWalk
					|| LinkKindOf(target, i) != Waypoints::LinkWalk)
				{
					continue;
				}
				int a = i;
				int b = target;
				while (regionOf[a] != a) { a = regionOf[a]; }
				while (regionOf[b] != b) { b = regionOf[b]; }
				if (a != b)
				{
					regionOf[b] = a;
				}
			}
		}
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			int root = i;
			while (regionOf[root] != root) { root = regionOf[root]; }
			regionOf[i] = root;
			++regionSize[root];
			if (rawNodes[i].isSeed)
			{
				regionHasSeed[root] = true;
			}
		}
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount; ++c)
			{
				const int target = rawNodes[i].links[c];
				if (regionOf[target] == regionOf[i])
				{
					continue;
				}
				if (!IsMantleLink(i, target))
				{
					regionHasEntry[regionOf[target]] = true;
					continue;
				}
				const int source = regionOf[i];
				int& recorded = regionMantleSource[regionOf[target]];
				if (recorded == -1)
				{
					recorded = source;
				}
				else if (recorded != source)
				{
					recorded = -2;
				}
			}
		}

		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount; ++c)
			{
				const int target = rawNodes[i].links[c];
				if (regionOf[target] != regionOf[i] && regionMantleSource[regionOf[i]] != regionOf[target])
				{
					regionExitsElsewhere[regionOf[i]] = true;
				}
			}
		}

		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed || regionOf[i] != i)
			{
				continue;
			}
			if (regionHasSeed[i] || regionHasEntry[i])
			{
				continue;
			}
			const bool isPropTop = regionSize[i] < pocketMax && !regionExitsElsewhere[i];
			const bool isDeadEnd = regionSize[i] < vantageMin && !regionExitsElsewhere[i];
			if (isPropTop || isDeadEnd)
			{
				const char* why = "prop top";
				if (!isPropTop)
				{
					why = "mantle dead end";
				}
				ReportFile("navgen:   pocket region of %d node(s) at %.0f %.0f %.0f removed (%s)\n",
						   regionSize[i], rawNodes[i].origin[0], rawNodes[i].origin[1],
						   rawNodes[i].origin[2], why);
			}
		}

		int nodesRemoved = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			const int root = regionOf[i];
			if (regionHasSeed[root] || regionHasEntry[root])
			{
				continue;
			}
			const bool isPropTop = regionSize[root] < pocketMax && !regionExitsElsewhere[root];
			const bool isDeadEnd = regionSize[root] < vantageMin && !regionExitsElsewhere[root];
			if (isPropTop || isDeadEnd)
			{
				RemoveNode(i);
				++nodesRemoved;
			}
		}
		return nodesRemoved;
	}


	static constexpr int bridgePairs = 48;
	static constexpr int bridgeSteps = 8;
	static constexpr int bridgeFailuresShown = 5;
	static constexpr int trapBridgeAttempts = 12;

	struct BridgePair
	{
		int rim;
		int mass;
		float distance;
	};

	static bool bridgeSource[maxRawNodes];
	static bool bridgeTarget[maxRawNodes];
	static bool trapSeen[maxRawNodes];

	static bool bridgeReachBack[maxRawNodes];
	static bool bridgeProtected[maxRawNodes];
	static int strandedSeeds[64];
	static int strandedSeedCount = 0;

	bool IsProtectedRaw(int nodeIndex)
	{
		return bridgeProtected[nodeIndex];
	}

	static bool TryBridgeChain(int from, int to, bool isTwoWay, char* failure, int failureSize)
	{
		int added[bridgeSteps];
		int addedCount = 0;
		int linkFrom[bridgeSteps * 2];
		int linkTo[bridgeSteps * 2];
		int linkCount = 0;

		const float* start = rawNodes[from].origin;
		const float* target = rawNodes[to].origin;
		int steps = static_cast<int>(Distance2(start, target) / gridStep) + 1;
		if (steps > bridgeSteps)
		{
			steps = bridgeSteps;
		}

		int previous = from;
		bool passed = true;
		for (int step = 1; step <= steps; ++step)
		{
			const float* prev = rawNodes[previous].origin;
			int node = to;
			if (step < steps)
			{
				const float t = static_cast<float>(step) / static_cast<float>(steps);
				const float x = start[0] + (target[0] - start[0]) * t;
				const float y = start[1] + (target[1] - start[1]) * t;
				GroundHit ground = SnapToGround(x, y, prev[2], 80.0f);
				if (!ground.isValid)
				{
					ground = SnapToGround(x, y, prev[2] - 100.0f, 220.0f);
				}
				if (!ground.isValid)
				{
					const char* why = "no floor";
					if (ground.isRayStartSolid)
					{
						why = "inside solid";
					}
					_snprintf_s(failure, failureSize, _TRUNCATE, "step %d at %.0f %.0f: %s", step, x, y, why);
					passed = false;
					break;
				}
				if (!HasPathFooting(x, y, ground.z))
				{
					_snprintf_s(failure, failureSize, _TRUNCATE, "step %d at %.0f %.0f %.0f: no footing", step, x, y, ground.z);
					passed = false;
					break;
				}
				if (!HasShoulderRoom(x, y, ground.z, ground.needsCrouch))
				{
					_snprintf_s(failure, failureSize, _TRUNCATE, "step %d at %.0f %.0f %.0f: no shoulder room", step, x, y, ground.z);
					passed = false;
					break;
				}
				const float point[3] = { x, y, ground.z };
				if (IsInHurtVolume(point))
				{
					_snprintf_s(failure, failureSize, _TRUNCATE, "step %d at %.0f %.0f %.0f: in a hurt volume", step, x, y, ground.z);
					passed = false;
					break;
				}

				node = FindNearbyNode(x, y, ground.z);
				if (node >= 0 && rawNodes[node].removed)
				{
					node = -1;
				}
				if (node < 0)
				{
					node = AddNode(ground, x, y);
					if (node < 0)
					{
						_snprintf_s(failure, failureSize, _TRUNCATE, "step %d: the raw node cap", step);
						passed = false;
						break;
					}
					added[addedCount++] = node;
				}
			}
			if (node == previous)
			{
				continue;
			}

			const float* next = rawNodes[node].origin;
			Waypoints::LinkKind kind = Waypoints::LinkWalk;
			unsigned char flags = 0;
			bool isJoined = false;
			if (CanWalk(prev, next))
			{
				isJoined = true;
			}
			else if (CanWalkCrouched(prev, next))
			{
				isJoined = true;
				flags = RawLinkCrouch;
			}
			else if (!isTwoWay && CanDrop(prev, next))
			{
				isJoined = true;
				kind = Waypoints::LinkDrop;
			}
			if (!isJoined)
			{
				_snprintf_s(failure, failureSize, _TRUNCATE, "step %d to %.0f %.0f %.0f: not walkable (rise %+.0f over %.0f)",
							step, next[0], next[1], next[2], next[2] - prev[2], Distance2(prev, next));
				passed = false;
				break;
			}
			if (isTwoWay && !CanWalk(next, prev) && !CanWalkCrouched(next, prev))
			{
				_snprintf_s(failure, failureSize, _TRUNCATE, "step %d from %.0f %.0f %.0f: not walkable back", step, next[0], next[1], next[2]);
				passed = false;
				break;
			}

			if (!HasLink(previous, node))
			{
				AddLink(previous, node, kind, flags);
				linkFrom[linkCount] = previous;
				linkTo[linkCount++] = node;
			}
			const bool hadBack = HasLink(node, previous);
			if (isTwoWay)
			{
				AddLink(node, previous);
			}
			else
			{
				LinkIfWalkable(node, previous);
			}
			if (!hadBack && HasLink(node, previous))
			{
				linkFrom[linkCount] = node;
				linkTo[linkCount++] = previous;
			}
			previous = node;
		}
		if (passed)
		{
			return true;
		}

		for (int i = 0; i < linkCount; ++i)
		{
			RemoveLink(linkFrom[i], linkTo[i]);
		}
		for (int i = 0; i < addedCount; ++i)
		{
			RemoveNode(added[i]);
		}
		return false;
	}

	static int BridgeRegion(bool isTwoWay, const char* label)
	{
		static int nearNodes[4096];
		BridgePair pairs[bridgePairs];
		int pairCount = 0;
		int rimCount = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed || !bridgeSource[i] || rawNodes[i].linkCount > bridgeRimLinks)
			{
				continue;
			}
			++rimCount;
			const int nearCount = CollectNodesWithin(rawNodes[i].origin[0], rawNodes[i].origin[1], bridgeReach,
													 nearNodes, 4096);
			for (int n = 0; n < nearCount; ++n)
			{
				const int candidate = nearNodes[n];
				if (rawNodes[candidate].removed || !bridgeTarget[candidate]
					|| std::fabs(rawNodes[candidate].origin[2] - rawNodes[i].origin[2]) > bridgeRiseMax)
				{
					continue;
				}

				const float distance = Distance3(rawNodes[i].origin, rawNodes[candidate].origin);
				int slot = pairCount;
				if (pairCount < bridgePairs)
				{
					++pairCount;
				}
				else if (distance >= pairs[pairCount - 1].distance)
				{
					continue;
				}
				else
				{
					slot = pairCount - 1;
				}
				while (slot > 0 && pairs[slot - 1].distance > distance)
				{
					pairs[slot] = pairs[slot - 1];
					--slot;
				}
				pairs[slot].rim = i;
				pairs[slot].mass = candidate;
				pairs[slot].distance = distance;
			}
		}
		if (pairCount == 0)
		{
			Report("navgen:   %s: no mass node within %.0f xy / %.0f z of its %d rim node(s)\n",
				   label, bridgeReach, bridgeRiseMax, rimCount);
			return -1;
		}

		char failures[bridgeFailuresShown][128];
		int failureCount = 0;
		for (int p = 0; p < pairCount; ++p)
		{
			char failure[96] = "";
			const int before = rawCount;
			if (TryBridgeChain(pairs[p].rim, pairs[p].mass, isTwoWay, failure, sizeof(failure)))
			{
				Report("navgen:   %s: bridged, node %d -> %d over %.0f units, %d node(s) added\n",
					   label, pairs[p].rim, pairs[p].mass, pairs[p].distance, rawCount - before);
				return rawCount - before;
			}
			if (failureCount < bridgeFailuresShown)
			{
				_snprintf_s(failures[failureCount], sizeof(failures[failureCount]), _TRUNCATE,
							"%d -> %d (%.0f): %s", pairs[p].rim, pairs[p].mass, pairs[p].distance, failure);
				++failureCount;
			}
		}
		Report("navgen:   %s: no bridge, %d pair(s) tried from %d rim node(s); the nearest failed at:\n",
			   label, pairCount, rimCount);
		for (int f = 0; f < failureCount; ++f)
		{
			Report("navgen:     %s\n", failures[f]);
		}
		return -1;
	}

	int BridgeStrandedRegions()
	{
		static int queue[maxRawNodes];
		int bridged = 0;
		for (int s = 0; s < strandedSeedCount; ++s)
		{
			const int seed = strandedSeeds[s];
			if (rawNodes[seed].removed)
			{
				continue;
			}

			for (int i = 0; i < rawCount; ++i)
			{
				bridgeSource[i] = false;
				bridgeTarget[i] = !rawNodes[i].removed && bridgeReachBack[i] && !bridgeProtected[i];
			}

			int head = 0;
			int tail = 0;
			int size = 0;
			bool isBridgedAlready = false;
			float low[3] = { 0.0f, 0.0f, 0.0f };
			float high[3] = { 0.0f, 0.0f, 0.0f };
			bridgeSource[seed] = true;
			queue[tail++] = seed;
			while (head < tail)
			{
				const int current = queue[head++];
				const RawNode& node = rawNodes[current];
				for (int axis = 0; axis < 3; ++axis)
				{
					if (size == 0 || node.origin[axis] < low[axis])
					{
						low[axis] = node.origin[axis];
					}
					if (size == 0 || node.origin[axis] > high[axis])
					{
						high[axis] = node.origin[axis];
					}
				}
				++size;
				if (bridgeReachBack[current])
				{
					isBridgedAlready = true;
				}
				for (int c = 0; c < node.linkCount; ++c)
				{
					const int next = node.links[c];
					if (!bridgeSource[next] && !rawNodes[next].removed)
					{
						bridgeSource[next] = true;
						queue[tail++] = next;
					}
				}
			}

			Report("navgen: stranded region of the spawn at %.0f %.0f %.0f: %d node(s) within %.0f %.0f %.0f .. %.0f %.0f %.0f\n",
				   rawNodes[seed].origin[0], rawNodes[seed].origin[1], rawNodes[seed].origin[2], size,
				   low[0], low[1], low[2], high[0], high[1], high[2]);
			if (isBridgedAlready)
			{
				Report("navgen:   reaches an earlier bridge\n");
				continue;
			}

			char label[64];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "spawn at %.0f %.0f %.0f",
						rawNodes[seed].origin[0], rawNodes[seed].origin[1], rawNodes[seed].origin[2]);
			if (BridgeRegion(false, label) < 0)
			{
				continue;
			}
			++bridged;
			for (int i = 0; i < rawCount; ++i)
			{
				if (bridgeSource[i])
				{
					bridgeReachBack[i] = true;
				}
			}
		}
		return bridged;
	}


	static bool HasGroundPastTip(int tip, int partner)
	{
		const float* from = rawNodes[partner].origin;
		const float* to = rawNodes[tip].origin;
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float run = std::sqrt(dx * dx + dy * dy);
		if (run < 1.0f)
		{
			return false;
		}
		const float x = to[0] + dx / run * gridStep;
		const float y = to[1] + dy / run * gridStep;
		const GroundHit ground = SnapToGround(x, y, to[2], 80.0f);
		return ground.isValid;
	}

	static void UnionFindAll(int* parent)
	{
		for (int i = 0; i < rawCount; ++i)
		{
			parent[i] = i;
		}
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int c = 0; c < rawNodes[i].linkCount; ++c)
			{
				int a = i;
				int b = rawNodes[i].links[c];
				while (parent[a] != a) { a = parent[a]; }
				while (parent[b] != b) { b = parent[b]; }
				if (a != b)
				{
					parent[b] = a;
				}
			}
		}
		for (int i = 0; i < rawCount; ++i)
		{
			int root = i;
			while (parent[root] != root) { root = parent[root]; }
			parent[i] = root;
		}
	}


	int PruneGraph()
	{
		static constexpr int stageCount = sizeof(latticeSpacing) / sizeof(latticeSpacing[0]);
		int stage = 0;
		int latticeCount = 0;
		for (; stage < stageCount - 1; ++stage)
		{
			SelectLattice(latticeSpacing[stage]);
			if (HeldCount(&latticeCount) <= maxFinalNodes)
			{
				break;
			}
		}

		removeStage = RemovedContract;
		int removed = 0;
		for (; stage < stageCount; ++stage)
		{
			SelectLattice(latticeSpacing[stage]);
			const int held = HeldCount(&latticeCount);
			Report("navgen: lattice %.0f: %d node(s) held (%d on the lattice, %d spawns and climb ends)\n",
				   latticeSpacing[stage], held, latticeCount, held - latticeCount);
			removed += ContractPass(latticeSpacing[stage] * latticeSpanScale, latticeSpacing[stage]);
			if (LiveNodeCount() <= maxFinalNodes)
			{
				break;
			}
		}

		removed += TrimMantlePockets();

		removeStage = RemovedSpur;
		int spursRemoved = 0;
		int stairsKept = 0;
		bool trimmed = true;
		while (trimmed)
		{
			trimmed = false;
			for (int i = 0; i < rawCount; ++i)
			{
				if (rawNodes[i].removed || rawNodes[i].linkCount != 1 || rawNodes[i].isSeed)
				{
					continue;
				}
				const int partner = rawNodes[i].links[0];
				const float rise = rawNodes[i].origin[2] - rawNodes[partner].origin[2];
				if (rise <= 20.0f)
				{
					continue;
				}

				if (rise <= 2.0f * stepUpMax && HasGroundPastTip(i, partner))
				{
					++stairsKept;
					continue;
				}
				RemoveNode(i);
				++removed;
				++spursRemoved;
				trimmed = true;
			}
		}
		Report("navgen: spur trim: %d climb-up dead end(s) removed, %d stair tip(s) kept\n", spursRemoved, stairsKept);

		removeStage = RemovedIsland;
		static int islandOf[maxRawNodes];
		static int islandSize[maxRawNodes];
		static bool islandHasSeed[maxRawNodes];
		static float islandSum[maxRawNodes][3];
		UnionFindAll(islandOf);
		for (int i = 0; i < rawCount; ++i)
		{
			islandSize[i] = 0;
			islandHasSeed[i] = false;
			islandSum[i][0] = 0.0f;
			islandSum[i][1] = 0.0f;
			islandSum[i][2] = 0.0f;
		}
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			const int root = islandOf[i];
			++islandSize[root];
			islandSum[root][0] += rawNodes[i].origin[0];
			islandSum[root][1] += rawNodes[i].origin[1];
			islandSum[root][2] += rawNodes[i].origin[2];
			if (rawNodes[i].isSeed)
			{
				islandHasSeed[root] = true;
			}
		}
		int largestIsland = 0;
		int largestRoot = -1;
		for (int i = 0; i < rawCount; ++i)
		{
			if (islandSize[i] > largestIsland)
			{
				largestIsland = islandSize[i];
				largestRoot = i;
			}
		}

		int islandFloor = largestIsland / 8;
		if (islandFloor < 8)
		{
			islandFloor = 8;
		}
		for (int root = 0; root < rawCount; ++root)
		{
			if (islandSize[root] == 0 || islandSize[root] >= islandFloor)
			{
				continue;
			}
			const float inv = 1.0f / static_cast<float>(islandSize[root]);

			if (largestRoot >= 0 && (islandSize[root] >= islandMin || islandHasSeed[root]))
			{
				for (int i = 0; i < rawCount; ++i)
				{
					bridgeSource[i] = !rawNodes[i].removed && islandOf[i] == root;
					bridgeTarget[i] = !rawNodes[i].removed && islandOf[i] == largestRoot;
				}
				char label[64];
				_snprintf_s(label, sizeof(label), _TRUNCATE, "island of %d nodes near %.0f %.0f %.0f",
							islandSize[root], islandSum[root][0] * inv, islandSum[root][1] * inv,
							islandSum[root][2] * inv);
				if (BridgeRegion(true, label) >= 0)
				{
					continue;
				}
			}
			if (islandHasSeed[root])
			{
				Report("navgen: WARNING island of %d nodes near %.0f %.0f %.0f holds a spawn but "
							   "is not joined to the main graph; kept, check the stairs/mantle there\n",
							   islandSize[root], islandSum[root][0] * inv, islandSum[root][1] * inv,
							   islandSum[root][2] * inv);
				continue;
			}
			Report("navgen: culled island of %d nodes near %.0f %.0f %.0f\n",
						   islandSize[root], islandSum[root][0] * inv, islandSum[root][1] * inv,
						   islandSum[root][2] * inv);
			for (int i = 0; i < rawCount; ++i)
			{
				if (!rawNodes[i].removed && islandOf[i] == root)
				{
					RemoveNode(i);
					++removed;
				}
			}
		}

		removeStage = RemovedTrap;
		int liveTotal = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (!rawNodes[i].removed)
			{
				++liveTotal;
			}
		}

		static bool reachForward[maxRawNodes];
		static bool reachBack[maxRawNodes];
		static bool isProtected[maxRawNodes];
		static int reachQueue[maxRawNodes];
		int triedHubs[8];
		int triedCount = 0;
		bool culled = false;
		int sweepMs = 0;
		strandedSeedCount = 0;

		while (triedCount < 8 && !culled)
		{
			strandedSeedCount = 0;
			int hub = -1;
			for (int i = 0; i < rawCount; ++i)
			{
				if (rawNodes[i].removed)
				{
					continue;
				}
				bool tried = false;
				for (int t = 0; t < triedCount; ++t)
				{
					if (triedHubs[t] == i)
					{
						tried = true;
					}
				}
				if (!tried && (hub < 0 || rawNodes[i].linkCount > rawNodes[hub].linkCount))
				{
					hub = i;
				}
			}
			if (hub < 0)
			{
				break;
			}
			triedHubs[triedCount++] = hub;

			for (int i = 0; i < rawCount; ++i)
			{
				reachForward[i] = false;
				reachBack[i] = false;
				isProtected[i] = false;
			}

			int queueHead = 0;
			int queueTail = 0;
			reachForward[hub] = true;
			reachQueue[queueTail++] = hub;
			while (queueHead < queueTail)
			{
				const RawNode& node = rawNodes[reachQueue[queueHead++]];
				for (int c = 0; c < node.linkCount; ++c)
				{
					const int next = node.links[c];
					if (!reachForward[next] && !rawNodes[next].removed)
					{
						reachForward[next] = true;
						reachQueue[queueTail++] = next;
					}
				}
			}

			const int sweepStart = NowMs();
			reachBack[hub] = true;
			bool spread = true;
			while (spread)
			{
				spread = false;
				for (int i = 0; i < rawCount; ++i)
				{
					if (rawNodes[i].removed || reachBack[i])
					{
						continue;
					}
					for (int c = 0; c < rawNodes[i].linkCount; ++c)
					{
						if (reachBack[rawNodes[i].links[c]])
						{
							reachBack[i] = true;
							spread = true;
							break;
						}
					}
				}
			}
			sweepMs += NowMs() - sweepStart;

			int mutualSize = 0;
			for (int i = 0; i < rawCount; ++i)
			{
				if (!rawNodes[i].removed && reachForward[i] && reachBack[i])
				{
					++mutualSize;
				}
			}
			if (mutualSize * 2 < liveTotal)
			{
				continue;
			}

			int strandedSpawns = 0;
			for (int seed = 0; seed < rawCount; ++seed)
			{
				if (rawNodes[seed].removed || !rawNodes[seed].isSeed || reachBack[seed] || isProtected[seed])
				{
					continue;
				}
				++strandedSpawns;
				if (strandedSeedCount < 64)
				{
					strandedSeeds[strandedSeedCount++] = seed;
				}

				int protectedCount = 0;
				queueHead = 0;
				queueTail = 0;
				isProtected[seed] = true;
				reachQueue[queueTail++] = seed;
				while (queueHead < queueTail)
				{
					const RawNode& node = rawNodes[reachQueue[queueHead++]];
					++protectedCount;
					for (int c = 0; c < node.linkCount; ++c)
					{
						const int next = node.links[c];
						if (!isProtected[next] && !rawNodes[next].removed)
						{
							isProtected[next] = true;
							reachQueue[queueTail++] = next;
						}
					}
				}
				Report("navgen: WARNING spawn at %.0f %.0f %.0f cannot reach the main graph; "
							   "its %d node(s) kept, bots spawning there will not get out\n",
							   rawNodes[seed].origin[0], rawNodes[seed].origin[1],
							   rawNodes[seed].origin[2], protectedCount);
			}

			const int preBridgeCount = rawCount;
			int trapRegions = 0;
			bool bridgedAny = false;
			for (int i = 0; i < preBridgeCount; ++i)
			{
				trapSeen[i] = false;
			}
			for (int i = 0; i < preBridgeCount; ++i)
			{
				if (rawNodes[i].removed || reachBack[i] || isProtected[i] || trapSeen[i])
				{
					continue;
				}

				int size = 0;
				float low[3] = { 0.0f, 0.0f, 0.0f };
				float high[3] = { 0.0f, 0.0f, 0.0f };
				queueHead = 0;
				queueTail = 0;
				trapSeen[i] = true;
				reachQueue[queueTail++] = i;
				while (queueHead < queueTail)
				{
					const int current = reachQueue[queueHead++];
					bridgeSource[current] = true;
					for (int axis = 0; axis < 3; ++axis)
					{
						if (size == 0 || rawNodes[current].origin[axis] < low[axis])
						{
							low[axis] = rawNodes[current].origin[axis];
						}
						if (size == 0 || rawNodes[current].origin[axis] > high[axis])
						{
							high[axis] = rawNodes[current].origin[axis];
						}
					}
					++size;
					for (int c = 0; c < rawNodes[current].linkCount; ++c)
					{
						const int next = rawNodes[current].links[c];
						if (next < preBridgeCount && !rawNodes[next].removed && !reachBack[next]
							&& !isProtected[next] && !trapSeen[next])
						{
							trapSeen[next] = true;
							reachQueue[queueTail++] = next;
						}
					}
				}

				++trapRegions;
				if (trapRegions > trapBridgeAttempts)
				{
					for (int n = 0; n < rawCount; ++n)
					{
						bridgeSource[n] = false;
					}
					continue;
				}

				for (int n = 0; n < rawCount; ++n)
				{
					bridgeTarget[n] = !rawNodes[n].removed && reachBack[n];
				}
				char label[128];
				_snprintf_s(label, sizeof(label), _TRUNCATE,
							"trap region of %d node(s) within %.0f %.0f %.0f .. %.0f %.0f %.0f",
							size, low[0], low[1], low[2], high[0], high[1], high[2]);
				if (BridgeRegion(false, label) > 0)
				{
					bridgedAny = true;
				}
				for (int n = 0; n < rawCount; ++n)
				{
					bridgeSource[n] = false;
					bridgeTarget[n] = false;
				}
			}
			if (trapRegions > trapBridgeAttempts)
			{
				Report("navgen: %d trapped region(s) past the %d bridge attempts went straight to the cull\n",
							   trapRegions - trapBridgeAttempts, trapBridgeAttempts);
			}
			if (bridgedAny)
			{
				for (int i = preBridgeCount; i < rawCount; ++i)
				{
					reachBack[i] = false;
					isProtected[i] = false;
				}
				spread = true;
				while (spread)
				{
					spread = false;
					for (int i = 0; i < rawCount; ++i)
					{
						if (rawNodes[i].removed || reachBack[i])
						{
							continue;
						}
						for (int c = 0; c < rawNodes[i].linkCount; ++c)
						{
							if (reachBack[rawNodes[i].links[c]])
							{
								reachBack[i] = true;
								spread = true;
								break;
							}
						}
					}
				}
			}

			int cullCount = 0;
			float cullMin[3] = { 0.0f, 0.0f, 0.0f };
			float cullMax[3] = { 0.0f, 0.0f, 0.0f };
			for (int i = 0; i < rawCount; ++i)
			{
				if (rawNodes[i].removed || reachBack[i] || isProtected[i])
				{
					continue;
				}
				for (int axis = 0; axis < 3; ++axis)
				{
					if (cullCount == 0 || rawNodes[i].origin[axis] < cullMin[axis])
					{
						cullMin[axis] = rawNodes[i].origin[axis];
					}
					if (cullCount == 0 || rawNodes[i].origin[axis] > cullMax[axis])
					{
						cullMax[axis] = rawNodes[i].origin[axis];
					}
				}
				RemoveNode(i);
				++removed;
				++cullCount;
			}
			Report("navgen: trap-pocket cull: mass %d of %d, %d node(s) removed%s, %d stranded spawn(s); %d ms in the backward sweep\n",
						   mutualSize, liveTotal, cullCount, cullCount ? "" : " (none)", strandedSpawns, sweepMs);
			if (cullCount)
			{
				Report("navgen:   removed within %.0f %.0f %.0f .. %.0f %.0f %.0f\n",
							   cullMin[0], cullMin[1], cullMin[2], cullMax[0], cullMax[1], cullMax[2]);
			}
			culled = true;

			for (int i = 0; i < rawCount; ++i)
			{
				bridgeReachBack[i] = reachBack[i];
				bridgeProtected[i] = isProtected[i];
			}
		}

		if (!culled)
		{
			Report("navgen: no dominant region found, trap-pocket cull skipped\n");
			strandedSeedCount = 0;
		}
		else if (strandedSeedCount)
		{
			const int bridged = BridgeStrandedRegions();
			Report("navgen: bridge search: %d of %d stranded region(s) joined to the mass\n", bridged, strandedSeedCount);
		}

		removeStage = RemovedStranded;
		static bool strandedWarned[maxRawNodes];
		for (int i = 0; i < rawCount; ++i)
		{
			strandedWarned[i] = false;
		}
		bool strandedRemoved = true;
		while (strandedRemoved)
		{
			strandedRemoved = false;
			for (int i = 0; i < rawCount; ++i)
			{
				if (rawNodes[i].removed)
				{
					continue;
				}

				int usableExits = 0;
				for (int c = 0; c < rawNodes[i].linkCount; ++c)
				{
					if (!rawNodes[rawNodes[i].links[c]].removed)
					{
						++usableExits;
					}
				}

				if (usableExits == 0 && rawNodes[i].isSeed)
				{
					if (!strandedWarned[i])
					{
						strandedWarned[i] = true;
						Report("navgen: WARNING spawn at %.0f %.0f %.0f has a node with no way out; "
							   "kept, bots spawning there will not get out\n",
							   rawNodes[i].origin[0], rawNodes[i].origin[1], rawNodes[i].origin[2]);
					}
					continue;
				}
				if (usableExits == 0)
				{
					RemoveNode(i);
					++removed;
					strandedRemoved = true;
				}
			}
		}
		SweepDanglingLinks();
		return removed;
	}
}
