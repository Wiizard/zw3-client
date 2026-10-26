#pragma once

#include "Types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <regex>

namespace Components::Menu::BackdropData
{
    struct placed_effect
    {
        string name;
        array<float, 3> origin {}, angles {};
        float delay = 0;
        float repeat = 0;
    };

    // These are data readers, not script execution. Only authored literal
    // placements are accepted; gameplay threads and quest logic never run.
    inline string environment_source(string_view source)
    {
        string out;
        bool quoted = false;
        for (size_t i = 0; i < source.size(); ++i)
        {
            const char c = source[i];
            if (c == '"' && (i == 0 || source[i - 1] != '\\')) quoted = !quoted;
            if (!quoted && source.substr(i, 2) == "//")
            {
                while (i < source.size() && source[i] != '\n' && source[i] != '\r') ++i;
                out += ' ';
            }
            else if (!quoted && source.substr(i, 2) == "/*")
            {
                const auto end = source.find("*/", i + 2);
                i = end == string_view::npos ? source.size() : end + 1;
                out += ' ';
            }
            else out += c;
        }
        return out;
    }

    inline optional<array<float, 3>> environment_vector(const string& text)
    {
        array<float, 3> result {};
        const char* p = text.c_str();
        for (float& value : result)
        {
            while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r' || *p == '\n') ++p;
            char* end = nullptr;
            value = std::strtof(p, &end);
            if (p == end || !std::isfinite(value)) return {};
            p = end;
        }
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        return *p ? optional<array<float, 3>>{} : result;
    }

    using effect_names = std::map<string, string, std::less<>>;
    inline void read_effect_names(const string& source, effect_names& names)
    {
        static const std::regex pattern(R"re(level\._effect\s*\[\s*"([^"]+)"\s*\]\s*=\s*loadfx\s*\(\s*"([^"]+)")re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), pattern), end; i != end; ++i)
            names[(*i)[1].str()] = (*i)[2].str();
        static const std::regex member(R"re(level\.([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*loadfx\s*\(\s*"([^"]+)")re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), member), end; i != end; ++i)
            names["level." + (*i)[1].str()] = (*i)[2].str();
    }

    inline vector<placed_effect> read_placed_effects(const string& source, const effect_names& names)
    {
        vector<placed_effect> result;
        const auto append = [&](string key, const string& position, const string& orientation, float delay)
        {
            const auto name = names.find(key);
            if (name == names.end()) return;
            const auto origin = environment_vector(position), angles = environment_vector(orientation);
            if (origin && angles && std::isfinite(delay)) result.push_back({name->second, *origin, *angles, std::clamp(delay, -30.0f, 60.0f)});
        };
        static const std::regex direct(R"re((?:playfx|spawnfx)\s*\(\s*level\._effect\s*\[\s*"([^"]+)"\s*\]\s*,\s*\(([^()]+)\)\s*,\s*anglestoforward\s*\(\s*\(([^()]+)\))re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), direct), end; i != end; ++i)
            append((*i)[1], (*i)[2], (*i)[3], -15);

        static const std::regex create(R"re(create(Oneshot|Loop)Effect\s*\(\s*"([^"]+)"\s*\))re", std::regex::icase);
        static const std::regex nextEntity(R"re(\bent\s*=)re");
        static const std::regex origin(R"re(\.v\s*\[\s*"origin"\s*\]\s*=\s*\(([^()]+)\))re", std::regex::icase);
        static const std::regex angles(R"re(\.v\s*\[\s*"angles"\s*\]\s*=\s*\(([^()]+)\))re", std::regex::icase);
        static const std::regex delay(R"re(\.v\s*\[\s*"delay"\s*\]\s*=\s*([-+0-9.eE]+))re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), create), end; i != end; ++i)
        {
            const auto begin = source.cbegin() + (*i).position() + (*i).length();
            std::smatch next;
            const auto endBlock = std::regex_search(begin, source.cend(), next, nextEntity) ? next[0].first : source.cend();
            const string block(begin, endBlock);
            std::smatch p, a, d;
            if (std::regex_search(block, p, origin) && std::regex_search(block, a, angles))
            {
                const float seconds = std::regex_search(block, d, delay) ? std::strtof(d[1].str().c_str(), nullptr) : 0;
                const bool looping = (*i)[1].str()[0] == 'L' || (*i)[1].str()[0] == 'l';
                const auto before = result.size();
                append((*i)[2], p[1], a[1], looping ? 0 : seconds);
                if (looping && result.size() != before) result.back().repeat = std::clamp(seconds, 0.05f, 60.0f);
            }
        }
        return result;
    }

    inline vector<placed_effect> read_entity_effects(const string& source, const string& entities, const effect_names& names)
    {
        // Older ports place fire, smoke and weather on named BSP entities.
        // Resolve literal getEnt/getEntArray lookups without running GSC.
        std::map<string, std::pair<string, string>, std::less<>> bindings;
        static const std::regex lookup(R"re(([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*getEnt(?:Array)?\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)")re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), lookup), end; i != end; ++i)
            bindings[(*i)[1].str()] = {(*i)[3].str(), (*i)[2].str()};
        vector<std::map<string, string, std::less<>>> objects;
        static const std::regex block(R"re(\{([^{}]*)\})re");
        static const std::regex pair(R"re("([^"]+)"\s*"([^"]*)")re");
        for (std::sregex_iterator i(entities.begin(), entities.end(), block), end; i != end; ++i)
        {
            const string text = (*i)[1];
            auto& object = objects.emplace_back();
            for (std::sregex_iterator j(text.begin(), text.end(), pair); j != end; ++j)
                object[(*j)[1].str()] = (*j)[2].str();
        }
        vector<placed_effect> result;
        static const std::regex play(R"re(playfx\s*\(\s*level\.([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)(?:\s*\[[^\]]+\])?\s*\.\s*origin\s*\))re", std::regex::icase);
        for (std::sregex_iterator i(source.begin(), source.end(), play), end; i != end; ++i)
        {
            const auto effect = names.find("level." + (*i)[1].str());
            const auto binding = bindings.find((*i)[2].str());
            if (effect == names.end() || binding == bindings.end()) continue;
            for (const auto& object : objects)
            {
                const auto match = object.find(binding->second.first), origin = object.find("origin");
                if (match == object.end() || match->second != binding->second.second || origin == object.end()) continue;
                if (const auto position = environment_vector(origin->second))
                    result.push_back({effect->second, *position, {}, -15});
            }
        }
        return result;
    }

    inline string read_vision_name(const string& source, string fallback)
    {
        static const std::regex pattern(R"re(\bVisionSetNaked\s*\(\s*"([a-zA-Z0-9_/-]+)")re", std::regex::icase);
        std::smatch match;
        if (std::regex_search(source, match, pattern) && match[1].str().find("..") == string::npos)
            return match[1];
        return fallback;
    }

    inline std::map<string, string, std::less<>> read_vision_fields(const string& source)
    {
        std::map<string, string, std::less<>> fields;
        static const std::regex pattern(R"re((r_[a-zA-Z0-9_]+|[0-9]{3})\s+(?:"([^"]*)"|([^\s{}]+)))re");
        for (std::sregex_iterator i(source.begin(), source.end(), pattern), end; i != end; ++i)
            fields[(*i)[1].str()] = (*i)[2].matched ? (*i)[2].str() : (*i)[3].str();
        return fields;
    }
}
