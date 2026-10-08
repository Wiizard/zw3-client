#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Lean : public Component
	{
	public:
		Lean();

		static Dvar::Var bg_lean;

	private:
		static bool isLeaningLeft;
		static bool isLeaningRight;

		static void PM_UpdateLean_Stub(Game::playerState_s* ps, float msec, Game::usercmd_s* cmd, void(*capsuleTrace)(Game::trace_t*, const float*, const float*, const Game::Bounds*, int, int));
		static bool IsBindingHeld(int binding);
		static void ApplyLeanFlags(Game::usercmd_s* cmd);
	};
}
