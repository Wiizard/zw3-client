#include "STDInclude.hpp"

#include "IO.hpp"
#include "Script.hpp"
#include "../Events.hpp"
#include "../Logger.hpp"

namespace Components::GSC
{
	const char* IO::forbiddenStrings[] = { R"(..)", R"(../)", R"(..\)" };

	FILE* IO::openScriptIOFileHandle;

	constexpr std::uintptr_t openFileEntry = 0x140423278;
	constexpr std::uintptr_t closeFileEntry = 0x140423290;
	constexpr std::uintptr_t emptyBuiltin = 0x140080E50;

	bool IO::ValidatePath(const char* function, const char* path)
	{
		for (std::size_t i = 0; i < std::extent_v<decltype(forbiddenStrings)>; ++i)
		{
			if (std::strstr(path, forbiddenStrings[i]) != nullptr)
			{
				Logger::Error("{}: directory traversal is not allowed!\n", function);
				return false;
			}
		}

		return true;
	}

	std::filesystem::path IO::BuildPath(const char* path)
	{
		std::string spath = path;

		if (!spath.starts_with(Game::SCRIPTDATA_DIR + "/"s) && !spath.starts_with(Game::SCRIPTDATA_DIR + "\\"s))
		{
			spath = Game::SCRIPTDATA_DIR + "/"s + spath;
		}

		std::filesystem::path coreRoot = Utils::GetBaseFilesLocation();

		if (coreRoot.empty())
		{
			coreRoot = std::filesystem::current_path();
		}

		coreRoot = coreRoot / "zw3" / "core";

		std::error_code error;
		std::filesystem::create_directories(coreRoot / Game::SCRIPTDATA_DIR, error);

		return coreRoot / spath;
	}

	void IO::GScr_OpenFile()
	{
		const auto* filepath = Game::Scr_GetString(0);
		const auto* mode = Game::Scr_GetString(1);

		if (!ValidatePath("OpenFile", filepath))
		{
			Game::Scr_AddInt(-1);
			return;
		}

		if (mode != "read"s)
		{
			Logger::Error("Valid openfile modes are 'read'\n");
			Game::Scr_AddInt(-1);
			return;
		}

		if (openScriptIOFileHandle)
		{
			Logger::Error("OpenFile failed. {} files already open\n", 1);
			Game::Scr_AddInt(-1);
			return;
		}

		const auto dest = BuildPath(filepath);

		_set_errno(0);
		const auto result = fopen_s(&openScriptIOFileHandle, dest.string().data(), "r");

		if (result || !openScriptIOFileHandle)
		{
			Logger::Error("OpenFile failed. '{}'", result);
			Game::Scr_AddInt(-1);
			return;
		}

		Game::Scr_AddInt(1);
	}

	void IO::GScr_ReadStream()
	{
		if (!openScriptIOFileHandle)
		{
			Logger::Error("ReadStream failed. File stream was not opened\n");
			return;
		}

		char line[1024]{};

		if (std::fgets(line, sizeof(line), openScriptIOFileHandle) != nullptr)
		{
			Game::Scr_AddString(line);
			return;
		}

		Logger::Warning("ReadStream failed.\n");

		if (std::feof(openScriptIOFileHandle))
		{
			Logger::Print("ReadStream: EOF reached\n");
		}
	}

	void IO::GScr_CloseFile()
	{
		if (!openScriptIOFileHandle)
		{
			Logger::Error("CloseFile failed. File stream was not opened\n");
			Game::Scr_AddInt(-1);
			return;
		}

		Game::Scr_AddInt(std::fclose(openScriptIOFileHandle));
		openScriptIOFileHandle = nullptr;
	}

