#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include <cmath>
#include <cstring>

namespace Components::BotAI::Navgen
{
	static constexpr int maxSpawnClasses = 32;
	static constexpr float sillDepth = 22.0f;

	static constexpr float seedDropDepth = 512.0f;

	static constexpr float directions[8][2] = {
		{ 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
		{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f },
	};


	float seedPositions[maxSeeds][3];
	bool seedIsObjective[maxSeeds];
	int seedCount = 0;
	int playerSeedCount = 0;

	enum SeedGate
	{
		SeedGateNone = 0,
		SeedGateNoFloor,
		SeedGateInClip,
		SeedGateSlope,
		SeedGateNoBodyFit,
		SeedGateHurt,
		SeedGateCap,
		SeedGateCount,
	};

	static int seedGateCounts[SeedGateCount];

	static const char* SeedGateName(int gate)
	{
		static const char* const nameTable[SeedGateCount] = {
			"placed", "no floor", "inside clip", "slope", "no body fit", "in a hurt volume", "raw node cap",
		};
		return nameTable[gate];
	}


	static int fillCandidates = 0;
	static int fillExisting = 0;
	static int fillNoFloor = 0;
	static int fillNoFooting = 0;
	static int fillNoShoulder = 0;
	static int fillInHurt = 0;
	static int fillNoWay = 0;
	static int fillShoulderShifted = 0;
	static int fillDoorways = 0;
	static int fillReverseDrops = 0;
	static int fillPathFooted = 0;
	static int fillCeilingShifted = 0;
	static int fillJumpMantles = 0;
	static constexpr int pathFootedShown = 60;

	void ReportFillStats()
	{
		Report("navgen: fill candidates %d: %d onto existing nodes, %d no floor, %d no footing, "
			   "%d no shoulder room (%d more placed off the lattice), %d in a hurt volume, %d neither walkable nor droppable, "
			   "%d doorway(s) threaded, %d reverse drop(s), %d on path footing only, %d shifted beside a low ceiling\n",
			   fillCandidates, fillExisting, fillNoFloor, fillNoFooting, fillNoShoulder, fillShoulderShifted,
			   fillInHurt, fillNoWay, fillDoorways, fillReverseDrops, fillPathFooted, fillCeilingShifted);
		Report("navgen: fill links: %d hop(s), %d slide-down(s), %d ladder(s); mantle probe: %d face(s), "
			   "%d without a mantle flag, %d walkable slope(s), %d with no floor up to the face, "
			   "%d with a wall between the face and a deeper ledge, %d link(s) of which %d over, %d from a jump\n",
			   statHopLinks, statSlideLinks, statLadderLinks, statMantleFaces, statMantleUnflagged,
			   statMantleSlopes, statMantleNoApproach, statMantleWalled, statMantleLinks, statMantleOver,
			   fillJumpMantles);
		Report("navgen: ladder probe: %d face(s) seen, %d without an exit floor, %d link(s) of which %d need a jump to grab\n",
			   statLadderFaces, statLadderNoTop, statLadderLinks, statLadderJumpLinks);
	}


	static void RecordSeedPosition(const float* origin, bool isObjective)
	{
		if (seedCount < maxSeeds)
		{
			seedPositions[seedCount][0] = origin[0];
			seedPositions[seedCount][1] = origin[1];
			seedPositions[seedCount][2] = origin[2];
			seedIsObjective[seedCount] = isObjective;
			++seedCount;
		}
	}

	static int AddNodeOutsideHurt(const GroundHit& ground, float x, float y)
	{
		const float point[3] = { x, y, ground.z };
		if (IsInHurtVolume(point))
		{
			++fillInHurt;
			return -1;
		}
		return AddNode(ground, x, y);
	}


	static GroundHit FitBodyOnSlope(float x, float y, const GroundHit& ray)
	{
		GroundHit hit = ray;

		float slopeNormal = ray.rayNormalZ;
		if (slopeNormal < 0.35f)
		{
			slopeNormal = 0.35f;
		}
		const float slopeTan = std::sqrt(1.0f / (slopeNormal * slopeNormal) - 1.0f);
		const float lifts[2] = { 1.0f, 1.0f + boxHalfWidth * slopeTan };
		static const float halfHeights[2] = { standHalfHeight, crouchHalfHeight };

		for (const float lift : lifts)
		{
			for (const float halfHeight : halfHeights)
			{
				const float bounds[6] = { 0.0f, 0.0f, halfHeight, boxHalfWidth, boxHalfWidth, halfHeight };
				const float start[3] = { x, y, ray.rayZ + lift };
				const float end[3] = { x, y, ray.rayZ - 2.0f };
				float normalZ = 0.0f;
				const float fraction = TraceWorld(start, end, bounds, &normalZ);
				if (fraction < 0.001f || fraction >= 1.0f)
				{
					continue;
				}

				hit.isValid = true;
				hit.needsCrouch = halfHeight == crouchHalfHeight;
				hit.z = start[2] + (end[2] - start[2]) * fraction;
				hit.normalZ = normalZ;
				return hit;
			}
		}
		return hit;
	}

	static GroundHit SnapSeed(float x, float y, float z, int* outGate)
	{
		const int missBefore = statRayMiss;
		const GroundHit ground = SnapToGround(x, y, z, seedDropDepth);
		if (ground.isValid)
		{
			*outGate = SeedGateNone;
			return ground;
		}
		if (ground.isSlope)
		{
			*outGate = SeedGateSlope;
			return FitBodyOnSlope(x, y, ground);
		}
		if (ground.isRayStartSolid)
		{
			*outGate = SeedGateInClip;
		}
		else if (statRayMiss != missBefore)
		{
			*outGate = SeedGateNoFloor;
		}
		else
		{
			*outGate = SeedGateNoBodyFit;
		}
		return ground;
	}

	static bool PlaceSeedNode(const float* origin, bool isSeed, bool isPlayerSeed, int* outGate, float* outDrop)
	{
		*outDrop = 0.0f;
		float x = origin[0];
		float y = origin[1];
		int gate = SeedGateNone;
		GroundHit ground = SnapSeed(x, y, origin[2], &gate);

		for (const float radius : seedRingRadius)
		{
			for (const float* direction : directions)
			{
				if (ground.isValid)
				{
					break;
				}
				const float ringX = origin[0] + direction[0] * radius;
				const float ringY = origin[1] + direction[1] * radius;
				int ringGate = SeedGateNone;
				const GroundHit ringGround = SnapSeed(ringX, ringY, origin[2], &ringGate);
				if (ringGround.isValid)
				{
					ground = ringGround;
					x = ringX;
					y = ringY;
				}
			}
		}
		if (!ground.isValid)
		{
			*outGate = gate;
			return false;
		}
		*outDrop = origin[2] - ground.z;

		const int existing = FindNearbyNode(x, y, ground.z);
		if (existing >= 0)
		{
			if (isSeed)
			{
				rawNodes[existing].isSeed = true;
			}
			if (isPlayerSeed)
			{
				rawNodes[existing].isPlayerSeed = true;
			}
			*outGate = SeedGateNone;
			return true;
		}

		const float point[3] = { x, y, ground.z };
		if (IsInHurtVolume(point))
		{
			*outGate = SeedGateHurt;
			return false;
		}
		const int added = AddNode(ground, x, y);
		if (added < 0)
		{
			*outGate = SeedGateCap;
			return false;
		}
		rawNodes[added].isSeed = isSeed;
		rawNodes[added].isPlayerSeed = isPlayerSeed;
		*outGate = SeedGateNone;
		return true;
	}


	void SeedGraph()
	{
		seedCount = 0;
		playerSeedCount = 0;
		fillCandidates = 0;
		fillExisting = 0;
		fillNoFloor = 0;
		fillNoFooting = 0;
		fillNoShoulder = 0;
		fillInHurt = 0;
		fillNoWay = 0;
		fillShoulderShifted = 0;
		fillDoorways = 0;
		fillReverseDrops = 0;
		fillPathFooted = 0;
		fillCeilingShifted = 0;
		fillJumpMantles = 0;
		for (int gate = 0; gate < SeedGateCount; ++gate)
		{
			seedGateCounts[gate] = 0;
		}

		ScanWorldEntities();
		SuppressDynamicSolids(true);
		SuppressGlass(true);

		struct ClassCount
		{
			char name[48];
			int count;
		};
		ClassCount classCounts[maxSpawnClasses] = {};
		int classCount = 0;
		int spawnCount = 0;
		int spawnRefused = 0;
		int spawnsHanging = 0;

		for (int entity = 0; entity < gentityMaxEntities; ++entity)
		{
			const char* raw = reinterpret_cast<const char*>(g_entities) + gentityStride * entity;
			const unsigned short classId = *reinterpret_cast<const unsigned short*>(raw + gentityClassname);
			if (!classId)
			{
				continue;
			}
			const char* classname = ClassnameText(classId);
			if (std::strncmp(classname, "mp_", 3) != 0 || !std::strstr(classname, "spawn"))
			{
				continue;
			}

			int slot = -1;
			for (int i = 0; i < classCount; ++i)
			{
				if (std::strcmp(classCounts[i].name, classname) == 0)
				{
					slot = i;
					break;
				}
			}
			if (slot < 0 && classCount < maxSpawnClasses)
			{
				slot = classCount++;
				std::strncpy(classCounts[slot].name, classname, sizeof(classCounts[slot].name) - 1);
			}
			if (slot >= 0)
			{
				++classCounts[slot].count;
			}

			const float* origin = reinterpret_cast<const float*>(raw + gentityOrigin);
			++spawnCount;

			int gate = SeedGateNone;
			float drop = 0.0f;
			const bool isPlaced = PlaceSeedNode(origin, true, false, &gate, &drop);
			const float dropped[3] = { origin[0], origin[1], origin[2] - drop };
			RecordSeedPosition(dropped, false);
			if (isPlaced)
			{
				if (drop > stepUpMax)
				{
					++spawnsHanging;
					ReportCapped(spawnsHanging - 1, 8,
								 "navgen:   spawn %s at %.0f %.0f %.0f hangs %.0f above its floor, seeded and counted at the floor\n",
								 classname, origin[0], origin[1], origin[2], drop);
				}
				continue;
			}
			++spawnRefused;
			++seedGateCounts[gate];
			ReportCapped(spawnRefused - 1, 8, "navgen:   spawn %s at %.0f %.0f %.0f refused: %s\n",
						 classname, origin[0], origin[1], origin[2], SeedGateName(gate));
		}

		int objectiveSeeds = 0;
		for (int i = 0; i < ObjectiveCount(); ++i)
		{
			const float* position = ObjectivePosition(i);

			int gate = SeedGateNone;
			float drop = 0.0f;
			const bool isPlaced = PlaceSeedNode(position, true, false, &gate, &drop);
			const float dropped[3] = { position[0], position[1], position[2] - drop };
			RecordSeedPosition(dropped, true);
			if (isPlaced)
			{
				++objectiveSeeds;
				continue;
			}
			Report("navgen:   objective %s at %.0f %.0f %.0f refused: %s\n",
				   ObjectiveName(i), position[0], position[1], position[2], SeedGateName(gate));
		}

		const int numClients = *reinterpret_cast<const int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			const char* client = reinterpret_cast<const char*>(svs_clients) + svClientStride * i;
			if (*reinterpret_cast<const int*>(client + svClientState) != 5)
			{
				continue;
			}
			const char* entity = *reinterpret_cast<const char* const*>(client + svClientGentity);
			if (!entity)
			{
				continue;
			}
			const char* gclient = *reinterpret_cast<const char* const*>(entity + gentityClient);
			if (!gclient || *reinterpret_cast<const int*>(gclient + gclientSessionState) != 0)
			{
				continue;
			}
			if (*reinterpret_cast<const int*>(entity + gentityHealth) <= 0)
			{
				continue;
			}

			const char* playerState = SV_GetPlayerstateForClientNum(i);
			int gate = SeedGateNone;
			float drop = 0.0f;
			if (PlaceSeedNode(reinterpret_cast<const float*>(playerState + psOrigin), false, true, &gate, &drop))
			{
				++playerSeedCount;
			}
		}

		Report("navgen: %d seed node(s) from %d spawn(s), %d objective(s) and %d player(s); "
			   "%d spawn(s) refused: %d no floor, %d inside clip, %d slope, %d no body fit, %d in a hurt volume, %d at the raw cap\n",
			   rawCount, spawnCount, ObjectiveCount(), playerSeedCount, spawnRefused,
			   seedGateCounts[SeedGateNoFloor], seedGateCounts[SeedGateInClip], seedGateCounts[SeedGateSlope],
			   seedGateCounts[SeedGateNoBodyFit], seedGateCounts[SeedGateHurt], seedGateCounts[SeedGateCap]);
		Report("navgen: %d spawn(s) hang more than a step above their floor; seeded and counted at the floor\n",
			   spawnsHanging);
		for (int i = 0; i < classCount; ++i)
		{
			Report("navgen:   %d x %s\n", classCounts[i].count, classCounts[i].name);
		}
		Report("navgen: %d objective seed(s) from %d objective(s)\n", objectiveSeeds, ObjectiveCount());
		Report("navgen: %d player seed(s)\n", playerSeedCount);
	}


