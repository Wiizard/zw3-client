#pragma once

namespace Components
{
	class Logger : public Component
	{
	public:
		Logger();

		static void Print(std::string_view format, auto&&... args)
		{
			Write(Utils::String::Format(format, std::forward<decltype(args)>(args)...));
		}

		static void Debug([[maybe_unused]] std::string_view format, [[maybe_unused]] auto&&... args)
		{
#if defined(DEBUG)
			Write(Utils::String::Format(format, std::forward<decltype(args)>(args)...), "^5debug");
#endif
		}

		static void Warning(std::string_view format, auto&&... args)
		{
			Write(Utils::String::Format(format, std::forward<decltype(args)>(args)...), "^3warning");
		}

		static void Error(std::string_view format, auto&&... args)
		{
			Write(Utils::String::Format(format, std::forward<decltype(args)>(args)...), "^1error");
		}

		[[noreturn]] static void Fatal(std::string_view format, auto&&... args)
		{
			const auto* const message = Utils::String::Format(format, std::forward<decltype(args)>(args)...);
			Write(message, "^1fatal");
			Game::Com_Error(1, "%s", message);
			std::terminate();
		}

		static void PrintFail2Ban(std::string_view format, auto&&... args)
		{
			WriteFail2Ban(Utils::String::Format(format, std::forward<decltype(args)>(args)...));
		}

		static void SetLogFile(const std::string& file);

		static void PipeOutput(void(*callback)(const std::string&));

		static void NetworkLog(const char* data, bool gLog);

	private:
		static std::string logFile;
		static std::mutex writeMutex;
		static void(*pipeCallback)(const std::string&);
		static Game::dvar_t* iw4x_fail2ban_location;

		static void Write(const char* message, const char* prefix = nullptr);
		static void WriteFail2Ban(std::string message);

		static void FlushNetworkQueue();
		static void G_LogPrintf_Hk(const char* fmt, ...);
		static void AddServerCommands();
	};
}
