#pragma once

#include "Dvar.hpp"
#include "UIScript.hpp"

namespace Components
{
	class UIFeeder : public Component
	{
	public:
		using GetItemCount = std::function<unsigned int()>;
		using GetItemText = std::function<const char*(unsigned int index, int column)>;
		using SelectItem = std::function<void(unsigned int index)>;

		struct Callbacks
		{
			GetItemCount getItemCount;
			GetItemText getItemText;
			SelectItem select;
		};

		struct EngineCallbacks
		{
			Game::UI_FeederCount_t count;
			Game::UI_FeederItemText_t text;
			Game::UI_FeederItemColor_t color;
			Game::UI_FeederDoubleClick_t doubleClick;
		};

		UIFeeder();

		static void Add(float feeder, GetItemCount count, GetItemText text, SelectItem select);

		static void Extend(float feeder, const EngineCallbacks& callbacks);

		static bool Has(float feeder);

		static void Select(float feeder, unsigned int index, bool resetScroll = false);

	private:
		static std::unordered_map<float, Callbacks> feeders;
		static std::unordered_map<float, EngineCallbacks> extensions;

		static Dvar::Var ui_map_long;
		static Dvar::Var ui_map_name;
		static Dvar::Var ui_map_desc;

		static unsigned int GetMapCount();
		static const char* GetMapText(unsigned int index, int column);
		static void SelectMap(unsigned int index);
		static void ApplyMap(const UIScript::Token& token);
		static void ApplyInitialMap(const UIScript::Token& token);

		static Callbacks* Find(float feeder);
		static EngineCallbacks* FindExtension(float feeder);

		static bool ShouldOverride(float feeder);

		static int FeederCount(int localClientNum, float feeder);

		static const char* FeederItemText(int localClientNum, Game::itemDef_s* item, float feeder,
			int index, int column, float* a6, float* a7, float* a8, float* a9, Game::Material** material);

		static void FeederSelection(int localClientNum, float feeder, int index);

		static void ListBoxMouseMove(int localClientNum, Game::itemDef_s* item, float x, float y);

		static void HoverSelection(int localClientNum, float feeder, int index);

		static int MouseOverSound(int localClientNum, const char* alias, int system);

		static void HandleKeyItem(int localClientNum, Game::itemDef_s* item);
		static int FeederDoubleClick(int localClientNum, float feeder, int index);

		static void OverrideCursorPos(int localClientNum, Game::itemDef_s* item);
		static void PaintCursorPos(int localClientNum, Game::itemDef_s* item);
		static void FollowStackedList(int localClientNum, Game::itemDef_s* item);
		static int HoverRowCount();
		static int ListBoxHandleKey(Game::UiContext* context, Game::itemDef_s* item, int key, int down, int force);
		static void ItemColor(int localClientNum, Game::itemDef_s* item, float feeder,
			int index, int column, float* color);

		static void BlinkColor(Game::UiContext* context, const float* color, float* out);
	};
}
