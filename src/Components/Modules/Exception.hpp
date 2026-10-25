#pragma once

namespace Components
{
	class Exception : public Component
	{
	public:
		Exception();

		static void SetMiniDumpType(bool codeSegment, bool dataSegment);

	private:
		static int miniDumpType;
		static LPTOP_LEVEL_EXCEPTION_FILTER previousFilter;
		static PVOID importThunk;

		static LONG WINAPI ExceptionFilter(LPEXCEPTION_POINTERS exceptionInfo);

		static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter_Stub(
			LPTOP_LEVEL_EXCEPTION_FILTER filter);

		static bool LockExceptionFilter();

		static bool WriteMiniDump(LPEXCEPTION_POINTERS exceptionInfo, std::string& path, int extraFlags = 0);
		static void HunkHighError_Hook(const char* format, std::size_t requestedBytes, std::size_t totalMB,
			std::size_t lowMB, std::size_t highMB);
		static void CopyToClipboard(const std::string& text);
		static std::string DescribeException(LPEXCEPTION_POINTERS exceptionInfo);
		static std::string GetErrorMessage(const std::string& error);
	};
}
