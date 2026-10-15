#pragma once

namespace Components::ZombieNavigation
{
	void RegisterScriptFunctions();
	std::optional<std::string> PatchScript(std::string_view filename, std::string_view source);
}
