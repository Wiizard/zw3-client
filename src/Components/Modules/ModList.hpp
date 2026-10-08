#pragma once

#include "Dvar.hpp"
#include "UIScript.hpp"

namespace Components
{
	class ModList : public Component
	{
	public:
		ModList();

		static Dvar::Var cl_modVidRestart;

		static void RunMod(const std::string& mod);

	private:
		static std::vector<std::string> mods;
		static unsigned int currentMod;

		static bool HasMod(const std::string& modName);

		static void ClearMods();

		static char* StructuredData_GetString(Game::StructuredDataLookup* lookup, Game::StructuredDataBuffer* buffer);

		static unsigned int GetItemCount();
		static const char* GetItemText(unsigned int index, int column);
		static void Select(unsigned int index);
		static void UIScript_LoadMods(const UIScript::Token& token);
		static void UIScript_RunMod(const UIScript::Token& token);
		static void UIScript_ClearMods(const UIScript::Token& token);
	};
}
