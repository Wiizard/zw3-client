#include "STDInclude.hpp"

#include "Chat.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "TextRenderer.hpp"
#include "Voice.hpp"
#include "GSC/Script.hpp"
#include "GSC/ScriptExtension.hpp"

namespace Components
{
	Dvar::Var Chat::cg_chatWidth;
	Dvar::Var Chat::sv_disableChat;
	Dvar::Var Chat::sv_sayName;

	bool Chat::shouldSendChat = true;

	bool Chat::canAddCallback = true;
	std::vector<Game::Scripting::Function> Chat::sayCallbacks;

	Utils::Concurrency::Container<Chat::MuteList> Chat::mutedList;
	const char* Chat::mutedListFile = "userraw/muted-users.json";

	Utils::Hook Chat::hooks[9];

	constexpr std::uintptr_t SV_GameSendServerCommand = 0x1402333E0;

	constexpr char gameMessageCommand = 'e';
	constexpr char chatMessageCommand = 'U';

	constexpr std::uintptr_t ConcatArgs = 0x140168930;

	constexpr std::uintptr_t Cmd_Say_f_ConcatArgsCall = 0x14019959E;

	constexpr std::uintptr_t Cmd_Say_f = 0x140199540;
	constexpr std::uintptr_t G_Say = 0x140199B40;
	constexpr std::uintptr_t G_SayTo = 0x140199D20;

	constexpr std::uintptr_t ClientCommand_SayCalls[] = { 0x140199130, 0x140199161 };

	constexpr std::uintptr_t Cmd_Say_f_SendCall = 0x1401998BF;

	constexpr std::uintptr_t PlayerCmd_SayCalls[] = { 0x1401983B4, 0x140198445 };

	constexpr std::uintptr_t G_Say_SayToCalls[] = { 0x140199C90, 0x140199CE2 };

	constexpr std::uintptr_t CG_AddToTeamChat = 0x1400E5C10;

	static const std::uint8_t addToTeamChatEntry[] = { 0x40, 0x53, 0x48, 0x8B, 0x05, 0x97, 0x76, 0x5D, 0x00 };

	constexpr std::uintptr_t cg_chatHeight = 0x1406BD2B0;
	constexpr std::uintptr_t cg_chatTime = 0x1406BD2A8;

	constexpr std::uintptr_t cg_time = 0x1404E1120;

	struct cgs_t
	{
		unsigned char pad[0x1400];
		char teamChatMsgs[8][160];
		int teamChatMsgTimes[8];
		int teamChatPos;
		int teamLastChatPos;
	};

	AssertOffset(cgs_t, teamChatMsgs, 0x1400);
	AssertOffset(cgs_t, teamChatMsgTimes, 0x1900);
	AssertOffset(cgs_t, teamChatPos, 0x1920);
	AssertOffset(cgs_t, teamLastChatPos, 0x1924);

	constexpr std::uintptr_t cgsArray = 0x140587510;
	constexpr int teamChatLineCount = 8;

	constexpr float fontIconChatWidthMultiplier = 2.0f;

	static const Game::client_s* GetClient(const int clientNum)
	{
		return &Game::svs_clients[clientNum];
	}

	static int GetEntityNum(const Game::gentity_s* ent)
	{
		return *reinterpret_cast<const int*>(ent);
	}

	static void SendServerCommand(const int clientNum, const char* text)
	{
		Game::SV_GameSendServerCommand(clientNum, Game::SV_CMD_CAN_IGNORE, text);
	}

	static const Game::client_s* SV_GetPlayerByNum(const Command::Params* params)
	{
		if (!Dedicated::IsRunning())
		{
			return nullptr;
		}

		if (params->Size() < 2)
		{
			Logger::Print("No player specified.\n");
			return nullptr;
		}

		const char* slotText = params->Get(1);

		for (const char* c = slotText; *c; ++c)
		{
			if (*c < '0' || *c > '9')
			{
				Logger::Print("Bad slot number: {}\n", slotText);
				return nullptr;
			}
		}

		const auto slot = std::strtol(slotText, nullptr, 10);

		if (slot < 0 || slot >= *Game::svs_clientCount)
		{
			Logger::Print("Bad client slot: {}\n", slot);
			return nullptr;
		}

		const auto* client = GetClient(slot);

		if (!client->header.state)
		{
			Logger::Print("Client {} is not active\n", slot);
			return nullptr;
		}

		return client;
	}

