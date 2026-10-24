#include "STDInclude.hpp"

#include "Camera.hpp"

#include <cmath>   // sin(), cos(), tan(), atan2(), hypot()
#include <numbers> // numbers::pi

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      constexpr double radians_per_degree (numbers::pi / 180.0);

      // Return the unit vector along a yaw, flat on the floor.
      //
      vec3
      yaw_vector (float yaw) noexcept
      {
        const double y (yaw * radians_per_degree);

        return vec3 {static_cast<float> (cos (y)),
                     static_cast<float> (sin (y)),
                     0.0f};
      }
    }

    axis3
    angles_to_axis (const vec3& a) noexcept
    {
      // This is the engine's AngleVectors() with the right vector turned
      // into the left one, as AnglesToAxis() does.
      //
      const double p (a[0] * radians_per_degree);
      const double y (a[1] * radians_per_degree);
      const double r (a[2] * radians_per_degree);

      const double sp (sin (p)), cp (cos (p));
      const double sy (sin (y)), cy (cos (y));
      const double sr (sin (r)), cr (cos (r));

      auto f = [] (double v) {return static_cast<float> (v);};

      return axis3 {
        vec3 {f (cp * cy), f (cp * sy), f (-sp)},
        vec3 {f (sr * sp * cy - cr * sy),
              f (sr * sp * sy + cr * cy),
              f (sr * cp)},
        vec3 {f (cr * sp * cy + sr * sy),
              f (cr * sp * sy - sr * cy),
              f (cr * cp)}};
    }

    vec3
    direction_to_angles (const vec3& d) noexcept
    {
      const double h (hypot (static_cast<double> (d[0]),
                             static_cast<double> (d[1])));

      const double yaw (atan2 (d[1], d[0]) / radians_per_degree);
      const double pitch (-atan2 (d[2], h) / radians_per_degree);

      return vec3 {static_cast<float> (pitch),
                   static_cast<float> (yaw),
                   0.0f};
    }

    fov_tangents
    tangents (float fov, float aspect) noexcept
    {
      // The tangent of the 4:3 horizontal half angle, scaled down to the
      // vertical one by the 4:3 aspect.
      //
      const double y (tan (fov * 0.5 * radians_per_degree) * 0.75);

      return fov_tangents {static_cast<float> (y * aspect),
                           static_cast<float> (y)};
    }

    drift
    sway (double t) noexcept
    {
      return drift {
        static_cast<float> (0.35 * sin (t * 0.23) + 0.15 * sin (t * 0.61)),
        static_cast<float> (0.60 * sin (t * 0.17) + 0.20 * sin (t * 0.43))};
    }

    view
    frame (const spawn& s,
           const framing& f,
           float aspect,
           double seconds) noexcept
    {
      // The character's frame on the floor: where it faces and its left.
      // Its right is the negated left.
      //
      const vec3 fwd (yaw_vector (s.yaw));
      const vec3 left (yaw_vector (s.yaw + 90.0f));

      const vec3& o (s.origin);

      // Place the camera in front of the character and to its left, then
      // aim at a point on the character moved to its right.
      //
      vec3 eye, aim;

      for (size_t i (0); i != 3; ++i)
      {
        eye[i] = o[i] + fwd[i] * f.distance + left[i] * f.side;
        aim[i] = o[i] - left[i] * f.shift;
      }

      eye[2] += f.eye_height;
      aim[2] += f.aim_height;

      vec3 angles (direction_to_angles (
        vec3 {aim[0] - eye[0], aim[1] - eye[1], aim[2] - eye[2]}));

      // Let it drift.
      //
      const drift d (sway (seconds));

      angles[0] += d.pitch;
      angles[1] += d.yaw;

      return view {eye, angles_to_axis (angles), tangents (f.fov, aspect)};
    }

    view rebase_view (const view& camera, const spawn& world_frame,
                      const spawn& actor_frame) noexcept
    {
      const double angle ((actor_frame.yaw - world_frame.yaw) * radians_per_degree);
      const float c (static_cast<float> (cos (angle))), s (static_cast<float> (sin (angle)));
      const auto rotate = [c, s] (const vec3& p)
      {
        return vec3 {c * p[0] - s * p[1], s * p[0] + c * p[1], p[2]};
      };
      view result (camera);
      vec3 relative;
      for (size_t i = 0; i < 3; ++i) relative[i] = camera.origin[i] - world_frame.origin[i];
      result.origin = rotate (relative);
      for (size_t i = 0; i < 3; ++i)
      {
        result.origin[i] += actor_frame.origin[i];
        result.axis[i] = rotate (camera.axis[i]);
      }
      return result;
    }
  }
}
