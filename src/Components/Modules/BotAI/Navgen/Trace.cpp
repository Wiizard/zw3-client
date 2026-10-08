#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include <cmath>

namespace Components::BotAI::Navgen
{
	int traceCount = 0;

	int statRayMiss = 0;
	int statRayStartSolid = 0;
	int statRaySlope = 0;
	int statRayZero = 0;
	int statMantleFaces = 0;
	int statMantleUnflagged = 0;
	int statMantleSlopes = 0;
	int statMantleNoApproach = 0;
	int statMantleWalled = 0;
	int statMantleLinks = 0;
	int statMantleOver = 0;
	int statHopLinks = 0;
	int statSlideLinks = 0;
	int statLadderLinks = 0;
	int statLadderFaces = 0;
	int statLadderNoTop = 0;
	int statLadderJumpLinks = 0;


	bool probeVerbose = false;

	static constexpr float rayBounds[6] = { 0.0f, 0.0f, 0.0f, 0.75f, 0.75f, 0.75f };


	void ResetTraceStats()
	{
		traceCount = 0;
		statRayMiss = 0;
		statRayStartSolid = 0;
		statRaySlope = 0;
		statRayZero = 0;
		statMantleFaces = 0;
		statMantleUnflagged = 0;
		statMantleSlopes = 0;
		statMantleNoApproach = 0;
		statMantleWalled = 0;
		statMantleLinks = 0;
		statMantleOver = 0;
		statHopLinks = 0;
		statSlideLinks = 0;
		statLadderLinks = 0;
		statLadderFaces = 0;
		statLadderNoTop = 0;
		statLadderJumpLinks = 0;
	}


	TraceHit TraceFull(const float* start, const float* end, const float* bounds, int contentmask)
	{
		unsigned char trace[128] = {};
		const int ignore[4] = { -1, -1, 0, 0 };

		++traceCount;
		SV_Trace(trace, start, end, bounds, ignore,
											  contentmask, 0, nullptr, 1);

		TraceHit hit = {};
		hit.fraction = *reinterpret_cast<const float*>(trace + traceFraction);
		const float* normal = reinterpret_cast<const float*>(trace + traceNormal);
		hit.normal[0] = normal[0];
		hit.normal[1] = normal[1];
		hit.normal[2] = normal[2];
		hit.normalZ = normal[2];
		hit.hitType = *reinterpret_cast<const int*>(trace + traceHitType);
		hit.surfaceFlags = *reinterpret_cast<const int*>(trace + traceSurfaceFlags);
		hit.contents = *reinterpret_cast<const int*>(trace + traceContents);
		hit.isStartSolid = trace[traceStartSolid] != 0;
		return hit;
	}

	float TraceWorldHit(const float* start, const float* end, const float* bounds,
						float* outNormalZ, int* outHitType)
	{
		const TraceHit hit = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
		if (outNormalZ)
		{
			*outNormalZ = hit.normalZ;
		}
		if (outHitType)
		{
			*outHitType = hit.hitType;
		}
		return hit.fraction;
	}

	float TraceWorld(const float* start, const float* end, const float* bounds, float* outNormalZ)
	{
		return TraceWorldHit(start, end, bounds, outNormalZ, nullptr);
	}

	static float TraceBox(const float* start, const float* end, float halfHeight, float* outNormalZ)
	{
		const float bounds[6] = { 0.0f, 0.0f, halfHeight, 15.0f, 15.0f, halfHeight };
		return TraceWorld(start, end, bounds, outNormalZ);
	}

	static TraceHit RayDown(float x, float y, float fromZ, float depth)
	{
		const float start[3] = { x, y, fromZ };
		const float end[3] = { x, y, fromZ - depth };
		return TraceFull(start, end, rayBounds, maskPlayerSolidNoBodies);
	}

	bool GroundRay(float x, float y, float fromZ, float depth, float* outZ, float* outNormalZ)
	{
		const TraceHit hit = RayDown(x, y, fromZ, depth);
		if (hit.isStartSolid || hit.fraction < 0.01f || hit.fraction >= 1.0f)
		{
			return false;
		}

		*outZ = fromZ - depth * hit.fraction;
		*outNormalZ = hit.normalZ;
		return true;
	}

	bool GroundRayBox(float x, float y, float fromZ, float depth, float halfWidth, float* outZ, float* outNormalZ)
	{
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, halfWidth, halfWidth, 0.75f };
		const float start[3] = { x, y, fromZ };
		const float end[3] = { x, y, fromZ - depth };
		const TraceHit hit = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
		if (hit.isStartSolid || hit.fraction < 0.01f || hit.fraction >= 1.0f)
		{
			return false;
		}

