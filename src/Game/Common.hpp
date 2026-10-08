#pragma once

namespace Game
{
	typedef void(*Com_Error_t)(int code, const char* format, ...);
	extern Com_Error_t Com_Error;

	typedef void(*Com_EndParseSession_t)();
	extern Com_EndParseSession_t Com_EndParseSession;

	typedef void(*Com_BeginParseSession_t)(const char* filename);
	extern Com_BeginParseSession_t Com_BeginParseSession;

	typedef char*(*Com_ParseOnLine_t)(const char** data);
	extern Com_ParseOnLine_t Com_ParseOnLine;

	typedef void(*Com_SkipRestOfLine_t)(const char** data);
	extern Com_SkipRestOfLine_t Com_SkipRestOfLine;

	typedef char*(*Com_Parse_t)(const char** data);
	extern Com_Parse_t Com_Parse;

	typedef void(*Com_SyncThreads_t)();
	extern Com_SyncThreads_t Com_SyncThreads;

	void Com_InitThreadData();

	void BindCommon();
}
