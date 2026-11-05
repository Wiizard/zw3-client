#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using HANDLE = void*;
constexpr bool FALSE = false;
constexpr int ERROR_ALREADY_EXISTS = 183, MB_ICONEXCLAMATION = 48, MB_YESNO = 4, IDNO = 7;
static bool dedicated = false, protocol = false, existing = false;
static int mutexCalls = 0, dialogCalls = 0, dialogResult = 6;
static std::vector<std::function<void()>> clientInit;
static HANDLE CreateMutexA(void*, bool, const char*) { ++mutexCalls; return reinterpret_cast<HANDLE>(1); }
static int GetLastError() { return existing ? ERROR_ALREADY_EXISTS : 0; }
static int MessageBoxA(void*, const char*, const char*, int) { ++dialogCalls; return dialogResult; }
struct ProcessExit {};
[[noreturn]] static void ExitProcess(int) { throw ProcessExit{}; }

namespace Components
{
    struct Console { static void FreeNativeConsole() {} };
    struct Dedicated { static bool IsEnabled() { return dedicated; } };
    struct ConnectProtocol { static bool Used() { return protocol; } };
    struct Flags { static bool HasFlag(std::string_view) { return false; } };
    struct Events { static void OnClientInit(std::function<void()> task) { clientInit.push_back(std::move(task)); } };
    struct Singleton
    {
        static inline HANDLE mutex = nullptr;
        static inline bool isFirstInstance = true;
        Singleton();
    };
    #include "SingletonConstructor.inc"
}

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void reset()
{
    dedicated = protocol = existing = false; mutexCalls = dialogCalls = 0; dialogResult = 6; clientInit.clear();
}
int main()
{
    try
    {
        reset(); existing = true; Components::Singleton second;
        require(!Components::Singleton::isFirstInstance, "Second instance identity was lost");
        require(dialogCalls == 0 && clientInit.size() == 1, "The constructor opened a modal dialog under the loader lock");
        clientInit.front()(); require(dialogCalls == 1, "Client initialization did not show the instance confirmation");

        reset(); existing = true; dialogResult = IDNO; Components::Singleton declined;
        bool exited = false;
        try { clientInit.front()(); } catch (const ProcessExit&) { exited = true; }
        require(exited, "Declining the second instance did not stop startup");

        reset(); Components::Singleton first;
        require(Components::Singleton::isFirstInstance && clientInit.empty(), "First instance was prompted");
        reset(); dedicated = true; Components::Singleton server;
        require(mutexCalls == 0 && clientInit.empty(), "Dedicated server startup acquired the client mutex or opened a dialog");
        reset(); existing = protocol = true; Components::Singleton forwarded;
        require(!Components::Singleton::isFirstInstance && clientInit.empty(), "Protocol forwarding was blocked by the instance prompt");
        std::cout << "Production Singleton startup, deferred confirmation, decline, dedicated and protocol scenarios passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
