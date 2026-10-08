#pragma once

namespace Steam
{
	class Networking
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static bool SendP2PPacket(Interface* self, SteamID remote, const void* data, unsigned int size, int sendType, int channel);
		static bool IsP2PPacketAvailable(Interface* self, unsigned int* size, int channel);
		static bool ReadP2PPacket(Interface* self, void* buffer, unsigned int bufferSize, unsigned int* size, SteamID* remote, int channel);
		static bool AcceptP2PSessionWithUser(Interface* self, SteamID remote);
	};
}
