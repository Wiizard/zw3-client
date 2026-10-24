#pragma once

#include "Types.hpp"


#include "Stage.hpp"


namespace Components::Menu
{
  namespace BackdropData
  {
    // The backdrop's camera: where it stands relative to the character and
    // what it sees, as the engine's refdef wants it.
    //
    // The engine's conventions throughout: angles are pitch, yaw, and roll
    // in degrees, pitch positive looking down; an axis is forward, left,
    // and up, in that order (AnglesToAxis).

    using vec3 = array<float, 3>;
    using axis3 = array<vec3, 3>;

    // Return the axis for angles.
    //
    axis3
    angles_to_axis (const vec3& angles) noexcept;

    // Return the angles that look along a direction, with no roll. The
    // direction need not be normalized but may not be zero.
    //
    vec3
    direction_to_angles (const vec3& d) noexcept;

    // The tangents of half the horizontal and vertical field of view, the
    // refdef's tanHalfFovX and tanHalfFovY.
    //
    struct fov_tangents
    {
      float x;
      float y;
    };

    // Return the tangents for a field of view given the way cg_fov gives
    // it: horizontal degrees on a 4:3 screen. A wider screen sees more to
    // the sides rather than less above and below, which is what the game
    // does for its own view, so the vertical tangent only depends on fov
    // and the horizontal one is scaled by the aspect ratio.
    //
    // The aspect ratio is width over height and has to be positive.
    //
    fov_tangents
    tangents (float fov, float aspect) noexcept;

    // How the camera frames the character.
    //
    // The character stands at the spawn facing along its yaw and the camera
    // stands in front of it looking back, so that it is seen from the
    // front. The camera is moved to the character's left for a three-
    // quarter view, and the point it aims at is moved to the character's
    // right so that the character ends up on the right of the screen,
    // clear of the menus, which are drawn on the left.
    //
    struct framing
    {
      float distance;   // From the character, along its facing.
      float side;       // To the character's left, for a three-quarter view.
      float eye_height; // Of the camera above the floor.
      float aim_height; // Of the point aimed at above the floor.
      float shift;      // Of the point aimed at, to the character's right.
      float fov;        // As cg_fov (see tangents()).
    };

    // A standing character from just past arm's reach, cut at the knees on
    // a 16:9 screen.
    //
    inline constexpr framing default_framing {110.0f, 34.0f, 58.0f, 46.0f,
                                              30.0f, 55.0f};

    // The camera's slow drift, in degrees of pitch and yaw at a point in
    // time, which keeps a still scene from reading as a screenshot.
    //
    // A sum of two incommensurate sines per angle, so the motion doesn't
    // visibly repeat. It is zero at time zero.
    //
    struct drift
    {
      float pitch;
      float yaw;
    };

    drift
    sway (double seconds) noexcept;

    // The view: what the refdef's view part is filled from.
    //
    struct view
    {
      vec3 origin;
      axis3 axis;
      fov_tangents fov;
    };

    // Return the view for a character on a spawn, seconds into the
    // backdrop.
    //
    view
    frame (const spawn&,
           const framing&,
           float aspect,
           double seconds) noexcept;

    // Express a world camera in the actor simulation's coordinate frame.
    view rebase_view (const view&, const spawn& world_frame,
                      const spawn& actor_frame) noexcept;
  }
}
