#pragma once

namespace Game
{
	typedef unsigned int(*BG_GetNumWeapons_t)();
	extern BG_GetNumWeapons_t BG_GetNumWeapons;

	typedef const char*(*BG_GetWeaponName_t)(unsigned int index);
	extern BG_GetWeaponName_t BG_GetWeaponName;

	typedef WeaponDef*(*BG_GetWeaponDef_t)(unsigned int weaponIndex);
	extern BG_GetWeaponDef_t BG_GetWeaponDef;

	typedef bool(*BG_IsWeaponValid_t)(const playerState_s* ps, unsigned int weaponIndex);
	extern BG_IsWeaponValid_t BG_IsWeaponValid;

	typedef unsigned int(*BG_GetViewmodelWeaponIndex_t)(const playerState_s* ps);
	extern BG_GetViewmodelWeaponIndex_t BG_GetViewmodelWeaponIndex;

	typedef PlayerEquippedWeaponState*(*BG_GetEquippedWeaponState_t)(playerState_s* ps, unsigned int weaponIndex);
	extern BG_GetEquippedWeaponState_t BG_GetEquippedWeaponState;

	typedef bool(*BG_PlayerHasWeapon_t)(const playerState_s* ps, unsigned int weaponIndex);
	extern BG_PlayerHasWeapon_t BG_PlayerHasWeapon;

	typedef WeaponCompleteDef*(*BG_GetWeaponCompleteDef_t)(unsigned int weaponIndex);
	extern BG_GetWeaponCompleteDef_t BG_GetWeaponCompleteDef;

	unsigned int BG_GetPerkCodeIndexForName(const char* perkName);

	void BindBothGames();
}
