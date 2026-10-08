#include "STDInclude.hpp"

#include "Controller/Mapping/Logical.hpp"

namespace Controller::Mapping
{
	const char* CommandFor(Action action) noexcept
	{
		switch (action)
		{
		case Action::None:
			return "";
		case Action::Fire:
			return "+attack";
		case Action::Ads:
			return "+speed_throw";
		case Action::AdsToggle:
			return "+toggleads_throw";
		case Action::JumpStand:
			return "+gostand";
		case Action::Stance:
			return "+stance";
		case Action::Melee:
			return "+melee";
		case Action::UseReload:
			return "+usereload";
		case Action::Sprint:
			return "+breath_sprint";
		case Action::NextWeapon:
			return "weapnext";
		case Action::Frag:
			return "+frag";
		case Action::SpecialGrenade:
			return "+smoke";
		case Action::Menu:
			return "togglemenu";
		case Action::Scoreboard:
			return "+scores";
		case Action::ActionSlot1:
			return "+actionslot 1";
		case Action::ActionSlot2:
			return "+actionslot 2";
		case Action::ActionSlot3:
			return "+actionslot 3";
		case Action::ActionSlot4:
			return "+actionslot 4";
		}

		return "";
	}
}
