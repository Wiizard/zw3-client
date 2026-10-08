#include "STDInclude.hpp"

namespace Game
{
	Z_FreeInternal_t Z_FreeInternal = nullptr;
	Hunk_UserAlloc_t Hunk_UserAlloc = nullptr;

	static bool Z_TryVirtualCommitInternal(void* ptr, int size)
	{
		assert(size >= 0);

		return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
	}

	static void Z_VirtualDecommitInternal(void* ptr, int size)
	{
		assert(size >= 0);

#pragma warning(push)
#pragma warning(disable: 6250)
		VirtualFree(ptr, size, MEM_DECOMMIT);
#pragma warning(pop)
	}

	static void Z_VirtualCommitInternal(void* ptr, int size)
	{
		if (Z_TryVirtualCommitInternal(ptr, size))
		{
			return;
		}

		Sys_OutOfMemError();
	}

	static void Z_VirtualFreeInternal(void* ptr)
	{
		VirtualFree(ptr, 0, MEM_RELEASE);
	}

	void Z_VirtualCommit(void* ptr, int size)
	{
		assert(ptr);
		assert(size);

		Z_VirtualCommitInternal(ptr, size);
	}

	void* Z_VirtualReserve(int size)
	{
		assert(size >= 0);

		void* const buffer = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
		assert(buffer);

		return buffer;
	}

	void Z_VirtualDecommit(void* ptr, int size)
	{
		assert(ptr);
		assert(size);

		Z_VirtualDecommitInternal(ptr, size);
	}

	void Z_VirtualFree(void* ptr)
	{
		Z_VirtualFreeInternal(ptr);
	}

	void BindZone()
	{
		Z_FreeInternal = BindFunction<Z_FreeInternal_t>(0x14027F7F0);
		Hunk_UserAlloc = BindFunction<Hunk_UserAlloc_t>(0x14027ECC0);
	}
}
