#include "STDInclude.hpp"

namespace Utils
{
	bool IsWineEnvironment()
	{
		const auto ntdll = GetModuleHandleA("ntdll.dll");

		if (!ntdll)
		{
			return false;
		}

		return GetProcAddress(ntdll, "wine_get_version") != nullptr;
	}

	std::string GetWindowsVersion()
	{
		const Library ntdll("ntdll.dll");
		const auto rtlGetVersion = ntdll.GetProc<LONG(WINAPI*)(PRTL_OSVERSIONINFOW)>("RtlGetVersion");

		if (!rtlGetVersion)
		{
			return "Unknown Version";
		}

		RTL_OSVERSIONINFOW versionInfo{};
		versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
		rtlGetVersion(&versionInfo);

		const auto major = versionInfo.dwMajorVersion;
		const auto minor = versionInfo.dwMinorVersion;
		const auto build = versionInfo.dwBuildNumber;
		const auto architecture = GetWindowsArchitecture();

		if (major == 10 && build >= 22000)
		{
			return std::format("Windows 11 (Build {}) {}", build, architecture);
		}

		if (major == 10)
		{
			return std::format("Windows 10 (Build {}) {}", build, architecture);
		}

		if (major == 6 && minor == 3)
		{
			return std::format("Windows 8.1 (Build {}) {}", build, architecture);
		}

		if (major == 6 && minor == 2)
		{
			return std::format("Windows 8.0 (Build {}) {}", build, architecture);
		}

		if (major == 6 && minor == 1)
		{
			return std::format("Windows 7 (Build {}) {}", build, architecture);
		}

		if (major == 6 && minor == 0)
		{
			return std::format("Windows Vista (Build {}) {}", build, architecture);
		}

		if (major == 5 && minor == 2)
		{
			return std::format("Windows XP Professional (Build {}) {}", build, architecture);
		}

		if (major == 5 && minor == 1)
		{
			return std::format("Windows XP (Build {}) {}", build, architecture);
		}

		return "Unknown Version";
	}

	std::string GetWindowsArchitecture()
	{
		SYSTEM_INFO systemInfo;
		GetNativeSystemInfo(&systemInfo);

		switch (systemInfo.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64:
			return "64 Bit";
		case PROCESSOR_ARCHITECTURE_INTEL:
			return "32 Bit";
		case PROCESSOR_ARCHITECTURE_ARM:
			return "ARM";
		default:
			return "Unknown Architecture";
		}
	}

	std::wstring GetLaunchParameters()
	{
		std::wstring parameters;

		int argumentCount;
		auto* const arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

		if (arguments)
		{
			for (int i = 1; i < argumentCount; ++i)
			{
				parameters.append(arguments[i]);
				parameters.append(L" ");
			}

			LocalFree(arguments);
		}

		return parameters;
	}

	std::filesystem::path GetBaseFilesLocation()
	{
		char* buffer = nullptr;
		std::size_t size = 0;

		if (_dupenv_s(&buffer, &size, "BASE_INSTALL") != 0 || !buffer)
		{
			return {};
		}

		const std::unique_ptr<char, decltype(&std::free)> owned(buffer, &std::free);

		try
		{
			return std::filesystem::path(buffer);
		}
		catch (const std::exception& error)
		{
			printf("Failed to convert '%s' to native file system path. Got error '%s'\n", buffer, error.what());
			return {};
		}
	}

	void SafeShellExecute(HWND hwnd, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT showCommand)
	{
		[=]
		{
			__try
			{
				ShellExecuteA(hwnd, operation, file, parameters, directory, showCommand);
			}
			__finally
			{
			}
		}();

		std::this_thread::yield();
	}

	void OpenUrl(const std::string& url)
	{
		SafeShellExecute(nullptr, "open", url.data(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	bool HasIntersection(std::uintptr_t base1, std::size_t len1, std::uintptr_t base2, std::size_t len2)
	{
		return !(base1 + len1 <= base2 || base2 + len2 <= base1);
	}
}
