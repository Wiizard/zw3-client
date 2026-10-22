#include "STDInclude.hpp"

#include "Events.hpp"
#include "Flags.hpp"
#include "Logger.hpp"

#include <intrin.h>

namespace Components
{
	bool Events::isInstalled = false;

	Utils::Concurrency::Container<Events::ClientCallback> Events::clientDisconnectTasks;
	Utils::Concurrency::Container<Events::ClientConnectCallback> Events::clientConnectTasks;
	Utils::Concurrency::Container<Events::Callback> Events::steamDisconnectTasks;
	Utils::Concurrency::Container<Events::Callback> Events::shutdownSystemTasks;
	Utils::Concurrency::Container<Events::Callback> Events::clientInitTasks;
	Utils::Concurrency::Container<Events::Callback> Events::serverInitTasks;
	Utils::Concurrency::Container<Events::Callback> Events::dvarInitTasks;
	Utils::Concurrency::Container<Events::Callback> Events::networkInitTasks;
	Utils::Concurrency::Container<Events::Callback> Events::cgameInitTasks;
	Utils::Concurrency::Container<Events::Callback> Events::uiInitTasks;
	Utils::Concurrency::Container<Events::CLDisconnectCallback> Events::disconnectedTasks;

	Events::ClientCmdCallback Events::clientCmdButtonsTasks;
	Events::ClientCmdCallback Events::clientKeyMoveTasks;

	constexpr std::uintptr_t ClientDisconnect = 0x1401961C0;

	constexpr std::uintptr_t SV_FreeClient_ClientDisconnectCall = 0x1402383F6;
	constexpr std::uintptr_t SV_FreeClients_ClientDisconnectCall = 0x14023851B;

	constexpr std::uintptr_t SV_UserinfoChanged = 0x1402398B0;
	constexpr std::uintptr_t SV_DirectConnect_UserinfoCall = 0x140237E6E;

	constexpr std::uintptr_t CL_SteamDisconnect = 0x14024BCF0;
	constexpr std::uintptr_t CL_Disconnect_SteamDisconnectCall = 0x1400F8E5F;

	constexpr std::uintptr_t Scr_ShutdownSystem = 0x14022ADE0;
	constexpr std::uintptr_t G_LoadGame_ShutdownSystemCall = 0x1401A0F06;
	constexpr std::uintptr_t G_ShutdownGame_ShutdownSystemCall = 0x14019F43E;

	constexpr std::uintptr_t CL_InitOnceForAllClients = 0x1400FAF00;
	constexpr std::uintptr_t Com_Init_ClientInitCall = 0x1401F59AA;

	constexpr std::uintptr_t CL_CmdButtons = 0x1400F5B90;
	constexpr std::uintptr_t CL_KeyMove = 0x1400F6E00;
	constexpr std::uintptr_t CL_CreateCmd_CmdButtonsCall = 0x1400F7BC0;
	constexpr std::uintptr_t CL_CreateCmd_KeyMoveCall = 0x1400F7BCA;

	constexpr std::uintptr_t Sys_Milliseconds = 0x1402A8620;
	constexpr std::uintptr_t CL_InitCGame_MillisecondsCall = 0x1400F4D7D;

	constexpr std::uintptr_t Com_InitDvars = 0x1401F4C70;
	constexpr std::uintptr_t Com_Init_InitDvarsCall = 0x1401F5455;

	constexpr std::uintptr_t SV_InitGameMode = 0x1402334C0;
	constexpr std::uintptr_t SV_Init_InitGameModeCall = 0x14023A2AB;

	constexpr std::uintptr_t NET_Config = 0x1402A7550;
	constexpr std::uintptr_t NET_Init_ConfigJump = 0x1402A7C82;
	constexpr std::uintptr_t ip_socket = 0x14678C430;

	constexpr std::uintptr_t UI_Init = 0x14026F440;
	constexpr std::uintptr_t CL_InitUI_UI_InitCall = 0x140101F26;

