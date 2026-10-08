#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "Steam/Proxy.hpp"
#include "SteamUtils.hpp"

namespace Steam
{
	void* const Utils::vtable[] =
	{
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(GetAppID),
		reinterpret_cast<void*>(SetOverlayNotificationPosition),
	};

	Interface Utils::object = { Utils::vtable };

	Interface* Utils::Get()
	{
		static_assert(std::size(vtable) == 11);
		return &object;
	}

	unsigned int Utils::GetAppID([[maybe_unused]] Interface* self)
	{
		return 10190;
	}

	void Utils::SetOverlayNotificationPosition([[maybe_unused]] Interface* self, int position)
	{
		Proxy::SetOverlayNotificationPosition(static_cast<std::uint32_t>(position));
	}
}
