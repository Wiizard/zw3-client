#pragma once

#include "Command.hpp"

namespace Components
{
	class Menus : public Component
	{
	public:
		Menus();
		~Menus() override;

		static int Load(const std::string& path, bool allowNew = true, int* droppedCount = nullptr);

		static std::vector<Game::menuDef_t*> LoadMenuByName_Recursive(const std::string& menu);

		static void LoadAll();

		static void LoadIngame();

		static void Add(const std::string& path);

		static Game::menuDef_t* Find(const std::string& name);

		static Game::menuDef_t* FindDiskMenu(const std::string& name);

		static void OpenLoadingScreen();

		static void CloseLoadingScreen();

		static Utils::Memory::Allocator* GetAllocator();

		static Game::ExpressionSupportingData* GetSupportingData();

	private:
		static Utils::Memory::Allocator allocator;
		static Game::ExpressionSupportingData supportingData;

		static std::unordered_map<std::string, Game::menuDef_t*> loaded;

		static std::unordered_map<std::string, Game::menuDef_t*> overridden;

		static std::vector<std::string> custom;

		static std::vector<std::string> deferred;

		static bool isIngameLoaded;

		static Utils::Hook uiInitHook;
		static Utils::Hook cgameInitHook;
		static Utils::Hook menusOpenHooks[6];
		static Utils::Hook findForOpenHook;
		static Utils::Hook paintVisibleHook;
		static Utils::Hook levelshotOpenHook;
		static Utils::Hook closeAllHooks[17];
		static Utils::Hook closeRequestHooks[6];
		static Utils::Hook responseHooks[3];

		static bool TryReadFile(const std::string& path, std::string* contents);
		static bool Link(Game::menuDef_t* menu, bool allowNew);
		static bool AppendMenu(Game::UiContext* context, Game::menuDef_t* menu);
		static void Reset();

		static void UI_Init_Hook(int localClientNum);
		static int CL_InitCGame_Hook();

		static void PointLobbyStatesAtMainText();

		static void WatchMenuOpens();
		static void Menus_Open_Hook(Game::UiContext* context, Game::menuDef_t* menu);
		static Game::menuDef_t* Menus_OpenByName_Find_Hook(Game::UiContext* context, const char* name);
		static void Menus_CloseAll_Hook(Game::UiContext* context);
		static bool Menu_Paint_IsVisible_Hook(Game::UiContext* context, Game::menuDef_t* menu);
		static void ForceOnlyCustomConnectMenu();
		static void UpdateLoadingProgress(bool isMainThread);
		static void UI_DrawMapLevelshot_Open_Hook(Game::UiContext* context, Game::menuDef_t* menu);
		static std::uintptr_t Menus_CloseRequest_Hook(Game::UiContext* context, Game::menuDef_t* menu);
		static void Cbuf_AddText_Response_Hook(int localClientNum, const char* text);

		static void MenuDebug_f();
		static void ReportOpenMenus();

		static void LoadMenu_f(const Command::Params* params);
		static void OpenMenu_f(const Command::Params* params);
		static void ListMenus_f(const Command::Params* params);
	};
}