	void IO::AddScriptFunctions()
	{
		Script::AddFunction("FileWrite", []
		{
			const auto* filepath = Game::Scr_GetString(0);
			const auto* text = Game::Scr_GetString(1);
			const auto* mode = Game::Scr_GetString(2);

			if (!ValidatePath("FileWrite", filepath))
			{
				return;
			}

			if (mode != "append"s && mode != "write"s)
			{
				Logger::Warning("FileWrite: mode not defined or was wrong, defaulting to 'write'\n");
				mode = "write";
			}

			const auto append = mode == "append"s;
			const auto dest = BuildPath(filepath);
			Utils::IO::WriteFile(dest.string(), text, append);
		});

		Script::AddFunction("FileRead", []
		{
			const auto* filepath = Game::Scr_GetString(0);

			if (!ValidatePath("FileRead", filepath))
			{
				return;
			}

			const auto dest = BuildPath(filepath);

			std::string file;

			if (!Utils::IO::ReadFile(dest.string(), &file))
			{
				Logger::Error("FileRead: file '{}' not found!\n", dest.string());
				return;
			}

			file = file.substr(0, (1 << 16) - 1);
			Game::Scr_AddString(file.data());
		});

		Script::AddFunction("FileExists", []
		{
			const auto* filepath = Game::Scr_GetString(0);

			if (!ValidatePath("FileExists", filepath))
			{
				return;
			}

			const auto dest = BuildPath(filepath);
			Game::Scr_AddBool(Utils::IO::FileExists(dest.string()));
		});

		Script::AddFunction("FileRemove", []
		{
			const auto* filepath = Game::Scr_GetString(0);

			if (!ValidatePath("FileRemove", filepath))
			{
				return;
			}

			const auto dest = BuildPath(filepath);
			Game::Scr_AddBool(Utils::IO::RemoveFile(dest.string()));
		});

		Script::AddFunction("FileRename", []
		{
			const auto* filepath = Game::Scr_GetString(0);
			const auto* destpath = Game::Scr_GetString(0);

			if (!ValidatePath("FileRename", filepath) || !ValidatePath("FileRename", destpath))
			{
				return;
			}

			const auto from = BuildPath(filepath);
			const auto to = BuildPath(destpath);

			std::error_code err;
			std::filesystem::rename(from, to, err);

			if (err.value())
			{
				Logger::Error("FileRename: failed to rename file! Error message: {}\n", err.message());
				Game::Scr_AddInt(-1);
				return;
			}

			Game::Scr_AddInt(1);
		});

		Script::AddFunction("FileCopy", []
		{
			const auto* filepath = Game::Scr_GetString(0);
			const auto* destpath = Game::Scr_GetString(0);

			if (!ValidatePath("FileCopy", filepath) || !ValidatePath("FileCopy", destpath))
			{
				return;
			}

			const auto from = BuildPath(filepath);
			const auto to = BuildPath(destpath);

			std::error_code err;
			std::filesystem::copy(from, to, err);

			if (err.value())
			{
				Logger::Error("FileCopy: failed to copy file! Error message: {}\n", err.message());
				Game::Scr_AddInt(-1);
				return;
			}

			Game::Scr_AddInt(1);
		});

		Script::AddFunction("ReadStream", GScr_ReadStream);

		Script::AddMethod("setanim", [](const Game::scr_entref_t entref)
		{
			auto* const ent = Script::Scr_GetPlayerEntity(entref);
			const int anim = Game::Scr_GetInt(0);

			ent->client->ps.weapState[0].weapAnim = anim;

			for (auto& hand : ent->client->ps.weapState)
			{
				hand.weaponDelay = 0;
				hand.weaponRestrictKickTime = 0;
				hand.weaponState = 0;
				hand.weaponTime = 0;
			}
		});

		Script::AddMethod("setAnimTime", [](const Game::scr_entref_t entref)
		{
			auto* const ent = Script::Scr_GetPlayerEntity(entref);
			const int time = Game::Scr_GetInt(0);

			for (auto& hand : ent->client->ps.weapState)
			{
				hand.weaponTime = time;
			}
		});
	}

	IO::IO()
	{
		openScriptIOFileHandle = nullptr;

		AddScriptFunctions();

		const bool isOpenFileIntact = Utils::Hook::Get<std::uintptr_t>(openFileEntry + 8) == Utils::Hook::Rebase(emptyBuiltin);
		const bool isCloseFileIntact = Utils::Hook::Get<std::uintptr_t>(closeFileEntry + 8) == Utils::Hook::Rebase(emptyBuiltin);

		if (!isOpenFileIntact || !isCloseFileIntact)
		{
			Logger::Error("io: the openfile and closefile entries do not read as expected, they stay the engine's\n");
		}
		else
		{
			Utils::Hook::Set<Game::BuiltinFunction>(openFileEntry + 8, GScr_OpenFile);
			Utils::Hook::Set<int>(openFileEntry + 16, 0);

			Utils::Hook::Set<Game::BuiltinFunction>(closeFileEntry + 8, GScr_CloseFile);
			Utils::Hook::Set<int>(closeFileEntry + 16, 0);
		}

		Events::OnVMShutdown([]
		{
			if (openScriptIOFileHandle)
			{
				std::fclose(openScriptIOFileHandle);
				openScriptIOFileHandle = nullptr;
			}
		});
	}
}
