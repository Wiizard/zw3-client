#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <intrin.h>

namespace Game
{
    struct snd_alias_t { const char* aliasName; };
    bool mainThread = true;
    bool Sys_IsMainThread() { return mainThread; }
    enum CriticalSection { Sound = 0x29 };
    int soundLockDepth = 0;
    void Sys_EnterCriticalSection(CriticalSection section)
    {
        if (section != Sound) throw std::runtime_error("unexpected sound lock");
        ++soundLockDepth;
    }
    void Sys_LeaveCriticalSection(CriticalSection section)
    {
        if (section != Sound || soundLockDepth != 1) throw std::runtime_error("unbalanced sound lock");
        --soundLockDepth;
    }
    int localPlays = 0;
    int SND_PlayLocalSoundAliasByName(int, const char*, int) { ++localPlays; return 101; }
}

namespace TestSound
{
    struct Channel
    {
        unsigned char prefix[0x68]{};
        const Game::snd_alias_t* primary = nullptr;
        const Game::snd_alias_t* secondary = nullptr;
        int primaryOffset = 0;
        int secondaryOffset = 0;
        unsigned char suffix[0x28]{};
    };
    static_assert(sizeof(Channel) == 0xA8);
    static_assert(offsetof(Channel, secondary) == 0x70);
    std::array<Channel, 52> channels{};
    std::array<void*, 52> voices{};
}

namespace Utils::Hook
{
    std::uintptr_t Rebase(std::uintptr_t address)
    {
        if (address == 0x148BCEA68) return reinterpret_cast<std::uintptr_t>(TestSound::voices.data());
        if (address == 0x14651C808) return reinterpret_cast<std::uintptr_t>(TestSound::channels.data());
        return address;
    }
}

namespace Components::Menu
{
    using std::string_view;
    constexpr int zone_flag_level = 4;
    struct LogLine
    {
        template<typename T> LogLine& operator<<(const T&) { return *this; }
    };
    struct Backdrop
    {
        static void BeginZoneFree(int freed);
        static void EndZoneFree();
        static bool PreserveMenuMusic();
    };
    bool preserve = true;
    bool Backdrop::PreserveMenuMusic() { return preserve; }
    std::atomic<int> zone_frees{0}, menu_music{1};
    std::atomic<const Game::snd_alias_t*> resident_music[3]{};
    std::array<bool, 52> voices{};
    std::array<const Game::snd_alias_t*, 52> aliases{};
    int nativeStops = 0, captures = 0;
    void capture_resident_music() { ++captures; }
    const Game::snd_alias_t* stream_alias(int channel) { return voices.at(channel) ? aliases.at(channel) : nullptr; }
    void StopStream(int channel) { voices.at(channel) = false; }
    using stop_sounds_fn = void (*)(int);
    using play_alias_fn = int (*)(int, const Game::snd_alias_t*, int);
    int residentPlays = 0;
    int PlayResident(int, const Game::snd_alias_t*, int) { ++residentPlays; return 102; }
    stop_sounds_fn stop_stream = StopStream;
    void stop_menu_stream(int channel);
    void NativeStopSounds(int flags)
    {
        if (flags != 0) throw std::runtime_error("zone teardown must stop every loaded sound");
        ++nativeStops;
        for (int channel = 0; channel < 40; ++channel) voices[channel] = false;
        for (int channel = 40; channel < 52; ++channel) stop_menu_stream(channel);
        for (std::size_t channel = 0; channel < voices.size(); ++channel)
        {
            TestSound::voices[channel] = voices[channel] ? &voices : nullptr;
            // Match the native stop: the secondary alias survives teardown.
            if (!voices[channel]) TestSound::channels[channel].primary = nullptr;
        }
    }
    namespace Engine
    {
        template<typename T> T Bind(std::uintptr_t address)
        {
            if constexpr (std::is_same_v<T, stop_sounds_fn>)
            {
                if (address != 0x14024A2E0) throw std::runtime_error("unexpected native stop entry");
                return &NativeStopSounds;
            }
            else
            {
                if (address != 0x140246CC0) throw std::runtime_error("unexpected native play entry");
                return &PlayResident;
            }
        }
    }
    #include "MenuSoundLifetime.inc"
}

