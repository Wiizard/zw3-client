#include "STDInclude.hpp"

#include "FileSystem.hpp"
#include "D3D9Ex.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Maps.hpp"
#include "ZoneBuilder.hpp"

namespace Components
{
	std::recursive_mutex FileSystem::fsMutex;

	constexpr unsigned int assetTypeRawFile = 0x24;
	Utils::Hook FileSystem::registerDvarsHook;
	Utils::Hook FileSystem::execSubStringHook;
	Utils::Hook FileSystem::execCompareHook;
	Utils::Hook FileSystem::startupHook;
	Utils::Hook FileSystem::addLocalizedHook;

	constexpr std::uintptr_t FS_RegisterDvarsCall = 0x140278859;

	constexpr int listAll = 2;

	constexpr int listTag = 10;

	constexpr std::uintptr_t iwdNameCollectReject = 0x140274E6F;
	constexpr std::uintptr_t iwdNameGate = 0x14027505F;
	constexpr std::uintptr_t iwdNameGateAccept = 0x1402750DD;

	constexpr std::uintptr_t Cmd_Exec_f_SubStringCall = 0x1401E75C1;
	constexpr std::uintptr_t Cmd_Exec_f_CompareCall = 0x1401E75D0;

	static const std::uint8_t execSubString[] = { 0x48, 0x8D, 0x4C, 0x24, 0x20, 0xE8, 0xFA, 0x47, 0x0A, 0x00 };
	static const std::uint8_t execCompare[] = { 0x48, 0x8B, 0xC8, 0xE8, 0x1B, 0x4B, 0x0A, 0x00, 0x85, 0xC0, 0x75, 0x53 };

	constexpr std::uintptr_t FS_FOpenFileByMode = 0x1402759C0;
	constexpr std::uintptr_t FS_FCloseFile = 0x140275920;
	constexpr int fsRead = 0;

	constexpr std::uintptr_t Cmd_ExecuteSingleCommandFromDisk_Match = 0x1401E7A6F;
	constexpr std::uintptr_t Cmd_ExecuteSingleCommandFromDisk_MatchJz = 0x1401E7A76;

	static const std::uint8_t diskCommandMatch[] = { 0xE8, 0x4C, 0x49, 0x0A, 0x00, 0x85, 0xC0, 0x74, 0x0D };

	constexpr std::uintptr_t FS_Startup_GameDirLoad = 0x140278B4C;

	static const std::uint8_t gameDirLoad[] = { 0x48, 0x8B, 0x05, 0x35, 0xC0, 0x3C, 0x06, 0x48, 0x8B, 0x48, 0x10 };

	constexpr std::uintptr_t fs_cdpath = 0x146644B78;
	constexpr std::uintptr_t fs_basepath = 0x146644B68;
	constexpr std::uintptr_t fs_homepath = 0x146644B60;
	constexpr std::uintptr_t fs_gameDirVar = 0x146644B88;
	constexpr std::uintptr_t FS_AddLocalizedGameDirectory = 0x140275290;

	constexpr std::uintptr_t FS_AddGameDirectory = 0x140274920;
	constexpr std::uintptr_t loc_language = 0x146521118;

