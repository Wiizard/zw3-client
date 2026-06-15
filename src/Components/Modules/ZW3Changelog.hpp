#pragma once

namespace Components
{
	class ZW3Changelog : public Component
	{
	public:
		ZW3Changelog();

	private:
		static constexpr const char* ChangelogUrl = "https://stats.zw3.eu/client/changelog.yaml";

		struct Entry
		{
			std::string Version;
			std::string Title;
			std::string Date;
			std::vector<std::string> Lines;
		};

		static std::mutex Mutex;
		static std::vector<Entry> Entries;
		static std::size_t SelectedIndex;

		static void Fetch(const UIScript::Token& token, const Game::uiInfo_s* info);
		static std::vector<Entry> ParseYamlEntries(const std::string& yaml);
		static void SetEntries(std::vector<Entry> entries);

		static unsigned int GetVersionCount();
		static const char* GetVersionText(unsigned int item, int column);
		static void SelectVersion(unsigned int index);

		static unsigned int GetDetailCount();
		static const char* GetDetailText(unsigned int item, int column);
		static void SelectDetail(unsigned int index);
	};
}
