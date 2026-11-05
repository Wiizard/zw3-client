#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using Json = nlohmann::json;
static std::map<std::string, Json> dvars;
static bool cgame = false, localServer = false, uiReady = true;
static int connection = 0, clockTime = 0;
static std::vector<std::string> menus, commands, connected;
static std::vector<std::function<void()>> mainWork, asyncWork;
static std::vector<std::function<bool()>> timers;
static std::vector<std::pair<std::string, Json>> requests;
static std::function<std::optional<Json>(const std::string&, const Json&)> httpResponse;
static int abandoned = 0, resumed = 0, updates = 0;

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
static void drain(std::vector<std::function<void()>>& queue)
{
    while (!queue.empty()) { auto task = std::move(queue.front()); queue.erase(queue.begin()); task(); }
}
static void tick(int milliseconds)
{
    clockTime += milliseconds;
    auto current = std::move(timers); timers.clear();
    for (auto& task : current) if (!task()) timers.push_back(std::move(task));
}

namespace Game
{
    struct alignas(16) PartyData { std::array<std::uint8_t, 0x1900> data{}; };
    static PartyData lobby, party;
    static PartyData* g_lobbyData = &lobby;
    static PartyData* g_partyData = &party;
    struct menuDef_t {};
    static int context;
    static int* uiContext = &context;
    enum { CA_DISCONNECTED = 0, CA_CONNECTING = 3, CA_LOADING = 7, CA_ACTIVE = 9 };
    static bool CL_IsCgameInitialized(int) { return cgame; }
    static int CL_GetLocalClientConnectionState(int) { return connection; }
    static int Sys_Milliseconds() { return clockTime; }
    static menuDef_t* Menus_FindByName(int*, const char*) { static menuDef_t menu; return uiReady ? &menu : nullptr; }
    static void Menus_CloseRequest(int*, menuDef_t*) {}
    static void Menus_CloseAll(int*) { menus.clear(); }
    static void Menus_OpenByName(int*, const char* name) { menus.emplace_back(name); }
}
namespace Components
{
    struct Dedicated { static bool IsRunning() { return localServer; } };
    struct Dvar
    {
        struct Var
        {
            std::string name;
            explicit Var(std::string value) : name(std::move(value)) {}
            template<class T> T Get() const { return dvars.contains(name) ? dvars.at(name).get<T>() : T{}; }
            template<class T> void Set(T&& value) const { dvars[name] = std::forward<T>(value); }
        };
    };
    struct Scheduler
    {
        enum class Pipeline { MAIN };
        template<class F> static void Once(F task, Pipeline) { mainWork.emplace_back(std::move(task)); }
        template<class F, class D> static void Schedule(F task, Pipeline, D) { timers.emplace_back(std::move(task)); }
    };
    struct Command
    {
        struct Params
        {
            std::string target;
            int Size() const { return target.empty() ? 1 : 2; }
            const char* Get(int) const { return target.c_str(); }
        };
        static inline std::function<void(const Params*)> connect;
        template<class F> static void Add(const char*, F task) { connect = std::move(task); }
        static void Execute(const std::string& text, bool = true) { commands.push_back(text); }
    };
    struct Network
    {
        struct Address
        {
            std::string value;
            explicit Address(std::string text) : value(std::move(text)) {}
            bool IsValid() const { return !value.empty() && value != "invalid"; }
            std::string GetString() const { return value; }
            bool operator==(const Address&) const = default;
        };
    };
    struct LobbyScene { static void StopTransition() {} };
    struct Events { struct NativeDisconnectError { bool hasNativeError = false; std::string message, title; }; };
    struct ScopeExit { std::function<void()> action; explicit ScopeExit(std::function<void()> fn) : action(std::move(fn)) {} ~ScopeExit() { action(); } };

    static std::atomic<std::uint64_t> joinTransitionGeneration{0}, lobbySnapshotGeneration{0}, playlistSelectorGeneration{0};
    static std::atomic_bool isActive{true}, isTerminalDisconnectRequested{false}, isEndpointJoinInFlight{false};
    static std::atomic_bool isServerJoinTransition{false}, isClosingOnlineSession{false}, isInGame{false};
    static std::atomic_bool isPlaylistSelectionStarting{false}, isSearching{false}, isIdleReturnPending{false};
    static bool isCompletedMatchReturnPending = false;
    static std::mutex stateMutex;
    static std::string currentPartyId, currentMatchId, currentProposalId, accessToken;
    static std::uint64_t connectRequestGeneration = 0;
    static bool isPrivatePartyJoin = false;
    constexpr std::size_t partyAreWeHost = 0x1808, partyInParty = 0x1814;
    static void TraceOnlineTransition(const char*) {}
    static void CancelJoinInProgressPreview() {}
    static void CancelRelayHandshake() {}
    struct Auth { static void ClearManagedConnectTicket() {} };
    static void ResetMatchLobbySoundSnapshot() {}
    static void SchedulePlaylistCatalogPublish() {}
    static void ResetManagedRoute() { ++joinTransitionGeneration; }
    static int NextPresenceSequence() { return 1; }
    static std::string FriendlyErrorText(const char* text) { return text; }

