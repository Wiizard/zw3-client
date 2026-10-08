#include "STDInclude.hpp"

#include "UIFeeder.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Maps.hpp"

namespace Components
{
	std::unordered_map<float, UIFeeder::Callbacks> UIFeeder::feeders;
	std::unordered_map<float, UIFeeder::EngineCallbacks> UIFeeder::extensions;

	Dvar::Var UIFeeder::ui_map_long;
	Dvar::Var UIFeeder::ui_map_name;
	Dvar::Var UIFeeder::ui_map_desc;

	constexpr std::uintptr_t Party_SetDisplayMapName = 0x14010BDB0;

	constexpr float mapFeeder = 60.0f;

	constexpr int listBoxType = 6;

	static const std::uintptr_t feederCountCalls[] = {
		0x14025E1BC, 0x14025E226, 0x14025F93A, 0x14025FBB6, 0x14025FC1A,
		0x14025FE39, 0x140260027, 0x14026023E, 0x14026058C, 0x1402667BB
	};

	static const std::uintptr_t feederItemTextCalls[] = {
		0x140261020
	};

	static const std::uintptr_t feederSelectionCalls[] = {
		0x14025FC75, 0x14026673B
	};

	static const std::uintptr_t feederSelectionJumps[] = {
		0x14025E120, 0x140261504
	};

	static constexpr std::uintptr_t listBoxMouseMoveCall = 0x14026596E;
	static constexpr std::uintptr_t hoverSelectionCall = 0x14026014E;
	static constexpr std::uintptr_t mouseOverSoundCall = 0x14026017F;
	static constexpr std::uintptr_t handleKeyItemCall = 0x14025F972;
	static constexpr std::uintptr_t feederDoubleClickCall = 0x14025FD40;
	static constexpr std::uintptr_t blinkColorCall = 0x14026112C;

	static constexpr std::uintptr_t listBoxMouseMove = 0x14025FE70;

	static constexpr std::uintptr_t paintCursorPosCall = 0x1402608F0;
	static constexpr std::uintptr_t hoverRowCountCall = 0x140260017;
	static constexpr std::uintptr_t itemColorCall = 0x140261046;
	static constexpr std::uintptr_t listBoxHandleKeyCall = 0x14025F3CA;
	static constexpr std::uintptr_t Item_ListBox_HandleKey = 0x14025F8C0;

	static constexpr int wheelDownKey = 0xCD;
	static constexpr int wheelUpKey = 0xCE;

	static constexpr std::uintptr_t hasFocusGate = 0x140260CD8;
	static constexpr std::uintptr_t overRowGate = 0x140260CEB;

	static constexpr std::uintptr_t feeder2OverrideBranch = 0x14026FF07;

	static constexpr std::size_t hookCount = ARRAY_COUNT(feederCountCalls)
		+ ARRAY_COUNT(feederItemTextCalls)
		+ ARRAY_COUNT(feederSelectionCalls)
		+ ARRAY_COUNT(feederSelectionJumps)
		+ 10;

	static Utils::Hook hooks[hookCount];
	static std::size_t installedHooks = 0;

	static Game::itemDef_s* hoverItem = nullptr;
	static int hoverLocalClientNum = 0;
	static Game::itemDef_s* keyItem = nullptr;
	static float paintFeeder = 0.0f;

	static int nextClickTime = 0;

	static constexpr int clickIntervalMs = 300;

	static bool SeatHook(std::uintptr_t site, void* replacement, bool asJump)
	{
		if (installedHooks >= hookCount)
		{
			return false;
		}

		Utils::Hook& hook = hooks[installedHooks];
		++installedHooks;

		hook.Initialize(site, replacement, asJump);
		hook.Install();

		if (!hook.IsInstalled())
		{
			return false;
		}

		hook.Quick();
		return true;
	}

	UIFeeder::Callbacks* UIFeeder::Find(float feeder)
	{
		const auto found = feeders.find(feeder);

		if (found == feeders.end())
		{
			return nullptr;
		}

		return &found->second;
	}

	UIFeeder::EngineCallbacks* UIFeeder::FindExtension(float feeder)
	{
		const auto found = extensions.find(feeder);

		if (found == extensions.end())
		{
			return nullptr;
		}

		return &found->second;
	}

	bool UIFeeder::ShouldOverride(float feeder)
	{
		if (feeder == 15.0f)
		{
			return false;
		}

		return feeders.contains(feeder);
	}

	void UIFeeder::Add(float feeder, GetItemCount count, GetItemText text, SelectItem select)
	{
		feeders[feeder] = { std::move(count), std::move(text), std::move(select) };
	}

	void UIFeeder::Extend(float feeder, const EngineCallbacks& callbacks)
	{
		extensions[feeder] = callbacks;
	}

