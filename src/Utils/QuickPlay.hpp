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
		// False when the player unticked this playlist in the Quick Play filter.
		bool isIncluded = true;
		// Fresh player count in the playlist, or -1 when it is not known.
		int population = -1;
	};

	// Catalog position, the selected row and restricted development access must
	// never decide which public queue Quick Play enters.
	//
	// Quick Play enters one queue, so players who ticked different sets of
	// playlists only meet if they pick the same one. Among the eligible ones,
	// take the busiest: whoever searches second joins the queue the first
	// already waits in, when it is among their ticked playlists. Equal counts
	// (or none known) fall back to the standard queue, then the lowest ID.
	inline std::optional<std::size_t> SelectQuickPlayPlaylist(const std::span<const QuickPlayPlaylist> playlists,
		const bool isLoaded, const bool isStale, const std::size_t partySize)
	{
		if (!isLoaded || isStale || partySize == 0) return std::nullopt;

		std::optional<std::size_t> selected;
		for (std::size_t index = 0; index < playlists.size(); ++index)
		{
			const auto& playlist = playlists[index];
			if (playlist.id.empty() || !playlist.isIncluded || playlist.audience != "PUBLIC" || playlist.availability != "AVAILABLE"
				|| !playlist.hasVerifiedContent || playlist.maxPlayers < 1
				|| partySize > static_cast<std::size_t>(playlist.maxPlayers)) continue;

			if (!selected)
			{
				selected = index;
				continue;
			}

			const auto& best = playlists[*selected];
			if (playlist.population != best.population)
			{
				if (playlist.population > best.population) selected = index;
				continue;
			}

			const bool isStandard = playlist.id == "zombies-quickplay";
			const bool isBestStandard = best.id == "zombies-quickplay";
			if (isStandard != isBestStandard)
			{
				if (isStandard) selected = index;
				continue;
			}

			if (playlist.id < best.id) selected = index;
		}
		return selected;
	}
}
