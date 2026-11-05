#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>

static int sleeps = 0, polls = 0, events = 0, serverPackets = 0, ready = 1;
static DWORD sleptMs = 0;
static SOCKET fakeSocket = 33;
static void TestSleep(DWORD ms) { ++sleeps; sleptMs = ms; }
static int TestSelect(int, fd_set*, fd_set*, fd_set*, const timeval*) { ++polls; return ready; }
static int TestSocketError() { return 0; }
static void EventLoop() { ++events; }
static void ServerPacketEvent() { ++serverPackets; }
namespace Game { struct dvar_t { struct { bool enabled; } current; }; }
static Game::dvar_t running{{false}};
static Game::dvar_t* fakeRunningDvar = &running;
static constexpr std::uintptr_t com_sv_running = 1, ip_socket = 2, Com_EventLoop = 3, Com_ServerPacketEvent = 4;
namespace Utils
{
    struct Hook
    {
        static std::uintptr_t Rebase(std::uintptr_t address)
        {
            switch (address)
            {
            case com_sv_running: return reinterpret_cast<std::uintptr_t>(&fakeRunningDvar);
            case ip_socket: return reinterpret_cast<std::uintptr_t>(&fakeSocket);
            case Com_EventLoop: return reinterpret_cast<std::uintptr_t>(EventLoop);
            case Com_ServerPacketEvent: return reinterpret_cast<std::uintptr_t>(ServerPacketEvent);
            default: throw std::runtime_error("Unexpected engine call");
            }
        }
    };
}
struct Logger { template<typename... T> static void Warning(const char*, T&&...) {} };
#define Sleep TestSleep
#define select TestSelect
#define WSAGetLastError TestSocketError
#include "NetSleep.inc"
#undef Sleep
#undef select
#undef WSAGetLastError
static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void reset() { sleeps = polls = events = serverPackets = 0; sleptMs = 0; ready = 1; fakeSocket = 33; fakeRunningDvar = &running; running.current.enabled = false; }
int main()
{
    try
    {
        reset(); running.current.enabled = true;
        NetSleep(10);
        require(sleeps == 1 && sleptMs == 10 && polls == 0 && serverPackets == 0 && events == 0,
            "A listen host must leave queued packets to the VM-owning server worker");
        reset(); NetSleep(10);
        require(polls == 1 && events == 1 && serverPackets == 0, "Clients retain event-driven waits");
        reset(); ready = 0; NetSleep(10);
        require(events == 0 && serverPackets == 0, "No readiness must not dispatch events");
        reset(); NetSleep(10, false);
        require(events == 0 && serverPackets == 0, "Disabled dispatch must remain disabled");
        reset(); fakeSocket = INVALID_SOCKET; NetSleep(10);
        require(sleeps == 1 && sleptMs == 10 && polls == 0, "An unavailable socket must use a timed sleep");
        reset(); fakeRunningDvar = nullptr; NetSleep(0);
        require(polls == 1 && events == 1, "Early startup supports an unregistered server dvar");
        std::cout << "Listen server frame wait tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
