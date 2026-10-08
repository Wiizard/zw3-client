#include "STDInclude.hpp"

#include "Command.hpp"
#include "Logger.hpp"

namespace Components
{
	std::unordered_map<std::string, Command::Callback> Command::clientCallbacks;
	std::unordered_map<std::string, Command::Callback> Command::serverCallbacks;

	std::string Command::Params::Join(int index) const
	{
		std::string result;

		for (int i = index; i < this->Size(); ++i)
		{
			if (i > index)
			{
				result.append(" ");
			}

			result.append(this->Get(i));
		}

		return result;
	}

	Command::ClientParams::ClientParams() : nesting(Game::cmd_args->nesting)
	{
	}

	int Command::ClientParams::Size() const
	{
		return Game::cmd_args->argc[this->nesting];
	}

	const char* Command::ClientParams::Get(int index) const
	{
		if (index >= this->Size())
		{
			return "";
		}

		return Game::cmd_args->argv[this->nesting][index];
	}

	Command::ServerParams::ServerParams() : nesting(Game::sv_cmd_args->nesting)
	{
	}

	int Command::ServerParams::Size() const
	{
		return Game::sv_cmd_args->argc[this->nesting];
	}

	const char* Command::ServerParams::Get(int index) const
	{
		if (index >= this->Size())
		{
			return "";
		}

		return Game::sv_cmd_args->argv[this->nesting][index];
	}

	Game::cmd_function_s* Command::Allocate()
	{
		return Utils::Memory::GetAllocator()->Allocate<Game::cmd_function_s>();
	}

	void Command::AddRaw(const char* name, void(*callback)())
	{
		Game::Cmd_AddCommandInternal(name, callback, Allocate());
	}

	constexpr std::uintptr_t Cbuf_AddServerText_f = 0x140080E50;

