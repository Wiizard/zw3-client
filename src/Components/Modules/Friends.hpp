#pragma once

#include "Dvar.hpp"
#include "Network.hpp"

#include "Steam/Steam.hpp"

namespace Components
{
	class Friends : public Component
	{
	public:
		Friends();
		~Friends();

		static void UpdateFriends();
		static void UpdateRank();
		static void UpdateServer(const Network::Address& server, const std::string& hostname, const std::string& mapname);
		static void UpdateName();

		static void SetPresence(const std::string& key, const std::string& value);
		static void ClearPresence(const std::string& key);

		static void RequestPresence(::Steam::SteamID user);
		static std::string GetPresence(::Steam::SteamID user, const std::string& key);

		static int GetGame(::Steam::SteamID user);

		static bool IsInvisible();

		static bool TryGetZombieRankByGuid(const std::string& guid, int& level, int& prestige);
		static std::string GetLobbyPlayerRelationship(const std::string& guid);

		static void AuthorizeDiscordPartyJoin(const std::string& discordUserId, const std::string& partyId, std::function<void(std::optional<std::string>)> completion);

		static Dvar::Var ui_streamFriendly;
		static Dvar::Var cl_anonymous;
		static Dvar::Var cl_notifyFriendState;

	private:
		struct FriendRichPresenceUpdate
		{
			::Steam::SteamID m_steamIDFriend;
			std::int32_t m_nAppID;
		};

		struct PersonaStateChange
		{
			::Steam::SteamID m_ulSteamID;
			int m_nChangeFlags;
		};

		struct Friend
		{
			::Steam::SteamID userId;
			::Steam::SteamID guid;
			std::string name;
			std::string playerName;
			std::string cleanName;
			Network::Address server;
			std::string serverName;
			std::string mapname;
			bool online;
			unsigned int lastTime;
			int experience;
			int prestige;
			bool isZombieRankKnown = false;
			int zombieRankLevel = 1;
			int zombieRankPrestige = 0;
		};

		static bool isLoggedOn;
		static bool shouldSort;
		static bool shouldUpdate;
		static int initialState;
		static unsigned int currentFriend;
		static std::recursive_mutex mutex;
		static std::vector<Friend> friendsList;

		static void ClearServer();
		static void SetServer();
		static void CG_ParseServerInfo_Hk(int localClientNum);

		static bool IsClientInParty(int controller, int clientNum);

		static void UpdateUserInfo(::Steam::SteamID user);
		static void UpdateState();
		static void UpdateZombieRankPresence();

		static void SortList(bool force = false);
		static void SortIndividualList(std::vector<Friend>* list);

		static unsigned int GetFriendCount();
		static const char* GetFriendText(unsigned int index, int column);
		static void SelectFriend(unsigned int index);

		static void UpdateTimeStamp();

		static bool IsOnline(std::uint64_t timeStamp);

		static void StoreFriendsList();

		static void SetRawPresence(const char* key, const char* value);

		static Game::Material* CreateAvatar(::Steam::SteamID user);

		static bool TryInstallHooks();
	};
}
