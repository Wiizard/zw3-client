#pragma once

namespace Utils
{
	bool IsWineEnvironment();

	std::string GetWindowsVersion();

	std::string GetWindowsArchitecture();

	std::wstring GetLaunchParameters();

	std::filesystem::path GetBaseFilesLocation();

	void SafeShellExecute(HWND hwnd, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT showCommand);
	void OpenUrl(const std::string& url);

	bool HasIntersection(std::uintptr_t base1, std::size_t len1, std::uintptr_t base2, std::size_t len2);
}
