#include "STDInclude.hpp"

#include "ServerList.hpp"
#include "Command.hpp"
#include "Friends.hpp"
#include "Logger.hpp"
#include "Localization.hpp"
#include "Maps.hpp"
#include "Scheduler.hpp"
#include "UIFeeder.hpp"
#include "UIScript.hpp"
#include "ServerInfo.hpp"
#include "Party.hpp"
#include "Discovery.hpp"
#include "Node.hpp"
#include "TextRenderer.hpp"
#include "Toast.hpp"

namespace Components
{
	int ServerList::sortKey = static_cast<std::underlying_type_t<Column>>(Column::Players);
	bool ServerList::sortAsc = false;

	unsigned int ServerList::currentServer = 0;
	ServerList::Container ServerList::refreshContainer;

	std::vector<ServerList::ServerInfo> ServerList::onlineList;
	std::vector<ServerList::ServerInfo> ServerList::offlineList;
	std::vector<ServerList::ServerInfo> ServerList::favouriteList;

	std::vector<unsigned int> ServerList::visibleList;


	bool ServerList::useMasterServer = false;

	Dvar::Var ServerList::uiServerSelected;
	Dvar::Var ServerList::uiServerSelectedMap;
	Dvar::Var ServerList::netServerQueryLimit;
	Dvar::Var ServerList::netServerFrames;
	Dvar::Var ServerList::netServerDeadTimeout;

	static const Utils::Hook::LeaSite masterServerNameDefaultLea = { 0x1401F4D11, Utils::Hook::leaRdx, 0x14038C4B8 };

	constexpr std::uintptr_t Com_InitDvars_MasterServerNameFlags = 0x1401F4D0D;
	constexpr std::uintptr_t Com_InitDvars_MasterPortFlags = 0x1401F4D71;

