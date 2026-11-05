#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace Game
{
    struct netIP_t { unsigned int full; };
    static netIP_t localIP[] = {{0x01801DAC}, {0x0564A8C0}};
    static int ipCount = 2;
    static int* numIP = &ipCount;
}
namespace Components
{
    struct Network
    {
        struct Address
        {
            unsigned int ip;
            bool loopback = false;
            bool IsLoopback() const { return loopback; }
            unsigned int GetIP() const { return ip; }
        };
    };
    static struct AdmissionConfig { bool isEnabled = false; } config;
    static const AdmissionConfig& GetAdmissionConfig() { return config; }
    struct Auth { static std::uint64_t GetKeyHash() { return 0x12345678; } };
    #include "LocalInstanceIdentity.inc"
}
static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
int main()
{
    using namespace Components;
    constexpr auto sharedCertificate = 0x12345678ULL;
    try
    {
        require(CanUseLocalInstanceIdentity({0x0100007F, true}, sharedCertificate), "Same-machine loopback must support a distinct player ID");
        require(CanUseLocalInstanceIdentity({Game::localIP[0].full}, sharedCertificate), "Native LAN rewrite must preserve local instance identity");
        require(CanUseLocalInstanceIdentity({Game::localIP[1].full}, sharedCertificate), "All local adapters must be recognized");
        require(!CanUseLocalInstanceIdentity({0x0964A8C0}, sharedCertificate), "Another computer on the LAN cannot use an alias");
        require(!CanUseLocalInstanceIdentity({0x08080808}, sharedCertificate), "Remote identities remain certificate-bound");
        require(!CanUseLocalInstanceIdentity({Game::localIP[0].full}, 0x99999999), "Local clients with another certificate cannot use an alias");
        config.isEnabled = true;
        require(!CanUseLocalInstanceIdentity({Game::localIP[0].full}, sharedCertificate), "Managed admission must keep the certificate identity");
        require(!CanUseLocalInstanceIdentity({0x0100007F, true}, sharedCertificate), "Managed loopback must keep the certificate identity");
        config.isEnabled = false;
        Game::ipCount = 0;
        require(!CanUseLocalInstanceIdentity({0x0564A8C0}, sharedCertificate), "An unknown adapter must not count as this machine");
        require(CanUseLocalInstanceIdentity({0x0100007F, true}, sharedCertificate), "Loopback works without a LAN adapter");
        std::cout << "Local instance identity tests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