	static bool TryGetClientNum(const Command::Params* params, int& clientNum)
	{
		const auto parsed = std::strtoul(params->Get(1), nullptr, 10);

		if (parsed >= static_cast<unsigned long>(*Game::svs_clientCount))
		{
			Logger::Print("Bad client slot: {}\n", parsed);
			return false;
		}

		clientNum = static_cast<int>(parsed);
		return true;
	}

	bool Chat::IsMuted(const Game::gentity_s* ent)
	{
		return IsMuted(GetClient(GetEntityNum(ent)));
	}

	bool Chat::IsMuted(const Game::client_s* cl)
	{
		const auto xuid = cl->steamID;

		return mutedList.Access<bool>([xuid](const MuteList& clients)
		{
			return clients.contains(xuid);
		});
	}

	void Chat::EvaluateSay(const char* text, const Game::gentity_s* player, const int mode)
	{
		shouldSendChat = true;

		canAddCallback = false;

		std::size_t msgIndex = 0;

		while (text[msgIndex] == '\x15' || text[msgIndex] == '\x14')
		{
			++msgIndex;
		}

		if (text[msgIndex] == '/')
		{
			shouldSendChat = false;
			++msgIndex;
		}

		const int clientNum = GetEntityNum(player);

		if (IsMuted(player))
		{
			shouldSendChat = false;
			SendServerCommand(clientNum, Utils::String::Format("{} \"You are muted\"", gameMessageCommand));
		}

		if (sv_disableChat.Get<bool>())
		{
			shouldSendChat = false;
			SendServerCommand(clientNum, Utils::String::Format("{} \"Chat is disabled\"", gameMessageCommand));
		}

		if (text[msgIndex] == '\0')
		{
			shouldSendChat = false;
			canAddCallback = true;
			return;
		}

		const char* name = GetClient(clientNum)->name;
		Logger::Print("{}: {}\n", name, text + msgIndex);

		for (const auto& callback : sayCallbacks)
		{
			if (!ChatCallback(player, callback.GetPos(), text + msgIndex, mode))
			{
				shouldSendChat = false;
			}
		}

		const auto sayString = Game::SL_GetString("say", 0);

		Game::Scr_AddEntity(player);
		Game::Scr_AddString(text + msgIndex);
		Game::Scr_NotifyLevel(sayString, 2);

		Game::SL_RemoveRefToString(sayString);

		canAddCallback = true;
	}

	int Chat::GetCallbackReturn()
	{
		if (*Game::scrVmPub_inparamcount == 0)
		{
			return 1;
		}

		Game::Scr_ClearOutParams();
		*Game::scrVmPub_outparamcount = *Game::scrVmPub_inparamcount;
		*Game::scrVmPub_inparamcount = 0;

		const auto index = 1 - static_cast<std::ptrdiff_t>(*Game::scrVmPub_outparamcount);
		const auto* result = &(*Game::scrVmPub_top)[index];

		if (result->type != Game::VAR_INTEGER)
		{
			return 1;
		}

		return result->u.intValue;
	}

	int Chat::ChatCallback(const Game::gentity_s* self, const char* codePos, const char* message, const int mode)
	{
		constexpr unsigned int paramCount = 2;

		Game::Scripting::StackIsolation isolation;

		Game::Scr_AddInt(mode);
		Game::Scr_AddString(message);

		const auto objectId = Game::Scr_GetEntityId(GetEntityNum(self), 0);
		Game::AddRefToObject(objectId);
		const auto threadId = Game::VM_Execute(Game::AllocThread(objectId), codePos, paramCount);

		const auto result = GetCallbackReturn();

		auto*& top = *Game::scrVmPub_top;
		Game::RemoveRefToValue(top->type, top->u);

		top->type = Game::VAR_UNDEFINED;
		--top;
		--*Game::scrVmPub_inparamcount;

		Game::Scr_FreeThread(static_cast<std::uint16_t>(threadId));

		return result;
	}