	static void MantleProbe(int nodeIndex, const float* from, float dirX, float dirY)
	{
		const MantleGeometry geometry = ProbeMantle(from, dirX, dirY);
		if (!geometry.isValid)
		{
			return;
		}

		const float sillX = from[0] + dirX * (geometry.faceDistance + geometry.ledgeDepth);
		const float sillY = from[1] + dirY * (geometry.faceDistance + geometry.ledgeDepth);
		const float* landing = geometry.landing;
		const float sillDx = landing[0] - sillX;
		const float sillDy = landing[1] - sillY;
		const bool isVault = geometry.isOver && (sillDx * sillDx + sillDy * sillDy) > 1.0f;

		int target = FindNearbyNodeWithin(landing[0], landing[1], landing[2], ledgeDedupHeight);
		if (target < 0)
		{
			const bool isSill = !isVault && geometry.ledgeDepth <= sillDepth;
			if (!isSill && !geometry.hasFooting)
			{
				++fillNoFooting;
				return;
			}

			GroundHit ground = {};
			ground.isValid = true;
			ground.needsCrouch = geometry.needsCrouch;
			ground.z = landing[2];
			ground.rayZ = landing[2];
			ground.normalZ = geometry.ledgeNormalZ;
			ground.rayNormalZ = geometry.ledgeNormalZ;
			target = AddNodeOutsideHurt(ground, landing[0], landing[1]);
			if (target < 0)
			{
				return;
			}
		}
		if (target == nodeIndex || HasLink(nodeIndex, target))
		{
			return;
		}

		unsigned char flags = 0;
		if (isVault)
		{
			flags = RawLinkOver;
			++statMantleOver;
		}
		if (geometry.needsJump)
		{
			flags |= RawLinkJumpFirst;
			++fillJumpMantles;
		}
		AddLink(nodeIndex, target, Waypoints::LinkMantle, flags);
		++statMantleLinks;

		if (HasLink(target, nodeIndex) || LinkIfWalkable(target, nodeIndex))
		{
			return;
		}
		if (CanDrop(rawNodes[target].origin, rawNodes[nodeIndex].origin))
		{
			AddLink(target, nodeIndex, Waypoints::LinkDrop);
			++fillReverseDrops;
		}
	}


