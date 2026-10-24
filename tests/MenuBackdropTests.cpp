#include "Stage.hpp"
#include "Camera.hpp"
#include "Cast.hpp"
#include "Budget.hpp"
#include "Turntable.hpp"

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
		rejected = false;
		try { find_spawn("{\"classname\" \"mp_dm_spawn\" \"origin\" \"nan 0 0\"}"); }
		catch (const std::invalid_argument&) { rejected = true; }
		Require(rejected, "non-finite camera origins must be rejected");
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
		std::cout << "Menu backdrop tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
