#include "Utils/QuickPlay.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
	void Require(const bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}

	std::string_view Selected(const std::vector<Utils::QuickPlayPlaylist>& playlists, const std::size_t partySize = 1,
		const bool isLoaded = true, const bool isStale = false)
	{
		const auto selected = Utils::SelectQuickPlayPlaylist(playlists, isLoaded, isStale, partySize);
		return selected ? playlists[*selected].id : std::string_view{};
	}
}

int main()
{
	try
	{
		std::vector<Utils::QuickPlayPlaylist> playlists{
			{ "playtest-v9-dev", "RESTRICTED", "AVAILABLE", 4, true },
			{ "z-public", "PUBLIC", "AVAILABLE", 4, true },
			{ "a-public", "PUBLIC", "AVAILABLE", 4, true },
		};
		Require(Selected(playlists) == "a-public", "Quick Play must choose a deterministic public queue, not visible row zero");
		std::ranges::reverse(playlists);
		Require(Selected(playlists) == "a-public", "Catalog order and pagination must not change the public selection");
		playlists.push_back({ "zombies-quickplay", "PUBLIC", "AVAILABLE", 4, true });
		Require(Selected(playlists) == "zombies-quickplay", "The eligible legacy standard public queue retains priority");
		playlists.back().availability = "NO_CAPACITY";
		Require(Selected(playlists) == "a-public", "An unavailable standard queue must not block other eligible public queues");
		playlists.back().availability = "AVAILABLE";
		playlists.back().audience = "RESTRICTED";
		Require(Selected(playlists) == "a-public", "Even the standard ID must not bypass restricted audience rules");
		Require(Selected(playlists, 1, false).empty(), "An unloaded catalog cannot be used");
		Require(Selected(playlists, 1, true, true).empty(), "A stale catalog cannot be used");
		Require(Selected(playlists, 0).empty(), "Unknown party size must not become a fabricated solo party");
		Require(Selected(playlists, 5).empty(), "Quick Play must not choose a queue too small for the party");
		Require(Selected(playlists, 4) == "a-public", "Exact party capacity remains eligible");

		std::vector<Utils::QuickPlayPlaylist> unavailable{
			{ "dev-only", "RESTRICTED", "AVAILABLE", 4, true },
			{ "full", "PUBLIC", "NO_CAPACITY", 4, true },
			{ "content-missing", "PUBLIC", "CONTENT_MISSING", 4, false },
			{ "content-corrupt", "PUBLIC", "CONTENT_CORRUPT", 4, false },
			{ "unverified", "PUBLIC", "AVAILABLE", 4, false },
			{ "small", "PUBLIC", "AVAILABLE", 1, true },
			{ "invalid-audience", "UNKNOWN", "AVAILABLE", 4, true },
			{ "invalid-capacity", "PUBLIC", "AVAILABLE", -1, true },
		};
		Require(Selected(unavailable, 2).empty(), "Unavailable, restricted, unverified and incompatible playlists must all fail closed");
		unavailable.push_back({ "off-page-public", "PUBLIC", "AVAILABLE", 4, true });
		Require(Selected(unavailable, 2) == "off-page-public", "A valid public playlist beyond the five visible rows remains selectable");
		Require(Selected({ { "dev-only", "RESTRICTED", "AVAILABLE", 4, true } }).empty(), "Restricted-only access must require an explicit development playlist click");
		Require(Selected({}).empty(), "An empty catalog must not invent a default playlist");
		std::cout << "Quick Play public selection, content, capacity, freshness and page-independent tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
