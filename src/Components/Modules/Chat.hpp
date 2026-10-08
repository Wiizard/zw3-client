#pragma once

#include "Dvar.hpp"

namespace Game
{
	struct client_s;
	struct gentity_s;
}

namespace Components
{
	class Chat : public Component
	{
	public:
		Chat();

		static bool IsMuted(const Game::gentity_s* ent);
		static bool IsMuted(const Game::client_s* cl);

	private:
		static Dvar::Var cg_chatWidth;
		static Dvar::Var sv_disableChat;
		static Dvar::Var sv_sayName;

		static bool shouldSendChat;

		static bool canAddCallback;
		static std::vector<Game::Scripting::Function> sayCallbacks;

		using MuteList = std::unordered_set<std::uint64_t>;
		static Utils::Concurrency::Container<MuteList> mutedList;
		static const char* mutedListFile;

		static std::unique_lock<Utils::NamedMutex> Lock();
		static void SaveMutedList(const MuteList& list);
		static void LoadMutedList();

		static Utils::Hook hooks[9];

		static void EvaluateSay(const char* text, const Game::gentity_s* player, int mode);

		static int GetCallbackReturn();
		static int ChatCallback(const Game::gentity_s* self, const char* codePos, const char* message, int mode);
		static void AddScriptFunctions();

		static void Cmd_Say_f_Hook(Game::gentity_s* ent, int mode, int arg0);
		static const char* ConcatArgs_Hook(int start);
		static void SV_GameSendServerCommand_Hook(int clientNum, int type, const char* text);
		static void G_Say_Hook(Game::gentity_s* ent, Game::gentity_s* target, int mode, const char* chatText);
		static void G_SayTo_Hook(Game::gentity_s* ent, Game::gentity_s* other, int mode, int color,
			const char* teamString, const char* name, const char* message);

		static bool CL_IsMessageFromMutedUser(const std::string& text);
		static void CG_AddToTeamChat_Hook(int localClientNum, const char* text);

		static void MuteClient(const Game::client_s* client);
		static void UnmuteClient(const Game::client_s* client);
		static void UnmuteInternal(std::uint64_t id, bool everyone = false);

		static void AddServerCommands();
		static void RegisterDvars();
	};
}
