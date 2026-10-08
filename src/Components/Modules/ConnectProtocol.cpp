#include "STDInclude.hpp"

#include "ConnectProtocol.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "IPCPipe.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Singleton.hpp"

namespace Components
{
	bool ConnectProtocol::isEvaluated = false;
	std::string ConnectProtocol::connectString;

	constexpr const char* scheme = "zw3";
	constexpr const char* schemePrefix = "zw3://";
	constexpr const char* classesKey = "SOFTWARE\\Classes\\zw3";
	constexpr const char* commandKey = "SOFTWARE\\Classes\\zw3\\shell\\open\\command";

	constexpr std::uintptr_t Sys_IsDatabaseReady2 = 0x14020A8B0;

	constexpr std::uintptr_t Com_Init_IntroTest = 0x1401F5A1D;

	static const std::uint8_t introTest[] = { 0x74, 0x2F, 0x48, 0x8D, 0x15 };

	bool ConnectProtocol::IsEvaluated()
	{
		return isEvaluated;
	}

	bool ConnectProtocol::Used()
	{
		if (!IsEvaluated())
		{
			EvaluateProtocol();
		}

		return !connectString.empty();
	}

	bool ConnectProtocol::InstallProtocol()
	{
		char ownPath[MAX_PATH]{};
		const DWORD length = GetModuleFileNameA(nullptr, ownPath, MAX_PATH);

		if (length == 0 || length >= MAX_PATH)
		{
			return false;
		}

		std::string workingDirectory(ownPath);
		const auto separator = workingDirectory.find_last_of('\\');

		if (separator == std::string::npos)
		{
			return false;
		}

		workingDirectory.resize(separator + 1);
		SetCurrentDirectoryA(workingDirectory.data());

		const std::string command = std::format("\"{}\" \"%1\"", ownPath);

		HKEY key = nullptr;

		if (RegOpenKeyExA(HKEY_CURRENT_USER, commandKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
		{
			char registered[MAX_PATH * 2]{};
			DWORD size = sizeof(registered) - 1;
			const LONG queried = RegQueryValueExA(key, nullptr, nullptr, nullptr, reinterpret_cast<BYTE*>(registered), &size);
			RegCloseKey(key);

			if (queried == ERROR_SUCCESS && command == registered)
			{
				return true;
			}
		}

		RegDeleteTreeA(HKEY_CURRENT_USER, classesKey);

		HKEY classes = nullptr;

		if (RegCreateKeyExA(HKEY_CURRENT_USER, classesKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &classes, nullptr) != ERROR_SUCCESS)
		{
			return false;
		}

		const std::string protocol = std::format("URL:{} Protocol", scheme);
		bool isWritten = RegSetValueExA(classes, "URL Protocol", 0, REG_SZ, reinterpret_cast<const BYTE*>(protocol.data()),
			static_cast<DWORD>(protocol.size() + 1)) == ERROR_SUCCESS;

		HKEY icon = nullptr;

		if (isWritten && RegCreateKeyExA(classes, "DefaultIcon", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &icon, nullptr) == ERROR_SUCCESS)
		{
			const std::string iconPath = std::format("{},0", ownPath);
			isWritten = RegSetValueExA(icon, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(iconPath.data()),
				static_cast<DWORD>(iconPath.size() + 1)) == ERROR_SUCCESS;
			RegCloseKey(icon);
		}
		else
		{
			isWritten = false;
		}

		HKEY open = nullptr;

		if (isWritten && RegCreateKeyExA(classes, "shell\\open\\command", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &open, nullptr) == ERROR_SUCCESS)
		{
			isWritten = RegSetValueExA(open, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.data()),
				static_cast<DWORD>(command.size() + 1)) == ERROR_SUCCESS;
			RegCloseKey(open);
		}
		else
		{
			isWritten = false;
		}

		RegCloseKey(classes);
		return isWritten;
	}

	void ConnectProtocol::EvaluateProtocol()
	{
		if (isEvaluated)
		{
			return;
		}

		isEvaluated = true;

		std::string commandLine = GetCommandLineA();
		const auto position = commandLine.find(schemePrefix);

		if (position == std::string::npos)
		{
			return;
		}

		commandLine = commandLine.substr(position + std::strlen(schemePrefix));

		const auto end = commandLine.find_first_of("/\" ");

		if (end != std::string::npos)
		{
			commandLine.resize(end);
		}

		connectString = commandLine;
	}

	void ConnectProtocol::Invocation()
	{
		if (Used())
		{
			Command::Execute(std::format("connect {}", connectString), false);
		}
	}

	ConnectProtocol::ConnectProtocol()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		IPCPipe::On("connect", [](const std::string& data)
		{
			Command::Execute(std::format("connect {}", data), false);
		});

		Scheduler::Schedule([]
		{
			if (!reinterpret_cast<bool(*)()>(Utils::Hook::Rebase(Sys_IsDatabaseReady2))())
			{
				return false;
			}

			Scheduler::Once(Invocation, Scheduler::Pipeline::MAIN);
			return true;
		}, Scheduler::Pipeline::MAIN);

		if (!InstallProtocol())
		{
			Logger::Error("connectprotocol: could not register {}:// links\n", scheme);
		}

		EvaluateProtocol();

		if (!Used())
		{
			return;
		}

		if (!Singleton::IsFirstInstance())
		{
			IPCPipe::Write("connect", connectString);
			ExitProcess(EXIT_SUCCESS);
		}

		if (Utils::Hook::MatchesBytes(Com_Init_IntroTest, introTest, sizeof(introTest)))
		{
			Utils::Hook::Set<std::uint8_t>(Com_Init_IntroTest, 0xEB);
		}
		else
		{
			Logger::Error("connectprotocol: Com_Init does not read as expected, the intro still plays\n");
		}

		Scheduler::Once([]
		{
			Command::Execute("openmenu popup_reconnectingtoparty", false);
		}, Scheduler::Pipeline::MAIN, std::chrono::seconds(8));
	}
}
