#include "STDInclude.hpp"

namespace Game
{
	BG_GetNumWeapons_t BG_GetNumWeapons = nullptr;
	BG_GetWeaponName_t BG_GetWeaponName = nullptr;
	BG_GetWeaponDef_t BG_GetWeaponDef = nullptr;
	BG_IsWeaponValid_t BG_IsWeaponValid = nullptr;
	BG_GetViewmodelWeaponIndex_t BG_GetViewmodelWeaponIndex = nullptr;
	BG_GetEquippedWeaponState_t BG_GetEquippedWeaponState = nullptr;
	BG_PlayerHasWeapon_t BG_PlayerHasWeapon = nullptr;
	BG_GetWeaponCompleteDef_t BG_GetWeaponCompleteDef = nullptr;

	unsigned int BG_GetPerkCodeIndexForName(const char* perkName)
	{
		return reinterpret_cast<unsigned int(*)(const char*)>(Utils::Hook::Rebase(0x14008D7E0))(perkName);
	}

	void BindBothGames()
	{
		BG_GetNumWeapons = BindFunction<BG_GetNumWeapons_t>(0x14009C7F0);
		BG_GetWeaponName = BindFunction<BG_GetWeaponName_t>(0x14009CA30);
		BG_GetWeaponDef = BindFunction<BG_GetWeaponDef_t>(0x14009C8A0);
		BG_IsWeaponValid = BindFunction<BG_IsWeaponValid_t>(0x14009D8A0);
		BG_GetViewmodelWeaponIndex = BindFunction<BG_GetViewmodelWeaponIndex_t>(0x14009C870);
		BG_GetEquippedWeaponState = BindFunction<BG_GetEquippedWeaponState_t>(0x1401876A0);
		BG_PlayerHasWeapon = BindFunction<BG_PlayerHasWeapon_t>(0x14008ABA0);
		BG_GetWeaponCompleteDef = BindFunction<BG_GetWeaponCompleteDef_t>(0x14009C890);
	}
}
