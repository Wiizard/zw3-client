#include "STDInclude.hpp"

#include "Bullet.hpp"
#include "Events.hpp"
#include "Logger.hpp"

namespace Components
{
	Dvar::Var Bullet::bg_surfacePenetration;
	Game::dvar_t* Bullet::bg_bulletRange;

	constexpr std::uintptr_t BG_GetSurfacePenetrationDepth = 0x14009C840;
	constexpr std::uintptr_t penetrationDepthCalls[] = { 0x1400C6A16, 0x1400C6E73, 0x1400C6E90, 0x1401620C8, 0x140162427, 0x140162442 };

	constexpr std::uintptr_t Bullet_Fire_RangeLoad = 0x140161BE5;
	static const std::uint8_t rangeLoad[] = { 0xF3, 0x0F, 0x10, 0x35, 0x7F, 0xDE, 0x20, 0x00 };
	constexpr std::size_t rangeLoadLength = sizeof(rangeLoad);

	constexpr std::uintptr_t Bullet_Fire_SrandCall = 0x140161C12;
	constexpr std::uintptr_t BG_srand = 0x14008E280;

	static Utils::Hook hooks[std::size(penetrationDepthCalls) + 1];

	float Bullet::BG_GetSurfacePenetrationDepth_Hk(const Game::WeaponDef* weapDef, int surfaceType)
	{
		assert(weapDef);

		const auto penetrationDepth = bg_surfacePenetration.Get<float>();

		if (penetrationDepth > 0.0f)
		{
			return penetrationDepth;
		}

		return reinterpret_cast<float(*)(const Game::WeaponDef*, int)>(Utils::Hook::Rebase(BG_GetSurfacePenetrationDepth))(weapDef, surfaceType);
	}

	void Bullet::BG_srand_Hk(unsigned int* pHoldrand)
	{
		*pHoldrand = static_cast<unsigned int>(std::rand());
	}

	Bullet::Bullet()
	{
		bool isExpected = Utils::Hook::MatchesBytes(Bullet_Fire_RangeLoad, rangeLoad, sizeof(rangeLoad))
			&& Utils::Hook::BranchesTo(Bullet_Fire_SrandCall, BG_srand, false);

		for (const auto call : penetrationDepthCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, BG_GetSurfacePenetrationDepth, false);
		}

		if (!isExpected)
		{
			Logger::Error("bullet: Bullet_Fire does not read as expected, no bullet dvars\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(penetrationDepthCalls); ++i)
		{
			isSeated = hooks[i].Initialize(penetrationDepthCalls[i], reinterpret_cast<void*>(BG_GetSurfacePenetrationDepth_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[std::size(penetrationDepthCalls)].Initialize(Bullet_Fire_SrandCall, reinterpret_cast<void*>(BG_srand_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("bullet: could not seat every hook, no bullet dvars\n");
			return;
		}

		Events::OnDvarInit([]
		{
			bg_surfacePenetration = Dvar::Register("bg_surfacePenetration", 0.0f, 0.0f, std::numeric_limits<float>::max(), Game::DVAR_CODINFO, "Set to a value greater than 0 to override the surface penetration depth");
			bg_bulletRange = Dvar::Register("bg_bulletRange", 8192.0f, 0.0f, std::numeric_limits<float>::max(), Game::DVAR_CODINFO, "Max range used when calculating the bullet end position").Get();

			const auto loadEnd = Utils::Hook::Rebase(Bullet_Fire_RangeLoad) + rangeLoadLength;
			const auto distance = reinterpret_cast<std::int64_t>(&bg_bulletRange->current.value) - static_cast<std::int64_t>(loadEnd);

			if (distance < std::numeric_limits<std::int32_t>::min() || distance > std::numeric_limits<std::int32_t>::max())
			{
				Logger::Error("bullet: bg_bulletRange is out of Bullet_Fire's reach, the range stays 8192\n");
				return;
			}

			Utils::Hook::Set<std::int32_t>(Bullet_Fire_RangeLoad + 4, static_cast<std::int32_t>(distance));
		});
	}
}
