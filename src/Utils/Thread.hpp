#pragma once

namespace Utils::Thread
{
	bool SetName(HANDLE thread, const std::string& name);
	bool SetName(DWORD id, const std::string& name);
	bool SetName(std::jthread& thread, const std::string& name);
	bool SetName(const std::string& name);

	template <typename... Args>
	std::jthread CreateNamedThread(const std::string& name, Args&&... args)
	{
		auto thread = std::jthread(std::forward<Args>(args)...);
		SetName(thread, name);
		return thread;
	}

	std::vector<DWORD> GetThreadIds();
}
