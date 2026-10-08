#include "STDInclude.hpp"

#include "ServerCommands.hpp"
#include "Logger.hpp"

namespace Components
{
	std::unordered_map<std::int32_t, std::vector<ServerCommands::Handler>> ServerCommands::commands;
	Utils::Hook ServerCommands::serverCommandHook;

	constexpr std::uintptr_t CG_ExecuteNewServerCommands_ServerCommandCall = 0x1400E7727;

	static const std::uint8_t serverCommandCall[] = { 0xE8, 0xB4, 0xED, 0xFF, 0xFF };

	constexpr std::uintptr_t CG_ServerCommand_LetterTest = 0x1400E652D;
	constexpr std::uintptr_t CG_ServerCommand_CaseLookup = 0x1400E655E;
	constexpr std::uintptr_t CG_ServerCommand_CaseIndex = 0x1400E7670;
	constexpr std::uintptr_t CG_ServerCommand_CaseTargets = 0x1400E75BC;
	constexpr std::uintptr_t CG_ServerCommand_Unknown = 0x1400E7587;
	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr unsigned int lastLetter = 0x7A;

	static const std::uint8_t letterTest[] = { 0x83, 0xF8, 0x7A, 0x0F, 0x87 };

	static const std::uint8_t caseLookup[] =
	{
		0x42, 0x0F, 0xB6, 0x84, 0x28, 0x70, 0x76, 0x0E, 0x00,
		0x41, 0x8B, 0x8C, 0x85, 0xBC, 0x75, 0x0E, 0x00,
	};

	static bool IsUnknownToEngine(char letter)
	{
		const int value = letter;

		if (static_cast<unsigned int>(value) > lastLetter)
		{
			return true;
		}

		const auto index = Utils::Hook::Get<std::uint8_t>(CG_ServerCommand_CaseIndex + value);
		const auto target = Utils::Hook::Get<std::uint32_t>(CG_ServerCommand_CaseTargets + index * sizeof(std::uint32_t));

		return Utils::Hook::Rebase(imageBase) + target == Utils::Hook::Rebase(CG_ServerCommand_Unknown);
	}

	void ServerCommands::OnCommand(std::int32_t command, const Handler& callback)
	{
		commands[command].push_back(callback);
	}

	bool ServerCommands::OnServerCommand()
	{
		const Command::ClientParams params;

		if (params.Size() < 1)
		{
			return false;
		}

		const auto handlers = commands.find(params.Get(0)[0]);

		if (handlers == commands.end())
		{
			return false;
		}

		for (const auto& handler : handlers->second)
		{
			if (handler(&params))
			{
				return true;
			}
		}

		return false;
	}

	void ServerCommands::CG_ServerCommand_Hook(int localClientNum)
	{
		const Command::ClientParams params;

		if (IsUnknownToEngine(params.Get(0)[0]) && OnServerCommand())
		{
			return;
		}

		reinterpret_cast<void(*)(int)>(serverCommandHook.GetOriginal())(localClientNum);
	}

	ServerCommands::ServerCommands()
	{
		if (!Utils::Hook::MatchesBytes(CG_ExecuteNewServerCommands_ServerCommandCall, serverCommandCall, sizeof(serverCommandCall))
			|| !Utils::Hook::MatchesBytes(CG_ServerCommand_LetterTest, letterTest, sizeof(letterTest))
			|| !Utils::Hook::MatchesBytes(CG_ServerCommand_CaseLookup, caseLookup, sizeof(caseLookup)))
		{
			Logger::Error("servercommands: CG_ServerCommand does not read as expected, IW4x's own server commands are ignored\n");
			return;
		}

		if (!serverCommandHook.Initialize(CG_ExecuteNewServerCommands_ServerCommandCall, reinterpret_cast<void*>(CG_ServerCommand_Hook), HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("servercommands: could not hook CG_ServerCommand, IW4x's own server commands are ignored\n");
			return;
		}

		serverCommandHook.Quick();
	}
}
