#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Components::LobbyCombat
{
	constexpr unsigned Population(int characters) { return 6u + 2u * std::clamp(characters, 1, 4); }
	constexpr unsigned InitialPopulation(int characters) { return 2u + std::clamp(characters, 1, 4); }
	constexpr unsigned SpawnInterval(int characters) { return 2600u - 200u * std::clamp(characters, 1, 4); }
	constexpr unsigned FireCooldown(int characters) { return 3500u + 350u * std::clamp(characters, 1, 4); }
	constexpr unsigned PressureCooldown(int characters, unsigned nearby)
	{
		return nearby >= 4 ? 1100u : nearby >= 2 ? 2200u : FireCooldown(characters);
	}
	inline bool InMeleeSweep(float dx, float dy, float facing)
	{
		const auto distance = std::hypot(dx, dy);
		return distance <= 68.0f && (distance < 0.001f ||
			(dx * std::cos(facing) + dy * std::sin(facing)) / distance >= 0.5f);
	}
	constexpr float AdmissionProgress = 0.92f;
	constexpr float RouteSpacing = 0.03f;
	constexpr float WaitingDistance = 98.0f;
	constexpr float AttackDistance = 44.0f;
	constexpr float MeleeDistance = 58.0f;
	constexpr float GunSafetyDistance = 72.0f;
	constexpr unsigned RecoveryMs = 900u;
	constexpr float ApproachRow = -665.0f;
	template <typename Encounter>
	void ReleaseEncounter(Encounter& encounter)
	{
		encounter.targetSurvivor = -1;
		encounter.admitted = false;
		encounter.attacking = false;
		encounter.approachPhase = 0;
		encounter.nextAttackTime = 0;
	}
	constexpr bool OlderEncounter(unsigned time, unsigned index, unsigned otherTime, unsigned otherIndex)
	{
		return time == otherTime ? index < otherIndex : static_cast<std::int32_t>(time - otherTime) < 0;
	}

	// Enter the rear row, align with the assigned character, then approach straight
	// ahead. Crossing diagonally toward an inner slot passes through its neighbours.
	inline float AdvanceApproach(float& x, float& y, int& phase, float homeX, float dt, float speed, bool waiting)
	{
		const auto startX = x, startY = y;
		auto budget = std::max(0.0f, dt * speed);
		for (int stage = 0; stage < 3 && budget > 0.0f; ++stage)
		{
			const auto goalX = phase == 0 ? x : homeX;
			const auto goalY = phase < 2 || waiting ? ApproachRow : -727.0f;
			const auto dx = goalX - x, dy = goalY - y;
			const auto distance = std::hypot(dx, dy);
			if (distance <= 0.001f)
			{
				if (phase < 2) { ++phase; continue; }
				break;
			}
			const auto step = std::min(distance, budget);
			x += dx / distance * step;
			y += dy / distance * step;
			budget -= step;
			if (step < distance || phase == 2) break;
			++phase;
		}
		return std::hypot(x - startX, y - startY);
	}

	// Balcony side panels occupy these rectangles from floor height upward.
	inline bool ClearShot(float fromX, float fromY, float toX, float toY)
	{
		for (const float side : {-1.0f, 1.0f})
		{
			float entry = 0.0f, exit = 1.0f;
			const auto axis = [&](float start, float delta, float low, float high)
			{
				if (delta == 0.0f) return start >= low && start <= high;
				auto a = (low - start) / delta, b = (high - start) / delta;
				if (a > b) std::swap(a, b);
				entry = std::max(entry, a);
				exit = std::min(exit, b);
				return entry <= exit;
			};
			if (axis(fromX * side, (toX - fromX) * side, 152.0f, 208.0f) &&
				axis(fromY, toY - fromY, -770.0f, -742.0f)) return false;
		}
		return true;
	}
}
