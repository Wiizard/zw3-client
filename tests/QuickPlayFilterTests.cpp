#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Components
{
	using Value = std::variant<std::string, bool, int>;
	static std::unordered_map<std::string, Value> values;
	struct Dvar
	{
		struct Var
		{
			std::string name;
			explicit Var(const std::string& value) : name(value) {}
			void Set(const std::string& value) { values[name] = value; }
			void Set(const char* value) { values[name] = std::string(value); }
			void Set(bool value) { values[name] = value; }
			void Set(int value) { values[name] = value; }
			template<class T> T Get() { const auto entry = values.find(name); return entry == values.end() ? T{} : std::get<T>(entry->second); }
		};
	};
	struct Entry { std::string id, name, audience, availability; };
	static struct { std::mutex mutex; std::vector<Entry> entries; bool isLoaded = true; std::string selectedId; } playlistCatalog;
	static bool isPlaylistSelectorOpen = true, isPlaylistSelectionStarting = false;
	static std::string actionError;
	static void SetPlaylistActionError(const std::string& value) { actionError = value; }
	struct ZWNet
	{
		static void HighlightQuickPlayFilterSlot(int);
		static void ToggleQuickPlayFilterSlot(int);
		static void SelectAllQuickPlayFilter();
		static void ChangeQuickPlayFilterPage(int);
		static void PublishPlaylistCatalog() {}
	};
#include "QuickPlayFilterState.inc"
#include "QuickPlayFilterActions.inc"
}

int main()
{
	using namespace Components;
	for (int i = 0; i < 8; ++i) playlistCatalog.entries.push_back({ "public-" + std::to_string(i), "Playlist " + std::to_string(i), "PUBLIC", i == 1 ? "NO_CAPACITY" : "AVAILABLE" });
	playlistCatalog.entries.push_back({ "private-dev", "Dev", "RESTRICTED", "AVAILABLE" });
	values["zwnet_quickplay_excluded"] = std::string("public-1,public-3");
	LoadQuickPlayFilterLocked(); PublishQuickPlayFilterLocked();
	assert(QuickPlayFilterIndicesLocked().size() == 8);
	assert(std::get<int>(values["zwnet_qpf_pages"]) == 2);
	assert(!std::get<bool>(values["zwnet_qpf_slot_1_enabled"]));
	assert(std::get<std::string>(values["zwnet_qpf_slot_1_status"]) == "FULL");
	assert(std::get<std::string>(values["zwnet_qpf_count_text"]) == "6 OF 8 SELECTED");
	ZWNet::ToggleQuickPlayFilterSlot(1); assert(!quickPlayExcluded.contains("public-1"));
	ZWNet::ToggleQuickPlayFilterSlot(1); assert(quickPlayExcluded.contains("public-1"));
	ZWNet::ChangeQuickPlayFilterPage(1); ZWNet::HighlightQuickPlayFilterSlot(1);
	assert(playlistCatalog.selectedId == "public-7");
	PublishQuickPlayFilterLocked(); assert(!std::get<bool>(values["zwnet_qpf_slot_2_visible"]));
	ZWNet::ChangeQuickPlayFilterPage(1); assert(quickPlayFilterPage == 1);
	ZWNet::ChangeQuickPlayFilterPage(-1); assert(quickPlayFilterPage == 0);
	ZWNet::SelectAllQuickPlayFilter(); assert(quickPlayExcluded.empty());
	assert(std::get<std::string>(values["zwnet_quickplay_excluded"]).empty());
	for (int i = 1; i < 8; ++i) quickPlayExcluded.insert("public-" + std::to_string(i));
	ZWNet::ToggleQuickPlayFilterSlot(0); assert(!actionError.empty()); assert(!quickPlayExcluded.contains("public-0"));
	isPlaylistSelectionStarting = true; ZWNet::ToggleQuickPlayFilterSlot(1); assert(quickPlayExcluded.contains("public-1"));
	isPlaylistSelectionStarting = false; isPlaylistSelectorOpen = false; ZWNet::SelectAllQuickPlayFilter(); assert(quickPlayExcluded.size() == 7);
	isPlaylistSelectorOpen = true; ZWNet::SelectAllQuickPlayFilter(); assert(quickPlayExcluded.empty());
	ZWNet::ToggleQuickPlayFilterSlot(-1); ZWNet::ToggleQuickPlayFilterSlot(6); assert(quickPlayExcluded.empty());
}