	static const std::uint8_t masterServerNameFlags[] = { 0x44, 0x8D, 0x47, 0x04 };
	static const std::uint8_t masterPortFlags[] = { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t UI_OwnerDrawWidth_NetSourceReset = 0x140270A5C;
	constexpr std::uintptr_t Dvar_SetInt = 0x140287670;

	constexpr float serversFeeder = 2.0f;

	constexpr const char* favouriteFile = "zw3/players/favourites.json";

	constexpr int netSourceOwnerDraw = 220;
	constexpr int joinGametypeOwnerDraw = 253;

	constexpr int clientLimit = 18;

	constexpr unsigned short firstPort = 28960;

	constexpr auto heartbeatInterval = std::chrono::seconds(30);
	constexpr auto deadCheckInterval = std::chrono::seconds(30);
	constexpr auto cacheSaveInterval = std::chrono::seconds(30);

	static std::string GetZombieModeName(const std::string& zombieMode)
	{
		if (zombieMode.empty() || zombieMode == "0")
		{
			return "Normal";
		}

		if (zombieMode == "1")
		{
			return "Classic";
		}

		if (zombieMode == "2")
		{
			return "Hardcore";
		}

		return {};
	}

	static int GetNetSource()
	{
		const Game::dvar_t* const dvar = *Game::ui_netSource;

		if (!dvar)
		{
			return -1;
		}

		return dvar->current.integer;
	}

	std::vector<ServerList::ServerInfo>* ServerList::GetList()
	{
		if (IsOnlineList())
		{
			return &onlineList;
		}

		if (IsOfflineList())
		{
			return &offlineList;
		}

		if (IsFavouriteList())
		{
			return &favouriteList;
		}

		return nullptr;
	}

	bool ServerList::IsFavouriteList()
	{
		return GetNetSource() == 2;
	}

	bool ServerList::IsOfflineList()
	{
		return GetNetSource() == 0;
	}

	bool ServerList::IsOnlineList()
	{
		return GetNetSource() == 1;
	}

	unsigned int ServerList::GetServerCount()
	{
		return static_cast<unsigned int>(visibleList.size());
	}

	const char* ServerList::GetServerText(unsigned int index, int column)
	{
		auto* const server = GetServer(index);

		if (!server)
		{
			return "";
		}

		return GetServerInfoText(server, column);
	}

	const char* ServerList::GetServerInfoText(ServerInfo* server, int column, bool sorting)
	{
		if (!server)
		{
			return "";
		}

		switch (static_cast<Column>(column))
		{
		case Column::Password:
		{
			if (server->password)
			{
				return ":icon_locked:";
			}

			return "";
		}

		case Column::Matchtype:
		{
			if (server->matchType == 1)
			{
				return "P";
			}

			return "M";
		}

		case Column::AimAssist:
		{
			if (server->aimassist)
			{
				return ":headshot:";
			}

			return "";
		}

		case Column::VoiceChat:
		{
			if (server->voice)
			{
				return ":voice_on:";
			}

			return "";
		}

		case Column::Hostname:
		{
			return server->hostname.data();
		}

		case Column::Mapname:
		{
			if (server->svRunning)
			{
				if (!sorting && !Maps::CheckMapInstalled(server->mapname))
				{
					return Utils::String::VA("^1%s", Localization::LocalizeMapName(server->mapname.data()));
				}

				return Localization::LocalizeMapName(server->mapname.data());
			}

			return Utils::String::VA("^3%s", Localization::LocalizeMapName(server->mapname.data()));
		}

		case Column::Players:
		{
			return Utils::String::VA("%i/%i (%i)", server->clients, server->maxClients, server->bots);
		}

		case Column::Gametype:
		{
			return Game::UI_GetGameTypeDisplayName(server->gametype.data());
		}

		case Column::Mod:
		{
			if (Utils::String::StartsWith(server->mod, "mods/"))
			{
				return server->mod.data() + 5;
			}

			return "";
		}

		case Column::Ping:
		{
			if (server->ping < 75)
			{
				return Utils::String::VA("^2%i", server->ping);
			}

			if (server->ping < 150)
			{
				return Utils::String::VA("^3%i", server->ping);
			}

			return Utils::String::VA("^1%i", server->ping);
		}

		default:
		{
			break;
		}
		}

		return "";
	}

	void ServerList::SelectServer(unsigned int index)
	{
		currentServer = index;

		const auto* const server = GetCurrentServer();

		if (!server)
		{
			uiServerSelected.Set(false);
			return;
		}

		uiServerSelected.Set(true);
		uiServerSelectedMap.Set(server->mapname);
		Dvar::Find("ui_serverSelectedGametype").Set(server->gametype);
	}

	void ServerList::UpdateVisibleList([[maybe_unused]] const UIScript::Token& token)
	{
		auto* const list = GetList();

		if (!list)
		{
			return;
		}

		const std::vector snapshot(*list);

		if (snapshot.empty())
		{
			Refresh();
		}
		else
		{
			for (const auto& server : snapshot)
			{
				InsertRequest(server.addr);
			}
		}

		Toast::Show("cardicon_headshot", "Server Browser", "Servers refreshed", 3000);
	}

	void ServerList::RefreshVisibleList([[maybe_unused]] const UIScript::Token& token)
	{
		Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
	}

	void ServerList::RefreshVisibleListInternal()
	{
		uiServerSelected.Set(false);

		visibleList.clear();

		auto* const list = GetList();

		if (!list)
		{
			return;
		}

		const auto showFull = Dvar::Find("ui_browserShowFull").Get<bool>();
		const auto showEmpty = Dvar::Find("ui_browserShowEmpty").Get<bool>();
		const auto showHardcore = Dvar::Find("ui_browserKillcam").Get<int>();
		const auto showPassword = Dvar::Find("ui_browserShowPassword").Get<int>();
		const auto showMod = Dvar::Find("ui_browserMod").Get<int>();
		const auto joinGametype = *Game::ui_joinGametype ? (*Game::ui_joinGametype)->current.integer : 0;

		for (unsigned int i = 0; i < list->size(); ++i)
		{
			const auto& server = (*list)[i];

			if (!showFull && server.clients >= server.maxClients)
			{
				continue;
			}

			if (!showEmpty && server.clients <= 0)
			{
				continue;
			}

			if ((showHardcore == 0 && server.hardcore) || (showHardcore == 1 && !server.hardcore))
			{
				continue;
			}

			if ((showPassword == 0 && server.password) || (showPassword == 1 && !server.password))
			{
				continue;
			}

			if ((showMod == 0 && !server.mod.empty()) || (showMod == 1 && server.mod.empty()))
			{
				continue;
			}

			if (joinGametype > 0 && (joinGametype - 1) < *Game::gameTypeCount
				&& Game::gameTypes[joinGametype - 1].gameType != server.gametype)
			{
				continue;
			}

			visibleList.push_back(i);
		}

		SortList();
	}


	static const char* const masterServerHost = "master.zw3.eu";

	static std::vector<std::string> SplitMasterEntries(const std::string& reply)
	{
		std::vector<std::string> entries;

		const auto arrayAt = reply.find("\"servers\"");

		if (arrayAt == std::string::npos)
		{
			return entries;
		}

		auto at = reply.find('[', arrayAt);

		if (at == std::string::npos)
		{
			return entries;
		}

		int depth = 0;
		std::size_t start = 0;
		bool isInString = false;

		for (; at < reply.size(); ++at)
		{
			const char current = reply[at];

			if (isInString)
			{
				if (current == '\\')
				{
					++at;
				}
				else if (current == '"')
				{
					isInString = false;
				}

				continue;
			}

			if (current == '"')
			{
				isInString = true;
				continue;
			}

			if (current == '{')
			{
				if (!depth)
				{
					start = at;
				}

				++depth;
				continue;
			}

			if (current == '}')
			{
				--depth;

				if (!depth)
				{
					entries.push_back(reply.substr(start, at - start + 1));
				}

				continue;
			}

			if (current == ']' && !depth)
			{
				break;
			}
		}

		return entries;
	}

	static std::string ReadMasterValue(const std::string& entry, const std::string& key)
	{
		const auto quoted = "\"" + key + "\"";
		const auto at = entry.find(quoted);

		if (at == std::string::npos)
		{
			return {};
		}

		auto colon = entry.find(':', at + quoted.size());

		if (colon == std::string::npos)
		{
			return {};
		}

		++colon;

		while (colon < entry.size() && std::isspace(static_cast<unsigned char>(entry[colon])))
		{
			++colon;
		}

		if (colon >= entry.size())
		{
			return {};
		}

		if (entry[colon] == '"')
		{
			const auto end = entry.find('"', colon + 1);

			if (end == std::string::npos)
			{
				return {};
			}

			return entry.substr(colon + 1, end - colon - 1);
		}

		const auto end = entry.find_first_of(",}", colon);
		auto value = entry.substr(colon, (end == std::string::npos ? entry.size() : end) - colon);

		Utils::String::Trim(value);

		return value;
	}

	constexpr const char* serverCacheFile = "zw3/players/server_cache.json";

	void ServerList::LoadServerCache()
	{
		const auto cache = Utils::IO::ReadFile(serverCacheFile);

		if (cache.empty())
		{
			return;
		}

		nlohmann::json root;

		try
		{
			root = nlohmann::json::parse(cache);
		}
		catch (const nlohmann::json::parse_error& ex)
		{
			Logger::Error("JSON parse error in server cache: {}\n", ex.what());
			return;
		}

		if (!root.is_object() || !root.contains("servers") || !root["servers"].is_array())
		{
			Logger::Print("server cache file is invalid\n");
			return;
		}

		const auto& servers = root["servers"];
		auto* const list = &onlineList;

		Logger::Print("loading {} cached servers...\n", servers.size());

		for (const auto& entry : servers)
		{
			if (!entry.is_object())
			{
				continue;
			}

			try
			{
				ServerInfo server{};

				server.addr = Network::Address(entry.value("address", std::string()));
				server.hostname = entry.value("hostname", std::string());
				server.mapname = entry.value("mapname", std::string());
				server.gametype = entry.value("gametype", std::string());
				server.mod = entry.value("mod", std::string());
				server.version = entry.value("version", std::string());
				server.clients = entry.value("clients", 0);
				server.bots = entry.value("bots", 0);
				server.maxClients = entry.value("maxClients", 0);
				server.password = entry.value("password", false);
				server.ping = entry.value("ping", 999);
				server.matchType = entry.value("matchType", 0);
				server.securityLevel = entry.value("securityLevel", 0);
				server.protocol = entry.value("protocol", Components::ServerInfo::GetProtocol());
				server.hardcore = entry.value("hardcore", false);
				server.svRunning = entry.value("svRunning", false);
				server.aimassist = entry.value("aimassist", false);
				server.voice = entry.value("voice", false);
				server.lastSeen = entry.value("lastSeen", std::time(nullptr));
				server.hash = std::hash<ServerInfo>()(server);

				if (!IsServerDuplicate(list, server))
				{
					list->push_back(server);
				}
			}
			catch (const std::exception& ex)
			{
				Logger::Error("error loading cached server: {}\n", ex.what());
			}
		}

		Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);

		Logger::Print("loaded {} servers from cache\n", list->size());
	}

