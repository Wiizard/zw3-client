#pragma once

#include "Types.hpp"



namespace Components::Menu
{
  namespace BackdropData
  {
    // Whether there is room for a map in the front end.
    //
    // The engine never has a map and the menus loaded together on its own:
    // a level load frees the menus' zones before it loads the map. The
    // backdrop does have both, so before it asks for a map it makes sure
    // the map fits, and it refuses rather than finding out the hard way.
    // The hard way is particularly hard here. Running out of zone memory is
    // not diagnosed by the engine at all (see zone/engine/zone-memory.hxx):
    // the zone lands in memory nobody validated and something unrelated
    // breaks later. Running out of asset entries is a fatal error. And
    // running the machine out of physical memory makes it page for as long
    // as the menus are up.
    //
    // A fast file's size on disk is all we know before it is loaded, and it
    // is compressed, so what it needs is an estimate: a multiple of that
    // size, with a margin on top for what the load itself and the frames
    // after it allocate. The multiple is on the safe side of what stock
    // maps decompress to. The module logs what a load really took so the
    // estimate can be checked against it.

    // What there is room for.
    //
    struct resources
    {
      uint64_t zone_free;     // Zone memory not yet allocated, in bytes.
      uint64_t physical_free; // Physical memory available, in bytes.
      uint32_t entries_free;  // Asset entries on the engine's free list.
    };

    // What a map needs.
    //
    struct requirement
    {
      uint64_t zone;
      uint64_t physical;
      uint32_t entries;
    };

    // Return what a map whose fast file is file_size bytes on disk needs.
    //
    requirement
    require (uint64_t file_size) noexcept;

    // Return why there is no room for a requirement, as a diagnostic
    // fragment naming the first resource that is short and by how much, or
    // nullopt if there is room.
    //
    optional<string>
    shortfall (const resources&, const requirement&);
  }
}
