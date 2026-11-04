#include "Utils/MouseStartupDiagnostics.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>
#include <optional>

namespace Utils { struct String { static bool Compare(const char* a, const char* b) { return std::strcmp(a, b) == 0; } }; }

namespace Components
{
	using namespace std::chrono_literals;
	static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
	struct Entry { std::string id, audience; };
	static struct { std::mutex mutex; std::vector<Entry> entries; bool isLoaded = true; std::string selectedId; } playlistCatalog;
	static bool isPlaylistSelectorOpen = true, isPlaylistSelectionStarting = false;
	static std::size_t quickPlayFilterPage = 0;
	static constexpr std::size_t quickPlayFilterPageSize = 6;
	static int previewWrites = 0;
	static std::mutex stateMutex;
	static std::string pendingFailedMatchRelease;
	static std::optional<nlohmann::json> releaseResponse;
	static nlohmann::json releaseBody;
	static int releaseCalls = 0;
	struct ZWNet
	{
		static void HighlightQuickPlayFilterSlot(int);
		static void PublishPlaylistCatalog() { ++previewWrites; }
		static bool ReleaseFailedMatch();
		static std::optional<nlohmann::json> Request(const std::string& method, const std::string& path, const nlohmann::json& body)
		{
			Require(method == "POST" && path == "/zwnet/matchmaking/disconnect", "failed join used the wrong cleanup endpoint");
			++releaseCalls; releaseBody = body; return releaseResponse;
		}
	};
#include "ReleaseFailedMatch.inc"
#include "FilterIndices.inc"
#include "FilterHover.inc"

