#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Bullet : public Component
	{
	public:
		Bullet();

	private:
		static Dvar::Var bg_surfacePenetration;
		static Game::dvar_t* bg_bulletRange;

		static float BG_GetSurfacePenetrationDepth_Hk(const Game::WeaponDef* weapDef, int surfaceType);
		static void BG_srand_Hk(unsigned int* pHoldrand);
	};
}
