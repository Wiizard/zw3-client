#pragma once

namespace Components::Remix::Hook
{
	void* AllocateNear(std::uintptr_t anchor, std::size_t size);

	std::uintptr_t MakeThunk(std::uintptr_t anchor, const void* target);

	std::uintptr_t ResolveCallTarget(std::uintptr_t site);

	bool PatchCall(std::uintptr_t site, const unsigned char* expected, std::size_t expectedSize, const void* replacement, void** outOriginal);

	bool PatchLiveCall(std::uintptr_t site, const void* replacement);

	bool PatchBytes(std::uintptr_t site, const unsigned char* expected, const unsigned char* replacement, std::size_t size);

	bool RestoreBytes(std::uintptr_t site);

	void RestoreAll();
}
