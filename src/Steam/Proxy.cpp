#include "STDInclude.hpp"

#include "Steam/Proxy.hpp"
#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Logger.hpp"

namespace Steam
{
	::Utils::Library Proxy::Client;
	::Utils::Library Proxy::Overlay;

	ISteamClient008* Proxy::SteamClient = nullptr;

	std::int32_t Proxy::SteamPipe = 0;
	std::int32_t Proxy::SteamUser = 0;

	Friends15* Proxy::SteamFriends = nullptr;
	Apps7* Proxy::SteamApps = nullptr;
	Utils5* Proxy::SteamUtils = nullptr;
	User12* Proxy::SteamUser_ = nullptr;

	HANDLE Proxy::Process = nullptr;
	HANDLE Proxy::CancelHandle = nullptr;
	std::jthread Proxy::WatchGuard;

	std::uint32_t Proxy::AppId = 0;

	std::recursive_mutex Proxy::CallMutex;
	std::vector<Proxy::CallContainer> Proxy::Calls;
	std::unordered_map<std::int32_t, Proxy::Callback> Proxy::Callbacks;

	Proxy::SteamBGetCallbackFn* Proxy::SteamBGetCallback = nullptr;
	Proxy::SteamFreeLastCallbackFn* Proxy::SteamFreeLastCallback = nullptr;
	Proxy::SteamGetAPICallResultFn* Proxy::SteamGetAPICallResult = nullptr;

	void Proxy::SetGame(std::uint32_t appId)
	{
		AppId = appId;

		if (Components::Dedicated::IsEnabled())
		{
			return;
		}

		SetEnvironmentVariableA("SteamAppId", std::to_string(AppId).data());
		SetEnvironmentVariableA("SteamGameId", std::to_string(AppId & 0xFFFFFF).data());

		::Utils::IO::WriteFile("steam_appid.txt", std::to_string(AppId), false);
	}

	void Proxy::RegisterCall(std::int32_t callId, std::uint32_t size, std::uint64_t call)
	{
		std::lock_guard _(CallMutex);

		CallContainer container;
		container.call = call;
		container.dataSize = size;
		container.callId = callId;
		container.handled = false;

		Calls.push_back(container);
	}

	void Proxy::UnregisterCalls()
	{
		std::lock_guard _(CallMutex);

		std::erase_if(Calls, [](const CallContainer& container)
		{
			return container.handled;
		});
	}

	void Proxy::RegisterCallback(std::int32_t callId, Callback callback)
	{
		std::lock_guard _(CallMutex);
		Callbacks[callId] = callback;
	}

	void Proxy::UnregisterCallback(std::int32_t callId)
	{
		std::lock_guard _(CallMutex);
		Callbacks.erase(callId);
	}

	void Proxy::RunCallback(std::int32_t callId, void* data)
	{
		std::lock_guard _(CallMutex);

		const auto callback = Callbacks.find(callId);

		if (callback != Callbacks.end())
		{
			callback->second(data);
		}
	}

	void Proxy::RunFrame()
	{
		std::lock_guard _(CallMutex);

		if (SteamUtils)
		{
			SteamUtils->RunFrame();
		}

		CallbackMsg message{};

		while (SteamBGetCallback && SteamFreeLastCallback && SteamBGetCallback(SteamPipe, &message))
		{
			Steam::Callbacks::RunCallback(message.m_iCallback, message.m_pubParam);
			RunCallback(message.m_iCallback, message.m_pubParam);
			SteamFreeLastCallback(SteamPipe);
		}

		if (SteamUtils)
		{
			for (auto& call : Calls)
			{
				bool isFailed = false;

				if (!SteamUtils->IsAPICallCompleted(call.call, &isFailed))
				{
					continue;
				}

				call.handled = true;

				if (isFailed)
				{
					continue;
				}

				std::vector<char> buffer(call.dataSize);
				SteamUtils->GetAPICallResult(call.call, buffer.data(), static_cast<int>(call.dataSize), call.callId, &isFailed);

				if (isFailed)
				{
					continue;
				}

				RunCallback(call.callId, buffer.data());
			}
		}

		UnregisterCalls();
	}

