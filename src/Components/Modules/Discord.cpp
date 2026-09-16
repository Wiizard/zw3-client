#include "Discord.hpp"
#include "Friends.hpp"
#include "Party.hpp"
#include "TextRenderer.hpp"
#include "ZWNet.hpp"

#include <algorithm>
#include <discord_rpc.h>
#include <Utils/WebIO.hpp>

namespace Components
{
	static DiscordRichPresence DiscordPresence;

	std::atomic_bool Discord::Initialized_;
	std::atomic_bool Discord::GameInitialized_;

	static std::recursive_mutex discordUpdateMutex;
	static unsigned int privateMatchNonce = 0;
	static int64_t discordSessionStart = 0;

	std::string hostIP = "";
	bool ipFetchInitiated = false;
	static bool ipFetchInProgress = false;
	static int64_t lastIpFetchAttempt = 0;

	static std::string lastDetails;
	static std::string lastState;
	static std::string lastPartyId;
	static std::string lastJoinSecret;
	static int lastPartySize = 0;
	static int lastPartyMax = 0;
	static int lastPartyPrivacy = -1;
	static int64_t lastUpdateTime = 0;

	static bool currentDiscordCanJoin = false;
	static bool lastCanJoinDiscordParty = false;
	static bool forcePresenceUpdate = false;
	static bool timestampResetPending = false;
	static unsigned long long presenceGeneration = 0;
	static std::atomic_bool discordJoinAuthorizationInFlight = false;
	static std::atomic_ullong discordConnectionGeneration = 0;
	static std::string discordJoinSecretOverride;
	static std::string discordJoinSecretOverridePartyId;
	static unsigned long long discordJoinSecretOverrideGeneration = 0;

	static void PublishDiscordPresence()
	{
		DiscordRichPresence presence{};
		presence.instance = 1;
		presence.largeImageKey = "https://i.imghippo.com/files/wbSr4660zUs.png";
		presence.startTimestamp = discordSessionStart;
		presence.details = lastDetails.empty() ? nullptr : lastDetails.c_str();
		presence.state = lastState.empty() ? nullptr : lastState.c_str();
		presence.partyId = lastPartyId.empty() ? nullptr : lastPartyId.c_str();
		presence.joinSecret = lastJoinSecret.empty() ? nullptr : lastJoinSecret.c_str();
		presence.partySize = lastPartySize;
		presence.partyMax = lastPartyMax;
		presence.partyPrivacy = lastPartyPrivacy;

		DiscordPresence = presence;
		Discord_UpdatePresence(&DiscordPresence);
		currentDiscordCanJoin = lastCanJoinDiscordParty;
	}

	static unsigned int GetDiscordNonce()
	{
		static auto nonce = Utils::Cryptography::Rand::GenerateInt();
		return nonce;
	}

	static const char* GetPartyPrivacyName(int privacy)
	{
		switch (privacy)
		{
		case 0:
			return "Open";
		case 1:
			return "Invite-Only";
		case 2:
			return "Closed";
		default:
			return "Open";
		}
	}

	bool Discord::IsZWNetPreGameState(const std::string& state)
	{
		return state == "SEARCH_STARTING"
			|| state == "SEARCHING"
			|| state == "MATCH_FOUND"
			|| state == "MAP_VOTE"
			|| state == "READY_CHECK"
			|| state == "WAITING_FOR_READY"
			|| state == "RESERVING_SERVER"
			|| state == "STARTING_SERVER"
			|| state == "SERVER_STARTING"
			|| state == "COUNTDOWN"
			|| state == "CONNECTING"
			|| state == "DIRECT_CONNECTION"
			|| state == "RELAY_CONNECTION";
	}

