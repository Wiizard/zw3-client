#include "STDInclude.hpp"

#include "Dedicated.hpp"
#include "CardTitles.hpp"
#include "ClientSlots.hpp"
#include "ClanTags.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "Flags.hpp"
#include "Friends.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "ServerCommands.hpp"
#include "ZoneBuilder.hpp"

#include "Steam/Proxy.hpp"

namespace Components
{
	::Steam::SteamID Dedicated::playerGuids[Game::MAX_CLIENTS][2];

	Dvar::Var Dedicated::sv_lanOnly;
	Dvar::Var Dedicated::sv_motd;
	Dvar::Var Dedicated::com_logFilter;
	Dvar::Var Dedicated::zwnet_show_in_server_browser;
	Dvar::Var Dedicated::zwnet_match_ended;
	Dvar::Var Dedicated::zwnet_match_id;
	Dvar::Var Dedicated::zwnet_selected_map;

	const Game::dvar_t* Dedicated::com_dedicated;

	constexpr char guidCommand = 20;

	extern "C"
	{
		void Com_ClampMsec_Stub();

		std::uintptr_t Com_Frame_SvRunningSlot = 0;

		void Dedicated_ComClampMsec(int msec);
	}

	struct EnginePatch
	{
		std::uintptr_t site;
		std::vector<std::uint8_t> expected;
		std::vector<std::uint8_t> replacement;
		std::vector<std::uint8_t> alreadyPatched = {};
	};