	void Proxy::LaunchWatchGuard()
	{
		if (WatchGuard.joinable())
		{
			return;
		}

		HKEY key;
		DWORD pid = 0;

		if (RegOpenKeyExA(HKEY_CURRENT_USER, STEAM_REGISTRY_PROCESS_PATH, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		{
			return;
		}

		DWORD length = sizeof(pid);
		RegQueryValueExA(key, "pid", nullptr, nullptr, reinterpret_cast<BYTE*>(&pid), &length);
		RegCloseKey(key);

		CancelHandle = CreateEventA(nullptr, TRUE, FALSE, nullptr);

		if (!CancelHandle)
		{
			return;
		}

		Process = OpenProcess(SYNCHRONIZE, FALSE, pid);

		if (!Process)
		{
			CloseHandle(CancelHandle);
			CancelHandle = nullptr;
			return;
		}

		WatchGuard = std::jthread([]
		{
			HANDLE handles[] = { Process, CancelHandle };

			const DWORD result = WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);
			CloseHandle(Process);
			CloseHandle(CancelHandle);

			if (result == WAIT_OBJECT_0)
			{
				SteamPipe = 0;
				SteamUser = 0;
				UnInitialize();
			}
		});
	}

	bool Proxy::Initialize()
	{
		const auto directory = GetSteamDirectory();

		if (directory.empty())
		{
			return false;
		}

		SetDllDirectoryA(directory.data());

		if (!Components::Dedicated::IsEnabled())
		{
			LaunchWatchGuard();

			Overlay = ::Utils::Library(GAMEOVERLAY_LIB, false);

			if (!Overlay)
			{
				return false;
			}
		}

		Client = ::Utils::Library(STEAMCLIENT_LIB, false);

		if (!Client)
		{
			return false;
		}

		SteamBGetCallback = Client.GetProc<SteamBGetCallbackFn*>("Steam_BGetCallback");
		SteamFreeLastCallback = Client.GetProc<SteamFreeLastCallbackFn*>("Steam_FreeLastCallback");
		SteamGetAPICallResult = Client.GetProc<SteamGetAPICallResultFn*>("Steam_GetAPICallResult");
		const auto createInterface = Client.GetProc<void*(*)(const char*, int*)>("CreateInterface");

		if (!SteamBGetCallback || !SteamFreeLastCallback || !SteamGetAPICallResult || !createInterface)
		{
			return false;
		}

		SteamClient = static_cast<ISteamClient008*>(createInterface("SteamClient008", nullptr));

		if (!SteamClient)
		{
			return false;
		}

		SteamPipe = SteamClient->CreateSteamPipe();

		if (!SteamPipe)
		{
			return false;
		}

		SteamUser = SteamClient->ConnectToGlobalUser(SteamPipe);

		if (!SteamUser)
		{
			return false;
		}

		SteamApps = static_cast<Apps7*>(SteamClient->GetISteamApps(SteamUser, SteamPipe, "STEAMAPPS_INTERFACE_VERSION007"));
		SteamFriends = static_cast<Friends15*>(SteamClient->GetISteamFriends(SteamUser, SteamPipe, "SteamFriends015"));
		SteamUtils = static_cast<Utils5*>(SteamClient->GetISteamUtils(SteamPipe, "SteamUtils005"));
		SteamUser_ = static_cast<User12*>(SteamClient->GetISteamUser(SteamUser, SteamPipe, "SteamUser012"));

		return SteamApps && SteamFriends && SteamUtils && SteamUser_;
	}

	void Proxy::UnInitialize()
	{
		if (WatchGuard.get_id() != std::this_thread::get_id() && WatchGuard.joinable())
		{
			if (CancelHandle)
			{
				SetEvent(CancelHandle);
				WatchGuard.join();
			}
			else
			{
				WatchGuard.detach();
			}
		}

		Process = nullptr;
		CancelHandle = nullptr;

		std::lock_guard _(CallMutex);

		SteamApps = nullptr;
		SteamFriends = nullptr;
		SteamUtils = nullptr;
		SteamUser_ = nullptr;

		if (SteamClient && SteamPipe)
		{
			if (SteamUser)
			{
				SteamClient->ReleaseUser(SteamPipe, SteamUser);
			}

			SteamClient->BReleaseSteamPipe(SteamPipe);
		}

		SteamPipe = 0;
		SteamUser = 0;
		SteamClient = nullptr;

		SteamBGetCallback = nullptr;
		SteamFreeLastCallback = nullptr;
		SteamGetAPICallResult = nullptr;

		Client = ::Utils::Library();
		Overlay = ::Utils::Library();
	}

	std::string Proxy::GetSteamDirectory()
	{
		HKEY key;
		char steamPath[MAX_PATH]{};

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, STEAM_REGISTRY_PATH, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		{
			return {};
		}

		DWORD length = sizeof(steamPath);
		RegQueryValueExA(key, "InstallPath", nullptr, nullptr, reinterpret_cast<BYTE*>(steamPath), &length);
		RegCloseKey(key);

		return steamPath;
	}

	void Proxy::SetOverlayNotificationPosition(std::uint32_t eNotificationPosition)
	{
		if (!Overlay)
		{
			return;
		}

		const auto setNotificationPosition = Overlay.GetProc<void(*)(std::uint32_t)>("SetNotificationPosition");

		if (setNotificationPosition)
		{
			setNotificationPosition(eNotificationPosition);
		}
	}
}