int main()
{
    using namespace Components::Menu;
    try
    {
        const auto require = [](bool value, const char* message)
        {
            if (!value) throw std::runtime_error(message);
        };
        Game::snd_alias_t residentTrack{"music_mainmenu_mp"}, mapTrack{"music_mainmenu_mp"}, mapEffect{"map_fx_loop"};
        resident_music[1].store(&residentTrack);
        const auto reset = [&]
        {
            voices.fill(false); aliases.fill(nullptr);
            TestSound::channels.fill({}); TestSound::voices.fill(nullptr);
            nativeStops = captures = residentPlays = Game::localPlays = 0; zone_frees.store(0);
            preserve = true; Game::mainThread = true; menu_music.store(1);
        };

        reset();
        Game::snd_alias_t thunder{"elm_thunder_strike"};
        TestSound::channels[3].primary = TestSound::channels[3].secondary = &thunder;
        TestSound::channels[3].secondaryOffset = 4;
        // A sound that finished before the free already has no voice or primary alias.
        TestSound::channels[17].secondary = &thunder;
        TestSound::channels[17].secondaryOffset = 4;
        TestSound::channels[44].primary = TestSound::channels[44].secondary = &residentTrack;
        TestSound::channels[44].secondaryOffset = 2;
        voices[3] = voices[17] = voices[44] = voices[45] = voices[46] = true;
        voices[17] = false;
        aliases[44] = &residentTrack; aliases[45] = &mapTrack; aliases[46] = &mapEffect;
        Backdrop::BeginZoneFree(zone_flag_level);
        require(!voices[3] && !voices[17], "loaded map/FX voices still reference samples at zone release");
        require(!voices[45] && !voices[46], "map-owned music and non-music streams must stop before release");
        require(!voices[44], "resident aliases must not keep queued PCM readers alive during a zone free");
        for (const auto channel : {3, 17})
        {
            const auto& stopped = TestSound::channels[channel];
            require(!stopped.primary && !stopped.secondary && stopped.primaryOffset == 0 && stopped.secondaryOffset == 0,
                "native alias restore would still look up unloaded elm_thunder_strike with offset 4");
        }
        require(!TestSound::channels[44].primary && !TestSound::channels[44].secondary,
            "stopped menu music retained aliases across zone release");
        require(play_menu_loop(0, "music_mainmenu_mp", 0) == -1 && residentPlays == 0,
            "loading-screen paint restarted resident music while its buffers were being released");
        require(play_menu_loop(0, "map_fx_loop", 0) == -1 && Game::localPlays == 0,
            "loading-screen paint started a map-owned loop during zone release");
        require(Game::soundLockDepth == 0, "sound cleanup left the mixer locked");
        require(nativeStops == 1 && zone_frees.load() == 1, "outer free must run native sound cleanup once");
        Backdrop::BeginZoneFree(zone_flag_level);
        require(nativeStops == 1, "nested frees must not repeat cleanup");
        Backdrop::EndZoneFree();
        require(zone_frees.load() == 1 && captures == 0, "nested completion must keep the free guard active");
        require(play_menu_loop(0, "music_mainmenu_mp", 0) == -1, "nested completion resumed music too early");
        Backdrop::EndZoneFree();
        require(zone_frees.load() == 0 && captures == 1, "outer completion must release the guard");
        require(play_menu_loop(0, "music_mainmenu_mp", 0) == 102 && residentPlays == 1,
            "normal menu painting did not resume resident music after zone release");

        reset(); voices[44] = true; aliases[44] = &residentTrack;
        Backdrop::BeginZoneFree(zone_flag_level | 2);
        require(!voices[44], "resident music must stop when front-end zones are also freed");
        Backdrop::EndZoneFree();

        reset(); voices[3] = voices[44] = true; aliases[44] = &residentTrack; preserve = false;
        Backdrop::BeginZoneFree(zone_flag_level);
        require(!voices[3] && !voices[44], "gameplay and shutdown must not preserve menu voices");
        Backdrop::EndZoneFree();

        reset(); voices[44] = true; aliases[44] = &residentTrack;
        stop_menu_stream(44);
        require(voices[44], "normal menu frames must still preserve requested music");
        std::cout << "Menu sound lifetime tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
