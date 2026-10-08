#include "STDInclude.hpp"

#include "Hunk.hpp"

namespace Game::Engine
{
	HunkUser* debugUser = nullptr;

	HunkUser* Hunk_UserCreate(int maxSize, const char* name, bool fixed, int type)
	{
		assert(!(maxSize % (64 * 1024)));

		auto* const user = static_cast<HunkUser*>(Z_VirtualReserve(maxSize));
		Z_VirtualCommit(user, static_cast<int>(offsetof(HunkUser, buf)));

		user->end = reinterpret_cast<std::intptr_t>(user) + maxSize;
		user->pos = reinterpret_cast<std::intptr_t>(user->buf);

		assert(!(user->pos & 31));

		user->maxSize = maxSize;
		user->name = name;
		user->current = user;
		user->fixed = fixed;
		user->type = type;

		assert(!user->next);

		return user;
	}

	void Hunk_UserDestroy(HunkUser* user)
	{
		auto* current = user->next;

		while (current)
		{
			auto* const next = current->next;
			Z_VirtualFree(current);
			current = next;
		}

		Z_VirtualFree(user);
	}

	void Hunk_InitDebugMemory()
	{
		assert(Sys_IsMainThread());
		assert(!debugUser);

		debugUser = Hunk_UserCreate(0x1000000, "Hunk_InitDebugMemory", false, 0);
	}

	void Hunk_ShutdownDebugMemory()
	{
		assert(Sys_IsMainThread());
		assert(debugUser);

		Hunk_UserDestroy(debugUser);
		debugUser = nullptr;
	}

	void* Hunk_AllocDebugMem(int size)
	{
		assert(Sys_IsMainThread());
		assert(debugUser);

		return Hunk_UserAlloc(debugUser, size, 4);
	}

	void Hunk_FreeDebugMem([[maybe_unused]] void* ptr)
	{
		assert(Sys_IsMainThread());
		assert(debugUser);
	}
}
