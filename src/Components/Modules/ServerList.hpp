#pragma once

#include "Network.hpp"
#include "UIScript.hpp"
#include "Dvar.hpp"

namespace Components
{
	class ServerList : public Component
	{
	public:
		struct ServerInfo
		{
			Network::Address addr;
			std::string hostname;
			std::string mapname;
			std::string gametype;
			std::string mod;
			std::string version;
			std::size_t hash;
			int clients;
			int bots;
			int maxClients;
			bool password;
			int ping;
			int matchType;
			int securityLevel;
			int protocol;
			bool hardcore;
			bool svRunning;
			bool aimassist;
			bool voice;
			std::time_t lastSeen;
		};

		ServerList();

		static void Refresh();
		static void RefreshVisibleList(const UIScript::Token& token);
		static void RefreshVisibleListInternal();
		static void UpdateVisibleList(const UIScript::Token& token);
		static void InsertRequest(const Network::Address& address);
		static void Insert(const Network::Address& address, const Utils::InfoString& info);

		static ServerInfo* GetCurrentServer();

		static bool IsFavouriteList();
		static bool IsOfflineList();
		static bool IsOnlineList();

		static void StoreFavourite(const std::string& server);
		static void RemoveFavourite(const std::string& server);
		static void LoadFavourites();

		static void Frame();
		static std::vector<ServerInfo>* GetList();

		static void UpdateVisibleInfo();

		static void FetchMasterList();

		static bool useMasterServer;

		static Dvar::Var netServerQueryLimit;
		static Dvar::Var netServerFrames;

	private:
		enum class Column : int
		{
			Password,
			Matchtype,
			AimAssist,
			VoiceChat,
			Hostname,
			Mapname,
			Players,
			Gametype,
			Mod,
			Ping,

			Count
		};

		class Container
		{
		public:
			class ServerContainer
			{
			public:
				bool sent;
				int sendTime;
				std::string challenge;
				Network::Address target;
				int sourceList;
			};

			bool needsInitialRefresh;
			std::vector<ServerContainer> servers;
			std::recursive_mutex mutex;
		};

		static unsigned int GetServerCount();
		static const char* GetServerText(unsigned int index, int column);
		static const char* GetServerInfoText(ServerInfo* server, int column, bool sorting = false);
		static void SelectServer(unsigned int index);

		static void UpdateSource();
		static void UpdateGameType();

		static void SortList();

		static void RemoveDeadServers();
		static void HeartbeatServers();

		static void LoadServerCache();
		static void SaveServerCache();

		static ServerInfo* GetServer(unsigned int index);
		static bool IsServerDuplicate(const std::vector<ServerInfo>* list, const ServerInfo& server);

		static bool IsServerListOpen();

		static int sortKey;
		static bool sortAsc;

		static unsigned int currentServer;
		static Container refreshContainer;

		static std::vector<ServerInfo> onlineList;
		static std::vector<ServerInfo> offlineList;
		static std::vector<ServerInfo> favouriteList;

		static std::vector<unsigned int> visibleList;

		static Dvar::Var uiServerSelected;
		static Dvar::Var uiServerSelectedMap;
		static Dvar::Var netServerDeadTimeout;
	};
}

template <>
struct std::hash<Components::ServerList::ServerInfo>
{
	std::size_t operator()(const Components::ServerList::ServerInfo& server) const noexcept
	{
		std::size_t hash = 0;

		hash ^= std::hash<std::string>()(server.hostname);
		hash ^= std::hash<std::string>()(server.mapname);
		hash ^= std::hash<std::string>()(server.mod);
		hash ^= std::hash<std::uint32_t>()(server.addr.GetIP());
		hash ^= server.clients;

		return hash;
	}
};
