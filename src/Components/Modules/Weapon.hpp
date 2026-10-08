#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Weapon : public Component
	{
	public:
		static constexpr unsigned int BASEGAME_WEAPON_LIMIT = 1400;
		static constexpr unsigned int WEAPON_LIMIT = 2400;

		Weapon();

		static bool IsLimitRaised();

		static Game::WeaponFullDef* ParseWeaponFile(const char* name);

	private:
		static bool MoveLimitArrays();
		static void* CG_ClearCg_Hook(void* dest, int value, std::size_t size);
		static bool RebuildAmmoCounts();
		static bool RaiseLimit();
		static int BG_GetMaxPickupableAmmo_Hk(Game::playerState_s* ps, unsigned int weapon);
		static int BG_GetTotalAmmoReserve_Hk(const Game::playerState_s* ps, unsigned int weapon);

		static Dvar::Var cg_recoilMultiplier;
		static Dvar::Var bg_disableDoubleTaps;

		static void BG_WeaponFireRecoil_Stub(void* ps, float* recoilSpeed, float* kickAVel, unsigned int* holdrand, int hand);
		static void PM_Weapon_Stub(Game::pmove_s* pm, void* pml);
		static void* LoadNoneWeaponHook();
		static int WeaponEntCanBeGrabbed_Stub(const Game::entityState_s* es, const Game::playerState_s* ps, int touched, unsigned int weapon);

		static void PlayerCmd_InitialWeaponRaise(Game::scr_entref_t entref);
		static void PlayerCmd_FreezeControlsAllowLook(Game::scr_entref_t entref);
		static void AddScriptMethods();

		static Game::WeaponCompleteDef* LoadWeaponCompleteDef(const char* name);

		static unsigned int BG_GetWeaponIndexForName_Hook(const char* name, void(*registerCallback)(unsigned int));
	};
}
