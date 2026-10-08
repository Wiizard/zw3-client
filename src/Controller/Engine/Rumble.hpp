#pragma once

#include "Controller/Types.hpp"

#include "Controller/Haptic/Effect.hpp"

namespace Controller::Engine
{
	bool TryEffectFromRumble(const Game::RumbleInfo& info, float scale, bool shouldLoop, Haptic::Effect& out) noexcept;
}