	static const std::uint8_t addLocalizedEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x56, 0x57, 0x41, 0x54 };

	constexpr std::uintptr_t FS_AddIwdFilesForGameDirectory_NoIwd = 0x140274D82;
	constexpr std::uintptr_t FS_AddIwdFilesForGameDirectory_NoIwdError = 0x140274D8F;

	static const std::uint8_t noIwdError[] = { 0x85, 0xC9, 0x74, 0x0E, 0x48, 0x8D, 0x15, 0xF3, 0xF8, 0x12, 0x00, 0x33, 0xC9, 0xE8, 0x5C, 0xEB, 0xF7, 0xFF };

	constexpr std::uintptr_t FS_AddIwdFilesForGameDirectory_PermanentChecksums = 0x140275169;
	constexpr std::uintptr_t FS_AddIwdFilesForGameDirectory_LinkSearchPath = 0x140275214;

	static const std::uint8_t permanentChecksumsCount[] = { 0x8B, 0x0D, 0x39, 0x76, 0x3D, 0x06, 0x85, 0xC9, 0x7E, 0x4F };
	static const std::uint8_t linkSearchPath[] = { 0x41, 0x83, 0x7E, 0x18, 0x00 };

	constexpr std::uintptr_t Hunk_FreeTempMemory_MagicTest = 0x14027F776;
	constexpr std::uintptr_t Hunk_FreeTempMemory_BadMagic = 0x14027F782;
	constexpr std::uintptr_t Hunk_FreeTempMemory_Epilogue = 0x14027F7E9;

	static const std::uint8_t badMagicError[] =
	{
		0x81, 0x79, 0xE8, 0x92, 0x78, 0x53, 0x89, 0x48, 0x8B, 0xD9, 0x74, 0x0E, 0x48, 0x8D, 0x15, 0xC7,
		0x53, 0x12, 0x00, 0x33, 0xC9, 0xE8, 0x60, 0x41, 0xF7, 0xFF,
	};

	static const std::uint8_t tempMemoryEpilogue[] = { 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3 };

	static bool MatchesBytes(std::uintptr_t address, const std::uint8_t* expected, std::size_t length)
	{
		const auto* const live = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(address));

		return std::memcmp(live, expected, length) == 0;
	}

	constexpr std::uintptr_t FS_Restart = 0x140277FB0;
	constexpr std::uintptr_t FS_RestartCalls[] = { 0x14023B2D2, 0x140100328, 0x140111FEF };
	constexpr std::uintptr_t FS_Shutdown = 0x140278510;
	constexpr std::uintptr_t Com_Quit_f_ShutdownCall = 0x1401F5C81;
	constexpr std::uintptr_t DB_SyncXAssets = 0x14012F990;
	constexpr std::uintptr_t Z_FreeInternal = 0x14027F7F0;
	constexpr std::uintptr_t IwdFreeCalls[] = { 0x14027859C, 0x14027804C };

	static Utils::Hook syncHooks[std::size(FS_RestartCalls) + 1 + std::size(IwdFreeCalls)];

	constexpr std::uintptr_t Load_GfxTextureLoad_LoadTextureCall = 0x14011AE36;
	constexpr std::uintptr_t Load_Texture = 0x140037B00;

	static Utils::Hook loadTextureHook;

	constexpr std::uintptr_t fs_checksumFeed = 0x146644B98;

	constexpr std::uintptr_t CL_Vid_Restart_f_UpdateLanguageCall = 0x1400FDDA6;
	constexpr std::uintptr_t SEH_UpdateLanguageInfo = 0x14024F320;
	constexpr std::uintptr_t FS_NeedRestart = 0x140277AB0;
	constexpr std::uintptr_t clc_checksumFeed = 0x140BFB8D0;

	static Utils::Hook vidRestartHook;

	constexpr std::uintptr_t Sys_ListFiles_GetBufCall = 0x14028E456;
	constexpr std::uintptr_t LargeLocal_GetBuf = 0x14027E970;
	constexpr std::uintptr_t Sys_ListFiles_HunkSizes[] = { 0x14028E477, 0x14028E6C0 };
	constexpr std::uintptr_t Sys_ListFiles_Caps[] = { 0x14028E520, 0x14028E797 };
	constexpr std::uintptr_t FS_ListFilteredFiles_HunkSize = 0x14027706D;
	constexpr std::uintptr_t FS_ListFilteredFiles_ListSize = 0x140277077;
	constexpr std::uintptr_t FS_ListFilteredFiles_Cap = 0x1402772F0;
	constexpr std::uintptr_t FS_AddFileToList_Cap = 0x1402748AA;

	static const std::uint8_t hunkSize[] = { 0xB9, 0x00, 0x00, 0x02, 0x00 };
	static const std::uint8_t listFilesCap[] = { 0x48, 0x81, 0xFB, 0xFF, 0x1F, 0x00, 0x00 };
	static const std::uint8_t filteredListSize[] = { 0xBA, 0x08, 0x00, 0x01, 0x00 };
	static const std::uint8_t filteredCap[] = { 0x41, 0x81, 0xFD, 0xFF, 0x1F, 0x00, 0x00 };
	static const std::uint8_t addFileCap[] = { 0x81, 0xFE, 0xFF, 0x1F, 0x00, 0x00 };

	constexpr int fileCountMultiplier = 8;
	constexpr int newMaxFilesListed = 8191 * fileCountMultiplier;
	constexpr std::size_t fileListEntries = newMaxFilesListed + fileCountMultiplier;
	constexpr std::uint32_t listHunkSize = 0x20000 * fileCountMultiplier;
	constexpr std::uint32_t filteredListBytes = fileListEntries * sizeof(char*) + sizeof(char*);

	static Utils::Hook listFilesBufferHook;

	static char** Sys_ListFiles_GetBuf([[maybe_unused]] void* largeLocal)
	{
		thread_local std::vector<char*> fileList(fileListEntries);
		return fileList.data();
	}

	static void RaiseFileListLimit()
	{
		bool isExpected = Utils::Hook::BranchesTo(Sys_ListFiles_GetBufCall, LargeLocal_GetBuf, HOOK_CALL)
			&& MatchesBytes(FS_ListFilteredFiles_HunkSize, hunkSize, sizeof(hunkSize))
			&& MatchesBytes(FS_ListFilteredFiles_ListSize, filteredListSize, sizeof(filteredListSize))
			&& MatchesBytes(FS_ListFilteredFiles_Cap, filteredCap, sizeof(filteredCap))
			&& MatchesBytes(FS_AddFileToList_Cap, addFileCap, sizeof(addFileCap));

		for (const std::uintptr_t site : Sys_ListFiles_HunkSizes)
		{
			isExpected = isExpected && MatchesBytes(site, hunkSize, sizeof(hunkSize));
		}

		for (const std::uintptr_t site : Sys_ListFiles_Caps)
		{
			isExpected = isExpected && MatchesBytes(site, listFilesCap, sizeof(listFilesCap));
		}

		if (!isExpected || !listFilesBufferHook.Initialize(Sys_ListFiles_GetBufCall, reinterpret_cast<void*>(Sys_ListFiles_GetBuf), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("filesystem: Sys_ListFiles or FS_ListFilteredFiles does not read as expected, a listing stops at 8191 files\n");
			return;
		}

		listFilesBufferHook.Quick();

		for (const std::uintptr_t site : Sys_ListFiles_Caps)
		{
			Utils::Hook::Set<std::uint32_t>(site + 3, newMaxFilesListed);
		}

		for (const std::uintptr_t site : Sys_ListFiles_HunkSizes)
		{
			Utils::Hook::Set<std::uint32_t>(site + 1, listHunkSize);
		}

		Utils::Hook::Set<std::uint32_t>(FS_ListFilteredFiles_HunkSize + 1, listHunkSize);
		Utils::Hook::Set<std::uint32_t>(FS_ListFilteredFiles_ListSize + 1, filteredListBytes);
		Utils::Hook::Set<std::uint32_t>(FS_ListFilteredFiles_Cap + 3, newMaxFilesListed);
		Utils::Hook::Set<std::uint32_t>(FS_AddFileToList_Cap + 2, newMaxFilesListed);
	}

	void FileSystem::CL_Vid_Restart_f_UpdateLanguage_Hk()
	{
		const int checksumFeed = Utils::Hook::Get<int>(clc_checksumFeed);

		if (reinterpret_cast<int(*)(int)>(Utils::Hook::Rebase(FS_NeedRestart))(checksumFeed))
		{
			FsRestartSync(0, checksumFeed);
		}

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SEH_UpdateLanguageInfo))();
	}

	void FileSystem::FsRestartSync(int localClientNum, int checksumFeed)
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(DB_SyncXAssets))();

		const auto* fs_game = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(fs_gameDirVar));
		const bool isGameModified = fs_game && fs_game->modified;

		if (*Game::fs_searchpaths && !isGameModified)
		{
			*reinterpret_cast<int*>(Utils::Hook::Rebase(fs_checksumFeed)) = checksumFeed;

			for (auto* searchPath = *Game::fs_searchpaths; searchPath; searchPath = searchPath->next)
			{
				if (searchPath->iwd)
				{
					searchPath->iwd->referenced = 0;
				}
			}

			return;
		}

		Maps::GetUserMap()->FreeIwd();
		reinterpret_cast<void(*)(int, int)>(Utils::Hook::Rebase(FS_Restart))(localClientNum, checksumFeed);
		Maps::GetUserMap()->ReloadIwd();
	}

	void FileSystem::FsShutdownSync(int closemfp)
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(DB_SyncXAssets))();
		Maps::GetUserMap()->FreeIwd();
		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(FS_Shutdown))(closemfp);
	}

	void FileSystem::LoadTextureSync(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image)
	{
		D3D9Ex::LoadTexture(loadDef, image);
	}

	void FileSystem::IwdFreeStub(Game::iwd_t* iwd)
	{
		Maps::GetUserMap()->HandlePackfile(iwd);
		reinterpret_cast<void(*)(void*)>(Utils::Hook::Rebase(Z_FreeInternal))(iwd);
	}

	void FileSystem::HookSync()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
		};

		const HookSite sites[] =
		{
			{ FS_RestartCalls[0], FS_Restart, reinterpret_cast<void*>(FsRestartSync) },
			{ FS_RestartCalls[1], FS_Restart, reinterpret_cast<void*>(FsRestartSync) },
			{ FS_RestartCalls[2], FS_Restart, reinterpret_cast<void*>(FsRestartSync) },
			{ Com_Quit_f_ShutdownCall, FS_Shutdown, reinterpret_cast<void*>(FsShutdownSync) },
			{ IwdFreeCalls[0], Z_FreeInternal, reinterpret_cast<void*>(IwdFreeStub) },
			{ IwdFreeCalls[1], Z_FreeInternal, reinterpret_cast<void*>(IwdFreeStub) },
		};

		static_assert(std::size(sites) == std::size(syncHooks));

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, HOOK_CALL))
			{
				Logger::Error("filesystem: 0x{:X} no longer calls 0x{:X}, a usermap's iwd cannot be loaded safely\n", hookSite.site, hookSite.target);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = syncHooks[i].Initialize(sites[i].site, sites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : syncHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("filesystem: could not seat every sync hook, a usermap's iwd cannot be loaded safely\n");
			return;
		}

		for (auto& hook : syncHooks)
		{
			hook.Quick();
		}
	}

	void FileSystem::File::Read(Game::FsThread thread)
	{
		std::lock_guard lock(fsMutex);

		assert(!this->filePath.empty());

		Game::fileHandle_t handle = 0;
		const int length = Game::FS_FOpenFileReadForThread(this->filePath.data(), &handle, thread);

		if (!handle)
		{
			return;
		}

		this->buffer.resize(static_cast<std::size_t>(length));

		[[maybe_unused]] const int bytesRead = Game::FS_Read(this->buffer.data(), length, handle);
		assert(bytesRead == length);

		Game::FS_FCloseFile(handle);
	}

	void FileSystem::RawFile::Read()
	{
		this->buffer.clear();

		const void* rawfile = Game::DB_FindXAssetHeader(assetTypeRawFile, this->filePath.data());

		if (!rawfile || Game::DB_IsXAssetDefault(assetTypeRawFile, this->filePath.data()))
		{
			return;
		}

		this->buffer.resize(static_cast<std::size_t>(Game::DB_GetRawFileLen(rawfile)));
		Game::DB_GetRawBuffer(rawfile, this->buffer.data(), static_cast<int>(this->buffer.size()));
	}

	FileSystem::FileReader::FileReader(std::string file) : handle(0), name(std::move(file))
	{
		this->size = Game::FS_FOpenFileReadCurrentThread(this->name.data(), &this->handle);
	}

	FileSystem::FileReader::~FileReader()
	{
		if (this->Exists() && this->handle)
		{
			Game::FS_FCloseFile(this->handle);
		}
	}

	bool FileSystem::FileReader::Exists() const noexcept
	{
		return this->size >= 0 && this->handle;
	}

	std::string FileSystem::FileReader::GetName() const
	{
		return this->name;
	}

	int FileSystem::FileReader::GetSize() const noexcept
	{
		return this->size;
	}

	std::string FileSystem::FileReader::GetBuffer() const
	{
		if (!this->Exists())
		{
			return {};
		}

		const int position = Game::FS_FTell(this->handle);
		this->Seek(0, Game::FS_SEEK_SET);

		std::string buffer(static_cast<std::size_t>(this->size), '\0');

		if (!this->Read(buffer.data(), buffer.size()))
		{
			this->Seek(position, Game::FS_SEEK_SET);
			return {};
		}

		this->Seek(position, Game::FS_SEEK_SET);

		return buffer;
	}

	bool FileSystem::FileReader::Read(void* buffer, std::size_t readSize) const noexcept
	{
		if (!this->Exists() || static_cast<std::size_t>(this->size) < readSize)
		{
			return false;
		}

		return Game::FS_Read(buffer, static_cast<int>(readSize), this->handle) == static_cast<int>(readSize);
	}

	void FileSystem::FileReader::Seek(int offset, int origin) const
	{
		if (this->Exists())
		{
			Game::FS_Seek(this->handle, offset, origin);
		}
	}

	void FileSystem::FileWriter::Write(const std::string& data) const
	{
		if (this->handle)
		{
			Game::FS_Write(data.data(), static_cast<int>(data.size()), this->handle);
		}
	}

	void FileSystem::FileWriter::Open(bool append)
	{
		if (append)
		{
			Game::FS_FOpenFileByMode(this->filePath.data(), &this->handle, Game::FS_APPEND);
		}
		else
		{
			this->handle = Game::FS_FOpenFileWrite(this->filePath.data());
		}
	}

	void FileSystem::FileWriter::Close()
	{
		if (this->handle)
		{
			Game::FS_FCloseFile(this->handle);
		}
	}

	Game::FsThread FileSystem::GetCurrentThread()
	{
		if (Game::Sys_IsRenderThread())
		{
			return Game::FS_THREAD_BACKEND;
		}

		if (Game::Sys_IsServerThread())
		{
			return Game::FS_THREAD_SERVER;
		}

		return Game::FS_THREAD_MAIN;
	}

	std::vector<std::string> FileSystem::GetSysFileList(const std::string& path, const std::string& extension, bool folders)
	{
		std::vector<std::string> fileList;

		auto numFiles = 0;
		auto** files = Game::Sys_ListFiles(path.data(), extension.data(), nullptr, &numFiles, folders);

		if (files)
		{
			for (int i = 0; i < numFiles; ++i)
			{
				if (files[i])
				{
					fileList.emplace_back(files[i]);
				}
			}

			Game::Sys_FreeFileList(files);
		}

		return fileList;
	}

	std::filesystem::path FileSystem::GetAppdataPath()
	{
		PWSTR appdata = nullptr;

		if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata)))
		{
			return {};
		}

		const auto path = std::filesystem::path(appdata) / "iw4x";
		CoTaskMemFree(appdata);

		return path;
	}

	std::vector<std::string> FileSystem::GetFileList(const std::string& path, const std::string& extension)
	{
		std::vector<std::string> names;

		if (!Game::FS_ListFiles || !Game::Sys_FreeFileList)
		{
			return names;
		}

		int count = 0;
		char** const list = Game::FS_ListFiles(path.data(), extension.data(), listAll, &count, listTag);

		if (!list)
		{
			return names;
		}

		if (count > 0)
		{
			names.reserve(static_cast<std::size_t>(count));

			for (int index = 0; index < count; ++index)
			{
				if (list[index])
				{
					names.emplace_back(list[index]);
				}
			}
		}

		Game::Sys_FreeFileList(list);

		return names;
	}

	constexpr const wchar_t* cleanupStampName = L".zw3_cleanup";
	constexpr const wchar_t* cleanupWindowClass = L"ZW3CleanupProgress";

	class CleanupProgressDialog
	{
	public:
		explicit CleanupProgressDialog(int totalSteps)
		{
			if (totalSteps > 0)
			{
				this->total = totalSteps;
			}

			RegisterWindowClass();

			constexpr int width = 640;
			constexpr int height = 160;
			const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
			const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

			this->window = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_COMPOSITED, cleanupWindowClass, L"Running cleanup (please wait)...",
				WS_OVERLAPPED | WS_CAPTION, x, y, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

			if (!this->window)
			{
				return;
			}

			SendMessageW(this->window, WM_SETICON, ICON_SMALL, 0);
			SendMessageW(this->window, WM_SETICON, ICON_BIG, 0);

			NONCLIENTMETRICSW metrics{};
			metrics.cbSize = sizeof(metrics);

			if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
			{
				this->font = CreateFontIndirectW(&metrics.lfMessageFont);
			}

			this->progressLabel = this->CreateLabel(L"Completed:", 16, 18, 0);
			this->progressValue = this->CreateLabel(L"0% (Calculating...)", 34, 18, 0);
			this->fileLabel = this->CreateLabel(L"Current file:", 58, 18, 0);
			this->fileValue = this->CreateLabel(L"(initializing)", 76, 20, SS_PATHELLIPSIS);

			ShowWindow(this->window, SW_SHOW);
			UpdateWindow(this->window);

			this->startTime = std::chrono::steady_clock::now();
		}

		~CleanupProgressDialog()
		{
			this->Close();
		}

		void Update(const std::wstring& currentFile)
		{
			if (!this->window || !this->progressValue || !this->fileValue)
			{
				return;
			}

			const int percent = (this->current * 100) / this->total;
			const DWORD now = GetTickCount();

			if (percent == this->lastPercent && currentFile == this->lastFile && (now - this->lastUpdateTick) < 40)
			{
				PumpMessages();
				return;
			}

			++this->current;
			const int nextPercent = (this->current * 100) / this->total;

			std::wstring timeLeft = L"Calculating...";
			const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - this->startTime).count();

			if (this->current > 5 && elapsedMs > 100)
			{
				const double msPerFile = static_cast<double>(elapsedMs) / this->current;
				const long long remainingFiles = this->total - this->current;
				const long long secondsLeft = static_cast<long long>(msPerFile * remainingFiles) / 1000;

				if (secondsLeft < 1)
				{
					timeLeft = L"Less than a second left";
				}
				else if (secondsLeft < 60)
				{
					timeLeft = std::to_wstring(secondsLeft) + L"s left";
				}
				else
				{
					timeLeft = std::to_wstring(secondsLeft / 60) + L"m " + std::to_wstring(secondsLeft % 60) + L"s left";
				}
			}

			const std::wstring progressText = std::to_wstring(nextPercent) + L"% (" + timeLeft + L")";

			if (progressText == this->lastProgressText && currentFile == this->lastFile)
			{
				PumpMessages();
				return;
			}

			this->lastProgressText = progressText;
			this->lastFile = currentFile;
			this->lastPercent = nextPercent;
			this->lastUpdateTick = now;

			SetWindowTextW(this->progressValue, progressText.data());
			SetWindowTextW(this->fileValue, currentFile.data());
			PumpMessages();
		}

		void Close()
		{
			if (this->window)
			{
				DestroyWindow(this->window);
				this->window = nullptr;
				this->progressLabel = nullptr;
				this->progressValue = nullptr;
				this->fileLabel = nullptr;
				this->fileValue = nullptr;
			}

			if (this->font)
			{
				DeleteObject(this->font);
				this->font = nullptr;
			}
		}

	private:
		HWND window = nullptr;
		HWND progressLabel = nullptr;
		HWND progressValue = nullptr;
		HWND fileLabel = nullptr;
		HWND fileValue = nullptr;
		HFONT font = nullptr;
		int current = 0;
		int total = 1;
		int lastPercent = -1;
		DWORD lastUpdateTick = 0;
		std::wstring lastProgressText;
		std::wstring lastFile;
		std::chrono::steady_clock::time_point startTime;

		HWND CreateLabel(const wchar_t* text, int y, int height, DWORD style) const
		{
			const HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
				20, y, 600, height, this->window, nullptr, GetModuleHandleW(nullptr), nullptr);

			if (label && this->font)
			{
				SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(this->font), TRUE);
			}

			return label;
		}

		static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
		{
			if (message == WM_ERASEBKGND)
			{
				RECT rect{};
				GetClientRect(window, &rect);
				FillRect(reinterpret_cast<HDC>(wParam), &rect, GetSysColorBrush(COLOR_WINDOW));
				return 1;
			}

			if (message == WM_CTLCOLORSTATIC)
			{
				const auto deviceContext = reinterpret_cast<HDC>(wParam);
				SetBkMode(deviceContext, OPAQUE);
				SetBkColor(deviceContext, GetSysColor(COLOR_WINDOW));
				return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
			}

			return DefWindowProcW(window, message, wParam, lParam);
		}

		static void RegisterWindowClass()
		{
			static bool isRegistered = false;

			if (isRegistered)
			{
				return;
			}

			WNDCLASSW windowClass{};
			windowClass.lpfnWndProc = WindowProc;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.lpszClassName = cleanupWindowClass;
			windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
			RegisterClassW(&windowClass);
			isRegistered = true;
		}

		static void PumpMessages()
		{
			MSG message{};

			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
	};

	static void WriteCleanupStamp(const std::filesystem::path& stampPath, const std::vector<std::string>& details)
	{
		if (stampPath.empty())
		{
			return;
		}

		std::error_code error;
		std::filesystem::create_directories(stampPath.parent_path(), error);

		std::ofstream stamp(stampPath, std::ios::trunc);

		if (!stamp.is_open())
		{
			return;
		}

		stamp << "cleanup complete\n";

		for (const auto& detail : details)
		{
			stamp << detail << '\n';
		}
	}

	static std::string LowerFileName(const std::filesystem::path& path)
	{
		std::string name = path.filename().string();
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		return name;
	}

	[[noreturn]] static void ExitOnLockedPath(const char* reason, const char* kind, const std::filesystem::path& path)
	{
		MessageBoxA(nullptr, std::format("{}:\n{}\n\nPlease close any processes that are using this {} and restart the game.",
			reason, path.string(), kind).data(), "Error", MB_OK | MB_ICONERROR);
		std::exit(EXIT_FAILURE);
	}

	void FileSystem::CleanupZw3Files()
	{
		static std::once_flag once;

		std::call_once(once, []
		{
			std::vector<std::filesystem::path> roots;
			const std::filesystem::path basePath = Utils::GetBaseFilesLocation();

			if (!basePath.empty())
			{
				roots.push_back(basePath);
			}

			try
			{
				const auto currentPath = std::filesystem::current_path();

				if (roots.empty() || !std::filesystem::equivalent(roots.front(), currentPath))
				{
					roots.push_back(currentPath);
				}
			}
			catch (const std::exception&)
			{
			}

			std::filesystem::path stampPath;

			if (!roots.empty())
			{
				stampPath = roots.front() / L"zw3" / cleanupStampName;
			}

			std::error_code stampError;

			if (!stampPath.empty() && std::filesystem::exists(stampPath, stampError) && !stampError)
			{
				return;
			}

			struct CleanupTask
			{
				std::filesystem::path source;
				std::filesystem::path destination;
				bool isMove;
				bool isDirectory;
			};

			std::vector<CleanupTask> tasks;
			std::unordered_set<std::wstring> seenPaths;

			const auto addDeleteFile = [&](const std::filesystem::path& path)
			{
				std::error_code error;

				if (std::filesystem::is_regular_file(path, error) && seenPaths.insert(path.wstring()).second)
				{
					tasks.push_back({ path, {}, false, false });
				}
			};

			const auto addDeleteDirectory = [&](const std::filesystem::path& path)
			{
				std::error_code error;

				if (std::filesystem::is_directory(path, error) && seenPaths.insert(path.wstring()).second)
				{
					tasks.push_back({ path, {}, false, true });
				}
			};

			const auto addMove = [&](const std::filesystem::path& source, const std::filesystem::path& destination)
			{
				std::error_code error;

				if (std::filesystem::is_regular_file(source, error) && seenPaths.insert(source.wstring()).second)
				{
					tasks.push_back({ source, destination, true, false });
				}
			};

			const std::filesystem::path legacyFiles[] =
			{
				L"zw3.iwd",
				L"iw4x/zw3.iwd",
				L"main/zw3.iwd",
				L"userraw/zw3.iwd",
				L"zone/patch/patch_mp.ff",
				L"zone/english/zw3_common.ff",
				L"zone/patch_mp.ff",
			};

			const std::filesystem::path extractedFolders[] =
			{
				L"images",
				L"localizedstrings",
				L"maps",
				L"mp",
				L"scriptdata",
				L"scripts",
				L"sound",
				L"ui_mp",
				L"weapons",
				L"zw",
			};

			for (const auto& root : roots)
			{
				std::error_code error;

				if (!std::filesystem::exists(root, error))
				{
					continue;
				}

				for (const auto& relative : legacyFiles)
				{
					addDeleteFile(root / relative);
				}

				const auto zw3Folder = root / L"zw3";

				for (const auto& relative : extractedFolders)
				{
					addDeleteDirectory(zw3Folder / relative);
				}

				addDeleteFile(zw3Folder / L"bots.txt");

				const auto mainFolder = root / L"main";

				if (std::filesystem::is_directory(mainFolder, error))
				{
					for (const auto& entry : std::filesystem::directory_iterator(mainFolder, error))
					{
						if (error)
						{
							break;
						}

						if (entry.path().extension() == ".iwd" && LowerFileName(entry.path()).starts_with("mp_"))
						{
							addDeleteFile(entry.path());
						}
					}
				}

				const auto userrawScriptData = root / L"userraw" / L"scriptdata";
				const auto coreScriptData = zw3Folder / L"core" / L"scriptdata";

				if (std::filesystem::is_directory(userrawScriptData, error))
				{
					for (const auto& entry : std::filesystem::recursive_directory_iterator(userrawScriptData, error))
					{
						if (error)
						{
							break;
						}

						if (!std::filesystem::is_regular_file(entry.path(), error))
						{
							continue;
						}

						const std::string name = LowerFileName(entry.path());

						if (name.starts_with("autosave") || name.starts_with("rank_") || name.starts_with("easteregg"))
						{
							addMove(entry.path(), coreScriptData / entry.path().lexically_relative(userrawScriptData));
						}
					}
				}
			}

			if (tasks.empty())
			{
				WriteCleanupStamp(stampPath, { "no files to clear" });
				return;
			}

			CleanupProgressDialog progress(static_cast<int>(tasks.size()));
			int movedCount = 0;
			int deletedCount = 0;
			int skippedCount = 0;
			std::vector<std::string> processedFiles;

			for (const auto& task : tasks)
			{
				progress.Update(task.source.wstring());
				SetFileAttributesW(task.source.wstring().data(), FILE_ATTRIBUTE_NORMAL);

				std::error_code error;

				if (task.isDirectory)
				{
					if (std::filesystem::remove_all(task.source, error) > 0 && !error)
					{
						processedFiles.emplace_back(std::format("deleted directory: {}", task.source.string()));
						++deletedCount;
						continue;
					}

					if (!std::filesystem::exists(task.source, error))
					{
						processedFiles.emplace_back(std::format("skipped (not found): {}", task.source.string()));
						++skippedCount;
						continue;
					}

					progress.Close();
					ExitOnLockedPath("Directory cannot be deleted", "directory", task.source);
				}

				if (task.isMove)
				{
					std::filesystem::create_directories(task.destination.parent_path(), error);
					SetFileAttributesW(task.destination.wstring().data(), FILE_ATTRIBUTE_NORMAL);

					if (MoveFileExW(task.source.wstring().data(), task.destination.wstring().data(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
					{
						processedFiles.emplace_back(std::format("moved: {} -> {}", task.source.string(), task.destination.string()));
						++movedCount;
						continue;
					}
				}
				else if (DeleteFileW(task.source.wstring().data()))
				{
					processedFiles.emplace_back(std::format("deleted: {}", task.source.string()));
					++deletedCount;
					continue;
				}

				const DWORD lastError = GetLastError();

				if (lastError == ERROR_FILE_NOT_FOUND)
				{
					processedFiles.emplace_back(std::format("skipped (not found): {}", task.source.string()));
					++skippedCount;
					continue;
				}

				if (lastError == ERROR_SHARING_VIOLATION || lastError == ERROR_ACCESS_DENIED)
				{
					progress.Close();
					ExitOnLockedPath("File is in use and cannot be updated", "file", task.source);
				}

				if (!task.isMove && std::filesystem::remove(task.source, error))
				{
					processedFiles.emplace_back(std::format("deleted: {}", task.source.string()));
					++deletedCount;
					continue;
				}

				if (task.isMove)
				{
					processedFiles.emplace_back(std::format("skipped: {} -> {}", task.source.string(), task.destination.string()));
				}
				else
				{
					processedFiles.emplace_back(std::format("skipped: {}", task.source.string()));
				}

				++skippedCount;
			}

			progress.Close();

			std::vector<std::string> stampDetails;
			stampDetails.emplace_back(std::format("files checked: {}", tasks.size()));
			stampDetails.emplace_back(std::format("moved: {}", movedCount));
			stampDetails.emplace_back(std::format("deleted: {}", deletedCount));
			stampDetails.emplace_back(std::format("skipped: {}", skippedCount));
			stampDetails.emplace_back("processed files:");
			stampDetails.insert(stampDetails.end(), processedFiles.begin(), processedFiles.end());
			WriteCleanupStamp(stampPath, stampDetails);

			MessageBoxA(nullptr, std::format("Cleanup completed successfully.\n\nFiles checked: {}\nMoved: {}\nDeleted: {}\nSkipped: {}",
				tasks.size(), movedCount, deletedCount, skippedCount).data(), "Done!", MB_OK | MB_ICONINFORMATION);
		});
	}

	static bool HasIwd(const std::filesystem::path& folder)
	{
		std::error_code error;

		for (std::filesystem::directory_iterator file(folder, error), end; !error && file != end; file.increment(error))
		{
			if (file->path().extension() == ".iwd")
			{
				return true;
			}
		}

		return false;
	}

	static const char* DataFolder()
	{
		const std::filesystem::path basepath = (*Game::fs_basepath)->current.string;

		for (const char* folder : { BASEGAME, "main/zw3/x86", "main/iw4x/x86" })
		{
			if (HasIwd(basepath / folder))
			{
				return folder;
			}
		}

		return BASEGAME;
	}

	void FileSystem::FS_RegisterDvars_Stub()
	{
		reinterpret_cast<void(*)()>(registerDvarsHook.GetOriginal())();

		Game::dvar_t* const basegame = Game::Dvar_FindVar("fs_basegame");

		if (!basegame)
		{
			Logger::Error("filesystem: fs_basegame was not registered, " BASEGAME " will not be searched\n");
			return;
		}

		Game::Dvar_SetString(basegame, DataFolder());
	}

	int FileSystem::Cmd_Exec_f_Stub(const char* name, [[maybe_unused]] const char* configName)
	{
		std::int64_t file = 0;

		const int length = reinterpret_cast<int(*)(const char*, std::int64_t*, int)>(
			Utils::Hook::Rebase(FS_FOpenFileByMode))(name, &file, fsRead);

		if (length < 0)
		{
			return 1;
		}

		reinterpret_cast<void(*)(std::int64_t)>(Utils::Hook::Rebase(FS_FCloseFile))(file);
		return 0;
	}

	char* FileSystem::Com_GetFilenameSubString_Stub(char* path)
	{
		return path;
	}

	void FileSystem::RegisterFolder(const char* folder)
	{
		const auto addGameDirectory = reinterpret_cast<void(*)(const char*, const char*)>(
			Utils::Hook::Rebase(FS_AddLocalizedGameDirectory));

		std::string name = folder;
		std::ranges::replace(name, '\\', '/');

		std::vector<std::string> addedPaths;

		for (const std::uintptr_t pathDvar : { fs_cdpath, fs_basepath, fs_homepath })
		{
			const auto* const dvar = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(pathDvar));
			const std::string path = dvar->current.string;

			if (path.empty() || std::ranges::find(addedPaths, path) != addedPaths.end())
			{
				continue;
			}

			addGameDirectory(path.data(), name.data());
			addedPaths.push_back(path);
		}
	}

	Game::dvar_t* FileSystem::FS_Startup_Stub()
	{
		if (ZoneBuilder::IsEnabled())
		{
			RegisterFolder("zonedata");
		}

		RegisterFolder("userraw");

		if (Flags::HasFlag("dev"))
		{
			Logger::Print("Skipping ZW3 filesystem folders because -dev is enabled.\n");
		}
		else
		{
			const auto* const basepath = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(fs_basepath));

			if (basepath->current.string && basepath->current.string[0] != '\0')
			{
				const auto zw3Folder = std::filesystem::path(basepath->current.string) / "zw3";
				std::error_code error;

				if (!std::filesystem::exists(zw3Folder, error))
				{
					MessageBoxA(nullptr, std::format("Missing 'zw3' folder:\n{}\n\nPlease run the Zombie Warfare 3 Launcher to verify game files.",
						zw3Folder.string()).data(), "Error", MB_OK | MB_ICONERROR);
					std::exit(EXIT_FAILURE);
				}

				RegisterFolder("zw3");
				RegisterFolder("zw3\\data");
				RegisterFolder("zw3\\core");
				RegisterFolder("zw3\\core\\scriptdata");
			}
		}

		return *reinterpret_cast<Game::dvar_t**>(Utils::Hook::Rebase(fs_gameDirVar));
	}

	void FileSystem::FS_AddLocalizedGameDirectory_Stub(const char* path, const char* folder)
	{
		const auto addGameDirectory = reinterpret_cast<void(*)(const char*, const char*, int, int)>(
			Utils::Hook::Rebase(FS_AddGameDirectory));
		const auto* const language = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(loc_language));

		int languageIndex = 0;

		if (language)
		{
			languageIndex = language->current.integer;
		}

		addGameDirectory(path, folder, 1, languageIndex);
		addGameDirectory(path, folder, 0, 0);
	}

	void FileSystem::HookLocalizedDirectories()
	{
		if (!MatchesBytes(FS_AddLocalizedGameDirectory, addLocalizedEntry, sizeof(addLocalizedEntry))
			|| !addLocalizedHook.Initialize(FS_AddLocalizedGameDirectory, reinterpret_cast<void*>(FS_AddLocalizedGameDirectory_Stub), HOOK_JUMP)
				->Install()->IsInstalled())
		{
			Logger::Error("filesystem: FS_AddLocalizedGameDirectory does not read as expected, every language is still searched\n");
			return;
		}

		addLocalizedHook.Quick();
	}

	void FileSystem::HookConfigExec()
	{
		if (!MatchesBytes(Cmd_Exec_f_SubStringCall - 5, execSubString, sizeof(execSubString))
			|| !MatchesBytes(Cmd_Exec_f_CompareCall - 3, execCompare, sizeof(execCompare)))
		{
			Logger::Error("filesystem: Cmd_Exec_f does not read as expected, only config_mp.cfg execs from disk\n");
			return;
		}

		const bool isSeated = execSubStringHook.Initialize(Cmd_Exec_f_SubStringCall,
			reinterpret_cast<void*>(Com_GetFilenameSubString_Stub), HOOK_CALL)->Install()->IsInstalled()
			&& execCompareHook.Initialize(Cmd_Exec_f_CompareCall,
				reinterpret_cast<void*>(Cmd_Exec_f_Stub), HOOK_CALL)->Install()->IsInstalled();

		if (!isSeated)
		{
			execSubStringHook.Uninstall();
			execCompareHook.Uninstall();

			Logger::Error("filesystem: could not hook Cmd_Exec_f, only config_mp.cfg execs from disk\n");
			return;
		}

		execSubStringHook.Quick();
		execCompareHook.Quick();
	}

	void FileSystem::HookStartupFolders()
	{
		if (!MatchesBytes(FS_Startup_GameDirLoad, gameDirLoad, sizeof(gameDirLoad)))
		{
			Logger::Error("filesystem: FS_Startup does not read as expected, userraw will not be searched\n");
			return;
		}

		if (!startupHook.Initialize(FS_Startup_GameDirLoad, reinterpret_cast<void*>(FS_Startup_Stub), HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("filesystem: could not hook FS_Startup, userraw will not be searched\n");
			return;
		}

		startupHook.Quick();
		Utils::Hook::Nop(FS_Startup_GameDirLoad + 5, 2);
	}

	void FileSystem::AllowAnyIwdName()
	{
		static const std::uint8_t collectReject[] = { 0x74, 0x3D };
		static const std::uint8_t gate[] = { 0x41, 0xB8, 0x03, 0x00, 0x00, 0x00 };

		if (!MatchesBytes(iwdNameCollectReject, collectReject, sizeof(collectReject))
			|| !MatchesBytes(iwdNameGate, gate, sizeof(gate)))
		{
			Logger::Error("filesystem: the iwd name checks are not where they were mapped, "
				"only iw_NN.iwd will load\n");
			return;
		}

		Utils::Hook::Nop(iwdNameCollectReject, sizeof(collectReject));

		const auto relative = static_cast<std::int32_t>(iwdNameGateAccept - (iwdNameGate + 5));

		Utils::Hook::Set<std::uint8_t>(iwdNameGate, 0xE9);
		Utils::Hook::Set<std::int32_t>(iwdNameGate + 1, relative);
		Utils::Hook::Nop(iwdNameGate + 5, 1);
	}

	FileSystem::FileSystem()
	{
		AllowAnyIwdName();

		HookConfigExec();

		if (MatchesBytes(Cmd_ExecuteSingleCommandFromDisk_Match, diskCommandMatch, sizeof(diskCommandMatch)))
		{
			Utils::Hook::Set<std::uint8_t>(Cmd_ExecuteSingleCommandFromDisk_MatchJz, 0xEB);
		}
		else
		{
			Logger::Error("filesystem: Cmd_ExecuteSingleCommandFromDisk does not read as expected, disk configs only bind and seta\n");
		}

		HookStartupFolders();

		HookLocalizedDirectories();

		HookSync();

		if (!Utils::Hook::BranchesTo(Load_GfxTextureLoad_LoadTextureCall, Load_Texture, HOOK_CALL)
			|| !loadTextureHook.Initialize(Load_GfxTextureLoad_LoadTextureCall, reinterpret_cast<void*>(LoadTextureSync), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("filesystem: Load_GfxTextureLoad does not read as expected, zone textures are not staged\n");
		}
		else
		{
			loadTextureHook.Quick();
		}

		RaiseFileListLimit();

		if (!Utils::Hook::BranchesTo(CL_Vid_Restart_f_UpdateLanguageCall, SEH_UpdateLanguageInfo, HOOK_CALL)
			|| !vidRestartHook.Initialize(CL_Vid_Restart_f_UpdateLanguageCall, reinterpret_cast<void*>(CL_Vid_Restart_f_UpdateLanguage_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("filesystem: CL_Vid_Restart_f does not read as expected, a vid_restart leaves the search path alone\n");
		}
		else
		{
			vidRestartHook.Quick();
		}

		if (MatchesBytes(FS_AddIwdFilesForGameDirectory_NoIwd, noIwdError, sizeof(noIwdError)))
		{
			Utils::Hook::Nop(FS_AddIwdFilesForGameDirectory_NoIwdError, 5);
		}
		else
		{
			Logger::Error("filesystem: FS_AddIwdFilesForGameDirectory does not read as expected, an empty main is still fatal\n");
		}

		if (MatchesBytes(FS_AddIwdFilesForGameDirectory_PermanentChecksums, permanentChecksumsCount, sizeof(permanentChecksumsCount))
			&& MatchesBytes(FS_AddIwdFilesForGameDirectory_LinkSearchPath, linkSearchPath, sizeof(linkSearchPath)))
		{
			const auto relative = static_cast<std::int32_t>(FS_AddIwdFilesForGameDirectory_LinkSearchPath - (FS_AddIwdFilesForGameDirectory_PermanentChecksums + 5));

			Utils::Hook::Set<std::int32_t>(FS_AddIwdFilesForGameDirectory_PermanentChecksums + 1, relative);
			Utils::Hook::Set<std::uint8_t>(FS_AddIwdFilesForGameDirectory_PermanentChecksums + 5, 0x90);
			Utils::Hook::Set<std::uint8_t>(FS_AddIwdFilesForGameDirectory_PermanentChecksums, 0xE9);
		}
		else
		{
			Logger::Error("filesystem: FS_AddIwdFilesForGameDirectory's iwd checksum list does not read as expected, a changed iwd or a 65th iwd is still fatal\n");
		}

		if (MatchesBytes(Hunk_FreeTempMemory_MagicTest, badMagicError, sizeof(badMagicError))
			&& MatchesBytes(Hunk_FreeTempMemory_Epilogue, tempMemoryEpilogue, sizeof(tempMemoryEpilogue)))
		{
			const auto relative = static_cast<std::int8_t>(Hunk_FreeTempMemory_Epilogue - (Hunk_FreeTempMemory_BadMagic + 2));

			Utils::Hook::Set<std::int8_t>(Hunk_FreeTempMemory_BadMagic + 1, relative);
			Utils::Hook::Set<std::uint8_t>(Hunk_FreeTempMemory_BadMagic, 0xEB);
		}
		else
		{
			Logger::Error("filesystem: Hunk_FreeTempMemory does not read as expected, a bad magic is still fatal\n");
		}

		if (!registerDvarsHook.Initialize(FS_RegisterDvarsCall, FS_RegisterDvars_Stub, HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("filesystem: could not hook FS_RegisterDvars, " BASEGAME " will not be searched\n");
			return;
		}

		registerDvarsHook.Quick();
	}
}
