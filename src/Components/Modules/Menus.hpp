#pragma once

#include <atomic>

#undef LoadMenuByName_Recursive

namespace Components
{
	class Menus : public Component
	{
	public:
		Menus();

		void preDestroy() override;

		static void Add(const std::string& menu);

		static std::vector<Game::menuDef_t*> LoadMenuByName_Recursive(const std::string& menu);

		static bool IsMenuVisible(Game::UiContext* dc, Game::menuDef_t* menu);
		static Game::menuDef_t* FindDiskMenu(const std::string& name);
		static void OpenLoadingScreen();

		static void RemoveMenuFromContext(Game::UiContext* dc, Game::menuDef_t* menuToRemove);

		static Game::XAssetHeader MenuFindHook(Game::XAssetType type, const std::string& filename);
		static Game::XAssetHeader MenuListFindHook(Game::XAssetType type, const std::string& filename);

		struct NewsItem
		{
			std::string Title;
			std::string Body;
			std::string ActionType;
			std::string ActionTarget;
			std::vector<std::string> ActionCommands;
			std::string ImageUrl;
			std::string ImageCachePath;
			std::string ImageMaterial;
			Game::Material* ImageMaterialPtr = nullptr;
			int Duration = 3000;
		};

		static Dvar::Var UINewsIndex;
		static Dvar::Var UINewsCount;
		static Dvar::Var UINewsProgress;
		static Dvar::Var UINewsHover;
		static Dvar::Var UINewsTitle;
		static Dvar::Var UINewsBody;
		static Dvar::Var UINewsCounter;
		static Dvar::Var UINewsImage;
		static Dvar::Var UINewsHasImage;
		static Dvar::Var UINewsLoading;
		static Dvar::Var UINewsPage;

		static std::vector<NewsItem> NewsItems;
		static int NewsElapsed;
		static int LastNewsUpdate;
		static int HoldNewsUntil;
		static bool WasNewsHovered;
		static std::atomic_bool NewsFetchInProgress;

		static void ClearNews();
		static std::string HashNewsString(const std::string& input);
		static std::filesystem::path GetNewsImageCacheDir();
		static std::string GetNewsImageCacheExtension(const std::string& url, const std::string& data = {});
		static std::string GetNewsImageCachePath(const std::string& url, const std::string& data = {});
		static std::string GetNewsImageMaterialName(const std::string& url, const std::string& data);
		static std::string CacheNewsImage(const std::string& url);
		static std::string CreateNewsImageMaterial(const NewsItem& item);
		static void ApplyNewsImageMaterialToMenu(Game::Material* material);
		static void ApplyNewsImageMaterialsToMenu();
		static void FetchNews();
		static void BeginNewsFetch();
		static void ApplyNewsItem();
		static void UpdateNewsCarousel();
		static void RefreshNews([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void OpenNews(const UIScript::Token& token, const Game::uiInfo_s* info);
		static void SelectNewsSlot(int slot);
		static void NewsPrevPage(const UIScript::Token& token, const Game::uiInfo_s* info);
		static void NewsNextPage(const UIScript::Token& token, const Game::uiInfo_s* info);
		static std::string GetNewsTileTitle(int slot);
		static void ApplyNewsTileTitles();

	private:
		static std::unordered_map<std::string, Game::menuDef_t*> MenusFromDisk;
		static std::unordered_map<std::string, Game::MenuList*> MenuListsFromDisk;
		static Game::ExpressionSupportingData* SupportingData;

		static Game::UiContext* GameUiContexts[];

		static Dvar::Var PrintMenuDebug;
		static Dvar::Var UILoadingStartTime;
		static Dvar::Var UILoadingProgress;
		static Dvar::Var UILoadingVisible;

		// Those two point to the ORIGINAL reference of the menu or menu list that was overriden
		static std::unordered_map<std::string, Game::menuDef_t*> OverridenMenus;

		static std::vector<std::string> CustomIW4xMenus;

		static Utils::Memory::Allocator Allocator;

		static bool MenuAlreadyExists(const std::string& name);

		static void FreeZAllocatedMemory(const void* ptr, bool fromTheGame = false);
		static void FreeAllocatedString(const void* ptr, bool fromTheGame = false);
		static void FreeHunkAllocatedMemory(const void* ptr, bool fromTheGame = false);

		template <typename T> static T* Reallocate(const T* ptr, size_t size)
		{
			const auto newData = Allocator.allocate(size);
			std::memcpy(newData, ptr, size);

			return reinterpret_cast<T*>(newData);
		}

		static void PrepareToUnloadMenu(Game::menuDef_t* menu);
		static void AfterLoadedMenuFromDisk(Game::menuDef_t* menu);

		static Game::Statement_s* ReallocateExpressionLocally(Game::Statement_s* statement, bool andFree = false);
		static Game::StaticDvar* ReallocateStaticDvarLocally(Game::StaticDvar* dvar);
		static Game::itemDef_s* ReallocateItemLocally(Game::itemDef_s* item, bool andFree = false);
		static Game::MenuEventHandlerSet* ReallocateEventHandlerSetLocally(const Game::MenuEventHandlerSet* handlerSet, bool andFree = false);
		static Game::ItemKeyHandler* ReallocateItemKeyHandler(const Game::ItemKeyHandler* handlerSet, bool andFree = false);

		static void FreeMenuListOnly(Game::MenuList* menuList);
		static void FreeMenuOnly(Game::menuDef_t* menu);
		static void FreeExpression(Game::Statement_s* statement, bool fromTheGame = false);
		static void FreeItem(Game::itemDef_s* item, bool fromTheGame = false);
		static void FreeEventHandlerSet(Game::MenuEventHandlerSet* handlerSet, bool fromTheGame = false);
		static void FreeItemKeyHandler(Game::ItemKeyHandler* handlerSet, bool fromTheGame = false);

		static void UpdateSupportingDataContents();
		static void FreeLocalSupportingDataContents();
		static void InitializeSupportingData();

		static void UnloadMenuFromDisk(const std::string& menuName);

		static void ReloadDiskMenus(bool preserveConnect = false);

		static void LoadScriptMenu(const char* menu, bool allowNewMenus);

		static Game::script_s* LoadMenuScript(const std::string& name, const std::string& buffer);
		static int LoadMenuSource(const std::string& name, const std::string& buffer);

		static int ReserveSourceHandle();
		static bool IsValidSourceHandle(int handle);

		static Game::menuDef_t* ParseMenu(int handle);

		static void FreeScript(Game::script_s* script);
		static void FreeMenuSource(int handle);


		static void ReloadDiskMenus_OnCGameStart();
		static void ReloadDiskMenus_OnUIInitialization();

		static void CheckMenus();

		static void RemoveMenuNameFromContext(Game::UiContext* dc, const std::string& name, Game::menuDef_t* keepMenu);
		static void ForceOnlyCustomConnectMenu();

		template <typename... Args>
		static void DebugPrint(const std::string_view& fmt, Args&&... args)
		{
			if (PrintMenuDebug.get<bool>())
			{
				const std::string msg = std::vformat(fmt, std::make_format_args(args...));
				const std::string preformatted = std::format("[MENUS] {:X} {}\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), msg);
				Logger::Print(preformatted);
			}
		}

	};
}