	static std::string GetZWNetPresenceState(const std::string& state, const int partySize, const int partyMax)
	{
		const auto currentPlayers = std::max(1, partySize);
		const auto maximumPlayers = std::max(currentPlayers, partyMax);

		std::string text;

		if (state == "SEARCH_STARTING")
			text = "Searching for available matches";
		else if (state == "SEARCHING")
			text = "Searching for a match";
		else if (state == "MATCH_FOUND")
			text = "Joining match lobby";
		else if (state == "MAP_VOTE")
			text = "Voting for the next map";
		else if (state == "READY_CHECK" || state == "WAITING_FOR_READY")
			text = "Waiting for all players to be ready";
		else if (state == "RESERVING_SERVER" || state == "STARTING_SERVER" || state == "SERVER_STARTING")
			text = "Setting up match";
		else if (state == "COUNTDOWN")
			text = "Starting match";
		else if (state == "CONNECTING")
			text = "Connecting to match";
		else if (state == "DIRECT_CONNECTION" || state == "RELAY_CONNECTION")
			text = "Joining match";
		else if (state == "IN_MATCH")
			text = "Playing matchmaking";
		else if (state == "ERROR")
			return "Unable to join game session";
		else
			text = "Idle";

		return Utils::String::Format("{} ({}/{})", text, currentPlayers, maximumPlayers);
	}

	static std::string GetLoadingMapDisplayName()
	{
		auto rawMapName = Dvar::Var("mapname").get<std::string>();

		if (rawMapName.empty())
			rawMapName = Dvar::Var("ui_mapname").get<std::string>();

		if (rawMapName.empty())
			return {};

		const auto* displayName = Game::UI_GetMapDisplayName(rawMapName.c_str());

		if (displayName && displayName[0])
			return displayName;

		return rawMapName;
	}

	static void Ready([[maybe_unused]] const DiscordUser* request)
	{
		ZeroMemory(&DiscordPresence, sizeof(DiscordPresence));
		DiscordPresence.instance = 1;
		Logger::Print("Discord: Ready\n");

		lastDetails.clear();
		lastState.clear();
		lastPartyId.clear();
		lastJoinSecret.clear();
		lastPartySize = -1;
		lastPartyMax = -1;
		lastPartyPrivacy = -1;
		lastCanJoinDiscordParty = false;
		lastUpdateTime = 0;
		discordSessionStart = 0;
		timestampResetPending = false;
		++presenceGeneration;
		currentDiscordCanJoin = false;
		discordJoinAuthorizationInFlight = false;
		discordJoinSecretOverride.clear();
		discordJoinSecretOverridePartyId.clear();
		++discordJoinSecretOverrideGeneration;
		++discordConnectionGeneration;

		forcePresenceUpdate = true;
	}

	void Discord::JoinGame(const char* joinSecret)
	{
		if (!Discord::GameInitialized_ || !joinSecret || !joinSecret[0])
			return;
		Logger::Print("Discord: Processing a join invitation\n");

		constexpr std::string_view zwnetCapabilityPrefix{"zwnet-cap:"};
		constexpr std::string_view zwnetPrefix{"zwnet:"};
		const std::string_view secret{joinSecret};
		if (secret.starts_with(zwnetCapabilityPrefix))
		{
			ZWNet::JoinCapability(std::string{
				secret.substr(zwnetCapabilityPrefix.size())});
			return;
		}
		if (secret.starts_with(zwnetPrefix))
		{
			ZWNet::JoinParty(std::string{secret.substr(zwnetPrefix.size())});
			return;
		}

		const char* connect_cmd = Utils::String::VA("connect %s\n", joinSecret);
		Game::Cbuf_AddText(0, connect_cmd);
	}

	static void Errored(const int errorCode, const char* message)
	{
		Logger::Print(Game::CON_CHANNEL_ERROR, "Discord: Error ({}): {}\n", errorCode, message);
	}

	static void FetchPublicIPAsync()
	{
		Utils::WebIO webio("zw3-get-host-ip");
		bool success = false;
		std::string ip = webio.get("https://api.ipify.org", &success);

		if (success && !ip.empty() && ip != "0.0.0.0")
		{
			hostIP = ip;
			forcePresenceUpdate = true;
		}
		else
		{
			hostIP.clear();
			ipFetchInitiated = false;
		}

		ipFetchInProgress = false;
	}

