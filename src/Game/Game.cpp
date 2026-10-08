#include "STDInclude.hpp"

namespace Game
{
	G_GetWeaponIndexForName_t G_GetWeaponIndexForName = nullptr;
	G_Spawn_t G_Spawn = nullptr;
	G_TempEntity_t G_TempEntity = nullptr;
	G_AddEvent_t G_AddEvent = nullptr;
	G_FreeEntity_t G_FreeEntity = nullptr;
	G_SpawnItem_t G_SpawnItem = nullptr;

	gentity_s* g_entities = nullptr;
	bool* g_quitRequested = nullptr;

	NetField* clientStateFields = nullptr;
	int clientStateFieldsCount = 0;
	WinVars_t* g_wv = nullptr;

	void G_GetItemClassname(int item, gentity_s* ent)
	{
		char classname[256]{};
		std::snprintf(classname, sizeof(classname), "weapon_%s", BG_GetWeaponName(item % 1400));

		G_SetConstString(&ent->classname, classname);
		G_SetConstString(&ent->script_classname, classname);
	}

	clientState_s* G_GetClientState(int clientNum)
	{
		return reinterpret_cast<clientState_s*(*)(int)>(Utils::Hook::Rebase(0x14019CF30))(clientNum);
	}

	static void BindGame()
	{
		G_GetWeaponIndexForName = BindFunction<G_GetWeaponIndexForName_t>(0x1401884D0);
		G_Spawn = BindFunction<G_Spawn_t>(0x1401AC440);
		G_TempEntity = BindFunction<G_TempEntity_t>(0x1401AC820);
		G_AddEvent = BindFunction<G_AddEvent_t>(0x1401AA270);
		G_FreeEntity = BindFunction<G_FreeEntity_t>(0x1401AB670);
		G_SpawnItem = BindFunction<G_SpawnItem_t>(0x14016D550);

		g_entities = reinterpret_cast<gentity_s*>(Utils::Hook::Rebase(0x141869B40));
		g_quitRequested = reinterpret_cast<bool*>(Utils::Hook::Rebase(0x1467894F8));

		clientStateFields = reinterpret_cast<NetField*>(Utils::Hook::Rebase(0x140395B40));
		clientStateFieldsCount = Utils::Hook::Get<int>(0x140394ED8);
		g_wv = reinterpret_cast<WinVars_t*>(Utils::Hook::Rebase(0x14678E180));
	}

	static bool initialized = false;
	static std::uintptr_t moduleBase = 0;

	struct ImageAnchor
	{
		std::uintptr_t address;
		const char* name;
		std::size_t length;
		std::uint8_t bytes[8];
	};

	static const ImageAnchor anchors[] =
	{
		{ 0x1401E6DC0, "Cbuf_AddText",     8, { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83 } },
		{ 0x1401F38F0, "Com_Error",        8, { 0x48, 0x89, 0x54, 0x24, 0x10, 0x4C, 0x89, 0x44 } },
		{ 0x14023B220, "SV_SpawnServer",   8, { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x44, 0x89, 0x44 } },
		{ 0x1402374E0, "SV_DirectConnect", 8, { 0x40, 0x55, 0x53, 0x57, 0x41, 0x56, 0x48, 0x8D } },
		{ 0x14024D600, "Steam_Frame",      6, { 0x48, 0x83, 0xEC, 0x48, 0x48, 0x89 } },
	};

	static bool MatchesAnchor(const ImageAnchor& anchor)
	{
		const auto* live = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(anchor.address));

		return std::memcmp(live, anchor.bytes, anchor.length) == 0;
	}

	bool Initialize()
	{
		if (initialized)
		{
			return true;
		}

		if (!Utils::Hook::Bind(GAME_IMAGEBASE))
		{
			return false;
		}

		moduleBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));

		for (const auto& anchor : anchors)
		{
			if (!MatchesAnchor(anchor))
			{
				return false;
			}
		}

		BindBothGames();
		BindClient();
		BindCommon();
		BindDatabase();
		BindFileSystem();
		BindFunctions();
		BindDvars();
		BindGame();
		BindPlayerMovement();
		BindScript();
		BindServer();
		BindSystem();
		BindZone();

		initialized = true;

		return true;
	}

	bool IsInitialized()
	{
		return initialized;
	}

	std::uintptr_t GetModuleBase()
	{
		return moduleBase;
	}

	std::uintptr_t GetSlide()
	{
		return moduleBase - GAME_IMAGEBASE;
	}
}
