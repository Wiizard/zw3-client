#include "Utils/ManagedServerPort.hpp"
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        if (Utils::ParseManagedServerPort("29000") != 29000 || Utils::ParseManagedServerPort("65535") != 65535
            || Utils::ParseManagedServerPort("1") != 1) throw std::runtime_error("valid reservation rejected");
        for (const auto* text : {"", "0", "65536", "-1", "+29000", "29000 ", " 29000", "29000x", "29.0", "999999"})
            if (Utils::ParseManagedServerPort(text)) throw std::runtime_error("invalid reservation accepted");
        std::cout << "Managed server port regression tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
