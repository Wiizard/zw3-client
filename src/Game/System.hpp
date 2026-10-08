#pragma once

namespace Game
{
	typedef void(*Sys_FreeFileList_t)(char** list);
	extern Sys_FreeFileList_t Sys_FreeFileList;

	typedef bool(*Sys_IsDatabaseReady2_t)();
	extern Sys_IsDatabaseReady2_t Sys_IsDatabaseReady;
	extern Sys_IsDatabaseReady2_t Sys_IsDatabaseReady2;

	typedef bool(*Sys_IsMainThread_t)();
	extern Sys_IsMainThread_t Sys_IsMainThread;
	extern Sys_IsMainThread_t Sys_IsRenderThread;
	extern Sys_IsMainThread_t Sys_IsServerThread;
	extern Sys_IsMainThread_t Sys_IsDatabaseThread;

	typedef char**(*Sys_ListFiles_t)(const char* directory, const char* extension, const char* filter, int* numfiles, int wantsubs);
	extern Sys_ListFiles_t Sys_ListFiles;

	typedef int(*Sys_Milliseconds_t)();
	extern Sys_Milliseconds_t Sys_Milliseconds;

	typedef void(*Sys_Sleep_t)(int msec);
	extern Sys_Sleep_t Sys_Sleep;

	typedef void(*Sys_TempPriorityAtLeastNormalBegin_t)(TempPriority* priority);
	extern Sys_TempPriorityAtLeastNormalBegin_t Sys_TempPriorityAtLeastNormalBegin;

	typedef void(*Sys_TempPriorityEnd_t)(TempPriority* priority);
	extern Sys_TempPriorityEnd_t Sys_TempPriorityEnd;

	typedef void(*Sys_EnterCriticalSection_t)(CriticalSection critSect);
	extern Sys_EnterCriticalSection_t Sys_EnterCriticalSection;

	typedef void(*Sys_LeaveCriticalSection_t)(CriticalSection critSect);
	extern Sys_LeaveCriticalSection_t Sys_LeaveCriticalSection;

	typedef void(*Sys_SetValue_t)(int valueIndex, void* data);
	extern Sys_SetValue_t Sys_SetValue;

	typedef void(*Sys_OutOfMemErrorInternal_t)(const char* filename, int line);
	extern Sys_OutOfMemErrorInternal_t Sys_OutOfMemErrorInternal;

	typedef void(*Sys_QueEvent_t)(int evTime, int evType, int value, int value2, int ptrLength, void* ptr);
	extern Sys_QueEvent_t Sys_QueEvent;

	extern RTL_CRITICAL_SECTION* s_criticalSection;

	void Sys_LockRead(FastCriticalSection* critSect);
	void Sys_UnlockRead(FastCriticalSection* critSect);
	void Sys_LockWrite(FastCriticalSection* critSect);
	void Sys_UnlockWrite(FastCriticalSection* critSect);

	bool Sys_TryEnterCriticalSection(CriticalSection critSect);

	class Sys
	{
	public:
		enum class TLS_OFFSET : unsigned int
		{
			LEVEL_BGS = 0x8,
			THREAD_VALUES = 0x18,
			DVAR_MODIFIED_FLAGS = 0x20,
		};

		template <typename T>
		static T* GetTls(TLS_OFFSET offset)
		{
			const auto* tls = reinterpret_cast<std::uintptr_t*>(__readgsqword(0x58));
			return reinterpret_cast<T*>(tls[*g_dwTlsIndex] + static_cast<std::underlying_type_t<TLS_OFFSET>>(offset));
		}
	};

	void BindSystem();
}

#define Sys_OutOfMemError() Game::Sys_OutOfMemErrorInternal(__FILE__, __LINE__)
