#include "STDInclude.hpp"

#include "PatchCalls.hpp"

#include <cstdint>

#include "Engine.hpp"
#include "Hook.hpp"

namespace Components::Remix
{
	bool TryPatchCallGroup(const Iw4::CallSite* calls, int count, const void* detour, void** original)
	{
		std::uintptr_t resolved = 0;

		for (int i = 0; i < count; ++i)
		{
			if (calls[i].address == 0)
			{
				return false;
			}

			const std::uintptr_t target = Hook::ResolveCallTarget(Engine::RebaseAddress(calls[i].address));

			if (!target || (resolved != 0 && target != resolved))
			{
				return false;
			}

			resolved = target;
		}

		*original = reinterpret_cast<void*>(resolved);

		for (int i = 0; i < count; ++i)
		{
			if (!Hook::PatchCall(Engine::RebaseAddress(calls[i].address), calls[i].bytes, sizeof(calls[i].bytes),
					detour, nullptr))
			{
				return false;
			}
		}

		return true;
	}
}
