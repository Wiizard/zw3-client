#include "STDInclude.hpp"

#include "Logger.hpp"
#include "Console.hpp"
#include "Dvar.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "Network.hpp"
#include "Scheduler.hpp"

namespace Components
{
	std::string Logger::logFile;
	std::mutex Logger::writeMutex;
	void(*Logger::pipeCallback)(const std::string&) = nullptr;
	Game::dvar_t* Logger::iw4x_fail2ban_location = nullptr;

	void Logger::Write(const char* message, const char* prefix)
	{
		if (!message)
		{
			return;
		}

		std::lock_guard _(writeMutex);

		std::string line;

		if (prefix)
		{
			line.append(prefix);
			line.append("^7: ");
		}

		line.append(message);

		if (pipeCallback)
		{
			pipeCallback(line);
			return;
		}

		Console::Print(line.data());

		if (logFile.empty())
		{
			return;
		}

		const auto now = std::chrono::system_clock::now();
		std::string stamped = std::format("[{:%Y-%m-%d %H:%M:%S}] ", std::chrono::floor<std::chrono::seconds>(now));
		stamped.append(line);
		stamped.append("\n");

		Utils::IO::WriteFile(logFile, stamped, true);
	}

	void Logger::SetLogFile(const std::string& file)
	{
		std::lock_guard _(writeMutex);

		logFile = file;
	}

	void Logger::PipeOutput(void(*callback)(const std::string&))
	{
		pipeCallback = callback;
	}

	void Logger::WriteFail2Ban(std::string message)
	{
		static const auto shouldPrint = Flags::HasFlag("fail2ban");

		if (!shouldPrint || !iw4x_fail2ban_location)
		{
			return;
		}

		static auto shouldStampNextLine = true;

		if (shouldStampNextLine)
		{
			const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			std::tm timeInfo{};
			localtime_s(&timeInfo, &now);

			std::ostringstream stamp;
			stamp << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S ");

			message.insert(0, stamp.str());
		}

		shouldStampNextLine = (message.find('\n') != std::string::npos);

		Utils::IO::WriteFile(iw4x_fail2ban_location->current.string, message, true);
	}

	static std::vector<Network::Address> loggingAddresses[2];
	static std::recursive_mutex loggingMutex;
	static Utils::Concurrency::Container<std::vector<std::pair<std::string, bool>>> networkQueue;