	static void LadderProbe(int nodeIndex, const float* from, float dirX, float dirY)
	{
		LadderHit ladder = {};
		if (!FindLadder(from, dirX, dirY, &ladder))
		{
			return;
		}

		int bottom = nodeIndex;
		if (Distance2(ladder.bottom, from) > dedupRadius)
		{
			int foot = FindNearbyNode(ladder.bottom[0], ladder.bottom[1], from[2]);
			if (foot < 0)
			{
				const GroundHit ground = SnapToGround(ladder.bottom[0], ladder.bottom[1], from[2], 40.0f);
				if (!ground.isValid)
				{
					return;
				}
				foot = AddNodeOutsideHurt(ground, ladder.bottom[0], ladder.bottom[1]);
				if (foot < 0)
				{
					return;
				}
			}
			if (foot != nodeIndex && !HasLink(nodeIndex, foot))
			{
				if (!LinkIfWalkable(nodeIndex, foot))
				{
					return;
				}
				LinkIfWalkable(foot, nodeIndex);
			}
			bottom = foot;
		}

		int top = FindNearbyNodeWithin(ladder.top[0], ladder.top[1], ladder.top[2], ledgeDedupHeight);
		if (top < 0)
		{
			GroundHit ground = SnapToGround(ladder.top[0], ladder.top[1], ladder.top[2], 20.0f);
			if (!ground.isValid || std::fabs(ground.z - ladder.top[2]) > stepUpMax)
			{
				ground = {};
				ground.isValid = true;
				ground.z = ladder.top[2];
				ground.rayZ = ladder.top[2];
				ground.normalZ = walkableNormal;
				ground.rayNormalZ = walkableNormal;
			}
			top = AddNodeOutsideHurt(ground, ladder.top[0], ladder.top[1]);
			if (top < 0)
			{
				return;
			}
		}
		if (top == bottom || HasLink(bottom, top))
		{
			return;
		}

		AddLink(bottom, top, Waypoints::LinkLadder);
		AddLink(top, bottom, Waypoints::LinkLadder);
		++statLadderLinks;
		if (ladder.needsJump)
		{
			++statLadderJumpLinks;
		}
	}


