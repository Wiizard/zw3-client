#include "STDInclude.hpp"

#include "Singleton.hpp"
#include "ConnectProtocol.hpp"
#include "Console.hpp"
#include "Dedicated.hpp"
#include "Flags.hpp"

namespace Components
{
	HANDLE Singleton::mutex = nullptr;
	HANDLE Singleton::slotMutex = nullptr;
	bool Singleton::isFirstInstance = true;
	int Singleton::instanceSlot = 1;

	bool Singleton::IsFirstInstance()
	{
		return isFirstInstance;
	}

	int Singleton::GetInstanceSlot()
	{
		return instanceSlot;
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

		if (isFirstInstance)
		{
			return;
		}

		if (!ConnectProtocol::Used() && MessageBoxA(nullptr, "Do you want to start another instance?\nNot all features will be available!", "Game already running",
			MB_ICONEXCLAMATION | MB_YESNO) == IDNO)
		{
			ExitProcess(EXIT_SUCCESS);
		}

		// Claim the lowest free extra-instance slot. The slot names the identity
		// file this instance uses, so it is stable across launches and never
		// shared by two running instances.
		for (int slot = 2; slot <= 16; ++slot)
		{
			HANDLE handle = CreateMutexA(nullptr, FALSE, Utils::String::VA("zw3_mutex_slot_%d", slot));
			if (handle && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				slotMutex = handle;
				instanceSlot = slot;
				return;
			}
			if (handle) CloseHandle(handle);
		}

		instanceSlot = 0; // Every slot is taken: fall back to a process-only identity.
	}
}
