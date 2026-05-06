#include "MemoryGuard.hpp"
#include "Logger.hpp"

namespace Components
{
	Utils::Hook MemoryGuard::OutOfMemHook;

	// ---------------------------------------------------------------------
	// Diagnostics
	// ---------------------------------------------------------------------

	std::string MemoryGuard::FormatMemoryInfo()
	{
		std::string out;

		PROCESS_MEMORY_COUNTERS_EX pmc{};
		pmc.cb = sizeof(pmc);
		if (GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
		{
			out += std::format(
				"  WorkingSet     : {} MiB\n"
				"  PrivateUsage   : {} MiB (virtual commit)\n"
				"  PagefileUsage  : {} MiB\n"
				"  PeakWorkingSet : {} MiB\n",
				pmc.WorkingSetSize      / (1024u * 1024u),
				pmc.PrivateUsage        / (1024u * 1024u),
				pmc.PagefileUsage       / (1024u * 1024u),
				pmc.PeakWorkingSetSize  / (1024u * 1024u));
		}

		MEMORYSTATUSEX ms{};
		ms.dwLength = sizeof(ms);
		if (GlobalMemoryStatusEx(&ms))
		{
			out += std::format(
				"  SystemMemoryLoad   : {}%\n"
				"  SystemPhysAvail    : {} MiB\n"
				"  ProcessVirtTotal   : {} MiB\n"
				"  ProcessVirtAvail   : {} MiB  (<--- this is what runs out first in 32-bit)\n",
				ms.dwMemoryLoad,
				static_cast<unsigned long long>(ms.ullAvailPhys)      / (1024ull * 1024ull),
				static_cast<unsigned long long>(ms.ullTotalVirtual)   / (1024ull * 1024ull),
				static_cast<unsigned long long>(ms.ullAvailVirtual)   / (1024ull * 1024ull));
		}

		// Walk virtual address space and report the largest free contiguous
		// block. Fragmentation frequently causes OOM even when total free
		// bytes look healthy, so this number is more useful than ullAvailVirtual.
		{
			SYSTEM_INFO si{};
			GetSystemInfo(&si);

			std::size_t largestFree = 0;
			std::size_t totalFree   = 0;
			auto* addr = static_cast<BYTE*>(si.lpMinimumApplicationAddress);
			auto* endAddr = static_cast<BYTE*>(si.lpMaximumApplicationAddress);

			while (addr < endAddr)
			{
				MEMORY_BASIC_INFORMATION mbi{};
				if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0)
				{
					break;
				}

				if (mbi.State == MEM_FREE)
				{
					totalFree += mbi.RegionSize;
					if (mbi.RegionSize > largestFree)
					{
						largestFree = mbi.RegionSize;
					}
				}

				addr += mbi.RegionSize;
			}

			out += std::format(
				"  LargestFreeBlock   : {} MiB  (contiguous; if this is small, it's fragmentation)\n"
				"  TotalFreeVA        : {} MiB\n",
				largestFree / (1024u * 1024u),
				totalFree   / (1024u * 1024u));
		}

		return out;
	}

	std::string MemoryGuard::FormatLoadedZones()
	{
		std::string out;

		// Mirror the layout used by Game::DB_IsZoneLoaded
		const auto zoneCount   = Utils::Hook::Get<int>(0x1261BCC);
		const auto* zoneIndices = reinterpret_cast<const unsigned char*>(0x16B8A34);
		const auto* zoneData    = reinterpret_cast<const char*>(0x14C0F80);

		out += std::format("  Loaded zones ({}):\n", zoneCount);

		for (int i = 0; i < zoneCount && i < 32; ++i)
		{
			const char* name = zoneData + 4 + 0xA4 * zoneIndices[i];
			out += std::format("    [{:>2}] {}\n", i, name);
		}

		return out;
	}

	std::string MemoryGuard::CollectDiagnostics(const char* filename, int line)
	{
		std::string diag;

		SYSTEMTIME st{};
		GetLocalTime(&st);

		diag += std::format(
			"=== Sys_OutOfMemError ({}-{:02}-{:02} {:02}:{:02}:{:02}) ===\n"
			"  Source         : {}:{}\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
			(filename != nullptr ? filename : "<null>"), line);

		diag += "Memory:\n";
		diag += FormatMemoryInfo();

		diag += "Zones:\n";
		diag += FormatLoadedZones();

		diag += "=== end ===\n\n";
		return diag;
	}

	void MemoryGuard::WriteDiagnosticsLog(const std::string& diag)
	{
		// Write to both the console (if attached) and a dedicated log file.
		// We intentionally do this via raw Win32 rather than fstream because
		// the CRT may itself be in a bad state when an OOM fires.
		OutputDebugStringA(diag.c_str());

		CreateDirectoryA("zw3", nullptr);
		CreateDirectoryA("zw3\\logs", nullptr);

		HANDLE h = CreateFileA("zw3\\logs\\oom_diagnostics.log",
			FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (h != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			SetFilePointer(h, 0, nullptr, FILE_END);
			WriteFile(h, diag.data(), static_cast<DWORD>(diag.size()), &written, nullptr);
			CloseHandle(h);
		}
	}

	// ---------------------------------------------------------------------
	// OOM hook
	// ---------------------------------------------------------------------

	void MemoryGuard::OutOfMemHandler_Stub(const char* filename, int line)
	{
		// Gather diagnostics first (best-effort — any failure here must not
		// prevent us from calling the original handler). We catch std
		// exceptions (bad_alloc in particular, since we are literally out
		// of memory) and swallow them so we always reach the original
		// handler below.
		try
		{
			const auto diag = CollectDiagnostics(filename, line);
			WriteDiagnosticsLog(diag);
		}
		catch (...)
		{
			// We're already dying — don't make it worse. Just log a stub
			// line via OutputDebugString that doesn't allocate.
			OutputDebugStringA("MemoryGuard: diagnostics failed (likely bad_alloc)\n");
		}

		// Hand control to the engine's original handler so the existing
		// error dialog / crashdump path still runs unchanged.
		OutOfMemHook.uninstall();
		Game::Sys_OutOfMemErrorInternal(filename, line);
		// Unreachable: original is __declspec(noreturn) in practice.
		OutOfMemHook.install();
	}

	// ---------------------------------------------------------------------
	// Map transition housekeeping
	// ---------------------------------------------------------------------

	void MemoryGuard::OnMapUnloaded()
	{
		// Currently a no-op. Previously this called HeapCompact() and
		// SetProcessWorkingSetSizeEx(). Both were removed after they were
		// shown to cause audio subsystem artifacts (menu music briefly
		// re-triggering during loadscreens) under heavy map transitions,
		// and the memory benefit turned out to be negligible compared to
		// the real win from picmip floors.
		//
		// Kept as a hook point: if future diagnostics or safe housekeeping
		// need to run between map unload and next map load, this is where
		// it goes.
	}

	// ---------------------------------------------------------------------
	// Component wiring
	// ---------------------------------------------------------------------

	MemoryGuard::MemoryGuard()
	{
		// Hook Sys_OutOfMemErrorInternal at its well-known address. We
		// keep a proper Utils::Hook object so we can temporarily uninstall
		// it when calling through to the original (avoids infinite recursion).
		OutOfMemHook.initialize(Game::Sys_OutOfMemErrorInternal,
			OutOfMemHandler_Stub, HOOK_JUMP);
		OutOfMemHook.install();
	}
}