	const char* Discord::GetHostDiscordInviteIP()
	{
		if (!hostIP.empty() && hostIP != "0.0.0.0")
			return hostIP.c_str();

		const auto now = std::time(nullptr);

		if (!ipFetchInProgress && (!ipFetchInitiated || now - lastIpFetchAttempt >= 10))
		{
			ipFetchInitiated = true;
			ipFetchInProgress = true;
			lastIpFetchAttempt = now;
			std::thread(FetchPublicIPAsync).detach();
		}

		return "0.0.0.0";
	}

	struct ZWNetDiscordPartySnapshot
	{
		std::string id;
		std::string state;
		std::string visibility;
		int members{};
	};

	static std::optional<ZWNetDiscordPartySnapshot> GetZWNetDiscordPartySnapshot()
	{
		const auto* lobbyActive = Game::Dvar_FindVar("zwnet_lobby_active");
		const auto* partyId = Game::Dvar_FindVar("zwnet_lobby_party_id");
		const auto* state = Game::Dvar_FindVar("ui_zwnet_state");
		const auto* visibility = Game::Dvar_FindVar("zwnet_lobby_visibility");
		const auto* memberCount = Game::Dvar_FindVar("zwnet_lobby_member_count");
		if (!lobbyActive || !partyId || !state || !visibility || !memberCount ||
			lobbyActive->type != Game::DVAR_TYPE_BOOL ||
			!lobbyActive->current.enabled ||
			partyId->type != Game::DVAR_TYPE_STRING ||
			state->type != Game::DVAR_TYPE_STRING ||
			visibility->type != Game::DVAR_TYPE_STRING ||
			memberCount->type != Game::DVAR_TYPE_INT)
		{
			return std::nullopt;
		}
		ZWNetDiscordPartySnapshot snapshot;
		snapshot.id = partyId->current.string ? partyId->current.string : "";
		if (!snapshot.id.starts_with("pty_") || snapshot.id.size() > 80) return std::nullopt;
		snapshot.state = state->current.string ? state->current.string : "";
		snapshot.visibility = visibility->current.string
			? visibility->current.string
			: "";
		snapshot.members = std::clamp(
			memberCount->current.integer, 1, 4);
		return snapshot;
	}

	void Discord::JoinRequest(const DiscordUser* request)
	{
		if (!Initialized_ || !request || !request->userId || !request->userId[0]) return;
		if (!GameInitialized_)
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}
		Logger::Print("Discord: Received a join request\n");

