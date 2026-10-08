#pragma once

namespace Components
{
	class FileSystem : public Component
	{
	public:
		class AbstractFile
		{
		public:
			virtual ~AbstractFile() = default;

			[[nodiscard]] virtual bool Exists() const noexcept = 0;
			[[nodiscard]] virtual std::string GetName() const = 0;
			[[nodiscard]] virtual std::string& GetBuffer() = 0;

			virtual explicit operator bool()
			{
				return this->Exists();
			}
		};

		class File : public AbstractFile
		{
		public:
			File() = default;
			File(std::string file) : filePath{ std::move(file) } { this->Read(); }
			File(std::string file, Game::FsThread thread) : filePath{ std::move(file) } { this->Read(thread); }

			[[nodiscard]] bool Exists() const noexcept override { return !this->buffer.empty(); }
			[[nodiscard]] std::string GetName() const override { return this->filePath; }
			[[nodiscard]] std::string& GetBuffer() override { return this->buffer; }

		private:
			std::string filePath;
			std::string buffer;

			void Read(Game::FsThread thread = Game::FS_THREAD_MAIN);
		};

		class RawFile : public AbstractFile
		{
		public:
			RawFile() = default;
			RawFile(std::string file) : filePath(std::move(file)) { this->Read(); }

			[[nodiscard]] bool Exists() const noexcept override { return !this->buffer.empty(); }
			[[nodiscard]] std::string GetName() const override { return this->filePath; }
			[[nodiscard]] std::string& GetBuffer() override { return this->buffer; }

		private:
			std::string filePath;
			std::string buffer;

			void Read();
		};

		class FileReader
		{
		public:
			FileReader() : handle(0), size(-1) {}
			FileReader(std::string file);
			~FileReader();

			[[nodiscard]] bool Exists() const noexcept;
			[[nodiscard]] std::string GetName() const;
			[[nodiscard]] std::string GetBuffer() const;
			[[nodiscard]] int GetSize() const noexcept;
			bool Read(void* buffer, std::size_t size) const noexcept;
			void Seek(int offset, int origin) const;

		private:
			Game::fileHandle_t handle;
			int size;
			std::string name;
		};

		class FileWriter
		{
		public:
			FileWriter(std::string file, bool append = false) : handle(0), filePath(std::move(file)) { this->Open(append); }
			~FileWriter() { this->Close(); }

			void Write(const std::string& data) const;

		private:
			Game::fileHandle_t handle;
			std::string filePath;

			void Open(bool append = false);
			void Close();
		};

		FileSystem();

		static std::vector<std::string> GetFileList(const std::string& path, const std::string& extension);

		static std::filesystem::path GetAppdataPath();

		static std::vector<std::string> GetSysFileList(const std::string& path, const std::string& extension, bool folders = false);

		static Game::FsThread GetCurrentThread();

	private:
		static std::recursive_mutex fsMutex;

		static Utils::Hook registerDvarsHook;
		static Utils::Hook execSubStringHook;
		static Utils::Hook execCompareHook;
		static Utils::Hook startupHook;

		static void FS_RegisterDvars_Stub();

		static void CleanupZw3Files();

		static void AllowAnyIwdName();

		static int Cmd_Exec_f_Stub(const char* name, const char* configName);
		static char* Com_GetFilenameSubString_Stub(char* path);
		static void HookConfigExec();

		static void RegisterFolder(const char* folder);
		static Game::dvar_t* FS_Startup_Stub();
		static void HookStartupFolders();

		static Utils::Hook addLocalizedHook;
		static void FS_AddLocalizedGameDirectory_Stub(const char* path, const char* folder);
		static void HookLocalizedDirectories();

		static void FsRestartSync(int localClientNum, int checksumFeed);
		static void CL_Vid_Restart_f_UpdateLanguage_Hk();
		static void FsShutdownSync(int closemfp);
		static void LoadTextureSync(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image);
		static void IwdFreeStub(Game::iwd_t* iwd);
		static void HookSync();
	};
}
