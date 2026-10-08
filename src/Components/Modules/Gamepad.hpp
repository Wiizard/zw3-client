#pragma once

#include "Dvar.hpp"

#include "Controller/Haptic/Effect.hpp"

namespace Components
{
	class Gamepad : public Component
	{
	public:
		Gamepad();

		static Dvar::Var sv_allowAimAssist;

		static constexpr int RUMBLE_CONFIGSTRINGS_COUNT = 32;

		static void OnMouseMove(int x, int y, int dx, int dy);

		static void PlayHapticEffect(const Controller::Haptic::Effect& effect);
		static void StopHapticEffect(std::uint32_t tag);

		static void GPad_SetLowRumble(int gamePadIndex, double rumble);
		static void GPad_SetHighRumble(int gamePadIndex, double rumble);
		static void GPad_StopRumbles(int gamePadIndex);
		static void GPad_UpdateFeedbacks();

	private:
		static float lowRumble;
		static float highRumble;

		static void SubmitRumble();
	};
}
