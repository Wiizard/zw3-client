#include "STDInclude.hpp"

namespace Utils
{
	Library Library::Load(const std::filesystem::path& path)
	{
		return Library(LoadLibraryA(path.generic_string().data()));
	}

	Library Library::GetByAddress(void* address)
	{
		HMODULE handle = nullptr;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCSTR>(address), &handle);

		return Library(handle);
	}

	Library::Library()
		: handle(nullptr), freeOnDestroy(false)
	{
	}

	Library::Library(const std::string& name, bool freeOnDestroy)
		: handle(nullptr), freeOnDestroy(freeOnDestroy)
	{
		this->handle = LoadLibraryExA(name.data(), nullptr, 0);
	}

	Library::Library(const std::string& name)
		: handle(GetModuleHandleA(name.data())), freeOnDestroy(false)
	{
	}

	Library::Library(HMODULE handle)
		: handle(handle), freeOnDestroy(true)
	{
	}

	Library::~Library()
	{
		if (this->freeOnDestroy)
		{
			this->Free();
		}
	}

	bool Library::operator!=(const Library& obj) const
	{
		return !(*this == obj);
	}

	bool Library::operator==(const Library& obj) const
	{
		return this->handle == obj.handle;
	}

	Library::operator bool() const
	{
		return this->IsValid();
	}

	Library::operator HMODULE() const
	{
		return this->GetModule();
	}

	bool Library::IsValid() const
	{
		return this->handle != nullptr;
	}

	HMODULE Library::GetModule() const
	{
		return this->handle;
	}

	std::string Library::GetName() const
	{
		if (!this->IsValid())
		{
			return {};
		}

		const auto path = this->GetPath();
		const auto genericPath = path.generic_string();
		const auto pos = genericPath.find_last_of("/\\");

		if (pos == std::string::npos)
		{
			return genericPath;
		}

		return genericPath.substr(pos + 1);
	}

	std::filesystem::path Library::GetPath() const
	{
		if (!this->IsValid())
		{
			return {};
		}

		wchar_t name[MAX_PATH]{};
		GetModuleFileNameW(this->handle, name, MAX_PATH);

		return { name };
	}

	std::filesystem::path Library::GetFolder() const
	{
		if (!this->IsValid())
		{
			return {};
		}

		const auto path = this->GetPath();
		return path.parent_path().generic_string();
	}

	void Library::Free()
	{
		if (this->IsValid())
		{
			FreeLibrary(this->handle);
		}

		this->handle = nullptr;
	}

	void Library::LaunchProcess(const std::wstring& process, const std::wstring& commandLine, const std::filesystem::path& currentDir)
	{
		STARTUPINFOW startupInfo{};
		PROCESS_INFORMATION processInfo{};
		startupInfo.cb = sizeof(startupInfo);

		CreateProcessW(process.data(), const_cast<wchar_t*>(commandLine.data()), nullptr,
			nullptr, false, NULL, nullptr, currentDir.wstring().data(),
			&startupInfo, &processInfo);

		if (processInfo.hThread && processInfo.hThread != INVALID_HANDLE_VALUE)
		{
			CloseHandle(processInfo.hThread);
		}

		if (processInfo.hProcess && processInfo.hProcess != INVALID_HANDLE_VALUE)
		{
			CloseHandle(processInfo.hProcess);
		}
	}
}
