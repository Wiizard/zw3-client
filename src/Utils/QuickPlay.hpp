#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace Utils
{
	struct QuickPlayPlaylist
	{
		std::string_view id;
		std::string_view audience;
		std::string_view availability;
		int maxPlayers;
		bool hasVerifiedContent;
	};

	// Catalog position, the selected row and restricted development access must
	// never decide which public queue Quick Play enters.
	inline std::optional<std::size_t> SelectQuickPlayPlaylist(const std::span<const QuickPlayPlaylist> playlists,
		const bool isLoaded, const bool isStale, const std::size_t partySize)
	{
		if (!isLoaded || isStale || partySize == 0) return std::nullopt;

		std::optional<std::size_t> selected;
		for (std::size_t index = 0; index < playlists.size(); ++index)
		{
			const auto& playlist = playlists[index];
			if (playlist.id.empty() || playlist.audience != "PUBLIC" || playlist.availability != "AVAILABLE"
				|| !playlist.hasVerifiedContent || playlist.maxPlayers < 1
				|| partySize > static_cast<std::size_t>(playlist.maxPlayers)) continue;

			if (playlist.id == "zombies-quickplay") return index;
			if (!selected || playlist.id < playlists[*selected].id) selected = index;
		}
		return selected;
	}
}