    struct Party
    {
        static bool IsHostingParty();
        static void CancelPendingJoin();
        static void RegisterConnect();
        static void Connect(const Network::Address& target, bool = false, bool = false) { connected.push_back(target.GetString()); }
        struct JoinContainer { bool isValid = false, isAwaitingPlaylist = false; };
        static inline JoinContainer joinContainer;
    };
    struct ZWNet
    {
        static bool PrepareEndpointJoin(const Network::Address&);
        static bool BeginEndpointJoin(const std::string&, bool = false);
        static void QuitToPlayOnline();
        static void CloseOnlineSession(bool, bool, bool = false);
        static void ReturnToIdleMenu(const Events::NativeDisconnectError&, bool);
        static void ScheduleReturnToIdleMenu(Events::NativeDisconnectError = {}, bool = false);
        static void EnqueueAsync(std::function<void()> fn) { asyncWork.push_back(std::move(fn)); }
        static std::optional<Json> Request(const std::string&, const std::string& path, const Json& body = {})
        {
            requests.emplace_back(path, body);
            return httpResponse ? httpResponse(path, body) : std::optional<Json>{Json::object()};
        }
        static void CancelPendingJoinPreparation();
        static void AbandonOnlineSession()
        {
            ++abandoned; CancelPendingJoinPreparation();
            currentPartyId.clear(); currentMatchId.clear(); currentProposalId.clear();
        }
        static void ClearMatchDvars() {}
        static void SetState(const std::string& state, const std::string& error = {}) { dvars["state"] = state; dvars["error"] = error; }
        static void ResumeParty(const Json&) { ++resumed; }
        static void UpdateMatchmaking() { ++updates; }
    };
    #include "PartyRole.inc"
    #include "CancelJoin.inc"
    #include "CancelPreparation.inc"
    #include "EndpointJoin.inc"
    #include "Quit.inc"
    #include "CloseSession.inc"
    #include "ReturnMenu.inc"
    void Party::RegisterConnect() {
        #include "ConnectCommand.inc"
    }
}

