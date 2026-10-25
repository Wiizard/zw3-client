#include "STDInclude.hpp"

#include "Director.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      constexpr double radians_per_degree (numbers::pi / 180.0);

      // A small, well mixed generator (splitmix64), so that a seed gives
      // the same sequence on every machine and every run.
      //
      struct generator
      {
        uint64_t state;

        uint64_t
        next () noexcept
        {
          uint64_t z (state += 0x9E3779B97F4A7C15ull);
          z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
          z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
          return z ^ (z >> 31);
        }

        // Uniform in [lo, hi].
        //
        float
        between (float lo, float hi) noexcept
        {
          const double u (static_cast<double> (next () >> 11) / 9007199254740992.0);
          return static_cast<float> (lo + (hi - lo) * u);
        }

        // Minus or plus one.
        //
        float
        side () noexcept
        {
          return (next () & 1) != 0 ? 1.0f : -1.0f;
        }
      };

      float
      smooth (float t) noexcept
      {
        t = clamp (t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
      }

      vec3
      lerp3 (const vec3& a, const vec3& b, float t) noexcept
      {
        return vec3 {a[0] + (b[0] - a[0]) * t,
                     a[1] + (b[1] - a[1]) * t,
                     a[2] + (b[2] - a[2]) * t};
      }

      vec3
      flat (float yaw_degrees) noexcept
      {
        const double y (yaw_degrees * radians_per_degree);
        return vec3 {static_cast<float> (cos (y)), static_cast<float> (sin (y)), 0.0f};
      }

      uint64_t
      hash_name (string_view s) noexcept
      {
        // FNV-1a, lowercased, so that the seed doesn't depend on the case
        // the map name happens to be spelled in.
        //
        uint64_t h (0xCBF29CE484222325ull);

        for (char c: s)
        {
          if (c >= 'A' && c <= 'Z')
            c = static_cast<char> (c - 'A' + 'a');

          h ^= static_cast<unsigned char> (c);
          h *= 0x100000001B3ull;
        }

        return h;
      }

      // The scene file.
      //
      using json = nlohmann::json;

      float
      number (const json& j, const char* key, float fallback)
      {
        auto i (j.find (key));

        if (i == j.end ())
          return fallback;

        if (!i->is_number ())
          throw invalid_argument (string ("'") + key + "' is not a number");

        const float v (i->get<float> ());

        if (!isfinite (v))
          throw invalid_argument (string ("'") + key + "' is not finite");

        return v;
      }

      shot_kind
      kind_of (const string& n)
      {
        if (n == "orbit") return shot_kind::orbit;
        if (n == "dolly") return shot_kind::dolly;
        if (n == "crane") return shot_kind::crane;
        if (n == "push")  return shot_kind::push;
        if (n == "hold")  return shot_kind::hold;

        throw invalid_argument ("unknown shot type '" + n + "'");
      }

      shot
      parse_shot (const json& j)
      {
        if (!j.is_object ())
          throw invalid_argument ("a shot is not an object");

        shot s;

        if (auto i = j.find ("type"); i != j.end ())
        {
          if (!i->is_string ())
            throw invalid_argument ("a shot's type is not a string");

          s.kind = kind_of (i->get<string> ());
        }

        s.duration   = number (j, "duration", s.duration);
        s.blend      = number (j, "blend", s.blend);
        s.yaw        = number (j, "yaw", s.yaw);
        s.arc        = number (j, "arc", s.arc);
        s.distance   = number (j, "distance", s.distance);
        s.travel     = number (j, "travel", s.travel);
        s.height     = number (j, "height", s.height);
        s.rise       = number (j, "rise", s.rise);
        s.aim_height = number (j, "aim_height", s.aim_height);
        s.fov        = number (j, "fov", s.fov);
        s.fov_end    = number (j, "fov_end", s.fov);

        s.duration = clamp (s.duration, 2.0f, 120.0f);
        s.blend = clamp (s.blend, 0.0f, s.duration * 0.5f);
        s.distance = clamp (s.distance, 24.0f, 4000.0f);
        s.fov = clamp (s.fov, 20.0f, 90.0f);
        s.fov_end = clamp (s.fov_end, 20.0f, 90.0f);

        return s;
      }

      // Apply what an entry (a preset, the default, or a map's) says on top
      // of a script: its shots or automatic style first, then the rest.
      //
      void
      apply (scene_script& r, const json& e, const json& presets, uint64_t seed, int depth)
      {
        if (!e.is_object ())
          throw invalid_argument ("an entry is not an object");

        if (depth > 8)
          throw invalid_argument ("presets refer to each other in a loop");

        // A seed picks the automatic sequence, for this entry and any
        // preset it names.
        //
        if (auto i = e.find ("seed"); i != e.end ())
        {
          if (!i->is_number_integer ())
            throw invalid_argument ("'seed' is not an integer");

          seed = i->get<uint64_t> ();

          if (!e.contains ("preset"))
          {
            r.shots = automatic_script (seed).shots;

            if (r.style.empty ())
              r.style = "auto";
          }
        }

        if (auto i = e.find ("preset"); i != e.end ())
        {
          if (!i->is_string ())
            throw invalid_argument ("'preset' is not a string");

          const string n (i->get<string> ());

          if (n == "auto")
            r = automatic_script (seed);
          else
          {
            auto p (presets.is_object () ? presets.find (n) : presets.end ());

            if (!presets.is_object () || p == presets.end ())
              throw invalid_argument ("unknown preset '" + n + "'");

            apply (r, *p, presets, seed, depth + 1);
          }

          r.style = n;
        }

        if (e.contains ("scale") || e.contains ("height_scale"))
        {
          const float scale (clamp (number (e, "scale", 1.0f), 0.2f, 5.0f));
          const float height (clamp (number (e, "height_scale", 1.0f), 0.2f, 5.0f));

          for (shot& s: r.shots)
          {
            s.distance *= scale;
            s.travel *= scale;
            s.height *= height;
            s.rise *= height;
          }
        }

        if (auto i = e.find ("shots"); i != e.end ())
        {
          if (!i->is_array () || i->empty ())
            throw invalid_argument ("'shots' is not a non-empty array");

          r.shots.clear ();

          for (const json& s: *i)
            r.shots.push_back (parse_shot (s));

          if (r.style.empty ())
            r.style = "scripted";
        }

        if (auto i = e.find ("focus"); i != e.end ())
        {
          if (!i->is_array () || i->size () != 2 ||
              !(*i)[0].is_number () || !(*i)[1].is_number ())
            throw invalid_argument ("'focus' is not two numbers");

          r.focus = focus_point {clamp ((*i)[0].get<float> (), 0.05f, 0.95f),
                                 clamp ((*i)[1].get<float> (), 0.05f, 0.95f)};
        }

        if (auto i = e.find ("anchor"); i != e.end ())
        {
          const json& a (*i);
          auto o (a.is_object () ? a.find ("origin") : a.end ());

          if (!a.is_object () || o == a.end () || !o->is_array () ||
              o->size () != 3)
            throw invalid_argument ("'anchor' needs an origin of three numbers");

          spawn sp {};

          for (size_t k (0); k != 3; ++k)
          {
            if (!(*o)[k].is_number ())
              throw invalid_argument ("the anchor's origin is not numbers");

            sp.origin[k] = (*o)[k].get<float> ();
          }

          sp.yaw = number (a, "yaw", 0.0f);
          r.anchor = sp;
        }

        r.sway = clamp (number (e, "sway", r.sway), 0.0f, 4.0f);
      }
    }

    scene_script
    automatic_script (uint64_t seed, float scale, float height_scale)
    {
      generator g {seed ^ 0x5A17E5CE11Eull};
      scene_script r;
      r.style = "auto";
      r.sway = 0.6f;

      // Slow and close to eye level, the way a film crew would cover the
      // survivors holding out: every move small, every shot long, the
      // subject always in frame. Heights stay near standing eye level so
      // the camera reads as a person in the room, not a drone.
      //
      shot open; // A wide, slow circle to set the scene.
      open.kind = shot_kind::orbit;
      open.duration = g.between (20.0f, 24.0f);
      open.blend = 5.0f;
      open.yaw = g.between (-25.0f, 25.0f);
      open.arc = g.side () * g.between (18.0f, 28.0f);
      open.distance = g.between (330.0f, 380.0f);
      open.height = g.between (88.0f, 110.0f);
      open.aim_height = 44.0f;
      open.fov = 55.0f;
      open.fov_end = 52.0f;

      vector<shot> rest;

      {
        shot s; // A slow push in on the fight.
        s.kind = shot_kind::push;
        s.duration = g.between (16.0f, 19.0f);
        s.blend = 5.0f;
        s.yaw = g.between (-20.0f, 20.0f);
        s.distance = g.between (290.0f, 320.0f);
        s.travel = g.between (60.0f, 90.0f);
        s.height = g.between (58.0f, 70.0f);
        s.aim_height = 50.0f;
        s.fov = 52.0f;
        s.fov_end = 46.0f;
        rest.push_back (s);
      }

      {
        shot s; // A gentle rise from the floor.
        s.kind = shot_kind::crane;
        s.duration = g.between (17.0f, 20.0f);
        s.blend = 5.0f;
        s.yaw = g.side () * g.between (30.0f, 55.0f);
        s.distance = g.between (270.0f, 310.0f);
        s.height = g.between (38.0f, 50.0f);
        s.rise = g.between (55.0f, 80.0f);
        s.aim_height = 46.0f;
        s.fov = 54.0f;
        s.fov_end = 54.0f;
        rest.push_back (s);
      }

      {
        shot s; // A slow slide past.
        s.kind = shot_kind::dolly;
        s.duration = g.between (17.0f, 20.0f);
        s.blend = 5.0f;
        s.yaw = g.side () * g.between (10.0f, 30.0f);
        s.distance = g.between (280.0f, 320.0f);
        s.travel = g.side () * g.between (110.0f, 160.0f);
        s.height = g.between (66.0f, 80.0f);
        s.aim_height = 48.0f;
        s.fov = 52.0f;
        s.fov_end = 52.0f;
        rest.push_back (s);
      }

      {
        shot s; // Over the survivors' shoulders, the horde coming on.
        s.kind = shot_kind::hold;
        s.duration = g.between (13.0f, 16.0f);
        s.blend = 5.0f;
        s.yaw = 180.0f + g.side () * g.between (20.0f, 35.0f);
        s.distance = g.between (200.0f, 230.0f);
        s.height = g.between (72.0f, 80.0f);
        s.aim_height = 58.0f;
        s.fov = 56.0f;
        s.fov_end = 53.0f;
        rest.push_back (s);
      }

      {
        shot s; // A close, slow circle.
        s.kind = shot_kind::orbit;
        s.duration = g.between (18.0f, 22.0f);
        s.blend = 5.0f;
        s.yaw = g.between (-50.0f, 50.0f);
        s.arc = g.side () * g.between (20.0f, 32.0f);
        s.distance = g.between (240.0f, 270.0f);
        s.height = g.between (62.0f, 72.0f);
        s.aim_height = 48.0f;
        s.fov = 48.0f;
        s.fov_end = 50.0f;
        rest.push_back (s);
      }

      // Shuffle the rest (Fisher-Yates) so maps differ in order too.
      //
      for (size_t i (rest.size ()); i > 1; --i)
        swap (rest[i - 1], rest[g.next () % i]);

      r.shots.push_back (open);
      r.shots.insert (r.shots.end (), rest.begin (), rest.end ());

      for (shot& s: r.shots)
      {
        s.distance *= scale;
        s.travel *= scale;
        s.height *= height_scale;
        s.rise *= height_scale;
      }

      return r;
    }

    scene_script
    script_for (string_view text, string_view map, uint64_t seed)
    {
      seed ^= hash_name (map);
      scene_script r (automatic_script (seed));

      if (text.empty ())
        return r;

      json j;

      try
      {
        j = json::parse (text.begin (), text.end (), nullptr, true, true);
      }
      catch (const json::exception& e)
      {
        throw invalid_argument (string ("the scene file is not JSON: ") + e.what ());
      }

      if (!j.is_object ())
        throw invalid_argument ("the scene file is not an object");

      const json presets (j.value ("presets", json::object ()));

      if (auto i = j.find ("focus"); i != j.end ())
      {
        json f {{"focus", *i}};
        apply (r, f, presets, seed, 0);
      }

      if (auto i = j.find ("default"); i != j.end ())
        apply (r, *i, presets, seed, 0);

      if (auto m = j.find ("maps"); m != j.end ())
      {
        if (!m->is_object ())
          throw invalid_argument ("'maps' is not an object");

        for (auto e (m->begin ()); e != m->end (); ++e)
        {
          const string& k (e.key ());

          if (k.size () == map.size () &&
              equal (k.begin (), k.end (), map.begin (), map.end (),
                     [] (char a, char b)
                     {
                       auto l = [] (char c)
                       {
                         return c >= 'A' && c <= 'Z' ? static_cast<char> (c - 'A' + 'a') : c;
                       };
                       return l (a) == l (b);
                     }))
          {
            apply (r, e.value (), presets, seed, 0);
            break;
          }
        }
      }

      if (r.shots.empty ())
        r = automatic_script (seed);

      return r;
    }

    director::pose
    director::evaluate (const shot& s, float yaw, float reach, float t) const noexcept
    {
      const float e (smooth (t));
      const vec3& o (subject_.origin);

      float around (subject_.yaw + yaw);
      float distance (s.distance * reach);
      float height (s.height);
      float side (0.0f);

      switch (s.kind)
      {
        case shot_kind::orbit: around += s.arc * (e - 0.5f); break;
        case shot_kind::dolly: side = s.travel * reach * (e - 0.5f); break;
        case shot_kind::crane: height += s.rise * e; break;
        case shot_kind::push:
          distance = max (distance - s.travel * reach * e, distance * 0.35f);
          break;
        case shot_kind::hold: break;
      }

      const vec3 out (flat (around));
      const vec3 left (flat (around + 90.0f));

      pose p;

      for (size_t i (0); i != 3; ++i)
      {
        p.eye[i] = o[i] + out[i] * distance + left[i] * side;
        p.aim[i] = o[i];
      }

      p.eye[2] += height;
      p.aim[2] += s.aim_height;
      p.fov = s.fov + (s.fov_end - s.fov) * e;

      return p;
    }

    director::pose
    director::evaluate (const placed& p, float t) const noexcept
    {
      return evaluate (p.s, p.yaw, p.reach, t);
    }

    void
    director::plan (const spawn& subject, scene_script script, const clearance& clear)
    {
      subject_ = script.anchor ? *script.anchor : subject;
      script_ = move (script);
      shots_.clear ();
      length_ = 0;
      pull_ = 1.0f;

      if (script_.shots.empty ())
        script_.shots = automatic_script (0).shots;

      for (const shot& s: script_.shots)
      {
        placed best {s, s.yaw, 1.0f};

        if (clear)
        {
          // Turn the shot around the subject until it is clear of the
          // world, keeping the turn small: the script's yaw is what it
          // asked for. A shot nowhere clear is pulled in instead.
          //
          float best_score (-1.0f);

          for (float turn: {0.0f, 40.0f, -40.0f, 80.0f, -80.0f, 120.0f, -120.0f, 180.0f})
          {
            float score (1.0f);

            for (float t: {0.0f, 0.5f, 1.0f})
            {
              const pose q (evaluate (s, s.yaw + turn, 1.0f, t));
              score = min (score, clear (q.aim, q.eye));
            }

            // Prefer the asked-for yaw: a turn has to be clearly better.
            //
            score -= abs (turn) / 180.0f * 0.08f;

            if (score > best_score)
            {
              best_score = score;
              best.yaw = s.yaw + turn;
            }

            if (score >= 0.9f)
              break;
          }

          best.reach = clamp (best_score - 0.04f, 0.45f, 1.0f);
        }

        shots_.push_back (best);
        length_ += s.duration;
      }
    }

    size_t
    director::shot_index (double seconds) const noexcept
    {
      if (shots_.empty () || length_ <= 0)
        return 0;

      double t (fmod (seconds, length_));

      if (t < 0)
        t += length_;

      for (size_t i (0); i != shots_.size (); ++i)
      {
        if (t < shots_[i].s.duration)
          return i;

        t -= shots_[i].s.duration;
      }

      return shots_.size () - 1;
    }

    view
    director::at (double seconds, float aspect, const clearance* clear) const
    {
      if (shots_.empty () || length_ <= 0)
        return frame (subject_, default_framing, aspect, seconds);

      double t (fmod (seconds, length_));

      if (t < 0)
        t += length_;

      size_t i (0);

      for (; i + 1 < shots_.size () && t >= shots_[i].s.duration; ++i)
        t -= shots_[i].s.duration;

      const placed& cur (shots_[i]);
      const placed& next (shots_[(i + 1) % shots_.size ()]);

      // The shot runs over its duration less the blend; during the blend
      // the camera turns from where it ended into where the next starts,
      // so the loop has no cut, the last shot turning into the first.
      //
      const float d (cur.s.duration);
      const float b (min (cur.s.blend, d * 0.5f));
      const float run (d - b);
      const float local (static_cast<float> (t));

      pose p (evaluate (cur, clamp (local / max (run, 0.001f), 0.0f, 1.0f)));

      if (b > 0.0f && local > run)
      {
        const float w (smooth ((local - run) / b));
        const pose q (evaluate (next, 0.0f));

        p.eye = lerp3 (p.eye, q.eye, w);
        p.aim = lerp3 (p.aim, q.aim, w);
        p.fov += (q.fov - p.fov) * w;
      }

      // Keep it out of walls: pull in toward what it looks at, easing so a
      // pillar crossing the shot doesn't make it jump.
      //
      if (clear != nullptr && *clear)
      {
        const float f ((*clear) (p.aim, p.eye));
        // Never closer than a couple of paces: inside that the subject
        // fills the screen and the shot is lost anyway.
        //
        const float span (static_cast<float> (hypot (p.eye[0] - p.aim[0], hypot (p.eye[1] - p.aim[1], p.eye[2] - p.aim[2]))));
        const float nearest (span > 1.0f ? min (1.0f, 110.0f / span) : 1.0f);
        const float target (f >= 0.999f ? 1.0f : max (f - 0.04f, nearest));

        // Pull in quickly but not in one jump, let go slowly.
        //
        pull_ += (target - pull_) * (target < pull_ ? 0.35f : 0.02f);

        p.eye = lerp3 (p.aim, p.eye, pull_);
      }

      vec3 angles (direction_to_angles (
        vec3 {p.aim[0] - p.eye[0], p.aim[1] - p.eye[1], p.aim[2] - p.eye[2]}));

      // Put the subject on the focus point rather than in the middle: look
      // off to the side and up or down by the angles at which the focus
      // point is off the middle of the view. Turning left (yaw up) moves
      // the subject right, looking up (pitch down) moves it down.
      //
      const fov_tangents ft (tangents (p.fov, aspect));
      const focus_point& f (script_.focus);

      angles[1] += static_cast<float> (atan ((2.0 * f.x - 1.0) * ft.x) / radians_per_degree);
      angles[0] -= static_cast<float> (atan ((2.0 * f.y - 1.0) * ft.y) / radians_per_degree);

      const drift dr (sway (seconds));

      angles[0] += dr.pitch * script_.sway;
      angles[1] += dr.yaw * script_.sway;

      return view {p.eye, angles_to_axis (angles), ft};
    }
  }
}
