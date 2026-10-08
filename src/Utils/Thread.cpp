#include "STDInclude.hpp"

namespace Utils::Thread
{
	bool SetName(HANDLE thread, const std::string& name)
	{
		const Library kernel32("kernel32.dll");

		if (!kernel32)
		{
			return false;
		}

		const auto setDescription = kernel32.GetProc<HRESULT(WINAPI*)(HANDLE, PCWSTR)>("SetThreadDescription");

		if (!setDescription)
		{
			return false;
		}

		return SUCCEEDED(setDescription(thread, String::Convert(name).data()));
	}

	bool SetName(DWORD id, const std::string& name)
	{
		const HANDLE thread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, id);

		if (!thread)
		{
			return false;
		}

		const bool isNamed = SetName(thread, name);
		CloseHandle(thread);

		return isNamed;
	}

	bool SetName(std::jthread& thread, const std::string& name)
	{
		return SetName(thread.native_handle(), name);
	}

	bool SetName(const std::string& name)
	{
		return SetName(GetCurrentThread(), name);
	}

	std::vector<DWORD> GetThreadIds()
	{
		const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());

		if (snapshot == INVALID_HANDLE_VALUE)
		{
			return {};
		}

		THREADENTRY32 entry{};
		entry.dwSize = sizeof(entry);

		std::vector<DWORD> ids{};

		if (!Thread32First(snapshot, &entry))
		{
			CloseHandle(snapshot);
			return ids;
		}

		do
		{
			const bool hasOwner = entry.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(entry.th32OwnerProcessID);
			entry.dwSize = sizeof(entry);

			if (hasOwner && entry.th32OwnerProcessID == GetCurrentProcessId())
			{
				ids.emplace_back(entry.th32ThreadID);
			}
		}
		while (Thread32Next(snapshot, &entry));

		CloseHandle(snapshot);

		return ids;
	}
}
