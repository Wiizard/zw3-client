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
			int id = 0;
		};

		static constexpr float FeederId = 70.0f;
		static constexpr int DefaultPage = 1;
		static constexpr int DefaultPerPage = 50;
		static constexpr int DefaultPeriod = 30;

		static std::vector<Entry> Entries;
		static Dvar::Var UILeaderboardStatus;
		static Dvar::Var UILeaderboardFirst;
		static Dvar::Var UILeaderboardMap;
		static Dvar::Var UILeaderboardPage;
		static int CurrentPage;
		static bool HasNextPage;

		static bool IsBetterEntry(const Entry& candidate, const Entry& current);
		static void UpdatePageDvar();
		static std::string GetCurrentMapName();
		static std::string UrlEncode(const std::string& value);
		static void Refresh([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info);
		static void RefreshFirstPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info);
		static void PreviousPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info);
		static void NextPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info);
		static void ParseResponse(const std::string& response, const std::string& mapName);

		static unsigned int GetEntryCount();
		static const char* GetEntryText(unsigned int index, int column);
		static void SelectEntry(unsigned int index);
	};
}
