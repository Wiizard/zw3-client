#pragma once

#include "BothGames.hpp"
#include "Client.hpp"
#include "Common.hpp"
#include "Database.hpp"
#include "FileSystem.hpp"
#include "Functions.hpp"
#include "Dvars.hpp"
#include "PlayerMovement.hpp"
#include "Script.hpp"
#include "Server.hpp"
#include "System.hpp"
#include "Zone.hpp"

namespace Game
{
	typedef unsigned int(*G_GetWeaponIndexForName_t)(const char* name);
	extern G_GetWeaponIndexForName_t G_GetWeaponIndexForName;

	typedef gentity_s*(*G_Spawn_t)();
	extern G_Spawn_t G_Spawn;

	typedef gentity_s*(*G_TempEntity_t)(const float* origin, int event);
	extern G_TempEntity_t G_TempEntity;

	typedef void(*G_AddEvent_t)(gentity_s* ent, int event, unsigned int eventParm);
	extern G_AddEvent_t G_AddEvent;

	typedef void(*G_FreeEntity_t)(gentity_s* ed);
	extern G_FreeEntity_t G_FreeEntity;

	typedef void(*G_SpawnItem_t)(gentity_s* ent, int item);
	extern G_SpawnItem_t G_SpawnItem;

	void G_GetItemClassname(int item, gentity_s* ent);

	clientState_s* G_GetClientState(int clientNum);

	extern gentity_s* g_entities;
	extern bool* g_quitRequested;

	extern NetField* clientStateFields;
	extern int clientStateFieldsCount;
	extern WinVars_t* g_wv;

	bool Initialize();

	bool IsInitialized();

	std::uintptr_t GetModuleBase();
	std::uintptr_t GetSlide();

	template <typename T>
	T BindFunction(std::uintptr_t idbAddress)
	{
		return reinterpret_cast<T>(Utils::Hook::Rebase(idbAddress));
	}
}