	void ServerList::SaveServerCache()
	{
		if (!IsOnlineList())
		{
			return;
		}

		const auto* const list = GetList();

		if (!list || list->empty())
		{
			return;
		}

		nlohmann::json::array_t servers;

		for (const auto& server : *list)
		{
			nlohmann::json entry;

			entry["address"] = server.addr.GetString();
			entry["hostname"] = server.hostname;
			entry["mapname"] = server.mapname;
			entry["gametype"] = server.gametype;
			entry["mod"] = server.mod;
			entry["version"] = server.version;
			entry["clients"] = server.clients;
			entry["bots"] = server.bots;
			entry["maxClients"] = server.maxClients;
			entry["password"] = server.password;
			entry["ping"] = server.ping;
			entry["matchType"] = server.matchType;
			entry["securityLevel"] = server.securityLevel;
			entry["protocol"] = server.protocol;
			entry["hardcore"] = server.hardcore;
			entry["svRunning"] = server.svRunning;
			entry["aimassist"] = server.aimassist;
			entry["voice"] = server.voice;
			entry["lastSeen"] = server.lastSeen;

			servers.push_back(entry);
		}

		nlohmann::json root;
		root["servers"] = servers;
		root["timestamp"] = std::time(nullptr);

		Utils::IO::WriteFile(serverCacheFile, root.dump());

		Logger::Print("saved {} servers to cache\n", servers.size());
	}