	static bool TryHop(int fromNode, int toNode)
	{
		const float* from = rawNodes[fromNode].origin;
		const float* to = rawNodes[toNode].origin;
		const float rise = to[2] - from[2];
		if (rise <= stepUpMax || rise > hopRiseMax || HasLink(fromNode, toNode))
		{
			return false;
		}
		if (!CanHop(from, to))
		{
			return false;
		}
		AddLink(fromNode, toNode, Waypoints::LinkJump);
		++statHopLinks;
		return true;
	}


	static constexpr float doorOffsets[6] = { 8.0f, -8.0f, 16.0f, -16.0f, 24.0f, -24.0f };
	static constexpr float doorDepth = 8.0f;

	static bool TryDoorway(int nodeIndex, const float* from, const float* direction)
	{
		const float bounds[6] = { 0.0f, 0.0f, standHalfHeight, boxHalfWidth, boxHalfWidth, standHalfHeight };
		const float start[3] = { from[0], from[1], from[2] + groundLift + stepUpMax };
		const float end[3] = { from[0] + direction[0] * gridStep, from[1] + direction[1] * gridStep, start[2] };
		const float fraction = TraceWorld(start, end, bounds, nullptr);
		if (fraction >= 1.0f)
		{
			return false;
		}
		const float wallDistance = fraction * gridStep + boxHalfWidth;
		if (wallDistance < 8.0f)
		{
			return false;
		}

		const float perpX = -direction[1];
		const float perpY = direction[0];
		for (const float offset : doorOffsets)
		{
			const float x = from[0] + perpX * offset + direction[0] * (wallDistance + doorDepth);
			const float y = from[1] + perpY * offset + direction[1] * (wallDistance + doorDepth);
			if (FindNearbyNode(x, y, from[2]) >= 0)
			{
				continue;
			}
			const GroundHit ground = SnapToGround(x, y, from[2], 40.0f);
			if (!ground.isValid)
			{
				continue;
			}
			const float threshold[3] = { x, y, ground.z };
			if (IsInHurtVolume(threshold))
			{
				continue;
			}
			unsigned char flags = 0;
			bool isHop = false;
			if (!CanWalk(from, threshold))
			{
				const float rise = ground.z - from[2];
				if (CanWalkCrouched(from, threshold))
				{
					flags = RawLinkCrouch;
				}
				else if (rise > stepUpMax && rise <= hopRiseMax && CanHop(from, threshold))
				{
					isHop = true;
				}
				else
				{
					continue;
				}
			}

			const int added = AddNode(ground, x, y);
			if (added < 0)
			{
				return false;
			}
			if (isHop)
			{
				AddLink(nodeIndex, added, Waypoints::LinkJump);
				++statHopLinks;
			}
			else
			{
				AddLink(nodeIndex, added, Waypoints::LinkWalk, flags);
			}
			LinkIfWalkable(added, nodeIndex);
			return true;
		}
		return false;
	}


