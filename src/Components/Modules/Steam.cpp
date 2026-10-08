#include "STDInclude.hpp"

#include "Steam.hpp"
#include "Logger.hpp"

#include <Steam/Steam.hpp>

namespace Components
{
	Utils::Hook Steam::initHook;

	constexpr std::uintptr_t Com_Init_Steam_InitCall = 0x1401F547D;

	struct ImportSlot
	{
		std::uintptr_t address;
		const char* name;
		void* replacement;
	};

	static const ImportSlot steamImports[] =
	{
		{ 0x140362A08, "SteamNetworking", reinterpret_cast<void*>(::Steam::SteamNetworking) },
		{ 0x140362A10, "SteamRemoteStorage", reinterpret_cast<void*>(::Steam::SteamRemoteStorage) },
		{ 0x140362A18, "SteamAPI_RunCallbacks", reinterpret_cast<void*>(::Steam::SteamAPI_RunCallbacks) },
		{ 0x140362A20, "SteamAPI_RegisterCallback", reinterpret_cast<void*>(::Steam::SteamAPI_RegisterCallback) },
		{ 0x140362A28, "SteamMatchmaking", reinterpret_cast<void*>(::Steam::SteamMatchmaking) },
		{ 0x140362A30, "SteamAPI_RegisterCallResult", reinterpret_cast<void*>(::Steam::SteamAPI_RegisterCallResult) },
		{ 0x140362A38, "SteamAPI_UnregisterCallResult", reinterpret_cast<void*>(::Steam::SteamAPI_UnregisterCallResult) },
		{ 0x140362A40, "SteamGameServer", reinterpret_cast<void*>(::Steam::SteamGameServer) },
		{ 0x140362A48, "SteamGameServer_Shutdown", reinterpret_cast<void*>(::Steam::SteamGameServer_Shutdown) },
		{ 0x140362A50, "SteamApps", reinterpret_cast<void*>(::Steam::SteamApps) },
		{ 0x140362A58, "SteamUtils", reinterpret_cast<void*>(::Steam::SteamUtils) },
		{ 0x140362A60, "SteamFriends", reinterpret_cast<void*>(::Steam::SteamFriends) },
		{ 0x140362A68, "SteamAPI_Init", reinterpret_cast<void*>(::Steam::SteamAPI_Init) },
		{ 0x140362A70, "SteamAPI_Shutdown", reinterpret_cast<void*>(::Steam::SteamAPI_Shutdown) },
		{ 0x140362A78, "SteamUser", reinterpret_cast<void*>(::Steam::SteamUser) },
		{ 0x140362A80, "SteamAPI_UnregisterCallback", reinterpret_cast<void*>(::Steam::SteamAPI_UnregisterCallback) },
		{ 0x140362A88, "SteamGameServer_RunCallbacks", reinterpret_cast<void*>(::Steam::SteamGameServer_RunCallbacks) },
		{ 0x140362A90, "SteamGameServer_Init", reinterpret_cast<void*>(::Steam::SteamGameServer_Init) },
	};

	bool Steam::RedirectImports()
	{
		const Utils::Library steamApi("steam_api64.dll");

		if (!steamApi)
		{
			return false;
		}

		for (const auto& slot : steamImports)
		{
			const auto* const live = reinterpret_cast<void* const*>(Utils::Hook::Rebase(slot.address));
			const auto exported = steamApi.GetProc<void*>(slot.name);

			if (!exported || *live != exported)
			{
				Logger::Error("steam: the {} import does not read as expected\n", slot.name);
				return false;
			}
		}

		for (const auto& slot : steamImports)
		{
			Utils::Hook::Set<void*>(slot.address, slot.replacement);
		}

		return true;
	}

	char Steam::Steam_Init_Stub()
	{
		if (!RedirectImports())
		{
			Logger::Error("steam: could not take over steam_api64, the game talks to the real steam and quits without it\n");
		}

		return reinterpret_cast<char(*)()>(initHook.GetOriginal())();
	}

	static void SkipDrmStub()
	{
		constexpr std::uintptr_t drmEntry = 0x1494C2310;
		constexpr std::uintptr_t crtEntry = 0x1402FD864;

		static const std::uint8_t stubPrologue[] = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x50, 0x53 };
		static const std::uint8_t crtPrologue[] = { 0x48, 0x83, 0xEC, 0x28, 0xE8 };

		const auto* const stub = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(drmEntry));
		const auto* const crt = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(crtEntry));

		if (std::memcmp(stub, stubPrologue, sizeof(stubPrologue)) != 0)
		{
			Logger::Warning("steam: the entry point is not the SteamStub prologue, left alone\n");
			return;
		}

		if (std::memcmp(crt, crtPrologue, sizeof(crtPrologue)) != 0)
		{
			Logger::Warning("steam: WinMainCRTStartup does not read as expected, left alone\n");
			return;
		}

		const auto relative = static_cast<std::int32_t>(crtEntry - (drmEntry + 5));

		Utils::Hook::Set<std::int32_t>(drmEntry + 1, relative);
		Utils::Hook::Set<std::uint8_t>(drmEntry, 0xE9);
	}

	Steam::Steam()
	{
		SkipDrmStub();

		if (!initHook.Initialize(Com_Init_Steam_InitCall, Steam_Init_Stub, HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("steam: could not hook Steam_Init, the game talks to the real steam and quits without it\n");
			return;
		}

		initHook.Quick();
	}
}