	void Command::AddRawSV(const char* name, void(*callback)())
	{
		Game::Cmd_AddServerCommandInternal(name, callback, Allocate());

		AddRaw(name, reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Cbuf_AddServerText_f)));
	}

	void Command::Add(const char* name, const std::function<void()>& callback)
	{
		Add(name, [callback]([[maybe_unused]] const Params* params)
		{
			callback();
		});
	}

	void Command::Add(const char* name, const Callback& callback)
	{
		const auto lowered = Utils::String::ToLower(name);

		if (!clientCallbacks.contains(lowered))
		{
			AddRaw(name, MainCallback);
		}

		clientCallbacks[lowered] = callback;
	}

	void Command::AddSV(const char* name, const Callback& callback)
	{
		const auto lowered = Utils::String::ToLower(name);

		if (!serverCallbacks.contains(lowered))
		{
			AddRawSV(name, MainCallbackSV);
		}

		serverCallbacks[lowered] = callback;
	}

	void Command::Execute(std::string command, bool sync)
	{
		command.append("\n");

		if (sync)
		{
			Game::Cmd_ExecuteSingleCommand(0, 0, command.data());
			return;
		}

		Game::Cbuf_AddText(0, command.data());
	}

	Game::cmd_function_s* Command::Find(const std::string& command)
	{
		for (auto* entry = *Game::cmd_functions; entry; entry = entry->next)
		{
			if (entry->name && Utils::String::Compare(entry->name, command))
			{
				return entry;
			}
		}

		return nullptr;
	}

	void Command::MainCallback()
	{
		ClientParams params;

		if (params.Size() < 1)
		{
			return;
		}

		const auto name = Utils::String::ToLower(params.Get(0));
		const auto callback = clientCallbacks.find(name);

		if (callback == clientCallbacks.end())
		{
			return;
		}

		callback->second(&params);
	}

	void Command::MainCallbackSV()
	{
		ServerParams params;

		if (params.Size() < 1)
		{
			return;
		}

		const auto name = Utils::String::ToLower(params.Get(0));
		const auto callback = serverCallbacks.find(name);

		if (callback == serverCallbacks.end())
		{
			return;
		}

		callback->second(&params);
	}

	constexpr std::uintptr_t g_bindCommands = 0x140420C90;
	constexpr int stockBindCommandCount = 78;
	constexpr int addedBindCommandLimit = 32;
	constexpr std::uintptr_t Key_GetBindingForCmd_CountCompare = 0x1400EF655;

	static const std::uint8_t countCompare[] = { 0x83, 0xFB, 0x4E };

	static const Utils::Hook::LeaSite bindCommandLeas[] =
	{
		{ 0x1400EF632, { 0x48, 0x8D, 0x3D }, g_bindCommands },
		{ 0x1400EFD0E, { 0x4C, 0x8D, 0x2D }, g_bindCommands },
	};

	constexpr std::uintptr_t Key_ExecBinding = 0x1400F5EE0;
	constexpr std::uintptr_t Key_ExecBindingPressCalls[] = { 0x1400EEAF8, 0x14026509C };

	static const char** bindCommands = nullptr;
	static int bindCommandCount = stockBindCommandCount;
	static Utils::Hook execBindingHooks[std::size(Key_ExecBindingPressCalls)];

	bool Command::TryExtendBindCommands()
	{
		if (!Utils::Hook::MatchesBytes(Key_GetBindingForCmd_CountCompare, countCompare, sizeof(countCompare)))
		{
			return false;
		}

		for (const auto& lea : bindCommandLeas)
		{
			if (!Utils::Hook::IsLeaIntact(lea))
			{
				return false;
			}
		}

		for (const auto call : Key_ExecBindingPressCalls)
		{
			if (!Utils::Hook::BranchesTo(call, Key_ExecBinding, false))
			{
				return false;
			}
		}

		auto* const table = static_cast<const char**>(Utils::Hook::AllocateDataNear(g_bindCommands, (stockBindCommandCount + addedBindCommandLimit) * sizeof(const char*)));

		if (!table)
		{
			return false;
		}

		std::memcpy(table, reinterpret_cast<const void*>(Utils::Hook::Rebase(g_bindCommands)), stockBindCommandCount * sizeof(const char*));

		for (const auto& lea : bindCommandLeas)
		{
			if (!Utils::Hook::CanLeaReach(lea, table))
			{
				return false;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(Key_ExecBindingPressCalls); ++i)
		{
			isSeated = execBindingHooks[i].Initialize(Key_ExecBindingPressCalls[i], reinterpret_cast<void*>(Key_ExecBinding_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : execBindingHooks)
			{
				hook.Uninstall();
			}

			return false;
		}

		for (auto& hook : execBindingHooks)
		{
			hook.Quick();
		}

		for (const auto& lea : bindCommandLeas)
		{
			Utils::Hook::PointLeaAt(lea, table);
		}

		bindCommands = table;
		return true;
	}

	bool Command::AddBindable(const char* name)
	{
		if (!bindCommands || bindCommandCount >= stockBindCommandCount + addedBindCommandLimit)
		{
			return false;
		}

		bindCommands[bindCommandCount] = name;
		++bindCommandCount;

		Utils::Hook::Set<std::uint8_t>(Key_GetBindingForCmd_CountCompare + 2, static_cast<std::uint8_t>(bindCommandCount));
		return true;
	}

	int Command::GetBinding(const char* name)
	{
		for (int binding = stockBindCommandCount; binding < bindCommandCount; ++binding)
		{
			if (!_stricmp(bindCommands[binding], name))
			{
				return binding;
			}
		}

		return -1;
	}

	void Command::ExecBinding(int localClientNum, int binding, int key)
	{
		Key_ExecBinding_Hook(localClientNum, binding, key);
	}

	void Command::Key_ExecBinding_Hook(int localClientNum, int binding, int key)
	{
		if (binding < stockBindCommandCount)
		{
			reinterpret_cast<void(*)(int, int, int)>(Utils::Hook::Rebase(Key_ExecBinding))(localClientNum, binding, key);
			return;
		}

		if (binding >= bindCommandCount)
		{
			return;
		}

		const char* const name = bindCommands[binding];

		if (name[0] == '+' || name[0] == '-')
		{
			return;
		}

		Game::Cbuf_AddText(localClientNum, Utils::String::VA("%s\n", name));
	}

	Command::Command()
	{
		if (!TryExtendBindCommands())
		{
			Logger::Error("command: g_bindCommands does not read as expected, no command of ours can be bound to a key\n");
		}

		Add("openLink", [](const Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			std::string url = params->Get(1);
			Utils::String::Trim(url);

			if (!url.empty() && !url.starts_with("http://") && !url.starts_with("https://"))
			{
				url = "http://" + url;
			}

			Utils::OpenUrl(url);
		});
	}
}
