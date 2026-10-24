#pragma once

#include "Types.hpp"

#include "Stage.hpp"
#include "Camera.hpp"

#include <functional>

namespace Components::Menu
{
  namespace BackdropData
  {
    // The backdrop's cinematography: an endless, looping sequence of camera
    // shots around the scene's subject (the spot the survivors hold), for
    // whichever map is shown.
    //
    // Every map gets a sequence. Without a script one is made up from the
    // map's name, so that no two maps are filmed the same way, and each shot
    // is fitted to the map when it is planned: turned around the subject and
    // pulled in until the world geometry no longer blocks it. A script (see
    // lobby/backdrop_scenes.json) can choose a style for a map, move the
    // subject, or lay out its shots one by one.
    //
    // Like the rest of BackdropData this knows nothing about the engine: the
    // collision it fits shots against is a function it is given.

    // Where on the screen the subject is kept, in fractions of the screen
    // from its top left corner. The menus keep this part of the screen clear
    // (ui_mp/common/zw3_layout.inc lays them out around the same point), so
    // the two must change together.
    //
    struct focus_point
    {
      float x;
      float y;
    };

    inline constexpr focus_point default_focus {0.50f, 0.60f};

    enum class shot_kind
    {
      orbit,  // Circles the subject by arc degrees.
      dolly,  // Slides sideways past it by travel units.
      crane,  // Rises by rise units while looking at it.
      push,   // Closes in on it by travel units.
      hold    // Stays put, drifting only.
    };

    struct shot
    {
      shot_kind kind {shot_kind::orbit};
      float duration {12.0f};  // Seconds, the blend into the next included.
      float blend {3.0f};      // Seconds spent turning into the next shot.
      float yaw {0.0f};        // Where the camera stands around the subject:
                               // degrees from the way the subject faces.
      float arc {30.0f};       // Orbit: degrees swept.
      float distance {280.0f}; // From the subject, along the floor.
      float travel {0.0f};     // Dolly: sideways; push: closer.
      float height {80.0f};    // Of the camera, above the subject's floor.
      float rise {0.0f};       // Crane: height gained.
      float aim_height {45.0f};// Of the point looked at.
      float fov {60.0f};       // As cg_fov, at the start of the shot...
      float fov_end {60.0f};   // ...and at its end.
    };

    struct scene_script
    {
      vector<shot> shots;
      focus_point focus {default_focus};
      optional<spawn> anchor;  // Instead of the spawn found in the map.
      float sway {1.0f};       // Of the handheld drift, 0 for none.
      string style;            // What it was made from, for the log.
    };

    // Return the made-up sequence for a seed (see the header comment). The
    // scale multiplies its distances, the height scale its heights: tight
    // interiors want both below 1, open exteriors above.
    //
    scene_script
    automatic_script (uint64_t seed, float scale = 1.0f, float height_scale = 1.0f);

    // Return the script for a map from the scene file's text: the map's own
    // entry, else the file's default, else the automatic one. The seed picks
    // among automatic sequences where the entry doesn't fix one. An empty
    // text means there is no file.
    //
    // Throw std::invalid_argument if the text is not a valid scene file.
    //
    scene_script
    script_for (string_view text, string_view map, uint64_t seed);

    // The fraction of the segment that is clear of the world (1 if all of
    // it, 0 if it starts in solid).
    //
    using clearance = std::function<float (const vec3& from, const vec3& to)>;

    class director
    {
    public:
      // Fit the script's shots around the subject. Without a clearance the
      // shots are taken as they are.
      //
      void
      plan (const spawn& subject, scene_script script, const clearance& clear);

      bool
      planned () const noexcept {return !shots_.empty ();}

      size_t
      shot_count () const noexcept {return shots_.size ();}

      // The length of one pass through the shots, in seconds.
      //
      double
      loop_length () const noexcept {return length_;}

      const spawn&
      subject () const noexcept {return subject_;}

      // Return the view seconds into the scene. The sequence repeats, the
      // last shot turning into the first, so any time is valid. With a
      // clearance the camera is also kept out of walls frame by frame.
      //
      view
      at (double seconds, float aspect, const clearance* clear = nullptr) const;

      // Which shot is on, for the log.
      //
      size_t
      shot_index (double seconds) const noexcept;

    private:
      struct placed
      {
        shot s;
        float yaw;    // The shot's yaw, turned to where it fits.
        float reach;  // The fraction of its distances that fit.
      };

      struct pose
      {
        vec3 eye;
        vec3 aim;
        float fov;
      };

      pose
      evaluate (const placed&, float t) const noexcept;

      pose
      evaluate (const shot&, float yaw, float reach, float t) const noexcept;

      spawn subject_ {};
      scene_script script_;
      vector<placed> shots_;
      double length_ {0};
      mutable float pull_ {1.0f}; // The smoothed per-frame wall clearance.
    };
  }
}
