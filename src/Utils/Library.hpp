#pragma once

namespace Utils
{
	class Library
	{
	public:
		static Library Load(const std::filesystem::path& path);
		static Library GetByAddress(void* address);

		Library();
		Library(const std::string& name, bool freeOnDestroy);
		explicit Library(const std::string& name);
		explicit Library(HMODULE handle);
		~Library();

		bool operator!=(const Library& obj) const;
		bool operator==(const Library& obj) const;

		operator bool() const;
		operator HMODULE() const;

		[[nodiscard]] bool IsValid() const;
		[[nodiscard]] HMODULE GetModule() const;
		[[nodiscard]] std::string GetName() const;
		[[nodiscard]] std::filesystem::path GetPath() const;
		[[nodiscard]] std::filesystem::path GetFolder() const;
		void Free();

		template <typename T>
		[[nodiscard]] T GetProc(const std::string& process) const
		{
			if (!this->IsValid())
			{
				return T{};
			}

			return reinterpret_cast<T>(GetProcAddress(this->handle, process.data()));
		}

		template <typename T>
		[[nodiscard]] std::function<T> Get(const std::string& process) const
		{
			if (!this->IsValid())
			{
				return std::function<T>();
			}

			return reinterpret_cast<T*>(this->GetProc<void*>(process));
		}

		template <typename T, typename... Args>
		T Invoke(const std::string& process, Args... args) const
		{
			const auto method = this->Get<T(__cdecl)(Args...)>(process);

			if (method)
			{
				return method(args...);
			}

			return T();
		}

		template <typename T, typename... Args>
		T InvokePascal(const std::string& process, Args... args) const
		{
			const auto method = this->Get<T(__stdcall)(Args...)>(process);

			if (method)
			{
				return method(args...);
			}

			return T();
		}

		template <typename T, typename... Args>
		T InvokeThis(const std::string& process, void* thisPtr, Args... args) const
		{
			const auto method = this->Get<T(__thiscall)(void*, Args...)>(process);

			if (method)
			{
				return method(thisPtr, args...);
			}

			return T();
		}

		static void LaunchProcess(const std::wstring& process, const std::wstring& commandLine, const std::filesystem::path& currentDir);

	private:
		HMODULE handle;
		bool freeOnDestroy;
	};
}
