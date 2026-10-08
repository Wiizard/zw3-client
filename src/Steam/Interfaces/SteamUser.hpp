#pragma once

namespace Steam
{
	class User
	{
	public:
		static Interface* Get();

		static SteamID LocalId();

	private:
		static void* const vtable[];
		static Interface object;

		static bool BLoggedOn(Interface* self);
		static SteamID* GetSteamID(Interface* self, SteamID* result);
		static int InitiateGameConnection(Interface* self, void* authBlob, int maxAuthBlob, SteamID gameServer, unsigned int serverIp, unsigned short serverPort, bool isSecure);
		static void TerminateGameConnection(Interface* self, unsigned int serverIp, unsigned short serverPort);
	};
}
