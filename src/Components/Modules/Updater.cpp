#include "STDInclude.hpp"

#include "Updater.hpp"
#include "Command.hpp"
#include "Dvar.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "LobbyScene.hpp"
#include "Logger.hpp"
#include "Menus.hpp"
#include "Scheduler.hpp"
#include "UIScript.hpp"
#include "ZoneBuilder.hpp"
#include <fstream>

namespace Components
{
	constexpr auto* updateManifestUrl = "https://zw3.eu/dl/launcher/game_update.yaml";
	constexpr auto* installGuideUrl = "https://forum.alterware.dev/t/how-to-install-the-alterware-launcher/56";
	constexpr auto* noChangelog = "No changelog was supplied for this update.";


	static std::string ReadCurrentVersion()
	{
		HMODULE module = nullptr;

		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&ReadCurrentVersion), &module))
		{
			return "";
		}

		wchar_t modulePath[MAX_PATH]{};
		const auto length = GetModuleFileNameW(module, modulePath, MAX_PATH);

		if (!length || length >= MAX_PATH)
		{
			return "";
		}

		const auto filePath = std::filesystem::path(modulePath).parent_path() / "zw3/core/ui_mp/common/zw3.inc";
		std::ifstream file(filePath);
		std::string line;

		if (!file)
		{
			Logger::Print("Could not read ZW3 version file\n");
			return "";
		}

		while (std::getline(file, line))
		{
			Utils::String::Trim(line);

			if (!line.starts_with("#define GAME_VERSION "))
			{
				continue;
			}

			const auto start = line.find('"');
			const auto end = line.find('"', start == std::string::npos ? start : start + 1);

			if (start == std::string::npos || end == std::string::npos)
			{
				break;
			}

			auto version = line.substr(start + 1, end - start - 1);

			if (version.starts_with("Version "))
			{
				version.erase(0, sizeof("Version ") - 1);
			}

			return version;
		}

		Logger::Print("Could not find ZW3 version\n");
		return "";
	}

	static const std::string& GetCurrentVersion()
	{
		static const std::string version = ReadCurrentVersion();
		return version;
	}


	struct UpdateVersion
	{
		int major;
		int minor;
		int patch;
	};

	struct UpdateManifest
	{
		std::string version;
		std::string changelog;
	};

	static Dvar::Var cl_updateAvailable;
	static Dvar::Var cl_updateVersion;
	static Dvar::Var cl_updateCurrentVersion;
	static Dvar::Var cl_updateChangelog;
	static Dvar::Var cl_updateStatus;
	static Dvar::Var cl_updatePopupDisabled;

	static std::atomic_bool isUpdateCheckRunning = false;
	static bool isClientInitialized = false;
	static bool isStartupUpdateCheckComplete = false;

	static std::optional<UpdateVersion> TryParseVersion(const std::string& text)
	{
		UpdateVersion version{};
		char suffix = 0;

		if (std::sscanf(text.data(), "%d.%d.%d%c", &version.major, &version.minor, &version.patch, &suffix) != 3)
		{
			return std::nullopt;
		}

		return version;
	}

	static bool IsNewerVersion(const std::string& latest, const std::string& installed)
	{
		const auto latestVersion = TryParseVersion(latest);

		if (!latestVersion)
		{
			return false;
		}

		const auto installedVersion = TryParseVersion(installed);

		if (!installedVersion)
		{
			return true;
		}

		return std::tie(latestVersion->major, latestVersion->minor, latestVersion->patch)
			> std::tie(installedVersion->major, installedVersion->minor, installedVersion->patch);
	}

	static std::optional<UpdateManifest> TryParseManifest(const std::string& yaml)
	{
		UpdateManifest manifest;
		std::istringstream stream(yaml);
		std::string line;
		bool isReadingNotes = false;

		while (std::getline(stream, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			if (!isReadingNotes && line.starts_with("version:"))
			{
				manifest.version = line.substr(8);
				Utils::String::Trim(manifest.version);

				const bool isQuoted = manifest.version.size() >= 2 && manifest.version.front() == '"' && manifest.version.back() == '"';

				if (isQuoted)
				{
					manifest.version = manifest.version.substr(1, manifest.version.size() - 2);
				}

				continue;
			}

			if (!isReadingNotes && line.starts_with("notes:"))
			{
				isReadingNotes = true;
				continue;
			}

			if (isReadingNotes)
			{
				if (line.starts_with("  "))
				{
					line.erase(0, 2);
				}

				manifest.changelog.append(line);
				manifest.changelog.push_back('\n');
			}
		}

		Utils::String::Trim(manifest.changelog);

		if (!TryParseVersion(manifest.version))
		{
			return std::nullopt;
		}

		if (manifest.changelog.empty())
		{
			manifest.changelog = noChangelog;
		}

		return manifest;
	}

	static void SetUpdateState(const bool isAvailable, const std::string& version, const std::string& changelog,
		const std::string& status, const bool isComplete = true)
	{
		const bool isRegistered = cl_updateAvailable.IsValid() && cl_updateVersion.IsValid() && cl_updateCurrentVersion.IsValid()
			&& cl_updateChangelog.IsValid() && cl_updateStatus.IsValid();

		if (!isRegistered)
		{
			return;
		}

		cl_updateAvailable.Set(isAvailable);
		cl_updateVersion.Set(version);
		cl_updateCurrentVersion.Set(GetCurrentVersion());
		cl_updateChangelog.Set(changelog);
		cl_updateStatus.Set(status);
		isStartupUpdateCheckComplete = isComplete;
	}

	static bool ShowStartupUpdatePopup()
	{
		// Initialization can pump frames before the saved config has finished.
		if (!isClientInitialized || !isStartupUpdateCheckComplete)
		{
			return false;
		}

		if (!cl_updateAvailable.Get<bool>() || cl_updatePopupDisabled.Get<bool>())
		{
			return true;
		}

		// End the startup prompt if the player has already started connecting.
		if (Game::CL_GetLocalClientConnectionState(0) >= Game::CA_CONNECTING)
		{
			return true;
		}

		if (LobbyScene::IsStartupLoading() || LobbyScene::IsCinematicActive() || LobbyScene::IsTransitionActive())
		{
			return false;
		}

		const auto* const context = Game::uiContext;
		if (!context || context->openMenuCount <= 0 || context->openMenuCount > static_cast<int>(ARRAYSIZE(context->menuStack)))
		{
			return false;
		}

		const auto* const top = context->menuStack[context->openMenuCount - 1];
		if (!top || !top->window.name)
		{
			return false;
		}

		if (_stricmp(top->window.name, "popup_zw3_update") == 0)
		{
			return true;
		}

		if (_stricmp(top->window.name, "main_text") != 0 && _stricmp(top->window.name, "pregame_loaderror") != 0)
		{
			return false;
		}

		return Game::Menus_OpenByName(Game::uiContext, "popup_zw3_update");
	}

	static void CheckForUpdate()
	{
		if (isUpdateCheckRunning.exchange(true))
		{
			return;
		}

		Scheduler::Once([]
		{
			SetUpdateState(false, "", "", "Checking for updates...", false);
		}, Scheduler::Pipeline::MAIN);

		const auto result = Utils::WebIO("ZW3", updateManifestUrl).SetTimeout(5000)->Get();

		if (result.empty())
		{
			Logger::Print("Could not fetch the ZW3 update manifest\n");

			Scheduler::Once([]
			{
				SetUpdateState(false, "", "", "Update check failed.");
				isUpdateCheckRunning = false;
			}, Scheduler::Pipeline::MAIN);

			return;
		}

		const auto manifest = TryParseManifest(result);

		if (!manifest)
		{
			Logger::Print("ZW3 update manifest is invalid\n");

			Scheduler::Once([]
			{
				SetUpdateState(false, "", "", "Update check failed.");
				isUpdateCheckRunning = false;
			}, Scheduler::Pipeline::MAIN);

			return;
		}

		const auto& currentVersion = GetCurrentVersion();

		if (!TryParseVersion(currentVersion))
		{
			Logger::Print("Could not determine the installed ZW3 version\n");

			Scheduler::Once([]
				{
					SetUpdateState(false, "", "", "Could not determine installed version.");
					isUpdateCheckRunning = false;
				}, Scheduler::Pipeline::MAIN);

			return;
		}

		const bool isAvailable = IsNewerVersion(manifest->version, currentVersion);

		Scheduler::Once([latest = *manifest, isAvailable]
		{
			std::string status = "Zombie Warfare 3 is up to date.";

			if (isAvailable)
			{
				status = std::format("Update available: v{}", latest.version);
			}

			SetUpdateState(isAvailable, latest.version, latest.changelog, status);
			isUpdateCheckRunning = false;
		}, Scheduler::Pipeline::MAIN);
	}

	static std::optional<std::string> TryFindLauncher()
	{
		std::vector<std::string> launcherNames = { "zw3_launcher.exe", "zw3-launcher.exe", "Zombie Warfare 3 Launcher.exe" };

		if (Utils::IsWineEnvironment())
		{
			launcherNames.emplace_back("zw3-launcher");
		}

		for (const std::string& name : launcherNames)
		{
			if (Utils::IO::FileExists(name))
			{
				return name;
			}
		}

		for (const char* const variable : { "ProgramW6432", "ProgramFiles", "ProgramFiles(x86)" })
		{
			const char* const programFiles = std::getenv(variable);

			if (!programFiles)
			{
				continue;
			}

			const auto installed = std::filesystem::path(programFiles) / "Zombie Warfare 3 Launcher" / "zw3_launcher.exe";

			if (Utils::IO::FileExists(installed.string()))
			{
				return installed.string();
			}
		}

		const char* const localAppData = std::getenv("LOCALAPPDATA");

		if (localAppData)
		{
			const auto programs = std::filesystem::path(localAppData) / "Programs";
			const std::filesystem::path candidates[] =
			{
				programs / "Zombie Warfare 3 Launcher" / "zw3_launcher.exe",
				programs / "zw3_launcher" / "zw3_launcher.exe",
			};

			for (const auto& candidate : candidates)
			{
				if (Utils::IO::FileExists(candidate.string()))
				{
					return candidate.string();
				}
			}
		}

		for (const std::string& name : launcherNames)
		{
			char resolved[MAX_PATH]{};

			if (SearchPathA(nullptr, name.data(), nullptr, ARRAYSIZE(resolved), resolved, nullptr) != 0)
			{
				return std::string(resolved);
			}
		}

		return std::nullopt;
	}

	static bool TryStartLauncher(const std::string& launcher)
	{
		const std::filesystem::path workingDir = std::filesystem::current_path();
		const std::wstring application = (workingDir / Utils::String::Convert(launcher)).wstring();
		std::wstring commandLine = std::format(L"\"{}\" iw4x --pass \"{}\"", application, Utils::GetLaunchParameters());

		SetEnvironmentVariableA("MW2_INSTALL", workingDir.string().data());

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};

		if (!CreateProcessW(application.data(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, workingDir.wstring().data(), &startupInfo, &processInfo))
		{
			Logger::Error("updater: could not start {}, error {}\n", launcher, GetLastError());
			return false;
		}

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}

	Updater::Updater()
	{
		Menus::Add("ui_mp/popup_zw3_update.menu");
		Events::OnClientInit([] { isClientInitialized = true; });

		Events::OnDvarInit([]
		{
			cl_updateAvailable = Dvar::Register("cl_updateAvailable", false, Game::DVAR_INTERNAL, "Whether a ZW3 update is available");
			cl_updateVersion = Dvar::Register("cl_updateVersion", "", Game::DVAR_INTERNAL, "Latest available ZW3 version");
			cl_updateCurrentVersion = Dvar::Register("cl_updateCurrentVersion", GetCurrentVersion().c_str(), Game::DVAR_INTERNAL, "Installed ZW3 version");
			cl_updateChangelog = Dvar::Register("cl_updateChangelog", "", Game::DVAR_INTERNAL, "Changelog for the latest ZW3 update");
			cl_updateStatus = Dvar::Register("cl_updateStatus", "Checking for updates...", Game::DVAR_INTERNAL, "Readable ZW3 updater status");
			cl_updatePopupDisabled = Dvar::Register("cl_updatePopupDisabled", false, Game::DVAR_ARCHIVE, "Do not automatically show the ZW3 update popup at startup");

			Scheduler::Once(CheckForUpdate, Scheduler::Pipeline::ASYNC);
			if (!Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled())
			{
				Scheduler::Schedule(ShowStartupUpdatePopup, Scheduler::Pipeline::MAIN);
			}
		});

		UIScript::Add("checkForUpdate", []([[maybe_unused]] const UIScript::Token& token)
		{
			Scheduler::Once(CheckForUpdate, Scheduler::Pipeline::ASYNC);
		});

		UIScript::Add("getAutoUpdate", []([[maybe_unused]] const UIScript::Token& token)
		{
			const auto launcher = TryFindLauncher();

			if (!launcher)
			{
				Utils::OpenUrl(installGuideUrl);
				return;
			}

			if (!TryStartLauncher(*launcher))
			{
				return;
			}

			Command::Execute("quit", false);
		});

		UIScript::Add("showUpdateDetails", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (!cl_updateAvailable.Get<bool>())
			{
				return;
			}

			Game::Menus_OpenByName(Game::uiContext, "popup_zw3_update");
		});

		UIScript::Add("neverShowUpdatePopup", []([[maybe_unused]] const UIScript::Token& token)
		{
			cl_updatePopupDisabled.Set(true);
		});
	}
}
