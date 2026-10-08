#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Engine/Dvar.hpp"

namespace Controller::Engine
{
	class BindBridge
	{
	public:
		BindBridge(const Context& context, const Dvars& dvars);

		void ApplyLayout(std::string_view name);
		void ApplyConfiguredLayout();
		void ApplyStartupLayout();
		void PollConfiguredLayout();
		void ReapplyLayout();

		void NoteManualRebind();

		static std::size_t CommandKeys(int client, bool isControllerInUse, const char* command, int (&keys)[2]);

	private:
		void InstallConfiguredLayout(bool shouldKeepConfigBindings);
		void MigrateControllerCommands();
		bool AreBindingsCustomized() const;

		const Context& context;
		const Dvars& dvars;

		std::string applied;
	};

	const char* ControllerCommandFor(const char* command) noexcept;

	int ControllerBindingFor(int binding);

	int BindingForBindCommand(int key, const char* command);
}
