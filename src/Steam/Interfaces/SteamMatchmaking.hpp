#pragma once

namespace Steam
{
	class Matchmaking
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static std::uint64_t CreateLobby(Interface* self, int lobbyType, int maxMembers);
		static std::uint64_t JoinLobby(Interface* self, SteamID lobby);
		static void LeaveLobby(Interface* self, SteamID lobby);
		static int GetNumLobbyMembers(Interface* self, SteamID lobby);
		static SteamID* GetLobbyMemberByIndex(Interface* self, SteamID* result, SteamID lobby, int member);
		static const char* GetLobbyData(Interface* self, SteamID lobby, const char* key);
		static bool SetLobbyData(Interface* self, SteamID lobby, const char* key, const char* value);
		static void SetLobbyGameServer(Interface* self, SteamID lobby, unsigned int serverIp, unsigned short serverPort, SteamID server);
		static bool SetLobbyMemberLimit(Interface* self, SteamID lobby, int maxMembers);
		static bool SetLobbyType(Interface* self, SteamID lobby, int lobbyType);
		static SteamID* GetLobbyOwner(Interface* self, SteamID* result, SteamID lobby);
		static bool SetLobbyOwner(Interface* self, SteamID lobby, SteamID newOwner);
	};
}
