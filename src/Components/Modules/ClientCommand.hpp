#pragma once

#include "Command.hpp"
#include "Dvar.hpp"

namespace Components
{
	class ClientCommand : public Component
	{
	public:
		ClientCommand();

		static void Add(const char* name, const std::function<void(Game::gentity_s*, const Command::ServerParams*)>& callback);
		static bool CheatsOk(const Game::gentity_s* ent);
		static void SetCheatsForSpawn(bool isEnabled);

		static Dvar::Var sv_cheats;

	private:
		static std::unordered_map<std::string, std::function<void(Game::gentity_s*, const Command::ServerParams*)>> handlersSV;

		static bool cheatsEnabled;

		class CheatsScopedLock
		{
		public:
			CheatsScopedLock();
			~CheatsScopedLock();
		};

		static void ClientCommandStub(int clientNum);
		static void AddCheatCommands();

		static void AddScriptFunctions();
		static void AddScriptMethods();

		static void Cmd_Noclip_f(Game::gentity_s* ent, const Command::ServerParams* params);
		static void Cmd_UFO_f(Game::gentity_s* ent, const Command::ServerParams* params);
	};
}
