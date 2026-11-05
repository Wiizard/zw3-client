#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Components
{
  using Json = nlohmann::json;
  static bool isActive, isSearching, isInGame, isServerJoinTransition, isClosingOnlineSession;
  static bool isPlaylistSearchStarting, isPlaylistSelectionStarting, isCompletedMatchReturnPending;
  static std::atomic_uint64_t joinTransitionGeneration;
  static std::mutex stateMutex;
  static std::string currentPartyId, currentMatchId, currentProposalId, presenceUiState;
  static int rosterWrites, presenceWrites, creations, clearCalls;
  static std::vector<std::function<void()>> queued, mainTasks;
  static std::vector<std::string> commands;
  static std::function<void()> requestHook, rankHook, createHook;
  static std::optional<Json> response;
  static std::string JsonString(const Json& value, const char* key, const std::string& fallback = {}) { return value.value(key, fallback); }
  static bool IsActiveMatchmakingState(const std::string& state) { return state == "SEARCHING" || state == "MAP_VOTE"; }
  static void ResetManagedRoute() { ++joinTransitionGeneration; }
  static void ClearMatchDvars() { ++clearCalls; }
  struct Scheduler {
    enum class Pipeline { MAIN };
    static void Once(std::function<void()> task, Pipeline) { mainTasks.push_back(std::move(task)); }
  };
  struct Command { static void Execute(const std::string& command, bool) { commands.push_back(command); } };
  struct Dvar { struct Var { explicit Var(const char*) {} template<class T> void Set(T) {} }; };
  struct ZWNet {
    static void RefreshActiveParty();
    static void ResumeParty(const Json& party);
    static std::optional<Json> Request(const char*, const char*) { if (requestHook) requestHook(); return response; }
    static Json PublishLocalRank(Json party) { if (rankHook) rankHook(); return party; }
    static std::optional<Json> CurrentOrNewParty() { ++creations; if (createHook) createHook(); return Json{{"id", "recovered"}}; }
    static void EnqueueAsync(std::function<void()> task) { queued.push_back(std::move(task)); }
    static void UpdateLobbyDvars(const Json& party) { ++rosterWrites; currentPartyId = JsonString(party, "id"); }
    static void UpdatePresence() { ++presenceWrites; }
    static void CancelJoinInProgressPreview() {}
    static void SetState(const std::string& state) { presenceUiState = state; }
    static void UpdateMatchmaking() {}
  };

#include "PartyRefresh.inc"
#include "PartyResume.inc"

  static void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
  static void Reset() {
    isActive = true;
    isSearching = isInGame = isServerJoinTransition = isClosingOnlineSession = false;
    isPlaylistSearchStarting = isPlaylistSelectionStarting = isCompletedMatchReturnPending = false;
    joinTransitionGeneration = 0; currentPartyId = "old"; presenceUiState = "IN_PARTY";
    rosterWrites = presenceWrites = creations = clearCalls = 0;
    queued.clear(); mainTasks.clear(); commands.clear(); requestHook = rankHook = createHook = {};
    response = Json{{"id", "old"}, {"state", "IDLE"}};
  }
  static void Drain(std::vector<std::function<void()>>& tasks) { auto pending = std::move(tasks); tasks.clear(); for (auto& task : pending) task(); }
  static void Tests() {
    Reset(); ZWNet::RefreshActiveParty(); Require(rosterWrites == 1, "normal idle roster did not refresh");
    Reset(); requestHook = [] { currentPartyId = "new"; }; ZWNet::RefreshActiveParty();
    Require(rosterWrites == 0 && currentPartyId == "new", "old HTTP response overwrote joined party");
    Reset(); requestHook = [] { ++joinTransitionGeneration; }; ZWNet::RefreshActiveParty();
    Require(rosterWrites == 0, "cancelled request updated roster");
    Reset(); rankHook = [] { ++joinTransitionGeneration; }; ZWNet::RefreshActiveParty();
    Require(rosterWrites == 0, "rank publication resurrected a cancelled lobby");
    Reset(); response = Json{{"id", "unrelated"}}; ZWNet::RefreshActiveParty();
    Require(rosterWrites == 0, "poll adopted an unrelated party without a transition");
    Reset(); response.reset(); ZWNet::RefreshActiveParty(); Drain(queued);
    Require(creations == 0, "network failure fabricated a new party");
    Reset(); response = nullptr; ZWNet::RefreshActiveParty(); ++joinTransitionGeneration; Drain(queued);
    Require(creations == 0, "queued recovery ran after cancellation");
    Reset(); response = nullptr; createHook = [] { ++joinTransitionGeneration; }; ZWNet::RefreshActiveParty(); Drain(queued);
    Require(rosterWrites == 0 && presenceWrites == 0, "late recovery reopened a closed lobby");
    Reset(); response = nullptr; ZWNet::RefreshActiveParty(); Drain(queued);
    Require(creations == 1 && rosterWrites == 1 && presenceWrites == 1, "confirmed missing pre-lobby did not recover");
    Reset(); ZWNet::ResumeParty(Json{{"id", "joined"}, {"state", "IDLE"}}); Drain(mainTasks);
    Require(clearCalls == 1 && commands.size() == 2, "normal idle acceptance did not open lobby");
    Reset(); ZWNet::ResumeParty(Json{{"id", "joined"}, {"state", "IDLE"}}); ++joinTransitionGeneration; Drain(mainTasks);
    Require(clearCalls == 0 && commands.empty(), "cancelled acceptance reopened or cleared a menu");
    Reset(); ZWNet::ResumeParty(Json{{"id", "first"}, {"state", "IDLE"}});
    ZWNet::ResumeParty(Json{{"id", "second"}, {"state", "IDLE"}}); Drain(mainTasks);
    Require(clearCalls == 1 && commands.size() == 2 && currentPartyId == "second", "superseded party callbacks still ran");
    Reset(); ZWNet::ResumeParty(Json{{"id", "joined"}, {"state", "IDLE"}}); isSearching = true; Drain(mainTasks);
    Require(clearCalls == 0, "late idle cleanup erased new search/vote state");
  }
}

int main() {
  try { Components::Tests(); std::cout << "13 production party refresh/resume regression scenarios passed\n"; return 0; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
