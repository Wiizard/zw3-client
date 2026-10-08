#pragma once

#include "Dvar.hpp"
#include "Network.hpp"

#include <Steam/Steam.hpp>

namespace Components
{
	class Party : public Component
	{
	public:
		Party();

		static bool IsInLobby();
		static bool IsInUserMapLobby();
		static bool IsEnabled();

		static bool IsHostingParty();
		static bool IsPrivateMatchClient();

		static void Connect(const Network::Address& target, bool downloadOnly = false, bool isUnmanagedRequired = false);

		static std::uint64_t GetLocalPlayerXuid();

		static Network::Address Target();

		static std::string GetHostName();
		static std::string GetMotd();
		static int GetMaxClients();

		static void ConnectError(const std::string& message);

		static bool HandleJoinResponse(const Network::Address& address, const Utils::InfoString& info);

		static const char* GetLobbyInfo(::Steam::SteamID lobby, const std::string& key);
		static void RemoveLobby(::Steam::SteamID lobby);

		static bool PlaylistAwaiting();
		static void PlaylistContinue();
		static void PlaylistError(const std::string& error);

	private:
		struct JoinContainer
		{
			Network::Address target;
			std::string challenge;
			int startTime;
			int requestTime;
			bool isValid;
			bool isAwaitingPlaylist;
			bool isDownloadOnly;
			bool isUnmanagedRequired;
			Utils::InfoString info;
			std::string motd;
		};

		static JoinContainer joinContainer;
		static std::map<std::uint64_t, Network::Address> lobbyMap;

		static Dvar::Var party_enable;

		static void HandleGetInfo(Network::Address& address, const std::string& data);
		static ::Steam::SteamID GenerateLobbyId();
	};
}
