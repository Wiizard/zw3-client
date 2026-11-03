#include "Utils/QuickPlay.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

// Exercise production request/transition functions with controllable HTTP and
// main-thread scheduling, including cancellation while requests are in flight.
namespace Components
{
	using Json = nlohmann::json;
	using namespace std::chrono_literals;
	static bool isActive, isInGame, isClosingOnlineSession, isEndpointJoinInFlight, isServerJoinTransition;
	static bool isCompletedMatchReturnPending;
	static std::atomic_bool isSearching, isPlaylistSearchStarting, isPlaylistSelectionStarting, isPlaylistSelectorOpen;
	static std::atomic_uint64_t playlistSelectorGeneration, joinTransitionGeneration, lobbySnapshotGeneration;
	static std::mutex stateMutex;
	static std::string currentPlayerId, currentPartyId, currentMatchId, currentProposalId, reportedState, reportedError;
	static std::atomic_int desiredPartyPrivacy;
	static int refreshes, rosterWrites, clears, connections;
	static std::vector<bool> lobbyReturns;
	static std::vector<std::function<void()>> mainTasks;
	static std::vector<std::function<bool()>> scheduledTasks;
	struct Call { std::string method, path; Json body; };
	static std::vector<Call> calls;
	static std::function<void(const std::string&)> requestHook;
	static std::function<void()> refreshHook;
	static std::optional<Json> partyResponse, searchResponse, statusResponse, disconnectResponse;
	static bool failRefresh;
	static const char* clientVersion = "3.0.3";
	static const char* modVersion = "3.0.3";
	struct ClientPlaylist
	{
		std::string id, name, availability, audience, zombieSettingsSummary;
		std::vector<std::string> requiredContent, verifiedContent;
		int maxPlayers = 4;
		std::int64_t revision = 1;
	};
	struct Catalog
	{
		std::mutex mutex;
		std::vector<ClientPlaylist> entries, pendingEntries;
		bool isLoaded = true, isStale = false, hasPending = false;
	};
	static Catalog playlistCatalog;
	static std::vector<ClientPlaylist> refreshedEntries;
	template<class F> struct ScopeExit { F f; explicit ScopeExit(F value) : f(value) {} ~ScopeExit() { f(); } };
	struct Scheduler
	{
		enum class Pipeline { MAIN };
		static void Once(std::function<void()> task, Pipeline) { mainTasks.push_back(std::move(task)); }
		static void Schedule(std::function<bool()> task, Pipeline, std::chrono::milliseconds) { scheduledTasks.push_back(std::move(task)); }
	};
	static std::unordered_map<std::string, Json> dvars;
	struct Dvar
	{
		struct Var
		{
			std::string name;
			explicit Var(std::string value) : name(std::move(value)) {}
			template<class T> void Set(T value) { dvars[name] = value; }
			template<class T> T Get() { return dvars.contains(name) ? dvars.at(name).get<T>() : T{}; }
		};
	};
	struct Command { static void Execute(const char*, bool) {} };
	struct Logger { template<class... T> static void Print(const char*, T...) {} };
	struct Maps { static void SynchronizeMapDvars(const std::string&) {} };
	struct Game
	{
		struct menuDef_t { std::string name; bool visible = false; };
		inline static std::unordered_map<std::string, menuDef_t> menus;
		inline static int context, closeAllCalls, nativeState;
		inline static bool cgame;
		inline static int* uiContext = &context;
		static constexpr int CA_DISCONNECTED = 0;
		static int Sys_Milliseconds() { return 1000; }
		static bool CL_IsCgameInitialized(int) { return cgame; }
		static int CL_GetLocalClientConnectionState(int) { return nativeState; }
		static menuDef_t* Menus_FindByName(int*, const char* name) { auto& menu = menus[name]; menu.name = name; return &menu; }
		static bool Menu_IsVisible(int*, menuDef_t* menu) { return menu->visible; }
		static void Menus_CloseRequest(int*, menuDef_t* menu) { menu->visible = false; }
		static void Menus_CloseAll(int*) { ++closeAllCalls; for (auto& [name, menu] : menus) menu.visible = false; }
		static void Menus_OpenByName(int*, const char* name)
		{
			menus[name].visible = true;
			if (std::string_view(name) == "zwnet_matchmaking") lobbyReturns.push_back(false);
			if (std::string_view(name) == "popup_zwnet_playlists") lobbyReturns.push_back(true);
		}
		static void Dvar_SetFromStringByName(const char*, const char*) {}
	};
	struct Auth { static void ClearManagedConnectTicket() {} };
	static std::string JsonString(const Json& value, const char* key, const std::string& fallback = {}) { return value.value(key, fallback); }
	static std::string ResponseErrorCode(const Json& value) { return value.contains("error") ? value.at("error").value("code", "") : ""; }
	static bool IsCurrentPlaylistActivation(std::uint64_t generation) { return isActive && (isPlaylistSelectorOpen || isPlaylistSelectionStarting || isPlaylistSearchStarting) && generation == playlistSelectorGeneration; }
	static bool IsActiveMatchmakingState(const std::string& value) { return value == "SEARCHING" || value == "CONNECTING" || value == "MAP_VOTE"; }
	static const char* PartyVisibilityName(int) { return "OPEN"; }
	static void ResetManagedRoute() { ++joinTransitionGeneration; }
	static void ResetMatchLobbySoundSnapshot() {}
	static void CancelRelayHandshake() {}
	static bool IsRelayUsedForMatch(const std::string&) { return false; }
	static bool IsPartyMemberReady(const Json& member) { return member.value("ready", false); }
	static void SetDisplayText(const char* name, const std::string& value) { Dvar::Var(name).Set(value); }
	static std::string ResolveVoteMapDisplayName(const std::string&, const std::string& name) { return name; }
	static std::string ResolveVoteMapImage(const std::string&, const std::string& image) { return image; }
	static void ServerStatusText(const std::string&, int, std::string&, std::string&) {}
	struct ZWNet
	{
		static void StartQuickPlay(std::string, std::int64_t, std::uint64_t, bool);
		static void CancelPlaylistSelection();
		static void CancelPendingJoinPreparation();
		static void CancelMatchmaking(bool openFindMatch = true);
		static void UpdateMatchmaking();
		static std::optional<Json> Request(const std::string& method, const std::string& path, const Json& body = {})
		{
			calls.push_back({ method, path, body });
			if (requestHook) requestHook(path);
			if (path == "/zwnet/matchmaking/search") return searchResponse;
			if (path == "/zwnet/matchmaking/status") return statusResponse;
			if (path == "/zwnet/matchmaking/disconnect") return disconnectResponse;
			return partyResponse;
		}
		static void RefreshPlaylistCatalog(bool)
		{
			++refreshes;
			playlistCatalog.isStale = failRefresh;
			if (!failRefresh) { playlistCatalog.entries = refreshedEntries; playlistCatalog.isLoaded = true; }
			if (refreshHook) refreshHook();
		}
		static void SetState(const std::string& value, const std::string& message = {}) { reportedState = value; reportedError = message; }
		static void SetPlaylistActionError(const std::string&) {}
		static void PublishPlaylistCatalog() {}
		static void SchedulePlaylistCatalogPublish() {}
		static void CancelJoinInProgressPreview() {}
		static void ClearMatchDvars() { ++clears; }
		static void ScheduleMatchmakingLobby(bool popup);
		static std::optional<Json> ApplyPartyVisibility(Json party) { return party; }
		static Json PublishLocalRank(Json party) { return party; }
		static void UpdateLobbyDvars(const Json&) { ++rosterWrites; }
		static void UpdateMatchLobbyDvars(const Json&) { ++rosterWrites; }
		static void UpdateVoteDvars(const Json&) {}
		static void UpdatePresence() {}
		static bool ReturnToMatchmakingLobby() { return false; }
		static void BeginJoinInProgressPreview(const Json&) { ++connections; }
		static void ConnectMatch(const std::string&, bool, bool, std::uint64_t) { ++connections; }
	};
#include "StartQuickPlay.inc"
#include "CancelSelection.inc"
#include "CancelPreparation.inc"
#include "CancelMatchmaking.inc"
#include "ReturnLobby.inc"
#include "UpdateMatchmaking.inc"
	static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
	static int Count(const std::string& path) { return static_cast<int>(std::ranges::count_if(calls, [&](const Call& call) { return call.path == path; })); }
	static void Drain()
	{
		auto tasks = std::move(mainTasks); mainTasks.clear(); for (auto& task : tasks) task();
		auto scheduled = std::move(scheduledTasks); scheduledTasks.clear();
		for (auto& task : scheduled) if (!task()) scheduledTasks.push_back(std::move(task));
	}
	static void Reset()
	{
		isActive = true;
		isInGame = isClosingOnlineSession = isEndpointJoinInFlight = isServerJoinTransition = isCompletedMatchReturnPending = false;
		isSearching = isPlaylistSearchStarting = false;
		isPlaylistSelectionStarting = isPlaylistSelectorOpen = true;
		playlistSelectorGeneration = joinTransitionGeneration = lobbySnapshotGeneration = 0;
		currentPlayerId = "local"; currentPartyId = "pty_test"; currentMatchId.clear(); currentProposalId.clear(); reportedState.clear(); reportedError.clear();
		calls.clear(); mainTasks.clear(); dvars.clear(); lobbyReturns.clear(); requestHook = {}; refreshHook = {};
		scheduledTasks.clear(); Game::menus.clear(); Game::menus["zwnet_matchmaking"].visible = true;
		Game::cgame = false; Game::nativeState = Game::CA_DISCONNECTED; Game::closeAllCalls = 0;
		failRefresh = false; refreshes = rosterWrites = clears = connections = 0;
		refreshedEntries = { { "public", "Zombies", "AVAILABLE", "PUBLIC", "", { "base" }, { "base" }, 4, 1 } };
		playlistCatalog.entries = refreshedEntries; playlistCatalog.pendingEntries.clear();
		playlistCatalog.isLoaded = true; playlistCatalog.isStale = playlistCatalog.hasPending = false;
		partyResponse = Json{ { "id", "pty_test" }, { "leader_id", "local" }, { "state", "IDLE" }, { "selected_playlist_id", "public" }, { "selected_playlist_revision", 1 }, { "members", Json::array({ Json{{ "player_id", "local" }, { "ready", false }} }) } };
		searchResponse = Json{{ "state", "SEARCHING" }};
		statusResponse = Json{{ "state", "CONNECTING" }, { "match_id", "mat_test" }};
		disconnectResponse = Json::object();
	}
	static void Tests()
	{
		Reset(); playlistCatalog.entries[0].availability = "NO_CAPACITY";
		ZWNet::StartQuickPlay("public", 1, 0, false);
		Require(Count("/zwnet/matchmaking/search") == 1 && isSearching && refreshes == 1, "fresh capacity did not recover an explicit stale-capacity click");
		Reset(); playlistCatalog.isLoaded = false; playlistCatalog.isStale = true;
		ZWNet::StartQuickPlay({}, 0, 0, true);
		Require(isSearching && Count("/zwnet/matchmaking/search") == 1, "Quick Play did not load its catalog before selecting");
		Reset(); playlistCatalog.entries[0].availability = "NO_CAPACITY";
		ZWNet::StartQuickPlay({}, 0, 0, true);
		Require(isSearching && Count("/zwnet/matchmaking/search") == 1, "Quick Play trusted old capacity instead of refreshing");
		Reset(); refreshedEntries[0].availability = "NO_CAPACITY";
		ZWNet::StartQuickPlay("public", 1, 0, false);
		Require(!isSearching && reportedError == "ZWNET_NO_PUBLIC_CAPACITY" && Count("/zwnet/matchmaking/search") == 0, "real capacity loss was ignored");
		Reset(); refreshedEntries[0].revision = 2;
		ZWNet::StartQuickPlay("public", 1, 0, false);
		Require(reportedError == "ZWNET_PLAYLIST_REFRESH_REQUIRED" && Count("/zwnet/matchmaking/search") == 0, "explicit playlist revision changed without confirmation");
		Reset(); refreshedEntries[0].audience = "RESTRICTED";
		ZWNet::StartQuickPlay({}, 0, 0, true);
		Require(!isSearching && Count("/zwnet/matchmaking/search") == 0, "Quick Play entered a restricted playlist");
		Reset(); failRefresh = true; ZWNet::StartQuickPlay({}, 0, 0, true);
		Require(reportedError == "ZWNET_CATALOG_UNAVAILABLE" && !isPlaylistSelectionStarting, "failed refresh stuck the selector or trusted stale data");
		Reset(); refreshHook = [] { ZWNet::CancelPlaylistSelection(); };
		ZWNet::StartQuickPlay({}, 0, 0, true);
		Require(calls.empty() && !isPlaylistSelectionStarting && !isSearching, "cancellation during refresh still queued a search");
		Reset(); requestHook = [](const std::string& path) { if (path == "/zwnet/matchmaking/search") ZWNet::CancelPlaylistSelection(); };
		ZWNet::StartQuickPlay("public", 1, 0, false); Drain();
		Require(!isSearching && Count("/zwnet/matchmaking/disconnect") == 1, "late search response resurrected a cancelled assignment");
		const auto released = std::ranges::find_if(calls, [](const Call& call) { return call.path == "/zwnet/matchmaking/disconnect"; });
		Require(released->body.value("preserve_party", false) && released->body.value("terminal", false), "late assigned search was not explicitly released");
		Reset(); ZWNet::StartQuickPlay("public", 1, 0, false); ZWNet::CancelPendingJoinPreparation(); Drain();
		Require(!dvars.contains("zwnet_search_playlist_id"), "late accepted-search callback rewrote cancelled UI");
		Reset(); isInGame = true; isSearching = true; ZWNet::UpdateMatchmaking();
		Require(calls.empty() && mainTasks.empty(), "gameplay still polls and rebuilds matchmaking menus");
		Reset(); isSearching = true; requestHook = [](const std::string& path) { if (path == "/zwnet/matchmaking/status") ZWNet::CancelPendingJoinPreparation(); };
		ZWNet::UpdateMatchmaking(); Drain();
		Require(connections == 0 && mainTasks.empty() && !isSearching, "cancelled status response started a connection");
		Reset(); isSearching = true; ZWNet::UpdateMatchmaking(); ZWNet::CancelPendingJoinPreparation(); Drain();
		Require(!dvars.contains("zwnet_match_id"), "late status callback restored the departed match");
		Reset(); currentMatchId = "mat_test"; ZWNet::CancelMatchmaking(); Drain();
		Require(currentMatchId.empty() && currentPartyId == "pty_test" && lobbyReturns == std::vector<bool>{true}, "search cancellation did not retain the party and reopen Find Match");
		Require(Game::closeAllCalls == 0, "search cancellation closed the live frontend and exposed the title screen");
		Require(calls.front().body.value("terminal", false) && calls.front().body.value("preserve_party", false), "explicit leave permits reconnection");
		Reset(); currentMatchId = "mat_test"; ZWNet::CancelMatchmaking(false); Drain();
		Require(lobbyReturns == std::vector<bool>{false} && currentMatchId.empty(), "in-game quit did not return to the retained lobby");
		Reset(); currentMatchId = "mat_test"; disconnectResponse.reset(); ZWNet::CancelMatchmaking();
		Require(currentMatchId == "mat_test" && reportedError == "ZWNET_REQUEST_FAILED" && lobbyReturns.empty(), "failed cleanup pretended the backend assignment was released");
		Reset(); isSearching = true; statusResponse = Json{{"state", "WAITING_FOR_READY"}};
		ZWNet::UpdateMatchmaking();
		Require(Count("/zwnet/parties/pty_test/ready") == 1, "legacy backend readiness still requires a button");
		Reset(); Game::cgame = true; ZWNet::ScheduleMatchmakingLobby(false); Drain();
		Require(lobbyReturns.empty() && scheduledTasks.size() == 1, "post-game return raced native disconnect");
		Game::cgame = false; Game::menus["zwnet_matchmaking"].visible = false; Drain();
		Require(lobbyReturns.empty(), "post-game return opened before frontend initialization");
		Game::menus["main_text"].visible = true; Drain();
		Require(lobbyReturns == std::vector<bool>{false} && scheduledTasks.empty(), "post-game return did not reopen after native frontend initialization");
		Reset(); ZWNet::ScheduleMatchmakingLobby(true); ++joinTransitionGeneration; Drain();
		Require(lobbyReturns.empty() && Game::closeAllCalls == 0, "superseded return reopened an old lobby");
	}
}
int main()
{
	try { Components::Tests(); std::cout << "19 production matchmaking flow regression scenarios passed\n"; return 0; }
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