	static bool focused = true, loading = true, ready = false, dragging = false, nativeInputActive = false;
	static int activationMessages = 0, nativeFrames = 0, rawResets = 0, returns = 0, clockMs = 0;
	static constexpr int WM_ACTIVATE = 6, WA_ACTIVE = 1;
	static unsigned int threadId = 1, windowThreadId = 1;
	static int mainWindow = 1;
	static unsigned int GetCurrentThreadId() { return threadId; }
	static void SendMessageA(int, int, int, int) { ++activationMessages; nativeInputActive = true; }
	static std::uint64_t GetTickCount64() { return static_cast<std::uint64_t>(clockMs); }
	struct Window
	{
		static bool HasFocus() { return focused; }
		static bool IsLoadingScreenMovable() { return loading; }
		static bool IsDragging() { return dragging; }
		static void PumpLoadingEvents() {}
		static bool RefreshInputActivation();
	};
#include "InputActivation.inc"
	struct FastFiles { static bool MainMenuReady() { return ready; } };
	struct Game
	{
		struct Menu { struct { const char* name; } window; };
		struct Context { int openMenuCount = 0; Menu* menuStack[4]{}; };
		inline static Context* uiContext = nullptr;
		struct Mouse { bool mouseActive = false; };
		inline static Mouse mouse{};
		inline static Mouse* s_wmv = &mouse;
		static constexpr int KEYCATCH_UI = 2, CA_DISCONNECTED = 0;
		inline static bool cgame = true;
		inline static int connection = 9;
		static bool Key_IsCatcherActive(int, int) { return true; }
		static int Sys_Milliseconds() { return clockMs; }
		static bool CL_IsCgameInitialized(int) { return cgame; }
		static int CL_GetLocalClientConnectionState(int) { return connection; }
	};
#include "MenuReady.inc"
	static void NativeFrame() { ++nativeFrames; Game::mouse.mouseActive = nativeInputActive && focused; }
	static constexpr std::uintptr_t IN_Frame_Engine = 1;
	static bool inRawInput = false;
	static Utils::MouseStartupDiagnostics startupDiagnostics;
	static void SaveMouseStartupDiagnostics(bool) {}
	struct RawMouse
	{
		static void IN_Frame();
		static bool IsMouseInClientBounds() { return focused; }
		static void ToggleRawInput(bool value) { inRawInput = value; }
		static void SuspendMouseInput() { inRawInput = false; }
		static void ResetMouseRawEvents() { ++rawResets; }
	};
}
namespace Utils { struct Hook { static std::uintptr_t Rebase(std::uintptr_t) { return reinterpret_cast<std::uintptr_t>(&Components::NativeFrame); } }; }
namespace Components
{
#include "InputFrame.inc"
	static std::vector<std::function<bool()>> scheduled;
	struct Scheduler
	{
		enum class Pipeline { MAIN };
		static void Schedule(std::function<bool()> task, Pipeline, std::chrono::milliseconds) { scheduled.push_back(std::move(task)); }
	};
	static void RecreatePrivateMatchLobby() { ++returns; }
	struct Party { static void SchedulePrivateLobbyReturn(); };
#include "PrivateReturn.inc"
	static void Step() { auto tasks = std::move(scheduled); scheduled.clear(); for (auto& task : tasks) if (!task()) scheduled.push_back(std::move(task)); }
	static void Tests()
	{
		Require(ZWNet::ReleaseFailedMatch() && releaseCalls == 0, "ordinary Play Online sent a failed-match release");
		pendingFailedMatchRelease = "mat_rejected";
		Require(!ZWNet::ReleaseFailedMatch() && pendingFailedMatchRelease == "mat_rejected", "network failure lost the rejected assignment cleanup");
		Require(releaseBody.at("terminal") == true && releaseBody.at("preserve_party") == true && releaseBody.at("match_id") == "mat_rejected", "failed join remained resumable or discarded its party");
		releaseResponse = nlohmann::json{{"error", "UNAVAILABLE"}};
		Require(!ZWNet::ReleaseFailedMatch() && !pendingFailedMatchRelease.empty(), "rejected cleanup allowed online entry to resume");
		releaseResponse = nlohmann::json{{"ok", false}};
		Require(!ZWNet::ReleaseFailedMatch(), "application rejection allowed online entry to resume");
		releaseResponse = nlohmann::json{{"ok", true}};
		Require(ZWNet::ReleaseFailedMatch() && pendingFailedMatchRelease.empty(), "successful cleanup did not release the rejected match");
		const auto callsAfterRelease = releaseCalls;
		Require(ZWNet::ReleaseFailedMatch() && releaseCalls == callsAfterRelease, "successful cleanup was repeated for an unrelated later entry");
		Require(!IsMainMenuOpen(), "uninitialized UI was marked ready");
		Game::Context context;
		Game::uiContext = &context;
		Game::Menu title{{"main_text"}}, menu{{"pregame_loaderror"}}, loadingMenu{{"connect"}};
		context.openMenuCount = 1; context.menuStack[0] = &loadingMenu;
		Require(!IsMainMenuOpen(), "connect screen ended startup loading");
		context.menuStack[0] = &title;
		Require(IsMainMenuOpen(), "intro title readiness regressed");
		context.menuStack[0] = &menu;
		Require(IsMainMenuOpen(), "intro 0 main menu without main_text left mouse movement suspended");
		Game::uiContext = nullptr;
		playlistCatalog.entries = {{"restricted", "RESTRICTED"}};
		for (int i = 0; i < 8; ++i) playlistCatalog.entries.push_back({"public" + std::to_string(i), "PUBLIC"});
		ZWNet::HighlightQuickPlayFilterSlot(0);
		Require(playlistCatalog.selectedId == "public0" && previewWrites == 1, "filter hover used a restricted Find Match row instead of the public filter row");
		quickPlayFilterPage = 1; ZWNet::HighlightQuickPlayFilterSlot(1);
		Require(playlistCatalog.selectedId == "public7", "filter hover ignored pagination");
		ZWNet::HighlightQuickPlayFilterSlot(2); ZWNet::HighlightQuickPlayFilterSlot(-1);
		Require(previewWrites == 2, "hidden or invalid filter slots changed details");
		isPlaylistSelectionStarting = true; ZWNet::HighlightQuickPlayFilterSlot(0);
		Require(previewWrites == 2, "in-flight search preview could be changed");

		// intro 0 can reach the menu before the first hooked input frame. It
		// still needs activation even when no movable loading frame was seen.
		loading = false; ready = true; RawMouse::IN_Frame();
		Require(activationMessages == 1 && Game::mouse.mouseActive, "intro 0 startup without a loading frame left native input inactive");
		RawMouse::IN_Frame();
		Require(activationMessages == 1, "intro 0 repeatedly reactivated input");
		loading = true; ready = false; RawMouse::IN_Frame();
		Require(activationMessages == 1 && nativeFrames == 2, "loading captured the mouse before the frontend was ready");
		loading = false; ready = true; RawMouse::IN_Frame();
		Require(activationMessages == 2 && Game::mouse.mouseActive && rawResets == 2, "focus retained during loading did not activate the native mouse at menu readiness");
		RawMouse::IN_Frame();
		Require(activationMessages == 2, "normal frames repeatedly reactivated input");
		loading = true; RawMouse::IN_Frame(); focused = false; loading = false; RawMouse::IN_Frame();
		Require(activationMessages == 2, "finishing loading while unfocused stole input activation");
		focused = true; RawMouse::IN_Frame();
		Require(activationMessages == 3, "deferred activation was lost when startup finished while unfocused");
		loading = true; RawMouse::IN_Frame(); loading = false; threadId = 2; RawMouse::IN_Frame();
		Require(activationMessages == 3, "activation was applied from the wrong window thread");
		threadId = 1; RawMouse::IN_Frame();
		Require(activationMessages == 4, "device/load recovery activation was lost on an earlier non-window thread frame");

		Party::SchedulePrivateLobbyReturn(); Party::SchedulePrivateLobbyReturn(); Step();
		Require(scheduled.size() == 1 && returns == 0, "private save quit and console callback scheduled competing lobby returns");
		Game::cgame = false; Game::connection = 0; clockMs = 100; Step(); clockMs = 250; Step();
		Require(returns == 1 && scheduled.empty(), "private host disconnect did not wait for cleanup and restore its lobby");
		Party::SchedulePrivateLobbyReturn(); Game::connection = 3; Step();
		Require(returns == 1 && scheduled.empty(), "late private return replaced a newly started connection");
		Game::cgame = true; Party::SchedulePrivateLobbyReturn(); clockMs = 11000; Step();
		Require(scheduled.empty(), "failed disconnect left an unbounded private lobby return");
	}
}
int main()
{
	try { Components::Tests(); std::cout << "Production filter hover, startup activation and private disconnect recovery scenarios passed\n"; return 0; }
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
