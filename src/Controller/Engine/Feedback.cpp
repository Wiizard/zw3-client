#include "STDInclude.hpp"

#include "Controller/Engine/Feedback.hpp"
#include "Controller/Engine/Engine.hpp"
#include "Controller/Mapping/Key.hpp"

namespace Controller::Engine
{
	using Driver::AdaptiveTriggerRequest;
	using Driver::TriggerEffect;

	enum class TriggerRole : std::uint8_t
	{
		None,
		Firing,
		Aiming,
		PrimaryOffhand,
		SecondaryOffhand,
	};

	struct TriggerTuning
	{
		std::uint8_t light = 0;
		std::uint8_t heavy = 0;

		std::uint8_t lightStart = 0;
		std::uint8_t lightEnd = 0;
		std::uint8_t hardStart = 0;
		std::uint8_t hardEnd = 0;

		std::uint8_t ads = 0;
	};

	static constexpr std::uint8_t slightStrength = 7;
	static constexpr std::uint8_t heavyStrength = 8;

	static constexpr std::uint8_t lightBreakStart = 2;
	static constexpr std::uint8_t lightBreakEnd = 5;
	static constexpr std::uint8_t hardBreakStart = 3;
	static constexpr std::uint8_t hardBreakEnd = 7;

	static constexpr std::uint8_t maxStrength = 8;
	static constexpr auto maxPosition = static_cast<std::uint8_t>(Driver::triggerZoneCount - 1);

	static TriggerRole RoleFor(Mapping::EngineKey key)
	{
		static const int attackBinding = Game::Key_GetBindingForCmd("+attack");
		static const int speedThrowBinding = Game::Key_GetBindingForCmd("+speed_throw");
		static const int toggleAdsThrowBinding = Game::Key_GetBindingForCmd("+toggleads_throw");
		static const int fragBinding = Game::Key_GetBindingForCmd("+frag");
		static const int smokeBinding = Game::Key_GetBindingForCmd("+smoke");

		const int binding = Game::playerKeys[localClient].keys[static_cast<int>(key)].binding;

		if (binding == 0)
		{
			return TriggerRole::None;
		}

		if (binding == attackBinding)
		{
			return TriggerRole::Firing;
		}

		if (binding == speedThrowBinding || binding == toggleAdsThrowBinding)
		{
			return TriggerRole::Aiming;
		}

		if (binding == fragBinding)
		{
			return TriggerRole::PrimaryOffhand;
		}

		if (binding == smokeBinding)
		{
			return TriggerRole::SecondaryOffhand;
		}

		return TriggerRole::None;
	}

	static AdaptiveTriggerRequest Released(TriggerSide side) noexcept
	{
		AdaptiveTriggerRequest request;
		request.side = side;
		request.effect = TriggerEffect::Off;
		return request;
	}

	static AdaptiveTriggerRequest ProfileRequest(TriggerSide side, const Driver::TriggerProfile& zones) noexcept
	{
		AdaptiveTriggerRequest request;
		request.side = side;
		request.effect = TriggerEffect::Feedback;
		request.zones = zones;
		return request;
	}

	static Driver::TriggerProfile Ramp(std::uint8_t from, std::uint8_t to) noexcept
	{
		Driver::TriggerProfile zones{};

		constexpr int last = static_cast<int>(Driver::triggerZoneCount - 1);

		for (std::size_t i = 0; i != Driver::triggerZoneCount; ++i)
		{
			zones[i] = static_cast<std::uint8_t>(from + (static_cast<int>(to) - from) * static_cast<int>(i) / last);
		}

		return zones;
	}

	static Driver::TriggerProfile Flat(std::uint8_t strength) noexcept
	{
		Driver::TriggerProfile zones{};
		zones.fill(strength);
		return zones;
	}

	static AdaptiveTriggerRequest Section(TriggerSide side, std::uint8_t start, std::uint8_t end, std::uint8_t strength) noexcept
	{
		AdaptiveTriggerRequest request;
		request.side = side;
		request.effect = TriggerEffect::Weapon;
		request.startPosition = start;
		request.endPosition = end;
		request.strength = strength;
		return request;
	}

	static std::uint8_t ClampTo(int value, std::uint8_t limit) noexcept
	{
		if (value < 0)
		{
			return 0;
		}

		if (value > limit)
		{
			return limit;
		}

		return static_cast<std::uint8_t>(value);
	}

	static std::uint8_t ScaledStrength(int configured, float scale) noexcept
	{
		const float clampedScale = std::clamp(scale, 0.0f, 1.0f);
		const float strength = static_cast<float>(ClampTo(configured, maxStrength)) * clampedScale;

		return ClampTo(static_cast<int>(strength + 0.5f), maxStrength);
	}

