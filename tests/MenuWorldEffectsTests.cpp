#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

namespace TestFx
{
    struct Command { void* system; int client; int padding; };
    bool updating = false, drawing = false, camera = false;
    bool simulated = false, unbolted = false, drawCamera = false;
    int systems = 0, spawned = 0, submitted = 0, vertices = 0, listeners = 0, stopped = 0;
    void Wait()
    {
        updating = false;
        simulated = true;
    }
}

namespace Game
{
    struct FxEffectDef {};
    struct RefdefView { float org[3]{}; float axis[3][3]{}; };
    void R_WaitWorkerCmds() { TestFx::Wait(); }
}

namespace Components::Menu::Engine
{
    struct Film { bool enabled{}; float brightness{}; float tintDark[3]{}, tintMedium[3]{}; };
    struct Glow {};
    struct refdef_t
    {
        Film film; Glow glow;
        struct { float diffuseScale{}, specularScale{}; } charPrimaryLightScale;
        Game::RefdefView view;
        float viewOffset[3]{};
        int time = 100000;
    };
    void Init(int) { ++TestFx::systems; }
    void Stop(int) { ++TestFx::stopped; TestFx::spawned = 0; }
    void AddNonSprites(int) {}
    void SetTime(int, int)
    {
        TestFx::simulated = TestFx::unbolted = TestFx::drawCamera = TestFx::camera = false;
    }
    void* Spawn(int, const Game::FxEffectDef*, int, const float*, const void*)
    {
        ++TestFx::spawned;
        return &TestFx::systems;
    }
    void Fill(int client, void* data)
    {
        *static_cast<TestFx::Command*>(data) = {&TestFx::systems, client, 0};
    }
    void Update(void* data)
    {
        const auto& command = *static_cast<TestFx::Command*>(data);
        Require(command.system == &TestFx::systems && command.client == 0, "invalid update command");
        TestFx::updating = true;
    }
    void Sprites(void* data)
    {
        const auto& command = *static_cast<TestFx::Command*>(data);
        Require(command.system == &TestFx::systems && command.client == 0, "invalid sprite command");
        Require(!TestFx::updating && TestFx::simulated && TestFx::unbolted,
            "sprite geometry read effects before both FX simulation passes completed");
        Require(TestFx::drawCamera, "sprite geometry used an uninitialized draw camera");
        TestFx::drawing = true;
    }
    void CgameWorker(void*) { throw std::runtime_error("worker 8 requires a player snapshot absent from the frontend"); }
    void UpdateUnbolted(void* system, int client, bool bolted)
    {
        Require(system == &TestFx::systems && client == 0 && !bolted, "invalid standalone FX update");
        Require(!TestFx::updating && TestFx::simulated, "FX simulation is still running on a worker");
        TestFx::unbolted = true;
    }
    void CopyCamera(int)
    {
        Require(TestFx::camera, "draw camera copied before menu camera setup");
        TestFx::drawCamera = true;
    }
    void FillVertices(void* source, void* target)
    {
        // The native command includes a full 3x3 axis at 0x38, ending at
        // 0x5C, and has an aligned size of 0x60 rather than 0x50.
        std::memset(target, 0, 0x60);
        *static_cast<void**>(target) = static_cast<TestFx::Command*>(source)->system;
    }
    void GenerateVertices(void* data)
    {
        Require(static_cast<TestFx::Command*>(data)->system == &TestFx::systems, "invalid vertex command");
        Require(TestFx::drawing && TestFx::drawCamera, "vertex generation ran before particle sorting");
        TestFx::vertices += TestFx::spawned * 4;
        ++TestFx::submitted;
        TestFx::drawing = false;
    }
    float FarPlane() { return 10000; }
    void Camera(int, const refdef_t*, const Game::RefdefView*, float, bool) { TestFx::camera = true; }
    void Listener(int, int, const float*, const void*, const float*)
    {
        Require(!TestFx::updating && !TestFx::drawing, "scene returned with worker commands still pending");
        ++TestFx::listeners;
    }
    template<typename T> T Bind(std::uintptr_t address)
    {
        if constexpr (std::is_same_v<T, void(*)(int)>)
        {
            if (address == 0x140157D30) return Init;
            if (address == 0x140157E90) return Stop;
            if (address == 0x14015C1C0) return CopyCamera;
            if (address == 0x14015FD50 || address == 0x14013AE70) return AddNonSprites;
        }
        else if constexpr (std::is_same_v<T, void(*)(int, int)>)
        { if (address == 0x14015DC20) return SetTime; }
        else if constexpr (std::is_same_v<T, decltype(&Spawn)>)
        { if (address == 0x14015AC60) return Spawn; }
        else if constexpr (std::is_same_v<T, decltype(&Fill)>)
        { if (address == 0x14015C250) return Fill; }
        else if constexpr (std::is_same_v<T, void(*)(void*)>)
        {
            if (address == 0x14002DE10) return Update;
            if (address == 0x14002DE30) return CgameWorker;
            if (address == 0x14015DC60) return Sprites;
            if (address == 0x14013D040) return GenerateVertices;
        }
        else if constexpr (std::is_same_v<T, decltype(&FillVertices)>)
        { if (address == 0x14013C3C0) return FillVertices; }
        else if constexpr (std::is_same_v<T, decltype(&UpdateUnbolted)>)
        { if (address == 0x14015E7D0) return UpdateUnbolted; }
        else if constexpr (std::is_same_v<T, decltype(&FarPlane)>)
        { if (address == 0x14002DD90) return FarPlane; }
        else if constexpr (std::is_same_v<T, decltype(&Camera)>)
        { if (address == 0x14015D910) return Camera; }
        else if constexpr (std::is_same_v<T, decltype(&Listener)>)
        { if (address == 0x140249760) return Listener; }
        throw std::runtime_error("unexpected native FX entry");
    }
}

namespace Components::Menu::WorldEffects
{
    using namespace Engine;
    struct Placement
    {
        std::array<float, 3> origin{}, angles{};
        float delay = 0, repeat = 0;
    };
    struct effect { Game::FxEffectDef* definition; Placement placement; int next = 0; };
    std::vector<effect> effects;
    Film film{}; Glow glow{};
    float diffuse = 1, specular = 1;
    bool loaded = false, active = false;
    auto angles_to_axis(const std::array<float, 3>&) { return std::array<float, 9>{}; }
    #include "MenuWorldEffects.inc"
}

int main()
{
    using namespace Components::Menu::WorldEffects;
    try
    {
        refdef_t view;
        Draw(view);
        Require(TestFx::systems == 0, "unloaded maps must not start FX");
        loaded = true;
        Draw(view);
        Require(TestFx::systems == 0, "maps without authored FX must not start workers");
        Game::FxEffectDef fire, smoke;
        effects = {{&fire, {}}, {&smoke, {}}};
        for (int frame = 0; frame < 3; ++frame) { view.time += 16; Draw(view); }
        Require(TestFx::systems == 1 && TestFx::spawned == 2, "persistent effects were restarted every frame");
        Require(TestFx::submitted == 3 && TestFx::vertices == 24,
            "live effects produced no sprite geometry on one or more menu frames");
        Require(TestFx::listeners == 3, "positional audio lost its per-frame listener update");
        Reset();
        Draw(view);
        Require(!active && effects.empty() && TestFx::stopped == 1, "map release retained native FX");
        Require(TestFx::submitted == 3, "old map particles were submitted after reset");
        std::cout << "Menu world FX simulation, sprite geometry, frame completion and teardown tests passed\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
