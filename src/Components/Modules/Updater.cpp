
#include "Updater.hpp"
#include "Scheduler.hpp"
#include "Events.hpp"
#include "version.hpp"

#include <Utils/WebIO.hpp>
#include <Utils/IO.hpp>

namespace Components
{
	namespace
	{
		const Game::dvar_t* cl_updateAvailable{};
		const Game::dvar_t* cl_updateVersion{};
		const Game::dvar_t* cl_updateCurrentVersion{};
		const Game::dvar_t* cl_updateChangelog{};
		const Game::dvar_t* cl_updateStatus{};
		std::atomic_bool updateCheckInProgress{ false };

		constexpr auto* UPDATE_MANIFEST_URL = "https://zw3.eu/dl/launcher/game_update.yaml";
		constexpr auto* CURRENT_VERSION = "4.0.0";

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

		struct Manifest
		{
			std::string version;
			std::string changelog;
		};

		bool IsNewerVersion(const std::string_view latest, const std::string_view installed)
		{
			const auto latestVersion = ParseVersion(latest);
			if (!latestVersion) return false;

			const auto installedVersion = ParseVersion(installed);
			return !installedVersion || std::tie(latestVersion->major, latestVersion->minor, latestVersion->patch)
				> std::tie(installedVersion->major, installedVersion->minor, installedVersion->patch);
		}

		std::optional<Manifest> ParseManifest(const std::string& yaml)
		{
			Manifest manifest{};
			std::istringstream stream(yaml);
			std::string line;
			bool readingNotes = false;

			while (std::getline(stream, line))
			{
				if (!line.empty() && line.back() == '\r') line.pop_back();

				if (!readingNotes && line.rfind("version:", 0) == 0)
				{
					manifest.version = line.substr(8);
					Utils::String::Trim(manifest.version);
					if (manifest.version.size() >= 2 && manifest.version.front() == '"' && manifest.version.back() == '"')
					{
						manifest.version = manifest.version.substr(1, manifest.version.size() - 2);
					}
					continue;
				}

				if (!readingNotes && line.rfind("notes:", 0) == 0)
				{
					readingNotes = true;
					continue;
				}

				if (readingNotes)
				{
					if (line.starts_with("  ")) line.erase(0, 2);
					manifest.changelog.append(line).push_back('\n');
				}
			}

			Utils::String::Trim(manifest.changelog);
			if (!ParseVersion(manifest.version)) return {};
			if (manifest.changelog.empty()) manifest.changelog = "No changelog was supplied for this update.";
			return manifest;
		}

		void SetUpdateState(const bool available, const std::string& version,
			const std::string& changelog, const std::string& status)
		{
			if (!cl_updateAvailable || !cl_updateVersion || !cl_updateCurrentVersion ||
				!cl_updateChangelog || !cl_updateStatus)
			{
				return;
			}

			Game::Dvar_SetBool(cl_updateAvailable, available);
			Game::Dvar_SetString(cl_updateVersion, version.c_str());
			Game::Dvar_SetString(cl_updateCurrentVersion, CURRENT_VERSION);
			Game::Dvar_SetString(cl_updateChangelog, changelog.c_str());
			Game::Dvar_SetString(cl_updateStatus, status.c_str());
		}

		constexpr auto* INSTALL_GUIDE_REMOTE_URL = "https://forum.alterware.dev/t/how-to-install-the-alterware-launcher/56";

		void CheckForUpdate()
		{
			if (updateCheckInProgress.exchange(true)) return;

			Scheduler::Once([]
			{
				SetUpdateState(false, "", "", "CHECKING FOR ZW3 UPDATES...");
			}, Scheduler::Pipeline::MAIN);

			const auto result = Utils::WebIO("ZW3", UPDATE_MANIFEST_URL).setTimeout(5000)->get();
			if (result.empty())
			{
				Logger::Print("Could not fetch the ZW3 update manifest\n");
				Scheduler::Once([]
				{
					SetUpdateState(false, "", "", "UPDATE CHECK UNAVAILABLE");
					updateCheckInProgress = false;
				}, Scheduler::Pipeline::MAIN);
				return;
			}

			const auto manifest = ParseManifest(result);
			if (!manifest)
			{
				Logger::Print("ZW3 update manifest is invalid\n");
				Scheduler::Once([]
				{
					SetUpdateState(false, "", "", "UPDATE CHECK UNAVAILABLE");
					updateCheckInProgress = false;
				}, Scheduler::Pipeline::MAIN);
				return;
			}

			const auto updateAvailable = IsNewerVersion(manifest->version, CURRENT_VERSION);
			Scheduler::Once([manifest = *manifest, updateAvailable]
			{
				const auto status = updateAvailable
					? Utils::String::Format("Update available: v{}", manifest.version)
					: std::string("ZW3 is up to date.");
				SetUpdateState(updateAvailable, manifest.version, manifest.changelog, status);
				updateCheckInProgress = false;
			}, Scheduler::Pipeline::MAIN);
		}

