#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamApps.hpp"

#include "Components/Modules/Maps.hpp"

namespace Steam
{
	void* const Apps::vtable[] =
	{
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(BIsDlcInstalled),
	};

	Interface Apps::object = { Apps::vtable };

	Interface* Apps::Get()
	{
		static_assert(std::size(vtable) == 8);
		return &object;
	}

	bool Apps::BIsDlcInstalled([[maybe_unused]] Interface* self, unsigned int appId)
	{
		if (appId == 10195)
		{
			return Components::Maps::IsDlcInstalled(1);
		}

		if (appId == 10196)
		{
			return Components::Maps::IsDlcInstalled(2);
		}

		return false;
	}
}
