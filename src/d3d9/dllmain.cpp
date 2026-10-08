#include <Windows.h>

#include "proxy.hpp"

static HMODULE client = nullptr;

static bool LoadClient()
{
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	wchar_t* const separator = wcsrchr(path, L'\\');

	if (!separator)
	{
		return false;
	}

	separator[1] = L'\0';

	if (wcscat_s(path, L"zw3.dll") != 0)
	{
		return false;
	}

	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
	{
		wchar_t message[MAX_PATH + 32];
		wcscpy_s(message, L"Can't find zw3.dll:\n");
		wcscat_s(message, path);

		MessageBoxW(nullptr, message, L"Zombie Warfare 3", MB_OK | MB_ICONERROR);
		TerminateProcess(GetCurrentProcess(), ERROR_MOD_NOT_FOUND);
	}

	client = LoadLibraryW(path);

	return client != nullptr;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID )
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(instance);
		D3D9Proxy_Attach(true);

		if (!LoadClient())
		{
			MessageBoxA(nullptr,
				"zw3.dll could not be loaded.\n"
				"It must sit next to the game alongside d3d9.dll.",
				"Zombie Warfare 3",
				MB_ICONERROR);
		}

		LoadLibraryExA("iw4dlss5.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

		return TRUE;
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		D3D9Proxy_Detach();
	}

	return TRUE;
}
