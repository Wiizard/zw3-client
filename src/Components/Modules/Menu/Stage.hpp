#pragma once

#include "Types.hpp"



namespace Components::Menu
{
  namespace BackdropData
  {
    // The front end's backdrop: a multiplayer map rendered behind the menus
    // with one of its characters standing in it, the way recent titles
    // present their main menu.
    //
    // This unit is the part that knows nothing about the engine: which maps
    // may be shown, how the ui_backdrop dvar selects one, and where in the
    // map the character stands. Everything that touches the engine is in
    // mod/backdrop.cxx.

    // A map that may be shown.
    //
    // Random selection uses the stock table. Explicit selections accept
    // safe map identifiers; the engine adapter checks installation and budget.
    //
    struct stage
    {
      const char* map;
    };

    // Return the curated maps, in a fixed order.
    //
    span<const stage>
    stages () noexcept;

    // Return the stage for a map name (ignoring case) or nullptr if the map
    // identifier is invalid.
    //
    const stage*
    find_stage (string_view map);

    // What the ui_backdrop dvar asks for.
    //
    enum class selection_kind
    {
      off,    // No backdrop, the menus' own background.
      random, // A stage picked among those installed, once per session.
      map     // The named stage.
    };

    struct selection
    {
      selection_kind kind;
      const Components::Menu::BackdropData::stage* stage {nullptr}; // Only for map.
    };

    // Parse a ui_backdrop value: off, random, or a map in the table, with
    // surrounding blanks ignored and the case of the map name too. An empty
    // value means off.
    //
    // Throw std::invalid_argument if the value is a map that is not in the
    // set of safe map identifiers.
    //
    selection
    parse_selection (string_view);

    // Return the stage that seed picks among the candidates.
    //
    // The pick is the seed modulo the number of candidates, so a seed drawn
    // once per session shows the same map for the whole session, and the
    // same seed picks the same map for the same candidates.
    //
    // The candidates may not be empty.
    //
    const stage&
    pick_stage (span<const stage* const> candidates, uint64_t seed);

    // Where the character stands: a point on the floor and the direction
    // it faces, both in the map's units.
    //
    struct spawn
    {
      array<float, 3> origin;
      float yaw;
    };

    // Return the spawn the character is placed on, taken from the map's
    // entity string (MapEnts::entityString), or nullopt if the map has no
    // spawn with an origin.
    //
    // The spawn classes are tried in order of preference, the team starts
    // first, since those are where a map's designer stands a side before
    // the round, so they tend to face something worth looking at. The
    // index selects among the spawns of the first class present and wraps
    // around, so any value is valid.
    //
    // Throw std::invalid_argument if the entity string is malformed.
    //
    optional<spawn>
    find_spawn (string_view entities, size_t index = 0);
  }
}