		// Depending on Linux/Windows 32/64 there are a few things we must check
		std::optional<std::string> GetLauncher()
		{
			const char* launcherNames[] = {
				"zw3_launcher.exe",
				"zw3-launcher.exe",
				"Zombie Warfare 3 Launcher.exe",
				Utils::IsWineEnvironment() ? "zw3-launcher" : nullptr
			};

			for (const char* launcher : launcherNames) {
				if (launcher && Utils::IO::FileExists(launcher)) {
					return launcher;
				}
			}

			// The release launcher installs here by default. Do not require users
			// to keep a second copy beside the game executable.
			const char* programFilesVariables[] = { "ProgramW6432", "ProgramFiles", "ProgramFiles(x86)" };
			for (const char* variable : programFilesVariables)
			{
				const auto* programFiles = std::getenv(variable);
				if (!programFiles) continue;
				const auto installed = std::filesystem::path(programFiles) /
					"Zombie Warfare 3 Launcher" / "zw3_launcher.exe";
				if (Utils::IO::FileExists(installed.string())) return installed.string();
			}

			if (const auto* localAppData = std::getenv("LOCALAPPDATA"))
			{
				const std::filesystem::path programs = std::filesystem::path(localAppData) / "Programs";
				const std::filesystem::path candidates[] = {
					programs / "Zombie Warfare 3 Launcher" / "zw3_launcher.exe",
					programs / "zw3_launcher" / "zw3_launcher.exe"
				};
				for (const auto& candidate : candidates)
				{
					if (Utils::IO::FileExists(candidate.string())) return candidate.string();
				}
			}

			// Finally honour a launcher available through PATH.
			for (const char* launcher : launcherNames)
			{
				if (!launcher) continue;
				char resolved[MAX_PATH]{};
				if (SearchPathA(nullptr, launcher, nullptr, ARRAYSIZE(resolved), resolved, nullptr) != 0)
				{
					return resolved;
				}
			}

			return {};
		}
	}

	Updater::Updater()
	{
		Events::OnDvarInit([]
		{
			cl_updateAvailable = Game::Dvar_RegisterBool("cl_updateAvailable", false, Game::DVAR_INTERNAL, "Whether a ZW3 update is available");
			cl_updateVersion = Game::Dvar_RegisterString("cl_updateVersion", "", Game::DVAR_INTERNAL, "Latest available ZW3 version");
			cl_updateCurrentVersion = Game::Dvar_RegisterString("cl_updateCurrentVersion", CURRENT_VERSION, Game::DVAR_INTERNAL, "Installed ZW3 version");
			cl_updateChangelog = Game::Dvar_RegisterString("cl_updateChangelog", "", Game::DVAR_INTERNAL, "Changelog for the latest ZW3 update");
			cl_updateStatus = Game::Dvar_RegisterString("cl_updateStatus", "CHECKING FOR ZW3 UPDATES...", Game::DVAR_INTERNAL, "Readable ZW3 updater status");
			Scheduler::Once(CheckForUpdate, Scheduler::Pipeline::ASYNC);
		});

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

		UIScript::Add("showUpdateDetails", [](const UIScript::Token& /*token*/, const Game::uiInfo_s* /*info*/)
		{
			if (cl_updateAvailable && cl_updateAvailable->current.enabled)
			{
				Game::Menus_OpenByName(Game::uiContext, "popup_zw3_update");
			}
		});
	}
}
