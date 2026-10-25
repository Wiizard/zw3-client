#include "Stage.hpp"
#include "Camera.hpp"
#include "Cast.hpp"
#include "Budget.hpp"
#include "Turntable.hpp"
#include "Transition.hpp"
#include "Selection.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
	bool Near(float a, float b) { return std::abs(a - b) < 0.001f; }
}

int main()
{
	using namespace Components::Menu::BackdropData;
	try
	{
		Require(parse_selection(" \tOFF\r").kind == selection_kind::off, "off setting");
		Require(parse_selection(" RANDOM ").kind == selection_kind::random, "random setting");
		Require(parse_selection("MP_RUST").stage == find_stage("mp_rust"), "map normalization");
		const auto custom = parse_selection("MP_OILRIG");
		Require(custom.stage && std::string_view(custom.stage->map) == "mp_oilrig", "selected custom maps are supported");
		Require(custom.stage == find_stage("mp_oilrig"), "custom stage references remain stable");
		Require(parse_selection("OILRIG").stage == find_stage("oilrig"), "ported campaign maps follow the lobby selection");
		Require(!find_stage("mp_../../bad") && !find_stage("C:\\map.ff"), "map selection rejects paths");
		bool rejected = false;
		try { parse_selection("../unlisted"); } catch (const std::invalid_argument&) { rejected = true; }
		Require(rejected, "unlisted stages must be rejected");
		const auto standing = find_spawn(R"(
{"classname" "mp_dm_spawn" "origin" "99 99 99"}
{"classname" "mp_tdm_spawn_allies_start" "origin" "1 2 3" "angles" "0 90 0"}
{"classname" "mp_tdm_spawn_allies_start" "origin" "4 5 6"}
)", 3);
		Require(standing && standing->origin[0] == 4, "preferred spawns and wrapped index");
		Require(!find_spawn("{\"classname\" \"mp_dm_spawn\"}"), "origin required");
		const auto coop = find_spawn("{\"classname\" \"info_player_start\" \"origin\" \"1 2 3\" \"angle\" \"270\"}");
		Require(coop && Near(coop->yaw, 270), "custom zombie maps use their player starts and yaw");
		const auto commented = find_spawn(R"(///dark water
/* replacement entities */ { "classname" /* note */ "info_player_start"
"origin" "4 5 6" // grounded spawn
"angle" "90" } // eof
)");
		Require(commented && Near(commented->origin[0], 4) && Near(commented->yaw, 90), "Oil Rig replacement entity comments are accepted");
		Require(!find_spawn("{\"classname\" \"mp_dm_spawn\" \"origin\" \"nan 0 0\"}"),
			"non-finite camera origins must not produce a spawn");
		const auto finiteSpawn = find_spawn(R"(
{"classname" "mp_dm_spawn" "origin" "nan 0 0"}
{"classname" "mp_dm_spawn" "origin" "1 2 3"}
)");
		Require(finiteSpawn && Near(finiteSpawn->origin[0], 1), "unusable spawns must not hide valid map placements");

		backdrop_selector selector;
		Require(selector.choose({}).choice.kind == selection_kind::random, "first menu chooses a random default");
		std::vector<const stage*> defaultPool;
		for (const auto map : default_backdrop_maps) defaultPool.push_back(find_stage(map));
		Require(defaultPool.size() == 5, "default backdrop pool has exactly five maps");
		for (std::size_t i = 0; i < defaultPool.size(); ++i)
		{
			Require(defaultPool[i] != nullptr && &pick_stage(defaultPool, i) == defaultPool[i], "every default map can be picked");
			for (std::size_t j = 0; j < i; ++j) Require(defaultPool[j] != defaultPool[i], "default pool has no duplicates");
		}
		for (const auto map : {"mp_zombie_theater", "mp_zombie_pentagon", "mp_factory_sh", "mp_cod5_prototype", "mp_zombie_diner"})
			Require(std::find(default_backdrop_maps.begin(), default_backdrop_maps.end(), map) != default_backdrop_maps.end(), "requested classic maps are in the pool");
		selector.remember_default("mp_zombie_theater");
		selector.remember_default("mp_oilrig");
		selector.remember_default("mp_zombie_diner");
		const auto home = find_stage("mp_zombie_theater");
		Require(selector.choose({}).choice.stage == home, "a chosen default remains stable throughout the session");
		lobby_selection lobby{true, false, false, "", "mp_rust", ""};
		Require(selector.choose(lobby).follows_lobby && selector.choose(lobby).choice.stage == find_stage("mp_rust"), "private lobby follows its selected map");
		lobby.private_map = "MP_OILRIG";
		Require(selector.choose(lobby).choice.stage == find_stage("mp_oilrig"), "changing a private selection follows the new custom map");
		Require(selector.choose({}).choice.stage == home, "leaving a private lobby restores the session default");
		lobby = {false, true, false, "SEARCHING", "mp_rust", "mp_oilrig"};
		Require(!selector.choose(lobby).follows_lobby && selector.choose(lobby).choice.stage == home, "searching ignores stale map dvars");
		lobby.matchmaking_state = "MAP_VOTE";
		Require(selector.choose(lobby).choice.stage == home, "vote candidates do not change the backdrop before a winner is chosen");
		lobby.matchmaking_state = "WAITING_FOR_READY";
		lobby.voting = true;
		Require(selector.choose(lobby).choice.stage == home, "an active vote cannot expose a stale winner");
		lobby.voting = false;
		for (const auto state : {"MATCH_FOUND", "READY_CHECK", "WAITING_FOR_READY", "RESERVING_SERVER", "SERVER_STARTING", "COUNTDOWN", "JOIN_PREVIEW", "CONNECTING"})
		{
			lobby.matchmaking_state = state;
			Require(selector.choose(lobby).follows_lobby && selector.choose(lobby).choice.stage == find_stage("mp_oilrig"), "confirmed vote or playlist map follows through server startup");
		}
		lobby.match_map = "mp_zombie_pentagon";
		Require(selector.choose(lobby).choice.stage == find_stage("mp_zombie_pentagon"), "the next matchmaking result replaces the previous map");
		lobby.matchmaking = false;
		Require(selector.choose(lobby).choice.stage == home, "main menu ignores the previous matchmaking winner");
		lobby.matchmaking = true;
		for (const auto state : {"IDLE", "IN_PARTY", "ONLINE", "ERROR", "OFFLINE"})
		{
			lobby.matchmaking_state = state;
			Require(selector.choose(lobby).choice.stage == home, "cancelling or leaving matchmaking restores the default");
		}
		lobby = {true, false, false, "", "../../map", ""};
		Require(selector.choose(lobby).choice.stage == home, "unsafe lobby map names fall back to the default");
		selector.reject("mp_oilrig");
		Require(selector.choose({}).choice.stage == home, "rejecting a lobby map does not reroll the default");
		selector.reject("mp_zombie_theater");
		Require(selector.choose({}).choice.kind == selection_kind::random, "an unavailable default can be replaced from the five-map pool");
		rejected = false;
		try { find_spawn("{\"classname\""); } catch (const std::invalid_argument&) { rejected = true; }
		Require(rejected, "malformed entity strings must be rejected");

		const auto narrow = tangents(55, 4.0f / 3.0f);
		const auto wide = tangents(55, 16.0f / 9.0f);
		Require(Near(narrow.y, wide.y) && wide.x > narrow.x, "widescreen preserves vertical FOV");
		const auto camera = frame(spawn{{0, 0, 0}, 90}, default_framing, 16.0f / 9.0f, 10);
		for (const auto& axis : camera.axis)
			Require(Near(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2], 1), "unit camera axes");
		Require(Near(camera.axis[0][0]*camera.axis[1][0] + camera.axis[0][1]*camera.axis[1][1] + camera.axis[0][2]*camera.axis[1][2], 0), "orthogonal camera axes");

		const spawn actorSpace {{0, -765, 245}, -90};
		const spawn mapSpace {{100, 200, 30}, 90};
		const view worldView {{100, 500, 115}, angles_to_axis({0, -90, 0}), wide};
		const auto actorView = rebase_view(worldView, mapSpace, actorSpace);
		Require(Near(actorView.origin[0], 0) && Near(actorView.origin[1], -1065) && Near(actorView.origin[2], 330), "actor camera follows the map spawn");
		Require(Near(actorView.axis[0][1], 1) && Near(actorView.fov.x, wide.x), "actor camera preserves facing and FOV");
		const auto roundTrip = rebase_view(actorView, actorSpace, mapSpace);
		for (std::size_t i=0; i<3; ++i)
		{
			Require(Near(roundTrip.origin[i], worldView.origin[i]), "actor camera transform is reversible");
			for (std::size_t j=0; j<3; ++j) Require(Near(roundTrip.axis[i][j], worldView.axis[i][j]), "actor axes transform is reversible");
		}

		const std::vector<std::string> models{"head_russian", "mp_body_us_army", "head_us_army", "weapon_m4"};
		const std::vector<std::string> idle{"pb_stand_alert"}, smoking{"patrol_bored_idle_smoke"};
		const auto soldier = choose_cast(models, idle, 42);
		Require(soldier && soldier->head == "head_us_army" && soldier->weapon == "weapon_m4", "faction matching and armed pose");
		Require(!choose_cast(models, smoking, 42)->weapon, "smoking pose must not hold a rifle");
		Require(!choose_cast({}, idle, 1), "missing body fallback");
		Require(!choose_cast(models, {}, 1)->animation, "missing animation fallback");

		const auto needed = require(1024 * 1024);
		Require(!shortfall({needed.zone, needed.physical, needed.entries}, needed), "exact resource budget");
		Require(shortfall({needed.zone-1, needed.physical, needed.entries}, needed).has_value(), "zone budget rejection");
		Require(shortfall({needed.zone, needed.physical, needed.entries-1}, needed).has_value(), "entry budget rejection");
		turntable one, many;
		one.turn(90); many.turn(90);
		const float target = one.advance(0.1f);
		for (int i=0; i<10; ++i) many.advance(0.01f);
		Require(Near(target, many.yaw()), "turntable is independent of frame rate");
		one.reset();
		Require(std::abs(one.advance(1)) < 0.01f, "turntable resets to its initial facing");

		teleport_transition transit;
		Require(!transit.active() && Near(transit.amount(1000), 0) && transit.started_at(40) == 40,
			"no transport over the ordinary menu; startup supplies its own animation clock");
		transit.close(1000);
		Require(Near(transit.amount(1000), 0) && Near(transit.amount(1225), 0.5f), "departure covers the old world gradually");
		Require(!transit.covered(1449) && !transit.covered(1569), "old world stays until a fully covered frame has been presented");
		Require(transit.covered(1570) && Near(transit.amount(1570), 1), "world may be unloaded behind the transport");
		Require(transit.covered(31000) && Near(transit.amount(31000), 1), "slow map loads keep their transport cover");
		transit.open(31250);
		Require(Near(transit.amount(31250), 1) && transit.started_at(40) == 1000,
			"arrival continues the animation without exposing an unfinished scene");
		Require(Near(transit.amount(31700), 0.5f) && !transit.finished(32149) && transit.finished(32150), "ready scene is revealed over the arrival blend");
		transit.reset();
		Require(!transit.active() && Near(transit.amount(32150), 0), "failure, disabling and leaving the menu remove the effect");
		transit.close(40000);
		const float departure = transit.amount(40225);
		transit.open(40225);
		Require(Near(transit.amount(40225), departure), "cancelling a map change does not jump opacity");
		const float arrival = transit.amount(40400);
		transit.close(40400);
		Require(Near(transit.amount(40400), arrival), "another map change during arrival keeps its current opacity");
		transit.reset();
		transit.hold(0);
		Require(transit.active() && Near(transit.amount(0), 1) && Near(transit.amount(30000), 1),
			"loading without an old scene starts fully covered, including at clock zero");
		for (const auto [width, height] : {std::pair{640, 480}, {1920, 1080}, {3440, 1440}, {3840, 2160}})
		{
			int count = 0;
			transport_arcs(width, height, 900, [&] (const transport_arc& arc)
			{
				++count;
				for (const auto& point : arc.vertices)
					Require(point[0] >= 0 && point[1] >= 0 && point[0] <= width && point[1] <= height,
						"electric arcs stay in the backdrop viewport on every display");
				const float length = std::hypot(arc.vertices[1][0] - arc.vertices[0][0], arc.vertices[1][1] - arc.vertices[0][1]);
				const float thickness = std::hypot(arc.vertices[2][0] - arc.vertices[1][0], arc.vertices[2][1] - arc.vertices[1][1]);
				Require(length <= 512 && Near(length / thickness, 4), "original Kino texture keeps its aspect ratio and is never enlarged");
				Require(arc.alpha > 0 && arc.alpha < 1, "teleport light intensity stays bounded");
			});
			Require(count > 0, "the loading veil always has visible electricity");
		}
		std::cout << "Menu backdrop tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
