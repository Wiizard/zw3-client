#pragma once

namespace Game
{
	typedef void(*Z_FreeInternal_t)(void* data);
	extern Z_FreeInternal_t Z_FreeInternal;

	typedef void*(*Hunk_UserAlloc_t)(HunkUser* user, int size, int alignment);
	extern Hunk_UserAlloc_t Hunk_UserAlloc;

	constexpr auto PAGE_SIZE = 4096;

	[[nodiscard]] void* Z_VirtualReserve(int size);
	void Z_VirtualCommit(void* ptr, int size);
	void Z_VirtualDecommit(void* ptr, int size);
	void Z_VirtualFree(void* ptr);

	void BindZone();
}
