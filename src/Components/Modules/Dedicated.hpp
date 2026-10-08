#pragma once

#include "Dvar.hpp"

#include "Steam/Steam.hpp"

namespace Components
{
	class Dedicated : public Component
	{
	public:
		Dedicated();

		static ::Steam::SteamID playerGuids[Game::MAX_CLIENTS][2];
		static Dvar::Var sv_lanOnly;
		static Dvar::Var sv_motd;
		static Dvar::Var com_logFilter;

		static Dvar::Var zwnet_show_in_server_browser;
		static Dvar::Var zwnet_match_ended;
		static Dvar::Var zwnet_match_id;
		static Dvar::Var zwnet_selected_map;

		static const Game::dvar_t* com_dedicated;

		static bool IsEnabled();
		static bool IsRunning();

		static void Heartbeat();

	private:
		static void TransmitGuids();

		static bool ApplyEnginePatches();
		static void InitDedicatedServer();
		static void PostInitialization();
		static void Com_EventLoop_Hk();
		static void TimeWrapStub(int code, const char* message);
	};
}
