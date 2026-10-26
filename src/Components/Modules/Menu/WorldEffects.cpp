#include "STDInclude.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "../Dedicated.hpp"
#include "../ZoneBuilder.hpp"
#include "Engine.hpp"
#include "EnvironmentData.hpp"
#include "Camera.hpp"
#include "WorldEffects.hpp"
#include <zlib.h>

namespace Components::Menu::WorldEffects
{
    namespace
    {
        using namespace BackdropData;
        using namespace Engine;
        struct effect { Game::FxEffectDef* definition; placed_effect placement; int next = 0; };
        std::vector<effect> effects;
        Film film {};
        Glow glow {};
        float diffuse = 1, specular = 1;
        bool loaded = false, active = false;

        std::string read(const std::string& path)
        {
            FileSystem::File file(path);
            if (file.Exists()) return file.GetBuffer();
            auto* entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, path.c_str());
            if (!entry || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, path.c_str())) return {};
            const auto* raw = entry->asset.header.rawfile;
            if (!raw || !raw->buffer || raw->len <= 0 || raw->len > 2 * 1024 * 1024 ||
                raw->compressedLen < 0 || raw->compressedLen > 2 * 1024 * 1024) return {};
            if (!raw->compressedLen) return {raw->buffer, static_cast<size_t>(raw->len)};
            // Read converted fastfile scripts with the client's zlib. Keep
            // output bounded by the asset's declared uncompressed length.
            std::string text(static_cast<size_t>(raw->len), '\0');
            uLongf length = static_cast<uLongf>(text.size());
            if (uncompress(reinterpret_cast<Bytef*>(text.data()), &length,
                reinterpret_cast<const Bytef*>(raw->buffer), static_cast<uLong>(raw->compressedLen)) != Z_OK) return {};
            text.resize(length);
            return text;
        }

        bool supported()
        {
            // Verified native IW4 x64 entry points and the five-argument
            // FX_PlayOrientedEffect call used by CG's createfx reader.
            const unsigned char init[] {0x48,0x89,0x5c,0x24,0x08};
            const unsigned char play[] {0x48,0x83,0xec,0x58};
            return Utils::Hook::MatchesBytes(0x140157D30, init, sizeof(init)) &&
                   Utils::Hook::MatchesBytes(0x14015AC60, play, sizeof(play)) &&
                   Utils::Hook::BranchesTo(0x1400B839D, 0x14015DC20, false) &&
                   Utils::Hook::BranchesTo(0x1400B8B7D, 0x14015D910, false);
        }
    }

    void Reset()
    {
        loaded = false;
        if (active)
        {
            active = false;
            Game::R_WaitWorkerCmds();
            Bind<void(*)(int)>(0x140157E90)(0);
            // StopAllEffects stops emission but retains dying elements and
            // child runners. Reinitialize before their zone is freed, so the
            // database's FX hot-reload pass cannot revisit old definitions.
            Bind<void(*)(int)>(0x140157D30)(0);
        }
        effects.clear();
    }

    void Load(const std::string& map)
    {
        Reset();
        film = {}; glow = {}; diffuse = specular = 1;
        film.contrast = 1;
        std::ranges::fill(film.tintDark, 1.0f);
        std::ranges::fill(film.tintMedium, 1.0f);
        std::ranges::fill(film.tintLight, 1.0f);

        const std::string mapScript = environment_source(read("maps/mp/" + map + ".gsc") + "\n" + read("maps/" + map + ".gsc"));
        const std::string art = environment_source(read("maps/createart/" + map + "_art.gsc"));
        const std::string vision = read_vision_name(mapScript, read_vision_name(art, map));
        std::string visionText = read("vision/" + vision + ".vision");
        if (visionText.empty()) visionText = read("vision/default.vision");
        const auto fields = read_vision_fields(environment_source(visionText));
        const auto scalar = [&](const char* name, const char* legacy, float fallback)
        {
            auto i = fields.find(name);
            if (i == fields.end()) i = fields.find(legacy);
            if (i == fields.end()) return fallback;
            char* end = nullptr;
            const float value = std::strtof(i->second.c_str(), &end);
            return end != i->second.c_str() && *end == '\0' && std::isfinite(value) ? value : fallback;
        };
        const auto tint = [&](const char* name, const char* legacy, float* target)
        {
            auto i = fields.find(name);
            if (i == fields.end()) i = fields.find(legacy);
            if (i != fields.end()) if (const auto value = environment_vector(i->second)) std::copy(value->begin(), value->end(), target);
        };
        film.enabled = scalar("r_filmEnable", "520", 0) != 0;
        film.brightness = scalar("r_filmBrightness", "521", 0);
        film.contrast = scalar("r_filmContrast", "522", 1);
        film.desaturation = scalar("r_filmDesaturation", "523", 0);
        film.desaturationDark = scalar("r_filmDesaturationDark", "524", 0);
        film.invert = scalar("r_filmInvert", "525", 0) != 0;
        tint("r_filmDarkTint", "528", film.tintDark);
        tint("r_filmMediumTint", "527", film.tintMedium);
        tint("r_filmLightTint", "526", film.tintLight);
        glow.enabled = scalar("r_glow", "511", 0) != 0;
        glow.radius = scalar("r_glowRadius0", "516", 5);
        glow.bloomCutoff = scalar("r_glowBloomCutoff", "512", 0.8f);
        glow.bloomDesaturation = scalar("r_glowBloomDesaturation", "513", 0);
        glow.bloomIntensity = scalar("r_glowBloomIntensity0", "514", 0);
        if (scalar("r_primaryLightUseTweaks", "529", 0) != 0)
        {
            diffuse = scalar("r_primaryLightTweakDiffuseStrength", "530", 1);
            specular = scalar("r_primaryLightTweakSpecularStrength", "531", 1);
        }

        const std::string fx = environment_source(read("maps/mp/" + map + "_fx.gsc") + "\n" + read("maps/" + map + "_fx.gsc"));
        const std::string create = environment_source(read("maps/createfx/" + map + "_fx.gsc"));
        effect_names names;
        read_effect_names(fx, names);
        read_effect_names(mapScript, names);
        auto placements = read_placed_effects(fx + "\n" + create + "\n" + mapScript, names);
        if (cm && cm->mapEnts && cm->mapEnts->entityString)
        {
            auto entityEffects = read_entity_effects(mapScript, cm->mapEnts->entityString, names);
            placements.insert(placements.end(), entityEffects.begin(), entityEffects.end());
        }
        const bool nativeFx = supported();
        if (nativeFx) for (auto& placement : placements)
        {
            auto* entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_FX, placement.name.c_str());
            if (!entry || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_FX, placement.name.c_str())) continue;
            if (std::ranges::any_of(effects, [&](const effect& other) { return other.placement.name == placement.name && other.placement.origin == placement.origin; })) continue;
            effects.push_back({entry->asset.header.fx, std::move(placement)});
        }
        loaded = true;
        Logger::Print("backdrop environment: {} uses vision '{}' ({} fields), {} of {} authored FX placements ({} definitions, script bytes {}/{}, native {})\n", map, vision, fields.size(), effects.size(), placements.size(), names.size(), fx.size(), create.size(), nativeFx);
    }

    void Draw(Engine::refdef_t& view)
    {
        if (!loaded) return;
        view.film = film;
        view.glow = glow;
        view.charPrimaryLightScale.diffuseScale = diffuse;
        view.charPrimaryLightScale.specularScale = specular;
        if (effects.empty()) return;
        if (!active)
        {
            Bind<void(*)(int)>(0x140157D30)(0);
            Bind<void(*)(int, int)>(0x14015DC20)(0, view.time);
            // The native effect pool holds 1024 root effects, including runners.
            // Prioritize the view's neighbourhood and leave room for child FX.
            std::ranges::sort(effects, {}, [&](const effect& e)
            {
                float distance = 0;
                for (size_t i = 0; i < 3; ++i) { const float d = e.placement.origin[i] - view.view.org[i]; distance += d * d; }
                return distance;
            });
            if (effects.size() > 512) effects.resize(512);
            for (effect& e : effects)
            {
                const auto axis = angles_to_axis(e.placement.angles);
                Bind<void*(*)(int, const Game::FxEffectDef*, int, const float*, const void*)>(0x14015AC60)(
                    0, e.definition, view.time + static_cast<int>(e.placement.delay * 1000), e.placement.origin.data(), axis.data());
                if (e.placement.repeat > 0) e.next = view.time + static_cast<int>(e.placement.repeat * 1000);
            }
            active = true;
        }
        Bind<void(*)(int, int)>(0x14015DC20)(0, view.time);
        for (effect& e : effects) if (e.placement.repeat > 0 && view.time >= e.next)
        {
            const auto axis = angles_to_axis(e.placement.angles);
            Bind<void*(*)(int, const Game::FxEffectDef*, int, const float*, const void*)>(0x14015AC60)(
                0, e.definition, view.time, e.placement.origin.data(), axis.data());
            e.next = view.time + static_cast<int>(e.placement.repeat * 1000);
        }
        struct update_command { void* system; int localClient; int padding; } command {};
        Bind<void(*)(int, void*)>(0x14015C250)(0, &command);
        Bind<void(*)(void*)>(0x14002DE10)(&command);
        const float farPlane = Bind<float(*)()>(0x14002DD90)();
        Bind<void(*)(int, const Engine::refdef_t*, const Game::RefdefView*, float, bool)>(0x14015D910)(0, &view, &view.view, farPlane, false);
        Bind<void(*)(int)>(0x14015FD50)(0);
        Bind<void(*)(int)>(0x14013AE70)(0);
        Game::R_WaitWorkerCmds();
        // Map FX use positional audio even though no player client is active.
        Bind<void(*)(int, int, const float*, const void*, const float*)>(0x140249760)(
            0, 0, view.view.org, view.view.axis, view.viewOffset);
    }
}
