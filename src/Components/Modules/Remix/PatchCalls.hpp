#pragma once

#include "Iw4.hpp"

namespace Components::Remix
{
	bool TryPatchCallGroup(const Iw4::CallSite* calls, int count, const void* detour, void** original);
}