static void reset()
{
    using namespace Components;
    menus.clear(); commands.clear(); connected.clear(); mainWork.clear(); asyncWork.clear(); timers.clear(); requests.clear(); dvars.clear(); httpResponse = {};
    cgame = localServer = false; uiReady = true; connection = clockTime = 0;
    Game::uiContext = &Game::context; Game::lobby = {}; Game::party = {};
    isActive = true; isTerminalDisconnectRequested = isEndpointJoinInFlight = isServerJoinTransition = false;
    isClosingOnlineSession = isInGame = isPlaylistSelectionStarting = isSearching = isIdleReturnPending = false;
    currentPartyId.clear(); currentMatchId.clear(); currentProposalId.clear(); accessToken = "test-account";
    abandoned = resumed = updates = 0; ++joinTransitionGeneration;
    Party::CancelPendingJoin(); Party::RegisterConnect();
}
static void nativeParty(Game::PartyData& party, bool joined, bool host)
{
    const int in = joined, owner = host;
    std::memcpy(party.data.data() + Components::partyInParty, &in, sizeof(in));
    std::memcpy(party.data.data() + Components::partyAreWeHost, &owner, sizeof(owner));
}
static void inputConnect(const std::string& target)
{
    Components::Command::Params args{target}; Components::Command::connect(&args);
}
int main()
{
    using namespace Components;
    try
    {
        reset(); nativeParty(Game::party, true, true); nativeParty(Game::lobby, true, false);
        require(!Party::IsHostingParty(), "Remote lobby guest inherited local social-party host controls");
        cgame = true; require(!Party::IsHostingParty(), "Remote player received in-game host controls");
        localServer = true; require(Party::IsHostingParty(), "Local listen server lost host controls");
        cgame = false; nativeParty(Game::lobby, false, false);
        require(Party::IsHostingParty(), "An idle private-party host lost host controls");

        reset(); cgame = true; connection = Game::CA_ACTIVE;
        ZWNet::ScheduleReturnToIdleMenu({}, true); tick(200);
        require(menus.empty(), "Return opened menus while the game was still running");
        cgame = false; connection = Game::CA_LOADING; tick(200);
        require(menus.empty(), "Return raced native disconnect");
        connection = Game::CA_DISCONNECTED; uiReady = false; tick(200);
        require(menus.empty(), "Return opened missing frontend menus");
        uiReady = true; tick(1); tick(149); require(menus.empty(), "Return skipped its frontend settling interval"); tick(1);
        require(menus == std::vector<std::string>{"zwnet_matchmaking", "popup_zwnet_connecting"}, "Explicit Quit did not enter the Play Online lobby");
        require(commands.size() == 5 && commands[3] == "xstopparty" && commands[4] == "xstopprivateparty", "Explicit Quit retained a native party");

        reset(); ZWNet::ScheduleReturnToIdleMenu({}, true); ++joinTransitionGeneration; tick(200);
        require(menus.empty() && timers.empty(), "An old return interrupted a newer join");
        reset(); ZWNet::ScheduleReturnToIdleMenu({true,"old error","Connection error"}, false);
        ZWNet::QuitToPlayOnline(); ZWNet::ScheduleReturnToIdleMenu({}, true); tick(1); tick(150);
        require(menus.front() == "zwnet_matchmaking", "A cancelled error return suppressed the newer explicit Quit");
        reset(); ZWNet::ReturnToIdleMenu({true,"native failure","Connection error"}, false);
        require(menus.back() == "error_popmenu_lobby" && dvars["com_errorMessage"] == "native failure", "Native disconnect error was lost");

        reset(); currentMatchId = "mat_old"; currentPartyId = "pty_old";
        ZWNet::CloseOnlineSession(false, true, true); ZWNet::ScheduleReturnToIdleMenu({}, true); drain(mainWork); tick(1); tick(150);
        require(menus == std::vector<std::string>{"zwnet_matchmaking", "popup_zwnet_connecting"} && abandoned == 1, "Terminal cleanup invalidated its own frontend return");
        reset(); ZWNet::CloseOnlineSession(false, true); drain(mainWork);
        require(abandoned == 1, "Closing a frontend online lobby did not abandon its local session");

        reset(); httpResponse = [](const auto&, const auto&) -> std::optional<Json> { return Json{{"managed",false}}; };
        require(ZWNet::BeginEndpointJoin("host-a"), "Endpoint join was not queued"); drain(asyncWork);
        ZWNet::QuitToPlayOnline(); drain(mainWork);
        require(connected.empty() && isTerminalDisconnectRequested && commands.back() == "disconnect", "Late endpoint response reconnected after Quit");

        reset(); httpResponse = [](const auto&, const auto&) -> std::optional<Json> {
            ZWNet::QuitToPlayOnline(); return Json{{"managed",true},{"connect",true},{"party",Json::object()}};
        };
        ZWNet::BeginEndpointJoin("host-a"); drain(asyncWork); drain(mainWork);
        require(resumed == 0 && updates == 0, "A cancelled HTTP request resumed a managed match");

        reset(); httpResponse = [](const auto&, const auto&) -> std::optional<Json> { return Json{{"managed",false}}; };
        ZWNet::BeginEndpointJoin("host-a"); ZWNet::BeginEndpointJoin("host-b"); drain(asyncWork); drain(mainWork);
        require(connected == std::vector<std::string>{"host-b"} && requests.size() == 1, "Overlapping joins used an obsolete target");

        reset(); httpResponse = [](const auto&, const auto&) -> std::optional<Json> { return std::nullopt; };
        currentMatchId = "mat_assigned"; dvars["zwnet_server_endpoint"] = "host-a";
        ZWNet::BeginEndpointJoin("host-a"); drain(asyncWork); drain(mainWork);
        require(connected.empty() && dvars["error"] == "ZWNET_REQUEST_FAILED", "An assigned server bypassed authorization on request failure");

        reset(); currentMatchId = "mat_old"; currentPartyId = "pty_old";
        ZWNet::PrepareEndpointJoin(Network::Address("host-b")); currentMatchId = "mat_new"; drain(asyncWork);
        require(requests.front().second["match_id"] == "mat_old", "Departing one server cleaned up the next assignment");

        reset(); currentMatchId = "mat_assigned"; dvars["zwnet_server_endpoint"] = "host-a";
        httpResponse = [](const auto&, const auto&) -> std::optional<Json> { return std::nullopt; };
        inputConnect("host-a"); drain(asyncWork); drain(mainWork);
        require(connected.empty() && dvars["error"] == "ZWNET_REQUEST_FAILED", "Disconnecting before reconnect discarded managed endpoint protection");

        reset(); inputConnect("invalid");
        require(commands.empty() && asyncWork.empty(), "Invalid connect input disconnected the current game");
        reset(); cgame = true; connection = Game::CA_ACTIVE;
        inputConnect("host-a"); tick(1);
        require(connected.empty() && requests.empty() && commands == std::vector<std::string>{"disconnect"}, "Connect was replayed in the same command-buffer drain");
        inputConnect("host-b"); cgame = false; connection = Game::CA_DISCONNECTED; tick(1);
        httpResponse = [](const auto&, const auto&) -> std::optional<Json> { return Json{{"managed",false}}; };
        drain(asyncWork); drain(mainWork);
        require(connected == std::vector<std::string>{"host-b"}, "Deferred connection ignored the latest requested target");
        reset(); cgame = true; connection = Game::CA_ACTIVE; inputConnect("host-a"); ZWNet::QuitToPlayOnline();
        cgame = false; connection = Game::CA_DISCONNECTED; tick(1); drain(asyncWork); drain(mainWork);
        require(connected.empty() && requests.empty(), "A deferred connect survived an explicit Quit");
        std::cout << "Production host-role, Quit, frontend readiness, cleanup and queued connection scenarios passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
