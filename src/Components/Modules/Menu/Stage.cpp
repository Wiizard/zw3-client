#include "STDInclude.hpp"

#include "Stage.hpp"

#include <charconv>  // from_chars()
#include <algorithm> // ranges::equal(), ranges::find_if()
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      // The curated maps (see stage in the header). The stock maps, all of
      // which have team starts.
      //
      const stage stage_table[]
      {
        {"mp_afghan"},
        {"mp_boneyard"},
        {"mp_brecourt"},
        {"mp_checkpoint"},
        {"mp_derail"},
        {"mp_estate"},
        {"mp_favela"},
        {"mp_highrise"},
        {"mp_invasion"},
        {"mp_nightshift"},
        {"mp_quarry"},
        {"mp_rundown"},
        {"mp_rust"},
        {"mp_subbase"},
        {"mp_terminal"},
        {"mp_underpass"}
      };

      // The spawn classes, in order of preference (see find_spawn()).
      //
      const char* const spawn_classes[]
      {
        "mp_tdm_spawn_allies_start",
        "mp_tdm_spawn_axis_start",
        "mp_sd_spawn_attacker",
        "mp_sd_spawn_defender",
        "mp_tdm_spawn",
        "mp_dm_spawn",
        "info_player_start",
        "info_player_deathmatch",
        "info_player_coop",
        "mp_zm_spawn"
      };

      bool
      iequal (string_view x, string_view y) noexcept
      {
        auto lower = [] (char c)
        {
          return c >= 'A' && c <= 'Z' ? static_cast<char> (c - 'A' + 'a') : c;
        };

        return ranges::equal (x, y, {}, lower, lower);
      }

      string_view
      trim (string_view s) noexcept
      {
        auto blank = [] (char c) {return c == ' ' || c == '\t' || c == '\r';};

        while (!s.empty () && blank (s.front ())) s.remove_prefix (1);
        while (!s.empty () && blank (s.back ()))  s.remove_suffix (1);

        return s;
      }

      // Parse n blank-separated numbers. Return false if there are fewer or
      // more, or if one of them is not a number.
      //
      bool
      parse_numbers (string_view s, span<float> r) noexcept
      {
        const char* b (s.data ());
        const char* e (b + s.size ());

        for (float& v: r)
        {
          while (b != e && *b == ' ')
            ++b;

          auto [p, ec] = from_chars (b, e, v);

          if (ec != errc () || !std::isfinite (v))
            return false;

          b = p;
        }

        while (b != e && *b == ' ')
          ++b;

        return b == e;
      }

      // An entity: its key/value pairs as they appear in the string, which
      // the string itself outlives.
      //
      using entity = vector<pair<string_view, string_view>>;

      optional<string_view>
      value (const entity& e, string_view k) noexcept
      {
        auto i (ranges::find (e, k, &pair<string_view, string_view>::first));

        if (i == e.end ())
          return nullopt;

        return i->second;
      }

      // Split the entity string into entities.
      //
      // The format is the one the map compiler writes and the engine's own
      // G_ParseSpawnVars reads: a sequence of brace-enclosed entities, each
      // a sequence of quoted key and value pairs, with whitespace (newlines
      // included) between the tokens:
      //
      // {
      // "classname" "mp_dm_spawn"
      // "origin" "1 2 3"
      // }
      //
      vector<entity>
      parse_entities (string_view s)
      {
        vector<entity> r;

        size_t i (s.starts_with ("\xEF\xBB\xBF") ? 3 : 0);

        auto skip = [&s, &i] ()
        {
          for (;;)
          {
            while (i != s.size () &&
                   (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
            if (s.substr (i).starts_with ("//"))
            {
              const size_t end (s.find ('\n', i + 2));
              i = end == string_view::npos ? s.size () : end + 1;
            }
            else if (s.substr (i).starts_with ("/*"))
            {
              const size_t end (s.find ("*/", i + 2));
              if (end == string_view::npos) throw invalid_argument ("unterminated entity comment");
              i = end + 2;
            }
            else break;
          }
        };

        auto quoted = [&s, &i] () -> string_view
        {
          size_t e (s.find ('"', ++i)); // Skip the opening quote.

          if (e == string_view::npos)
            throw invalid_argument ("unterminated string");

          string_view r (s.substr (i, e - i));
          i = e + 1;
          return r;
        };

        for (skip (); i != s.size (); skip ())
        {
          if (s[i] != '{')
            throw invalid_argument ("expected '{'");

          ++i;
          entity e;

          for (;;)
          {
            skip ();

            if (i == s.size ())
              throw invalid_argument ("unterminated entity");

            if (s[i] == '}')
            {
              ++i;
              break;
            }

            if (s[i] != '"')
              throw invalid_argument ("expected key");

            string_view k (quoted ());

            skip ();

            if (i == s.size () || s[i] != '"')
              throw invalid_argument (
                "missing value for key '" + string (k) + '\'');

            e.emplace_back (k, quoted ());
          }

          r.push_back (move (e));
        }

        return r;
      }
    }

    span<const stage>
    stages () noexcept
    {
      return stage_table;
    }

    const stage*
    find_stage (string_view map)
    {
      auto i (ranges::find_if (stage_table,
                               [map] (const stage& s)
                               {
                                 return iequal (s.map, map);
                               }));

      if (i != ranges::end (stage_table)) return &*i;

      // Random selection stays curated. Explicit lobby selections may name
      // installed custom/DLC maps; reject paths before the file resolver runs.
      if (map.empty () || map.size () > 63) return nullptr;
      string name;
      for (char c: map)
      {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return nullptr;
        name.push_back (c);
      }
      if (name[0] < 'a' || name[0] > 'z') return nullptr;
      static std::mutex mutex;
      static std::map<string, stage> custom;
      std::lock_guard lock (mutex);
      auto [entry, inserted] = custom.try_emplace (name, stage{});
      if (inserted) entry->second.map = entry->first.c_str ();
      return &entry->second;
    }

    selection
    parse_selection (string_view v)
    {
      v = trim (v);

      if (v.empty () || iequal (v, "off"))
        return selection {selection_kind::off};

      if (iequal (v, "random"))
        return selection {selection_kind::random};

      if (const stage* s = find_stage (v))
        return selection {selection_kind::map, s};

      throw invalid_argument (
        "map '" + string (v) + "' is not a backdrop stage");
    }

    const stage&
    pick_stage (span<const stage* const> cs, uint64_t seed)
    {
      return *cs[seed % cs.size ()];
    }

    optional<spawn>
    find_spawn (string_view es, size_t index)
    {
      const vector<entity> ents (parse_entities (es));

      // Go over the classes in order of preference and settle on the first
      // one with at least one spawn we can use. Note that an entity without
      // an origin is not usable and is skipped rather than diagnosed: the
      // engine would place it at the map's origin, which is no better.
      //
      for (const char* c: spawn_classes)
      {
        vector<spawn> ss;

        for (const entity& e: ents)
        {
          optional<string_view> cn (value (e, "classname"));

          if (!cn || *cn != c)
            continue;

          optional<string_view> o (value (e, "origin"));

          if (!o)
            continue;

          spawn s {};

          if (!parse_numbers (*o, s.origin))
            throw invalid_argument ("invalid origin '" + string (*o) + '\'');

          if (optional<string_view> a = value (e, "angles"))
          {
            array<float, 3> as;

            if (!parse_numbers (*a, as))
              throw invalid_argument ("invalid angles '" + string (*a) + '\'');

            s.yaw = as[1];
          }
          else if (optional<string_view> single_angle = value (e, "angle"))
          {
            array<float, 1> angle;
            if (!parse_numbers (*single_angle, angle)) throw invalid_argument ("invalid spawn angle");
            s.yaw = angle[0];
          }

          ss.push_back (s);
        }

        if (!ss.empty ())
          return ss[index % ss.size ()];
      }

      return nullopt;
    }
  }
}
