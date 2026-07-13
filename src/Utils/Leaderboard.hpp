#pragma once

namespace Components
{
	class Leaderboard : public Component
	{
	public:
		Leaderboard();
		~Leaderboard();
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

		static constexpr float FeederId = 70.0f;
		static constexpr int DefaultLimit = 10;

		static std::vector<Entry> Entries;
		static Dvar::Var UILeaderboardMap;
		static Dvar::Var UILeaderboardPage;
		static Dvar::Var UILeaderboardLoadingIndicator;
		static Dvar::Var UILeaderboardPlayerStatus;
		static Dvar::Var UIMapNameDisplay;
		static Dvar::Var UILeaderboardCanPrev;
		static Dvar::Var UILeaderboardCanNext;
		static int CurrentOffset;
		static int NextOffset;
		static int TotalItems;
		static bool HasNextPage;
		static bool Loading;
		static unsigned int RequestSerial;
		static int LoadingFrame;
		static std::string CurrentMap;
		static int LastKnownRank;
		static bool IsSearching;
		static int DisplayedOffset;

		static void UpdatePageDvar();
		static void UpdateButtonDvars();
		static void UpdateLocalPlayerStatus();
		static void UpdateMapDisplayDvar(const std::string& rawMap);
		static std::string GetCurrentMapName();
		static std::string UrlEncode(const std::string& value);
		static void StartRefresh(int offset);
		static void UpdateLoadingStatus();
		static void Refresh([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void RefreshFirstPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void PreviousPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void NextPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void FetchRankBackground(int offset);
		static void ParseResponse(const std::string& response);

		static unsigned int GetEntryCount();
		static const char* GetEntryText(unsigned int index, int column);
		static void SelectEntry(unsigned int index);
	};
}
