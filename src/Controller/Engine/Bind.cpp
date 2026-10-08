#include "STDInclude.hpp"

#include "Controller/Engine/Bind.hpp"
#include "Controller/Engine/Engine.hpp"
#include "Controller/Mapping/Binding.hpp"

namespace Controller::Engine
{
	using Mapping::EngineKey;

	static constexpr char customLayout[] = "custom";

	static constexpr int keyStateCount = 256;

	struct CommandSwap
	{
		const char* from;
		const char* to;
	};

	static constexpr CommandSwap controllerCommands[] =
	{
		{ "+activate", "+usereload" },
		{ "+reload", "+usereload" },
	};

	static constexpr CommandSwap retiredCommands[] =
	{
		{ "+melee_breath", "+holdbreath" },
		{ "togglescores", "+scores" },
	};

	static std::string NameForBinding(int binding)
	{
		constexpr auto lastAction = static_cast<int>(Mapping::Action::ActionSlot4);

		for (int action = 1; action <= lastAction; ++action)
		{
			const char* command = Mapping::CommandFor(static_cast<Mapping::Action>(action));

			if (Game::Key_GetBindingForCmd(command) == binding)
			{
				return command;
			}
		}

		return std::format("#{}", binding);
	}

	const char* ControllerCommandFor(const char* command) noexcept
	{
		if (command == nullptr)
		{
			return nullptr;
		}

		for (const auto& swap : controllerCommands)
		{
			if (std::strcmp(command, swap.from) == 0)
			{
				return swap.to;
			}
		}

		for (const auto& swap : retiredCommands)
		{
			if (std::strcmp(command, swap.from) == 0)
			{
				return swap.to;
			}
		}

		return command;
	}

	int BindingForBindCommand(int key, const char* command)
	{
		const int binding = Game::Key_GetBindingForCmd(command);

		if (binding != 0 || !Mapping::IsControllerKey(key))
		{
			return binding;
		}

		for (const auto& swap : retiredCommands)
		{
			if (_stricmp(command, swap.from) == 0)
			{
				return Game::Key_GetBindingForCmd(swap.to);
			}
		}

		return 0;
	}

	int ControllerBindingFor(int binding)
	{
		if (binding == 0)
		{
			return 0;
		}

		for (const auto& swap : controllerCommands)
		{
			const int from = Game::Key_GetBindingForCmd(swap.from);

			if (from == 0 || from != binding)
			{
				continue;
			}

			const int to = Game::Key_GetBindingForCmd(swap.to);

			if (to == 0)
			{
				return binding;
			}

			return to;
		}

		return binding;
	}

	BindBridge::BindBridge(const Context& context, const Dvars& dvars)
		: context(context),
		dvars(dvars)
	{
	}

	void BindBridge::ApplyLayout(std::string_view name)
	{
		Mapping::BindingTable table;
		Mapping::ApplyButtonLayout(table, name);

		std::size_t bound = 0;
		std::size_t unknown = 0;

		table.ForEach([&bound, &unknown](EngineKey key, const std::string& command)
		{
			const int binding = Game::Key_GetBindingForCmd(command.c_str());

			if (binding == 0)
			{
				++unknown;
				return;
			}

			Game::Key_SetBinding(localClient, static_cast<int>(key), binding);
			++bound;
		});

		const std::string message = std::format("applied controller layout '{}': {} keys bound, {} commands unknown to the engine", name, bound, unknown);

		if (unknown == 0)
		{
			this->context.Report(Severity::Info, Facility::Mapping, ErrorCode::None, message);
		}
		else
		{
			this->context.Report(Severity::Warning, Facility::Mapping, ErrorCode::BindingInvalid, message);
		}
	}

	void BindBridge::ApplyConfiguredLayout()
	{
		this->InstallConfiguredLayout(false);
	}

	void BindBridge::ApplyStartupLayout()
	{
		this->InstallConfiguredLayout(true);
	}

	void BindBridge::InstallConfiguredLayout(bool shouldKeepConfigBindings)
	{
		const std::string name = Read(this->dvars.buttonsConfig, "buttons_default");

		this->applied = name;

		this->MigrateControllerCommands();

		if (name == customLayout)
		{
			return;
		}

		if (shouldKeepConfigBindings && this->AreBindingsCustomized())
		{
			this->context.Report(Severity::Info, Facility::Mapping, ErrorCode::None,
				std::format("kept the controller bindings loaded from the config; layout '{}' left unapplied", name));
			return;
		}

		this->ApplyLayout(name);
	}

	bool BindBridge::AreBindingsCustomized() const
	{
		Mapping::BindingTable table;

		const auto& keyState = Game::playerKeys[localClient];

		for (const auto key : Mapping::Keys())
		{
			const int binding = keyState.keys[static_cast<int>(key)].binding;

			if (binding != 0)
			{
				table.Bind(key, NameForBinding(binding));
			}
		}

		if (table.Size() == 0)
		{
			return false;
		}

		return !Mapping::MatchesButtonLayout(table);
	}

	void BindBridge::PollConfiguredLayout()
	{
		const char* name = Read(this->dvars.buttonsConfig, "buttons_default");

		if (this->applied == name)
		{
			return;
		}

		this->ApplyConfiguredLayout();
	}

	void BindBridge::MigrateControllerCommands()
	{
		const auto& keyState = Game::playerKeys[localClient];

		std::size_t migrated = 0;

		for (int key = 0; key != keyStateCount; ++key)
		{
			if (!Mapping::IsControllerKey(key))
			{
				continue;
			}

			const int binding = keyState.keys[key].binding;

			if (binding == 0)
			{
				continue;
			}

			const int wanted = ControllerBindingFor(binding);

			if (wanted == binding)
			{
				continue;
			}

			Game::Key_SetBinding(localClient, key, wanted);
			++migrated;
		}

		if (migrated != 0)
		{
			this->context.Report(Severity::Info, Facility::Mapping, ErrorCode::None, std::format("migrated {} controller key(s) to the merged controller command", migrated));
		}
	}

	void BindBridge::ReapplyLayout()
	{
		this->applied.clear();

		this->ApplyConfiguredLayout();
	}

	void BindBridge::NoteManualRebind()
	{
		if (this->dvars.buttonsConfig == nullptr)
		{
			return;
		}

		Game::Dvar_SetString(this->dvars.buttonsConfig, customLayout);
	}

	std::size_t BindBridge::CommandKeys(int client, bool isControllerInUse, const char* command, int (&keys)[2])
	{
		keys[0] = -1;
		keys[1] = -1;

		if (command == nullptr)
		{
			return 0;
		}

		const char* lookup = command;

		if (isControllerInUse)
		{
			lookup = ControllerCommandFor(command);
		}

		const int binding = Game::Key_GetBindingForCmd(lookup);

		if (binding == 0)
		{
			return 0;
		}

		std::size_t count = 0;
		const auto& keyState = Game::playerKeys[client];

		for (int key = 0; key != keyStateCount; ++key)
		{
			if (Mapping::IsControllerKey(key) != isControllerInUse)
			{
				continue;
			}

			if (keyState.keys[key].binding != binding)
			{
				continue;
			}

			keys[count] = key;
			++count;

			if (count == 2)
			{
				break;
			}
		}

		return count;
	}
}
