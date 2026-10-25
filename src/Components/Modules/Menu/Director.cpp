#include "STDInclude.hpp"

#include "Director.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      constexpr double radians_per_degree (numbers::pi / 180.0);

      // A small, well mixed generator (splitmix64), so that a seed gives
      // the same value on every machine and every run.
      //
      float
      unit (uint64_t seed) noexcept
      {
        uint64_t z (seed + 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        return static_cast<float> (static_cast<double> (z >> 11) / 9007199254740992.0);
      }

      vec3
      flat (float yaw_degrees) noexcept
      {
        const double y (yaw_degrees * radians_per_degree);
        return vec3 {static_cast<float> (cos (y)), static_cast<float> (sin (y)), 0.0f};
      }

      float
      length (const vec3& a, const vec3& b) noexcept
      {
        return static_cast<float> (hypot (a[0] - b[0], hypot (a[1] - b[1], a[2] - b[2])));
      }

      vec3
      lerp3 (const vec3& a, const vec3& b, float t) noexcept
      {
        return vec3 {a[0] + (b[0] - a[0]) * t,
                     a[1] + (b[1] - a[1]) * t,
                     a[2] + (b[2] - a[2]) * t};
      }

      // The share of the way to its target an eased value covers in dt
      // seconds at a rate, the same whatever the frame rate.
      //
      float
      ease (float rate, float dt) noexcept
      {
        return 1.0f - static_cast<float> (exp (-rate * dt));
      }
    }

    scene_script
    lineup_script (uint64_t seed)
    {
      scene_script r;
      r.period = 34.0f + unit (seed) * 8.0f;
      r.glide = 60.0f + unit (seed ^ 0x51DEull) * 20.0f;
      return r;
    }

    void
    director::bound (const vec3& lo, const vec3& hi) noexcept
    {
      lo_ = lo;
      hi_ = hi;
      bounded_ = true;
    }

    bool
    director::inside (const vec3& p) const noexcept
    {
      if (!bounded_)
        return true;

      for (size_t i (0); i != 3; ++i)
        if (p[i] < lo_[i] || p[i] > hi_[i])
          return false;

      return true;
    }

    director::pose
    director::place (const vec3& o, float turn, float scale, float t, float height) const noexcept
    {
      const vec3 out (flat (subject_.yaw + turn));
      const vec3 left (flat (subject_.yaw + turn + 90.0f));
      const float glide (static_cast<float> (sin (2.0 * numbers::pi * t)) * script_.glide * scale);
      const float d (script_.distance * scale);

      pose p;

      for (size_t i (0); i != 3; ++i)
      {
        p.aim[i] = o[i];
        p.eye[i] = o[i] + out[i] * d + left[i] * glide;
      }

      p.aim[2] += script_.aim_height;
      p.eye[2] += height;
      return p;
    }

    void
    director::plan (const spawn& subject, scene_script script, const clearance& clear)
    {
      subject_ = subject;
      script_ = move (script);
      room_ = 1.0f;
      height_ = script_.height;
      planned_ = true;

      last_ = -1.0;
      target_.reset ();
      crew_.reset ();
      pull_ = 1.0f;
      fov_ = 0.0f;

      if (!clear)
        return;

      // The camera looks down on the crew from its full height wherever
      // there is room; under a low ceiling it comes down, as little as
      // will do, rather than film through the ceiling.
      //
      fit best {-1.0f, 0.0f, 1.0f};

      for (float drop: {0.0f, 40.0f, 80.0f, 110.0f})
      {
        const float height (max (script_.height - drop, script_.aim_height + 4.0f));
        fit f (search (height, clear));

        f.score -= drop / 110.0f * 0.06f; // Lower has to be clearly better.

        if (f.score > best.score)
        {
          best = f;
          height_ = height;
        }

        if (best.score >= 0.9f)
          break;
      }

      subject_.yaw = static_cast<float> (remainder (subject_.yaw + best.turn, 360.0));
      room_ = best.scale;
    }

    director::fit
    director::search (float height, const clearance& clear) const
    {
      // Face the crew where the camera has a clear view of all of them over
      // the whole glide: nothing between the camera and any of the four
      // (walls, props, pillars), floor under the camera rather than a drop,
      // and inside the world. Try the spawn's own facing first, then the
      // others round it, and only then come in closer.
      //
      const float turns[] {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
      const float scales[] {1.0f, 0.8f, 0.65f, 0.5f};
      const float glide_points[] {0.0f, 0.25f, 0.75f};

      float best_score (-1.0f), best_turn (0.0f), best_scale (1.0f);

      for (float scale: scales)
      {
        for (float turn: turns)
        {
          const vec3 side (flat (subject_.yaw + turn + 90.0f));
          float score (1.0f);

          for (float t: glide_points)
          {
            const pose q (place (subject_.origin, turn, scale, t, height));

            if (!inside (q.eye))
            {
              score = 0.0f;
              break;
            }

            score = min (score, clear (q.aim, q.eye));

            const vec3 below {q.eye[0], q.eye[1], q.eye[2] - height - 512.0f};

            if (clear (q.eye, below) >= 1.0f)
              score = min (score, 0.3f);

            for (float f: {-0.8f, 0.0f, 0.8f})
              for (float h: {30.0f, 60.0f})
              {
                const vec3 member {subject_.origin[0] + side[0] * script_.spread * f,
                                   subject_.origin[1] + side[1] * script_.spread * f,
                                   subject_.origin[2] + h};
                score = min (score, clear (q.eye, member));
              }

            if (score <= best_score)
              break;
          }

          // A turn or coming in closer has to be clearly better.
          //
          score -= abs (turn) / 180.0f * 0.02f + (1.0f - scale) * 0.05f;

          if (score > best_score)
          {
            best_score = score;
            best_turn = turn;
            best_scale = scale;
          }

          if (best_score >= 0.97f)
            break;
        }

        if (best_score >= 0.97f)
          break;
      }

      return fit {best_score, best_turn, best_scale};
    }

    view
    director::at (double seconds, float aspect, const clearance* clear) const
    {
      if (!planned_)
        return frame (subject_, default_framing, aspect, seconds);

      // The time since the last frame, for the easing. Starting over (or
      // going back in time) resets it.
      //
      const bool first (last_ < 0.0 || seconds < last_);
      const float dt (first ? 0.0f : static_cast<float> (min (seconds - last_, 0.1)));
      last_ = seconds;

      if (first)
      {
        crew_.reset ();
        pull_ = 1.0f;
        fov_ = 0.0f;
      }

      // Follow the crew as drawn.
      //
      if (target_)
      {
        if (!crew_)
          crew_ = target_;
        else
          *crew_ = lerp3 (*crew_, *target_, ease (6.0f, dt));
      }

      const vec3& o (crew_ ? *crew_ : subject_.origin);
      const double t (fmod (seconds, static_cast<double> (script_.period)) / script_.period);

      pose p (place (o, 0.0f, room_, static_cast<float> (t)));

      // Keep it out of walls: pull in toward the crew, quickly but not in
      // one jump, and let go slowly. Never closer than a couple of paces.
      //
      if (clear != nullptr && *clear)
      {
        const float f ((*clear) (p.aim, p.eye));
        const float span (length (p.eye, p.aim));
        const float nearest (span > 1.0f ? min (1.0f, 110.0f / span) : 1.0f);
        const float target (f >= 0.999f ? 1.0f : max (f - 0.04f, nearest));

        pull_ = first ? target : pull_ + (target - pull_) * ease (target < pull_ ? 20.0f : 1.2f, dt);
        p.eye = lerp3 (p.aim, p.eye, pull_);
      }

      if (bounded_)
        for (size_t i (0); i != 3; ++i)
          p.eye[i] = clamp (p.eye[i], lo_[i], hi_[i]);

      // Widen the view until the whole crew fits in the gap between the
      // menus. Where the camera has room this changes nothing; in tight
      // rooms it trades distance for lens, as a camera operator would.
      //
      {
        const float dist (max (length (p.eye, p.aim), 1.0f));
        // The crew stands closer together in tight rooms (Actors spaces
        // them by room ()), and the lens stops short of a fisheye.
        //
        const float spread (script_.spread * clamp (room_, 0.65f, 1.0f));
        const float need_x (spread / (dist * script_.fit));
        const float need (static_cast<float> (2.0 * atan (need_x / (0.75 * max (aspect, 0.1f))) / radians_per_degree));
        const float target (clamp (max (script_.fov, need), 20.0f, 75.0f));

        fov_ = fov_ <= 0.0f ? target : fov_ + (target - fov_) * ease (4.0f, dt);
      }

      vec3 angles (direction_to_angles (
        vec3 {p.aim[0] - p.eye[0], p.aim[1] - p.eye[1], p.aim[2] - p.eye[2]}));

      // Put the crew on the focus point rather than in the middle: look off
      // to the side and up or down by the angles at which the focus point is
      // off the middle of the view. Turning left (yaw up) moves the crew
      // right, looking up (pitch down) moves them down.
      //
      const fov_tangents ft (tangents (fov_, aspect));
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