	constexpr std::uintptr_t CL_Disconnect = 0x1400F8E20;
	constexpr std::uintptr_t Com_ErrorEntered = 0x1401F3F40;
	static const std::uint8_t comErrorEnteredBytes[] = { 0x83, 0x3D, 0xD5, 0x5B, 0x9E, 0x01, 0x00, 0x0F, 0x9F, 0xC0, 0xC3 };
	static thread_local const Events::NativeDisconnectError* currentNativeDisconnectError = nullptr;

	static int ReadNativeErrorEntered()
	{
		static const bool canReadError = Utils::Hook::MatchesBytes(Com_ErrorEntered, comErrorEnteredBytes, sizeof(comErrorEnteredBytes));
		return canReadError ? (reinterpret_cast<bool(*)()>(Utils::Hook::Rebase(Com_ErrorEntered))() ? 1 : 0) : -1;
	}

	static std::string ReadNativeErrorText(const char* name)
	{
		const auto* const dvar = Game::Dvar_FindVar(name);
		if (!dvar || dvar->type != Game::DVAR_TYPE_STRING || !dvar->current.string) return {};
		constexpr std::size_t maximumLength = 4096;
		const auto length = strnlen(dvar->current.string, maximumLength + 1);
		if (length > maximumLength) return {};
		return std::string(dvar->current.string, length);
	}

	// Never print arbitrary server/error text: it can contain addresses or
	// credentials. Only fixed engine localization keys reach the diagnostic log.
	static constexpr std::string_view ClassifyDisconnectReason(std::string_view value)
	{
		if (value.empty()) return "empty";
		if (value.front() == '\x15' || value.front() == '@') value.remove_prefix(1);
		constexpr std::string_view known[] = {
			"EXE_DISCONNECTED", "EXE_DISCONNECTEDFROMOWNLISTENSERVER", "EXE_DEMONWARE_DISCONNECT",
			"EXE_TIMEDOUT", "EXE_ERR_SERVER_TIMEOUT", "EXE_LOSTCONNECTION", "EXE_LOSTRELIABLECOMMANDS",
			"EXE_SERVER_DISCONNECTED", "EXE_SERVERDISCONNECTREASON", "EXE_SERVERQUIT", "EXE_SERVERKILLED",
			"EXE_SERVERISFULL", "EXE_PLAYERKICKED", "EXE_SERVER_IS_DIFFERENT_VER", "EXE_SERVERCOMMANDOVERFLOW",
			"EXE_SERVERMESSAGEOVERFLOW", "EXE_SERVERRESTARTMISC", "EXE_SERVERRESTARTTIMEWRAP",
			"EXE_MIGRATION_IN_PROGRESS", "EXE_MIGRATIONABORTED", "EXE_MIGRATIONLIMIT"
		};
		for (const auto reason : known) if (value == reason) return reason;
		return "other";
	}

	static_assert(ClassifyDisconnectReason("EXE_TIMEDOUT") == "EXE_TIMEDOUT");
	static_assert(ClassifyDisconnectReason("@EXE_SERVERQUIT") == "EXE_SERVERQUIT");
	static_assert(ClassifyDisconnectReason("\x15" "EXE_DISCONNECTED") == "EXE_DISCONNECTED");
	static_assert(ClassifyDisconnectReason("") == "empty");
	static_assert(ClassifyDisconnectReason("https://private.invalid/?connect_ticket=secret") == "other");
	static_assert(ClassifyDisconnectReason("EXE_TIMEDOUT secret") == "other");

	static std::string_view ReadDisconnectReason(const char* name)
	{
		const auto* const dvar = Game::Dvar_FindVar(name);
		if (!dvar || dvar->type != Game::DVAR_TYPE_STRING || !dvar->current.string) return "unavailable";
		const auto length = strnlen(dvar->current.string, 129);
		if (length > 128) return "other";
		return ClassifyDisconnectReason(std::string_view(dvar->current.string, length));
	}

