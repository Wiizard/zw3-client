#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamFriends.hpp"

#include "Components/Modules/Dvar.hpp"

namespace Steam
{
	void* const Friends::vtable[] =
	{
		reinterpret_cast<void*>(GetPersonaName),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(ActivateGameOverlay),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(ActivateGameOverlayToStore),
	};

	Interface Friends::object = { Friends::vtable };

	Interface* Friends::Get()
	{
		static_assert(std::size(vtable) == 25);
		return &object;
	}

	const char* Friends::GetPersonaName([[maybe_unused]] Interface* self)
	{
		return Components::Dvar::Name.Get<const char*>();
	}

	void Friends::ActivateGameOverlay([[maybe_unused]] Interface* self, [[maybe_unused]] const char* dialog)
	{
	}

	void Friends::ActivateGameOverlayToStore([[maybe_unused]] Interface* self, [[maybe_unused]] unsigned int appId, [[maybe_unused]] int flag)
	{
	}
}