	void ServerList::FetchMasterList()
	{
		const auto protocol = Components::ServerInfo::GetProtocol();
		const auto url = std::format("http://{}/v1/servers/zw3?protocol={}", masterServerHost, protocol);

		std::thread([url, protocol]
		{
			const auto reply = Utils::WebIO("zw3", url).SetTimeout(5000)->Get();

			Scheduler::Once([reply, url, protocol]
			{
				if (reply.empty())
				{
					Logger::Print("master: no answer from {}\n", url);
					Toast::Show("cardicon_redhand", "^1Error", "Could not get a response.\n", 5000);

					useMasterServer = false;
					return;
				}

				std::size_t queued = 0;

				for (const auto& entry : SplitMasterEntries(reply))
				{
					const auto ip = ReadMasterValue(entry, "ip");
					const auto port = ReadMasterValue(entry, "port");

					if (ip.empty() || port.empty())
					{
						continue;
					}

					if (std::strtol(ReadMasterValue(entry, "protocol").data(), nullptr, 10) != protocol)
					{
						continue;
					}

					Network::Address server(std::format("{}:{}", ip, port));
					server.SetType(Game::NA_IP);

					InsertRequest(server);
					++queued;
				}

				if (!queued)
				{
					useMasterServer = false;
					Logger::Print("master: no servers in the answer from {}\n", url);
					return;
				}

				useMasterServer = true;
				Logger::Print("master: {} servers queued for query\n", queued);
			}, Scheduler::Pipeline::CLIENT);
		}).detach();
	}

	void ServerList::Refresh()
	{
		uiServerSelected.Set(false);

		auto* const list = GetList();

		const bool hasCachedServers = list && !list->empty();

		if (!hasCachedServers && IsOnlineList())
		{
			visibleList.clear();
		}

		{
			std::lock_guard _(refreshContainer.mutex);
			refreshContainer.servers.clear();
			refreshContainer.needsInitialRefresh = true;
		}

		if (IsOfflineList())
		{
			Discovery::Perform();

			Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
			return;
		}

		if (IsOnlineList())
		{
			if (!hasCachedServers)
			{
				Toast::Show("cardicon_redhand", "Fetching Servers", "This may take some time. Please wait...", 3000);
			}

			FetchMasterList();

			if (hasCachedServers)
			{
				for (const auto& server : *list)
				{
					InsertRequest(server.addr);
				}
			}

			Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
			return;
		}

		if (IsFavouriteList())
		{
			LoadFavourites();

			Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
		}
	}

	void ServerList::StoreFavourite(const std::string& server)
	{
		std::vector<std::string> servers;

		const auto parseData = Utils::IO::ReadFile(favouriteFile);

		if (!parseData.empty())
		{
			nlohmann::json object;

			try
			{
				object = nlohmann::json::parse(parseData);
			}
			catch (const nlohmann::json::parse_error& ex)
			{
				Logger::Error("JSON Parse Error: {}\n", ex.what());
				return;
			}

			if (!object.is_array())
			{
				Logger::Print("Favourites storage file is invalid!\n");
				Game::ShowMessageBox("Favourites storage file is invalid!", "Error");
				return;
			}

			const nlohmann::json::array_t storedServers = object;

			for (const auto& storedServer : storedServers)
			{
				if (!storedServer.is_string())
				{
					continue;
				}

				if (storedServer.get<std::string>() == server)
				{
					Game::ShowMessageBox("Server already marked as favourite.", "Error");
					return;
				}

				servers.push_back(storedServer.get<std::string>());
			}
		}

		servers.push_back(server);

		const auto data = nlohmann::json(servers);
		Utils::IO::WriteFile(favouriteFile, data.dump());
		Game::ShowMessageBox("Server added to favourites.", "Success");
	}

