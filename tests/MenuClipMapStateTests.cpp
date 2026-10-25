#include "Components/Modules/Menu/ClipMapState.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
	struct ClipMap
	{
		const char* name;
		const void* mapEnts;
	};
	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using Components::Menu::BackdropData::loaded_clip_map_name;
		Require(loaded_clip_map_name<ClipMap>(nullptr) == nullptr, "missing clip map");

		// A freed name can be unreadable or can already contain unrelated data.
		ClipMap disconnected{ reinterpret_cast<const char*>(std::uintptr_t{ 1 }), nullptr };
		Require(loaded_clip_map_name(&disconnected) == nullptr && disconnected.name == nullptr,
			"disconnect must discard an unreadable name without reading it");
		Require(loaded_clip_map_name(&disconnected) == nullptr, "repeated frontend frames must stay safe");
		disconnected.name = "unrelated allocation reused the old name";
		Require(loaded_clip_map_name(&disconnected) == nullptr, "readable stale names must also be discarded");

		const int entities = 0;
		ClipMap resident{ "maps/mp/mp_zombie_coast.d3dbsp", &entities };
		Require(std::string_view(loaded_clip_map_name(&resident)) == "maps/mp/mp_zombie_coast.d3dbsp",
			"a resident level must remain available to the backdrop and same-map devmap handoff");

		resident.mapEnts = nullptr;
		Require(loaded_clip_map_name(&resident) == nullptr && resident.name == nullptr,
			"unloading a previously resident level must invalidate its name");
		resident = { "maps/mp/mp_zombie_theater.d3dbsp", &entities };
		Require(std::string_view(loaded_clip_map_name(&resident)) == "maps/mp/mp_zombie_theater.d3dbsp",
			"the next map must be recognized after unloading");

		std::cout << "Menu clip-map lifecycle: disconnect, resident devmap handoff and reload passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
