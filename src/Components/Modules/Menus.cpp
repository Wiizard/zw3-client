#include "Menus.hpp"
#include "Party.hpp"
#include "Events.hpp"
// Ensure you have includes for AssetHandler, if it's a separate component.
// #include "AssetHandler.hpp" // If AssetHandler is in its own header

#define MAX_SOURCEFILES	64
#define DEFINEHASHSIZE 1024

namespace Components
{

	// NO LONGER NEEDED: decltype(&Game::DB_FindXAssetHeader) Menus::DB_FindXAssetHeader_Original = nullptr;

	// As of now it is not sure whether supporting data needs to be reallocated
	// It is a global singleton, cleared on UI_Init, which is also when we clear our menus
	// so maybe keeping a reference to it is fine actually!
	// EDIT: Okay so it needs to be allocated ONCE per ZONE, so we have to reallocate our own

#define ALLOCATED_BY_GAME true
#define ALLOCATED_BY_IW4X false

#define DUPLICATE_STRING_IF_EXISTS(obj, x) if (##obj->##x) ##obj->##x = Allocator.duplicateString(##obj->##x)
#define FREE_STRING_IF_EXISTS(obj, x, fromTheGame) if (##obj->##x) FreeAllocatedString(##obj->##x, fromTheGame)

	/// This variable dispenses us from the horror of having a text file in IWD containing the menus we want to load
	std::vector<std::string> Menus::CustomIW4xMenus;

	Dvar::Var Menus::PrintMenuDebug;

	Game::UiContext* Menus::GameUiContexts[] = {
		Game::uiContext,
		Game::cgDC // Ingame context
	};

	std::unordered_map<std::string, Game::menuDef_t*> Menus::MenusFromDisk;
	std::unordered_map<std::string, Game::MenuList*> Menus::MenuListsFromDisk;

	std::unordered_map<std::string, Game::menuDef_t*> Menus::OverridenMenus;

	Game::ExpressionSupportingData* Menus::SupportingData;

	Utils::Memory::Allocator Menus::Allocator;

	Game::KeywordHashEntry<Game::menuDef_t, 128, 3523>** menuParseKeywordHash;

	template <int HASH_COUNT, int HASH_SEED>
	static int KeywordHashKey(const char* keyword)
	{
		auto hash = 0;
		for (auto i = 0; keyword[i]; ++i)
		{
			hash += (i + HASH_SEED) * std::tolower(static_cast<unsigned char>(keyword[i]));
		}
		return (hash + (hash >> 8)) & (128 - 1);
	}

	template <typename T, int N, int M>
	static Game::KeywordHashEntry<T, N, M>* KeywordHashFind(Game::KeywordHashEntry<T, N, M>** table, const char* keyword)
	{
		auto hash = KeywordHashKey<N, M>(keyword);
		Game::KeywordHashEntry<T, N, M>* key = table[hash];
		if (key && !_stricmp(key->keyword, keyword))
		{
			return key;
		}
		return nullptr;
	}

	int Menus::ReserveSourceHandle()
	{
		// Check if a free slot is available
		auto i = 1;
		while (i < MAX_SOURCEFILES)
		{
			if (!Game::sourceFiles[i])
			{
				break;
			}

			++i;
		}

		if (i >= MAX_SOURCEFILES)
		{
			return 0;
		}

		// Reserve it, if yes
		Game::sourceFiles[i] = reinterpret_cast<Game::source_s*>(1);

		return i;
	}

	Game::script_s* Menus::LoadMenuScript(const std::string& name, const std::string& buffer)
	{
		auto* script = static_cast<Game::script_s*>(Game::GetClearedMemory(sizeof(Game::script_s) + 1 + buffer.length()));
		if (!script) return nullptr;

		strcpy_s(script->filename, sizeof(script->filename), name.data());
		script->buffer = reinterpret_cast<char*>(script + 1);

		*(script->buffer + buffer.length()) = '\0';

		script->script_p = script->buffer;
		script->lastscript_p = script->buffer;
		script->length = static_cast<int>(buffer.length());
		script->end_p = &script->buffer[buffer.length()];
		script->line = 1;
		script->lastline = 1;
		script->tokenavailable = 0;

		Game::PS_CreatePunctuationTable(script, Game::default_punctuations);
		script->punctuations = Game::default_punctuations;

		std::memcpy(script->buffer, buffer.data(), script->length + 1);

		script->length = Game::Com_Compress(script->buffer);

		return script;
	}

	int Menus::LoadMenuSource(const std::string& name, const std::string& buffer)
	{
		const auto handle = ReserveSourceHandle();
		if (!IsValidSourceHandle(handle)) return 0; // No free source slot!

		auto* script = LoadMenuScript(name, buffer);
		if (!script)
		{
			Game::sourceFiles[handle] = nullptr; // Free reserved slot
			return 0;
		}

		auto* source = static_cast<Game::source_s*>(Game::GetMemory(sizeof(Game::source_s)));
		std::memset(source, 0, sizeof(Game::source_s));

		script->next = nullptr;

		strncpy_s(source->filename, name.data(), _TRUNCATE);
		source->scriptstack = script;
		source->tokens = nullptr;
		source->defines = nullptr;
		source->indentstack = nullptr;
		source->skip = 0;
		source->definehash = static_cast<Game::define_s**>(Game::GetClearedMemory(DEFINEHASHSIZE * sizeof(Game::define_s*)));

		Game::sourceFiles[handle] = source;

		return handle;
	}

	bool Menus::IsValidSourceHandle(int handle)
	{
		return (handle > 0 && handle < MAX_SOURCEFILES && Game::sourceFiles[handle]);
	}

