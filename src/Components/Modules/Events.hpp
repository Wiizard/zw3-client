#pragma once

namespace Game
{
	struct client_s;
}

namespace Components
{
	class Events : public Component
	{
	public:
		using Callback = std::vector<std::function<void()>>;
		using ClientConnectCallback = std::vector<std::function<void(Game::client_s* cl)>>;
		using ClientCallback = std::vector<std::function<void(int clientNum)>>;
		using ClientCmdCallback = std::vector<std::function<void(Game::usercmd_s* cmd)>>;
		using CLDisconnectCallback = std::vector<std::function<void(bool wasConnected)>>;

		Events();

		static bool IsInstalled();

		static void OnClientDisconnect(const std::function<void(int clientNum)>& callback);

		static void OnClientConnect(const std::function<void(Game::client_s* cl)>& callback);

		static void OnSteamDisconnect(const std::function<void()>& callback);

		static void OnCLDisconnected(const std::function<void(bool)>& callback);

		static void OnVMShutdown(const std::function<void()>& callback);

		static void OnClientInit(const std::function<void()>& callback);

		static void AfterUIInit(const std::function<void()>& callback);

		static void OnClientCmdButtons(const std::function<void(Game::usercmd_s*)>& callback);

		static void OnClientKeyMove(const std::function<void(Game::usercmd_s*)>& callback);

		static void OnSVInit(const std::function<void()>& callback);

		static void OnDvarInit(const std::function<void()>& callback);

		static void OnNetworkInit(const std::function<void()>& callback);

		static void OnCGameInit(const std::function<void()>& callback);

	private:
		static bool isInstalled;

		static Utils::Concurrency::Container<ClientCallback> clientDisconnectTasks;
		static Utils::Concurrency::Container<ClientConnectCallback> clientConnectTasks;
		static Utils::Concurrency::Container<Callback> steamDisconnectTasks;
		static Utils::Concurrency::Container<Callback> shutdownSystemTasks;
		static Utils::Concurrency::Container<Callback> clientInitTasks;
		static Utils::Concurrency::Container<Callback> serverInitTasks;
		static Utils::Concurrency::Container<Callback> dvarInitTasks;
		static Utils::Concurrency::Container<Callback> networkInitTasks;
		static Utils::Concurrency::Container<Callback> cgameInitTasks;
		static Utils::Concurrency::Container<Callback> uiInitTasks;
		static Utils::Concurrency::Container<CLDisconnectCallback> disconnectedTasks;

		static ClientCmdCallback clientCmdButtonsTasks;
		static ClientCmdCallback clientKeyMoveTasks;

		static void ClientDisconnect_Hook(int clientNum);
		static void SV_UserinfoChanged_Hook(Game::client_s* cl);
		static void CL_SteamDisconnect_Hook();
		static void Scr_ShutdownSystem_Hook(unsigned char sys);
		static void CL_InitOnceForAllClients_Hook();
		static void CL_CmdButtons_Hook(int localClientNum, Game::usercmd_s* cmd);
		static void CL_KeyMove_Hook(int localClientNum, Game::usercmd_s* cmd);
		static int CL_InitCGame_Hook();
		static void Com_InitDvars_Hook();
		static void SV_InitGameMode_Hook();
		static void NET_Config_Hook(int enableNetworking);
		static void UI_Init_Hook(int localClientNum);
		static void CL_Disconnect_Hook(int localClientNum);
	};
}
