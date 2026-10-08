#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamNetworking.hpp"

namespace Steam
{
	void* const Networking::vtable[] =
	{
		reinterpret_cast<void*>(SendP2PPacket),
		reinterpret_cast<void*>(IsP2PPacketAvailable),
		reinterpret_cast<void*>(ReadP2PPacket),
		reinterpret_cast<void*>(AcceptP2PSessionWithUser),
	};

	Interface Networking::object = { Networking::vtable };

	Interface* Networking::Get()
	{
		static_assert(std::size(vtable) == 4);
		return &object;
	}

	bool Networking::SendP2PPacket([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID remote, [[maybe_unused]] const void* data,
		[[maybe_unused]] unsigned int size, [[maybe_unused]] int sendType, [[maybe_unused]] int channel)
	{
		return false;
	}

	bool Networking::IsP2PPacketAvailable([[maybe_unused]] Interface* self, [[maybe_unused]] unsigned int* size, [[maybe_unused]] int channel)
	{
		return false;
	}

	bool Networking::ReadP2PPacket([[maybe_unused]] Interface* self, [[maybe_unused]] void* buffer, [[maybe_unused]] unsigned int bufferSize,
		[[maybe_unused]] unsigned int* size, [[maybe_unused]] SteamID* remote, [[maybe_unused]] int channel)
	{
		return false;
	}

	bool Networking::AcceptP2PSessionWithUser([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID remote)
	{
		return false;
	}
}
