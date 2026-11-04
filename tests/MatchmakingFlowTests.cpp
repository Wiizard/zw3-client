#include <array>
#include <atomic>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Components
{
  using Json = nlohmann::json;
  static bool isActive = true;
  static std::atomic<std::uint64_t> joinTransitionGeneration{0};
  static std::mutex stateMutex;
  static std::string currentProposalId, currentMatchId;
  static std::map<std::string, Json> dvars;
  static std::vector<std::function<void()>> tasks;
  static std::optional<Json> response;
  static std::function<void()> requestHook;
  static int errors = 0, polls = 0;
  struct Scheduler {
    enum class Pipeline { MAIN };
    static void Once(std::function<void()> task, Pipeline) { tasks.push_back(std::move(task)); }
  };
  struct Dvar { struct Var {
    std::string name;
    explicit Var(const std::string& value) : name(value) {}
    template<class T> void Set(T value) { dvars[name] = value; }
  }; };
  static std::string JsonString(const Json& value, const char* key) { return value.value(key, std::string{}); }
  static int JsonIntegerOr(const Json& value, const char* key, int fallback) { return value.value(key, fallback); }
  static std::string ResponseErrorCode(const Json& value) { return value.at("error").value("code", std::string{}); }
  static void SetDisplayText(const std::string& name, const std::string& value) { dvars[name] = value; }
  static std::string ResolveVoteMapDisplayName(const std::string&, const std::string& name) { return name; }
  static std::string arena;
  struct Localization { static const char* GetMapImageName(const char*) { return arena.c_str(); } };
  struct Materials {
    static inline std::set<std::string> available, cached;
    static inline int loads = 0, deleted = 0;
    static inline bool createFails = false;
    static int* GetRuntimeMaterial(const std::string& name) { static int material; return cached.contains(name) ? &material : nullptr; }
    static int* LoadPreviewImage(const std::string& name) { static int image; ++loads; return available.contains(name) ? &image : nullptr; }
    static int* Create(const std::string& name, int* image) { if (createFails) return nullptr; cached.insert(name); return image; }
    static void DeleteImage(int*) { ++deleted; }
  };
  struct ZWNet {
    static void UpdateVoteDvars(const Json&, std::uint64_t, std::uint64_t);
    static void VoteMap(const std::string&);
    static std::optional<Json> Request(const char*, const char*, const Json&) { if (requestHook) requestHook(); return response; }
    static void SetState(const char*, const char*) { ++errors; }
    static void UpdateMatchmaking() { ++polls; }
  };

#include "MatchUiGuard.inc"
#include "VoteMapImage.inc"
#include "VoteDvars.inc"
#include "VoteSubmit.inc"

  static void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
  static void Drain() { auto pending = std::move(tasks); tasks.clear(); for (auto& task : pending) task(); }
  static void Reset() {
    isActive = true; joinTransitionGeneration = 7; matchUiSnapshotGeneration = 12;
    dvars.clear(); tasks.clear(); currentProposalId = currentMatchId = "new";
    arena = "preview_map"; Materials::available = {"preview_map"}; Materials::cached.clear();
    Materials::loads = Materials::deleted = 0; Materials::createFails = false;
    response.reset(); requestHook = {}; errors = polls = 0;
  }
  static Json Vote() {
    return {{"match_id", "match"}, {"map_vote", {{"proposal_id", "vote"}, {"seconds_remaining", 11},
      {"choices", Json::array({{{"id", "map"}, {"name", "Map A"}, {"votes", 2}},
        {{"id", "other"}, {"name", "Map B"}, {"votes", 1}}, {{"id", "RANDOM"}, {"votes", 0}}})}}}};
  }
  static void Tests() {
    Reset(); ZWNet::UpdateVoteDvars(Vote(), 7, 12); Drain();
    Require(dvars.at("zwnet_vote_seconds") == 11 && dvars.at("zwnet_vote_map_a_votes") == 2, "current vote lost backend time/votes");
    Require(dvars.at("zwnet_vote_map_a_image") == "zw3_vote_preview_map", "vote did not publish verified runtime material");
    Reset(); ZWNet::UpdateVoteDvars(Vote(), 7, 12); ++joinTransitionGeneration; Drain();
    Require(dvars.empty(), "cancelled vote repainted the lobby");
    Reset(); ZWNet::UpdateVoteDvars(Vote(), 7, 12); ++matchUiSnapshotGeneration; Drain();
    Require(dvars.empty(), "older vote replaced a newer snapshot");
    Reset(); ZWNet::UpdateVoteDvars(Vote(), 7, 12); isActive = false; Drain();
    Require(dvars.empty(), "closed menu was repainted");
    Reset(); ZWNet::UpdateVoteDvars(Vote(), 6, 12); Drain();
    Require(dvars.empty() && currentMatchId == "new", "late response adopted a cancelled assignment");
    Reset(); auto malformed = Vote(); malformed["map_vote"]["choices"] = Json::array();
    ZWNet::UpdateVoteDvars(malformed, 7, 12); Drain(); Require(dvars.empty(), "invalid options mutated vote UI");
    Reset(); Require(ResolveVoteMapImage("map", "preview_wrong") == "zw3_vote_preview_map", "arena priority changed");
    const int loads = Materials::loads; ResolveVoteMapImage("map", "preview_wrong");
    Require(Materials::loads == loads, "polling reloaded an existing preview image");
    Reset(); arena = "preview_missing";
    Require(ResolveVoteMapImage("map", "preview_wrong") == "zw3_vote_preview_map", "canonical texture fallback failed");
    Reset(); arena.clear(); Materials::available.clear();
    Require(ResolveVoteMapImage("map", "preview_missing").empty(), "missing images fabricated a material");
    Reset(); arena = "../../bad"; Materials::available.clear();
    Require(ResolveVoteMapImage("../bad", "preview_../bad").empty() && Materials::loads == 0, "unsafe image path reached loader");
    Reset(); Materials::createFails = true;
    Require(ResolveVoteMapImage("map", "").empty() && Materials::deleted > 0, "failed material leaked its owned image");
    Reset(); currentMatchId = "match"; currentProposalId = "vote"; response = Vote().at("map_vote");
    ZWNet::VoteMap("A"); Drain(); Require(dvars.at("zwnet_vote_seconds") == 11, "submitted vote discarded authoritative result");
    Reset(); response = Vote().at("map_vote"); requestHook = [] { ++joinTransitionGeneration; };
    ZWNet::VoteMap("A"); Drain(); Require(dvars.empty() && errors == 0 && polls == 0, "cancelled submission mutated UI");
    Reset(); requestHook = [] { ++joinTransitionGeneration; };
    ZWNet::VoteMap("A"); Drain(); Require(dvars.empty() && errors == 0, "cancelled failed request replaced lobby with error");
  }
}
int main() {
  try { Components::Tests(); std::cout << "15 production vote/preview regression scenarios passed\n"; return 0; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