	void Chat::AddScriptFunctions()
	{
		GSC::Script::AddFunction("OnPlayerSay", []
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				GSC::Script::Scr_Error("OnPlayerSay: Needs one function pointer!");
				return;
			}

			if (!canAddCallback)
			{
				GSC::Script::Scr_Error("OnPlayerSay: Cannot add a callback in this context");
				return;
			}

			sayCallbacks.emplace_back(GSC::ScriptExtension::GetCodePosForParam(0));
		});
	}

	void Chat::Cmd_Say_f_Hook(Game::gentity_s* ent, const int mode, const int arg0)
	{
		const Command::ServerParams params;

		if (params.Size() >= 2 || arg0)
		{
			int start = 1;

			if (arg0)
			{
				start = 0;
			}

			EvaluateSay(ConcatArgs_Hook(start), ent, mode);
		}

		reinterpret_cast<void(*)(Game::gentity_s*, int, int)>(Utils::Hook::Rebase(Cmd_Say_f))(ent, mode, arg0);
	}

	const char* Chat::ConcatArgs_Hook(const int start)
	{
		auto* const text = reinterpret_cast<char*(*)(int)>(Utils::Hook::Rebase(ConcatArgs))(start);
		TextRenderer::StripMaterialTextIcons(text, text, std::strlen(text) + 1);

		return text;
	}

	void Chat::SV_GameSendServerCommand_Hook(const int clientNum, const int type, const char* text)
	{
		if (!shouldSendChat)
		{
			return;
		}

		Game::SV_GameSendServerCommand(clientNum, static_cast<Game::svscmd_type>(type), text);
	}

	void Chat::G_Say_Hook(Game::gentity_s* ent, Game::gentity_s* target, const int mode, const char* chatText)
	{
		const std::string text = TextRenderer::StripMaterialTextIcons(std::string(chatText));

		EvaluateSay(text.data(), ent, mode);

		reinterpret_cast<void(*)(Game::gentity_s*, Game::gentity_s*, int, const char*)>(Utils::Hook::Rebase(G_Say))(ent, target, mode, text.data());
	}

	void Chat::G_SayTo_Hook(Game::gentity_s* ent, Game::gentity_s* other, const int mode, const int color,
		const char* teamString, const char* name, const char* message)
	{
		if (!shouldSendChat)
		{
			return;
		}

		reinterpret_cast<void(*)(Game::gentity_s*, Game::gentity_s*, int, int, const char*, const char*, const char*)>(
			Utils::Hook::Rebase(G_SayTo))(ent, other, mode, color, teamString, name, message);
	}

	bool Chat::CL_IsMessageFromMutedUser(const std::string& text)
	{
		const std::string colorlessText = TextRenderer::StripColors(text);
		const std::string rawText = TextRenderer::StripAllTextIcons(colorlessText);

		const auto index = rawText.find(':');

		if (index == std::string::npos)
		{
			return false;
		}

		const std::string authorName = rawText.substr(0, index);

		char nameBuffer[64]{};

		for (int i = 0; i < static_cast<int>(Game::MAX_CLIENTS); ++i)
		{
			if (!Voice::CL_IsPlayerMuted(i))
			{
				continue;
			}

			if (!Game::CL_GetClientName(0, i, nameBuffer, sizeof(nameBuffer)))
			{
				continue;
			}

			if (authorName == TextRenderer::StripColors(std::string(nameBuffer)))
			{
				return true;
			}
		}

		return false;
	}

	void Chat::CG_AddToTeamChat_Hook([[maybe_unused]] const int localClientNum, const char* text)
	{
		if (CL_IsMessageFromMutedUser(text))
		{
			return;
		}

		auto* const cgs = reinterpret_cast<cgs_t*>(Utils::Hook::Rebase(cgsArray));
		const int chatHeight = Utils::Hook::Get<Game::dvar_t*>(cg_chatHeight)->current.integer;
		const int chatTime = Utils::Hook::Get<Game::dvar_t*>(cg_chatTime)->current.integer;
		const int chatWidth = cg_chatWidth.Get<int>();

		if (chatHeight <= 0 || chatHeight > teamChatLineCount || chatWidth <= 0 || chatTime <= 0)
		{
			cgs->teamLastChatPos = 0;
			cgs->teamChatPos = 0;
			return;
		}

		constexpr char lastColorChar = '0' + TEXT_COLOR_COUNT - 1;

		char lastColor = '0' + TEXT_COLOR_DEFAULT;
		char* lastSpace = nullptr;
		char* lastFontIcon = nullptr;
		char* line = cgs->teamChatMsgs[cgs->teamChatPos % chatHeight];
		float length = 0.0f;

		line[0] = '\0';

		const auto checkLineEnd = [&]
		{
			if (length <= static_cast<float>(chatWidth))
			{
				return;
			}

			if (lastSpace && (!lastFontIcon || lastSpace > lastFontIcon))
			{
				text += lastSpace - line + 1;
				line = lastSpace;
			}
			else if (lastFontIcon)
			{
				text += lastFontIcon - line;
				line = lastFontIcon;
			}

			line[0] = '\0';
			length = 0.0f;

			cgs->teamChatMsgTimes[cgs->teamChatPos % chatHeight] = Utils::Hook::Get<int>(cg_time);
			cgs->teamChatPos++;

			line = cgs->teamChatMsgs[cgs->teamChatPos % chatHeight];
			line[0] = '^';
			line[1] = lastColor;
			line += 2;
			lastSpace = nullptr;
			lastFontIcon = nullptr;
		};

		while (*text)
		{
			checkLineEnd();

			const char* fontIconEnd = nullptr;
			float fontIconWidth = 0.0f;

			if (TextRenderer::TryGetFontIconWidth(text, fontIconEnd, fontIconWidth))
			{
				length += fontIconWidth * fontIconChatWidthMultiplier;
				lastFontIcon = line;

				while (text < fontIconEnd)
				{
					line[0] = text[0];
					line += 1;
					text += 1;
				}

				checkLineEnd();
				continue;
			}

			if (text[0] == '^' && text[1] >= '0' && text[1] <= lastColorChar)
			{
				line[0] = '^';
				line[1] = text[1];
				lastColor = text[1];
				line += 2;
				text += 2;
				continue;
			}

			if (text[0] == ' ')
			{
				lastSpace = line;
			}

			line[0] = text[0];
			line += 1;
			text += 1;
			length += 1.0f;
		}

		line[0] = '\0';

		cgs->teamChatMsgTimes[cgs->teamChatPos % chatHeight] = Utils::Hook::Get<int>(cg_time);
		cgs->teamChatPos++;

		if (cgs->teamChatPos - cgs->teamLastChatPos > chatHeight)
		{
			cgs->teamLastChatPos = cgs->teamChatPos - chatHeight;
		}
	}

	void Chat::MuteClient(const Game::client_s* client)
	{
		const auto* const cl = client;
		const auto xuid = cl->steamID;

		mutedList.Access([xuid](MuteList& clients)
		{
			clients.insert(xuid);
			SaveMutedList(clients);
		});

		const char* name = cl->name;
		Logger::Print("{} was muted\n", name);
		SendServerCommand(static_cast<int>(cl - GetClient(0)), Utils::String::Format("{} \"You were muted\"", gameMessageCommand));
	}

	void Chat::UnmuteClient(const Game::client_s* client)
	{
		const auto* const cl = client;

		UnmuteInternal(cl->steamID);

		const char* name = cl->name;
		Logger::Print("{} was unmuted\n", name);
		SendServerCommand(static_cast<int>(cl - GetClient(0)), Utils::String::Format("{} \"You were unmuted\"", gameMessageCommand));
	}

	void Chat::UnmuteInternal(const std::uint64_t id, const bool everyone)
	{
		mutedList.Access([id, everyone](MuteList& clients)
		{
			if (everyone)
			{
				clients.clear();
			}
			else
			{
				clients.erase(id);
			}

			SaveMutedList(clients);
		});
	}

	std::unique_lock<Utils::NamedMutex> Chat::Lock()
	{
		static Utils::NamedMutex mutex{ "iw4x-mute-list-lock" };
		std::unique_lock lock{ mutex };
		return lock;
	}

	void Chat::SaveMutedList(const MuteList& list)
	{
		const auto _ = Lock();

		const nlohmann::json mutedUsers = nlohmann::json
		{
			{ "SteamID", list },
		};

		Utils::IO::WriteFile(mutedListFile, mutedUsers.dump());
	}

	void Chat::LoadMutedList()
	{
		const auto _ = Lock();

		const auto mutedUsers = Utils::IO::ReadFile(mutedListFile);

		if (mutedUsers.empty())
		{
			Logger::Debug("muted-users.json does not exist");
			return;
		}

		nlohmann::json mutedUsersData;

		try
		{
			mutedUsersData = nlohmann::json::parse(mutedUsers);
		}
		catch (const std::exception& ex)
		{
			Logger::Error("JSON Parse Error: {}\n", ex.what());
			return;
		}

		if (!mutedUsersData.contains("SteamID"))
		{
			Logger::Error("muted-users.json contains invalid data\n");
			return;
		}

		const auto& list = mutedUsersData["SteamID"];

		if (!list.is_array())
		{
			return;
		}

		mutedList.Access([&list](MuteList& clients)
		{
			const nlohmann::json::array_t entries = list;

			for (const auto& entry : entries)
			{
				if (entry.is_number_unsigned())
				{
					clients.insert(entry.get<std::uint64_t>());
				}
			}
		});
	}

	void Chat::AddServerCommands()
	{
		Command::AddSV("muteClient", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 2)
			{
				Logger::Print("Usage: {} <client number> : prevent the player from using the chat\n", params->Get(0));
				return;
			}

			const auto* client = SV_GetPlayerByNum(params);

			if (client && !client->bIsTestClient)
			{
				Voice::SV_MuteClient(static_cast<int>(client - GetClient(0)));
				MuteClient(client);
			}
		});

		Command::AddSV("unmute", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 2)
			{
				Logger::Print("Usage: {} <client number or guid>\n{} all = unmute everyone\n", params->Get(0), params->Get(0));
				return;
			}

			const auto* client = SV_GetPlayerByNum(params);

			if (client && client->bIsTestClient)
			{
				return;
			}

			if (client)
			{
				UnmuteClient(client);
				Voice::SV_UnmuteClient(static_cast<int>(client - GetClient(0)));
				return;
			}

			if (std::strcmp(params->Get(1), "all") == 0)
			{
				Logger::Print("All players were unmuted\n");
				UnmuteInternal(0, true);
				Voice::SV_ClearMutedList();
				return;
			}

			UnmuteInternal(std::strtoull(params->Get(1), nullptr, 16));
		});

		Command::AddSV("say", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 2)
			{
				return;
			}

			const auto message = params->Join(1);
			const std::string name = sv_sayName.Get<const char*>();

			if (!name.empty())
			{
				SendServerCommand(-1, Utils::String::Format("{} \"{}: {}\"", chatMessageCommand, name, message));
				Logger::Print("{}: {}\n", name, message);
				return;
			}

			SendServerCommand(-1, Utils::String::Format("{} \"Console: {}\"", chatMessageCommand, message));
			Logger::Print("Console: {}\n", message);
		});

		Command::AddSV("tell", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 3)
			{
				return;
			}

			int clientNum = 0;

			if (!TryGetClientNum(params, clientNum))
			{
				return;
			}

			const auto message = params->Join(2);
			const std::string name = sv_sayName.Get<const char*>();

			if (!name.empty())
			{
				SendServerCommand(clientNum, Utils::String::Format("{} \"{}: {}\"", chatMessageCommand, name, message));
				Logger::Print("{} -> {}: {}\n", name, clientNum, message);
				return;
			}

			SendServerCommand(clientNum, Utils::String::Format("{} \"Console: {}\"", chatMessageCommand, message));
			Logger::Print("Console -> {}: {}\n", clientNum, message);
		});

		Command::AddSV("sayraw", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 2)
			{
				return;
			}

			const auto message = params->Join(1);
			SendServerCommand(-1, Utils::String::Format("{} \"{}\"", chatMessageCommand, message));
			Logger::Print("Raw: {}\n", message);
		});

		Command::AddSV("tellraw", [](const Command::Params* params)
		{
			if (!Dedicated::IsRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (params->Size() < 3)
			{
				return;
			}

			int clientNum = 0;

			if (!TryGetClientNum(params, clientNum))
			{
				return;
			}

			const auto message = params->Join(2);
			SendServerCommand(clientNum, Utils::String::Format("{} \"{}\"", chatMessageCommand, message));
			Logger::Print("Raw -> {}: {}\n", clientNum, message);
		});

		sv_sayName = Dvar::Register("sv_sayName", "^7Console", Game::DVAR_NONE, "The alias of the server when broadcasting a chat message");
	}

	void Chat::RegisterDvars()
	{
		cg_chatWidth = Dvar::Register("cg_chatWidth", 52, 1, std::numeric_limits<int>::max(), Game::DVAR_ARCHIVE, "The normalized maximum width of a chat message");
		sv_disableChat = Dvar::Register("sv_disableChat", false, Game::DVAR_NONE, "Disable chat messages from clients");
	}

	Chat::Chat()
	{
		if (!Events::IsInstalled())
		{
			Logger::Error("chat: events are not installed, chat is left as the engine has it\n");
			return;
		}

		LoadMutedList();

		struct CallSite
		{
			std::uintptr_t site;
			std::uintptr_t callee;
			void* replacement;
		};

		const CallSite callSites[] =
		{
			{ ClientCommand_SayCalls[0], Cmd_Say_f, reinterpret_cast<void*>(Cmd_Say_f_Hook) },
			{ ClientCommand_SayCalls[1], Cmd_Say_f, reinterpret_cast<void*>(Cmd_Say_f_Hook) },
			{ Cmd_Say_f_SendCall, SV_GameSendServerCommand, reinterpret_cast<void*>(SV_GameSendServerCommand_Hook) },
			{ Cmd_Say_f_ConcatArgsCall, ConcatArgs, reinterpret_cast<void*>(ConcatArgs_Hook) },
			{ PlayerCmd_SayCalls[0], G_Say, reinterpret_cast<void*>(G_Say_Hook) },
			{ PlayerCmd_SayCalls[1], G_Say, reinterpret_cast<void*>(G_Say_Hook) },
			{ G_Say_SayToCalls[0], G_SayTo, reinterpret_cast<void*>(G_SayTo_Hook) },
			{ G_Say_SayToCalls[1], G_SayTo, reinterpret_cast<void*>(G_SayTo_Hook) },
		};

		static_assert(sizeof(callSites) / sizeof(callSites[0]) + 1 == sizeof(hooks) / sizeof(hooks[0]));

		for (const auto& callSite : callSites)
		{
			if (!Utils::Hook::BranchesTo(callSite.site, callSite.callee, HOOK_CALL))
			{
				Logger::Error("chat: 0x{:X} is not a call to 0x{:X}, chat is left as the engine has it\n", callSite.site, callSite.callee);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(CG_AddToTeamChat, addToTeamChatEntry, sizeof(addToTeamChatEntry)))
		{
			Logger::Error("chat: CG_AddToTeamChat does not read as expected, chat is left as the engine has it\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(callSites); ++i)
		{
			isSeated = hooks[i].Initialize(callSites[i].site, callSites[i].replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[std::size(callSites)].Initialize(CG_AddToTeamChat, reinterpret_cast<void*>(CG_AddToTeamChat_Hook), HOOK_JUMP)
			->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("chat: could not seat every hook, chat is left as the engine has it\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		Events::OnDvarInit(RegisterDvars);
		Events::OnSVInit(AddServerCommands);

		Command::Add("mp_QuickMessage", []
		{
			Command::Execute("openmenu quickmessage");
		});

		AddScriptFunctions();

		Events::OnVMShutdown([]
		{
			sayCallbacks.clear();
		});
	}
}