	constexpr std::uintptr_t CL_DisconnectCalls[] =
	{
		0x1400F89B7,
		0x1400F8D23,
		0x1400F924C,
		0x1400F92DB,
		0x1400F954A,
		0x1400F95E2,
		0x1400FC5AD,
		0x1400FD614,
		0x1401F69FB,
		0x14023B1B8,
	};

	struct clientUIActive_t
	{
		bool active;
		bool isRunning;
		unsigned char pad[0xB46];
		int connectionState;
	};

	AssertOffset(clientUIActive_t, isRunning, 0x1);
	AssertOffset(clientUIActive_t, connectionState, 0xB48);

	constexpr std::uintptr_t clientUIActives = 0x1406CE1B0;
	constexpr int CA_CONNECTING = 3;

	struct HookSite
	{
		std::uintptr_t site;
		std::uintptr_t callee;
		void* replacement;
		bool isJump;
	};

	constexpr std::size_t hookCount = 14 + std::size(CL_DisconnectCalls);

	static Utils::Hook hooks[hookCount];

	bool Events::IsInstalled()
	{
		return isInstalled;
	}

	void Events::OnClientDisconnect(const std::function<void(int clientNum)>& callback)
	{
		clientDisconnectTasks.Access([&callback](ClientCallback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnClientConnect(const std::function<void(Game::client_s* cl)>& callback)
	{
		clientConnectTasks.Access([&callback](ClientConnectCallback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnSteamDisconnect(const std::function<void()>& callback)
	{
		steamDisconnectTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnCLDisconnected(const std::function<void(bool)>& callback)
	{
		disconnectedTasks.Access([&callback](CLDisconnectCallback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	Events::NativeDisconnectError Events::CurrentNativeDisconnectError()
	{
		return currentNativeDisconnectError ? *currentNativeDisconnectError : NativeDisconnectError{};
	}

	void Events::OnVMShutdown(const std::function<void()>& callback)
	{
		shutdownSystemTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnClientInit(const std::function<void()>& callback)
	{
		clientInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::AfterUIInit(const std::function<void()>& callback)
	{
		uiInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnClientCmdButtons(const std::function<void(Game::usercmd_s*)>& callback)
	{
		clientCmdButtonsTasks.emplace_back(callback);
	}

	void Events::OnClientKeyMove(const std::function<void(Game::usercmd_s*)>& callback)
	{
		clientKeyMoveTasks.emplace_back(callback);
	}

	void Events::OnSVInit(const std::function<void()>& callback)
	{
		serverInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnDvarInit(const std::function<void()>& callback)
	{
		dvarInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnNetworkInit(const std::function<void()>& callback)
	{
		networkInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::OnCGameInit(const std::function<void()>& callback)
	{
		cgameInitTasks.Access([&callback](Callback& tasks)
		{
			tasks.emplace_back(callback);
		});
	}

	void Events::ClientDisconnect_Hook(const int clientNum)
	{
		clientDisconnectTasks.Access([clientNum](ClientCallback& tasks)
		{
			for (const auto& func : tasks)
			{
				func(clientNum);
			}
		});

		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(ClientDisconnect))(clientNum);
	}

	void Events::SV_UserinfoChanged_Hook(Game::client_s* cl)
	{
		clientConnectTasks.Access([cl](ClientConnectCallback& tasks)
		{
			for (const auto& func : tasks)
			{
				func(cl);
			}
		});

		reinterpret_cast<void(*)(Game::client_s*)>(Utils::Hook::Rebase(SV_UserinfoChanged))(cl);
	}

	void Events::CL_SteamDisconnect_Hook()
	{
		steamDisconnectTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}
		});

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_SteamDisconnect))();
	}

	void Events::Scr_ShutdownSystem_Hook(const unsigned char sys)
	{
		shutdownSystemTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}
		});

		reinterpret_cast<void(*)(unsigned char)>(Utils::Hook::Rebase(Scr_ShutdownSystem))(sys);
	}

	void Events::CL_InitOnceForAllClients_Hook()
	{
		clientInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}

			tasks = {};
		});

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_InitOnceForAllClients))();
	}

	void Events::CL_CmdButtons_Hook(const int localClientNum, Game::usercmd_s* cmd)
	{
		reinterpret_cast<void(*)(int, Game::usercmd_s*)>(Utils::Hook::Rebase(CL_CmdButtons))(localClientNum, cmd);

		for (const auto& func : clientCmdButtonsTasks)
		{
			func(cmd);
		}
	}

	void Events::CL_KeyMove_Hook(const int localClientNum, Game::usercmd_s* cmd)
	{
		reinterpret_cast<void(*)(int, Game::usercmd_s*)>(Utils::Hook::Rebase(CL_KeyMove))(localClientNum, cmd);

		for (const auto& func : clientKeyMoveTasks)
		{
			func(cmd);
		}
	}

	int Events::CL_InitCGame_Hook()
	{
		cgameInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}
		});