	static const EnginePatch enginePatches[] =
	{
		{ 0x1400FBD60, { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D }, { 0xC3 } },
		{ 0x140032950, { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D }, { 0xC3 } },
		{ 0x1402C1A30, { 0x48, 0x89, 0x5C, 0x24, 0x18, 0x57 }, { 0xC3 } },
		{ 0x140246070, { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x57 }, { 0xC3 } },
		{ 0x140101C30, { 0x48, 0x83, 0xEC, 0x38, 0x80, 0x3D }, { 0xC3 } },
		{ 0x1400F81B0, { 0x40, 0x55, 0x57, 0x48, 0x8D, 0xAC }, { 0xC3 } },

		{ 0x140033AF0, { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83 }, { 0xC3 } },

		{ 0x1402AA620, { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55 }, { 0xC3 } },

		{ 0x14010E310, { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48 }, { 0xC3 } },

		{ 0x14010EE60, { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20 }, { 0xC3 } },
		{ 0x140112417, { 0xE8, 0x44, 0x69, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },
		{ 0x140112425, { 0xE8, 0x76, 0x9B, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },
		{ 0x140112648, { 0xE8, 0x13, 0x67, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },
		{ 0x140112656, { 0xE8, 0x45, 0x99, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },

		{ 0x1400FADBD, { 0x83, 0xF8, 0x03 }, { 0x83, 0xF8, 0x04 } },

		{ 0x14002E82A, { 0xB2, 0x01 }, { 0xB2, 0x00 } },

		{ 0x1400FAF87, { 0x41, 0xB8, 0x84, 0x00, 0x00, 0x00 }, { 0x41, 0xB8, 0x80, 0x00, 0x00, 0x00 } },

		{ 0x14019865B, { 0x40, 0x0F, 0x95, 0xC7 }, { 0x90, 0x90, 0x90, 0x90 } },

		{ 0x14023BF51, { 0x0F, 0x85, 0x9A, 0x00, 0x00, 0x00 }, { 0xE9, 0x9B, 0x00, 0x00, 0x00, 0x90 } },

		{ 0x14024D6F3, { 0x0F, 0x85, 0x44, 0x01, 0x00, 0x00 }, { 0xE9, 0x45, 0x01, 0x00, 0x00, 0x90 } },

		{ 0x1401F42E1, { 0x0F, 0x84, 0x86, 0x00, 0x00, 0x00 }, { 0xE9, 0x87, 0x00, 0x00, 0x00, 0x90 } },

		{ 0x1401F5755, { 0xE8, 0xF6, 0xFD, 0x0A, 0x00 }, { 0x90, 0x90, 0x90, 0x90, 0x90 }, { 0x31, 0xC0, 0x90, 0x90, 0x90 } },
		{ 0x1401F576C, { 0x74, 0x09 }, { 0xEB, 0x09 } },

		{ 0x1402A5E65, { 0xE8, 0x66, 0x28, 0x00, 0x00 }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },
		{ 0x1402A5E0E, { 0xE8, 0x6D, 0xC7, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },

		{ 0x1402381CB, { 0x0F, 0x84, 0x11, 0x01, 0x00, 0x00 }, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 } },

		{ 0x1400FD67E, { 0x74, 0x30 }, { 0x90, 0x90 } },
		{ 0x1400FD6A2, { 0xE8, 0x69, 0x55, 0xF3, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },

		{ 0x1401F556F, { 0xE8, 0x4C, 0x18, 0xFF, 0xFF }, { 0x90, 0x90, 0x90, 0x90, 0x90 } },

		{ 0x140278185, { 0x75, 0x1D }, { 0xEB, 0x1D } },
	};

	constexpr std::uintptr_t Com_Init_CL_InitRendererCall = 0x1401F59E3;
	constexpr std::uintptr_t CL_InitRenderer = 0x1400FBD60;
	constexpr std::uintptr_t R_LoadGraphicsAssets = 0x140030C30;
	constexpr std::uintptr_t graphicsZoneNames = 0x148CCC960;

	constexpr std::uintptr_t Com_StartHunkUsers_Com_EventLoopCall = 0x1401F6B26;
	constexpr std::uintptr_t Com_EventLoop = 0x1401F3F90;
	constexpr std::uintptr_t Com_AddStartupCommands = 0x1401F35B0;

	constexpr std::uintptr_t TimeWrap_Com_ErrorCall = 0x14023C1E4;
	constexpr std::uintptr_t Com_Error = 0x1401F38F0;

	constexpr std::uintptr_t Com_Frame_ClampMsecSite = 0x1401F4432;
	constexpr std::uintptr_t com_sv_running = 0x141BD9A90;

	static const std::uint8_t clampMsecSite[] = { 0x48, 0x8B, 0x05, 0x57, 0x56, 0x9E, 0x01 };

	static Utils::Hook initHook;
	static Utils::Hook postInitHook;
	static Utils::Hook timeWrapHook;
	static Utils::Hook clampMsecHook;

	void Dedicated_ComClampMsec(int msec)
	{
		if (msec > 500 && msec < 500000)
		{
			Logger::Warning("Hitch warning: {} msec frame time\n", msec);
		}
	}

	void Dedicated::InitDedicatedServer()
	{
		static const char* fastfiles[7] =
		{
			"code_post_gfx_mp",
			"localized_code_post_gfx_mp",
			"ui_mp",
			"localized_ui_mp",
			"common_mp",
			"localized_common_mp",
			"patch_mp"
		};

		if (FastFiles::HasZW3CommonZone())
		{
			fastfiles[4] = "zw3_common";
		}

		std::memcpy(reinterpret_cast<void*>(Utils::Hook::Rebase(graphicsZoneNames)), fastfiles, sizeof(fastfiles));
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(R_LoadGraphicsAssets))();
	}

	void Dedicated::PostInitialization()
	{
		if (ZoneBuilder::IsEnabled())
		{
			reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_AddStartupCommands))();
			return;
		}

		Command::Execute("exec autoexec.cfg");
		Command::Execute("onlinegame 1");
		Command::Execute("exec default_xboxlive.cfg");
		Command::Execute("xblive_rankedmatch 1");
		Command::Execute("xblive_privatematch 1");
		Command::Execute("xblive_privateserver 0");
		Command::Execute("xstartprivatematch");
		Command::Execute("cl_maxpackets 125");
		Command::Execute("snaps 30");
		Command::Execute("com_maxfps 125");

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_AddStartupCommands))();
	}

