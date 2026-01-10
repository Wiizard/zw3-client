#include "FileSystem.hpp"
#include <cstdlib>
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace Components
{
	// patch max file amount returned by Sys_ListFiles
	constexpr auto FILE_COUNT_MULTIPLIER = 8;
	constexpr auto NEW_MAX_FILES_LISTED = 8191 * FILE_COUNT_MULTIPLIER;

	std::mutex FileSystem::Mutex;
	std::recursive_mutex FileSystem::FSMutex;
	Utils::Memory::Allocator FileSystem::MemAllocator;

	class CleanupProgressDialog
	{
	public:
		CleanupProgressDialog(int totalSteps)
			: total_(totalSteps > 0 ? totalSteps : 1)
		{
			EnsureClass();
			constexpr int width = 560;
			constexpr int height = 260;
			const auto posX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
			const auto posY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
			INITCOMMONCONTROLSEX icc{};
			icc.dwSize = sizeof(icc);
			icc.dwICC = ICC_PROGRESS_CLASS;
			InitCommonControlsEx(&icc);
			hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_COMPOSITED, kClassName, L"Running cleanup (Please wait)...",
				WS_OVERLAPPED | WS_CAPTION,
				posX, posY, width, height,
				nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

			if (hwnd_)
			{
				SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, 0);
				SendMessageW(hwnd_, WM_SETICON, ICON_BIG, 0);
				label_ = CreateWindowExW(0, L"STATIC", L"Checking files:",
					WS_CHILD | WS_VISIBLE,
					20, 16, 520, 18,
					hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
				detailsValue_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
					L"",
					WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
					20, 38, 520, 140,
					hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
				progressBar_ = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
					WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
					20, 188, 420, 18,
					hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
				SendMessageW(progressBar_, PBM_SETSTATE, PBST_NORMAL, 0);
				SendMessageW(progressBar_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
				SendMessageW(progressBar_, PBM_SETPOS, 0, 0);
				SendMessageW(progressBar_, PBM_SETMARQUEE, TRUE, 30);
				progressValue_ = CreateWindowExW(0, L"STATIC", L"0% (0/0)",
					WS_CHILD | WS_VISIBLE,
					450, 188, 90, 18,
					hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
				ApplyFont(label_);
				ApplyFont(progressValue_);
				ApplyFont(progressBar_);
				ApplyFont(detailsValue_);
				ShowWindow(hwnd_, SW_SHOW);
				UpdateWindow(hwnd_);
			}
		}

		void Update(const std::wstring& currentFile, int percent, int index, int totalFiles)
		{
			if (!hwnd_ || !label_ || !progressValue_ || !progressBar_ || !detailsValue_)
			{
				return;
			}

			std::wstring percentText = std::to_wstring(percent) + L"% (" + std::to_wstring(index)
				+ L"/" + std::to_wstring(totalFiles) + L")";
			if (percentText == lastLabel_ && currentFile == lastFile_)
			{
				PumpMessages();
				return;
			}

			lastLabel_ = percentText;
			lastFile_ = currentFile;
			lastPercent_ = percent;
			SetWindowTextW(progressValue_, percentText.c_str());
			SendMessageW(progressBar_, PBM_SETPOS, percent, 0);
			const auto line = currentFile + L"\r\n";
			const auto length = GetWindowTextLengthW(detailsValue_);
			const auto nextLength = length + static_cast<int>(line.size());
			constexpr int kMaxLength = 8192;
			if (nextLength > kMaxLength)
			{
				SendMessageW(detailsValue_, EM_SETSEL, 0, length);
				SendMessageW(detailsValue_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
				SendMessageW(detailsValue_, EM_SETSEL, 0, 0);
			}

			SendMessageW(detailsValue_, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
			SendMessageW(detailsValue_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
			SendMessageW(detailsValue_, EM_LINESCROLL, 0, 0x7FFFFFFF);
			SendMessageW(detailsValue_, EM_SCROLLCARET, 0, 0);
			PumpMessages();
			Sleep(30);
		}

		void SetComplete(int totalFiles)
		{
			if (!hwnd_ || !progressValue_)
			{
				return;
			}

			std::wstring percentText = L"100% (" + std::to_wstring(totalFiles) + L"/"
				+ std::to_wstring(totalFiles) + L")";
			SetWindowTextW(progressValue_, percentText.c_str());
			SendMessageW(progressBar_, PBM_SETPOS, 100, 0);
			PumpMessages();
		}

		void Close()
		{
			if (hwnd_)
			{
				DestroyWindow(hwnd_);
				hwnd_ = nullptr;
				label_ = nullptr;
				progressValue_ = nullptr;
				progressBar_ = nullptr;
				detailsValue_ = nullptr;
			}
		}

	private:
		static constexpr const wchar_t* kClassName = L"ZW3CleanupProgress";
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			if (msg == WM_ERASEBKGND)
			{
				const auto hdc = reinterpret_cast<HDC>(wParam);
				RECT rect{};
				GetClientRect(hwnd, &rect);
				FillRect(hdc, &rect, GetSysColorBrush(COLOR_WINDOW));
				return 1;
			}

			if (msg == WM_CTLCOLORSTATIC)
			{
				const auto hdc = reinterpret_cast<HDC>(wParam);
				SetBkMode(hdc, OPAQUE);
				SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
				return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
			}

			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		static void EnsureClass()
		{
			static bool registered = false;
			if (registered)
			{
				return;
			}

			WNDCLASSW wc{};
			wc.lpfnWndProc = WndProc;
			wc.hInstance = GetModuleHandleW(nullptr);
			wc.lpszClassName = kClassName;
			wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
			RegisterClassW(&wc);
			registered = true;
		}

		static void ApplyFont(HWND target)
		{
			if (!target)
			{
				return;
			}

			NONCLIENTMETRICSW metrics{};
			metrics.cbSize = sizeof(metrics);
			if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
			{
				HFONT font = CreateFontIndirectW(&metrics.lfMessageFont);
				SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
			}
		}

		static void PumpMessages()
		{
			MSG msg{};
			while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		HWND hwnd_ = nullptr;
		HWND label_ = nullptr;
		HWND progressValue_ = nullptr;
		HWND progressBar_ = nullptr;
		HWND detailsValue_ = nullptr;
		int total_ = 1;
		int lastPercent_ = -1;
		std::wstring lastLabel_;
		std::wstring lastFile_;
	};

	void FileSystem::CleanupZw3Files()
	{
		static std::once_flag once;
		std::call_once(once, []()
			{
				std::vector<std::filesystem::path> roots;
				if (const auto basePath = Utils::GetBaseFilesLocation(); !basePath.empty())
				{
					roots.push_back(basePath);
				}

				try
				{
					roots.push_back(std::filesystem::current_path());
				}
				catch (const std::exception&)
				{
				}

				std::unordered_set<std::wstring> legacySet;
				std::vector<std::filesystem::path> allFiles;
				const auto shouldDelete = [&](const std::filesystem::path& path, const std::filesystem::path& root) -> bool
					{
						const auto relative = path.lexically_relative(root).generic_string();
						std::string relativeLower = relative;
						std::transform(relativeLower.begin(), relativeLower.end(), relativeLower.begin(),
							[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
						const auto filename = path.filename().string();
						std::string filenameLower = filename;
						std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(),
							[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

						if (relativeLower == "zw3/zw3.iwd"
							|| relativeLower == "zw3/zone/patch/patch_mp.ff"
							|| relativeLower == "zw3/zone/english/zw3_common.ff")
						{
							return false;
						}

						if (relativeLower.size() >= 8 && relativeLower.rfind("zw3/", 0) == 0)
						{
							return false;
						}

						if (relativeLower == "iw4x/zw3.iwd"
							|| relativeLower == "zw3.iwd"
							|| relativeLower == "main/zw3.iwd"
							|| relativeLower == "zone/patch/patch_mp.ff")
						{
							return true;
						}

						if (path.extension() == ".iwd"
							&& filenameLower.rfind("mp_", 0) == 0
							&& path.parent_path().filename() == "main")
						{
							return true;
						}

						return false;
					};
				const auto collectLegacyInRoot = [&](const char* root)
					{
						if (root == nullptr || root[0] == '\0')
						{
							return;
						}

						std::error_code ec;
						const auto baseDir = std::filesystem::path(root);
						if (!std::filesystem::exists(baseDir, ec))
						{
							return;
						}

						for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir, ec))
						{
							if (ec)
							{
								break;
							}

							if (!entry.is_regular_file(ec))
							{
								ec.clear();
								continue;
							}

							const auto& path = entry.path();
							allFiles.push_back(path);
							if (shouldDelete(path, baseDir))
							{
								legacySet.insert(path.wstring());
							}
						}
					};

				for (const auto& root : roots)
				{
					collectLegacyInRoot(root.string().c_str());
				}

				if (legacySet.empty())
				{
					return;
				}

				CleanupProgressDialog progress(static_cast<int>(allFiles.size()));
				int deletedCount = 0;
				int skippedCount = 0;
				const int totalFiles = static_cast<int>(allFiles.size());
				int index = 0;
				for (const auto& path : allFiles)
				{
					++index;
					const auto percent = totalFiles > 0 ? (index * 100) / totalFiles : 100;
					if (index == 1 || index == totalFiles || index % 25 == 0)
					{
						progress.Update(path.wstring(), percent, index, totalFiles);
						Sleep(1);
					}

					std::error_code ec;
					if (!std::filesystem::exists(path, ec))
					{
						++skippedCount;
						continue;
					}

					if (legacySet.find(path.wstring()) == legacySet.end())
					{
						++skippedCount;
						continue;
					}

					const auto widePath = path.wstring();
					SetFileAttributesW(widePath.c_str(), FILE_ATTRIBUTE_NORMAL);

					ec.clear();
					if (DeleteFileW(widePath.c_str()) != 0)
					{
						++deletedCount;
						continue;
					}

					const auto winError = GetLastError();
					if (winError == ERROR_FILE_NOT_FOUND)
					{
						++skippedCount;
						continue;
					}

					if (winError == ERROR_SHARING_VIOLATION || winError == ERROR_ACCESS_DENIED)
					{
						progress.Close();
						MessageBoxA(nullptr,
							Utils::String::Format("File is in use and cannot be removed:\n{}\n\nPlease close any processes that are using this file and restart the game.",
								path.string().c_str()),
							"Error",
							MB_OK | MB_ICONERROR);
						std::exit(EXIT_FAILURE);
					}

					const auto removed = std::filesystem::remove(path, ec);
					if (!removed || ec)
					{
						progress.Close();
						MessageBoxA(nullptr,
							Utils::String::Format("Failed to remove file:\n{}\n\n{}",
								path.string().c_str(),
								ec ? ec.message().c_str() : "unknown"),
							"Error",
							MB_OK | MB_ICONERROR);
						std::exit(EXIT_FAILURE);
					}
					++deletedCount;
				}

				progress.Close();
				MessageBoxA(nullptr,
					Utils::String::Format("Cleanup complete.\n\nFiles checked: {}\nDeleted: {}\nSkipped: {}",
						static_cast<int>(allFiles.size()),
						deletedCount,
						skippedCount),
					"Done!",
					MB_OK | MB_ICONINFORMATION);
			});
	}

	void FileSystem::File::read(Game::FsThread thread)
	{
		std::lock_guard _(FSMutex);

		assert(!filePath.empty());

		int handle;
		const auto len = Game::FS_FOpenFileReadForThread(filePath.data(), &handle, thread);

		if (!handle)
		{
			return;
		}

		auto* buf = AllocateFile(len + 1);

		[[maybe_unused]] auto bytesRead = Game::FS_Read(buf, len, handle);

		assert(bytesRead == len);

		buf[len] = '\0';

		Game::FS_FCloseFile(handle);

		this->buffer.append(buf, len);
		FreeFile(buf);
	}

	void FileSystem::RawFile::read()
	{
		this->buffer.clear();

		auto* rawfile = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_RAWFILE, this->filePath.data()).rawfile;
		if (!rawfile || Game::DB_IsXAssetDefault(Game::XAssetType::ASSET_TYPE_RAWFILE, this->filePath.data())) return;

		this->buffer.resize(Game::DB_GetRawFileLen(rawfile));
		Game::DB_GetRawBuffer(rawfile, this->buffer.data(), static_cast<int>(this->buffer.size()));
	}

	FileSystem::FileReader::FileReader(std::string file) : handle(0), name(std::move(file))
	{
		this->size = Game::FS_FOpenFileReadCurrentThread(this->name.data(), &this->handle);
	}

	FileSystem::FileReader::~FileReader()
	{
		if (this->exists() && this->handle)
		{
			Game::FS_FCloseFile(this->handle);
		}
	}

	bool FileSystem::FileReader::exists() const noexcept
	{
		return (this->size >= 0 && this->handle);
	}

	std::string FileSystem::FileReader::getName() const
	{
		return this->name;
	}

	int FileSystem::FileReader::getSize() const noexcept
	{
		return this->size;
	}

	std::string FileSystem::FileReader::getBuffer() const
	{
		Utils::Memory::Allocator allocator;
		if (!this->exists()) return {};

		const auto position = Game::FS_FTell(this->handle);
		this->seek(0, Game::FS_SEEK_SET);

		char* buffer = allocator.allocateArray<char>(this->size);
		if (!this->read(buffer, this->size))
		{
			this->seek(position, Game::FS_SEEK_SET);
			return {};
		}

		this->seek(position, Game::FS_SEEK_SET);

		return {buffer, static_cast<std::size_t>(this->size)};
	}

	bool FileSystem::FileReader::read(void* buffer, std::size_t _size) const noexcept
	{
		if (!this->exists() || static_cast<std::size_t>(this->size) < _size || Game::FS_Read(buffer, static_cast<int>(_size), this->handle) != static_cast<int>(_size))
		{
			return false;
		}

		return true;
	}

	void FileSystem::FileReader::seek(int offset, int origin) const
	{
		if (this->exists())
		{
			Game::FS_Seek(this->handle, offset, origin);
		}
	}

	void FileSystem::FileWriter::write(const std::string& data) const
	{
		if (this->handle)
		{
			Game::FS_Write(data.data(), static_cast<int>(data.size()), this->handle);
		}
	}

	void FileSystem::FileWriter::open(bool append)
	{
		if (append)
		{
			this->handle = Game::FS_FOpenFileAppend(this->filePath.data());
		}
		else
		{
			this->handle = Game::FS_FOpenFileWrite(this->filePath.data());
		}
	}

	void FileSystem::FileWriter::close()
	{
		if (this->handle)
		{
			Game::FS_FCloseFile(this->handle);
			this->handle = 0;
		}
	}

	std::filesystem::path FileSystem::GetAppdataPath()
	{
		PWSTR path;
		if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
		{
			throw std::runtime_error("Failed to read APPDATA path!");
		}

		auto _0 = gsl::finally([&path]
		{
			CoTaskMemFree(path);
		});

		return std::filesystem::path(path) / "iw4x";
	}

	std::vector<std::string> FileSystem::GetFileList(const std::string& path, const std::string& extension,  Game::FsListBehavior_e behaviour)
	{
		std::vector<std::string> fileList;

		auto numFiles = 0;
		const auto** files = Game::FS_ListFiles(path.data(), extension.data(), behaviour, &numFiles);

		if (files)
		{
			for (int i = 0; i < numFiles; ++i)
			{
				if (files[i])
				{
					fileList.emplace_back(files[i]);
				}
			}

			Game::FS_FreeFileList(files, 10);
		}

		return fileList;
	}

	std::vector<std::string> FileSystem::GetSysFileList(const std::string& path, const std::string& extension, bool folders)
	{
		std::vector<std::string> fileList;

		auto numFiles = 0;
		const auto** files = Game::Sys_ListFiles(path.data(), extension.data(), nullptr, &numFiles, folders);

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

	bool FileSystem::_DeleteFile(const std::string& folder, const std::string& file)
	{
		char path[MAX_PATH]{};
		Game::FS_BuildPathToFile((*Game::fs_basepath)->current.string, reinterpret_cast<char*>(0x63D0BB8), Utils::String::VA("%s/%s", folder.data(), file.data()), reinterpret_cast<char**>(&path));
		return Game::FS_Remove(path);
	}

	int FileSystem::ReadFile(const char* path, char** buffer)
	{
		if (!buffer) return -1;
		if (!path) return -1;

		std::lock_guard _(Mutex);
		FileReader reader(path);

		int size = reader.getSize();
		if (reader.exists() && size >= 0)
		{
			*buffer = AllocateFile(size + 1);
			if (reader.read(*buffer, size)) return size;

			FreeFile(*buffer);
			*buffer = nullptr;
		}

		return -1;
	}

	char* FileSystem::AllocateFile(int size)
	{
		return MemAllocator.allocateArray<char>(size);
	}

	void FileSystem::FreeFile(void* buffer)
	{
		MemAllocator.free(buffer);
	}

	void FileSystem::RegisterFolder(const char* folder)
	{
		const std::string fs_cdpath = (*Game::fs_cdpath)->current.string;
		const std::string fs_basepath = (*Game::fs_basepath)->current.string;
		const std::string fs_homepath = (*Game::fs_homepath)->current.string;

		if (!fs_cdpath.empty())   Game::FS_AddLocalizedGameDirectory(fs_cdpath.data(),   folder);
		if (!fs_basepath.empty()) Game::FS_AddLocalizedGameDirectory(fs_basepath.data(), folder);
		if (!fs_homepath.empty()) Game::FS_AddLocalizedGameDirectory(fs_homepath.data(), folder);
	}

	void FileSystem::RegisterFolders()
	{
		if (ZoneBuilder::IsEnabled())
		{
			RegisterFolder("zonedata");
		}

		RegisterFolder("userraw");

		const auto basepath = (*Game::fs_basepath)->current.string;
		if (basepath && basepath[0] != '\0')
		{
			std::error_code ec;
			const auto zw3Dir = std::filesystem::path(basepath) / "zw3";
			const auto zw3Iwd = zw3Dir / "zw3.iwd";
			if (!std::filesystem::exists(zw3Dir, ec))
			{
				MessageBoxA(nullptr,
					Utils::String::Format("Missing 'zw3' folder:\n{}\n\nPlease run the Zombie Warfare 3 Launcher to verify game files.", zw3Dir.string().c_str()),
					"Error",
					MB_OK | MB_ICONERROR);
				std::exit(EXIT_FAILURE);
			}
			else if (!std::filesystem::exists(zw3Iwd, ec))
			{
				MessageBoxA(nullptr,
					Utils::String::Format("Missing 'zw3.iwd':\n{}\n\nPlease run the Zombie Warfare 3 Launcher to verify game files or place it inside the zw3 folder.", zw3Iwd.string().c_str()),
					"Error",
					MB_OK | MB_ICONERROR);
				std::exit(EXIT_FAILURE);
			}
			else
			{
				RegisterFolder("zw3");
			}
		}
	}

	__declspec(naked) void FileSystem::StartupStub()
	{
		__asm
		{
			pushad
			push esi
			call FileSystem::RegisterFolders
			pop esi
			popad

			mov edx, ds:63D0CC0h

			push 48264Dh
			retn
		}
	}

	int FileSystem::Cmd_Exec_f_Stub(const char* s0, [[maybe_unused]] const char* s1)
	{
		int f;
		const auto len = Game::FS_FOpenFileByMode(s0, &f, Game::FS_READ);
		if (len < 0)
		{
			return 1; // Not found
		}

		Game::FS_FCloseFile(f);
		return 0; // Found
	}

	void FileSystem::FsStartupSync(const char* a1)
	{
		std::lock_guard _(FSMutex);
		return Utils::Hook::Call<void(const char*)>(0x4823A0)(a1); // FS_Startup
	}

	void FileSystem::FsRestartSync(int localClientNum, int checksumFeed)
	{
		std::lock_guard _(FSMutex);
		Maps::GetUserMap()->freeIwd();
		Utils::Hook::Call<void(int, int)>(0x461A50)(localClientNum, checksumFeed); // FS_Restart
		Maps::GetUserMap()->reloadIwd();
	}

	void FileSystem::FsShutdownSync(int closemfp)
	{
		std::lock_guard _(FSMutex);
		Maps::GetUserMap()->freeIwd();
		Utils::Hook::Call<void(int)>(0x4A46C0)(closemfp); // FS_Shutdown
	}

	void FileSystem::DelayLoadImagesSync()
	{
		std::lock_guard _(FSMutex);
		return Utils::Hook::Call<void()>(0x494060)(); // DB_LoadDelayedImages
	}

	int FileSystem::LoadTextureSync(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image)
	{
		std::lock_guard _(FSMutex);
		return Game::Load_Texture(loadDef, image);
	}

	void FileSystem::IwdFreeStub(Game::iwd_t* iwd)
	{
		Maps::GetUserMap()->handlePackfile(iwd);
		Utils::Hook::Call<void(void*)>(0x4291A0)(iwd);
	}

	const char* FileSystem::Sys_DefaultInstallPath_Hk()
	{
		static auto current_path = std::filesystem::current_path().string();
		return current_path.data();
	}

	FILE* FileSystem::FS_FileOpenReadText_Hk(const char* file)
	{
		const auto path = Utils::GetBaseFilesLocation();
		if (!path.empty() && Utils::IO::FileExists((path / file).string()))
		{
			return Game::FS_FileOpenReadText((path / file).string().data());
		}

		return Game::FS_FileOpenReadText(file);
	}

	const char* FileSystem::Sys_DefaultCDPath_Hk()
	{
		return Sys_DefaultInstallPath_Hk();
	}

	const char* FileSystem::Sys_HomePath_Hk()
	{
		const auto path = Utils::GetBaseFilesLocation();
		if (!path.empty())
		{
			static auto current_path = path.string();
			return current_path.data();
		}

		return "";
	}

	const char* FileSystem::Sys_Cwd_Hk()
	{
		return Sys_DefaultInstallPath_Hk();
	}

	Game::HunkUser* Hunk_UserCreate_Stub(int maxSize, const char* name, bool fixed, int type)
	{
		maxSize *= FILE_COUNT_MULTIPLIER;
		return Utils::Hook::Call<Game::HunkUser*(int, const char*, bool, int)>(0x430E90)(maxSize, name, fixed, type);
	}

	bool FileSystem::FileWrapper_Rotate(const char* ospath)
	{
		constexpr auto MAX_BACKUPS = 20;

		std::string renamedPath;

		std::optional<int> oldestIndex;
		auto currentIndex = 0;
		std::filesystem::file_time_type oldestime{};

		// Check if the original file exists
		if (!Utils::IO::FileExists(ospath))
		{
			return true; // Return true if the file does not exist (no file to rotate)
		}

		for (; currentIndex < MAX_BACKUPS; ++currentIndex)
		{
			renamedPath = std::format("{0}.{1:03}", ospath, currentIndex);

			if (!Utils::IO::FileExists(renamedPath))
			{
				break; // Stop if an available slot is found
			}

			auto time = std::filesystem::last_write_time(renamedPath);
			if (!oldestIndex.has_value() || time < oldestime)
			{
				oldestime = time;
				oldestIndex = currentIndex;
			}
		}

		if (currentIndex == MAX_BACKUPS)
		{
			renamedPath = std::format("{0}.{1:03}", ospath, *oldestIndex);
			Utils::IO::RemoveFile(renamedPath); // Remove the oldest backup file
		}
		else
		{
			renamedPath = std::format("{0}.{1:03}", ospath, currentIndex);
		}

		// Rename the original file to the selected backup slot
		std::error_code ec;
		std::filesystem::rename(ospath, renamedPath, ec);

		return !ec;
	}

	bool FileSystem::FileRotate(const std::string& filename)
	{
		std::array<char, MAX_OSPATH> ospath{};

		const auto* basepath = (*Game::fs_homepath)->current.string;
		Game::FS_BuildOSPath(basepath, Game::fs_gamedir, filename.c_str(), ospath.data());
		return FileWrapper_Rotate(ospath.data());
	}

	FileSystem::FileSystem()
	{
		// Thread safe file system interaction
		Utils::Hook(0x4F4BFF, AllocateFile, HOOK_CALL).install()->quick();
		Utils::Hook(Game::FS_FreeFile, FreeFile, HOOK_JUMP).install()->quick();

		// Filesystem config checks
		Utils::Hook(0x6098FD, Cmd_Exec_f_Stub, HOOK_CALL).install()->quick();

		// Don't strip the folders from the config name (otherwise our ExecIsFSStub fails)
		Utils::Hook::Nop(0x6098F2, 5);

		// Register additional folders
		Utils::Hook(0x482647, StartupStub, HOOK_JUMP).install()->quick();

		// exec whitelist removal
		Utils::Hook::Nop(0x609685, 5);
		Utils::Hook::Nop(0x60968C, 2);

		// ignore 'no iwd files found in main'
		Utils::Hook::Nop(0x642A4B, 5);

		// Ignore bad magic, when trying to free hunk when it's already cleared
		Utils::Hook::Set<std::uint16_t>(0x49AACE, 0xC35E);

		// Synchronize filesystem starts
		Utils::Hook(0x4290C6, FsStartupSync, HOOK_CALL).install()->quick(); // FS_InitFilesystem
		Utils::Hook(0x461A88, FsStartupSync, HOOK_CALL).install()->quick(); // FS_Restart

		// Synchronize filesystem restarts
		Utils::Hook(0x4A745B, FsRestartSync, HOOK_CALL).install()->quick(); // SV_SpawnServer
		Utils::Hook(0x4C8609, FsRestartSync, HOOK_CALL).install()->quick(); // FS_ConditionalRestart
		Utils::Hook(0x5AC68E, FsRestartSync, HOOK_CALL).install()->quick(); // CL_ParseServerMessage

		// Synchronize filesystem stops
		Utils::Hook(0x461A55, FsShutdownSync, HOOK_CALL).install()->quick(); // FS_Restart
		Utils::Hook(0x4D40DB, FsShutdownSync, HOOK_CALL).install()->quick(); // Com_Quitf

		// Synchronize db image loading
		Utils::Hook(0x415AB8, DelayLoadImagesSync, HOOK_CALL).install()->quick();
		Utils::Hook(0x4D32BC, LoadTextureSync, HOOK_CALL).install()->quick();

		// Handle IWD freeing
		Utils::Hook(0x642F60, IwdFreeStub, HOOK_CALL).install()->quick();

		// Set the working dir based on info from the Xlabs launcher
		Utils::Hook(0x4326E0, Sys_DefaultInstallPath_Hk, HOOK_JUMP).install()->quick();

		// Make the exe run from a folder other than the game folder
		Utils::Hook(0x406D26, FS_FileOpenReadText_Hk, HOOK_CALL).install()->quick();

		// Make the exe run from a folder other than the game folder
		Utils::Hook::Nop(0x4290D8, 5); // FS_IsBasePathValid
		Utils::Hook::Set<uint8_t>(0x4290DF, 0xEB);
		// ^^ This check by the game above is super redundant, IW4x has other checks in place to make sure we
		// are running from a properly installed directory. This only breaks the containerized patch and we don't need it

		// Patch FS dvar values
		Utils::Hook(0x643194, Sys_DefaultCDPath_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x643232, Sys_HomePath_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x6431B6, Sys_Cwd_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x51C29A, Sys_Cwd_Hk, HOOK_CALL).install()->quick();

		// patch max file amount returned by Sys_ListFiles
		Utils::Hook::Set<std::uint32_t>(0x45A66B, (NEW_MAX_FILES_LISTED + FILE_COUNT_MULTIPLIER) * 4);
		Utils::Hook::Set<std::uint32_t>(0x64AF78, NEW_MAX_FILES_LISTED);
		Utils::Hook::Set<std::uint32_t>(0x64B04F, NEW_MAX_FILES_LISTED);
		Utils::Hook::Set<std::uint32_t>(0x45A8CE, NEW_MAX_FILES_LISTED);

		// Sys_ListFiles
		Utils::Hook(0x45A806, Hunk_UserCreate_Stub, HOOK_CALL).install()->quick();
		Utils::Hook(0x45A6A0, Hunk_UserCreate_Stub, HOOK_CALL).install()->quick();

		// FS_ListFilteredFiles
		Utils::Hook(0x4FCE82,  Hunk_UserCreate_Stub, HOOK_CALL).install()->quick();
		Utils::Hook::Set<std::uint32_t>(0x6427F0 + 2, NEW_MAX_FILES_LISTED);
		Utils::Hook::Set<uint32_t>(0X4FCE8B + 1, (NEW_MAX_FILES_LISTED + FILE_COUNT_MULTIPLIER) * 4 + 4);

	}

	FileSystem::~FileSystem()
	{
		assert(FileSystem::MemAllocator.empty());
	}
}
