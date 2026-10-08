#pragma once

namespace Steam
{
	class GameServer
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static void LogOff(Interface* self);
		static bool BSecure(Interface* self);
		static SteamID* GetSteamID(Interface* self, SteamID* result);
		static bool SendUserConnectAndAuthenticate(Interface* self, unsigned int clientIp, const void* authBlob, unsigned int authBlobSize, SteamID* user);
		static void SendUserDisconnect(Interface* self, SteamID user);
		static int UserHasLicenseForApp(Interface* self, SteamID user, unsigned int appId);
	};
}