	void Dedicated::Com_EventLoop_Hk()
	{
		PostInitialization();
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_EventLoop))();
	}

	void Dedicated::TimeWrapStub(int code, const char* message)
	{
		Scheduler::Once([]
		{
			std::string mapname = Dvar::Var("mapname").Get<std::string>();

			if (!Party::IsEnabled())
			{
				if (mapname.empty())
				{
					mapname = "mp_rust";
				}

				Command::Execute(std::format("map {}", mapname), true);
			}
		}, Scheduler::Pipeline::SERVER);

		reinterpret_cast<void(*)(int, const char*, ...)>(Utils::Hook::Rebase(Com_Error))(code, message);
	}

	bool Dedicated::ApplyEnginePatches()
	{
		for (const auto& patch : enginePatches)
		{
			const bool isExpected = Utils::Hook::MatchesBytes(patch.site, patch.expected.data(), patch.expected.size());
			const bool isAlreadyPatched = !patch.alreadyPatched.empty() && Utils::Hook::MatchesBytes(patch.site, patch.alreadyPatched.data(), patch.alreadyPatched.size());

			if (!isExpected && !isAlreadyPatched)
			{
				Logger::Error("dedicated: 0x{:X} does not read as expected, the server is not started headless\n", patch.site);
				return false;
			}
		}

		const bool areCallsIntact = Utils::Hook::BranchesTo(Com_Init_CL_InitRendererCall, CL_InitRenderer, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Com_StartHunkUsers_Com_EventLoopCall, Com_EventLoop, HOOK_CALL)
			&& Utils::Hook::BranchesTo(TimeWrap_Com_ErrorCall, Com_Error, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(Com_Frame_ClampMsecSite, clampMsecSite, sizeof(clampMsecSite));

		if (!areCallsIntact)
		{
			Logger::Error("dedicated: the engine's init and frame calls do not read as expected, the server is not started headless\n");
			return false;
		}

		Com_Frame_SvRunningSlot = Utils::Hook::Rebase(com_sv_running);

		bool isSeated = initHook.Initialize(Com_Init_CL_InitRendererCall, reinterpret_cast<void*>(InitDedicatedServer), HOOK_CALL)->Install()->IsInstalled();
		isSeated = postInitHook.Initialize(Com_StartHunkUsers_Com_EventLoopCall, reinterpret_cast<void*>(Com_EventLoop_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = timeWrapHook.Initialize(TimeWrap_Com_ErrorCall, reinterpret_cast<void*>(TimeWrapStub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = clampMsecHook.Initialize(Com_Frame_ClampMsecSite, reinterpret_cast<void*>(Com_ClampMsec_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			initHook.Uninstall();
			postInitHook.Uninstall();
			timeWrapHook.Uninstall();
			clampMsecHook.Uninstall();
			Logger::Error("dedicated: could not seat the engine hooks, the server is not started headless\n");
			return false;
		}

		initHook.Quick();
		postInitHook.Quick();
		timeWrapHook.Quick();
		clampMsecHook.Quick();
		Utils::Hook::Nop(Com_Frame_ClampMsecSite + 5, 2);

		for (const auto& patch : enginePatches)
		{
			for (std::size_t i = 0; i < patch.replacement.size(); ++i)
			{
				Utils::Hook::Set<std::uint8_t>(patch.site + i, patch.replacement[i]);
			}
		}

		return true;
	}

	bool Dedicated::IsEnabled()
	{
		static std::optional<bool> flag;

		if (!flag.has_value())
		{
			flag.emplace(Flags::HasFlag("dedicated") || ZoneBuilder::IsEnabled());
		}

		return flag.value();
	}

	bool Dedicated::IsRunning()
	{
		assert(*Game::com_sv_running);
		return *Game::com_sv_running && (*Game::com_sv_running)->current.enabled;
	}

	void Dedicated::TransmitGuids()
	{
		constexpr std::size_t guidsPerCommand = 16;

		const auto count = ClientSlots::SentClientCount();
		const bool isWide = ClientSlots::IsServerWide();
		const auto step = isWide ? guidsPerCommand : count;

		for (std::size_t first = 0; first < count; first += step)
		{
			std::string list = Utils::String::VA("%c", guidCommand);

			if (isWide)
			{
				list.append(Utils::String::VA(" @%zu", first));
			}

			for (std::size_t i = first; i < std::min(first + step, count); ++i)
			{
				if (Game::svs_clients[i].header.state >= Game::CS_CONNECTED)
				{
					list.append(Utils::String::VA(" %llX", Game::svs_clients[i].steamID));

					const Utils::InfoString info(Game::svs_clients[i].userinfo);
					list.append(Utils::String::VA(" %llX", std::strtoull(info.Get("realsteamId").data(), nullptr, 16)));
				}
				else
				{
					list.append(" 0 0");
				}
			}

			Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, list.data());
		}
	}

	void Dedicated::Heartbeat()
	{
		if (sv_lanOnly.Get<bool>() || zwnet_show_in_server_browser.Get<std::string>() == "0")
		{
			return;
		}

		const auto masterPort = (*Game::com_masterPort)->current.unsignedInt;
		const auto* masterServerName = (*Game::com_masterServerName)->current.string;

		const Network::Address master(Utils::String::VA("%s:%u", masterServerName, masterPort));

		Logger::Print("Sending heartbeat to master: {}:{}\n", masterServerName, masterPort);
		Network::SendCommand(master, "heartbeat", "ZW3");
	}

	Dedicated::Dedicated()
	{
		Events::OnDvarInit([]
		{
			com_logFilter = Dvar::Register("com_logFilter", true, Game::DVAR_LATCH, "Removes ~95% of unneeded lines from the log");
		});

		Events::OnSVInit(Game::AddOperatorCommands);

		if (IsEnabled())
		{
			if (!ApplyEnginePatches())
			{
				MessageBoxA(nullptr, "The dedicated server's engine patches could not be applied, see iw4x\\iw4x.log.", "Zombie Warfare 3", MB_ICONERROR);
				TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
			}

			Scheduler::OnGameInitialized([]
			{
				zwnet_show_in_server_browser = Dvar::Register("zwnet_show_in_server_browser", "1", Game::DVAR_SERVERINFO, "Advertise this server in the normal server browser");
				zwnet_match_ended = Dvar::Register("zwnet_match_ended", "0", Game::DVAR_SERVERINFO, "The assigned ZWNET match reached its authoritative end state");
				zwnet_match_id = Dvar::Register("zwnet_match_id", "", Game::DVAR_SERVERINFO, "The backend-assigned ZWNET match identifier");
				zwnet_selected_map = Dvar::Register("zwnet_selected_map", "", Game::DVAR_SERVERINFO, "The backend-assigned ZWNET map identifier");
			}, Scheduler::Pipeline::MAIN);

			Events::OnDvarInit([]
			{
				sv_motd = Dvar::Register("sv_motd", "", Game::DVAR_NONE, "A custom message of the day for servers");
				sv_lanOnly = Dvar::Register("sv_lanOnly", false, Game::DVAR_NONE, "Don't act as node");

				static const char* dedicatedEnumNames[] =
				{
					"listen server",
					"dedicated LAN server",
					"dedicated internet server",
					nullptr,
				};

				com_dedicated = Game::Dvar_RegisterEnum("dedicated", dedicatedEnumNames, 2, Game::DVAR_ROM, "True if this is a dedicated server");

				if (com_dedicated->current.integer != 1 && com_dedicated->current.integer != 2)
				{
					Game::DvarValue value{};
					value.integer = 0;
					Game::Dvar_SetVariant(const_cast<Game::dvar_t*>(com_dedicated), value, 0);
				}
			});

			Scheduler::Loop([]
			{
				CardTitles::SendCustomTitlesToClients();
				ClanTags::SendClanTagsToClients();
			}, Scheduler::Pipeline::SERVER, 10s);

			if (!ZoneBuilder::IsEnabled())
			{
				Scheduler::Once(Heartbeat, Scheduler::Pipeline::SERVER);
				Scheduler::Loop(Heartbeat, Scheduler::Pipeline::SERVER, 2min);
			}
		}
		else
		{
			ZeroMemory(playerGuids, sizeof(playerGuids));

			ServerCommands::OnCommand(guidCommand, [](const Command::Params* params)
			{
				int first = 0;
				int last = static_cast<int>(Game::MAX_CLIENTS);
				int token = 1;

				if (params->Get(1)[0] == '@')
				{
					first = std::clamp(std::atoi(params->Get(1) + 1), 0, static_cast<int>(Game::MAX_CLIENTS));
					last = std::clamp(first + (params->Size() - 2) / 2, first, static_cast<int>(Game::MAX_CLIENTS));
					token = 2;
				}

				for (int client = first; client < last; client++, token += 2)
				{
					playerGuids[client][0].bits = std::strtoull(params->Get(token), nullptr, 16);
					playerGuids[client][1].bits = std::strtoull(params->Get(token + 1), nullptr, 16);

					if (::Steam::Proxy::SteamFriends && playerGuids[client][1].bits != 0 && !Friends::IsInvisible() && !Friends::cl_anonymous.Get<bool>())
					{
						::Steam::Proxy::SteamFriends->SetPlayedWith(playerGuids[client][1]);
					}
				}

				return true;
			});
		}

		Scheduler::Loop([]
		{
			if (IsRunning())
			{
				TransmitGuids();
			}
		}, Scheduler::Pipeline::SERVER, 15s);
	}
}
