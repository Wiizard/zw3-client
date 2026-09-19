
#include "Updater.hpp"
#include "Scheduler.hpp"
#include "version.hpp"

#include <Utils/WebIO.hpp>
#include <Utils/IO.hpp>

#include <rapidjson/document.h>

namespace Components
{
	namespace
	{
		const Game::dvar_t* cl_updateAvailable;

		constexpr auto* UPDATE_MANIFEST_URL = "https://zw3.eu/dl/launcher/game_update.yaml";
		constexpr auto* INSTALLED_VERSION_FILE = "main/zw3/zw3-version.json";

		struct Version
		{
			int major{};
			int minor{};
			int patch{};
		};

		std::optional<Version> ParseVersion(const std::string_view value)
		{
			Version version{};
			char suffix{};
			if (std::sscanf(value.data(), "%d.%d.%d%c", &version.major, &version.minor, &version.patch, &suffix) != 3)
				return {};
			return version;
		}

		std::optional<std::string> ReadInstalledVersion()
		{
			const auto state = Utils::IO::ReadFile(INSTALLED_VERSION_FILE);
			if (state.empty()) return {};

			rapidjson::Document document{};
			const rapidjson::ParseResult parseResult = document.Parse(state);
			if (!parseResult || !document.IsObject() || !document.HasMember("version") || !document["version"].IsString()) return {};

			const std::string version = document["version"].GetString();
			return ParseVersion(version) ? std::make_optional(version) : std::nullopt;
		}

		bool IsNewerVersion(const std::string_view latest, const std::optional<std::string>& installed)
		{
			const auto latestVersion = ParseVersion(latest);
			if (!latestVersion) return false;
			if (!installed) return true;

			const auto installedVersion = ParseVersion(*installed);
			return !installedVersion || std::tie(latestVersion->major, latestVersion->minor, latestVersion->patch)
				> std::tie(installedVersion->major, installedVersion->minor, installedVersion->patch);
		}
		constexpr auto* INSTALL_GUIDE_REMOTE_URL = "https://forum.alterware.dev/t/how-to-install-the-alterware-launcher/56";

		void CheckForUpdate()
		{
			const auto result = Utils::WebIO("ZW3", UPDATE_MANIFEST_URL).setTimeout(5000)->get();
			if (result.empty())
			{
				Logger::Print("Could not fetch the ZW3 update manifest\n");
				return;
			}

			std::istringstream stream(result);
			std::string line;
			std::string version;
			while (std::getline(stream, line))
			{
				if (line.rfind("version:", 0) == 0)
				{
					version = line.substr(8);
					Utils::String::Trim(version);
				}
			}

			if (version.empty())
			{
				Logger::Print("ZW3 update manifest has no version\n");
				return;
			}

			// Engine dvars are updated only after returning to the main pipeline.
			const auto updateAvailable = IsNewerVersion(version, ReadInstalledVersion());
			Scheduler::Once([updateAvailable]
			{
				Game::Dvar_SetBool(cl_updateAvailable, updateAvailable);
			}, Scheduler::Pipeline::MAIN);
		}

		// Depending on Linux/Windows 32/64 there are a few things we must check
		std::optional<std::string> GetLauncher()
		{
			const char* launchers[] = {
				"zw3_launcher.exe",
				"zw3-launcher.exe",
				"Zombie Warfare 3 Launcher.exe",
				Utils::IsWineEnvironment() ? "zw3-launcher" : nullptr
			};

			for (const char* launcher : launchers) {
				if (launcher && Utils::IO::FileExists(launcher)) {
					return launcher;
				}
			}

			return {};
		}
	}

	Updater::Updater()
	{
		cl_updateAvailable = Game::Dvar_RegisterBool("cl_updateAvailable", false, Game::DVAR_NONE, "Whether a ZW3 update is available");
		Scheduler::Once(CheckForUpdate, Scheduler::Pipeline::ASYNC);

		UIScript::Add("checkForUpdate", [](const UIScript::Token& /*token*/, const Game::uiInfo_s* /*info*/)
		{
			Scheduler::Once(CheckForUpdate, Scheduler::Pipeline::ASYNC);
		});

		UIScript::Add("getAutoUpdate", [](const UIScript::Token& /*token*/, const Game::uiInfo_s* /*info*/)
		{
			const auto exe = GetLauncher();
			if (exe.has_value())
			{
				Game::Sys_QuitAndStartProcess(exe.value().data());
				return;
			}

			// No launcher was found on the system, time to tell them to download it from GitHub
			Utils::OpenUrl(INSTALL_GUIDE_REMOTE_URL);
		});
	}
}
