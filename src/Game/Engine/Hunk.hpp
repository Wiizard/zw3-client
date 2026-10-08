#pragma once

namespace Game::Engine
{
	constexpr auto FIXED_HUNK_USER_COUNT = 1;
	constexpr auto VIRTUAL_HUNK_USER_MAX = 128;

	extern HunkUser* debugUser;

	HunkUser* Hunk_UserCreate(int maxSize, const char* name, bool fixed, int type);
	void Hunk_UserDestroy(HunkUser* user);

	void Hunk_InitDebugMemory();
	void Hunk_ShutdownDebugMemory();
	void* Hunk_AllocDebugMem(int size);
	void Hunk_FreeDebugMem(void* ptr);
}
