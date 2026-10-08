#pragma once

namespace Components::LobbyCombat
{
	inline constexpr float admissionProgress = 0.92f;
	inline constexpr float routeSpacing = 0.03f;
	inline constexpr float attackDistance = 44.0f;
	inline constexpr float meleeDistance = 58.0f;
	inline constexpr float meleeReach = 68.0f;
	inline constexpr float gunSafetyDistance = 72.0f;
	inline constexpr unsigned int recoveryMs = 900;
	inline constexpr float approachRow = -665.0f;
	inline constexpr float balconyRow = -727.0f;

	constexpr unsigned int Population(int characters)
	{
		return 6u + 2u * static_cast<unsigned int>(std::clamp(characters, 1, 4));
	}

	constexpr unsigned int SpawnInterval(int characters)
	{
		return 2600u - 200u * static_cast<unsigned int>(std::clamp(characters, 1, 4));
	}

	constexpr unsigned int FireCooldown(int characters)
	{
		return 3500u + 350u * static_cast<unsigned int>(std::clamp(characters, 1, 4));
	}

	constexpr unsigned int PressureCooldown(int characters, unsigned int nearby)
	{
		if (nearby >= 4)
		{
			return 1100u;
		}

		if (nearby >= 2)
		{
			return 2200u;
		}

		return FireCooldown(characters);
	}

	inline bool InMeleeSweep(float dx, float dy, float facing)
	{
		const float distance = std::hypot(dx, dy);

		if (distance > meleeReach)
		{
			return false;
		}

		if (distance < 0.001f)
		{
			return true;
		}

		return (dx * std::cos(facing) + dy * std::sin(facing)) / distance >= 0.5f;
	}

	template <typename Encounter>
	void ReleaseEncounter(Encounter& encounter)
	{
		encounter.targetSurvivor = -1;
		encounter.isAdmitted = false;
		encounter.isAttacking = false;
		encounter.approachPhase = 0;
		encounter.nextAttackTime = 0;
	}

	constexpr bool IsOlderEncounter(unsigned int time, unsigned int index, unsigned int otherTime, unsigned int otherIndex)
	{
		if (time == otherTime)
		{
			return index < otherIndex;
		}

		return static_cast<std::int32_t>(time - otherTime) < 0;
	}

	template <typename Encounter>
	float AdvanceApproach(Encounter& encounter, float homeX, float budget, bool isWaiting)
	{
		float& x = encounter.location.x;
		float& y = encounter.location.y;
		int& phase = encounter.approachPhase;
		const float startX = x;
		const float startY = y;
		budget = std::max(0.0f, budget);

		for (int stage = 0; stage < 3 && budget > 0.0f; ++stage)
		{
			float goalX = homeX;

			if (phase == 0)
			{
				goalX = x;
			}

			float goalY = balconyRow;

			if (phase < 2 || isWaiting)
			{
				goalY = approachRow;
			}

			const float dx = goalX - x;
			const float dy = goalY - y;
			const float distance = std::hypot(dx, dy);

			if (distance <= 0.001f)
			{
				if (phase < 2)
				{
					++phase;
					continue;
				}

				break;
			}

			const float step = std::min(distance, budget);
			x += dx / distance * step;
			y += dy / distance * step;
			budget -= step;

			if (step < distance || phase == 2)
			{
				break;
			}

			++phase;
		}

		return std::hypot(x - startX, y - startY);
	}

	inline bool ClipSlab(float start, float delta, float low, float high, float& entry, float& exit)
	{
		if (delta == 0.0f)
		{
			return start >= low && start <= high;
		}

		float a = (low - start) / delta;
		float b = (high - start) / delta;

		if (a > b)
		{
			std::swap(a, b);
		}

		entry = std::max(entry, a);
		exit = std::min(exit, b);
		return entry <= exit;
	}

	inline bool IsShotClear(float fromX, float fromY, float toX, float toY)
	{
		for (const float side : { -1.0f, 1.0f })
		{
			float entry = 0.0f;
			float exit = 1.0f;

			const bool crossesX = ClipSlab(fromX * side, (toX - fromX) * side, 152.0f, 208.0f, entry, exit);

			if (crossesX && ClipSlab(fromY, toY - fromY, -770.0f, -742.0f, entry, exit))
			{
				return false;
			}
		}

		return true;
	}
}
