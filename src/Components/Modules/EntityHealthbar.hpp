#pragma once

#include "Command.hpp"
#include "Dvar.hpp"

namespace Components
{
	class EntityHealthbar : public Component
	{
	public:
		EntityHealthbar();

	private:
		static Dvar::Var cg_drawHealthbars;
		static Dvar::Var cg_healthbarMaxDist;
		static Dvar::Var cg_healthbarAimWidth;
		static Dvar::Var cg_healthbarAimAngle;

		static void Scr_SetModelHealthbar(Game::scr_entref_t entref);
		static void SendChanges();
		static void ReplayToNewClients();
		static bool OnHealthbarCommand(const Command::Params* params);
		static void Draw();
	};
}
