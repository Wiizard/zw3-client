#include "STDInclude.hpp"

namespace Main
{
	HMODULE Instance = nullptr;

	bool Initialize()
	{
		std::srand(std::uint32_t(std::time(nullptr)) ^ ~(GetTickCount() * GetCurrentProcessId()));

		if (!Game::Initialize())
		{
			return false;
		}

		Components::Loader::Initialize();

		return true;
	}
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID )
{
	if (reason != DLL_PROCESS_ATTACH)
	{
		return TRUE;
	}

	DisableThreadLibraryCalls(instance);

	Main::Instance = instance;

	Utils::DeleteCrashMarker();

	if (!Main::Initialize())
	{
		MessageBoxA(nullptr,
			"zw3 could not bind to this copy of the game.\n"
			"It is built for the 64 bit 2.0.13 build and this is not it.\n"
			"Nothing has been patched and the game will run unmodified.",
			"Zombie Warfare 3",
			MB_ICONERROR);
	}

	return TRUE;
}