	constexpr std::uintptr_t G_LogPrintf = 0x14019D7E0;
	static const std::uint8_t logPrintfEntry[] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08 };
	constexpr std::uintptr_t level_time = 0x1418673E8;
	constexpr std::uintptr_t level_logFile = 0x141867050;

	static Utils::Hook logPrintfHook;

	constexpr std::uintptr_t FS_FOpenFileByMode_BuildOSPathCall = 0x140275A8B;
	constexpr std::uintptr_t FS_BuildOSPathForThread = 0x1402755C0;

	static Utils::Hook buildOSPathHook;
	static Dvar::Var iw4x_onelog;

	static void FS_BuildOSPath_Hk(const char* base, const char* game, const char* qpath, char* ospath)
	{
		const char* folder = game;

		if (iw4x_onelog.IsValid() && iw4x_onelog.Get<bool>())
		{
			const Dvar::Var g_log("g_log");

			if (g_log.IsValid() && g_log.Get<std::string>() == qpath)
			{
				folder = "userraw";
			}
		}

		reinterpret_cast<void(*)(const char*, const char*, const char*, char*)>(Utils::Hook::Rebase(FS_BuildOSPathForThread))(base, folder, qpath, ospath);
	}

	constexpr std::uintptr_t GScr_LogString_Jump = 0x1401A3128;
	constexpr std::uintptr_t LSP_LogString = 0x1401B0B70;
	constexpr std::uintptr_t ScrCmd_LogString_Jump = 0x1401A3164;
	constexpr std::uintptr_t LSP_LogStringAboutUser = 0x1401B0C20;

	static Utils::Hook logStringHook;
	static Utils::Hook logStringAboutUserHook;

	static void LSP_LogString_Stub([[maybe_unused]] const int localControllerIndex, const char* string)
	{
		Logger::NetworkLog(string, false);
	}

	static void LSP_LogStringAboutUser_Stub([[maybe_unused]] const int localControllerIndex, const std::uint64_t xuid, const char* string)
	{
		Logger::NetworkLog(Utils::String::VA("%" PRIx64 ";%s", xuid, string), false);
	}

	void Logger::NetworkLog(const char* data, const bool gLog)
	{
		if (!data)
		{
			return;
		}

		std::lock_guard lock(loggingMutex);

		if (loggingAddresses[gLog].empty())
		{
			return;
		}

		if (!Game::Sys_IsMainThread())
		{
			networkQueue.Access([data, gLog](std::vector<std::pair<std::string, bool>>& queue)
			{
				queue.emplace_back(data, gLog);
			});

			return;
		}

		thread_local bool isSending = false;

		if (isSending)
		{
			return;
		}

		isSending = true;

		for (const auto& address : loggingAddresses[gLog])
		{
			Network::SendCommand(address, "print", data);
		}

		isSending = false;
	}

	void Logger::FlushNetworkQueue()
	{
		std::vector<std::pair<std::string, bool>> queued;

		networkQueue.Access([&queued](std::vector<std::pair<std::string, bool>>& queue)
		{
			queued.swap(queue);
		});

		for (const auto& [data, gLog] : queued)
		{
			NetworkLog(data.data(), gLog);
		}
	}

	void Logger::G_LogPrintf_Hk(const char* fmt, ...)
	{
		char line[1024]{};
		char message[1024]{};

		va_list ap;
		va_start(ap, fmt);
		vsnprintf_s(message, _TRUNCATE, fmt, ap);
		va_end(ap);

		const auto time = Utils::Hook::Get<int>(level_time) / 1000;
		const auto length = sprintf_s(line, "%3i:%i%i %s", time / 60, time % 60 / 10, time % 60 % 10, message);

		const auto gameLogFile = Utils::Hook::Get<Game::fileHandle_t>(level_logFile);

		if (gameLogFile && length > 0)
		{
			Game::FS_Write(line, length, gameLogFile);
		}

		NetworkLog(line, true);
	}

	static void AddLoggingAddress(std::vector<Network::Address>& addresses, const Command::Params* params)
	{
		if (params->Size() < 2)
		{
			return;
		}

		std::lock_guard lock(loggingMutex);

		const Network::Address address(params->Get(1));

		if (std::ranges::find(addresses, address) == addresses.end())
		{
			addresses.push_back(address);
		}
	}

	static void RemoveLoggingAddress(std::vector<Network::Address>& addresses, const Command::Params* params)
	{
		if (params->Size() < 2)
		{
			return;
		}

		std::lock_guard lock(loggingMutex);

		const auto num = std::atoi(params->Get(1));

		if (std::to_string(num) == params->Get(1) && static_cast<unsigned int>(num) < addresses.size())
		{
			const auto entry = addresses.begin() + num;
			Logger::Print("Address {} removed\n", entry->GetString());
			addresses.erase(entry);
			return;
		}

		const Network::Address address(params->Get(1));
		const auto entry = std::ranges::find(addresses, address);

		if (entry == addresses.end())
		{
			Logger::Print("Address {} not found!\n", address.GetString());
			return;
		}

		addresses.erase(entry);
		Logger::Print("Address {} removed\n", address.GetString());
	}

	static void ListLoggingAddresses(const std::vector<Network::Address>& addresses)
	{
		Logger::Print("# ID: Address\n");
		Logger::Print("-------------\n");

		std::lock_guard lock(loggingMutex);

		for (std::size_t i = 0; i < addresses.size(); ++i)
		{
			Logger::Print("#{:03d}: {}\n", i, addresses[i].GetString());
		}
	}

	void Logger::AddServerCommands()
	{
		Command::AddSV("log_add", [](const Command::Params* params)
		{
			AddLoggingAddress(loggingAddresses[0], params);
		});

		Command::AddSV("log_del", [](const Command::Params* params)
		{
			RemoveLoggingAddress(loggingAddresses[0], params);
		});

		Command::AddSV("log_list", []([[maybe_unused]] const Command::Params* params)
		{
			ListLoggingAddresses(loggingAddresses[0]);
		});

		Command::AddSV("g_log_add", [](const Command::Params* params)
		{
			AddLoggingAddress(loggingAddresses[1], params);
		});

		Command::AddSV("g_log_del", [](const Command::Params* params)
		{
			RemoveLoggingAddress(loggingAddresses[1], params);
		});

		Command::AddSV("g_log_list", []([[maybe_unused]] const Command::Params* params)
		{
			ListLoggingAddresses(loggingAddresses[1]);
		});
	}

	Logger::Logger()
	{
		if (Flags::HasFlag("log"))
		{
			SetLogFile("iw4x\\iw4x.log");
		}

		Events::OnDvarInit([]
		{
			iw4x_onelog = Dvar::Register("iw4x_onelog", false, Game::DVAR_LATCH, "Only write the game log to the 'userraw' OS folder");
			iw4x_fail2ban_location = Dvar::Register("iw4x_fail2ban_location", "/var/log/iw4x.log", Game::DVAR_NONE, "Fail2Ban logfile location").Get();
		});

		if (!Utils::Hook::BranchesTo(FS_FOpenFileByMode_BuildOSPathCall, FS_BuildOSPathForThread, HOOK_CALL)
			|| !buildOSPathHook.Initialize(FS_FOpenFileByMode_BuildOSPathCall, reinterpret_cast<void*>(FS_BuildOSPath_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Error("logger: FS_FOpenFileByMode does not read as expected, iw4x_onelog does nothing\n");
		}
		else
		{
			buildOSPathHook.Quick();
		}

		const bool isLogStringExpected = Utils::Hook::BranchesTo(GScr_LogString_Jump, LSP_LogString, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(ScrCmd_LogString_Jump, LSP_LogStringAboutUser, HOOK_JUMP);

		bool isLogStringSeated = isLogStringExpected;

		if (isLogStringExpected)
		{
			isLogStringSeated = logStringHook.Initialize(GScr_LogString_Jump, reinterpret_cast<void*>(LSP_LogString_Stub), HOOK_JUMP)->Install()->IsInstalled();
			isLogStringSeated = logStringAboutUserHook.Initialize(ScrCmd_LogString_Jump, reinterpret_cast<void*>(LSP_LogStringAboutUser_Stub), HOOK_JUMP)->Install()->IsInstalled() && isLogStringSeated;
		}

		if (!isLogStringSeated)
		{
			logStringHook.Uninstall();
			logStringAboutUserHook.Uninstall();

			Error("logger: the logstring builtins do not read as expected, logstring still goes to the LSP\n");
		}
		else
		{
			logStringHook.Quick();
			logStringAboutUserHook.Quick();
		}

		Events::OnSVInit(AddServerCommands);
		Scheduler::Loop(FlushNetworkQueue, Scheduler::Pipeline::SERVER);

		if (!Utils::Hook::MatchesBytes(G_LogPrintf, logPrintfEntry, sizeof(logPrintfEntry))
			|| !logPrintfHook.Initialize(G_LogPrintf, reinterpret_cast<void*>(G_LogPrintf_Hk), HOOK_JUMP)->Install()->IsInstalled())
		{
			Error("logger: G_LogPrintf does not read as expected, g_log_add gets no game log\n");
		}
		else
		{
			logPrintfHook.Quick();
		}
	}
}
