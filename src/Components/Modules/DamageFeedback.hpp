#pragma once

#include "Command.hpp"
#include "Dvar.hpp"

namespace Components
{
	class DamageFeedback : public Component
	{
	public:
		DamageFeedback();

	private:
		static Dvar::Var cg_hitmarkerStyle;
		static Dvar::Var cg_hitmarkerScale;

		static void Scr_PlayHitmarker(Game::scr_entref_t entref);
		static bool OnHitmarkerCommand(const Command::Params* params);
		static void Draw();
	};
}