	void ServerList::RemoveFavourite(const std::string& server)
	{
		std::vector<std::string> servers;

		const auto parseData = Utils::IO::ReadFile(favouriteFile);

		if (!parseData.empty())
		{
			nlohmann::json object;

			try
			{
				object = nlohmann::json::parse(parseData);
			}
			catch (const nlohmann::json::parse_error& ex)
			{
				Logger::Error("JSON Parse Error: {}\n", ex.what());
				return;
			}

			if (!object.is_array())
			{
				Logger::Print("Favourites storage file is invalid!\n");
				Game::ShowMessageBox("Favourites storage file is invalid!", "Error");
				return;
			}

			const nlohmann::json::array_t storedServers = object;

			for (const auto& storedServer : storedServers)
			{
				if (storedServer.is_string() && storedServer.get<std::string>() != server)
				{
					servers.push_back(storedServer.get<std::string>());
				}
			}
		}

		const auto data = nlohmann::json(servers);
		Utils::IO::WriteFile(favouriteFile, data.dump());

		auto* const list = GetList();

		if (list)
		{
			list->clear();
		}

		Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
	}

	void ServerList::LoadFavourites()
	{
		if (!IsFavouriteList())
		{
			return;
		}

		auto* const list = GetList();

		if (list)
		{
			list->clear();
		}

		const auto parseData = Utils::IO::ReadFile(favouriteFile);

		if (parseData.empty())
		{
			return;
		}

		nlohmann::json object;

		try
		{
			object = nlohmann::json::parse(parseData);
		}
		catch (const nlohmann::json::parse_error& ex)
		{
			Logger::Error("JSON Parse Error: {}\n", ex.what());
			return;
		}

		if (!object.is_array())
		{
			Logger::Print("Favourites storage file is invalid!\n");
			Game::ShowMessageBox("Favourites storage file is invalid!", "Error");
			return;
		}

		const nlohmann::json::array_t servers = object;

		for (const auto& server : servers)
		{
			if (!server.is_string())
			{
				continue;
			}

			InsertRequest(Network::Address(server.get<std::string>()));
		}
	}

	void ServerList::InsertRequest(const Network::Address& address)
	{
		std::lock_guard _(refreshContainer.mutex);

		for (const auto& queued : refreshContainer.servers)
		{
			if (queued.target == address)
			{
				return;
			}
		}

		Container::ServerContainer request;
		request.sent = false;
		request.sendTime = 0;
		request.target = address;
		request.sourceList = GetNetSource();

		refreshContainer.servers.push_back(request);
	}

	void ServerList::Insert(const Network::Address& address, const Utils::InfoString& info)
	{
		std::lock_guard _(refreshContainer.mutex);

		for (auto i = refreshContainer.servers.begin(); i != refreshContainer.servers.end();)
		{
			if (i->target != address || !i->sent)
			{
				++i;
				continue;
			}

			if (i->challenge != info.Get("challenge"))
			{
				break;
			}

			ServerInfo server{};
			server.hostname = info.Get("hostname");
			server.mapname = info.Get("mapname");
			server.gametype = GetZombieModeName(info.Get("zombiemode"));
			server.version = info.Get("version");
			server.mod = info.Get("fs_game");
			server.matchType = std::strtol(info.Get("matchtype").data(), nullptr, 10);
			server.clients = std::strtol(info.Get("clients").data(), nullptr, 10);
			server.bots = std::strtol(info.Get("bots").data(), nullptr, 10);
			server.securityLevel = std::strtol(info.Get("securityLevel").data(), nullptr, 10);
			server.maxClients = std::strtol(info.Get("sv_maxclients").data(), nullptr, 10);
			server.protocol = std::strtol(info.Get("protocol").data(), nullptr, 10);
			server.password = info.Get("isPrivate") == "1";
			server.aimassist = info.Get("aimAssist") == "1";
			server.voice = info.Get("voiceChat") == "1";
			server.hardcore = info.Get("hc") == "1";
			server.svRunning = info.Get("sv_running") == "1";
			server.ping = Game::Sys_Milliseconds() - i->sendTime;
			server.addr = address;
			server.lastSeen = std::time(nullptr);

			server.hash = std::hash<ServerInfo>()(server);

			server.hostname = TextRenderer::StripMaterialTextIcons(server.hostname);

			const bool isBlankHostname = std::all_of(server.hostname.begin(), server.hostname.end(), [](const char letter)
			{
				return std::isspace(static_cast<unsigned char>(letter)) != 0;
			});

			if (server.hostname.empty() || isBlankHostname)
			{
				return;
			}

			server.mapname = TextRenderer::StripMaterialTextIcons(server.mapname);
			server.gametype = TextRenderer::StripMaterialTextIcons(server.gametype);
			server.mod = TextRenderer::StripMaterialTextIcons(server.mod);

			std::vector<ServerInfo>* target = nullptr;

			const auto sourceList = i->sourceList;

			switch (sourceList)
			{
			case 0:
				target = &offlineList;
				break;
			case 1:
				target = &onlineList;
				break;
			case 2:
				target = &favouriteList;
				break;
			default:
				return;
			}

			refreshContainer.servers.erase(i);

			if (info.Get("zwnet_show_in_server_browser") == "0")
			{
				std::erase_if(*target, [&address](const ServerInfo& known)
				{
					return known.addr == address;
				});

				if (sourceList == GetNetSource())
				{
					Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
				}

				return;
			}

			if (server.clients > clientLimit || server.maxClients > clientLimit)
			{
				return;
			}

			bool found = false;

			for (auto& known : *target)
			{
				if (known.addr == address)
				{
					known = server;
					found = true;
					break;
				}
			}

			if (info.Get("gamename") != "IW4" || !server.matchType)
			{
				return;
			}

			if (!found && !IsServerDuplicate(target, server))
			{
				target->push_back(server);
			}

			if (refreshContainer.needsInitialRefresh && sourceList == GetNetSource())
			{
				Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
			}

			return;
		}
	}

