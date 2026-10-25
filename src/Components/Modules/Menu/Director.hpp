#pragma once

#include "Types.hpp"

#include "Stage.hpp"
#include "Camera.hpp"

#include <functional>

namespace Components::Menu
{
  namespace BackdropData
  {
    // The backdrop's camera: the crew standing together posing (see
    // Actors::SetLineup), filmed from the front at chest height, gliding
    // slowly from one side to the other and back.
    //
    // The same for every map, fitted to it when the map is shown: the crew
    // is turned to face wherever the camera has a clear view of all four
    // (no wall, prop or tree in between, floor under the camera, inside the
    // world), and comes in closer only when no direction has room.
    //
    // Like the rest of BackdropData this knows nothing about the engine:
    // the collision it fits the shot against is a function it is given.

    // Where on the screen the crew is kept, in fractions of the screen from
    // its top left corner: the gap the menus leave between their left
    // column and the panels on the right.
    //
    struct focus_point
    {
      float x;
      float y;
    };

    // How the crew is filmed.
    //
    struct scene_script
    {
      focus_point focus {0.50f, 0.56f};
      float distance {300.0f};   // Of the camera in front of the crew.
      float height {150.0f};     // Of the camera, above the crew's floor.
      float aim_height {46.0f};  // Of the point looked at (chest height).
      float glide {70.0f};       // How far it glides to either side.
      float period {38.0f};      // Seconds for one glide there and back.
      float fov {50.0f};         // As cg_fov, before fitting the crew in.
      float sway {0.25f};        // Of the handheld drift, 0 for none.

      // The crew's half-width, the outer two included, and how much of the
      // screen's half-width either side of the focus point it may take: the
      // view widens until it fits.
      //
      float spread {100.0f};
      float fit {0.40f};
    };

    // Return the line-up for a seed: the same shot with a slightly
    // different glide, so maps don't all move in step.
    //
    scene_script
    lineup_script (uint64_t seed);

    // The fraction of the segment that is clear of the world (1 if all of
    // it, 0 if it starts in solid).
    //
    using clearance = std::function<float (const vec3& from, const vec3& to)>;

    class director
    {
    public:
      // The rendered world's bounds, which the camera never leaves (it
      // would see the void outside, and on maps without collision nothing
      // else tells it where the walls are). Set or cleared before plan ().
      //
      void
      bound (const vec3& lo, const vec3& hi) noexcept;

      void
      unbound () noexcept {bounded_ = false;}

      // Fit the shot to the map around the crew's spawn. Without a
      // clearance the shot is taken as it is.
      //
      void
      plan (const spawn& subject, scene_script script, const clearance& clear);

      // Where the crew stands and the way it faces, as plan () turned it.
      //
      const spawn&
      subject () const noexcept {return subject_;}

      // How much room plan () found, 1 for all it wanted: the crew stands
      // closer together in tight rooms.
      //
      float
      room () const noexcept {return room_;}

      // Where the crew actually stands, as drawn: the shot is framed on it
      // rather than on the spawn, which uneven floors move them away from.
      //
      void
      track (const vec3& center) const noexcept {target_ = center;}

      // Return the view seconds into the scene. Any time is valid: the
      // glide repeats. Successive calls should move forward in time, since
      // the camera's easing follows the time between them; a time earlier
      // than the last starts the easing over. With a clearance the camera
      // is also kept out of walls frame by frame.
      //
      view
      at (double seconds, float aspect, const clearance* clear = nullptr) const;

    private:
      struct pose
      {
        vec3 eye;
        vec3 aim;
      };

      // The camera at a point of the glide (t from 0 to 1) around a crew
      // standing at o, turned by turn degrees from the subject's facing,
      // with its distances scaled by scale, and at a height.
      //
      pose
      place (const vec3& o, float turn, float scale, float t, float height) const noexcept;

      pose
      place (const vec3& o, float turn, float scale, float t) const noexcept
      {
        return place (o, turn, scale, t, height_);
      }

      struct fit
      {
        float score;
        float turn;
        float scale;
      };

      // The best turn and scale for the shot at a height.
      //
      fit
      search (float height, const clearance&) const;

      bool
      inside (const vec3&) const noexcept;

      spawn subject_ {};
      scene_script script_;
      float room_ {1.0f};
      float height_ {150.0f};    // As plan () found room for it.
      bool planned_ {false};

      vec3 lo_ {}, hi_ {};
      bool bounded_ {false};

      // What the caller last said and where the easing has got to.
      //
      mutable optional<vec3> target_;

      mutable double last_ {-1.0};
      mutable optional<vec3> crew_;
      mutable float pull_ {1.0f};
      mutable float fov_ {0.0f};
    };
  }
}
