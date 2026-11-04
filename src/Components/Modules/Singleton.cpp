#include "STDInclude.hpp"

#include "Singleton.hpp"
#include "ConnectProtocol.hpp"
#include "Console.hpp"
#include "Dedicated.hpp"
#include "Flags.hpp"

namespace Components
{
	HANDLE Singleton::mutex = nullptr;
	bool Singleton::isFirstInstance = true;

	bool Singleton::IsFirstInstance()
	{
		return isFirstInstance;
	}

	Singleton::Singleton()
	{
		if (Flags::HasFlag("version"))
		{
			printf("%s", "Call of Duty: Zombie Warfare 3 (built " __DATE__ " " __TIME__ ")\n");
			ExitProcess(EXIT_SUCCESS);
		}

		Console::FreeNativeConsole();

		if (Dedicated::IsEnabled())
		{
			return;
		}

		mutex = CreateMutexA(nullptr, FALSE, "zw3_mutex");
		isFirstInstance = mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;

		if (isFirstInstance || ConnectProtocol::Used())
		{
			return;
		}

		if (MessageBoxA(nullptr, "Do you want to start another instance?\nNot all features will be available!", "Game already running",
			MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
		{
			ExitProcess(EXIT_SUCCESS);
		}
	}
}
