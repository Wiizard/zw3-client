#pragma once

#include "Dvar.hpp"
#include "UIScript.hpp"

namespace Components
{
	class ZW3Changelog : public Component
	{
	public:
		ZW3Changelog();

	private:
		struct Entry
		{
			std::string version;
			std::string title;
			std::string date;
			std::vector<std::string> lines;
		};

		static std::vector<Entry> entries;
		static std::size_t selectedIndex;

		static Dvar::Var zw3_changelog_patch_title;
		static Dvar::Var zw3_changelog_patch_date;

		static void Fetch(const UIScript::Token& token);
		static std::vector<Entry> ParseYamlEntries(const std::string& yaml);
		static std::string FormatDate(const std::string& date);
		static void SetEntries(std::vector<Entry> newEntries);
		static void ShowSelectedTitle();

		static unsigned int GetVersionCount();
		static const char* GetVersionText(unsigned int item, int column);
		static void SelectVersion(unsigned int index);

		static unsigned int GetDetailCount();
		static const char* GetDetailText(unsigned int item, int column);
		static void SelectDetail(unsigned int index);
	};
}