	bool ServerList::IsServerDuplicate(const std::vector<ServerInfo>* list, const ServerInfo& server)
	{
		for (const auto& known : *list)
		{
			if (known.hash == server.hash)
			{
				return true;
			}
		}

		return false;
	}

	ServerList::ServerInfo* ServerList::GetCurrentServer()
	{
		return GetServer(currentServer);
	}

	ServerList::ServerInfo* ServerList::GetServer(unsigned int index)
	{
		if (visibleList.size() <= index)
		{
			return nullptr;
		}

		auto* const list = GetList();

		if (!list || list->size() <= visibleList[index])
		{
			return nullptr;
		}

		return &(*list)[visibleList[index]];
	}

	void ServerList::SortList()
	{
		if (!IsServerListOpen())
		{
			return;
		}

		std::ranges::stable_sort(visibleList, [](const unsigned int first, const unsigned int second)
		{
			auto* const list = GetList();

			if (!list || list->size() <= first || list->size() <= second)
			{
				return false;
			}

			ServerInfo* const one = &(*list)[first];
			ServerInfo* const other = &(*list)[second];

			if (sortKey == static_cast<std::underlying_type_t<Column>>(Column::Ping))
			{
				return one->ping < other->ping;
			}

			if (sortKey == static_cast<std::underlying_type_t<Column>>(Column::Players))
			{
				return one->clients < other->clients;
			}

			const auto text = Utils::String::ToLower(TextRenderer::StripColors(GetServerInfoText(one, sortKey, true)));
			const auto otherText = Utils::String::ToLower(TextRenderer::StripColors(GetServerInfoText(other, sortKey, true)));

			return text.compare(otherText) < 0;
		});

		if (!sortAsc)
		{
			std::ranges::reverse(visibleList);
		}
	}

	void ServerList::RemoveDeadServers()
	{
		auto* const list = GetList();

		if (!list || list->empty())
		{
			return;
		}

		const std::time_t timeout = netServerDeadTimeout.Get<int>();

		if (timeout <= 0)
		{
			return;
		}

		const auto now = std::time(nullptr);

		std::size_t removed = 0;

		for (auto i = list->begin(); i != list->end();)
		{
			if ((now - i->lastSeen) > timeout)
			{
				i = list->erase(i);
				++removed;
			}
			else
			{
				++i;
			}
		}

		if (removed > 0)
		{
			Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
		}
	}

	void ServerList::HeartbeatServers()
	{
		auto* const list = GetList();

		if (!list || list->empty())
		{
			return;
		}

		for (const auto& server : *list)
		{
			InsertRequest(server.addr);
		}
	}

