#pragma once

#include "Dvar.hpp"
#include "UIScript.hpp"

namespace Components
{
	class Leaderboard : public Component
	{
	public:
		Leaderboard();

		static const char* GetApiKey();

	private:
		struct Entry
		{
			std::string guid;
			std::string player;
			std::string map;
			int round = 0;
			std::string zombiemode;
			int players = 0;
			std::string playerRank;
			int score = 0;
			int kills = 0;
			int downs = 0;
			int revives = 0;
			int exfiltrated = 0;
			float time = 0.0f;
			std::string version;
			std::string uploadedAt;
		};

		static std::vector<Entry> entries;

		static Dvar::Var zw3_leaderboard_map;
		static Dvar::Var zw3_leaderboard_page;
		static Dvar::Var zw3_leaderboard_player_status;
		static Dvar::Var zw3_leaderboard_mapname_display;
		static Dvar::Var zw3_leaderboard_can_prev;
		static Dvar::Var zw3_leaderboard_can_next;

		static int currentOffset;
		static int displayedOffset;
		static int nextOffset;
		static int totalItems;
		static bool hasNextPage;
		static bool isLoading;
		static unsigned int requestSerial;
		static std::string currentMap;
		static int lastKnownRank;
		static bool isSearching;

		static void UpdatePageDvar();
		static void UpdateButtonDvars();
		static void UpdateLocalPlayerStatus();
		static void UpdateMapDisplayDvar(const std::string& rawMap);
		static std::string GetCurrentMapName();
		static void StartRefresh(int offset);
		static void RefreshFirstPage(const UIScript::Token& token);
		static void PreviousPage(const UIScript::Token& token);
		static void NextPage(const UIScript::Token& token);
		static void FetchRankBackground(int offset);
		static void ParseResponse(const std::string& response);

		static unsigned int GetEntryCount();
		static const char* GetEntryText(unsigned int index, int column);
		static void SelectEntry(unsigned int index);
	};
}