		const auto snapshot = GetZWNetDiscordPartySnapshot();
		const auto advertisedZWNetParty = lastPartyId.starts_with("zwnet_");
		if (advertisedZWNetParty && (!snapshot
			|| lastPartyId != "zwnet_" + snapshot->id))
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}

		if (snapshot && snapshot->visibility == "INVITE_ONLY")
		{
			if (!currentDiscordCanJoin || snapshot->members >= 4
				|| discordJoinAuthorizationInFlight.exchange(true))
			{
				Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
				return;
			}

			const std::string userId{request->userId};
			const auto partyId = snapshot->id;
			const auto connectionGeneration = discordConnectionGeneration.load();
			Friends::AuthorizeDiscordPartyJoin(userId, partyId,
				[userId, partyId, connectionGeneration](
					std::optional<std::string> joinSecret)
				{
					if (discordConnectionGeneration.load() != connectionGeneration) return;
					std::lock_guard lock(discordUpdateMutex);
					if (!Initialized_ || !GameInitialized_)
					{
						discordJoinAuthorizationInFlight = false;
						return;
					}
					const auto current = GetZWNetDiscordPartySnapshot();
					const auto stillJoinable = joinSecret && current
						&& current->id == partyId
						&& current->visibility == "INVITE_ONLY"
						&& current->members < 4;
					if (!stillJoinable)
					{
						discordJoinAuthorizationInFlight = false;
						Discord_Respond(userId.c_str(), DISCORD_REPLY_NO);
						return;
					}

					discordJoinSecretOverride = std::move(*joinSecret);
					discordJoinSecretOverridePartyId = partyId;
					const auto overrideGeneration =
						++discordJoinSecretOverrideGeneration;
					forcePresenceUpdate = true;
					UpdateDiscord();
					Discord_Respond(userId.c_str(), DISCORD_REPLY_YES);

					Scheduler::Once([partyId, overrideGeneration]
					{
						std::lock_guard resetLock(discordUpdateMutex);
						if (overrideGeneration == discordJoinSecretOverrideGeneration &&
							discordJoinSecretOverridePartyId == partyId)
						{
							discordJoinSecretOverride.clear();
							discordJoinSecretOverridePartyId.clear();
							forcePresenceUpdate = true;
							discordJoinAuthorizationInFlight = false;
						}
					}, Scheduler::Pipeline::MAIN, 3s);
				});
			return;
		}

		Discord_Respond(request->userId, currentDiscordCanJoin
			? DISCORD_REPLY_YES
			: DISCORD_REPLY_IGNORE);
	}

	static void ApplyZWNetDiscordParty(const ZWNetDiscordPartySnapshot& snapshot,
		std::string& partyId, std::string& joinSecret, int& partySize,
		int& partyMax, int& partyPrivacy, bool& canJoin)
	{
		partyId = "zwnet_" + snapshot.id;
		partySize = snapshot.members;
		partyMax = 4;
		partyPrivacy = snapshot.visibility == "OPEN"
			? DISCORD_PARTY_PUBLIC
			: DISCORD_PARTY_PRIVATE;

		// Discord keeps invite-only parties private. Their opaque party id only
		// becomes usable after JoinRequest creates a receiver-specific backend
		// invitation; the backend still rechecks friendship, capacity and state.
		canJoin = snapshot.visibility != "CLOSED" && partySize < partyMax;
		joinSecret = canJoin ? "zwnet:" + snapshot.id : std::string{};
	}

	void Discord::UpdateDiscord()
	{
		std::lock_guard lock(discordUpdateMutex);

		if (!Initialized_)
			return;

		Discord_RunCallbacks();

		bool isInGame = false;
		bool isPrivateLobby = false;
		bool isPartyLobby = false;
		bool isZWNetMatchmaking = false;
		bool isConnectMenu = false;
		bool isServerList = false;
		bool isMainMenu = false;
		bool isHosting = false;
		bool isDedi = false;

		if (GameInitialized_)
		{
			isInGame = Game::CL_IsCgameInitialized();
			isPrivateLobby = Discord::IsPrivateMatchOpen();
			isPartyLobby = Discord::IsPartyLobbyOpen();
			isZWNetMatchmaking = Discord::IsZWNetMatchmakingOpen();
			isConnectMenu = Discord::IsConnectMenuOpen();
			isServerList = Discord::IsServerListOpen();
			isMainMenu = Discord::IsMainMenuOpen();
			isHosting = Dvar::Var("party_host").get<bool>();
			isDedi = Dvar::Var("sv_running").get<bool>();
		}

		DiscordRichPresence newPresence{};
		newPresence.instance = 1;
		newPresence.largeImageKey = "https://i.imghippo.com/files/wbSr4660zUs.png";

		std::string details;
		std::string state;
		std::string partyId;
		std::string joinSecret;
		int partySize = 0;
		int partyMax = 0;
		int partyPrivacy = DISCORD_PARTY_PUBLIC;
		bool canJoinDiscordParty = false;

		if (!GameInitialized_)
		{
			details = "Launching game";
			state.clear();
			currentDiscordCanJoin = false;
		}
		else if (isConnectMenu)
		{
			const auto mapName = GetLoadingMapDisplayName();
			state = mapName.empty()
				? "Preparing game..."
				: Utils::String::Format("Loading {}...", mapName);
			canJoinDiscordParty = false;

			if (const auto zwnetParty = GetZWNetDiscordPartySnapshot())
			{
				const auto* privacyName = zwnetParty->visibility == "OPEN"
					? "Open"
					: zwnetParty->visibility == "CLOSED" ? "Closed" : "Invite-Only";

				details = Utils::String::Format(
					"In pre-game lobby ({})", privacyName);
				ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret,
					partySize, partyMax, partyPrivacy, canJoinDiscordParty);
			}
			else
			{
				details = "Loading map";
			}
		}
		else if (!isInGame)
		{
			if (isServerList)
			{
				details = "Browsing servers";
				state = "";
			}
			else if (isZWNetMatchmaking)
			{
				if (const auto zwnetParty = GetZWNetDiscordPartySnapshot())
				{
					const auto* privacyName = zwnetParty->visibility == "OPEN"
						? "Open"
						: zwnetParty->visibility == "CLOSED" ? "Closed" : "Invite-Only";
					details = Discord::IsZWNetPreGameState(zwnetParty->state)
						? Utils::String::Format("In pre-game lobby ({})", privacyName)
						: Utils::String::Format("In a public party ({})", privacyName);
					ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret,
						partySize, partyMax, partyPrivacy, canJoinDiscordParty);
					state = GetZWNetPresenceState(
						zwnetParty->state, partySize, partyMax);
				}
				else
				{
					details = "Preparing ZW3 matchmaking";
					state.clear();
				}
			}
			else if (isMainMenu)
			{
				details = "At the main menu";
				state = "";
			}
			else if (isPrivateLobby || isPartyLobby)
			{
				int privacy = Dvar::Var("partyPrivacy").get<int>();
				if (privacy < 0 || privacy > 2)
					privacy = 0;

				const bool isOpen = privacy == 0;
				const bool isClosed = privacy == 2;
				const char* privacyName = GetPartyPrivacyName(privacy);

				partyPrivacy = isOpen ? DISCORD_PARTY_PUBLIC : DISCORD_PARTY_PRIVATE;
				details = isPrivateLobby
					? Utils::String::Format("In a private party ({})", privacyName)
					: Utils::String::Format("In a public party ({})", privacyName);

				int realPlayers = Dvar::Var("party_realPlayers").get<int>();
				int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
				int numBots = totalPlayers - realPlayers;

				partySize = realPlayers > 0 ? realPlayers : 1;
				partyMax = 4;

				if (isPartyLobby)
				{
					std::string raw = Dvar::Var("party_lobbyPlayerCount").get<std::string>();
					int lobbyRealPlayers = 0;
					int lobbyMaxPlayers = 0;
					sscanf(raw.c_str(), "%d/%d", &lobbyRealPlayers, &lobbyMaxPlayers);

					if (lobbyRealPlayers > 0)
						partySize = lobbyRealPlayers;

					if (lobbyMaxPlayers > 0)
						partyMax = lobbyMaxPlayers;
				}

				if (partySize < 1)
					partySize = 1;

				if (partyMax < partySize)
					partyMax = partySize;

				if (isHosting)
				{
					if (isPrivateLobby)
					{
						state = numBots > 0
							? Utils::String::Format("Setting up a private match (with {} bot{})", numBots, numBots == 1 ? "" : "s")
							: "Setting up a private match";
					}
					else
					{
						state = Utils::String::Format("Waiting for players ({}/{})", partySize, partyMax);
					}

					if (privateMatchNonce == 0)
						privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();

					const char* publicIp = Discord::GetHostDiscordInviteIP();
					if (!isClosed && std::strcmp(publicIp, "0.0.0.0") != 0)
					{
						joinSecret = Utils::String::VA("%s:28960", publicIp);
						partyId = Utils::String::VA("party_%s_%u", publicIp, privateMatchNonce);
						canJoinDiscordParty = true;
					}
					else
					{
						partyId = Utils::String::VA("party_pending_%u", privateMatchNonce);
						canJoinDiscordParty = false;
					}
				}
				else
				{
					state = isPrivateLobby
						? "Waiting for host to start a match"
						: Utils::String::Format("Waiting in party ({}/{})", partySize, partyMax);

					std::hash<Network::Address> hashFn;
					const auto address = Party::Target();
					partyId = Utils::String::VA("party_%zu_%u", hashFn(address), GetDiscordNonce());
				}
			}
		}
		else
		{
			if (const auto zwnetParty = GetZWNetDiscordPartySnapshot())
			{
				const auto* map = Game::UI_GetMapDisplayName(
					(*Game::ui_mapname)->current.string);
				details = Utils::String::Format("ZW3 matchmaking on {}", map);
				state = GetZWNetPresenceState(
					zwnetParty->state, zwnetParty->members, 4);
				ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret,
					partySize, partyMax, partyPrivacy, canJoinDiscordParty);
			}
			else
			{
			const auto* map = Game::UI_GetMapDisplayName((*Game::ui_mapname)->current.string);
			const int zModeVal = Dvar::Var("zombiemode").get<int>();
			static const char* zModeNames[] = { "Normal", "Classic", "Hardcore" };
			const char* zMode = (zModeVal >= 0 && zModeVal < 3) ? zModeNames[zModeVal] : "Normal";

			details = Utils::String::Format("{} on {}", zMode, map);

			if (isHosting)
			{
				const int privacy = Dvar::Var("partyPrivacy").get<int>();
				const bool isOpen = privacy == 0;
				const bool isClosed = privacy == 2;
				const char* privacyName = GetPartyPrivacyName(privacy);

				partyPrivacy = isOpen ? DISCORD_PARTY_PUBLIC : DISCORD_PARTY_PRIVATE;
				details += Utils::String::Format(" ({})", privacyName);

				int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
				int realPlayers = Dvar::Var("party_realPlayers").get<int>();
				int numBots = totalPlayers - realPlayers;

				state = numBots > 0
					? Utils::String::Format("In a private match (with {} bot{})", numBots, numBots == 1 ? "" : "s")
					: "In a private match";

				if (privateMatchNonce == 0)
					privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();

				const char* publicIp = Discord::GetHostDiscordInviteIP();
				if (!isClosed && std::strcmp(publicIp, "0.0.0.0") != 0)
				{
					joinSecret = Utils::String::VA("%s:28960", publicIp);
					partyId = Utils::String::VA("match_%s_%u", publicIp, privateMatchNonce);
					canJoinDiscordParty = true;
				}
				else
				{
					partyId = Utils::String::VA("match_pending_%u", privateMatchNonce);
					canJoinDiscordParty = false;
				}

				partySize = realPlayers > 0 ? realPlayers : 1;
				partyMax = 4;
			}
			else
			{
				char hostNameBuffer[256]{};
				TextRenderer::StripColors(Party::GetHostName().data(), hostNameBuffer, sizeof(hostNameBuffer));
				TextRenderer::StripAllTextIcons(hostNameBuffer, hostNameBuffer, sizeof(hostNameBuffer));

				state = hostNameBuffer;

				std::hash<Network::Address> hashFn;
				const auto address = Party::Target();
				partyId = Utils::String::VA("%s_%zu", hostNameBuffer, hashFn(address) ^ GetDiscordNonce());
				joinSecret = address.getCString();

				partySize = Game::cgArray[0].snap ? Game::cgArray[0].snap->numClients : 1;
				partyMax = Party::GetMaxClients();

				if (partySize < 1)
					partySize = 1;

				if (partyMax < partySize)
					partyMax = partySize;

				canJoinDiscordParty = !joinSecret.empty();
				partyPrivacy = DISCORD_PARTY_PUBLIC;
			}
			}
		}

		if (details.empty())
			details = "At the main menu";
		if (!discordJoinSecretOverride.empty() &&
			partyId == "zwnet_" + discordJoinSecretOverridePartyId)
		{
			joinSecret = discordJoinSecretOverride;
			canJoinDiscordParty = true;
		}

		const auto now = std::time(nullptr);
		const bool presenceActivityChanged = details != lastDetails || state != lastState;
		const bool presenceDataChanged = partyId != lastPartyId || joinSecret != lastJoinSecret
			|| partySize != lastPartySize || partyMax != lastPartyMax || partyPrivacy != lastPartyPrivacy
			|| canJoinDiscordParty != lastCanJoinDiscordParty;

		if (!forcePresenceUpdate && !presenceActivityChanged && !presenceDataChanged && now - lastUpdateTime < 1)
			return;

		lastDetails = details;
		lastState = state;
		lastPartyId = partyId;
		lastJoinSecret = joinSecret;
		lastPartySize = partySize;
		lastPartyMax = partyMax;
		lastPartyPrivacy = partyPrivacy;
		lastCanJoinDiscordParty = canJoinDiscordParty;
		lastUpdateTime = now;
		forcePresenceUpdate = false;

		if (presenceActivityChanged)
		{
			const auto generation = ++presenceGeneration;
			const auto publishAt = std::chrono::steady_clock::now() + 100ms;

			discordSessionStart = 0;
			timestampResetPending = true;
			PublishDiscordPresence();

			Scheduler::Schedule([generation, publishAt]
				{
					if (std::chrono::steady_clock::now() < publishAt)
						return false;

					std::lock_guard lock(discordUpdateMutex);

					if (!Initialized_ || generation != presenceGeneration)
						return true;

					discordSessionStart = std::time(nullptr);
					timestampResetPending = false;
					PublishDiscordPresence();
					return true;
				}, Scheduler::Pipeline::ASYNC, 25ms);

			return;
		}

		if (timestampResetPending)
		{
			PublishDiscordPresence();
			return;
		}

		if (!discordSessionStart)
			discordSessionStart = now;

		PublishDiscordPresence();
	}

	bool Discord::IsPrivateMatchOpen()
	{
		auto* menuPrivateLobby = Game::Menus_FindByName(Game::uiContext, "menu_xboxlive_privatelobby");
		auto* menuCreateServer = Game::Menus_FindByName(Game::uiContext, "createserver");

		return
			(menuPrivateLobby && Game::Menu_IsVisible(Game::uiContext, menuPrivateLobby)) ||
			(menuCreateServer && Game::Menu_IsVisible(Game::uiContext, menuCreateServer));
	}

	bool Discord::IsServerListOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "pc_join_unranked");
		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	bool Discord::IsMainMenuOpen()
	{
		auto* menuMain = Game::Menus_FindByName(Game::uiContext, "main_text");
		auto* menuMainZW3 = Game::Menus_FindByName(Game::uiContext, "pregame_loaderror");

		return
			(menuMain && Game::Menu_IsVisible(Game::uiContext, menuMain)) ||
			(menuMainZW3 && Game::Menu_IsVisible(Game::uiContext, menuMainZW3));
	}

	bool Discord::IsPartyLobbyOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "menu_xboxlive_lobby");
		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	bool Discord::IsZWNetMatchmakingOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "zwnet_matchmaking");
		return menu && Game::Menu_IsVisible(Game::uiContext, menu);
	}

	bool Discord::IsConnectMenuOpen()
	{
		auto* uiMenu = Game::Menus_FindByName(Game::uiContext, "connect");
		auto* cgameMenu = Game::Menus_FindByName(Game::cgDC, "connect");

		return
			(uiMenu && Game::Menu_IsVisible(Game::uiContext, uiMenu)) ||
			(cgameMenu && Game::Menu_IsVisible(Game::cgDC, cgameMenu));
	}

	void Discord::InitializeDiscord()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
			return;

		DiscordEventHandlers handlers{};
		handlers.ready = Ready;
		handlers.errored = Errored;
		handlers.disconnected = Errored;
		handlers.joinGame = JoinGame;
		handlers.joinRequest = JoinRequest;

		Discord_Initialize("1047291181404528660", &handlers, 1, nullptr);

		Initialized_ = true;

		Scheduler::Schedule([]
			{
				if (!Initialized_ || GameInitialized_)
					return true;

				Discord_RunCallbacks();
				return false;
			}, Scheduler::Pipeline::ASYNC, 250ms);
	}

	Discord::Discord()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
			return;

		InitializeDiscord();

		Scheduler::OnGameInitialized([]
			{
				GameInitialized_ = true;
				forcePresenceUpdate = true;
				UpdateDiscord();
				Scheduler::Loop(UpdateDiscord, Scheduler::Pipeline::MAIN, 1s);
			}, Scheduler::Pipeline::MAIN);
	}

	void Discord::preDestroy()
	{
		if (!Initialized_)
			return;

		Initialized_ = false;
		discordJoinAuthorizationInFlight = false;
		{
			std::lock_guard lock(discordUpdateMutex);
			discordJoinSecretOverride.clear();
			discordJoinSecretOverridePartyId.clear();
			++discordJoinSecretOverrideGeneration;
		}
		++discordConnectionGeneration;
		Discord_Shutdown();
	}
}
