#include "STDInclude.hpp"

#include "Cast.hpp"

#include <algorithm> // ranges::sort(), ranges::find(), ranges::count()

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      constexpr string_view body_prefix ("mp_body_");
      constexpr string_view head_prefix ("head_");

      // The ambient campaign animations first, then the multiplayer idles.
      //
      const pose pose_table[]
      {
        {"patrol_bored_idle_smoke",    false},
        {"civilian_smoking_a",         false},
        {"patrol_bored_idle",          true},
        {"pb_stand_alert",             true},
        {"pb_stand_alert_pistol",      true}
      };

      // Rifles, the world models of the multiplayer default classes.
      //
      const char* const weapon_table[]
      {
        "weapon_m4",
        "weapon_ak47",
        "weapon_scar_h",
        "weapon_famas",
        "weapon_m16"
      };

      // Return the words of a model name that tell a faction apart, that
      // is, every '_'-separated word except those every body or head has.
      //
      vector<string_view>
      faction_words (string_view n)
      {
        vector<string_view> r;

        for (size_t b (0); b <= n.size (); )
        {
          size_t e (n.find ('_', b));

          if (e == string_view::npos)
            e = n.size ();

          string_view w (n.substr (b, e - b));

          if (!w.empty () && w != "mp" && w != "body" && w != "head")
            r.push_back (w);

          b = e + 1;
        }

        return r;
      }
    }

    span<const pose>
    poses () noexcept
    {
      return pose_table;
    }

    optional<cast>
    choose_cast (span<const string> models,
                 span<const string> anims,
                 uint64_t seed)
    {
      // Sort the bodies and heads out of the models.
      //
      vector<string> bodies;
      vector<string> heads;

      for (const string& m: models)
      {
        if (m.starts_with (body_prefix))
          bodies.push_back (m);
        else if (m.starts_with (head_prefix))
          heads.push_back (m);
      }

      if (bodies.empty ())
        return nullopt;

      ranges::sort (bodies);
      ranges::sort (heads);

      cast r;
      r.body = bodies[seed % bodies.size ()];

      // Pair the head by faction words. Note that the strict comparison
      // keeps the first head (by name) of those with the best score.
      //
      {
        const vector<string_view> bw (faction_words (r.body));

        size_t best (0);

        for (const string& h: heads)
        {
          size_t n (0);

          for (string_view w: faction_words (h))
            n += ranges::count (bw, w) != 0 ? 1 : 0;

          if (!r.head || n > best)
          {
            r.head = h;
            best = n;
          }
        }
      }

      // Pick the pose and, if it is armed, the weapon.
      //
      for (const pose& p: pose_table)
      {
        if (ranges::find (anims, string_view (p.anim)) != anims.end ())
        {
          r.animation = p;
          break;
        }
      }

      if (r.animation && r.animation->armed)
      {
        for (const char* w: weapon_table)
        {
          if (ranges::find (models, string_view (w)) != models.end ())
          {
            r.weapon = w;
            break;
          }
        }
      }

      return r;
    }
  }
}
