#include "STDInclude.hpp"

#include "RawFiles.hpp"
#include "Command.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"

namespace Components
{
	constexpr std::uintptr_t Scr_ReadFile_FastFile = 0x14021EBA0;
	constexpr std::uintptr_t Scr_ReadFile_FastFileCalls[] =
	{
		0x14021DCD6,
		0x14021DEAD,
		0x140212359,
	};

	constexpr std::uintptr_t FS_FOpenFileByMode = 0x1402759C0;
	constexpr std::uintptr_t FS_Read = 0x140277CB0;
	constexpr std::uintptr_t FS_FCloseFile = 0x140275920;
	constexpr std::uintptr_t Hunk_AllocateTempMemoryHigh = 0x14027F3E0;
	constexpr int fsRead = 0;

	static Utils::Hook readFileHooks[std::size(Scr_ReadFile_FastFileCalls)];

	static char lastScriptRead[64];

	constexpr std::uintptr_t DB_ReadRawFile = 0x14012F370;
	static const std::uint8_t readRawFileEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };

	static Utils::Hook readRawFileHook;

	constexpr std::uintptr_t UI_UpdateGameTypesList_VaCall = 0x14026F073;
	constexpr std::uintptr_t UI_UpdateGameTypesList_ReadBuffer = 0x14026F078;
	constexpr std::uintptr_t UI_UpdateGameTypesList_StoreBuffer = 0x14026F0C7;
	constexpr std::uintptr_t va = 0x14028D1E0;
	constexpr std::uintptr_t Z_VirtualAllocInternal = 0x14027F860;
	static const std::uint8_t readBufferHead[] = { 0x48, 0x8B, 0xD0, 0xB9, 0x24, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD8 };
	static const std::uint8_t storeBuffer[] = { 0x48, 0x89, 0x5C, 0x24, 0x68, 0x48, 0x85, 0xDB, 0x75, 0x54 };

	static Utils::Hook gameTypeBufferHook;

	constexpr std::uintptr_t Com_LoadInfoString = 0x1401F6F00;
	constexpr std::uintptr_t Com_LoadInfoStringCalls[] =
	{
		0x14009CA8C,
		0x14019BB47,
		0x1401FF861,
	};
	constexpr std::uintptr_t Info_Validate = 0x14028C9F0;

	constexpr int infoStringBufferSize = 0x2000;

	static Utils::Hook loadInfoStringHooks[std::size(Com_LoadInfoStringCalls)];

	static char* UI_UpdateGameTypesList_GetMenuBuffer(const char* format, const char* gameType)
	{
		return RawFiles::GetMenuBuffer(Utils::String::VA(format, gameType));
	}

	char* RawFiles::ReadRawFile(const char* filename, char* buf, int size)
	{
		Game::fileHandle_t fileHandle = 0;
		const auto fileSize = Game::FS_FOpenFileRead(filename, &fileHandle);

		if (fileHandle)
		{
			if ((fileSize + 1) <= size)
			{
				Game::FS_Read(buf, fileSize, fileHandle);
				buf[fileSize] = '\0';
				Game::FS_FCloseFile(fileHandle);
				return buf;
			}

			Game::FS_FCloseFile(fileHandle);
			Logger::Error("Ignoring raw file '{}' as it exceeds buffer size {} > {}\n", filename, fileSize, size);
		}

		auto* rawfile = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_RAWFILE, filename);

		if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, filename))
		{
			return nullptr;
		}

		Game::DB_GetRawBuffer(rawfile, buf, size);
		return buf;
	}

	static char* Z_VirtualAlloc(const int size)
	{
		return static_cast<char*>(reinterpret_cast<void*(*)(int)>(Utils::Hook::Rebase(Z_VirtualAllocInternal))(size));
	}

	char* RawFiles::GetMenuBuffer(const char* filename)
	{
		Game::fileHandle_t fileHandle = 0;
		const auto fileSize = Game::FS_FOpenFileRead(filename, &fileHandle);

		if (fileHandle)
		{
			if (fileSize < 0x8000)
			{
				auto* const buffer = Z_VirtualAlloc(fileSize + 1);
				Game::FS_Read(buffer, fileSize, fileHandle);
				Game::FS_FCloseFile(fileHandle);
				return buffer;
			}

			Game::FS_FCloseFile(fileHandle);
			Logger::Error("Menu file too large: {} is {}, max allowed is {}\n", filename, fileSize, 0x8000);
		}

		auto* const rawfile = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_RAWFILE, filename);
		if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_RAWFILE, filename))
		{
			Logger::Error("Menu file not found: {}, using default\n", filename);
			return nullptr;
		}

		const auto length = Game::DB_GetRawFileLen(rawfile);
		auto* const buffer = Z_VirtualAlloc(length);
		Game::DB_GetRawBuffer(rawfile, buffer, length);
		return buffer;
	}

	char* RawFiles::Com_LoadInfoString_LoadObj(const char* fileName, const char* fileDesc, const char* ident, char* loadBuffer)
	{
		Game::fileHandle_t fileHandle = 0;
		const auto fileLen = Game::FS_FOpenFileByMode(fileName, &fileHandle, Game::FS_READ);

		if (fileLen < 0)
		{
			Logger::Debug("Could not load {} [{}] as rawfile", fileDesc, fileName);
			return nullptr;
		}

		const auto identLen = static_cast<int>(std::strlen(ident));
		Game::FS_Read(loadBuffer, identLen, fileHandle);
		loadBuffer[identLen] = '\0';

		if (std::strncmp(loadBuffer, ident, identLen) != 0)
		{
			Game::FS_FCloseFile(fileHandle);
			Game::Com_Error(Game::ERR_DROP, "\x15" "File [%s] is not a %s\n", fileName, fileDesc);
			return nullptr;
		}

		if ((fileLen - identLen) >= infoStringBufferSize)
		{
			Game::FS_FCloseFile(fileHandle);
			Game::Com_Error(Game::ERR_DROP, "\x15" "File [%s] is too long of a %s to parse\n", fileName, fileDesc);
			return nullptr;
		}

		Game::FS_Read(loadBuffer, fileLen - identLen, fileHandle);
		loadBuffer[fileLen - identLen] = '\0';
		Game::FS_FCloseFile(fileHandle);

		return loadBuffer;
	}

	const char* RawFiles::Com_LoadInfoString_Hk(const char* fileName, const char* fileDesc, const char* ident, char* loadBuffer)
	{
		const auto* buffer = Com_LoadInfoString_LoadObj(fileName, fileDesc, ident, loadBuffer);
		if (!buffer)
		{
			return reinterpret_cast<const char*(*)(const char*, const char*, const char*, char*)>(
				Utils::Hook::Rebase(Com_LoadInfoString))(fileName, fileDesc, ident, loadBuffer);
		}

		if (!reinterpret_cast<int(*)(const char*)>(Utils::Hook::Rebase(Info_Validate))(buffer))
		{
			Game::Com_Error(Game::ERR_DROP, "\x15" "File [%s] is not a valid %s\n", fileName, fileDesc);
			return nullptr;
		}

		return buffer;
	}

	const char* RawFiles::LastScriptRead()
	{
		return lastScriptRead;
	}

	char* RawFiles::Scr_ReadFile_Stub(const char* filename, const char* extFilename)
	{
		if (extFilename && std::string_view(extFilename).ends_with(".gsc"))
		{
			strncpy_s(lastScriptRead, extFilename, _TRUNCATE);
		}

		std::int64_t file = 0;
		const int length = reinterpret_cast<int(*)(const char*, std::int64_t*, int)>(
			Utils::Hook::Rebase(FS_FOpenFileByMode))(extFilename, &file, fsRead);

		if (length < 0)
		{
			return reinterpret_cast<char*(*)(const char*, const char*)>(Utils::Hook::Rebase(Scr_ReadFile_FastFile))(filename, extFilename);
		}

		auto* const buffer = reinterpret_cast<char*(*)(int)>(Utils::Hook::Rebase(Hunk_AllocateTempMemoryHigh))(length + 1);

		reinterpret_cast<int(*)(void*, int, std::int64_t)>(Utils::Hook::Rebase(FS_Read))(buffer, length, file);
		buffer[length] = '\0';

		reinterpret_cast<void(*)(std::int64_t)>(Utils::Hook::Rebase(FS_FCloseFile))(file);

		return buffer;
	}

	RawFiles::RawFiles()
	{
		Command::Add("dumpraw", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("Specify a filename!\n");
				return;
			}

			FileSystem::File file(params->Join(1));
			if (file.Exists())
			{
				Utils::IO::WriteFile("raw/" + file.GetName(), file.GetBuffer());
				Logger::Print("File '{}' written to raw!\n", file.GetName());
				return;
			}

			const auto* data = Scr_ReadFile_Stub(file.GetName().data(), file.GetName().data());

			if (data)
			{
				Utils::IO::WriteFile("raw/" + file.GetName(), data);
				Logger::Print("File '{}' written to raw!\n", file.GetName());
			}
			else
			{
				Logger::Print("File '{}' does not exist!\n", file.GetName());
			}
		});

		for (const std::uintptr_t site : Scr_ReadFile_FastFileCalls)
		{
			if (!Utils::Hook::BranchesTo(site, Scr_ReadFile_FastFile, HOOK_CALL))
			{
				Logger::Error("rawfiles: 0x{:X} no longer calls Scr_ReadFile_FastFile, IW4x's scripts will not load\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(Scr_ReadFile_FastFileCalls); ++i)
		{
			isSeated = readFileHooks[i].Initialize(Scr_ReadFile_FastFileCalls[i], reinterpret_cast<void*>(Scr_ReadFile_Stub), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : readFileHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("rawfiles: could not seat every hook, IW4x's scripts will not load\n");
			return;
		}

		for (auto& hook : readFileHooks)
		{
			hook.Quick();
		}

		if (!Utils::Hook::MatchesBytes(DB_ReadRawFile, readRawFileEntry, sizeof(readRawFileEntry))
			|| !readRawFileHook.Initialize(DB_ReadRawFile, reinterpret_cast<void*>(ReadRawFile), HOOK_JUMP)->Install()->IsInstalled())
		{
			Logger::Error("rawfiles: DB_ReadRawFile does not read as expected, raw files come only from the zones\n");
		}
		else
		{
			readRawFileHook.Quick();
		}

		const bool isGameTypeReadExpected = Utils::Hook::BranchesTo(UI_UpdateGameTypesList_VaCall, va, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(UI_UpdateGameTypesList_ReadBuffer, readBufferHead, sizeof(readBufferHead))
			&& Utils::Hook::MatchesBytes(UI_UpdateGameTypesList_StoreBuffer, storeBuffer, sizeof(storeBuffer));

		if (!isGameTypeReadExpected
			|| !gameTypeBufferHook.Initialize(UI_UpdateGameTypesList_VaCall, reinterpret_cast<void*>(UI_UpdateGameTypesList_GetMenuBuffer), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("rawfiles: UI_UpdateGameTypesList does not read as expected, gametype files come only from the zones\n");
		}
		else
		{
			gameTypeBufferHook.Quick();

			const auto jumpToStore = static_cast<std::uint8_t>(UI_UpdateGameTypesList_StoreBuffer - (UI_UpdateGameTypesList_ReadBuffer + 5));
			Utils::Hook::Set<std::array<std::uint8_t, 5>>(UI_UpdateGameTypesList_ReadBuffer, { 0x48, 0x8B, 0xD8, 0xEB, jumpToStore });
		}

		bool isInfoStringSeated = true;

		for (std::size_t i = 0; i < std::size(Com_LoadInfoStringCalls); ++i)
		{
			isInfoStringSeated = isInfoStringSeated
				&& Utils::Hook::BranchesTo(Com_LoadInfoStringCalls[i], Com_LoadInfoString, HOOK_CALL)
				&& loadInfoStringHooks[i].Initialize(Com_LoadInfoStringCalls[i], reinterpret_cast<void*>(Com_LoadInfoString_Hk), HOOK_CALL)->Install()->IsInstalled();
		}

		if (!isInfoStringSeated)
		{
			for (auto& hook : loadInfoStringHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("rawfiles: a Com_LoadInfoString caller does not read as expected, info strings come only from the zones\n");
			return;
		}

		for (auto& hook : loadInfoStringHooks)
		{
			hook.Quick();
		}
	}
}
