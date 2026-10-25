#include "STDInclude.hpp"

#include "Exception.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Flags.hpp"
#include "Network.hpp"
#include "Party.hpp"
#include "RawFiles.hpp"
#include "TextRenderer.hpp"

namespace Components
{
	int Exception::miniDumpType = MiniDumpNormal | MiniDumpWithHandleData | MiniDumpScanMemory
		| MiniDumpWithProcessThreadData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo;

	LPTOP_LEVEL_EXCEPTION_FILTER Exception::previousFilter = nullptr;
	PVOID Exception::importThunk = nullptr;
	static Utils::Hook hunkHighErrorHook;
	static constexpr std::uintptr_t hunkHighErrorCall = 0x14027F470;
	static constexpr std::uintptr_t nativeSysError = 0x1402A4F90;

	void Exception::SetMiniDumpType(bool codeSegment, bool dataSegment)
	{
		miniDumpType = MiniDumpIgnoreInaccessibleMemory | MiniDumpWithHandleData | MiniDumpScanMemory
			| MiniDumpWithProcessThreadData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo;

		if (codeSegment)
		{
			miniDumpType |= MiniDumpWithCodeSegs;
		}

		if (dataSegment)
		{
			miniDumpType |= MiniDumpWithDataSegs;
		}
	}

	std::string Exception::DescribeException(LPEXCEPTION_POINTERS exceptionInfo)
	{
		const auto* const record = exceptionInfo->ExceptionRecord;
		const auto address = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);

		std::string description = Utils::String::VA("code 0x%08X at 0x%llX",
			record->ExceptionCode, static_cast<unsigned long long>(address));

		if (Utils::Hook::IsBound())
		{
			MEMORY_BASIC_INFORMATION information{};

			if (VirtualQuery(record->ExceptionAddress, &information, sizeof(information)))
			{
				char moduleName[MAX_PATH]{};
				const auto module = reinterpret_cast<HMODULE>(information.AllocationBase);

				if (GetModuleFileNameA(module, moduleName, MAX_PATH))
				{
					PathStripPathA(moduleName);

					description.append(Utils::String::VA(" in %s+0x%llX", moduleName,
						static_cast<unsigned long long>(address - reinterpret_cast<std::uintptr_t>(module))));

					if (module == GetModuleHandleA(nullptr))
					{
						description.append(Utils::String::VA(", IDB 0x%llX",
							static_cast<unsigned long long>(Utils::Hook::Unrebase(address))));
					}
				}
			}
		}

