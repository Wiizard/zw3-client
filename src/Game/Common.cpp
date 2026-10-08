#include "STDInclude.hpp"

namespace Game
{
	Com_Error_t Com_Error = nullptr;
	Com_EndParseSession_t Com_EndParseSession = nullptr;
	Com_BeginParseSession_t Com_BeginParseSession = nullptr;
	Com_ParseOnLine_t Com_ParseOnLine = nullptr;
	Com_SkipRestOfLine_t Com_SkipRestOfLine = nullptr;
	Com_Parse_t Com_Parse = nullptr;
	Com_SyncThreads_t Com_SyncThreads = nullptr;

	void Com_InitThreadData()
	{
		constexpr int threadValueCount = 4;
		constexpr int threadValueVa = 1;
		constexpr int threadValueComError = 2;
		constexpr int threadValueTrace = 3;

		thread_local char vaInfo[0x804]{};
		thread_local jmp_buf comError{};
		thread_local std::uint32_t traceInfo[4]{};
		thread_local void* threadValues[threadValueCount]{};

		*Sys::GetTls<void*>(Sys::TLS_OFFSET::THREAD_VALUES) = threadValues;

		Sys_SetValue(threadValueVa, vaInfo);
		Sys_SetValue(threadValueComError, &comError);
		Sys_SetValue(threadValueTrace, traceInfo);
	}

	void BindCommon()
	{
		Com_Error = BindFunction<Com_Error_t>(0x1401F38F0);
		Com_EndParseSession = BindFunction<Com_EndParseSession_t>(0x14028AD40);
		Com_BeginParseSession = BindFunction<Com_BeginParseSession_t>(0x14028AC50);
		Com_ParseOnLine = BindFunction<Com_ParseOnLine_t>(0x14028B6B0);
		Com_SkipRestOfLine = BindFunction<Com_SkipRestOfLine_t>(0x14028BAB0);
		Com_Parse = BindFunction<Com_Parse_t>(0x14028AF80);
		Com_SyncThreads = BindFunction<Com_SyncThreads_t>(0x1401F6B30);
	}
}