	Game::menuDef_t* Menus::ParseMenu(int handle)
	{
		auto* menu = Allocator.allocate<Game::menuDef_t>();
		if (!menu)
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "No more memory to allocate menu\n");
			return nullptr;
		}

		menu->items = Allocator.allocateArray<Game::itemDef_s*>(512);
		if (!menu->items)
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "No more memory to allocate menu items\n");
			Allocator.free(menu);
			return nullptr;
		}

		Game::pc_token_s token;
		if (!Game::PC_ReadTokenHandle(handle, &token) || token.string[0] != '{')
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "Invalid or unexpected syntax on menu\n");
			Allocator.free(menu->items);
			Allocator.free(menu);
			return nullptr;
		}

		while (true)
		{
			ZeroMemory(&token, sizeof(token));

			if (!Game::PC_ReadTokenHandle(handle, &token))
			{
				Game::PC_SourceError(handle, "end of file inside menu\n");
				break; // Fail
			}

			if (*token.string == '}')
			{
				break; // Success
			}

			auto* key = KeywordHashFind(menuParseKeywordHash, token.string);
			if (!key)
			{
				Game::PC_SourceError(handle, "unknown menu keyword %s", token.string);
				continue;
			}

			if (!key->func(menu, handle))
			{
				Game::PC_SourceError(handle, "couldn't parse menu keyword %s", token.string);
				break; // Fail
			}
		}

		if (!menu->window.name)
		{
			Game::PC_SourceError(handle, "menu has no name");
			Allocator.free(menu->items);
			Allocator.free(menu);
			return nullptr;
		}

		// Shrink item size now that we're done parsing
		{
			const auto newItemArray = Allocator.allocateArray<Game::itemDef_s*>(menu->itemCount);
			std::memcpy(newItemArray, menu->items, menu->itemCount * sizeof(Game::itemDef_s*));

			Allocator.free(menu->items);

			menu->items = newItemArray;
		}

		// Reallocate Menu with our allocator because these data will get freed when LargeLocal::Reset gets called!
		{
			DebugPrint("Reallocating menu {} ({:X})...", menu->window.name, (unsigned int)(menu));


			menu->window.name = Allocator.duplicateString(menu->window.name);

			for (int i = 0; i < menu->itemCount; i++)
			{
				menu->items[i] = ReallocateItemLocally(menu->items[i], true);
			}

			menu->onKey = ReallocateItemKeyHandler(menu->onKey);

			menu->onOpen = ReallocateEventHandlerSetLocally(menu->onOpen, true);
			menu->onCloseRequest = ReallocateEventHandlerSetLocally(menu->onCloseRequest, true);
			menu->onClose = ReallocateEventHandlerSetLocally(menu->onClose, true);
			menu->onESC = ReallocateEventHandlerSetLocally(menu->onESC, true);

			menu->visibleExp = ReallocateExpressionLocally(menu->visibleExp, true);
			menu->rectXExp = ReallocateExpressionLocally(menu->rectXExp, true);
			menu->rectYExp = ReallocateExpressionLocally(menu->rectYExp, true);
			menu->rectWExp = ReallocateExpressionLocally(menu->rectWExp, true);
			menu->rectHExp = ReallocateExpressionLocally(menu->rectHExp, true);
			menu->openSoundExp = ReallocateExpressionLocally(menu->openSoundExp, true);
			menu->closeSoundExp = ReallocateExpressionLocally(menu->closeSoundExp, true);

			DUPLICATE_STRING_IF_EXISTS(menu, font);
			DUPLICATE_STRING_IF_EXISTS(menu, allowedBinding);
			DUPLICATE_STRING_IF_EXISTS(menu, soundName);

			// Sometimes it requries updating even if the menu _itself_ does not have any
			// Because it might have items that did update it
			UpdateSupportingDataContents();

			if (menu->expressionData)
			{
				assert(menu->expressionData == Game::menuSupportingData);
				menu->expressionData = Menus::SupportingData;
			}
		}

		return menu;
	}

	std::vector<Game::menuDef_t*> Menus::LoadMenuByName_Recursive(const std::string& menu)
	{
		std::vector<Game::menuDef_t*> menus;
		FileSystem::File menuFile(menu);

		if (menuFile.exists())
		{
			Game::pc_token_s token;
			const auto handle = LoadMenuSource(menu, menuFile.getBuffer());

			if (IsValidSourceHandle(handle))
			{
				while (true)
				{
					ZeroMemory(&token, sizeof(token));

					if (!Game::PC_ReadTokenHandle(handle, &token) || token.string[0] == '}')
					{
						break;
					}

					if (!_stricmp(token.string, "loadmenu"))
					{
						Game::PC_ReadTokenHandle(handle, &token);

						const auto loadedMenu = LoadMenuByName_Recursive(Utils::String::VA("ui_mp\\%s.menu", token.string));

						for (const auto& loaded : loadedMenu)
						{
							menus.emplace_back(loaded);
						}
					}
					else if (!_stricmp(token.string, "menudef"))
					{
						auto* menuDef = ParseMenu(handle);
						if (menuDef)
						{
							menus.emplace_back(menuDef);
						}
					}
				}

				FreeMenuSource(handle);
			}
		}

		return menus;
	}

	// Add the RemoveMenuFromContext helper function (same as previous iteration)
	void Menus::RemoveMenuFromContext(Game::UiContext* dc, Game::menuDef_t* menuToRemove)
	{
		if (!dc || !menuToRemove) return;

		// Search for the menu in the context's main menu array
		for (int i = 0; i < dc->menuCount; ++i)
		{
			if (dc->Menus[i] == menuToRemove)
			{
				DebugPrint("Removing menu {} from UI context {:X} at index {}",
					menuToRemove->window.name, (unsigned int)dc, i);

				// Shift elements left to fill the gap
				for (int j = i; j < dc->menuCount - 1; ++j)
				{
					dc->Menus[j] = dc->Menus[j + 1];
				}

				// Clear the last element and decrement count
				dc->Menus[--dc->menuCount] = nullptr;
				// Adjust loop counter as we removed an element and shifted
				i--;
			}
		}

		// Also check and remove from the menu stack if present
		for (int i = 0; i < dc->openMenuCount; ++i)
		{
			if (dc->menuStack[i] == menuToRemove)
			{
				DebugPrint("Removing menu {} from UI context {:X} stack at index {}",
					menuToRemove->window.name, (unsigned int)dc, i);

				// Shift elements left to fill the gap
				for (int j = i; j < dc->openMenuCount - 1; ++j)
				{
					dc->menuStack[j] = dc->menuStack[j + 1];
				}

				// Clear the last element and decrement count
				dc->menuStack[--dc->openMenuCount] = nullptr;
				// Adjust loop counter as we removed an element and shifted
				i--;
			}
		}
	}


	bool Menus::MenuAlreadyExists(const std::string& name)
	{
		for (size_t i = 0; i < ARRAYSIZE(GameUiContexts); i++)
		{
			if (Game::Menus_FindByName(GameUiContexts[i], name.data()))
			{
				return true;
			}
		}

		return false;
	}

	void Menus::LoadScriptMenu(const char* menu, bool allowNewMenus)
	{
		auto menus = LoadMenuByName_Recursive(menu);

		if (menus.empty())
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "Could not load menu {}\n", menu);
			return;
		}

		if (!allowNewMenus)
		{
			// We remove every menu we loaded that is not going to override something
			for (int i = 0; i < static_cast<int>(menus.size()); i++)
			{
				const auto menuName = menus[i]->window.name;
				if (MenuAlreadyExists(menuName))
				{
					// It's an override, we keep it
				}
				else
				{
					// We are not allowed to keep this one, let's free it
					FreeMenuOnly(menus[i]);
					menus.erase(menus.begin() + i);
					i--;
				}
			}

			if (menus.empty())
			{
				return; // No overrides!
			}
		}

		// Tracking
		for (const auto& loadedMenu : menus)
		{
			// Unload previous loaded-from-disk versions of these menus, if we had any
			const std::string menuName = loadedMenu->window.name;
			if (MenusFromDisk.contains(menuName))
			{
				UnloadMenuFromDisk(menuName); // This calls PrepareToUnloadMenu which updates contexts
				MenusFromDisk.erase(menuName);
			}

			// Then mark them as loaded
			MenusFromDisk[menuName] = loadedMenu;

			AfterLoadedMenuFromDisk(loadedMenu); // This function will add/override in GameUiContexts
		}


		// Allocate new menu list
		auto* newList = Allocator.allocate<Game::MenuList>();
		if (!newList)
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "No more memory to allocate menu list {}\n", menu);
			return;
		}

		newList->menus = Allocator.allocateArray<Game::menuDef_t*>(menus.size());
		if (!newList->menus)
		{
			Components::Logger::PrintError(Game::CON_CHANNEL_UI, "No more memory to allocate menus for {}\n", menu);
			Allocator.free(newList);
			return;
		}

		newList->name = Allocator.duplicateString(menu);
		newList->menuCount = static_cast<int>(menus.size());

		// Copy new menu references
		for (unsigned int i = 0; i < menus.size(); ++i)
		{
			newList->menus[i] = menus[i];
		}

		// Tracking
		{
			const auto menuListName = newList->name;
			if (MenuListsFromDisk.contains(menuListName))
			{
				FreeMenuListOnly(MenuListsFromDisk[menuListName]);
			}

			DebugPrint("Loaded menuList {} at {:X}",
				newList->name,
				(unsigned int)newList
			);

			MenuListsFromDisk[menuListName] = newList;
		}
	}

	void Menus::FreeScript(Game::script_s* script)
	{
		if (script->punctuationtable)
		{
			Game::FreeMemory(script->punctuationtable);
		}

		Game::FreeMemory(script);
	}

	void Menus::FreeMenuSource(int handle)
	{
		if (!IsValidSourceHandle(handle)) return;

		auto* source = Game::sourceFiles[handle];

		while (source->scriptstack)
		{
			auto* script = source->scriptstack;
			source->scriptstack = source->scriptstack->next;
			FreeScript(script);
		}

		while (source->tokens)
		{
			auto* token = source->tokens;
			source->tokens = source->tokens->next;

			Game::FreeMemory(token);
			--*Game::numtokens;
		}

		for (auto i = 0; i < DEFINEHASHSIZE; ++i)
		{
			while (source->definehash[i])
			{
				auto* define = source->definehash[i];
				source->definehash[i] = source->definehash[i]->hashnext;
				Game::PC_FreeDefine(define);
			}
		}

		while (source->indentstack)
		{
			auto* indent = source->indentstack;
			source->indentstack = source->indentstack->next;
			Game::FreeMemory(indent);
		}

		if (source->definehash)
		{
			Game::FreeMemory(source->definehash);
		}

		Game::FreeMemory(source);

		Game::sourceFiles[handle] = nullptr;
	}

	void Menus::FreeItem(Game::itemDef_s* item, bool fromTheGame)
	{
		for (auto i = 0; i < item->floatExpressionCount; ++i)
		{
			FreeExpression(item->floatExpressions[i].expression, fromTheGame);
		}

		FreeEventHandlerSet(item->accept, fromTheGame);
		FreeEventHandlerSet(item->action, fromTheGame);
		FreeEventHandlerSet(item->leaveFocus, fromTheGame);
		FreeEventHandlerSet(item->mouseEnter, fromTheGame);
		FreeEventHandlerSet(item->mouseEnterText, fromTheGame);
		FreeEventHandlerSet(item->mouseExit, fromTheGame);
		FreeEventHandlerSet(item->mouseExitText, fromTheGame);
		FreeEventHandlerSet(item->onFocus, fromTheGame);

		FreeItemKeyHandler(item->onKey, fromTheGame);

		FREE_STRING_IF_EXISTS(item, dvar, fromTheGame);
		FREE_STRING_IF_EXISTS(item, dvarTest, fromTheGame);
		FREE_STRING_IF_EXISTS(item, localVar, fromTheGame);
		FREE_STRING_IF_EXISTS(item, enableDvar, fromTheGame);
		FREE_STRING_IF_EXISTS(item, text, fromTheGame);

		FREE_STRING_IF_EXISTS(item, window.name, fromTheGame);

		FreeExpression(item->visibleExp, fromTheGame);
		item->visibleExp = nullptr;

		FreeExpression(item->disabledExp, fromTheGame);
		item->disabledExp = nullptr;

		FreeExpression(item->textExp, fromTheGame);
		item->textExp = nullptr;

		FreeExpression(item->materialExp, fromTheGame);
		item->materialExp = nullptr;

		if (item->typeData.data)
		{
			switch (item->dataType)
			{
			case Game::ITEM_TYPE_LISTBOX:
			case Game::ITEM_TYPE_EDITFIELD:
			case Game::ITEM_TYPE_NUMERICFIELD:
			case Game::ITEM_TYPE_VALIDFILEFIELD:
			case Game::ITEM_TYPE_UPREDITFIELD:
			case Game::ITEM_TYPE_YESNO:
			case Game::ITEM_TYPE_BIND:
			case Game::ITEM_TYPE_SLIDER:
			case Game::ITEM_TYPE_TEXT:
			case Game::ITEM_TYPE_DECIMALFIELD:
			case Game::ITEM_TYPE_EMAILFIELD:
			case Game::ITEM_TYPE_PASSWORDFIELD:
			case Game::ITEM_TYPE_MULTI:
			case Game::ITEM_TYPE_NEWS_TICKER:
			case Game::ITEM_TYPE_TEXT_SCROLL:
				FreeHunkAllocatedMemory(item->typeData.data, fromTheGame);
				break;
			}
		}


		FreeHunkAllocatedMemory(item->floatExpressions, fromTheGame);

		item->floatExpressionCount = 0;
		FreeHunkAllocatedMemory(item, fromTheGame);
	}

	void Menus::FreeAllocatedString(const void* ptr, bool fromTheGame)
	{
		if (fromTheGame)
		{
			// Ideally, this is what we should do.
			// The issue is I don't nkow enough about StringTable to know what I'm doing
			// and so currently when doing this, the game hangs. I suspect it's removing one too many users on a string
			// and ends up with -1 unsigned users and loops forever
			// Until we know what we're doing here we'll have to accept a little leak
			//
			// Game::Free_String(reinterpret_cast<const char*>(ptr));
		}
		else
		{
			Allocator.free(ptr);
		}
	}

	void Menus::FreeHunkAllocatedMemory(const void* ptr, bool fromTheGame)
	{
		if (ptr)
		{
			if (fromTheGame)
			{
				// Hunk memory doesn't need freeing - in that context the hunk is cleared at once
			}
			else
			{
				Allocator.free(ptr);
			}
		}
	}

	void Menus::FreeZAllocatedMemory(const void* ptr, bool fromTheGame)
	{
		if (ptr)
		{
			if (fromTheGame)
			{
				Game::Z_Free(ptr);
			}
			else
			{
				Allocator.free(ptr);
			}
		}
	}

	void Menus::PrepareToUnloadMenu(Game::menuDef_t* menu)
	{
		const std::string name = menu->window.name;
		Game::menuDef_t* originalMenu = nullptr;

		// Check if this menu was an override we previously tracked
		if (OverridenMenus.count(name)) {
			originalMenu = OverridenMenus[name]; // This could be nullptr if it was an implicit override of an unknown pointer
			DebugPrint("PrepareToUnloadMenu: Unloading menu '{}' ({:X}). Original was tracked as {:X}.", name, (unsigned int)menu, (unsigned int)originalMenu);
		}
		else {
			// This case might happen if a menu was loaded and became an implicit override,
			// but we didn't populate OverridenMenus with its original counterpart,
			// or if it's a new menu being unloaded.
			DebugPrint("PrepareToUnloadMenu: Unloading menu '{}' ({:X}). Not tracked as explicit override.", name, (unsigned int)menu);
		}

		for (size_t contextIndex = 0; contextIndex < ARRAYSIZE(Menus::GameUiContexts); contextIndex++)
		{
			const auto context = Menus::GameUiContexts[contextIndex];

			// 1. Remove the menu we are unloading from all contexts and stacks
			RemoveMenuFromContext(context, menu);

			// 2. If this menu was an override, attempt to put the original back
			// This is critical for game-referenced menus like 'connect'.
			if (originalMenu)
			{
				bool foundExistingSpot = false;
				// Check if original is already back (e.g., if another part of the game re-added it)
				for (int i = 0; i < context->menuCount; ++i) {
					if (context->Menus[i] == originalMenu) {
						foundExistingSpot = true;
						break;
					}
				}
				if (!foundExistingSpot) {
					// Attempt to add the original menu back to the context's main array
					if (context->menuCount < ARRAYSIZE(context->Menus)) {
						context->Menus[context->menuCount] = originalMenu;
						context->menuCount++;
						DebugPrint("PrepareToUnloadMenu: Restored original menu '{}' ({:X}) to UI context {:X}.", name, (unsigned int)originalMenu, (unsigned int)context);
					}
					else {
						Components::Logger::Print(Game::CON_CHANNEL_UI, "PrepareToUnloadMenu: UI context menu array full for restoring original menu {}\n", name);
					}
				}
			}
		}

		// Clear the override tracking after processing.
		if (OverridenMenus.count(name))
		{
			DebugPrint("PrepareToUnloadMenu: Clearing override tracking for menu '{}'.", name);
			OverridenMenus.erase(name);
		}
	}

	void Menus::AfterLoadedMenuFromDisk(Game::menuDef_t* menu)
	{
		const std::string name = menu->window.name;
		DebugPrint("AfterLoadedMenuFromDisk: Loaded menu '{}' at {:X}.", name, (unsigned int)menu);

		Game::menuDef_t* existingGameMenu = nullptr;
		bool foundExistingInContext = false;

		// Check if a menu with the same name already exists in ANY of the game contexts.
		// This identifies if our newly loaded menu is an override.
		for (size_t contextIndex = 0; contextIndex < ARRAYSIZE(Menus::GameUiContexts); contextIndex++)
		{
			const auto context = Menus::GameUiContexts[contextIndex];
			Game::menuDef_t* found = Game::Menus_FindByName(context, name.data());
			if (found && found != menu) // Found an existing menu that is NOT our newly loaded one
			{
				existingGameMenu = found;
				foundExistingInContext = true;
				DebugPrint("AfterLoadedMenuFromDisk: Found existing menu '{}' ({:X}) in context {:X}. This is an override.", name, (unsigned int)existingGameMenu, (unsigned int)context);
				break; // Only need to find one existing instance to confirm it's an override
			}
		}

		if (foundExistingInContext)
		{
			// This new menu is an override.
			// 1. Store the original menu for later restoration if this custom menu is unloaded.
			if (!OverridenMenus.count(name) || OverridenMenus[name] == nullptr) { // Only store if not already tracked or if previously tracked as nullptr
				OverridenMenus[name] = existingGameMenu;
				DebugPrint("AfterLoadedMenuFromDisk: Stored original menu '{}' ({:X}) for override by new menu ({:X}).", name, (unsigned int)existingGameMenu, (unsigned int)menu);
			}

			// 2. Remove all instances of the original menu from ALL contexts and their stacks.
			// This is crucial to prevent the original from lingering.
			for (size_t contextIndex = 0; contextIndex < ARRAYSIZE(Menus::GameUiContexts); contextIndex++)
			{
				RemoveMenuFromContext(GameUiContexts[contextIndex], existingGameMenu);
			}
		}
		else
		{
			// This is a brand new menu, not an override. Mark it as such.
			// Ensure it's not present in OverridenMenus, or set to nullptr.
			OverridenMenus[name] = nullptr;
			DebugPrint("AfterLoadedMenuFromDisk: Menu '{}' ({:X}) is a new menu, not an override.", name, (unsigned int)menu);
		}

		// Now, add the newly loaded custom menu to the main UI context (Game::uiContext).
		// This applies to both new menus and overrides.
		bool menuAlreadyActiveInUiContext = false;
		for (int i = 0; i < Game::uiContext->menuCount; ++i) {
			if (Game::uiContext->Menus[i] == menu) { // Check if our new menu instance is already there
				menuAlreadyActiveInUiContext = true;
				break;
			}
		}

		if (!menuAlreadyActiveInUiContext) {
			if (Game::uiContext->menuCount < ARRAYSIZE(Game::uiContext->Menus))
			{
				Game::uiContext->Menus[Game::uiContext->menuCount] = menu;
				Game::uiContext->menuCount++;
				DebugPrint("AfterLoadedMenuFromDisk: Added menu '{}' ({:X}) to Game::uiContext->Menus[{}] (Total count: {}).",
					name, (unsigned int)menu, Game::uiContext->menuCount - 1, Game::uiContext->menuCount);
			}
			else {
				Components::Logger::PrintError(Game::CON_CHANNEL_UI, "AfterLoadedMenuFromDisk: UI context menu array full for adding menu {}\n", name);
			}
		}
		else {
			DebugPrint("AfterLoadedMenuFromDisk: Menu '{}' ({:X}) already present in Game::uiContext->Menus, no re-addition.", name, (unsigned int)menu);
		}

		// Do NOT automatically add it to the menuStack here unless you're explicitly opening it.
		// Opening a menu (e.g., via `open "menuName"`) is what typically pushes it to the stack.
		// If you push it here and the game doesn't expect it, it can lead to stacking issues.
		// The `Game::Menus_OpenByName` call usually handles pushing to the stack.
		// The previous logic for `wasOverride && !menuAlreadyInStack` to add to stack is risky.
		// Remove this block:
		/*
		bool menuAlreadyInStack = false;
		for (int i = 0; i < Game::uiContext->openMenuCount; ++i) {
			if (Game::uiContext->menuStack[i] == MenusFromDisk[name]) {
				menuAlreadyInStack = true;
				break;
			}
		}
		if (wasOverride && !menuAlreadyInStack) {
			if (Game::uiContext->openMenuCount < ARRAYSIZE(Game::uiContext->menuStack)) {
				Game::uiContext->menuStack[Game::uiContext->openMenuCount] = MenusFromDisk[name];
				Game::uiContext->openMenuCount++;
				DebugPrint("Added menu {} ({:X}) to Game::uiContext->menuStack[{}] (Total count: {})",
					name, (unsigned int)MenusFromDisk[name], Game::uiContext->openMenuCount - 1, Game::uiContext->openMenuCount);
			}
		}
		*/
	}

	void Menus::Add(const std::string& menu)
	{
		CustomIW4xMenus.push_back(menu);
	}


	Game::StaticDvar* Menus::ReallocateStaticDvarLocally(Game::StaticDvar* sdvar)
	{
		Game::StaticDvar* reallocated = nullptr;

		if (sdvar)
		{
			reallocated = Allocator.allocate<Game::StaticDvar>();
			std::memcpy(reallocated, sdvar, sizeof(Game::StaticDvar));

			DUPLICATE_STRING_IF_EXISTS(reallocated, dvarName);

			// this one is fetched at runtime, on-demand, so we can tolerate to put it to NULLPTR !
			reallocated->dvar = nullptr;
		}

		return reallocated;
	}

	void Menus::UpdateSupportingDataContents()
	{
		assert(Menus::SupportingData->staticDvarList.staticDvars);
		assert(Menus::SupportingData->uiStrings.strings);
		assert(Menus::SupportingData->uifunctions.functions);

		const auto original = Game::menuSupportingData;
		const auto supportingData = Menus::SupportingData;

		// It should never has _decreased_ otherwise we're in trouble lol
		assert(original->uifunctions.totalFunctions >= supportingData->uifunctions.totalFunctions);
		assert(original->staticDvarList.numStaticDvars >= supportingData->staticDvarList.numStaticDvars);
		assert(original->uiStrings.totalStrings >= supportingData->uiStrings.totalStrings);

		// Grab all the stuff we might be missing - normally there's already room for it
		for (auto i = supportingData->uifunctions.totalFunctions; i < original->uifunctions.totalFunctions; ++i) {
			auto* function = original->uifunctions.functions[i];
			supportingData->uifunctions.functions[i] = ReallocateExpressionLocally(function);
		}

		for (auto i = supportingData->staticDvarList.numStaticDvars; i < original->staticDvarList.numStaticDvars; ++i) {
			auto* dvar = original->staticDvarList.staticDvars[i];
			supportingData->staticDvarList.staticDvars[i] = ReallocateStaticDvarLocally(dvar);
		}

		for (auto i = supportingData->uiStrings.totalStrings; i < original->uiStrings.totalStrings; ++i) {
			auto string = original->uiStrings.strings[i];
			supportingData->uiStrings.strings[i] = Allocator.duplicateString(string);
		}

		supportingData->uifunctions.totalFunctions = original->uifunctions.totalFunctions;
		supportingData->staticDvarList.numStaticDvars = original->staticDvarList.numStaticDvars;
		supportingData->uiStrings.totalStrings = original->uiStrings.totalStrings;
	}

	Game::itemDef_s* Menus::ReallocateItemLocally(Game::itemDef_s* item, bool andFree)
	{
		Game::itemDef_s* reallocatedItem = nullptr;

		if (item)
		{
			reallocatedItem = Allocator.allocate<Game::itemDef_s>();
			std::memcpy(reallocatedItem, item, sizeof(Game::itemDef_s));

			reallocatedItem->floatExpressions = Allocator.allocateArray<Game::ItemFloatExpression>(item->floatExpressionCount);

			if (item->floatExpressionCount)
			{
				std::memcpy(reallocatedItem->floatExpressions, item->floatExpressions, sizeof(Game::ItemFloatExpression) * item->floatExpressionCount);

				for (auto j = 0; j < item->floatExpressionCount; ++j)
				{
					const auto previousExpression = item->floatExpressions[j].expression;
					reallocatedItem->floatExpressions[j].expression = ReallocateExpressionLocally(previousExpression);
				}
			}

			reallocatedItem->accept = ReallocateEventHandlerSetLocally(item->accept);
			reallocatedItem->action = ReallocateEventHandlerSetLocally(item->action);
			reallocatedItem->leaveFocus = ReallocateEventHandlerSetLocally(item->leaveFocus);
			reallocatedItem->mouseEnter = ReallocateEventHandlerSetLocally(item->mouseEnter);
			reallocatedItem->mouseEnterText = ReallocateEventHandlerSetLocally(item->mouseEnterText);
			reallocatedItem->mouseExit = ReallocateEventHandlerSetLocally(item->mouseExit);
			reallocatedItem->mouseExitText = ReallocateEventHandlerSetLocally(item->mouseExitText);
			reallocatedItem->onFocus = ReallocateEventHandlerSetLocally(item->onFocus);

			reallocatedItem->onKey = ReallocateItemKeyHandler(item->onKey);

			reallocatedItem->disabledExp = ReallocateExpressionLocally(item->disabledExp);
			reallocatedItem->visibleExp = ReallocateExpressionLocally(item->visibleExp);
			reallocatedItem->materialExp = ReallocateExpressionLocally(item->materialExp);
			reallocatedItem->textExp = ReallocateExpressionLocally(item->textExp);

			// You can check this at 0x63EEA0
			if (reallocatedItem->typeData.data)
			{
				switch (reallocatedItem->dataType)
				{
				case Game::ITEM_TYPE_LISTBOX:
					reallocatedItem->typeData.data = Reallocate(reallocatedItem->typeData.data, 324);
					break;

				case Game::ITEM_TYPE_EDITFIELD:
				case Game::ITEM_TYPE_NUMERICFIELD:
				case Game::ITEM_TYPE_VALIDFILEFIELD:
				case Game::ITEM_TYPE_UPREDITFIELD:
				case Game::ITEM_TYPE_YESNO:
				case Game::ITEM_TYPE_BIND:
				case Game::ITEM_TYPE_SLIDER:
				case Game::ITEM_TYPE_TEXT:
				case Game::ITEM_TYPE_DECIMALFIELD:
				case Game::ITEM_TYPE_EMAILFIELD:
				case Game::ITEM_TYPE_PASSWORDFIELD:
					reallocatedItem->typeData.data = Reallocate(reallocatedItem->typeData.data, 32);
					break;

				case Game::ITEM_TYPE_MULTI:
					reallocatedItem->typeData.data = Reallocate(reallocatedItem->typeData.data, 392);
					break;

				case Game::ITEM_TYPE_NEWS_TICKER:
					reallocatedItem->typeData.data = Reallocate(reallocatedItem->typeData.data, 28);
					break;

				case Game::ITEM_TYPE_TEXT_SCROLL:
					reallocatedItem->typeData.data = Reallocate(reallocatedItem->typeData.data, 4);
					break;
				}
			}

			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, dvar);
			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, dvarTest);
			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, localVar);
			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, enableDvar);
			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, text);

			DUPLICATE_STRING_IF_EXISTS(reallocatedItem, window.name);

			// What about item expressions? We don't free these?
			// Apparently not, the game doesn't free them
			// They're freed in bulk!
			if (andFree)
			{
#if 0
				Game::Menu_FreeItem(item);
#else
				// The menuFreeItem misses lots of stuff! Mainly allocated item entries.
				// And those are Z_Alloced so they are NOT FREED IN BULK!
				// This is a good example: 0x413050
				// Let's do us a favor and free them too otherwise it leaks into the engine
				Menus::FreeItem(item, ALLOCATED_BY_GAME);
#endif
			}


		}

		return reallocatedItem;

	}

	Game::Statement_s* Menus::ReallocateExpressionLocally(Game::Statement_s* statement, bool andFree)
	{
		Game::Statement_s* reallocated = nullptr;

		if (statement)
		{
			reallocated = Allocator.allocate<Game::Statement_s>();
			std::memcpy(reallocated, statement, sizeof(Game::Statement_s));

			if (statement->entries)
			{
				if (reallocated->numEntries == 0)
				{
					// happens! In the vanilla game. I don't know why.
					reallocated->entries = Allocator.allocate<Game::expressionEntry>();
				}
				else
				{
					reallocated->entries = Allocator.allocateArray<Game::expressionEntry>(reallocated->numEntries);
					std::memcpy(reallocated->entries, statement->entries, sizeof(Game::expressionEntry) * reallocated->numEntries);
				}
			}

			// Reallocate all the supporting data
			if (statement->supportingData)
			{
#if DEBUG
				assert(statement->supportingData == Game::menuSupportingData);
#endif
				// It might have moved in the meantime
				UpdateSupportingDataContents();

				reallocated->supportingData = Menus::SupportingData;
			}

			if (andFree)
			{
				Game::free_expression(statement); // this is not really necessary anyway - the game allocates and frees menu memory in bulk (using HunkUser)
			}
		}

		return reallocated;
	}

	void Menus::FreeMenuListOnly(Game::MenuList* menuList)
	{
		DebugPrint("Freeing only menuList {} at {:X}",
			menuList->name,
			(unsigned int)menuList
		);

		Allocator.free(menuList->name);
		Allocator.free(menuList->menus);
		Allocator.free(menuList);
	}

	void Menus::FreeMenuOnly(Game::menuDef_t* menu)
	{
		DebugPrint("Freeing only menu {} at {:X}",
			menu->window.name,
			(unsigned int)menu
		);

		if (menu->items)
		{
			for (int i = 0; i < menu->itemCount; ++i)
			{
				FreeItem(menu->items[i], ALLOCATED_BY_IW4X);
			}

			FreeZAllocatedMemory(menu->items, ALLOCATED_BY_IW4X);
		}

		FreeItemKeyHandler(menu->onKey, ALLOCATED_BY_IW4X);

		FreeEventHandlerSet(menu->onOpen, ALLOCATED_BY_IW4X);
		FreeEventHandlerSet(menu->onCloseRequest, ALLOCATED_BY_IW4X);
		FreeEventHandlerSet(menu->onClose, ALLOCATED_BY_IW4X);
		FreeEventHandlerSet(menu->onESC, ALLOCATED_BY_IW4X);

		FreeExpression(menu->visibleExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->rectXExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->rectYExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->rectWExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->rectHExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->openSoundExp, ALLOCATED_BY_IW4X);
		FreeExpression(menu->closeSoundExp, ALLOCATED_BY_IW4X);


		FREE_STRING_IF_EXISTS(menu, font, ALLOCATED_BY_IW4X);
		FREE_STRING_IF_EXISTS(menu, allowedBinding, ALLOCATED_BY_IW4X);
		FREE_STRING_IF_EXISTS(menu, soundName, ALLOCATED_BY_IW4X);

		FreeZAllocatedMemory(menu->window.name, ALLOCATED_BY_IW4X);

		FreeZAllocatedMemory(menu, ALLOCATED_BY_IW4X);
	}

	// We free our own, but keep the object because we're going to reuse it
	void Menus::FreeLocalSupportingDataContents() {

		const auto data = Menus::SupportingData;

		for (auto i = 0; i < data->uifunctions.totalFunctions; ++i) {
			auto* function = data->uifunctions.functions[i];
			FreeExpression(function);
		}

		for (auto i = 0; i < data->staticDvarList.numStaticDvars; i++)
		{
			// This is not on the string table, it IS a zmalloced string!
			FreeZAllocatedMemory(data->staticDvarList.staticDvars[i]->dvarName);
			FreeZAllocatedMemory(data->staticDvarList.staticDvars[i]);
		}

		for (auto i = 0; i < data->uiStrings.totalStrings; i++)
		{
			FREE_STRING_IF_EXISTS(data, uiStrings.strings[i], false);
		}

		data->staticDvarList.numStaticDvars = 0;
		data->uiStrings.totalStrings = 0;
		data->uifunctions.totalFunctions = 0;
	}

	Game::MenuEventHandlerSet* Menus::ReallocateEventHandlerSetLocally(const Game::MenuEventHandlerSet* handlerSet, bool andFree)
	{
		Game::MenuEventHandlerSet* reallocated = nullptr;

		if (handlerSet)
		{
			reallocated = Allocator.allocate<Game::MenuEventHandlerSet>();
			std::memcpy(reallocated, handlerSet, sizeof(Game::MenuEventHandlerSet));

			reallocated->eventHandlers = Allocator.allocateArray<Game::MenuEventHandler*>(handlerSet->eventHandlerCount);

			for (auto i = 0; i < handlerSet->eventHandlerCount; ++i) {
				auto event = Allocator.allocate<Game::MenuEventHandler>();
				std::memcpy(event, handlerSet->eventHandlers[i], sizeof(Game::MenuEventHandler));

				reallocated->eventHandlers[i] = event;

				Game::ConditionalScript* conditionalScript;
				Game::SetLocalVarData* localVar;

				switch (event->eventType) {
				case Game::EVENT_IF:
					conditionalScript = Allocator.allocate<Game::ConditionalScript>();
					std::memcpy(conditionalScript, event->eventData.conditionalScript, sizeof(Game::ConditionalScript));

					if (conditionalScript->eventHandlerSet)
					{
						conditionalScript->eventHandlerSet = ReallocateEventHandlerSetLocally(conditionalScript->eventHandlerSet, andFree);
					}

					if (conditionalScript->eventExpression)
					{
						conditionalScript->eventExpression = ReallocateExpressionLocally(conditionalScript->eventExpression, andFree);
					}

					event->eventData.conditionalScript = conditionalScript;

					break;

				case Game::EVENT_ELSE:
					if (event->eventData.elseScript)
					{
						event->eventData.elseScript = ReallocateEventHandlerSetLocally(event->eventData.elseScript, andFree);
					}

					break;

				case Game::EVENT_SET_LOCAL_VAR_BOOL:
				case Game::EVENT_SET_LOCAL_VAR_INT:
				case Game::EVENT_SET_LOCAL_VAR_FLOAT:
				case Game::EVENT_SET_LOCAL_VAR_STRING:
					localVar = Allocator.allocate<Game::SetLocalVarData>();
					std::memcpy(localVar, event->eventData.setLocalVarData, sizeof(Game::SetLocalVarData));

					if (localVar->expression)
					{
						localVar->expression = ReallocateExpressionLocally(localVar->expression, andFree);
					}

					event->eventData.setLocalVarData = localVar;

					break;

				default:
					break;
				}
			}
		}

		return reallocated;
	}

	Game::ItemKeyHandler* Menus::ReallocateItemKeyHandler(const Game::ItemKeyHandler* keyHandler, bool andFree)
	{
		Game::ItemKeyHandler* reallocated = nullptr;

		if (keyHandler)
		{
			reallocated = Reallocate(keyHandler, sizeof(Game::ItemKeyHandler));
			std::memcpy(reallocated, keyHandler, sizeof(Game::MenuEventHandlerSet));

			reallocated->action = ReallocateEventHandlerSetLocally(reallocated->action, andFree);

			if (keyHandler->next)
			{
				if (keyHandler == keyHandler->next)
				{
					reallocated->next = reallocated;
				}
				else
				{
					// Recurse
					reallocated->next = ReallocateItemKeyHandler(reallocated->next, andFree);
				}
			}

		}

		return reallocated;
	}

	void Menus::FreeEventHandlerSet(Game::MenuEventHandlerSet* handlerSet, bool fromTheGame)
	{
		if (handlerSet)
		{

			for (auto i = 0; i < handlerSet->eventHandlerCount; ++i) {
				auto event = handlerSet->eventHandlers[i];

				Game::ConditionalScript* conditionalScript;
				Game::MenuEventHandlerSet* elseScript;
				Game::SetLocalVarData* localVar;

				switch (event->eventType) {
				case Game::EVENT_IF:
					conditionalScript = event->eventData.conditionalScript;

					if (conditionalScript->eventHandlerSet)
					{
						FreeEventHandlerSet(conditionalScript->eventHandlerSet, fromTheGame);
						conditionalScript->eventHandlerSet = nullptr;
					}

					if (conditionalScript->eventExpression)
					{
						FreeExpression(conditionalScript->eventExpression, fromTheGame);
						conditionalScript->eventExpression = nullptr;
					}

					FreeHunkAllocatedMemory(conditionalScript, fromTheGame);
					event->eventData.conditionalScript = nullptr;

					break;

				case Game::EVENT_ELSE:
					elseScript = event->eventData.elseScript;

					if (elseScript)
					{
						FreeEventHandlerSet(elseScript, fromTheGame);
						event->eventData.elseScript = nullptr;
					}

					FreeHunkAllocatedMemory(elseScript, fromTheGame);

					break;

				case Game::EVENT_SET_LOCAL_VAR_BOOL:
				case Game::EVENT_SET_LOCAL_VAR_INT:
				case Game::EVENT_SET_LOCAL_VAR_FLOAT:
				case Game::EVENT_SET_LOCAL_VAR_STRING:
					localVar = event->eventData.setLocalVarData;

					if (localVar->expression)
					{
						FreeExpression(localVar->expression, fromTheGame);
						localVar->expression = nullptr;
					}

					FreeHunkAllocatedMemory(localVar, fromTheGame);

					break;

				case Game::EVENT_UNCONDITIONAL:
					FREE_STRING_IF_EXISTS(event, eventData.unconditionalScript, fromTheGame);
					break;

				default:
					break;
				}

				FreeHunkAllocatedMemory(event, fromTheGame);
			}

			handlerSet->eventHandlerCount = 0;
			FreeHunkAllocatedMemory(handlerSet->eventHandlers, fromTheGame);
			FreeHunkAllocatedMemory(handlerSet, fromTheGame);
		}
	}

	void Menus::FreeItemKeyHandler(Game::ItemKeyHandler* itemKeyHandler, bool fromTheGame)
	{
		if (itemKeyHandler)
		{
			if (itemKeyHandler->next && itemKeyHandler->next != itemKeyHandler)
			{
				FreeItemKeyHandler(itemKeyHandler->next, fromTheGame);
			}

			FreeEventHandlerSet(itemKeyHandler->action, fromTheGame);

			FreeHunkAllocatedMemory(itemKeyHandler, fromTheGame);
		}
	}

	void Menus::FreeExpression(Game::Statement_s* statement, bool fromTheGame)
	{
		if (statement)
		{
			if (statement->entries)
			{
				FreeZAllocatedMemory(statement->entries, fromTheGame);
				statement->entries = nullptr;
			}

			if (statement->supportingData)
			{
				// <
				//	DO NOT FREE SUPPORTING DATA !
				// >

				if (!fromTheGame)
				{
					assert(statement->supportingData == SupportingData);
				}
			}

			FreeZAllocatedMemory(statement, fromTheGame);
		}
	}

	void Menus::UnloadMenuFromDisk(const std::string& menuName)
	{
		if (MenusFromDisk.contains(menuName)) {
			const auto menu = MenusFromDisk[menuName];
			PrepareToUnloadMenu(menu);
			FreeMenuOnly(menu);
			MenusFromDisk.erase(menuName);
		}
	}

	// This is fired up on Vid_restart / filesystem restart, like changing mod
	void Menus::ReloadDiskMenus_OnUIInitialization()
	{
		// Free _your_ locally allocated supporting data contents.
		// The game's `UI_Init` will have already memset `Game::uiContext` and thus `Game::menuSupportingData`.
		FreeLocalSupportingDataContents();

		// Free the CGDC - The game doesn't do it, but it _should_
		// Otherwise it's full of weird garbage. It's never used until CGame starts anyway!
		{
			// At this point our menus are already tracked so we will be able to free them
			// and the HUD menus are freed in bulk at 0x4E32D5
			Game::cgDC->menuCount = 0;
			Game::cgDC->openMenuCount = 0; // Crucial: clear the stack too
			// Also clear any pointers to avoid stale data
			std::memset(Game::cgDC->Menus, 0, sizeof(Game::cgDC->Menus));
			std::memset(Game::cgDC->menuStack, 0, sizeof(Game::cgDC->menuStack));
		}

		// Initialize Menus::SupportingData structure with arrays if they were freed.
		// This should be done AFTER FreeLocalSupportingDataContents().
		// If your `FreeLocalSupportingDataContents` only clears the data inside,
		// but not the arrays themselves, this might not be needed.
		// However, it's safer to ensure these arrays are valid.
		if (!Menus::SupportingData->uifunctions.functions) {
			InitializeSupportingData(); // Re-allocate the top-level arrays if they were freed
		}

		// Now, proceed with reloading all menus.
		ReloadDiskMenus();
	}


	// This is fired up _right before the game starts_, we need to do it once again to load "ingame" menus that we might have skipped prior
	void Menus::ReloadDiskMenus_OnCGameStart()
	{
		ReloadDiskMenus();
	}

	void Menus::ReloadDiskMenus()
	{
		const auto connectionState = *reinterpret_cast<Game::connstate_t*>(0xB2C540);

		// We only allow non-menulist menus when we're ingame
		// Otherwise we load a ton of ingame-only menus that are glitched on main_text
		const bool allowStrayMenus = connectionState > Game::connstate_t::CA_DISCONNECTED
			&& Game::CL_IsCgameInitialized();

		DebugPrint("Reloading disk menus...");

		// Step 1: unload everything
		const auto listsFromDisk = MenuListsFromDisk;
		for (const auto& menuList : listsFromDisk)
		{
			FreeMenuListOnly(menuList.second);
			MenuListsFromDisk.erase(menuList.first);
		}

		const auto menusFromDisk = MenusFromDisk;
		for (const auto& element : menusFromDisk)
		{
			// IMPORTANT: Call UnloadMenuFromDisk which now uses RemoveMenuFromContext
			UnloadMenuFromDisk(element.first);
		}

#if DEBUG
		if (Allocator.empty() || allowStrayMenus)
		{
			// good
		}
		else
		{
			__debugbreak();
			Logger::Print("Warning - menu leak? Expected allocator to be empty after reload, but it's not!\n");
		}
#endif

		if (OverridenMenus.empty())
		{
			// good
		}
		else
		{
			Logger::Print("Warning - menu leak? Expected overriden menus to be empty after reload, but they're not!\n");
			OverridenMenus.clear();
		}

		// Step 2: Load everything
		{
			const auto menus = FileSystem::GetFileList("ui_mp", "menu", Game::FS_LIST_ALL);

			// Load standalone menus
			for (const auto& filename : menus)
			{
				const std::string fullPath = std::format("ui_mp\\{}", filename);

				LoadScriptMenu(fullPath.c_str(), allowStrayMenus);
			}

			if (allowStrayMenus)
			{
				const auto scriptmenus = FileSystem::GetFileList("ui_mp\\scriptmenus", "menu", Game::FS_LIST_ALL);

				// Load standalone menus
				for (const auto& filename : scriptmenus)
				{
					const std::string fullPath = std::format("ui_mp\\scriptmenus\\{}", filename);

					LoadScriptMenu(fullPath.c_str(), allowStrayMenus);
				}
			}
		}

		{
			const auto menuLists = FileSystem::GetFileList("ui_mp", "txt", Game::FS_LIST_ALL);

			// Load menu list
			for (const auto& filename : menuLists)
			{
				const std::string fullPath = std::format("ui_mp\\{}", filename);
				LoadScriptMenu(fullPath.c_str(), true);
			}

			// "code.txt" for IW4x
			for (const auto& menuName : CustomIW4xMenus)
			{
				LoadScriptMenu(menuName.c_str(), true);
			}
		}

		// Step 3 - Keep supporting data around

		// Debug-only check
		CheckMenus();
	}

	bool Menus::IsMenuVisible(Game::UiContext* dc, Game::menuDef_t* menu)
	{
		if (menu && menu->window.name)
		{
			if (menu->window.name == "connect"s) // Check if we're supposed to draw the loadscreen
			{
				// If this 'menu' is the original one that was overridden by our custom 'connect' menu
				// then hide it.
				if (OverridenMenus.contains("connect"s) && OverridenMenus["connect"s] == menu)
				{
					return false; // Hide the original
				}
				// Also, if the original AssetHandler::FindOriginalAsset is not hooked,
				// and if the AssetHandler::OnFind hook path is taken,
				// then check if our custom menu is active and this is the original (fall-through logic)
				else if (MenusFromDisk.contains("connect"s)) {
					// If our custom 'connect' is loaded, and this is NOT our custom 'connect' menu (i.e., it's the default one), hide it.
					if (menu != MenusFromDisk["connect"s]) {
						DebugPrint("IsMenuVisible: Hiding default 'connect' menu ({:X}) because custom 'connect' is in MenusFromDisk.", (unsigned int)menu);
						return false;
					}
				}
			}
		}

		return Game::Menu_IsVisible(dc, menu); // Call the original game function
	}


	void Menus::CheckMenus()
	{
#if DEBUG
		// Give a hand to the poor programmer there

		{
			// Uniqueness check - each unique menu should have a unique name for this whole circus to run
			std::unordered_map<std::string, void*> names{};

			assert(Game::menuSupportingData->staticDvarList.numStaticDvars == Menus::SupportingData->staticDvarList.numStaticDvars);
			assert(Game::menuSupportingData->uifunctions.totalFunctions == Menus::SupportingData->uifunctions.totalFunctions);
			assert(Game::menuSupportingData->uiStrings.totalStrings == Menus::SupportingData->uiStrings.totalStrings);

			for (size_t contextIndex = 0; contextIndex < ARRAYSIZE(Menus::GameUiContexts); contextIndex++)
			{
				const auto context = Menus::GameUiContexts[contextIndex];

				for (size_t i = 0; i < ARRAYSIZE(context->Menus); i++)
				{
					if (context->Menus[i] && static_cast<int>(i) < context->menuCount)
					{
						const auto name = context->Menus[i]->window.name;

						if (names.contains(name))
						{
							if (names[name] != context->Menus[i])
							{
								assert(false && "Two menus were loaded with the same name!");
							}
							else
							{
								// This behaviour is actually normal in the basegame
							}
						}
						else
						{
							names[name] = context->Menus[i];
						}
					}
					else
					{
						assert(static_cast<int>(i) >= context->menuCount && "Unexpected NULL data where the game expects a menu!");
					}
				}
			}

			for (const auto& pair : MenusFromDisk)
			{
				const auto menu = pair.second;

#define CHECK_SD(x) if (menu->##x && menu->##x->supportingData) assert(menu->##x->supportingData == Menus::SupportingData)

				CHECK_SD(visibleExp);
				CHECK_SD(rectXExp);
				CHECK_SD(rectYExp);
				CHECK_SD(rectWExp);
				CHECK_SD(rectHExp);
				CHECK_SD(openSoundExp);
				CHECK_SD(closeSoundExp);
			}
		}
#endif
	}

	void Menus::InitializeSupportingData()
	{
		// Do not use the local allocator for this
		const auto allocator = Utils::Memory::GetAllocator();

		Menus::SupportingData = allocator->allocate<Game::ExpressionSupportingData>();

		const auto staticDvarSize = *reinterpret_cast<size_t*>(0x4A1299 + 1);
		const auto functionListSize = *reinterpret_cast<size_t*>(0x4A12A3 + 1);
		const auto stringListSize = *reinterpret_cast<size_t*>(0x4A12B2 + 1);

		Menus::SupportingData->uifunctions.functions = allocator->allocateArray<Game::Statement_s*>(functionListSize / sizeof(Game::Statement_s*));
		Menus::SupportingData->staticDvarList.staticDvars = allocator->allocateArray<Game::StaticDvar*>(staticDvarSize / sizeof(Game::StaticDvar*));
		Menus::SupportingData->uiStrings.strings = allocator->allocateArray<const char*>(stringListSize / sizeof(const char*));
	}

	Menus::Menus()
	{
		menuParseKeywordHash = reinterpret_cast<Game::KeywordHashEntry<Game::menuDef_t, 128, 3523>**>(0x63AE928);

		if (ZoneBuilder::IsEnabled())
		{
			Game::Menu_Setup(Game::uiContext);
		}

		if (Dedicated::IsEnabled()) return;

		Menus::InitializeSupportingData();

		Components::Events::OnCGameInit(ReloadDiskMenus_OnCGameStart);
		Components::Events::AfterUIInit(ReloadDiskMenus_OnUIInitialization);

		// --- HOOK ASSET HANDLER ---
		// These hooks were present in your OLD working code.
		// They are the key to intercepting asset lookup calls via AssetHandler.
		AssetHandler::OnFind(Game::ASSET_TYPE_MENU, MenuFindHook);
		AssetHandler::OnFind(Game::ASSET_TYPE_MENULIST, MenuListFindHook);


		Components::Scheduler::Once([]() {
			PrintMenuDebug = Dvar::Register<bool>("g_log_menu_allocations", false, Game::DVAR_SAVED, "Prints all menu allocations and swapping in the console");
			}, Components::Scheduler::Pipeline::MAIN);


		// Increase HunkMemory for people with heavy-loaded menus
		// Original is 0xA00000
		Utils::Hook::Set<uint32_t>(0x420830 + 6, 0xB00000);

		// Don't open connect menu twice - it gets stuck! (This was NOPed in old code, might need to match it)
		// Utils::Hook::Nop(0x428E48, 5); // Original old code used NOP

		// Use the connect menu open call to update server motds (This hook was in old code)
		Utils::Hook(0x428E48, []
			{
				if (!Party::GetMotd().empty() && Party::Target() == *Game::connectedHost)
				{
					Dvar::Var("didyouknow").set(Party::GetMotd());
				}
			}, HOOK_CALL).install()->quick();

		// Intercept menu painting (This hook was in old code)
		Utils::Hook(0x4FFBDF, IsMenuVisible, HOOK_CALL).install()->quick();

		// disable the 2 new tokens in ItemParse_rect (Fix by NTA. Probably because he didn't want to update the menus)
		Utils::Hook::Set<std::uint8_t>(0x640693, 0xEB);

		// don't load ASSET_TYPE_MENU assets for every menu (might cause patch menus to fail) (This NOP was in old code)
		Utils::Hook::Nop(0x453406, 5); // Re-added this NOP

		// make Com_Error and similar go back to main_text instead of menu_xboxlive.
		Utils::Hook::SetString(0x6FC790, "main_text");

		Command::Add("openmenu", [](const Command::Params* params)
			{
				if (params->size() != 2)
				{
					Logger::Print("USAGE: openmenu <menu name>\n");
					return;
				}

				// Not quite sure if we want to do this if we're not ingame, but it's only needed for ingame menus.
				if ((*Game::cl_ingame)->current.enabled)
				{
					Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
				}

				const char* menuName = params->get(1);

				Game::Menus_OpenByName(Game::uiContext, menuName);
			});

		// The "reloadmenus" command from the old code is removed here as the new system's ReloadDiskMenus() handles it differently.
		// Command::Add("reloadmenus", []() { ... });

		// Define custom menus here (Keep this, as ReloadDiskMenus() still uses it)
		Add("ui_mp/changelog.menu");
		Add("ui_mp/iw4x_credits.menu");
		Add("ui_mp/menu_first_launch.menu");
		Add("ui_mp/mod_download_popmenu.menu");
		Add("ui_mp/pc_options_game.menu");
		Add("ui_mp/pc_options_gamepad.menu");
		Add("ui_mp/pc_options_multi.menu");
		Add("ui_mp/popup_customclan.menu");
		Add("ui_mp/popup_customtitle.menu");
		Add("ui_mp/popup_friends.menu");
		Add("ui_mp/resetclass.menu");
		Add("ui_mp/security_increase_popmenu.menu");
		Add("ui_mp/startup_messages.menu");
		Add("ui_mp/stats_reset.menu");
		Add("ui_mp/stats_unlock.menu");
		Add("ui_mp/stats_mod_warning.menu");
		Add("ui_mp/theater_menu.menu");
		Add("ui_mp/connect.menu");
		Add("ui_mp/popup_partyconnect.menu");
		Add("ui_mp/popup_partyconnect_warning.menu");
	}

	void Menus::preDestroy()
	{
		// Let Windows handle the memory leaks for you!
		// The old code had Menus::FreeEverything(); here.
		// If the new system handles freeing globally, this might not be needed.
	}
}

