#include "Utils/ServerQuery.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
    try
    {
        const std::string header(4, static_cast<char>(0xff));
        for (const auto* query : {"getstatus\n", "getStatus ", "getInfo \\challenge\\test", "GETINFO\n", "getZWNetRuntime 0123456789abcdef0123456789abcdef"})
            if (!Utils::ReadServerQuery(header + query)) throw std::runtime_error("valid discovery query rejected");
        for (const auto* command : {"connect ticket", "getchallenge x", "rcon x status", "statusResponse\n", "getstatus", "getinfox\n", "getZWNetRuntimeX test", "\n"})
            if (Utils::ReadServerQuery(header + command)) throw std::runtime_error("non-query packet intercepted");
        if (Utils::ReadServerQuery("getinfo\n") || Utils::ReadServerQuery(header.substr(0, 3) + "getinfo\n"))
            throw std::runtime_error("invalid packet header accepted");
        std::cout << "Dedicated server query regression tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
