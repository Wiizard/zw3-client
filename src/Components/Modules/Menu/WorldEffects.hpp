#pragma once

namespace Components::Menu::Engine { struct refdef_t; }
namespace Components::Menu::WorldEffects
{
    void Load(const std::string& map);
    void Reset();
    void Draw(Engine::refdef_t& view);
}
