#pragma once

#include "Command.hpp"

namespace Components
{
	class ServerCommands : public Component
	{
	public:
		using Handler = std::function<bool(const Command::Params*)>;

		ServerCommands();

		static void OnCommand(std::int32_t command, const Handler& callback);

	private:
		static std::unordered_map<std::int32_t, std::vector<Handler>> commands;
		static Utils::Hook serverCommandHook;

		static bool OnServerCommand();
		static void CG_ServerCommand_Hook(int localClientNum);
	};
}
