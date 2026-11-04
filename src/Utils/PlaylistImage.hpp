#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Utils
{
	inline std::optional<std::string> PlaylistImageDigest(const std::string_view path)
	{
		constexpr std::string_view prefix = "/zwnet/playlist-images/";
		if (!path.starts_with(prefix) || path.size() != prefix.size() + 64 + 4 || !path.ends_with(".png")) return std::nullopt;
		const auto digest = path.substr(prefix.size(), 64);
		if (!std::ranges::all_of(digest, [](const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) return std::nullopt;
		return std::string(digest);
	}

	inline bool IsBoundedPlaylistPng(const std::string_view bytes)
	{
		if (bytes.size() < 57 || bytes.size() > 2 * 1024 * 1024 || bytes.substr(0, 8) != std::string_view("\x89PNG\r\n\x1a\n", 8)
			|| bytes.substr(12, 4) != "IHDR") return false;
		const auto read = [&bytes](const std::size_t offset)
		{
			std::uint32_t value = 0;
			for (std::size_t i = 0; i < 4; ++i) value = (value << 8) | static_cast<unsigned char>(bytes[offset + i]);
			return value;
		};
		const auto width = read(16), height = read(20);
		return width > 0 && height > 0 && width <= 2048 && height <= 2048 && width * height <= 1'048'576;
	}
}
