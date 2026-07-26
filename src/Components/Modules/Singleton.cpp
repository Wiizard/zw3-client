#include "ConnectProtocol.hpp"
#include "Console.hpp"

#include <version.hpp>

namespace Components
{
	HANDLE Singleton::Mutex;

	bool Singleton::FirstInstance = true;
	bool Singleton::MutexInitialized = false;

	bool Singleton::IsFirstInstance()
	{
		return FirstInstance;
	}

	bool Singleton::InitializeMutex()
	{
		if (MutexInitialized)
		{
			return FirstInstance;
		}

		MutexInitialized = true;

		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
		{
			return FirstInstance;
		}

		Mutex = CreateMutexA(nullptr, FALSE, "zw3_mutex");
		FirstInstance = (Mutex != nullptr && Mutex != INVALID_HANDLE_VALUE && GetLastError() != ERROR_ALREADY_EXISTS);

		if (!FirstInstance && !ConnectProtocol::Used() && MessageBoxA(nullptr, "Do you want to start another instance?\nNot all features will be available!", "Game already running", MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
		{
			ExitProcess(EXIT_SUCCESS);
		}

		return FirstInstance;
	}

	void Singleton::preDestroy()
	{
		if (Mutex != nullptr && Mutex != INVALID_HANDLE_VALUE)
		{
			CloseHandle(Mutex);
			Mutex = nullptr;
		}
	}

	Singleton::Singleton()
	{
		if (Flags::HasFlag("version"))
		{
#ifdef EXPERIMENTAL_BUILD
			printf("%s", "Call of Duty: Zombie Warfare 3 (built " __DATE__ " " __TIME__ ")\n");
#else
			printf("%s", "Call of Duty: Zombie Warfare 3 (built " __DATE__ " " __TIME__ ")\n");
#endif

			ExitProcess(EXIT_SUCCESS);
		}

		Console::FreeNativeConsole();

		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		InitializeMutex();
	}
}
