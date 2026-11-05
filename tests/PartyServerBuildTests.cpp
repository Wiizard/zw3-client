#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
namespace Game
{
    enum netadrtype_t { NA_LOOPBACK = 2, NA_IP = 4 };
    struct netIP_t { unsigned int full; };
    static int ipCount = 1;
    static int* numIP = &ipCount;
    static netIP_t localIP[] = {{0x01801DAC}};
}
namespace Steam { struct SteamID { std::uint64_t bits; }; }
static bool recorded = false, recordedX86 = true;
static char JoinLobby(std::uint64_t, char)
{
    require(recorded, "Wire format must be registered before the native lobby join");
    return 1;
}
namespace Utils
{
    struct Hook
    {
        template<typename T> static void Set(std::uintptr_t, T) {}
        static std::uintptr_t Rebase(std::uintptr_t) { return reinterpret_cast<std::uintptr_t>(JoinLobby); }
    };
}
namespace Components
{
    struct Network
    {
        struct Address
        {
            Game::netadrtype_t type = Game::NA_IP;
            unsigned int ip = 0;
            unsigned short port = 0;
            bool IsLoopback() const { return type == Game::NA_LOOPBACK || ip == 0x0100007F; }
            void SetType(Game::netadrtype_t value) { type = value; }
            void SetIP(unsigned int value) { ip = value; }
            std::string GetString() const { return "test endpoint"; }
        };
        static inline Address server{};
        static void RecordServerBuild(const Address& address, bool x86) { server = address; recordedX86 = x86; recorded = true; }
    };
    struct Dvar
    {
        struct Var { explicit Var(const char*) {} void Set(bool) const {} };
    };
    struct Logger { template<typename... T> static void Print(const char*, T&&...) {} };
    struct Info
    {
        std::string marker;
        std::string Get(const char*) const { return marker; }
    };
    static struct { Network::Address target; bool isAwaitingPlaylist; Info info; } joinContainer;
    static std::map<std::uint64_t, Network::Address> lobbyMap;
    static constexpr std::uintptr_t g_lobbyCreateInProgress = 1, Steam_JoinLobby = 2;
    struct Party
    {
        static Steam::SteamID GenerateLobbyId() { return {42}; }
        static void PlaylistContinue();
    };
    #include "PlaylistContinue.inc"
}
static void join(unsigned int ip, const std::string& marker)
{
    using namespace Components;
    recorded = false; recordedX86 = true; lobbyMap.clear();
    joinContainer = {{Game::NA_IP, ip, 28961}, true, {marker}};
    Party::PlaylistContinue();
    const auto& endpoint = lobbyMap.at(42);
    require(Network::server.ip == endpoint.ip && Network::server.port == endpoint.port,
        "Protocol metadata must match the actual native game endpoint");
    require(!joinContainer.isAwaitingPlaylist, "Playlist wait must finish");
}
int main()
{
    using namespace Components;
    try
    {
        join(0x0100007F, "1");
        require(!recordedX86 && Network::server.ip == Game::localIP[0].full,
            "x64 localhost party must use x64 messages after the LAN rewrite");
        join(0x0964A8C0, "1");
        require(!recordedX86 && Network::server.ip == 0x0964A8C0, "Remote x64 parties need x64 messages too");
        join(0x0964A8C0, "0");
        require(recordedX86, "Legacy servers keep x86 message translation");
        join(0x0964A8C0, "");
        require(recordedX86, "Missing markers preserve legacy compatibility");
        std::cout << "Party server build tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
