#pragma once

#include "Types.hpp"



namespace Components::Menu
{
  namespace BackdropData
  {
    // The character standing in the backdrop: which models it is put
    // together from and which animation it plays.
    //
    // A multiplayer character is a body with a head attached and, for a
    // soldier, a weapon in its right hand. Which bodies and heads exist
    // depends on the map: its zone carries the two teams' characters (or
    // the team zones that the zone module adds for it do), and nothing
    // lists them other than the names of the loaded models. So the cast is
    // chosen from those names, the way the map's own character scripts
    // pair them: by faction words shared between body and head.
    //
    // The animation is the first one of a list of preferences that is
    // loaded. The ambient ones of the campaign come first and are only
    // there if a zone the game loaded carries them; the multiplayer idle is
    // in common_mp and so always is.

    // An animation the character may play, and whether it holds a weapon
    // while it does. A soldier at ease does; a smoker doesn't.
    //
    struct pose
    {
      const char* anim;
      bool armed;
    };

    // Return the animations in order of preference.
    //
    span<const pose>
    poses () noexcept;

    // The cast. Everything but the body may be absent: a character without
    // a head looks broken but still frames the scene, and one without an
    // animation isn't shown at all (see mod/backdrop.cxx).
    //
    struct cast
    {
      string body;
      optional<string> head;
      optional<string> weapon;  // Only for an armed pose.
      optional<pose> animation;
    };

    // Return the cast for the loaded model and animation names (in any
    // order), or nullopt if there is no multiplayer body among the models.
    //
    // The body is the one seed picks among the bodies sorted by name, the
    // head the one sharing the most faction words with the body (on a tie,
    // the first by name), the weapon the first of a list of rifles that is
    // loaded, and the animation the first pose loaded (see poses()).
    //
    optional<cast>
    choose_cast (span<const string> models,
                 span<const string> anims,
                 uint64_t seed);
  }
}
