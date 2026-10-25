#pragma once

#include "Stage.hpp"
#include <algorithm>

namespace Components::Menu::BackdropData
{
	inline constexpr array<string_view, 5> default_backdrop_maps{
		"mp_zombie_theater", "mp_zombie_pentagon", "mp_factory_sh",
		"mp_cod5_prototype", "mp_zombie_diner"
	};

	struct lobby_selection
	{
		bool private_lobby = false;
		bool matchmaking = false;
		bool voting = false;
		string_view matchmaking_state;
		string_view private_map;
		string_view match_map;
	};

	struct backdrop_request
	{
		selection choice;
		bool follows_lobby = false;
	};

	class backdrop_selector
	{
	public:
		backdrop_request choose(const lobby_selection& lobby) const
		{
			const stage* selected = nullptr;
			if (lobby.private_lobby)
				selected = find_stage(lobby.private_map);
			else if (lobby.matchmaking && !lobby.voting)
			{
				constexpr array<string_view, 13> picked_states{
					"MATCH_FOUND", "READY_CHECK", "WAITING_FOR_READY", "RESERVING_SERVER",
					"STARTING_SERVER", "SERVER_STARTING", "COUNTDOWN", "JOIN_PREVIEW",
					"CONNECTING", "DIRECT_CONNECTION", "RELAY_CONNECTION", "RESETTING", "POST_MATCH"
				};
				if (std::ranges::find(picked_states, lobby.matchmaking_state) != picked_states.end())
					selected = find_stage(lobby.match_map);
			}
			if (selected) return {{selection_kind::map, selected}, true};
			if (!default_map_.empty()) return {{selection_kind::map, find_stage(default_map_)}, false};
			return {{selection_kind::random}, false};
		}

		void remember_default(string_view map)
		{
			if (default_map_.empty() && std::ranges::find(default_backdrop_maps, map) != default_backdrop_maps.end())
				default_map_ = map;
		}

		void reject(string_view map)
		{
			if (default_map_ == map) default_map_.clear();
		}

	private:
		string default_map_;
	};
}
