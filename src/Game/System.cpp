#include "STDInclude.hpp"

namespace Game
{
	Sys_FreeFileList_t Sys_FreeFileList = nullptr;
	Sys_IsDatabaseReady2_t Sys_IsDatabaseReady = nullptr;
	Sys_IsDatabaseReady2_t Sys_IsDatabaseReady2 = nullptr;
	Sys_IsMainThread_t Sys_IsMainThread = nullptr;
	Sys_IsMainThread_t Sys_IsRenderThread = nullptr;
	Sys_IsMainThread_t Sys_IsServerThread = nullptr;
	Sys_IsMainThread_t Sys_IsDatabaseThread = nullptr;
	Sys_ListFiles_t Sys_ListFiles = nullptr;
	Sys_Milliseconds_t Sys_Milliseconds = nullptr;
	Sys_Sleep_t Sys_Sleep = nullptr;
	Sys_TempPriorityAtLeastNormalBegin_t Sys_TempPriorityAtLeastNormalBegin = nullptr;
	Sys_TempPriorityEnd_t Sys_TempPriorityEnd = nullptr;
	Sys_EnterCriticalSection_t Sys_EnterCriticalSection = nullptr;
	Sys_LeaveCriticalSection_t Sys_LeaveCriticalSection = nullptr;
	Sys_SetValue_t Sys_SetValue = nullptr;
	Sys_OutOfMemErrorInternal_t Sys_OutOfMemErrorInternal = nullptr;
	Sys_QueEvent_t Sys_QueEvent = nullptr;

	RTL_CRITICAL_SECTION* s_criticalSection = nullptr;

	void Sys_LockRead(FastCriticalSection* critSect)
	{
		InterlockedIncrement(&critSect->readCount);

		while (critSect->writeCount)
		{
			Sys_Sleep(1);
		}
	}

	void Sys_UnlockRead(FastCriticalSection* critSect)
	{
		assert(critSect->readCount > 0);

		InterlockedDecrement(&critSect->readCount);
	}

	void Sys_LockWrite(FastCriticalSection* critSect)
	{
		TempPriority priority{};
		Sys_TempPriorityAtLeastNormalBegin(&priority);

		while (true)
		{
			if (!critSect->readCount)
			{
				if (InterlockedIncrement(&critSect->writeCount) == 1 && !critSect->readCount)
				{
					break;
				}

				InterlockedDecrement(&critSect->writeCount);
			}

			Sys_Sleep(1);
		}

		critSect->tempPriority = priority;
	}

	void Sys_UnlockWrite(FastCriticalSection* critSect)
	{
		assert(critSect->writeCount > 0);

		InterlockedDecrement(&critSect->writeCount);
		Sys_TempPriorityEnd(&critSect->tempPriority);
	}

	bool Sys_TryEnterCriticalSection(CriticalSection critSect)
	{
		AssertIn(critSect, CRITSECT_COUNT);

		return TryEnterCriticalSection(&s_criticalSection[critSect]) != FALSE;
	}

	void BindSystem()
	{
		Sys_FreeFileList = BindFunction<Sys_FreeFileList_t>(0x140279010);
		Sys_IsDatabaseReady = BindFunction<Sys_IsDatabaseReady2_t>(0x14020A410);
		Sys_IsDatabaseReady2 = BindFunction<Sys_IsDatabaseReady2_t>(0x14020A8B0);
		Sys_IsMainThread = BindFunction<Sys_IsMainThread_t>(0x14020A8E0);
		Sys_IsRenderThread = BindFunction<Sys_IsMainThread_t>(0x14020A930);
		Sys_IsServerThread = BindFunction<Sys_IsMainThread_t>(0x14020A970);
		Sys_IsDatabaseThread = BindFunction<Sys_IsMainThread_t>(0x14020B780);
		Sys_ListFiles = BindFunction<Sys_ListFiles_t>(0x14028E410);
		Sys_Milliseconds = BindFunction<Sys_Milliseconds_t>(0x1402A8620);
		Sys_Sleep = BindFunction<Sys_Sleep_t>(0x14020AB90);
		Sys_TempPriorityAtLeastNormalBegin = BindFunction<Sys_TempPriorityAtLeastNormalBegin_t>(0x14020B060);
		Sys_TempPriorityEnd = BindFunction<Sys_TempPriorityEnd_t>(0x14020B0A0);
		Sys_EnterCriticalSection = BindFunction<Sys_EnterCriticalSection_t>(0x14028E390);
		Sys_LeaveCriticalSection = BindFunction<Sys_LeaveCriticalSection_t>(0x14028E3F0);
		Sys_SetValue = BindFunction<Sys_SetValue_t>(0x14020AB50);
		Sys_OutOfMemErrorInternal = BindFunction<Sys_OutOfMemErrorInternal_t>(0x1402A5770);
		Sys_QueEvent = BindFunction<Sys_QueEvent_t>(0x1402A57D0);

		s_criticalSection = reinterpret_cast<RTL_CRITICAL_SECTION*>(Utils::Hook::Rebase(0x14673EDA0));
	}
}
