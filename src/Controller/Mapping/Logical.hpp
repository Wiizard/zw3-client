#pragma once

#include "Controller/Types.hpp"

namespace Controller::Mapping
{
	enum class Action : std::uint8_t
	{
		None,

		Fire,
		Ads,
		AdsToggle,
		JumpStand,
		Stance,
		Melee,
		UseReload,
		Sprint,
		NextWeapon,
		Frag,
		SpecialGrenade,
		Menu,

		Scoreboard,
		ActionSlot1,
		ActionSlot2,
		ActionSlot3,
		ActionSlot4,
	};

	const char* CommandFor(Action action) noexcept;
}
