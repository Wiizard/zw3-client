#pragma once

#include "Types.hpp"



namespace Components::Menu
{
  namespace BackdropData
  {
    // A figure's turntable: which way the figure faces about the vertical,
    // in degrees counterclockwise as seen from above, turning toward where
    // it is turned to rather than snapping there, the way a menu's choices
    // move (see layout/motion.hxx).
    //
    class turntable
    {
    public:
      // Turn it by the degrees, clockwise as seen from above for a positive
      // angle, which is to the right for a figure facing the viewer.
      //
      void
      turn (float degrees) noexcept;

      // Turn it back to facing where it started, the shortest way: whole
      // turns it was turned by stay.
      //
      void
      reset () noexcept;

      // Advance it by the seconds and return which way it faces.
      //
      float
      advance (float seconds) noexcept;

      float
      yaw () const noexcept {return yaw_;}

    private:
      float yaw_ = 0;
      float target_ = 0;
    };
  }
}