	void ServerList::Frame()
	{
		static auto lastQuery = std::chrono::steady_clock::now();
		static auto lastHeartbeat = std::chrono::steady_clock::now();
		static auto lastDeadCheck = std::chrono::steady_clock::now();
		static auto lastCacheSave = std::chrono::steady_clock::now();
		static bool wasOpen = false;

		if (!IsServerListOpen())
		{
			std::lock_guard _(refreshContainer.mutex);

			refreshContainer.servers.clear();
			wasOpen = false;
			return;
		}

		if (!wasOpen)
		{
			wasOpen = true;

			auto* const list = GetList();

			if (list)
			{
				const auto now = std::time(nullptr);

				for (auto& server : *list)
				{
					server.lastSeen = now;
				}
			}

			Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
		}

		const auto frames = std::max(1, netServerFrames.Get<int>());
		const auto interval = std::chrono::milliseconds(1000 / frames);
		const auto now = std::chrono::steady_clock::now();

		if ((now - lastQuery) < interval)
		{
			return;
		}

		lastQuery = now;

		if ((now - lastHeartbeat) > heartbeatInterval)
		{
			lastHeartbeat = now;

			if (IsOnlineList())
			{
				HeartbeatServers();
			}
		}

		if ((now - lastDeadCheck) > deadCheckInterval)
		{
			lastDeadCheck = now;

			if (IsOnlineList())
			{
				RemoveDeadServers();
			}
		}

		if ((now - lastCacheSave) > cacheSaveInterval)
		{
			lastCacheSave = now;
			SaveServerCache();
		}

		std::lock_guard _(refreshContainer.mutex);

		const auto challenge = Utils::Cryptography::Rand::GenerateChallenge();
		auto requestLimit = netServerQueryLimit.Get<int>();

		for (std::size_t i = 0; i < refreshContainer.servers.size() && requestLimit > 0; ++i)
		{
			auto& request = refreshContainer.servers[i];

			if (request.sent)
			{
				continue;
			}

			request.sent = true;
			request.sendTime = Game::Sys_Milliseconds();
			request.challenge = challenge;
			--requestLimit;

			Network::SendCommand(request.target, "getinfo", request.challenge);
		}

		if (refreshContainer.needsInitialRefresh && refreshContainer.servers.empty())
		{
			auto* const list = GetList();

			if (list && !list->empty())
			{
				refreshContainer.needsInitialRefresh = false;
				SaveServerCache();
			}
		}

		UpdateVisibleInfo();
	}

	void ServerList::UpdateSource()
	{
		auto source = GetNetSource();

		const Game::dvar_t* const dvar = *Game::ui_netSource;

		if (!dvar)
		{
			return;
		}

		if (++source > dvar->domain.integer.max)
		{
			source = 0;
		}

		Game::Dvar_SetInt(dvar, source);

		Scheduler::Once(Refresh, Scheduler::Pipeline::CLIENT);
	}

	void ServerList::UpdateGameType()
	{
		const Game::dvar_t* const dvar = *Game::ui_joinGametype;

		if (!dvar)
		{
			return;
		}

		auto gametype = dvar->current.integer;

		if (++gametype > *Game::gameTypeCount)
		{
			gametype = 0;
		}

		Game::Dvar_SetInt(dvar, gametype);

		Scheduler::Once(RefreshVisibleListInternal, Scheduler::Pipeline::CLIENT);
	}

	void ServerList::UpdateVisibleInfo()
	{
		static auto servers = 0;
		static auto players = 0;
		static auto bots = 0;

		const auto* const list = GetList();

		if (!list)
		{
			return;
		}

		auto newServers = static_cast<int>(list->size());
		auto newPlayers = 0;
		auto newBots = 0;

		for (const auto& server : *list)
		{
			newPlayers += server.clients;
			newBots += server.bots;
		}

		if (newServers == servers && newPlayers == players && newBots == bots)
		{
			return;
		}

		servers = newServers;
		players = newPlayers;
		bots = newBots;

		Localization::Set("MPUI_SERVERQUERIED",
			std::format("Servers: {}\nPlayers: {} ({})", servers, players, bots));
	}

	bool ServerList::IsServerListOpen()
	{
		Game::menuDef_t* const menu = Game::Menus_FindByName(Game::uiContext, "pc_join_unranked");

		if (!menu)
		{
			return false;
		}

		return Game::Menu_IsVisible(Game::uiContext, menu);
	}

