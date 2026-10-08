#include "STDInclude.hpp"

#include "Steam.hpp"
#include "Proxy.hpp"
#include "Components/Modules/Logger.hpp"
#include "Components/Modules/Flags.hpp"
#include "Interfaces/SteamApps.hpp"
#include "Interfaces/SteamFriends.hpp"
#include "Interfaces/SteamGameServer.hpp"
#include "Interfaces/SteamMatchmaking.hpp"
#include "Interfaces/SteamNetworking.hpp"
#include "Interfaces/SteamRemoteStorage.hpp"
#include "Interfaces/SteamUser.hpp"
#include "Interfaces/SteamUtils.hpp"

#include "Components/Modules/Logger.hpp"

namespace Steam
{
	std::uint64_t Callbacks::CallID = 0;
	std::map<std::uint64_t, bool> Callbacks::Calls;
	std::map<std::uint64_t, Callbacks::Base*> Callbacks::ResultHandlers;
	std::vector<Callbacks::Result> Callbacks::Results;
	std::vector<Callbacks::Base*> Callbacks::CallbackList;
	std::recursive_mutex Callbacks::Mutex;

	std::uint64_t Callbacks::RegisterCall()
	{
		std::lock_guard _(Mutex);

		Calls[++CallID] = false;

		return CallID;
	}

	void Callbacks::RegisterCallback(Base* handler, int callback)
	{
		std::lock_guard _(Mutex);

		if (!handler)
		{
			return;
		}

		handler->SetICallback(callback);
		handler->SetRegistered(true);

		if (std::ranges::find(CallbackList, handler) == CallbackList.end())
		{
			CallbackList.push_back(handler);
		}
	}

	void Callbacks::RegisterCallResult(std::uint64_t call, Base* result)
	{
		std::lock_guard _(Mutex);

		if (!result)
		{
			return;
		}

		ResultHandlers[call] = result;
	}

	void Callbacks::UnregisterCallback(Base* handler)
	{
		std::lock_guard _(Mutex);

		std::erase(CallbackList, handler);

		if (handler)
		{
			handler->SetRegistered(false);
		}
	}

	void Callbacks::UnregisterCallResult(Base* result, std::uint64_t call)
	{
		std::lock_guard _(Mutex);

		const auto entry = ResultHandlers.find(call);

		if (entry != ResultHandlers.end() && entry->second == result)
		{
			ResultHandlers.erase(entry);
		}
	}

	void Callbacks::ReturnCall(void* data, int size, int type, std::uint64_t call)
	{
		std::lock_guard _(Mutex);

		Calls[call] = true;

		Result result{};
		result.call = call;
		result.data = data;
		result.size = size;
		result.type = type;

		Results.push_back(result);
	}

	void Callbacks::RunCallbacks()
	{
		std::lock_guard _(Mutex);

		const auto pending = Results;
		Results.clear();

		for (const auto& result : pending)
		{
			const auto handler = ResultHandlers.find(result.call);

			if (handler != ResultHandlers.end())
			{
				auto* const resultHandler = handler->second;
				ResultHandlers.erase(handler);
				resultHandler->Run(result.data, false, result.call);
			}

			const auto callbacks = CallbackList;

			for (auto* const callback : callbacks)
			{
				const bool isStillRegistered = std::ranges::find(CallbackList, callback) != CallbackList.end();

				if (isStillRegistered && callback->GetICallback() == result.type)
				{
					callback->Run(result.data, false, 0);
				}
			}

			if (result.data)
			{
				::Utils::Memory::Free(result.data);
			}
		}
	}

	void Callbacks::RunCallback(int callback, void* data)
	{
		std::lock_guard _(Mutex);

		const auto handlers = CallbackList;

		for (auto* const handler : handlers)
		{
			const bool isStillRegistered = std::ranges::find(CallbackList, handler) != CallbackList.end();

			if (handler && isStillRegistered && handler->GetICallback() == callback)
			{
				handler->Run(data);
			}
		}
	}

	void Callbacks::Uninitialize()
	{
		std::lock_guard _(Mutex);

		for (const auto& result : Results)
		{
			if (result.data)
			{
				::Utils::Memory::Free(result.data);
			}
		}

		Results.clear();
	}

	bool SteamAPI_Init()
	{
		if (!Components::Flags::HasFlag("steam"))
		{
			Components::Logger::Print("steam: not proxying a Steam client, -steam turns it on\n");
			return true;
		}

		Proxy::SetGame(10190);

		if (Proxy::Initialize())
		{
			Components::Logger::Print("steam: proxying the running Steam client\n");
		}
		else
		{
			Components::Logger::Print("steam: no Steam client to proxy\n");
		}

		return true;
	}

	void SteamAPI_RegisterCallResult(Callbacks::Base* result, std::uint64_t call)
	{
		Callbacks::RegisterCallResult(call, result);
	}

	void SteamAPI_RegisterCallback(Callbacks::Base* handler, int callback)
	{
		Callbacks::RegisterCallback(handler, callback);
	}

	void SteamAPI_RunCallbacks()
	{
		Callbacks::RunCallbacks();
		Proxy::RunFrame();
	}

	void SteamAPI_Shutdown()
	{
		Proxy::UnInitialize();
		Callbacks::Uninitialize();
	}

	void SteamAPI_UnregisterCallResult(Callbacks::Base* result, std::uint64_t call)
	{
		Callbacks::UnregisterCallResult(result, call);
	}

	void SteamAPI_UnregisterCallback(Callbacks::Base* handler)
	{
		Callbacks::UnregisterCallback(handler);
	}

	bool SteamGameServer_Init()
	{
		return true;
	}

	void SteamGameServer_RunCallbacks()
	{
	}

	void SteamGameServer_Shutdown()
	{
	}

	std::uint64_t UnusedSlot()
	{
		static std::mutex mutex;
		static std::unordered_set<std::uintptr_t> reportedSites;

		const auto callSite = ::Utils::Hook::Unrebase(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

		std::lock_guard _(mutex);

		if (reportedSites.insert(callSite).second)
		{
			Components::Logger::Error("steam: a vtable slot nothing was mapped to call was called, returning to IDB {:#x}\n", callSite);
		}

		return 0;
	}

	Interface* SteamApps()
	{
		return Apps::Get();
	}

	Interface* SteamFriends()
	{
		return Friends::Get();
	}

	Interface* SteamGameServer()
	{
		return GameServer::Get();
	}

	Interface* SteamMatchmaking()
	{
		return Matchmaking::Get();
	}

	Interface* SteamNetworking()
	{
		return Networking::Get();
	}

	Interface* SteamRemoteStorage()
	{
		return RemoteStorage::Get();
	}

	Interface* SteamUser()
	{
		return User::Get();
	}

	Interface* SteamUtils()
	{
		return Utils::Get();
	}
}