	bool UIFeeder::Has(float feeder)
	{
		return feeders.contains(feeder);
	}

	int UIFeeder::FeederCount(int localClientNum, float feeder)
	{
		const Callbacks* const callbacks = Find(feeder);

		if (!callbacks || !callbacks->getItemCount)
		{
			const EngineCallbacks* const extension = FindExtension(feeder);

			if (extension && extension->count)
			{
				return extension->count(localClientNum, feeder);
			}

			return Game::UI_FeederCount(localClientNum, feeder);
		}

		return static_cast<int>(callbacks->getItemCount());
	}

	const char* UIFeeder::FeederItemText(int localClientNum, Game::itemDef_s* item, float feeder,
		int index, int column, float* a6, float* a7, float* a8, float* a9, Game::Material** material)
	{
		paintFeeder = feeder;

		const Callbacks* const callbacks = Find(feeder);

		if (!callbacks || !callbacks->getItemText)
		{
			const EngineCallbacks* const extension = FindExtension(feeder);

			if (extension && extension->text)
			{
				return extension->text(localClientNum, item, feeder, index, column,
					a6, a7, a8, a9, material);
			}

			return Game::UI_FeederItemText(localClientNum, item, feeder, index, column,
				a6, a7, a8, a9, material);
		}

		if (material)
		{
			*material = nullptr;
		}

		const char* const text = callbacks->getItemText(static_cast<unsigned int>(index), column);
		return text ? text : "";
	}

	void UIFeeder::FeederSelection(int localClientNum, float feeder, int index)
	{
		const Callbacks* const callbacks = Find(feeder);

		if (!callbacks || !callbacks->select)
		{
			Game::UI_FeederSelection(localClientNum, feeder, index);
			return;
		}

		callbacks->select(static_cast<unsigned int>(index));
	}

	void UIFeeder::ListBoxMouseMove(int localClientNum, Game::itemDef_s* item, float x, float y)
	{
		const bool isOurs = ShouldOverride(item->special);
		const int selected = item->cursorPos[localClientNum];

		hoverItem = item;
		hoverLocalClientNum = localClientNum;

		Utils::Hook::Call<void(int, Game::itemDef_s*, float, float)>(listBoxMouseMove)(
			localClientNum, item, x, y);

		hoverItem = nullptr;

		if (isOurs)
		{
			item->cursorPos[localClientNum] = selected;
		}
	}

	void UIFeeder::HoverSelection(int localClientNum, float feeder, int index)
	{
		if (ShouldOverride(feeder))
		{
			return;
		}

		Game::UI_FeederSelection(localClientNum, feeder, index);
	}

	int UIFeeder::MouseOverSound(int localClientNum, const char* alias, int system)
	{
		if (hoverItem && ShouldOverride(hoverItem->special))
		{
			return -1;
		}

		return Game::SND_PlayLocalSoundAliasByName(localClientNum, alias, system);
	}

	void UIFeeder::HandleKeyItem(int localClientNum, Game::itemDef_s* item)
	{
		keyItem = item;

		OverrideCursorPos(localClientNum, item);
	}

	int UIFeeder::FeederDoubleClick(int localClientNum, float feeder, int index)
	{
		if (!ShouldOverride(feeder))
		{
			const EngineCallbacks* const extension = FindExtension(feeder);

			if (extension && extension->doubleClick)
			{
				return extension->doubleClick(localClientNum, feeder, index);
			}

			return Game::UI_FeederDoubleClick(localClientNum, feeder, index);
		}

		const int now = Game::Sys_Milliseconds();

		if (now < nextClickTime)
		{
			return 0;
		}

		Game::itemDef_s* const item = keyItem;

		if (!item || item->special != feeder)
		{
			return 0;
		}

		Game::listBoxDef_s* const listBox = Game::Item_GetListBoxDef(item);

		if (!listBox)
		{
			return 0;
		}

		nextClickTime = now + clickIntervalMs;

		if (item->cursorPos[localClientNum] != listBox->mousePos)
		{
			item->cursorPos[localClientNum] = listBox->mousePos;

			const Callbacks* const callbacks = Find(feeder);

			if (callbacks && callbacks->select)
			{
				callbacks->select(static_cast<unsigned int>(listBox->mousePos));
			}
		}

		return 1;
	}

	void UIFeeder::OverrideCursorPos(int localClientNum, Game::itemDef_s* item)
	{
		const EngineCallbacks* const extension = FindExtension(item->special);

		if (!extension || !extension->count)
		{
			Game::UI_OverrideCursorPos(localClientNum, item);
			return;
		}

		const int count = extension->count(localClientNum, item->special);

		if (count <= 0)
		{
			item->cursorPos[localClientNum] = 0;
			return;
		}

		if (item->cursorPos[localClientNum] >= count)
		{
			item->cursorPos[localClientNum] = count - 1;
		}
	}