	ServerList::ServerList()
	{
		Localization::Set("MPUI_SERVERQUERIED", "Servers: 0\nPlayers: 0 (0)");

		Scheduler::Once([]
		{
			uiServerSelected = Dvar::Register("ui_serverSelected", false,
				Game::DVAR_NONE, "Whether a server has been selected in the serverlist");
			uiServerSelectedMap = Dvar::Register("ui_serverSelectedMap", "mp_afghan",
				Game::DVAR_NONE, "Map of the selected server");
			Dvar::Register("ui_serverSelectedGametype", "war",
				Game::DVAR_NONE, "Gametype of the selected server");

			netServerQueryLimit = Dvar::Register("net_serverQueryLimit", 1,
				1, 10, Game::DVAR_ARCHIVE, "Amount of server queries per frame");
			netServerFrames = Dvar::Register("net_serverFrames", 30,
				1, 60, Game::DVAR_ARCHIVE, "Amount of server query frames per second");
			netServerDeadTimeout = Dvar::Register("net_serverDeadTimeout", 60,
				1, 604800, Game::DVAR_ARCHIVE, "Seconds after which unresponsive servers are removed from cache");
		}, Scheduler::Pipeline::MAIN);

		if (Utils::Hook::BranchesTo(UI_OwnerDrawWidth_NetSourceReset, Dvar_SetInt, HOOK_CALL))
		{
			Utils::Hook::Nop(UI_OwnerDrawWidth_NetSourceReset, 5);
		}
		else
		{
			Logger::Error("serverlist: UI_OwnerDrawWidth does not read as expected, the netsource still resets\n");
		}

		Network::OnPacket("infoResponse", [](Network::Address& address, const std::string& data)
		{
			const Utils::InfoString info(data);

			if (Party::HandleJoinResponse(address, info))
			{
				return;
			}

			Insert(address, info);
			Friends::UpdateServer(address, info.Get("hostname"), info.Get("mapname"));
		});

		const bool isMasterExpected = Utils::Hook::IsLeaIntact(masterServerNameDefaultLea)
			&& Utils::Hook::MatchesBytes(Com_InitDvars_MasterServerNameFlags, masterServerNameFlags, sizeof(masterServerNameFlags))
			&& Utils::Hook::MatchesBytes(Com_InitDvars_MasterPortFlags, masterPortFlags, sizeof(masterPortFlags));

		if (!isMasterExpected || !Utils::Hook::TryPointLeaAt(masterServerNameDefaultLea, Utils::Hook::PlaceNearImage("master.zw3.eu")))
		{
			Logger::Error("serverlist: masterServerName's registration does not read as expected, it stays the engine's\n");
		}
		else
		{
			Utils::Hook::Set<std::uint8_t>(Com_InitDvars_MasterServerNameFlags + 3, Game::DVAR_NONE);
			Utils::Hook::Set<std::uint32_t>(Com_InitDvars_MasterPortFlags + 4, Game::DVAR_NONE);
		}

		UIFeeder::Add(serversFeeder, GetServerCount, GetServerText, SelectServer);

		UIScript::Add("UpdateFilter", RefreshVisibleList);
		UIScript::Add("RefreshFilter", [](const UIScript::Token&)
		{
		});

		UIScript::Add("RefreshServers", [](const UIScript::Token&)
		{
			if (onlineList.empty())
			{
				LoadServerCache();
				Refresh();
				return;
			}

			Toast::Show("cardicon_headshot", "Server Browser", "Servers refreshed", 3000);
			Refresh();
		});

		UIScript::Add("ServerSort", [](const UIScript::Token& token)
		{
			const auto key = token.Get<int>();

			if (sortKey == key)
			{
				sortAsc = !sortAsc;
			}
			else
			{
				sortKey = key;
				sortAsc = true;
			}

			SortList();
		});

		UIScript::Add("JoinServer", [](const UIScript::Token&)
		{
			const auto* const server = GetCurrentServer();

			if (!server)
			{
				return;
			}

			Party::Connect(server->addr);
		});

		UIScript::Add("DownloadServerMod", [](const UIScript::Token&)
		{
			const auto* const server = GetCurrentServer();

			if (!server)
			{
				return;
			}

			Party::Connect(server->addr, true);
		});

		UIScript::Add("CreateListFavorite", [](const UIScript::Token&)
		{
			const auto* const server = GetCurrentServer();

			if (server && server->addr.IsValid())
			{
				StoreFavourite(server->addr.GetString());
			}
		});

		UIScript::Add("CreateFavorite", [](const UIScript::Token&)
		{
			const Dvar::Var favoriteAddress("ui_favoriteAddress");

			if (!favoriteAddress.IsValid())
			{
				return;
			}

			const auto value = favoriteAddress.Get<std::string>();

			if (!value.empty())
			{
				StoreFavourite(value);
			}
		});

		UIScript::Add("CreateCurrentServerFavorite", [](const UIScript::Token&)
		{
			if (!Game::CL_IsCgameInitialized(0))
			{
				return;
			}

			const auto addressText = Network::Address(Game::clc_serverAddress).GetString();

			if (addressText != "0.0.0.0:0" && addressText != "loopback")
			{
				StoreFavourite(addressText);
			}
		});

		UIScript::Add("DeleteFavorite", [](const UIScript::Token&)
		{
			const auto* const server = GetCurrentServer();

			if (server)
			{
				RemoveFavourite(server->addr.GetString());
			}
		});

		UIScript::AddOwnerDraw(netSourceOwnerDraw, UpdateSource);
		UIScript::AddOwnerDraw(joinGametypeOwnerDraw, UpdateGameType);

		Command::Add("addserver", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("usage: addserver <ip>[:port]\n");
				return;
			}

			Network::Address target(params->Get(1));

			if (!target.IsValid())
			{
				Logger::Print("could not parse {}\n", params->Get(1));
				return;
			}

			if (target.GetPort() == 0)
			{
				target.SetPort(firstPort);
			}

			InsertRequest(target);
			Logger::Print("queued {} for the server browser\n", target.GetString());
		});

		Scheduler::Loop(Frame, Scheduler::Pipeline::CLIENT);
	}
}
