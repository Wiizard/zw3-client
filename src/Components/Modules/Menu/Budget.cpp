#include "STDInclude.hpp"

#include "Budget.hpp"

#include <format>

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      constexpr uint64_t mib (1024 * 1024);

      // How much larger than its fast file a map is once loaded. Stock map
      // zones decompress to two to three times their size; four leaves
      // room for a map that compresses better than they do.
      //
      constexpr uint64_t expansion (4);

      // What the load and the frames after it allocate on top of the zone
      // itself: the zone's own load buffers, the world's GPU resources, and
      // whatever the menus do next.
      //
      constexpr uint64_t zone_margin (64 * mib);
      constexpr uint64_t physical_margin (512 * mib);

      // Asset entries. The engine's entry array (zone/engine/entry-pool.hxx)
      // is shared with every other loaded zone, and the menus' zones hold
      // entries that a level load would have freed: with the front end up
      // and the array grown, about 36600 are left. The maps we measured
      // took between 5000 (mp_boneyard) and 7900 (mp_terminal), so this
      // covers the largest with some room. Running out is a fatal engine
      // error, which is why we would rather not show a map than guess low.
      //
      constexpr uint32_t entry_need (8192);

      string
      size (uint64_t n)
      {
        return format ("{} MiB", (n + mib - 1) / mib);
      }
    }

    requirement
    require (uint64_t file_size) noexcept
    {
      const uint64_t z (file_size * expansion);

      return requirement {z + zone_margin, z + physical_margin, entry_need};
    }

    optional<string>
    shortfall (const resources& r, const requirement& q)
    {
      if (r.zone_free < q.zone)
        return format ("zone memory is short: {} free, {} needed",
                       size (r.zone_free), size (q.zone));

      if (r.physical_free < q.physical)
        return format ("physical memory is short: {} available, {} needed",
                       size (r.physical_free), size (q.physical));

      if (r.entries_free < q.entries)
        return format ("asset entries are short: {} free, {} needed",
                       r.entries_free, q.entries);

      return nullopt;
    }
  }
}
