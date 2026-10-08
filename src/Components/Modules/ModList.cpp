#include "STDInclude.hpp"

#include "ModList.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "UIFeeder.hpp"

namespace Components
{
	static std::unordered_map<unsigned int, char*> customClassesPrefixedNames{};

	std::vector<std::string> ModList::mods;
	unsigned int ModList::currentMod;

	Dvar::Var ModList::cl_modVidRestart;

	constexpr std::uintptr_t GetPlayerData_GetStringCall = 0x140251EB5;
	constexpr std::uintptr_t StructuredData_GetString_Engine = 0x140281690;

	constexpr std::uintptr_t LiveStorage_DownloadStats_Core_ThrottleJl = 0x1401FAA90;
	static const std::uint8_t throttleJl[] = { 0x0F, 0x8C, 0xD9, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t cgs_localServer = 0x14058752C;

	static Utils::Hook getStringHook;

	bool ModList::HasMod(const std::string& modName)
	{
		const auto list = FileSystem::GetSysFileList(Dvar::Var("fs_basepath").Get<std::string>() + "\\mods", "", true);

		for (const auto& mod : list)
		{
			if (mod == modName)
			{
				return true;
			}
		}

		return false;
	}

	void ModList::ClearMods()
	{
		if (*Game::fs_gameDirVar == nullptr || *(*Game::fs_gameDirVar)->current.string == '\0')
		{
			return;
		}

		Game::Dvar_SetString(*Game::fs_gameDirVar, "");

		FastFiles::PrefetchZone("mod");

		if (cl_modVidRestart.Get<bool>())
		{
			Command::Execute("vid_restart", false);
		}
		else
		{
			Command::Execute("closemenu mods_menu", false);
		}
	}

	unsigned int ModList::GetItemCount()
	{
		return static_cast<unsigned int>(mods.size());
	}

	const char* ModList::GetItemText(unsigned int index, [[maybe_unused]] int column)
	{
		if (index < mods.size())
		{
			return mods[index].data();
		}

		return "...";
	}

	void ModList::Select(unsigned int index)
	{
		currentMod = index;
	}

	void ModList::UIScript_LoadMods([[maybe_unused]] const UIScript::Token& token)
	{
		const auto folder = (*Game::fs_basepath)->current.string + "\\mods"s;
		Logger::Debug("Searching for mods in {}...", folder);
		mods = FileSystem::GetSysFileList(folder, "", true);
		Logger::Debug("Found {} mods!", mods.size());
	}

	void ModList::UIScript_RunMod([[maybe_unused]] const UIScript::Token& token)
	{
		if (currentMod < mods.size())
		{
			RunMod(mods[currentMod]);
		}
	}

	void ModList::UIScript_ClearMods([[maybe_unused]] const UIScript::Token& token)
	{
		ClearMods();
	}

	void ModList::RunMod(const std::string& mod)
	{
		Game::Dvar_SetString(*Game::fs_gameDirVar, Utils::String::Format("mods/{}", mod));
		FastFiles::PrefetchZone("mod");

		if (cl_modVidRestart.Get<bool>())
		{
			Command::Execute("vid_restart", false);
		}
		else
		{
			Command::Execute("closemenu mods_menu", false);
		}
	}

	char* ModList::StructuredData_GetString(Game::StructuredDataLookup* lookup, Game::StructuredDataBuffer* buffer)
	{
		auto* result = reinterpret_cast<char*(*)(Game::StructuredDataLookup*, Game::StructuredDataBuffer*)>(Utils::Hook::Rebase(StructuredData_GetString_Engine))(lookup, buffer);

		if (lookup->error == Game::LOOKUP_ERROR_NONE &&
			lookup->type->type == Game::StructuredDataTypeCategory::DATA_STRING &&
			lookup->type->u.stringDataLength == 21)
		{
			if (*Game::fs_gameDirVar != nullptr && *(*Game::fs_gameDirVar)->current.string != '\0')
			{
				const std::string currentName = result;

				if (currentName.empty())
				{
					return result;
				}

				constexpr char prefix[] = "* ";
				constexpr auto prefixLength = std::size(prefix) - 1;

				if (currentName.starts_with(prefix))
				{
					return result;
				}

				const auto prefixedNameLength = lookup->type->u.stringDataLength + prefixLength;

				if (!customClassesPrefixedNames.contains(lookup->offset))
				{
					customClassesPrefixedNames[lookup->offset] = Utils::Memory::AllocateArray<char>(prefixedNameLength);
				}

				auto* namePtr = customClassesPrefixedNames[lookup->offset];

				std::memcpy(&namePtr[prefixLength], result, lookup->type->u.stringDataLength);
				std::memcpy(namePtr, prefix, prefixLength);

				return namePtr;
			}
		}

		return result;
	}

	ModList::ModList()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		if (!Utils::Hook::BranchesTo(GetPlayerData_GetStringCall, StructuredData_GetString_Engine, false)
			|| !Utils::Hook::MatchesBytes(LiveStorage_DownloadStats_Core_ThrottleJl, throttleJl, sizeof(throttleJl)))
		{
			Logger::Error("modlist: GetPlayerData or LiveStorage_DownloadStats_Core does not read as expected, no mod list\n");
			return;
		}

		if (!getStringHook.Initialize(GetPlayerData_GetStringCall, reinterpret_cast<void*>(StructuredData_GetString), HOOK_CALL)->Install()->IsInstalled())
		{
			getStringHook.Uninstall();
			Logger::Error("modlist: could not seat the StructuredData_GetString hook, no mod list\n");
			return;
		}

		Utils::Hook::Nop(LiveStorage_DownloadStats_Core_ThrottleJl, sizeof(throttleJl));

		currentMod = 0;

		Events::OnDvarInit([]
		{
			cl_modVidRestart = Dvar::Register("cl_modVidRestart", true, Game::DVAR_ARCHIVE, "Perform a vid_restart when loading a mod.");
		});

		UIScript::Add("LoadMods", UIScript_LoadMods);
		UIScript::Add("RunMod", UIScript_RunMod);
		UIScript::Add("ClearMods", UIScript_ClearMods);

		UIFeeder::Add(9.0f, GetItemCount, GetItemText, Select);

		Events::OnCLDisconnected([](bool wasConnected) -> void
		{
			if (*reinterpret_cast<const int*>(Utils::Hook::Rebase(cgs_localServer)))
			{
				return;
			}

			if (!wasConnected)
			{
				return;
			}

			if (Flags::HasFlag("disable-mod-unloading"))
			{
				return;
			}

			if (*Game::fs_gameDirVar != nullptr && *(*Game::fs_gameDirVar)->current.string != '\0')
			{
				return;
			}

			ClearMods();
		});
	}
}