// --- Implement AssetHandler hooks (from old Menus.cpp) ---
// These will act as the new AssetHandler for menus, routing to your internal loaders.
namespace Components
{
	Game::XAssetHeader Menus::MenuFindHook(Game::XAssetType /*type*/, const std::string& filename)
	{
		// This hook needs to return YOUR loaded menu, if it exists in MenusFromDisk.
		// Otherwise, it should call the original game function to find the asset.
		// Since we don't have a direct "DB_FindXAssetHeader_Original" in this strategy,
		// we rely on the game's default asset handler to load it if we don't have it.
		// The ImenuDef_t::load function (which is part of the AssetHandler process)
		// will handle loading from disk.

		// Clean the name similar to DB_FindXAssetHeader_Hook logic in the previous attempt
		std::string shortName = filename;
		size_t lastSlash = shortName.find_last_of("/\\");
		if (lastSlash != std::string::npos) shortName = shortName.substr(lastSlash + 1);
		if (shortName.length() > 5 && shortName.substr(shortName.length() - 5) == ".menu") {
			shortName = shortName.substr(0, shortName.length() - 5);
		}

		if (MenusFromDisk.contains(shortName))
		{
			DebugPrint("MenuFindHook: Intercepted request for menu '{}'. Returning custom menu ({:X})", shortName, (unsigned int)MenusFromDisk[shortName]);
			return { MenusFromDisk[shortName] };
		}

		// If not found in our custom map, let the game's default DB_FindXAssetHeader handle it.
		// The AssetHandler::IAsset::load implementation (e.g., in ImenuDef_t.cpp)
		// will be responsible for loading the actual game asset or calling our LoadMenuByName_Recursive.
		// Returning a nullptr XAssetHeader might force the AssetHandler to call the IAsset::load method.
		DebugPrint("MenuFindHook: Menu '{}' not found in custom list. Returning nullptr to trigger default AssetHandler load.", shortName);
		return { nullptr };
	}

	Game::XAssetHeader Menus::MenuListFindHook(Game::XAssetType type, const std::string& filename)
	{
		(void)type;

		// This hook needs to return YOUR loaded menulist, if it exists.
		if (MenuListsFromDisk.contains(filename))
		{
			DebugPrint("MenuListFindHook: Intercepted request for menulist '{}'. Returning custom menulist ({:X})", filename, (unsigned int)MenuListsFromDisk[filename]);
			return { MenuListsFromDisk[filename] };
		}

		// If not found in our custom map, let the game's default DB_FindXAssetHeader handle it.
		DebugPrint("MenuListFindHook: Menulist '{}' not found in custom list. Returning nullptr to trigger default AssetHandler load.", filename);
		return { nullptr };
	}
}
