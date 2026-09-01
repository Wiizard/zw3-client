namespace
{
	double ProcessAgeMilliseconds()
	{
		FILETIME created{}, exited{}, kernel{}, user{}, now{};
		if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0.0;
		GetSystemTimeAsFileTime(&now);
		ULARGE_INTEGER start{}, current{};
		start.LowPart = created.dwLowDateTime;
		start.HighPart = created.dwHighDateTime;
		current.LowPart = now.dwLowDateTime;
		current.HighPart = now.dwHighDateTime;
		return static_cast<double>(current.QuadPart - start.QuadPart) / 10000.0;
	}
}

namespace Main
{
	void Initialize()
	{
		const bool profile = Components::Flags::HasFlag("startupProfile");
		const auto stage = [profile](const char* name)
		{
			if (profile) printf("Startup profile: process -> %s %.2f ms.\n", name, ProcessAgeMilliseconds());
		};
		stage("client entry");
		std::srand(std::uint32_t(std::time(nullptr)) ^ ~(GetTickCount() * GetCurrentProcessId()));

		Utils::SetEnvironment();
		Steam::Proxy::RunMod();
		stage("environment ready");

		Utils::Cryptography::Initialize();
		stage("crypto ready");

		Components::FileSystem::CleanupZw3Files();
		stage("cleanup complete");
		Components::Loader::Initialize();
		stage("components ready");
	}

	void Uninitialize()
	{
		Components::Loader::Uninitialize();
	}

	int EntryPoint()
	{
		// /GS security cookie must be initialized before any exception-handling
		// constructs are registered in the current module.
		Game::__security_init_cookie();

		// Perform ZW3-specific initialization before transferring control to
		// the original C runtime startup.
		Initialize();

		return Game::__tmainCRTStartup();
	}
}

BOOL APIENTRY DllMain(HINSTANCE /*hinstDLL*/, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		SetProcessDEPPolicy(PROCESS_DEP_ENABLE);

#ifndef DISABLE_BINARY_CHECK
		const auto* binary = reinterpret_cast<const char*>(0x6F9358);
		if (!binary || std::memcmp(binary, BASEGAME_NAME, 14) != 0)
		{
			MessageBoxA(nullptr,
			            "Failed to load game binary.\n"
			            "You did not install the iw4x-rawfiles!\n"
			            "Please use the Zombie Warfare 3 Launcher to run the game. For support, please visit https://zw3.eu",
			            "ERROR",
			            MB_ICONERROR
			);
			return FALSE;
		}
#endif

		Utils::Hook(0x6BAC0F, Main::EntryPoint, HOOK_JUMP).install()->quick();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		// For `DLL_PROCESS_DETACH`, the `lpReserved` parameter is used to
		// determine the context:
		//
		//   - `lpReserved == nullptr` when `FreeLibrary()` is called.
		//   - `lpReserved != nullptr` when the process is being terminated.
		//
		// When `FreeLibrary()` is called, worker threads remain alive. That is,
		// runtime's state is consistent, and executing proper shutdown is
		// acceptable.
		//
		// When process is terminated, worker threads have either exited or been
		// forcefully terminated by the OS, leaving only the shutdown thread.
		// This situation leaves runtime in an inconsistent state.
		//
		// Hence, proper cleanup should only be attempted when `FreeLibrary()`
		// is called. Otherwise, the process should rely on the OS to reclaim
		// resources.
		if (lpvReserved != nullptr) return TRUE;

		Main::Uninitialize();
	}

	return TRUE;
}
