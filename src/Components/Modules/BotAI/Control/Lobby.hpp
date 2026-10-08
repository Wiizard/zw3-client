#pragma once

#include "Components/Modules/BotAI/Control/Identity.hpp"

namespace Components::BotAI
{
	bool InstallLobby();

	Identity TakeLobbyIdentity(const char* const* takenNames, int takenCount);
}
