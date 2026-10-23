#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace Utils
{
    inline std::optional<int> ParseManagedServerPort(const std::string_view text)
    {
        if (text.empty() || text.size() > 5) return std::nullopt;
        int port = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || port < 1 || port > 65535)
            return std::nullopt;
        return port;
    }
}
