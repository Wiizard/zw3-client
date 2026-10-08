#include "STDInclude.hpp"

#include "Exception.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Flags.hpp"
#include "Network.hpp"
#include "Party.hpp"
#include "TextRenderer.hpp"

namespace Components
{
	int Exception::miniDumpType = MiniDumpNormal | MiniDumpWithHandleData | MiniDumpScanMemory
		| MiniDumpWithProcessThreadData | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo;

	LPTOP_LEVEL_EXCEPTION_FILTER Exception::previousFilter = nullptr;
	PVOID Exception::importThunk = nullptr;

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

	bool Exception::WriteMiniDump(LPEXCEPTION_POINTERS exceptionInfo, std::string& path)
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
			static_cast<MINIDUMP_TYPE>(miniDumpType), &information, nullptr, nullptr);

		CloseHandle(file);

		return written == TRUE;
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

		if (code == EXCEPTION_STACK_OVERFLOW)
		{
			const auto notice = std::format("Termination because of a stack overflow.\n{}", clipboardQuestion);

			if (MessageBoxA(nullptr, notice.data(), nullptr, MB_YESNO | MB_ICONERROR) == IDYES)
			{
				CopyToClipboard(description);
			}
		}

		std::string dumpPath;
		const bool wroteDump = WriteMiniDump(exceptionInfo, dumpPath);

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
	}
}
