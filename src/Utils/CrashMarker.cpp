#include "STDInclude.hpp"

#include "CrashMarker.hpp"

namespace Utils
{
	bool DeleteCrashMarker()
	{
		char modulePath[MAX_PATH]{};

		const auto length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);

		if (!length || length >= MAX_PATH)
		{
			return false;
		}

		const std::filesystem::path executable(modulePath);
		const auto markerName = "__" + executable.stem().string();

		if (markerName.size() <= 2)
		{
			return false;
		}

		const auto removedBesideExe = IO::RemoveFile((executable.parent_path() / markerName).string());
		const auto removedInWorkingDir = IO::RemoveFile(markerName);

		return removedBesideExe || removedInWorkingDir;
	}
}
