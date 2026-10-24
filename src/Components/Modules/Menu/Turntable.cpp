#include "STDInclude.hpp"

#include "Turntable.hpp"

#include <cmath> // exp(), remainder()

using namespace std;

namespace Components::Menu
{
  namespace BackdropData
  {
    namespace
    {
      // How fast it turns: the fraction of the way left that it covers in
      // a second is 1 - e^-rate.
      //
      constexpr float rate (10.0f);
    }

    void turntable::
    turn (float d) noexcept
    {
      target_ -= d;
    }

    void turntable::
    reset () noexcept
    {
      target_ = yaw_ - remainder (yaw_, 360.0f);
    }

    float turntable::
    advance (float s) noexcept
    {
      yaw_ += (target_ - yaw_) * (1 - exp (-rate * s));
      return yaw_;
    }
  }
}
