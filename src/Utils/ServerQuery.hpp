#pragma once

#include <optional>
#include <string_view>

namespace Utils
{
    inline std::optional<std::string_view> ReadServerQuery(const std::string_view packet)
    {
        if (packet.size() < 6) return std::nullopt;
        for (int i = 0; i < 4; ++i)
            if (static_cast<unsigned char>(packet[i]) != 0xff) return std::nullopt;
        const auto end = packet.find_first_of(std::string_view(" \n\r\0", 4), 4);
        if (end == std::string_view::npos || end == 4 || end - 4 > 16) return std::nullopt;
        const auto command = packet.substr(4, end - 4);
        const auto equals = [command](const std::string_view expected)
        {
            if (command.size() != expected.size()) return false;
            for (std::size_t i = 0; i < command.size(); ++i)
            {
                auto value = command[i];
                if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
                if (value != expected[i]) return false;
            }
            return true;
        };
        if (!equals("getinfo") && !equals("getstatus") && !equals("getzwnetruntime")) return std::nullopt;
        return command;
    }
}