		return Game::Sys_Milliseconds();
	}

	void Events::Com_InitDvars_Hook()
	{
		dvarInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}

			tasks = {};
		});

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_InitDvars))();
	}

	void Events::SV_InitGameMode_Hook()
	{
		serverInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}

			tasks = {};
		});

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SV_InitGameMode))();
	}

	void Events::NET_Config_Hook(const int enableNetworking)
	{
		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(NET_Config))(enableNetworking);

		if (!Utils::Hook::Get<std::uint64_t>(ip_socket))
		{
			return;
		}

		networkInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}

			tasks = {};
		});
	}

	void Events::UI_Init_Hook(const int localClientNum)
	{
		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(UI_Init))(localClientNum);

		uiInitTasks.Access([](Callback& tasks)
		{
			for (const auto& func : tasks)
			{
				func();
			}
		});
	}

	void Events::CL_Disconnect_Hook(const int localClientNum)
	{
		const auto* const uiActive = reinterpret_cast<const clientUIActive_t*>(Utils::Hook::Rebase(clientUIActives));
		const bool wasRunning = uiActive->isRunning;
		const bool wasConnected = uiActive->connectionState >= CA_CONNECTING;
		const int errorEntered = ReadNativeErrorEntered();
		NativeDisconnectError nativeError;
		if (errorEntered == 1)
		{
			nativeError.hasNativeError = true;
			try
			{
				// Preserve the current engine UI text before native cleanup clears
				// it. Never log these strings: they may contain sensitive details.
				nativeError.message = ReadNativeErrorText("com_errorMessage");
				nativeError.title = ReadNativeErrorText("com_errorTitle");
			}
			catch (...) { /* An error snapshot must not prevent native cleanup. */ }
		}
		static thread_local bool isTracing = false;
		if (Flags::HasFlag("log") && !isTracing)
		{
			// Capture the native call site and safe categories before the engine
			// clears connection state. This does not infer user intent or a cause.
			struct TraceScope
			{
				bool& value;
				explicit TraceScope(bool& flag) : value(flag) { value = true; }
				~TraceScope() { value = false; }
			} traceScope(isTracing);
			try
			{
				const auto returnedTo = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
				const auto caller = std::ranges::find_if(CL_DisconnectCalls, [returnedTo](const std::uintptr_t site)
				{
					return returnedTo == Utils::Hook::Rebase(site) + 5;
				});
				const bool isEngineCaller = caller != std::end(CL_DisconnectCalls);
				const auto callerRva = isEngineCaller ? *caller - 0x140000000ULL : 0;
				Logger::Print("native disconnect: caller={} rva=0x{:X} client={} running={} connection={} error_entered={} error={} title={} party_reason={}\n",
					isEngineCaller ? "engine" : "other", callerRva, localClientNum, wasRunning ? 1 : 0, uiActive->connectionState,
					errorEntered, ReadDisconnectReason("com_errorMessage"), ReadDisconnectReason("com_errorTitle"), ReadDisconnectReason("partyend_reason"));
			}
			catch (...) { /* Diagnostics must not replace native disconnect/cleanup. */ }
		}

		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(CL_Disconnect))(localClientNum);

		if (!wasRunning)
		{
			return;
		}

		struct NativeErrorCallbackScope
		{
			const NativeDisconnectError* previous;
			explicit NativeErrorCallbackScope(const NativeDisconnectError& error)
				: previous(currentNativeDisconnectError) { currentNativeDisconnectError = &error; }
			~NativeErrorCallbackScope() { currentNativeDisconnectError = previous; }
		} nativeErrorScope(nativeError);

		disconnectedTasks.Access([wasConnected](CLDisconnectCallback& tasks)
		{
			for (const auto& func : tasks)
			{
				func(wasConnected);
			}
		});
	}

	Events::Events()
	{
		const auto disconnectReplacement = reinterpret_cast<void*>(CL_Disconnect_Hook);

		const HookSite sites[] =
		{
			{ SV_FreeClient_ClientDisconnectCall, ClientDisconnect, reinterpret_cast<void*>(ClientDisconnect_Hook), HOOK_CALL },
			{ SV_FreeClients_ClientDisconnectCall, ClientDisconnect, reinterpret_cast<void*>(ClientDisconnect_Hook), HOOK_CALL },
			{ SV_DirectConnect_UserinfoCall, SV_UserinfoChanged, reinterpret_cast<void*>(SV_UserinfoChanged_Hook), HOOK_CALL },
			{ CL_Disconnect_SteamDisconnectCall, CL_SteamDisconnect, reinterpret_cast<void*>(CL_SteamDisconnect_Hook), HOOK_CALL },
			{ G_LoadGame_ShutdownSystemCall, Scr_ShutdownSystem, reinterpret_cast<void*>(Scr_ShutdownSystem_Hook), HOOK_CALL },
			{ G_ShutdownGame_ShutdownSystemCall, Scr_ShutdownSystem, reinterpret_cast<void*>(Scr_ShutdownSystem_Hook), HOOK_CALL },
			{ Com_Init_ClientInitCall, CL_InitOnceForAllClients, reinterpret_cast<void*>(CL_InitOnceForAllClients_Hook), HOOK_CALL },
			{ CL_CreateCmd_CmdButtonsCall, CL_CmdButtons, reinterpret_cast<void*>(CL_CmdButtons_Hook), HOOK_CALL },
			{ CL_CreateCmd_KeyMoveCall, CL_KeyMove, reinterpret_cast<void*>(CL_KeyMove_Hook), HOOK_CALL },
			{ CL_InitCGame_MillisecondsCall, Sys_Milliseconds, reinterpret_cast<void*>(CL_InitCGame_Hook), HOOK_CALL },
			{ Com_Init_InitDvarsCall, Com_InitDvars, reinterpret_cast<void*>(Com_InitDvars_Hook), HOOK_CALL },
			{ SV_Init_InitGameModeCall, SV_InitGameMode, reinterpret_cast<void*>(SV_InitGameMode_Hook), HOOK_CALL },
			{ NET_Init_ConfigJump, NET_Config, reinterpret_cast<void*>(NET_Config_Hook), HOOK_JUMP },
			{ CL_InitUI_UI_InitCall, UI_Init, reinterpret_cast<void*>(UI_Init_Hook), HOOK_CALL },
			{ CL_DisconnectCalls[0], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[1], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[2], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[3], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[4], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[5], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[6], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[7], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[8], CL_Disconnect, disconnectReplacement, HOOK_CALL },
			{ CL_DisconnectCalls[9], CL_Disconnect, disconnectReplacement, HOOK_CALL },
		};

		static_assert(sizeof(sites) / sizeof(sites[0]) == hookCount);

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.callee, hookSite.isJump))
			{
				Logger::Error("events: 0x{:X} no longer calls 0x{:X}, no event will fire. Events must be registered before Menus\n",
					hookSite.site, hookSite.callee);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < hookCount; ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].replacement, sites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("events: could not seat every hook, no event will fire\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		isInstalled = true;
	}
}