	void UIFeeder::PaintCursorPos(int localClientNum, Game::itemDef_s* item)
	{
		OverrideCursorPos(localClientNum, item);

		if (FindExtension(item->special))
		{
			FollowStackedList(localClientNum, item);
		}
	}

	void UIFeeder::FollowStackedList(int localClientNum, Game::itemDef_s* item)
	{
		if (!(item->window.staticFlags & Game::WINDOW_STATIC_DECORATION) || !item->parent)
		{
			return;
		}

		Game::listBoxDef_s* const listBox = Game::Item_GetListBoxDef(item);

		if (!listBox)
		{
			return;
		}

		const Game::menuDef_t* const menu = item->parent;
		const Game::rectDef_s& rect = item->window.rect;

		for (int i = 0; i < menu->itemCount; ++i)
		{
			Game::itemDef_s* const other = menu->items[i];

			if (!other || other == item || other->type != listBoxType)
			{
				continue;
			}

			if (other->window.staticFlags & Game::WINDOW_STATIC_DECORATION)
			{
				continue;
			}

			const Game::rectDef_s& otherRect = other->window.rect;
			const bool isStacked = otherRect.x == rect.x && otherRect.y == rect.y
				&& otherRect.w == rect.w && otherRect.h == rect.h;

			if (!isStacked)
			{
				continue;
			}

			const Game::listBoxDef_s* const leader = Game::Item_GetListBoxDef(other);

			if (leader)
			{
				listBox->startPos[localClientNum] = leader->startPos[localClientNum];
			}

			return;
		}
	}

	int UIFeeder::ListBoxHandleKey(Game::UiContext* context, Game::itemDef_s* item, int key, int down, int force)
	{
		const auto handleKey = Utils::Hook::Call<int(Game::UiContext*, Game::itemDef_s*, int, int, int)>(Item_ListBox_HandleKey);

		const bool isWheel = key == wheelDownKey || key == wheelUpKey;
		Game::listBoxDef_s* const listBox = Game::Item_GetListBoxDef(item);

		if (!isWheel || !listBox || !FindExtension(item->special))
		{
			return handleKey(context, item, key, down, force);
		}

		const int localClientNum = context->localClientNum;
		const int firstRowBefore = listBox->startPos[localClientNum];
		const int wasNotSelectable = listBox->notselectable;

		listBox->notselectable = 1;
		const int result = handleKey(context, item, key, down, force);
		listBox->notselectable = wasNotSelectable;

		const int scrolled = listBox->startPos[localClientNum] - firstRowBefore;

		if (scrolled != 0 && item->cursorPos[localClientNum] >= 0)
		{
			item->cursorPos[localClientNum] += scrolled;
			listBox->mousePos += scrolled;
		}

		return result;
	}

	int UIFeeder::HoverRowCount()
	{
		if (hoverItem)
		{
			const EngineCallbacks* const extension = FindExtension(hoverItem->special);

			if (extension && extension->count)
			{
				return extension->count(hoverLocalClientNum, hoverItem->special);
			}
		}

		return Game::GetLobbyMemberCount();
	}

	void UIFeeder::ItemColor(int localClientNum, Game::itemDef_s* item, float feeder,
		int index, int column, float* color)
	{
		const EngineCallbacks* const extension = FindExtension(feeder);

		if (extension && extension->color)
		{
			extension->color(localClientNum, item, feeder, index, column, color);
			return;
		}

		Game::UI_FeederItemColor(localClientNum, item, feeder, index, column, color);
	}

	void UIFeeder::BlinkColor(Game::UiContext* context, const float* color, float* out)
	{
		if (paintFeeder == 62.0f || paintFeeder == 13.0f)
		{
			return;
		}

		Game::GetBlinkColor(context, color, out);
	}

	UIFeeder::UIFeeder()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		bool isSeated = true;

		for (const auto site : feederCountCalls)
		{
			isSeated = SeatHook(site, FeederCount, HOOK_CALL) && isSeated;
		}

		for (const auto site : feederItemTextCalls)
		{
			isSeated = SeatHook(site, FeederItemText, HOOK_CALL) && isSeated;
		}

		for (const auto site : feederSelectionCalls)
		{
			isSeated = SeatHook(site, FeederSelection, HOOK_CALL) && isSeated;
		}

		for (const auto site : feederSelectionJumps)
		{
			isSeated = SeatHook(site, FeederSelection, HOOK_JUMP) && isSeated;
		}

