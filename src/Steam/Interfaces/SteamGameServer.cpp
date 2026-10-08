#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamGameServer.hpp"

namespace Steam
{
	void* const GameServer::vtable[] =
	{
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(LogOff),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(BSecure),
		reinterpret_cast<void*>(GetSteamID),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(SendUserConnectAndAuthenticate),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(SendUserDisconnect),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UserHasLicenseForApp),
	};

	Interface GameServer::object = { GameServer::vtable };

	Interface* GameServer::Get()
	{
		static_assert(std::size(vtable) == 33);
		return &object;
	}

	void GameServer::LogOff([[maybe_unused]] Interface* self)
	{
	}

	bool GameServer::BSecure([[maybe_unused]] Interface* self)
	{
		return false;
	}

	SteamID* GameServer::GetSteamID([[maybe_unused]] Interface* self, SteamID* result)
	{
		*result = SteamID();
		return result;
	}

	bool GameServer::SendUserConnectAndAuthenticate([[maybe_unused]] Interface* self, [[maybe_unused]] unsigned int clientIp,
		[[maybe_unused]] const void* authBlob, [[maybe_unused]] unsigned int authBlobSize, [[maybe_unused]] SteamID* user)
	{
		return true;
	}

	void GameServer::SendUserDisconnect([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID user)
	{
	}

	int GameServer::UserHasLicenseForApp([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID user, [[maybe_unused]] unsigned int appId)
	{
		return 0;
	}
}