	void ExpandNode(int nodeIndex)
	{
		const float* from = rawNodes[nodeIndex].origin;

		for (const float* direction : directions)
		{
			++fillCandidates;
			MantleProbe(nodeIndex, from, direction[0], direction[1]);
			LadderProbe(nodeIndex, from, direction[0], direction[1]);

			float x = std::round((from[0] + direction[0] * gridStep) / gridStep) * gridStep;
			float y = std::round((from[1] + direction[1] * gridStep) / gridStep) * gridStep;

			const int existing = FindNearbyNode(x, y, from[2]);
			if (existing >= 0)
			{
				++fillExisting;
				if (existing == nodeIndex || HasLink(nodeIndex, existing))
				{
					continue;
				}
				if (LinkIfWalkable(nodeIndex, existing))
				{
					if (!HasLink(existing, nodeIndex) && !LinkIfWalkable(existing, nodeIndex))
					{
						const float* reverse = rawNodes[existing].origin;
						if (CanSlideDown(reverse, from) || CanDropDeep(reverse, from))
						{
							AddLink(existing, nodeIndex, Waypoints::LinkDrop);
							++fillReverseDrops;
						}
					}
				}
				else if (!TryHop(nodeIndex, existing) && TryDoorway(nodeIndex, from, direction))
				{
					++fillDoorways;
				}

				const bool isParentOffLattice =
					std::fabs(from[0] - std::round(from[0] / gridStep) * gridStep) >= 4.0f
					|| std::fabs(from[1] - std::round(from[1] / gridStep) * gridStep) >= 4.0f;
				if (isParentOffLattice)
				{
					const GroundHit lattice = SnapToGround(x, y, from[2], 80.0f);
					if (!lattice.isValid && lattice.isRayStartSolid)
					{
						float shiftedX = x;
						float shiftedY = y;
						const GroundHit shifted = SnapBesideSolid(x, y, from[2], from, &shiftedX, &shiftedY);
						const int nearNode = FindNearbyNode(shiftedX, shiftedY, shifted.z);
						const bool isFree = nearNode < 0
							|| (nearNode == nodeIndex && std::fabs(shifted.z - from[2]) > stepUpMax
								&& FindOtherNearbyNode(shiftedX, shiftedY, shifted.z, nodeIndex) < 0);
						if (shifted.isValid && isFree)
						{
							const int added = AddNodeOutsideHurt(shifted, shiftedX, shiftedY);
							if (added >= 0)
							{
								LinkIfWalkable(nodeIndex, added);
								LinkIfWalkable(added, nodeIndex);
								++fillCeilingShifted;
							}
						}
					}
				}
				continue;
			}

			GroundHit ground = SnapToGround(x, y, from[2], 80.0f);
			const bool isOffLatticeParent =
				std::fabs(from[0] - std::round(from[0] / gridStep) * gridStep) >= 4.0f
				|| std::fabs(from[1] - std::round(from[1] / gridStep) * gridStep) >= 4.0f;
			bool isLatticePointLost = !ground.isValid && ground.isRayStartSolid;
			if (isOffLatticeParent)
			{
				if (!ground.isValid)
				{
					isLatticePointLost = true;
				}
				else if (ground.z - from[2] > hopRiseMax)
				{
					isLatticePointLost = true;
				}
			}
			if (isLatticePointLost)
			{
				float shiftedX = x;
				float shiftedY = y;
				const GroundHit shifted = SnapBesideSolid(x, y, from[2], from, &shiftedX, &shiftedY);
				if (shifted.isValid)
				{
					x = shiftedX;
					y = shiftedY;
					ground = shifted;
					++fillCeilingShifted;
				}
			}
			const bool isBankAhead = ground.isSlope;
			bool isDrop = false;
			if (!ground.isValid)
			{
				ground = SnapToGround(x, y, from[2] - 100.0f, 220.0f);
				isDrop = true;
			}

			for (float reach = 1.5f; !ground.isValid && reach <= 2.0f; reach += 0.5f)
			{
				x = from[0] + direction[0] * gridStep * reach;
				y = from[1] + direction[1] * gridStep * reach;
				ground = SnapToGround(x, y, from[2] - 100.0f, 220.0f);
				isDrop = true;
			}

			bool isSlide = false;
			if (!ground.isValid && isBankAhead)
			{
				for (int step = 3; step <= slideReachSteps; ++step)
				{
					x = from[0] + direction[0] * gridStep * static_cast<float>(step);
					y = from[1] + direction[1] * gridStep * static_cast<float>(step);
					ground = SnapToGround(x, y, from[2] - 100.0f, 220.0f);
					if (ground.isValid)
					{
						isSlide = true;
						break;
					}
					if (!ground.isSlope)
					{
						break;
					}
				}
			}
			if (!ground.isValid)
			{
				if (TryDoorway(nodeIndex, from, direction))
				{
					++fillDoorways;
				}
				else
				{
					++fillNoFloor;
				}
				continue;
			}

			int lower = FindNearbyNode(x, y, ground.z);
			if (lower == nodeIndex && std::fabs(ground.z - from[2]) > stepUpMax)
			{
				lower = FindOtherNearbyNode(x, y, ground.z, nodeIndex);
			}
			if (lower >= 0)
			{
				++fillExisting;
				if (lower == nodeIndex)
				{
					continue;
				}
				bool isJoined = HasLink(nodeIndex, lower);
				if (!isJoined)
				{
					isJoined = LinkIfWalkable(nodeIndex, lower);
				}
				if (!HasLink(lower, nodeIndex))
				{
					LinkIfWalkable(lower, nodeIndex);
				}
				if (!isJoined)
				{
					isJoined = TryHop(nodeIndex, lower);
				}
				if (!isJoined && !isSlide && CanDropDeep(from, rawNodes[lower].origin))
				{
					AddLink(nodeIndex, lower, Waypoints::LinkDrop);
					isJoined = true;
				}
				if (!isJoined && isBankAhead && CanSlideDown(from, rawNodes[lower].origin))
				{
					AddLink(nodeIndex, lower, Waypoints::LinkDrop);
					++statSlideLinks;
					isJoined = true;
				}
				if (!isJoined && TryDoorway(nodeIndex, from, direction))
				{
					++fillDoorways;
				}
				continue;
			}

			if (!HasSolidFooting(x, y, ground.z))
			{
				if (!HasPathFooting(x, y, ground.z))
				{
					++fillNoFooting;
					continue;
				}
				++fillPathFooted;
				if (fillPathFooted <= pathFootedShown)
				{
					ReportFile("navgen:   path footing admits %.0f %.0f %.0f (strict refused)\n", x, y, ground.z);
				}
			}
			float pushX = 0.0f;
			float pushY = 0.0f;
			if (!HasShoulderRoom(x, y, ground.z, ground.needsCrouch, &pushX, &pushY))
			{
				bool hasRoom = false;
				for (float push = 12.0f; push <= 24.0f && !hasRoom; push += 12.0f)
				{
					const float shiftedX = x + pushX * push;
					const float shiftedY = y + pushY * push;
					const int neighbour = FindNearbyNode(shiftedX, shiftedY, ground.z);
					if (neighbour >= 0)
					{
						if (neighbour != nodeIndex && !HasLink(nodeIndex, neighbour))
						{
							LinkIfWalkable(nodeIndex, neighbour);
							LinkIfWalkable(neighbour, nodeIndex);
						}
						break;
					}
					const GroundHit shifted = SnapToGround(shiftedX, shiftedY, from[2], 80.0f);
					if (!shifted.isValid || !HasPathFooting(shiftedX, shiftedY, shifted.z)
						|| !HasShoulderRoom(shiftedX, shiftedY, shifted.z, shifted.needsCrouch))
					{
						continue;
					}
					x = shiftedX;
					y = shiftedY;
					ground = shifted;
					hasRoom = true;
				}
				if (!hasRoom)
				{
					++fillNoShoulder;
					continue;
				}
				++fillShoulderShifted;
			}
			const float candidate[3] = { x, y, ground.z };
			if (IsInHurtVolume(candidate))
			{
				++fillInHurt;
				continue;
			}

			bool isWalkable = false;
			unsigned char walkFlags = 0;
			if (!isDrop)
			{
				isWalkable = CanWalk(from, candidate);
				if (!isWalkable && CanWalkCrouched(from, candidate))
				{
					isWalkable = true;
					walkFlags = RawLinkCrouch;
				}
			}
			if (isWalkable)
			{
				const int added = AddNode(ground, x, y);
				if (added >= 0)
				{
					AddLink(nodeIndex, added, Waypoints::LinkWalk, walkFlags);
					LinkIfWalkable(added, nodeIndex);
				}
				continue;
			}

			const float rise = ground.z - from[2];
			if (!isDrop && rise > stepUpMax && rise <= hopRiseMax && CanHop(from, candidate))
			{
				const int added = AddNode(ground, x, y);
				if (added >= 0)
				{
					AddLink(nodeIndex, added, Waypoints::LinkJump);
					++statHopLinks;
					LinkIfWalkable(added, nodeIndex);
				}
				continue;
			}

			if (!isSlide && CanDrop(from, candidate))
			{
				const int added = AddNode(ground, x, y);
				if (added >= 0)
				{
					AddLink(nodeIndex, added, Waypoints::LinkDrop);
					LinkIfWalkable(added, nodeIndex);
				}
				continue;
			}

			if (isBankAhead && CanSlideDown(from, candidate))
			{
				const int added = AddNode(ground, x, y);
				if (added >= 0)
				{
					AddLink(nodeIndex, added, Waypoints::LinkDrop);
					++statSlideLinks;
				}
				continue;
			}

			if (TryDoorway(nodeIndex, from, direction))
			{
				++fillDoorways;
				continue;
			}
			++fillNoWay;
		}
	}
}