		isSeated = SeatHook(listBoxMouseMoveCall, ListBoxMouseMove, HOOK_CALL) && isSeated;
		isSeated = SeatHook(hoverSelectionCall, HoverSelection, HOOK_CALL) && isSeated;
		isSeated = SeatHook(mouseOverSoundCall, MouseOverSound, HOOK_CALL) && isSeated;
		isSeated = SeatHook(handleKeyItemCall, HandleKeyItem, HOOK_CALL) && isSeated;
		isSeated = SeatHook(feederDoubleClickCall, FeederDoubleClick, HOOK_CALL) && isSeated;
		isSeated = SeatHook(blinkColorCall, BlinkColor, HOOK_CALL) && isSeated;
		isSeated = SeatHook(paintCursorPosCall, PaintCursorPos, HOOK_CALL) && isSeated;
		isSeated = SeatHook(hoverRowCountCall, HoverRowCount, HOOK_CALL) && isSeated;
		isSeated = SeatHook(itemColorCall, ItemColor, HOOK_CALL) && isSeated;
		isSeated = SeatHook(listBoxHandleKeyCall, ListBoxHandleKey, HOOK_CALL) && isSeated;

		if (!isSeated)
		{
			Logger::Error("uifeeder: not every call site could be redirected, listboxes fed from code will be wrong\n");
		}

		Utils::Hook::Nop(hasFocusGate, 6);
		Utils::Hook::Nop(overRowGate, 6);

		Utils::Hook::Set<std::uint8_t>(feeder2OverrideBranch, 0xEB);

		Events::OnDvarInit([]
		{
			ui_map_long = Dvar::Register("ui_map_long", "", Game::DVAR_NONE, "");
			ui_map_name = Dvar::Register("ui_map_name", "", Game::DVAR_NONE, "");
			ui_map_desc = Dvar::Register("ui_map_desc", "", Game::DVAR_NONE, "");
		});

		Add(mapFeeder, GetMapCount, GetMapText, SelectMap);
		UIScript::Add("ApplyMap", ApplyMap);
		UIScript::Add("ApplyInitialMap", ApplyInitialMap);
	}

	void UIFeeder::Select(const float feeder, const unsigned int index, const bool resetScroll)
	{
		if (Game::uiContext->openMenuCount <= 0)
		{
			return;
		}

		const auto* const menu = Game::uiContext->menuStack[Game::uiContext->openMenuCount - 1];

		if (!menu || !menu->items)
		{
			return;
		}

		for (int i = 0; i < menu->itemCount; ++i)
		{
			auto* const item = menu->items[i];

			if (item && item->type == listBoxType && item->special == feeder)
			{
				item->cursorPos[0] = static_cast<int>(index);

				auto* const listBox = Game::Item_GetListBoxDef(item);

				if (resetScroll && listBox)
				{
					listBox->startPos[0] = 0;
				}

				break;
			}
		}
	}

	unsigned int UIFeeder::GetMapCount()
	{
		return static_cast<unsigned int>(Maps::GetCustomMaps().size());
	}

	const char* UIFeeder::GetMapText(const unsigned int index, [[maybe_unused]] const int column)
	{
		const auto& maps = Maps::GetCustomMaps();

		if (index < maps.size())
		{
			return maps.at(index).data();
		}

		return "";
	}

	void UIFeeder::SelectMap(const unsigned int index)
	{
		const auto& maps = Maps::GetCustomMaps();

		if (index >= maps.size())
		{
			return;
		}

		std::string mapName = maps[index];
		std::string longName = mapName;
		std::string description = "(Missing arena file!)";

		const auto arenaPath = Maps::GetArenaPath(mapName);

		if (Utils::IO::FileExists(arenaPath))
		{
			const auto arena = Maps::ParseCustomMapArena(Utils::IO::ReadFile(arenaPath));

			if (arena.contains("longname"))
			{
				longName = arena.at("longname");
			}

			if (arena.contains("map"))
			{
				mapName = arena.at("map");
			}

			if (arena.contains("description"))
			{
				description = arena.at("description");
			}
		}

		ui_map_name.Set(Localization::Get(mapName.data()));
		ui_map_long.Set(Localization::Get(longName.data()));
		ui_map_desc.Set(Localization::Get(description.data()));
	}

	void UIFeeder::ApplyMap([[maybe_unused]] const UIScript::Token& token)
	{
		const auto mapname = ui_map_name.Get<std::string>();

		if (mapname.empty())
		{
			return;
		}

		Dvar::Var("ui_mapname").Set(mapname);
		reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Party_SetDisplayMapName))(mapname.data());
	}

	void UIFeeder::ApplyInitialMap([[maybe_unused]] const UIScript::Token& token)
	{
		Maps::ScanCustomMaps();

		Select(mapFeeder, 0);

		if (GetMapCount() > 0)
		{
			SelectMap(0);
		}
	}
}