	static TriggerTuning ReadTuning(const Dvars& dvars) noexcept
	{
		const float scale = Read(dvars.adaptiveTriggerStrength, 1.0f);

		TriggerTuning tuning;

		tuning.light = ScaledStrength(Read(dvars.adaptiveTriggerLight, static_cast<int>(slightStrength)), scale);
		tuning.heavy = ScaledStrength(Read(dvars.adaptiveTriggerHeavy, static_cast<int>(heavyStrength)), scale);
		tuning.ads = ScaledStrength(Read(dvars.adaptiveTriggerAds, 0), scale);

		tuning.lightStart = ClampTo(Read(dvars.adaptiveTriggerLightStart, static_cast<int>(lightBreakStart)), maxPosition);
		tuning.lightEnd = ClampTo(Read(dvars.adaptiveTriggerLightEnd, static_cast<int>(lightBreakEnd)), maxPosition);
		tuning.hardStart = ClampTo(Read(dvars.adaptiveTriggerHeavyStart, static_cast<int>(hardBreakStart)), maxPosition);
		tuning.hardEnd = ClampTo(Read(dvars.adaptiveTriggerHeavyEnd, static_cast<int>(hardBreakEnd)), maxPosition);

		tuning.lightEnd = std::max(tuning.lightEnd, tuning.lightStart);
		tuning.hardEnd = std::max(tuning.hardEnd, tuning.hardStart);

		return tuning;
	}

	static AdaptiveTriggerRequest FiringFeedback(TriggerSide side, const Game::playerState_s& playerState, const TriggerTuning& tuning)
	{
		const unsigned int weaponIndex = Game::BG_GetViewmodelWeaponIndex(&playerState);

		if (weaponIndex == 0)
		{
			return Released(side);
		}

		const auto* weaponDef = Game::BG_GetWeaponDef(weaponIndex);

		if (weaponDef == nullptr)
		{
			return Released(side);
		}

		switch (weaponDef->weapClass)
		{
		case Game::WEAPCLASS_MG:
		case Game::WEAPCLASS_RIFLE:
		case Game::WEAPCLASS_TURRET:
			return ProfileRequest(side, Flat(tuning.heavy));

		case Game::WEAPCLASS_SMG:
			return ProfileRequest(side, Ramp(tuning.light, tuning.heavy));

		case Game::WEAPCLASS_PISTOL:
			return Section(side, tuning.lightStart, tuning.lightEnd, tuning.light);

		case Game::WEAPCLASS_SPREAD:
		case Game::WEAPCLASS_SNIPER:
		case Game::WEAPCLASS_ROCKETLAUNCHER:
			return Section(side, tuning.hardStart, tuning.hardEnd, tuning.heavy);

		default:
			return Released(side);
		}
	}

	static AdaptiveTriggerRequest OffhandFeedback(TriggerSide side, const Game::playerState_s& playerState, bool isPrimary, const TriggerTuning& tuning)
	{
		auto held = playerState.weapCommon.offhandSecondary;

		if (isPrimary)
		{
			held = playerState.weapCommon.offhandPrimary;
		}

		if (held == Game::OFFHAND_CLASS_NONE)
		{
			return Released(side);
		}

		return Section(side, tuning.lightStart, tuning.lightEnd, tuning.light);
	}

	static AdaptiveTriggerRequest AimingFeedback(TriggerSide side, const TriggerTuning& tuning)
	{
		if (tuning.ads == 0)
		{
			return Released(side);
		}

		return ProfileRequest(side, Flat(tuning.ads));
	}

	static AdaptiveTriggerRequest EffectFor(TriggerSide side, TriggerRole role, const Game::playerState_s& playerState, const TriggerTuning& tuning)
	{
		switch (role)
		{
		case TriggerRole::Firing:
			return FiringFeedback(side, playerState, tuning);

		case TriggerRole::Aiming:
			return AimingFeedback(side, tuning);

		case TriggerRole::None:
			return Released(side);

		case TriggerRole::PrimaryOffhand:
			return OffhandFeedback(side, playerState, true, tuning);

		case TriggerRole::SecondaryOffhand:
			return OffhandFeedback(side, playerState, false, tuning);
		}

		return Released(side);
	}

	bool TryEvaluateTriggerFeedback(const Dvars& dvars, int client, AdaptiveTriggerRequest& left, AdaptiveTriggerRequest& right)
	{
		if (!Read(dvars.adaptiveTriggers, false))
		{
			return false;
		}

		const auto* cg = Game::CL_GetLocalClientGlobals(client);

		if (cg == nullptr || cg->snap == nullptr)
		{
			return false;
		}

		const auto& playerState = cg->snap->ps;

		const TriggerRole leftRole = RoleFor(Mapping::EngineKey::ButtonLTrigger);
		const TriggerRole rightRole = RoleFor(Mapping::EngineKey::ButtonRTrigger);

		const TriggerTuning tuning = ReadTuning(dvars);

		left = EffectFor(TriggerSide::Left, leftRole, playerState, tuning);
		right = EffectFor(TriggerSide::Right, rightRole, playerState, tuning);

		if (leftRole != TriggerRole::Firing || rightRole != TriggerRole::Firing)
		{
			return true;
		}

		const unsigned int weaponIndex = Game::BG_GetViewmodelWeaponIndex(&playerState);

		if (weaponIndex == 0)
		{
			return true;
		}

		const auto* equipped = Game::BG_GetEquippedWeaponState(const_cast<Game::playerState_s*>(&playerState), weaponIndex);

		if (equipped != nullptr && equipped->dualWielding)
		{
			right = left;
			right.side = TriggerSide::Right;
		}

		return true;
	}
}
