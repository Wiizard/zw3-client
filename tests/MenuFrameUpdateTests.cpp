#include "Utils/StutterDiagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <variant>

static int writes = 0, relinks = 0, newsUpdates = 0, motdReads = 0, hints = 0, scoreboardResets = 0;
struct Variable
{
	std::variant<int, float, bool, std::string> value;
	template <typename T> T Get() const { return std::get<T>(value); }
	template <typename T> void Set(T next) { value = next; ++writes; }
};
static Variable mapname{std::string("mp_zombie_farm")};
static Variable zw3_ui_loading_visible{false}, zw3_ui_loading_progress{0.0f}, zw3_ui_loading_start_time{0};
static Variable zw3_ui_news_index{0}, zw3_ui_news_progress{0.0f}, zw3_ui_news_hover{false};
static std::mutex loadingMutex;
static int newsElapsedMs = 0, lastNewsUpdate = 0, holdNewsUntil = 0;
static bool wasNewsHovered = false;
struct NewsItem { int durationMs = 3000; };
static std::vector<NewsItem> newsItems{NewsItem{}};
static void ApplyNewsItem() { ++newsUpdates; }
static float EaseOutCubic(float value) { return 1.0f - std::pow(1.0f - value, 3.0f); }
static float EaseOutQuad(float value) { return 1.0f - (1.0f - value) * (1.0f - value); }
namespace Game
{
	enum connstate_t { CA_DISCONNECTED, CA_CONNECTING, CA_LOADING, CA_PRIMED, CA_ACTIVE };
	static connstate_t state = CA_ACTIVE;
	static bool clientInitialized = true, connectVisible = false;
	static int now = 100;
	struct UiContext {};
	struct menuDef_t {};
	struct StringTable { int rowCount = 1; };
	static UiContext context;
	static UiContext* uiContext = &context;
	static int serverAddress;
	static int* clc_serverAddress = &serverAddress;
	static bool CL_IsCgameInitialized(int) { return clientInitialized; }
	static connstate_t CL_GetLocalClientConnectionState(int) { return state; }
	static int Sys_Milliseconds() { return now; }
	static menuDef_t* Menus_FindByName(UiContext*, const char*) { static menuDef_t connect; return &connect; }
	static bool Menu_IsVisible(UiContext*, menuDef_t*) { return connectVisible; }
	static void StringTable_GetAsset(const char*, const StringTable** out) { static StringTable table; *out = &table; }
	static const char* StringTable_GetColumnValueForRow(const StringTable*, int, int) { return "test hint"; }
	static void Dvar_SetFromStringByName(const char* name, const char*)
	{
		++writes;
		if (std::string(name) == "didyouknow") ++hints;
		if (std::string(name) == "zw3_ui_sb_survived_time") ++scoreboardResets;
	}
}
namespace LobbyScene { static bool IsTransitionActive() { return false; } }
namespace FastFiles { static float GetFullLoadedFraction() { return 0.5f; } }
namespace Network
{
	struct Address
	{
		explicit Address(int*) {}
		bool operator==(const Address&) const { return true; }
	};
}
namespace Party
{
	static std::string GetMotd() { ++motdReads; return {}; }
	static Network::Address Target() { return Network::Address(nullptr); }
}
struct Menus
{
	static void UpdateLoadingProgress(bool);
	static void ForceOnlyCustomConnectMenu() { ++relinks; }
};

#include "MenuFrameUpdates.inc"

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}
static void Frame()
{
	Game::now += 16;
	Menus::UpdateLoadingProgress(true);
	Menus::UpdateLoadingProgress(false);
	UpdateNewsCarousel();
}

int main()
{
	try
	{
		for (int i = 0; i < 1000; ++i) Frame();
		Require(writes == 0 && relinks == 0 && motdReads == 0 && newsUpdates == 0,
			"Stable gameplay still rewrites loading/news UI or relinks connect menus");

		Game::state = Game::CA_DISCONNECTED;
		Game::clientInitialized = false;
		Frame();
		Require(newsUpdates > 0 && lastNewsUpdate == Game::now, "Returning to the frontend did not resume news");

		Game::state = Game::CA_CONNECTING;
		Game::connectVisible = true;
		Frame();
		Require(zw3_ui_loading_visible.Get<bool>() && relinks > 0 && scoreboardResets > 0,
			"Connecting did not start the custom loading screen");
		Game::state = Game::CA_LOADING;
		Frame();
		Require(zw3_ui_loading_progress.Get<float>() > 0.0f, "Map loading did not advance progress");
		Game::state = Game::CA_PRIMED;
		Frame();
		Game::state = Game::CA_ACTIVE;
		Game::clientInitialized = true;
		Frame();
		Require(zw3_ui_loading_visible.Get<bool>(), "A still-visible connect screen was hidden prematurely");
		Game::connectVisible = false;
		Frame();
		Require(!zw3_ui_loading_visible.Get<bool>() && zw3_ui_loading_progress.Get<float>() == 0.0f,
			"Entering gameplay left loading progress visible");
		const int beforeWrites = writes, beforeRelinks = relinks, beforeMotd = motdReads, beforeNews = newsUpdates;
		for (int i = 0; i < 1000; ++i) Frame();
		Require(writes == beforeWrites && relinks == beforeRelinks && motdReads == beforeMotd && newsUpdates == beforeNews,
			"Settled gameplay resumed frontend work");

		const auto resets = scoreboardResets;
		Game::state = Game::CA_LOADING;
		Game::clientInitialized = false;
		Game::connectVisible = true;
		Frame();
		Require(scoreboardResets > resets && zw3_ui_loading_visible.Get<bool>(), "Same-map restart lost its loading transition");
		Game::state = Game::CA_ACTIVE;
		Game::clientInitialized = true;
		Game::connectVisible = false;
		Frame();
		Require(!zw3_ui_loading_visible.Get<bool>(), "Restart did not complete");
		Game::state = Game::CA_DISCONNECTED;
		Game::clientInitialized = false;
		const auto updates = newsUpdates;
		Frame();
		Require(newsUpdates > updates, "News did not resume after disconnect");
		std::cout << "Menu frame updates: idle gameplay, connect/loading, active connect screen, restart and frontend return passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