		return description;
	}

	constexpr const char* clipboardQuestion = "Do you want to copy this message to the clipboard?";

	static std::string StripName(const char* name)
	{
		return TextRenderer::StripAllTextIcons(TextRenderer::StripColors(std::string(name)));
	}

	static std::string DvarString(const char* name)
	{
		const Dvar::Var dvar(name);

		if (!dvar.IsValid())
		{
			return "";
		}

		return dvar.Get<std::string>();
	}

	std::string Exception::GetErrorMessage(const std::string& error)
	{
		std::string osVersion = "Wine";

		if (!Utils::IsWineEnvironment())
		{
			osVersion = Utils::GetWindowsVersion();
		}

		const auto launchParameters = Utils::String::Convert(Utils::GetLaunchParameters());

		const std::string clientVersion = "4.0.0";
		const std::string clientInfo = std::format("Client Info:\nZW3 Version: {}\nOS Version: {}\nParameters: {}",
			clientVersion, osVersion, launchParameters);

		if (!Game::CL_IsCgameInitialized(0))
		{
			return std::format("{}\n\n{}", clientInfo, error);
		}

		const auto gameType = DvarString("g_gametype");
		const auto mapName = DvarString("mapname");

		std::string modName = "None";

		if (*Game::fs_gameDirVar && (*Game::fs_gameDirVar)->current.string[0] != '\0')
		{
			modName = StripName((*Game::fs_gameDirVar)->current.string);
		}

		if (Dedicated::IsRunning())
		{
			const std::string hostInfo = std::format("Host Info:\nType: Private Match\nGametype: {}\nMap Name: {}\nMod Name: {}",
				gameType, mapName, modName);

			return std::format("{}\n\n{}\n\n{}", clientInfo, hostInfo, error);
		}

		const std::string serverVersion = "2.0.0";
		const auto* const serverAddress = Game::clc_serverAddress;
		const auto serverName = StripName(Party::GetHostName().data());

		const std::string serverInfo = std::format(
			"Server Info:\nType: Dedicated Server\nZW3 Version: {}\nServer Name: {}\nIP Address: {}\nGametype: {}\nMap Name: {}\nMod Name: {}",
			serverVersion, serverName, Network::Address(serverAddress).GetString(), gameType, mapName, modName);

		return std::format("{}\n\n{}\n\n{}", clientInfo, serverInfo, error);
	}

	bool Exception::WriteMiniDump(LPEXCEPTION_POINTERS exceptionInfo, std::string& path, int extraFlags)
	{
		char executable[MAX_PATH]{};
		GetModuleFileNameA(nullptr, executable, MAX_PATH);
		PathStripPathA(executable);
		PathRemoveExtensionA(executable);

		char stamp[MAX_PATH]{};
		__time64_t now;
		tm local{};
		_time64(&now);
		_localtime64_s(&local, &now);
		strftime(stamp, sizeof(stamp) - 1, "%Y%m%d%H%M%S", &local);

		CreateDirectoryA("minidumps", nullptr);

		path = Utils::String::VA("minidumps\\%s-%s.dmp", executable, stamp);

		const HANDLE file = CreateFileA(path.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		MINIDUMP_EXCEPTION_INFORMATION information{};
		information.ThreadId = GetCurrentThreadId();
		information.ExceptionPointers = exceptionInfo;
		information.ClientPointers = FALSE;

		const BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
			static_cast<MINIDUMP_TYPE>(miniDumpType | extraFlags), &information, nullptr, nullptr);

		CloseHandle(file);

		return written == TRUE;
	}

	void Exception::HunkHighError_Hook(const char* format, std::size_t requestedBytes, std::size_t totalMB,
		std::size_t lowMB, std::size_t highMB)
	{
		// Sys_Error displays a dialog and exits without an SEH exception. Capture the actual allocation
		// caller before that happens; increasing the hunk cannot repair a corrupt script-file length.
		static std::atomic_flag isWriting = ATOMIC_FLAG_INIT;
		if (!isWriting.test_and_set())
		{
			try
			{
				CONTEXT context{};
				RtlCaptureContext(&context);
				EXCEPTION_RECORD record{};
				record.ExceptionCode = 0xE05A5733; // Application-defined diagnostic, not a hardware exception.
				record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
				record.NumberParameters = 5;
				record.ExceptionInformation[0] = requestedBytes;
				record.ExceptionInformation[1] = totalMB;
				record.ExceptionInformation[2] = lowMB;
				record.ExceptionInformation[3] = highMB;
				record.ExceptionInformation[4] = reinterpret_cast<ULONG_PTR>(RawFiles::LastScriptRead());
				EXCEPTION_POINTERS pointers{ &record, &context };
				std::string dumpPath;
				const bool written = WriteMiniDump(&pointers, dumpPath, MiniDumpWithIndirectlyReferencedMemory);

				if (!dumpPath.empty())
				{
					char description[512]{};
					const int length = sprintf_s(description,
						"Hunk_AllocateTempMemoryHigh: requested %llu bytes; total %llu MB, low %llu MB, high %llu MB\r\n"
						"Last script read: %s\r\nDump written: %u\r\n",
						static_cast<unsigned long long>(requestedBytes), static_cast<unsigned long long>(totalMB),
						static_cast<unsigned long long>(lowMB), static_cast<unsigned long long>(highMB),
						RawFiles::LastScriptRead(), written ? 1u : 0u);
					const std::string reportPath = dumpPath + ".txt";
					const HANDLE report = CreateFileA(reportPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
						CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
					if (report != INVALID_HANDLE_VALUE)
					{
						DWORD bytesWritten = 0;
						if (length > 0) WriteFile(report, description, static_cast<DWORD>(length), &bytesWritten, nullptr);
						CloseHandle(report);
					}
				}
			}
			catch (...)
			{
				// Diagnostics must not replace the original fatal-error path, including on allocation failure.
			}
		}

		reinterpret_cast<void(*)(const char*, ...)>(Utils::Hook::Rebase(nativeSysError))(
			format, requestedBytes, totalMB, lowMB, highMB);
	}

	void Exception::CopyToClipboard(const std::string& text)
	{
		if (!OpenClipboard(GetDesktopWindow()))
		{
			return;
		}

		EmptyClipboard();

		const HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);

		if (block)
		{
			void* const locked = GlobalLock(block);

			if (locked)
			{
				std::memcpy(locked, text.data(), text.size() + 1);
				GlobalUnlock(block);

				if (!SetClipboardData(CF_TEXT, block))
				{
					GlobalFree(block);
				}
			}
			else
			{
				GlobalFree(block);
			}
		}

		CloseClipboard();
	}

	LONG WINAPI Exception::ExceptionFilter(LPEXCEPTION_POINTERS exceptionInfo)
	{
		const auto code = exceptionInfo->ExceptionRecord->ExceptionCode;

		if (code == STATUS_INTEGER_OVERFLOW || code == STATUS_FLOAT_OVERFLOW)
		{
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		const auto description = DescribeException(exceptionInfo);
		const bool isHeadlessServer = Flags::HasFlag("dedicated");

		if (code == EXCEPTION_STACK_OVERFLOW && !isHeadlessServer)
		{
			const auto notice = std::format("Termination because of a stack overflow.\n{}", clipboardQuestion);

			if (MessageBoxA(nullptr, notice.data(), nullptr, MB_YESNO | MB_ICONERROR) == IDYES)
			{
				CopyToClipboard(description);
			}
		}

		std::string dumpPath;
		const bool wroteDump = WriteMiniDump(exceptionInfo, dumpPath);

		if (isHeadlessServer)
		{
			// A modal dialog would strand the game thread while the network thread
			// keeps answering discovery queries. Preserve the dump, then let the
			// unhandled exception terminate the process so its agent can retire it.
			// Do not log launch parameters, DVARs or arbitrary exception text.
			// Avoid shared stdio/logger locks on the faulting thread.
			char notice[] = "ZWNET dedicated fatal exception: code=0x00000000 dump_written=0\r\n";
			constexpr auto codeOffset = sizeof("ZWNET dedicated fatal exception: code=0x") - 1;
			constexpr auto dumpOffset = sizeof("ZWNET dedicated fatal exception: code=0x00000000 dump_written=") - 1;
			constexpr char hex[] = "0123456789ABCDEF";
			for (unsigned digit = 0; digit < 8; ++digit)
				notice[codeOffset + digit] = hex[(code >> ((7 - digit) * 4)) & 0xF];
			notice[dumpOffset] = wroteDump ? '1' : '0';
			DWORD written = 0;
			WriteFile(GetStdHandle(STD_ERROR_HANDLE), notice, sizeof(notice) - 1, &written, nullptr);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		std::string error = description;
		error.append("\n");

		if (wroteDump)
		{
			error.append("A minidump was written to ");
			error.append(dumpPath);
		}
		else
		{
			error.append("A minidump could not be written.");
		}

		const auto report = GetErrorMessage(error);
		const auto message = std::format("zw3 crashed.\n\n{}\n\n{}", report, clipboardQuestion);

		if (MessageBoxA(nullptr, message.data(), "Zombie Warfare 3", MB_YESNO | MB_ICONERROR) == IDYES)
		{
			CopyToClipboard(report);
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}

	LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Exception::SetUnhandledExceptionFilter_Stub(
		LPTOP_LEVEL_EXCEPTION_FILTER filter)
	{
		return filter;
	}

	bool Exception::LockExceptionFilter()
	{
		const auto base = reinterpret_cast<std::uint8_t*>(GetModuleHandleA(nullptr));

		if (!base)
		{
			return false;
		}

		const auto* const dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);

		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		{
			return false;
		}

		const auto* const ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

		if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
		{
			return false;
		}

		const auto& directory = ntHeaders->OptionalHeader
			.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

		if (!directory.VirtualAddress)
		{
			return false;
		}

		const auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);

		for (; descriptor->Name; ++descriptor)
		{
			if (!descriptor->OriginalFirstThunk)
			{
				continue;
			}

			const auto* nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
			auto* addressThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);

			for (; nameThunk->u1.AddressOfData; ++nameThunk, ++addressThunk)
			{
				if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal))
				{
					continue;
				}

				const auto* const import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
					base + nameThunk->u1.AddressOfData);

				if (std::strcmp(import->Name, "SetUnhandledExceptionFilter") != 0)
				{
					continue;
				}

				DWORD oldProtect;

				if (!VirtualProtect(addressThunk, sizeof(*addressThunk), PAGE_READWRITE, &oldProtect))
				{
					return false;
				}

				importThunk = addressThunk;
				addressThunk->u1.Function = reinterpret_cast<ULONGLONG>(SetUnhandledExceptionFilter_Stub);

				VirtualProtect(addressThunk, sizeof(*addressThunk), oldProtect, &oldProtect);

				return true;
			}
		}

		return false;
	}

	Exception::Exception()
	{
		const bool isBigDump = Flags::HasFlag("bigminidumps");
		const bool isReallyBigDump = Flags::HasFlag("reallybigminidumps");

		SetMiniDumpType(isBigDump, isReallyBigDump && !isBigDump);

		previousFilter = SetUnhandledExceptionFilter(ExceptionFilter);

		LockExceptionFilter();

		// Hook only this fatal call site so Console retains its Sys_Error handler for dedicated/-stdout launches.
		if (Utils::Hook::BranchesTo(hunkHighErrorCall, nativeSysError, HOOK_CALL)
			&& hunkHighErrorHook.Initialize(hunkHighErrorCall, reinterpret_cast<void*>(HunkHighError_Hook), HOOK_CALL)
				->Install()->IsInstalled())
		{
			hunkHighErrorHook.Quick();
		}
		else
		{
			OutputDebugStringA("zw3: high-hunk allocation diagnostics could not be installed\n");
		}
	}
}
