#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Components::AssetBundleZones
{
	inline char LowerAscii(char value) noexcept
	{
		return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
	}

	inline bool IsZoneName(std::string_view name) noexcept
	{
		constexpr std::string_view prefix = "zw3_assets";
		return name.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), name.begin(),
			[](char expected, char actual) { return expected == LowerAscii(actual); });
	}

	inline std::vector<std::string> Discover(const std::filesystem::path& directory, std::error_code& error)
	{
		std::vector<std::string> zones;
		std::filesystem::directory_iterator entry(directory, error);
		if (error == std::errc::no_such_file_or_directory)
		{
			error.clear();
			return zones;
		}

		for (; !error && entry != std::filesystem::directory_iterator{}; entry.increment(error))
		{
			const bool regular = entry->is_regular_file(error);
			if (error) break;
			if (!regular) continue;
			const auto& path = entry->path();
			std::string extension = path.extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), LowerAscii);
			if (extension != ".ff") continue;

			std::string name = path.stem().string();
			if (!IsZoneName(name)) continue;
			// Windows filenames and engine zone lookups are case-insensitive. Keep a stable load order.
			std::transform(name.begin(), name.end(), name.begin(), LowerAscii);
			zones.push_back(std::move(name));
		}

		std::sort(zones.begin(), zones.end());
		zones.erase(std::unique(zones.begin(), zones.end()), zones.end());
		return zones;
	}
}