		*outZ = fromZ - depth * hit.fraction;
		*outNormalZ = hit.normalZ;
		return true;
	}

	GroundHit SnapBesideSolid(float x, float y, float fromZ, const float* parent, float* outX, float* outY)
	{
		static const float pushOffsets[12] = { 4.0f, -4.0f, 8.0f, -8.0f, 12.0f, -12.0f, 16.0f, -16.0f, 20.0f, -20.0f, 24.0f, -24.0f };
		static const float startDrops[2] = { 0.0f, 16.0f };
		const float point[2] = { x, y };
		float tries[28][2];
		int tryCount = 0;

		for (int axis = 0; axis < 2; ++axis)
		{
			float parentOffset = parent[axis] - point[axis];
			if (parentOffset > 24.0f)
			{
				parentOffset = 24.0f;
			}
			if (parentOffset < -24.0f)
			{
				parentOffset = -24.0f;
			}
			if (std::fabs(parentOffset) >= 4.0f)
			{
				tries[tryCount][0] = x;
				tries[tryCount][1] = y;
				tries[tryCount][axis] += parentOffset;
				++tryCount;
			}
		}
		for (const float push : pushOffsets)
		{
			tries[tryCount][0] = x + push;
			tries[tryCount][1] = y;
			++tryCount;
			tries[tryCount][0] = x;
			tries[tryCount][1] = y + push;
			++tryCount;
		}

		const GroundHit lower = SnapToGround(x, y, fromZ - startDrops[1], 80.0f - startDrops[1]);
		if (lower.isValid)
		{
			const float candidate[3] = { x, y, lower.z };
			if (CanWalk(parent, candidate) || CanWalkCrouched(parent, candidate))
			{
				*outX = x;
				*outY = y;
				return lower;
			}
		}
		for (int t = 0; t < tryCount; ++t)
		{
			for (const float drop : startDrops)
			{
				const GroundHit shifted = SnapToGround(tries[t][0], tries[t][1], fromZ - drop, 80.0f - drop);
				if (!shifted.isValid)
				{
					continue;
				}
				const float candidate[3] = { tries[t][0], tries[t][1], shifted.z };
				if (CanWalk(parent, candidate) || CanWalkCrouched(parent, candidate))
				{
					*outX = tries[t][0];
					*outY = tries[t][1];
					return shifted;
				}
				break;
			}
		}

		GroundHit none = {};
		return none;
	}

	bool IsInsideSolid(float x, float y, float z)
	{
		const float point[3] = { x, y, z };
		return TraceFull(point, point, rayBounds, maskPlayerSolidNoBodies).isStartSolid;
	}

	int SurfaceFlagsUnder(float x, float y, float z)
	{
		const TraceHit hit = RayDown(x, y, z + 4.0f, 8.0f);
		if (hit.fraction >= 1.0f)
		{
			return 0;
		}
		return hit.surfaceFlags;
	}

	bool IsInsidePlayerClip(float x, float y, float z)
	{
		const float point[3] = { x, y, z };
		const TraceHit hit = TraceFull(point, point, rayBounds, contentsPlayerClip);
		return hit.isStartSolid;
	}


	bool ClearThroughGlass(const float* start, const float* end, const float* bounds)
	{
		const TraceHit hit = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
		return !hit.isStartSolid && hit.fraction >= 1.0f;
	}

	int GlassHitId(const float* start, const float* end, const float* bounds)
	{
		unsigned char trace[128] = {};
		const int ignore[4] = { -1, -1, 0, 0 };

		++traceCount;
		SV_Trace(trace, start, end, bounds, ignore,
											  maskPlayerSolidNoBodies, 0, nullptr, 1);

		if (*reinterpret_cast<const float*>(trace + traceFraction) >= 1.0f
			|| *reinterpret_cast<const int*>(trace + traceHitType) != traceHitTypeGlass)
		{
			return 0;
		}
		return *reinterpret_cast<const unsigned short*>(trace + traceHitId);
	}


	using G_Glass_GetPieceOrigin_t = float* (__cdecl*)(int piece, float* out);

	static const char* GlassData()
	{
		return *reinterpret_cast<const char* const*>(g_glassData);
	}

	int GlassPieceCount()
	{
		const char* data = GlassData();
		if (!data)
		{
			return 0;
		}
		return static_cast<int>(reinterpret_cast<const Game::G_GlassData*>(data)->pieceCount);
	}

	bool GlassPieceOrigin(int piece, float out[3])
	{
		if (piece < 0 || piece >= GlassPieceCount())
		{
			return false;
		}
		G_Glass_GetPieceOrigin(piece, out);
		return true;
	}

	int GlassPieceDamage(int piece)
	{
		if (piece < 0 || piece >= GlassPieceCount())
		{
			return glassDamageDeleted;
		}
		const char* pieces = *reinterpret_cast<const char* const*>(GlassData());
		return *reinterpret_cast<const unsigned short*>(pieces + glassPieceStride * piece);
	}


	static constexpr int maxGlassPieces = 4096;
	static unsigned short savedGlassDamage[maxGlassPieces];
	static int savedGlassCount = 0;
	static bool isGlassSuppressed = false;
	static constexpr unsigned short glassDamageSuppressed = 0xFFFE;

	static unsigned short* GlassDamageAt(int piece)
	{
		char* pieces = *reinterpret_cast<char* const*>(GlassData());
		return reinterpret_cast<unsigned short*>(pieces + glassPieceStride * piece);
	}

	void SuppressGlass(bool suppress)
	{
		if (suppress == isGlassSuppressed || !GlassData()
			|| !*reinterpret_cast<const char* const*>(GlassData()))
		{
			return;
		}

		if (suppress)
		{
			savedGlassCount = GlassPieceCount();
			if (savedGlassCount > maxGlassPieces)
			{
				savedGlassCount = maxGlassPieces;
			}
			for (int i = 0; i < savedGlassCount; ++i)
			{
				savedGlassDamage[i] = *GlassDamageAt(i);
				*GlassDamageAt(i) = glassDamageSuppressed;
			}
		}
		else
		{
			for (int i = 0; i < savedGlassCount; ++i)
			{
				*GlassDamageAt(i) = savedGlassDamage[i];
			}
			savedGlassCount = 0;
		}
		isGlassSuppressed = suppress;
	}


	GroundHit SnapToGround(float x, float y, float fromZ, float depth)
	{
		GroundHit hit = {};

		static const float rayReach[4] = { snapReachUp, 40.0f, 24.0f, 8.0f };
		float groundZ = 0.0f;
		float rayNormalZ = 0.0f;
		bool hasFloor = false;
		bool isEveryStartSolid = true;
		for (const float reach : rayReach)
		{
			const TraceHit ray = RayDown(x, y, fromZ + reach, reach + depth);
			if (ray.isStartSolid || ray.fraction < 0.01f)
			{
				continue;
			}
			isEveryStartSolid = false;
			if (ray.fraction >= 1.0f)
			{
				break;
			}
			groundZ = fromZ + reach - (reach + depth) * ray.fraction;
			rayNormalZ = ray.normalZ;
			hasFloor = true;
			break;
		}
		if (!hasFloor)
		{
			if (isEveryStartSolid)
			{
				++statRayStartSolid;
				hit.isRayStartSolid = true;
			}
			else
			{
				++statRayMiss;
			}
			return hit;
		}

		hit.rayZ = groundZ;
		hit.rayNormalZ = rayNormalZ;

		float slopeLift = 0.0f;
		if (rayNormalZ > 0.1f && rayNormalZ < trueNormalMin)
		{
			++statRaySlope;
			hit.isSlope = true;
			float slopeNormal = rayNormalZ;
			if (slopeNormal < 0.35f)
			{
				slopeNormal = 0.35f;
			}
			slopeLift = boxHalfWidth * std::sqrt(1.0f / (slopeNormal * slopeNormal) - 1.0f);
		}
		else if (rayNormalZ <= 0.1f)
		{
			++statRayZero;
		}

		static const float fitTable[4][2] = {
			{ standHalfHeight,  1.0f },
			{ standHalfHeight,  stepUpMax + 1.0f },
			{ crouchHalfHeight, 1.0f },
			{ crouchHalfHeight, stepUpMax + 1.0f },
		};
		for (const float* fit : fitTable)
		{
			const float halfHeight = fit[0];
			const float start[3] = { x, y, groundZ + fit[1] + slopeLift };
			const float end[3] = { x, y, groundZ - 2.0f };

			float normalZ = 0.0f;
			const float fraction = TraceBox(start, end, halfHeight, &normalZ);
			if (fraction < 0.001f)
			{
				continue;
			}
			if (fraction >= 1.0f || normalZ < walkableNormal)
			{
				continue;
			}

			hit.isValid = true;
			hit.needsCrouch = halfHeight == crouchHalfHeight;
			hit.z = start[2] + (end[2] - start[2]) * fraction;
			hit.normalZ = normalZ;
			return hit;
		}

		return hit;
	}


	static bool SlideSegment(const float* fromXY, const float* toXY, float* z, float halfHeight)
	{
		const float bounds[6] = { 0.0f, 0.0f, halfHeight, 15.0f, 15.0f, halfHeight };
		float start[3] = { fromXY[0], fromXY[1], *z };
		float end[3] = { toXY[0], toXY[1], *z };

		TraceHit hit = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
		if (hit.fraction >= 1.0f)
		{
			return true;
		}
		if (hit.hitType == traceHitTypeGlass)
		{
			if (probeVerbose)
			{
				Report("navgen:       slide: glass across the way, counted clear\n");
			}
			return true;
		}

		for (int bump = 0; bump < 4 && hit.normalZ >= walkableNormal; ++bump)
		{
			const float contact[3] = {
				start[0] + (end[0] - start[0]) * hit.fraction,
				start[1] + (end[1] - start[1]) * hit.fraction,
				start[2] + (end[2] - start[2]) * hit.fraction,
			};
			const float dx = toXY[0] - contact[0];
			const float dy = toXY[1] - contact[1];
			const float dz = -(hit.normal[0] * dx + hit.normal[1] * dy) / hit.normal[2];
			if (probeVerbose)
			{
				Report("navgen:       slide: ground (normal %.2f) at %.0f%%, clipped along the plane, %+.1f up\n",
					   hit.normalZ, hit.fraction * 100.0f, dz);
			}

			start[0] = contact[0];
			start[1] = contact[1];
			start[2] = contact[2];
			end[0] = toXY[0];
			end[1] = toXY[1];
			end[2] = contact[2] + dz + groundLift;

			hit = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
			if (hit.fraction >= 1.0f || hit.hitType == traceHitTypeGlass)
			{
				*z = end[2];
				return true;
			}
		}

		const float contact[3] = {
			start[0] + (end[0] - start[0]) * hit.fraction,
			start[1] + (end[1] - start[1]) * hit.fraction,
			start[2] + (end[2] - start[2]) * hit.fraction,
		};
		const float lifted[3] = { contact[0], contact[1], contact[2] + stepUpMax };
		if (TraceWorld(contact, lifted, bounds, nullptr) < 1.0f)
		{
			if (probeVerbose)
			{
				Report("navgen:       slide: stopped at %.0f%%, no room to step up\n", hit.fraction * 100.0f);
			}
			return false;
		}

		const float liftedEnd[3] = { toXY[0], toXY[1], contact[2] + stepUpMax };
		int hitType = 0;
		if (TraceWorldHit(lifted, liftedEnd, bounds, nullptr, &hitType) < 1.0f
			&& hitType != traceHitTypeGlass)
		{
			if (probeVerbose)
			{
				Report("navgen:       slide: stopped at %.0f%%, stepped up, blocked again\n", hit.fraction * 100.0f);
			}
			return false;
		}

		if (probeVerbose)
		{
			Report("navgen:       slide: stopped at %.0f%%, stepped up and through\n", hit.fraction * 100.0f);
		}
		*z = contact[2] + stepUpMax;
		return true;
	}


	bool HasShoulderRoom(float x, float y, float z, bool crouch, float* outPushX, float* outPushY)
	{
		static const float shifts[4][2] = {
			{ shoulderShift, 0.0f }, { -shoulderShift, 0.0f },
			{ 0.0f, shoulderShift }, { 0.0f, -shoulderShift },
		};

		if (outPushX)
		{
			*outPushX = 0.0f;
			*outPushY = 0.0f;
		}
		const float halfHeight = crouch ? crouchHalfHeight : standHalfHeight;
		const float fromXY[2] = { x, y };

		for (const float* shift : shifts)
		{
			const float toXY[2] = { x + shift[0], y + shift[1] };
			float shiftedZ = z + groundLift;
			if (probeVerbose)
			{
				Report("navgen:     shoulder shift %+.0f %+.0f:\n", shift[0], shift[1]);
			}
			if (SlideSegment(fromXY, toXY, &shiftedZ, halfHeight))
			{
				continue;
			}

			float stepZ = 0.0f;
			float stepNormalZ = 0.0f;
			const float riserReach = stepUpMax + 4.0f;
			const bool isRiser = GroundRay(toXY[0], toXY[1], z + riserReach, riserReach + walkDropMax,
										   &stepZ, &stepNormalZ)
				&& stepZ - z > 2.0f && stepZ - z <= stepUpMax;
			if (probeVerbose)
			{
				Report("navgen:       blocked; floor there %s\n",
					   isRiser ? "is a riser, allowed" : "is not a riser, rejected");
			}
			if (!isRiser)
			{
				if (outPushX)
				{
					*outPushX = -shift[0] / shoulderShift;
					*outPushY = -shift[1] / shoulderShift;
				}
				return false;
			}
		}
		return true;
	}


	static constexpr float flightRise = 4.0f;

	static bool FootingCore(float x, float y, float z, bool allowLowSides)
	{
		static const float offsetTable[4][2] = {
			{ 14.0f, 0.0f }, { -14.0f, 0.0f }, { 0.0f, 14.0f }, { 0.0f, -14.0f },
		};

		float centreZ = 0.0f;
		float centreNormalZ = 0.0f;
		if (!GroundRay(x, y, z + 24.0f, 64.0f, &centreZ, &centreNormalZ))
		{
			return false;
		}

		float slopeNormal = centreNormalZ;
		if (slopeNormal < walkableNormal)
		{
			slopeNormal = walkableNormal;
		}
		const float slopeTan = std::sqrt(1.0f / (slopeNormal * slopeNormal) - 1.0f);
		const float tolerance = 24.0f + boxHalfWidth * slopeTan;

		const float lowAllowance = tolerance + jumpHeight;
		const float rayDepth = (z + 24.0f) - (centreZ - lowAllowance) + 1.0f;

		bool isFound[4] = { false, false, false, false };
		float delta[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (int side = 0; side < 4; ++side)
		{
			const float* offset = offsetTable[side];
			float groundZ = 0.0f;
			float normalZ = 0.0f;
			if (!GroundRayBox(x + offset[0], y + offset[1], z + 24.0f, rayDepth, 3.0f, &groundZ, &normalZ))
			{
				if (!allowLowSides)
				{
					return false;
				}
				continue;
			}
			delta[side] = groundZ - centreZ;
			if (delta[side] > tolerance)
			{
				return false;
			}
			if (delta[side] < -tolerance && !allowLowSides)
			{
				return false;
			}
			if (normalZ > 0.1f && normalZ < walkableNormal)
			{
				return false;
			}
			isFound[side] = true;
		}

		bool isPlain = true;
		for (int side = 0; side < 4; ++side)
		{
			if (!isFound[side] || delta[side] < -tolerance)
			{
				isPlain = false;
			}
		}
		if (isPlain)
		{
			return true;
		}

		for (int axis = 0; axis < 2; ++axis)
		{
			const int a = axis * 2;
			const int b = a + 1;
			const bool isFlight = isFound[a] && isFound[b]
				&& std::fabs(delta[a]) <= tolerance && std::fabs(delta[b]) <= tolerance
				&& delta[a] * delta[b] < 0.0f
				&& std::fabs(delta[a]) >= flightRise && std::fabs(delta[b]) >= flightRise;
			if (isFlight)
			{
				return true;
			}
		}
		return false;
	}

	bool HasSolidFooting(float x, float y, float z)
	{
		return FootingCore(x, y, z, false);
	}

	bool HasPathFooting(float x, float y, float z)
	{
		return FootingCore(x, y, z, true);
	}


	GroundHit GroundFitNoGlass(float x, float y, float ledgeZ)
	{
		GroundHit hit = {};
		hit.rayZ = ledgeZ;

		for (int attempt = 0; attempt < 2; ++attempt)
		{
			const bool crouch = attempt == 1;
			const float halfHeight = crouch ? crouchHalfHeight : standHalfHeight;
			const float bounds[6] = { 0.0f, 0.0f, halfHeight, 15.0f, 15.0f, halfHeight };
			const float start[3] = { x, y, ledgeZ + 10.0f };
			const float end[3] = { x, y, ledgeZ + 1.0f };

			const TraceHit drop = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
			if (drop.isStartSolid || drop.fraction < 1.0f)
			{
				continue;
			}

			hit.isValid = true;
			hit.needsCrouch = crouch;
			hit.z = ledgeZ;
			hit.normalZ = walkableNormal;
			hit.rayNormalZ = walkableNormal;
			return hit;
		}

		return hit;
	}


	static bool CanWalkWith(const float* from, const float* to, float halfHeight)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance < 1.0f)
		{
			return true;
		}

		const float bounds[6] = { 0.0f, 0.0f, halfHeight, 15.0f, 15.0f, halfHeight };
		const int segments = static_cast<int>(distance / walkSegment) + 1;
		float previous[2] = { from[0], from[1] };
		float groundZ = from[2];
		float z = groundZ + groundLift;

		for (int i = 1; i <= segments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(segments);
			const float next[2] = { from[0] + dx * t, from[1] + dy * t };

			if (probeVerbose)
			{
				Report("navgen:     seg %d/%d to %.0f %.0f from floor %.1f:\n", i, segments, next[0], next[1], groundZ);
			}
			if (!SlideSegment(previous, next, &z, halfHeight))
			{
				return false;
			}

			const float landStart[3] = { next[0], next[1], z };
			const float landEnd[3] = { next[0], next[1], groundZ - walkDropMax };
			float normalZ = 0.0f;
			int landHitType = 0;
			const float fraction = TraceWorldHit(landStart, landEnd, bounds, &normalZ, &landHitType);
			if (fraction < 1.0f && landHitType == traceHitTypeGlass)
			{
				float rayZ = 0.0f;
				float rayNormalZ = 0.0f;
				if (GroundRay(next[0], next[1], z, z - (groundZ - walkDropMax), &rayZ, &rayNormalZ)
					&& rayNormalZ >= walkableNormal)
				{
					if (probeVerbose)
					{
						Report("navgen:       land: glass beside the line, floor %.1f by ray\n", rayZ);
					}
					groundZ = rayZ;
					z = groundZ + groundLift;
					previous[0] = next[0];
					previous[1] = next[1];
					continue;
				}
			}
			if (fraction >= 1.0f || normalZ < walkableNormal)
			{
				if (probeVerbose)
				{
					Report("navgen:       land: %s\n", fraction >= 1.0f
						   ? "nothing to stand on within the fall tolerance"
						   : "surface too steep to stand on");
				}
				return false;
			}

			groundZ = landStart[2] + (landEnd[2] - landStart[2]) * fraction;
			z = groundZ + groundLift;
			if (probeVerbose)
			{
				Report("navgen:       land: floor %.1f, normal %.2f\n", groundZ, normalZ);
			}
			previous[0] = next[0];
			previous[1] = next[1];
		}

		const bool arrived = std::fabs(groundZ - to[2]) <= arriveSlack;
		if (probeVerbose)
		{
			Report("navgen:     arrival: floor %.1f vs target %.1f, %s\n", groundZ, to[2],
				   arrived ? "within slack" : "too far off");
		}
		return arrived;
	}

	bool CanWalk(const float* from, const float* to)
	{
		return CanWalkWith(from, to, standHalfHeight);
	}

	bool CanWalkCrouched(const float* from, const float* to)
	{
		return CanWalkWith(from, to, crouchHalfHeight);
	}


	static bool CanDropUpTo(const float* from, const float* to, float maxFall)
	{
		const float fall = from[2] - to[2];
		if (fall < dropStepMin || fall > maxFall)
		{
			if (probeVerbose)
			{
				Report("navgen:     drop: fall %.0f outside %.0f..%.0f\n", fall, dropStepMin, maxFall);
			}
			return false;
		}
		if (IsInHurtVolume(to))
		{
			if (probeVerbose)
			{
				Report("navgen:     drop: the landing is in a hurt volume\n");
			}
			return false;
		}

		static const float exitHeights[6] = { groundLift, 6.0f, 12.0f, 20.0f, 28.0f, jumpHeight };
		const float bounds[6] = { 0.0f, 0.0f, standHalfHeight, 15.0f, 15.0f, standHalfHeight };
		for (const float exitHeight : exitHeights)
		{
			const float ledge[3] = { from[0], from[1], from[2] + exitHeight };
			const float exit[3] = { to[0], to[1], from[2] + exitHeight };
			int hitType = 0;
			if (TraceWorldHit(ledge, exit, bounds, nullptr, &hitType) < 1.0f && hitType != traceHitTypeGlass)
			{
				if (probeVerbose)
				{
					Report("navgen:     drop: exit at +%.0f blocked\n", exitHeight);
				}
				continue;
			}

			const float landing[3] = { to[0], to[1], to[2] + 4.0f };
			const float landFraction = TraceBox(exit, landing, standHalfHeight, nullptr);
			if (landFraction >= 0.95f)
			{
				if (probeVerbose)
				{
					Report("navgen:     drop: fall %.0f, exit at +%.0f clear, lands\n", fall, exitHeight);
				}
				return true;
			}
			if (probeVerbose)
			{
				Report("navgen:     drop: exit at +%.0f clear, the fall stops %.0f%% of the way down\n",
					   exitHeight, landFraction * 100.0f);
			}
		}
		return false;
	}


	bool CanSlideDown(const float* from, const float* to)
	{
		const float fall = from[2] - to[2];
		if (fall < dropLinkMin || fall > dropLinkMax)
		{
			if (probeVerbose)
			{
				Report("navgen:     slide-down: fall %.0f outside %.0f..%.0f\n", fall, dropLinkMin, dropLinkMax);
			}
			return false;
		}

		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float distance = std::sqrt(dx * dx + dy * dy);
		const int samples = static_cast<int>(distance / walkSegment) + 1;
		const float bounds[6] = { 0.0f, 0.0f, crouchHalfHeight, 15.0f, 15.0f, crouchHalfHeight };

		float previousFloor = from[2];
		for (int i = 1; i <= samples; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(samples);
			const float sx = from[0] + dx * t;
			const float sy = from[1] + dy * t;

			float floorZ = 0.0f;
			float normalZ = 0.0f;
			if (!GroundRay(sx, sy, previousFloor + 24.0f, 24.0f + 60.0f + 1.0f, &floorZ, &normalZ))
			{
				if (probeVerbose)
				{
					Report("navgen:     slide-down: sample %d/%d at %.0f %.0f: no floor within 60 of %.1f\n",
						   i, samples, sx, sy, previousFloor);
				}
				return false;
			}
			if (previousFloor - floorZ > 60.0f)
			{
				if (probeVerbose)
				{
					Report("navgen:     slide-down: sample %d/%d: floor %.1f is a cliff below %.1f\n",
						   i, samples, floorZ, previousFloor);
				}
				return false;
			}

			float slopeNormal = normalZ;
			if (slopeNormal < 0.35f)
			{
				slopeNormal = 0.35f;
			}
			const float slopeTan = std::sqrt(1.0f / (slopeNormal * slopeNormal) - 1.0f);
			const float body[3] = { sx, sy, floorZ + 4.0f + boxHalfWidth * slopeTan };
			const TraceHit fit = TraceFull(body, body, bounds, maskPlayerSolidNoBodies);
			if (fit.isStartSolid)
			{
				if (probeVerbose)
				{
					Report("navgen:     slide-down: sample %d/%d: crouch body does not fit at floor %.1f\n",
						   i, samples, floorZ);
				}
				return false;
			}

			if (probeVerbose)
			{
				Report("navgen:     slide-down: sample %d/%d: floor %.1f normal %.2f, fits\n",
					   i, samples, floorZ, normalZ);
			}
			previousFloor = floorZ;
		}

		const bool arrived = std::fabs(previousFloor - to[2]) <= arriveSlack;
		if (probeVerbose)
		{
			Report("navgen:     slide-down: arrival floor %.1f vs target %.1f, %s\n",
				   previousFloor, to[2], arrived ? "within slack" : "too far off");
		}
		return arrived;
	}


	bool CanHop(const float* from, const float* to)
	{
		const float rise = to[2] - from[2];
		if (rise <= stepUpMax || rise > hopRiseMax)
		{
			return false;
		}

		const float bounds[6] = { 0.0f, 0.0f, standHalfHeight, 15.0f, 15.0f, standHalfHeight };
		const float apexZ = from[2] + jumpHeight;

		const float foot[3] = { from[0], from[1], from[2] + groundLift };
		const float apex[3] = { from[0], from[1], apexZ };
		const TraceHit up = TraceFull(foot, apex, bounds, maskPlayerSolidNoBodies);
		if (up.isStartSolid || up.fraction < 1.0f)
		{
			if (probeVerbose)
			{
				Report("navgen:     hop: rise %.0f, no headroom for the jump\n", rise);
			}
			return false;
		}

		const float over[3] = { to[0], to[1], apexZ };
		const TraceHit across = TraceFull(apex, over, bounds, maskPlayerSolidNoBodies);
		if (across.fraction < 1.0f && across.hitType != traceHitTypeGlass)
		{
			if (probeVerbose)
			{
				Report("navgen:     hop: rise %.0f, blocked at %.0f%% of the way across at jump height\n",
					   rise, across.fraction * 100.0f);
			}
			return false;
		}

		const float landing[3] = { to[0], to[1], to[2] + 4.0f };
		const TraceHit down = TraceFull(over, landing, bounds, maskPlayerSolidNoBodies);
		const float landZ = over[2] + (landing[2] - over[2]) * down.fraction;
		if (down.isStartSolid || landZ - landing[2] > 4.0f)
		{
			if (probeVerbose)
			{
				Report("navgen:     hop: rise %.0f, the box stops at %.1f, %.1f above the top\n",
					   rise, landZ, landZ - to[2]);
			}
			return false;
		}

		if (probeVerbose)
		{
			Report("navgen:     hop: rise %.0f, up, across and down all clear\n", rise);
		}
		return true;
	}


	static const char* MantleFlagName(int surfaceFlags)
	{
		const bool isOn = (surfaceFlags & surfMantleOn) != 0;
		const bool isOver = (surfaceFlags & surfMantleOver) != 0;
		if (isOn && isOver)
		{
			return "on+over";
		}
		if (isOn)
		{
			return "on";
		}
		if (isOver)
		{
			return "over";
		}
		return "none";
	}

	static void MantleRefuse(MantleGeometry* geometry, const char* refusal)
	{
		geometry->refusal = refusal;
		if (probeVerbose)
		{
			Report("navgen:     mantle: refused: %s\n", refusal);
		}
	}

	MantleGeometry ProbeMantle(const float* from, float dirX, float dirY)
	{
		MantleGeometry geometry = {};
		geometry.refusal = "";

		const float gateReach = 96.0f;
		const float gateBounds[6] = { 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, standHalfHeight };
		const float gateStart[3] = { from[0] - dirX * mantleCheckBehind, from[1] - dirY * mantleCheckBehind,
									 from[2] + standHalfHeight };
		const float gateEnd[3] = { from[0] + dirX * gateReach, from[1] + dirY * gateReach,
								   from[2] + standHalfHeight };
		const TraceHit gate = TraceFull(gateStart, gateEnd, gateBounds, contentsMantle);
		geometry.hasFace = gate.fraction < 1.0f && !gate.isStartSolid;
		if (!geometry.hasFace)
		{
			geometry.refusal = "no face";
			return geometry;
		}
		geometry.faceDistance = gate.fraction * (gateReach + mantleCheckBehind) - mantleCheckBehind;
		if (geometry.faceDistance < 4.0f)
		{
			geometry.hasFace = false;
			geometry.refusal = "face too close";
			return geometry;
		}
		geometry.isFlagged = (gate.surfaceFlags & surfMantleAny) != 0;
		geometry.isOver = (gate.surfaceFlags & surfMantleOver) != 0;
		++statMantleFaces;

		static const float normalHeights[3] = { 30.0f, 50.0f, 20.0f };
		bool hasSolidFace = false;
		for (const float height : normalHeights)
		{
			const float start[3] = { from[0], from[1], from[2] + height };
			const float end[3] = { from[0] + dirX * gateReach, from[1] + dirY * gateReach, from[2] + height };
			const TraceHit face = TraceFull(start, end, rayBounds, maskPlayerSolidNoBodies);
			if (face.fraction < 1.0f && !face.isStartSolid)
			{
				geometry.faceNormalZ = face.normalZ;
				hasSolidFace = true;
				break;
			}
		}
		if (probeVerbose)
		{
			Report("navgen:     mantle: face at %.0f normal %.2f flags %s\n",
				   geometry.faceDistance, geometry.faceNormalZ, MantleFlagName(gate.surfaceFlags));
		}
		if (!geometry.isFlagged)
		{
			++statMantleUnflagged;
			MantleRefuse(&geometry, "no mantle flag");
			return geometry;
		}
		if (!hasSolidFace)
		{
			MantleRefuse(&geometry, "no solid face behind the mantle tag");
			return geometry;
		}
		if (geometry.faceNormalZ >= faceNormalMax)
		{
			++statMantleSlopes;
			MantleRefuse(&geometry, "a walkable slope, not a face");
			return geometry;
		}

		const float approachEnd = geometry.faceDistance - boxHalfWidth - 1.0f;
		for (float along = mantleApproachStep; along < approachEnd; along += mantleApproachStep)
		{
			float approachZ = 0.0f;
			float approachNormalZ = 0.0f;
			const bool hasFloor = GroundRayBox(from[0] + dirX * along, from[1] + dirY * along, from[2] + stepUpMax,
											   stepUpMax * 2.0f, mantleApproachHalfWidth, &approachZ, &approachNormalZ);
			if (!hasFloor)
			{
				++statMantleNoApproach;
				MantleRefuse(&geometry, "no floor up to the face");
				return geometry;
			}
		}

		static const float ledgeDepths[3] = { 22.0f, 45.0f, 70.0f };
		const char* refusal = "no ledge in the mantle band";
		bool isWalledCounted = false;
		bool hasNearerLedge = false;
		float nearerLedgeZ = 0.0f;
		for (const float depth : ledgeDepths)
		{
			const float px = from[0] + dirX * (geometry.faceDistance + depth);
			const float py = from[1] + dirY * (geometry.faceDistance + depth);
			float ledgeZ = 0.0f;
			float ledgeNormalZ = 0.0f;
			bool hasLedge = GroundRay(px, py, from[2] + 76.0f, 70.0f, &ledgeZ, &ledgeNormalZ);
			if (!hasLedge)
			{
				hasLedge = GroundRay(px, py, from[2] + 76.0f + jumpHeight, jumpHeight, &ledgeZ, &ledgeNormalZ);
			}
			if (!hasLedge)
			{
				continue;
			}
			if (hasNearerLedge && std::fabs(ledgeZ - nearerLedgeZ) > stepUpMax)
			{
				refusal = "a nearer ledge is where the engine lands";
				break;
			}
			if (!hasNearerLedge)
			{
				hasNearerLedge = true;
				nearerLedgeZ = ledgeZ;
			}

			const float rise = ledgeZ - from[2];
			const bool needsJump = rise > mantleUpMax;
			if (probeVerbose)
			{
				const char* jumpNote = "";
				if (needsJump)
				{
					jumpNote = " (a jump first)";
				}
				Report("navgen:     mantle: ledge at depth %.0f rise %.0f normal %.2f%s\n", depth, rise, ledgeNormalZ, jumpNote);
			}
			if (rise <= stepUpMax || rise > mantleJumpReliableMax)
			{
				refusal = "ledge rise outside the mantle band";
				continue;
			}
			if (ledgeNormalZ < walkableNormal)
			{
				refusal = "ledge too steep";
				continue;
			}

			static const float ledgeTiers[3] = { 20.0f, 40.0f, 60.0f };
			const float standBack = geometry.faceDistance - boxHalfWidth - 1.0f;
			const float standX = from[0] + dirX * standBack;
			const float standY = from[1] + dirY * standBack;
			const float clearBounds[6] = { 0.0f, 0.0f, 15.0f, 15.0f, 15.0f, 15.0f };
			float originLift = 0.0f;
			if (needsJump)
			{
				originLift = jumpHeight;
			}
			bool hasOpening = false;
			for (const float tier : ledgeTiers)
			{
				if (tier <= rise - originLift)
				{
					continue;
				}
				const float clearStart[3] = { standX, standY, from[2] + originLift + tier };
				const float clearEnd[3] = { standX + dirX * 16.0f, standY + dirY * 16.0f, from[2] + originLift + tier };
				if (ClearThroughGlass(clearStart, clearEnd, clearBounds))
				{
					hasOpening = true;
					break;
				}
			}
			if (!hasOpening)
			{
				refusal = "no opening above the ledge";
				continue;
			}

			GroundHit ground = SnapToGround(px, py, ledgeZ - 10.0f, 20.0f);
			if (!ground.isValid)
			{
				ground = GroundFitNoGlass(px, py, ledgeZ);
			}
			if (!ground.isValid || std::fabs(ground.z - ledgeZ) > 12.0f)
			{
				refusal = "no room for the body on the ledge";
				continue;
			}

			if (depth > ledgeDepths[0])
			{
				float walkHalfHeight = standHalfHeight;
				if (ground.needsCrouch)
				{
					walkHalfHeight = crouchHalfHeight;
				}
				const float walkBounds[6] = { 0.0f, 0.0f, walkHalfHeight, boxHalfWidth, boxHalfWidth, walkHalfHeight };
				const float nearDepth = geometry.faceDistance + ledgeDepths[0];
				const float walkStart[3] = { from[0] + dirX * nearDepth, from[1] + dirY * nearDepth, ledgeZ + stepUpMax };
				const float walkEnd[3] = { px, py, ledgeZ + stepUpMax };
				if (!ClearThroughGlass(walkStart, walkEnd, walkBounds))
				{
					if (!isWalledCounted)
					{
						++statMantleWalled;
						isWalledCounted = true;
					}
					refusal = "a wall between the face and the ledge";
					continue;
				}
			}

			geometry.ledgeDepth = depth;
			geometry.ledgeZ = ledgeZ;
			geometry.ledgeNormalZ = ledgeNormalZ;
			geometry.needsCrouch = ground.needsCrouch;
			geometry.needsJump = needsJump;
			geometry.landing[0] = px;
			geometry.landing[1] = py;
			geometry.landing[2] = ground.z;
			geometry.hasFooting = HasSolidFooting(px, py, ground.z);
			if (geometry.hasFooting || !geometry.isOver || depth != ledgeDepths[0])
			{
				geometry.isValid = true;
				if (probeVerbose)
				{
					Report("navgen:     mantle: lands at %.0f %.0f %.1f%s, footing %s\n", px, py, ground.z,
						   ground.needsCrouch ? " (crouch)" : "", geometry.hasFooting ? "ok" : "none");
				}
				return geometry;
			}

			const float farX = px + dirX * mantleOverReach;
			const float farY = py + dirY * mantleOverReach;
			const float overBounds[6] = { 0.0f, 0.0f, crouchHalfHeight, 7.5f, 7.5f, crouchHalfHeight };
			const float overStart[3] = { px, py, ledgeZ + 1.0f };
			const float overEnd[3] = { farX, farY, ledgeZ + 1.0f };
			const TraceHit over = TraceFull(overStart, overEnd, overBounds, maskPlayerSolidNoBodies);
			if (over.isStartSolid || over.fraction < 1.0f)
			{
				refusal = "over: the obstacle is too thick to vault";
				continue;
			}

			float floorZ = 0.0f;
			float floorNormalZ = 0.0f;
			if (!GroundRay(farX, farY, ledgeZ + 1.0f, dropLinkMax + 1.0f, &floorZ, &floorNormalZ)
				|| ledgeZ - floorZ < mantleOverDrop || floorNormalZ < walkableNormal)
			{
				refusal = "over: no walkable floor beyond the obstacle";
				continue;
			}

			const GroundHit farGround = SnapToGround(farX, farY, floorZ - 40.0f, 20.0f);
			if (!farGround.isValid)
			{
				refusal = "over: no room for the body beyond the obstacle";
				continue;
			}

			geometry.isValid = true;
			geometry.needsCrouch = farGround.needsCrouch;
			geometry.needsJump = needsJump;
			geometry.landing[0] = farX;
			geometry.landing[1] = farY;
			geometry.landing[2] = farGround.z;
			geometry.hasFooting = HasSolidFooting(farX, farY, farGround.z);
			if (probeVerbose)
			{
				Report("navgen:     mantle: over the obstacle, lands at %.0f %.0f %.1f (%.0f below the top), footing %s\n",
					   farX, farY, farGround.z, ledgeZ - farGround.z, geometry.hasFooting ? "ok" : "none");
			}
			return geometry;
		}

		MantleRefuse(&geometry, refusal);
		return geometry;
	}


	bool FindLadder(const float* from, float dirX, float dirY, LadderHit* out)
	{
		*out = {};

		const float feetBounds[6] = { 0.0f, 0.0f, 0.0f, 9.0f, 9.0f, 4.0f };

		static const float grabLifts[2] = { 0.0f, jumpHeight };
		const float grabBounds[6] = { 0.0f, 0.0f, ladderGrabMid, 9.0f, 9.0f, ladderGrabHalf };
		TraceHit grab = {};
		float grabHeight = 0.0f;
		bool hasGrab = false;
		for (const float lift : grabLifts)
		{
			const float start[3] = { from[0], from[1], from[2] + lift };
			const float end[3] = { from[0] + dirX * ladderProbeReach, from[1] + dirY * ladderProbeReach,
								   from[2] + lift };
			grab = TraceFull(start, end, grabBounds, maskPlayerSolidNoBodies);
			if (grab.isStartSolid || grab.fraction >= 1.0f)
			{
				continue;
			}
			if ((grab.surfaceFlags & surfLadder) == 0)
			{
				continue;
			}
			hasGrab = true;
			grabHeight = lift + ladderGrabMid;
			break;
		}
		if (!hasGrab)
		{
			return false;
		}
		++statLadderFaces;
		out->needsJump = grabHeight > ladderGrabMid;

		float normalX = grab.normal[0];
		float normalY = grab.normal[1];
		const float normalLength = std::sqrt(normalX * normalX + normalY * normalY);
		if (normalLength < 0.5f)
		{
			return false;
		}
		normalX /= normalLength;
		normalY /= normalLength;
		const float boxReach = 9.0f * (std::fabs(normalX) + std::fabs(normalY));
		const float planeX = from[0] + dirX * grab.fraction * ladderProbeReach - normalX * boxReach;
		const float planeY = from[1] + dirY * grab.fraction * ladderProbeReach - normalY * boxReach;
		if (probeVerbose)
		{
			const char* jumpNote = "";
			if (out->needsJump)
			{
				jumpNote = " (a jump grabs it)";
			}
			Report("navgen:     ladder: rung at %.0f ahead, %.0f up, plane at %.0f %.0f facing %.2f %.2f%s\n",
				   grab.fraction * ladderProbeReach, grabHeight, planeX, planeY, normalX, normalY, jumpNote);
		}

		float lastHitZ = from[2] + grabHeight;
		float lastFraction = grab.fraction;
		for (int step = 1; step <= 16; ++step)
		{
			const float stepZ = from[2] + grabHeight + 40.0f * static_cast<float>(step);
			const float stepStart[3] = { from[0], from[1], stepZ };
			const float stepEnd[3] = { from[0] + dirX * ladderProbeReach, from[1] + dirY * ladderProbeReach, stepZ };
			const TraceHit rung = TraceFull(stepStart, stepEnd, feetBounds, maskPlayerSolidNoBodies);
			if (rung.isStartSolid || rung.fraction >= 1.0f || (rung.surfaceFlags & surfLadder) == 0)
			{
				break;
			}
			lastHitZ = stepZ;
			lastFraction = rung.fraction;
		}

		const float topPlaneX = from[0] + dirX * lastFraction * ladderProbeReach - normalX * boxReach;
		const float topPlaneY = from[1] + dirY * lastFraction * ladderProbeReach - normalY * boxReach;
		float searchDepth = lastHitZ - (from[2] + 20.0f);
		if (searchDepth > ladderTopSearch)
		{
			searchDepth = ladderTopSearch;
		}
		if (searchDepth < 20.0f)
		{
			searchDepth = 20.0f;
		}
		static const float exitDepths[3] = { 20.0f, 40.0f, 60.0f };
		for (const float depth : exitDepths)
		{
			const float topX = topPlaneX - normalX * depth;
			const float topY = topPlaneY - normalY * depth;
			const GroundHit top = SnapToGround(topX, topY, lastHitZ + 8.0f, searchDepth);
			if (!top.isValid || top.z - from[2] <= stepUpMax)
			{
				continue;
			}

			out->isValid = true;
			out->bottom[0] = planeX + normalX * ladderStandOff;
			out->bottom[1] = planeY + normalY * ladderStandOff;
			out->bottom[2] = from[2];
			out->top[0] = topX;
			out->top[1] = topY;
			out->top[2] = top.z;
			out->normal[0] = normalX;
			out->normal[1] = normalY;
			if (probeVerbose)
			{
				Report("navgen:     ladder: top at %.0f %.0f %.1f, %.0f up, %.0f past the plane\n",
					   topX, topY, top.z, top.z - from[2], depth);
			}
			return true;
		}

		++statLadderNoTop;
		if (probeVerbose)
		{
			Report("navgen:     ladder: last rung at %.1f, no floor to step off onto past the plane\n", lastHitZ);
		}
		return false;
	}


	bool CanDrop(const float* from, const float* to)
	{
		return CanDropUpTo(from, to, dropLinkMax);
	}

	bool CanDropDeep(const float* from, const float* to)
	{
		return CanDropUpTo(from, to, dropDeepMax);
	}


	bool MantleFaceAhead(const float* start, const float* end, float* outNormalZ)
	{
		const float bounds[6] = { 0.0f, 0.0f, 37.0f, 0.1f, 0.1f, 34.0f };
		const TraceHit solid = TraceFull(start, end, bounds, maskPlayerSolidNoBodies);
		if (outNormalZ)
		{
			*outNormalZ = solid.normalZ;
		}
		if (solid.fraction >= 1.0f || solid.normalZ >= faceNormalMax)
		{
			return false;
		}

		const TraceHit gate = TraceFull(start, end, bounds, contentsMantle);
		return gate.fraction < 1.0f && !gate.isStartSolid && (gate.surfaceFlags & surfMantleAny) != 0;
	}
}
