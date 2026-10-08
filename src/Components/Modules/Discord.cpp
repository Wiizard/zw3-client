#include "STDInclude.hpp"

#include <discord_rpc.h>

#include "Discord.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Friends.hpp"
#include "Logger.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"
#include "ZWNet.hpp"

namespace Components
{
	constexpr std::uintptr_t cg_snap = 0x140479D30;

	constexpr auto applicationId = "1047291181404528660";
	constexpr auto largeImage = "https://i.imghippo.com/files/wbSr4660zUs.png";

	static DiscordRichPresence discordPresence;

	static std::atomic_bool isInitialized = false;
	static std::atomic_bool isGameInitialized = false;

	static std::recursive_mutex discordUpdateMutex;
	static unsigned int privateMatchNonce = 0;
	static std::int64_t discordSessionStart = 0;

	static std::string hostIP;
	static bool isIpFetchInitiated = false;
	static bool isIpFetchInProgress = false;
	static std::int64_t lastIpFetchAttempt = 0;

	static std::string lastDetails;
	static std::string lastState;
	static std::string lastPartyId;
	static std::string lastJoinSecret;
	static int lastPartySize = 0;
	static int lastPartyMax = 0;
	static int lastPartyPrivacy = -1;
	static std::int64_t lastUpdateTime = 0;

	static bool canCurrentlyJoin = false;
	static bool couldLastJoin = false;
	static bool isPresenceUpdateForced = false;
	static bool isTimestampResetPending = false;
	static unsigned long long presenceGeneration = 0;
	static std::atomic_bool isJoinAuthorizationInFlight = false;
	static std::atomic_ullong connectionGeneration = 0;
	static std::string joinSecretOverride;
	static std::string joinSecretOverridePartyId;
	static unsigned long long joinSecretOverrideGeneration = 0;

	static const char* NullIfEmpty(const std::string& text)
	{
		if (text.empty())
		{
			return nullptr;
		}

		return text.data();
	}

	static void PublishDiscordPresence()
	{
		DiscordRichPresence presence{};
		presence.instance = 1;
		presence.largeImageKey = largeImage;
		presence.startTimestamp = discordSessionStart;
		presence.details = NullIfEmpty(lastDetails);
		presence.state = NullIfEmpty(lastState);
		presence.partyId = NullIfEmpty(lastPartyId);
		presence.joinSecret = NullIfEmpty(lastJoinSecret);
		presence.partySize = lastPartySize;
		presence.partyMax = lastPartyMax;
		presence.partyPrivacy = lastPartyPrivacy;

		discordPresence = presence;
		Discord_UpdatePresence(&discordPresence);
		canCurrentlyJoin = couldLastJoin;
	}

	static unsigned int GetDiscordNonce()
	{
		static const auto nonce = Utils::Cryptography::Rand::GenerateInt();
		return nonce;
	}

	static const char* GetPartyPrivacyName(const int privacy)
	{
		switch (privacy)
		{
		case 1:
			return "Invite-Only";
		case 2:
			return "Closed";
		default:
			return "Open";
		}
	}

	static const char* VisibilityPrivacyName(const std::string& visibility)
	{
		if (visibility == "OPEN")
		{
			return "Open";
		}

		if (visibility == "CLOSED")
		{
			return "Closed";
		}

		return "Invite-Only";
	}

	bool Discord::IsZWNetPreGameState(const std::string& state)
	{
		return state == "SEARCH_STARTING" || state == "SEARCHING" || state == "MATCH_FOUND"
			|| state == "MAP_VOTE" || state == "READY_CHECK" || state == "WAITING_FOR_READY"
			|| state == "RESERVING_SERVER" || state == "STARTING_SERVER" || state == "SERVER_STARTING"
			|| state == "COUNTDOWN" || state == "CONNECTING" || state == "DIRECT_CONNECTION"
			|| state == "RELAY_CONNECTION";
	}

	static std::string GetZWNetPresenceState(const std::string& state, const int partySize, const int partyMax)
	{
		static const std::unordered_map<std::string, std::string> texts =
		{
			{ "SEARCH_STARTING", "Searching for available matches" },
			{ "SEARCHING", "Searching for a match" },
			{ "MATCH_FOUND", "Joining match lobby" },
			{ "MAP_VOTE", "Voting for the next map" },
			{ "READY_CHECK", "Waiting for all players to be ready" },
			{ "WAITING_FOR_READY", "Waiting for all players to be ready" },
			{ "RESERVING_SERVER", "Setting up match" },
			{ "STARTING_SERVER", "Setting up match" },
			{ "SERVER_STARTING", "Setting up match" },
			{ "COUNTDOWN", "Starting match" },
			{ "CONNECTING", "Connecting to match" },
			{ "DIRECT_CONNECTION", "Joining match" },
			{ "RELAY_CONNECTION", "Joining match" },
			{ "IN_MATCH", "Playing matchmaking" },
		};

		if (state == "ERROR")
		{
			return "Unable to join game session";
		}

		std::string text = "Idle";
		const auto known = texts.find(state);

		if (known != texts.end())
		{
			text = known->second;
		}

		const auto currentPlayers = std::max(1, partySize);
		const auto maximumPlayers = std::max(currentPlayers, partyMax);

		return std::format("{} ({}/{})", text, currentPlayers, maximumPlayers);
	}

	static std::string GetMapDisplayName(const std::string& rawMapName)
	{
		const auto* displayName = Game::UI_GetMapDisplayName(rawMapName.data());

		if (displayName && *displayName)
		{
			return displayName;
		}

		return rawMapName;
	}

	static std::string GetLoadingMapDisplayName()
	{
		auto rawMapName = Dvar::Var("mapname").Get<std::string>();

		if (rawMapName.empty())
		{
			rawMapName = Dvar::Var("ui_mapname").Get<std::string>();
		}

		if (rawMapName.empty())
		{
			return {};
		}

		return GetMapDisplayName(rawMapName);
	}

	static void Ready([[maybe_unused]] const DiscordUser* request)
	{
		std::lock_guard _(discordUpdateMutex);

		ZeroMemory(&discordPresence, sizeof(discordPresence));
		discordPresence.instance = 1;
		Logger::Print("Discord: Ready\n");

		lastDetails.clear();
		lastState.clear();
		lastPartyId.clear();
		lastJoinSecret.clear();
		lastPartySize = -1;
		lastPartyMax = -1;
		lastPartyPrivacy = -1;
		couldLastJoin = false;
		lastUpdateTime = 0;
		discordSessionStart = 0;
		isTimestampResetPending = false;
		++presenceGeneration;
		canCurrentlyJoin = false;
		isJoinAuthorizationInFlight = false;
		joinSecretOverride.clear();
		joinSecretOverridePartyId.clear();
		++joinSecretOverrideGeneration;
		++connectionGeneration;

		isPresenceUpdateForced = true;
	}

	void Discord::JoinGame(const char* joinSecret)
	{
		if (!isGameInitialized || !joinSecret || !*joinSecret)
		{
			return;
		}

		Logger::Print("Discord: Processing a join invitation\n");

		constexpr std::string_view capabilityPrefix = "zwnet-cap:";
		constexpr std::string_view partyPrefix = "zwnet:";
		const std::string_view secret = joinSecret;

		if (secret.starts_with(capabilityPrefix))
		{
			ZWNet::JoinCapability(std::string(secret.substr(capabilityPrefix.size())));
			return;
		}

		if (secret.starts_with(partyPrefix))
		{
			ZWNet::JoinParty(std::string(secret.substr(partyPrefix.size())));
			return;
		}

		Game::Cbuf_AddText(0, Utils::String::VA("connect %s\n", joinSecret));
	}

	static void Errored(const int errorCode, const char* message)
	{
		Logger::Error("Discord: Error ({}): {}\n", errorCode, message);
	}

	static const char* GetHostDiscordInviteIP()
	{
		if (!hostIP.empty() && hostIP != "0.0.0.0")
		{
			return hostIP.data();
		}

		const auto now = std::time(nullptr);

		if (!isIpFetchInProgress && (!isIpFetchInitiated || now - lastIpFetchAttempt >= 10))
		{
			isIpFetchInitiated = true;
			isIpFetchInProgress = true;
			lastIpFetchAttempt = now;

			Scheduler::Once([]
			{
				bool isSuccessful = false;
				auto ip = Utils::WebIO("zw3-get-host-ip").Get("https://api.ipify.org", &isSuccessful);

				Scheduler::Once([ip = std::move(ip), isSuccessful]
				{
					std::lock_guard _(discordUpdateMutex);

					if (isSuccessful && !ip.empty() && ip != "0.0.0.0")
					{
						hostIP = ip;
						isPresenceUpdateForced = true;
					}
					else
					{
						hostIP.clear();
						isIpFetchInitiated = false;
					}

					isIpFetchInProgress = false;
				}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
		}

		return "0.0.0.0";
	}

	struct ZWNetDiscordPartySnapshot
	{
		std::string id;
		std::string state;
		std::string visibility;
		int members = 0;
	};

	static std::optional<ZWNetDiscordPartySnapshot> GetZWNetDiscordPartySnapshot()
	{
		const auto* lobbyActive = Game::Dvar_FindVar("zwnet_lobby_active");
		const auto* partyId = Game::Dvar_FindVar("zwnet_lobby_party_id");
		const auto* state = Game::Dvar_FindVar("ui_zwnet_state");
		const auto* visibility = Game::Dvar_FindVar("zwnet_lobby_visibility");
		const auto* memberCount = Game::Dvar_FindVar("zwnet_lobby_member_count");

		if (!lobbyActive || !partyId || !state || !visibility || !memberCount)
		{
			return std::nullopt;
		}

		const bool areTypesRight = lobbyActive->type == Game::DVAR_TYPE_BOOL && partyId->type == Game::DVAR_TYPE_STRING
			&& state->type == Game::DVAR_TYPE_STRING && visibility->type == Game::DVAR_TYPE_STRING && memberCount->type == Game::DVAR_TYPE_INT;

		if (!areTypesRight || !lobbyActive->current.enabled || !partyId->current.string)
		{
			return std::nullopt;
		}

		ZWNetDiscordPartySnapshot snapshot;
		snapshot.id = partyId->current.string;

		if (!snapshot.id.starts_with("pty_") || snapshot.id.size() > 80)
		{
			return std::nullopt;
		}

		if (state->current.string)
		{
			snapshot.state = state->current.string;
		}

		if (visibility->current.string)
		{
			snapshot.visibility = visibility->current.string;
		}

		snapshot.members = std::clamp(memberCount->current.integer, 1, 4);
		return snapshot;
	}

	void Discord::JoinRequest(const DiscordUser* request)
	{
		if (!isInitialized || !request || !request->userId || !*request->userId)
		{
			return;
		}

		if (!isGameInitialized)
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}

		Logger::Print("Discord: Received a join request\n");

		const auto snapshot = GetZWNetDiscordPartySnapshot();
		const bool isAdvertisingZWNetParty = lastPartyId.starts_with("zwnet_");

		if (isAdvertisingZWNetParty && (!snapshot || lastPartyId != "zwnet_" + snapshot->id))
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}

		if (!snapshot || snapshot->visibility != "INVITE_ONLY")
		{
			if (canCurrentlyJoin)
			{
				Discord_Respond(request->userId, DISCORD_REPLY_YES);
			}
			else
			{
				Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			}

			return;
		}

		if (!canCurrentlyJoin || snapshot->members >= 4 || isJoinAuthorizationInFlight.exchange(true))
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}

		const std::string userId = request->userId;
		const auto partyId = snapshot->id;
		const auto generation = connectionGeneration.load();

		Friends::AuthorizeDiscordPartyJoin(userId, partyId, [userId, partyId, generation](std::optional<std::string> joinSecret)
		{
			if (connectionGeneration.load() != generation)
			{
				return;
			}

			std::lock_guard _(discordUpdateMutex);

			if (!isInitialized || !isGameInitialized)
			{
				isJoinAuthorizationInFlight = false;
				return;
			}

			const auto current = GetZWNetDiscordPartySnapshot();
			const bool isStillJoinable = joinSecret && current && current->id == partyId && current->visibility == "INVITE_ONLY" && current->members < 4;

			if (!isStillJoinable)
			{
				isJoinAuthorizationInFlight = false;
				Discord_Respond(userId.data(), DISCORD_REPLY_NO);
				return;
			}

			joinSecretOverride = std::move(*joinSecret);
			joinSecretOverridePartyId = partyId;
			const auto overrideGeneration = ++joinSecretOverrideGeneration;
			isPresenceUpdateForced = true;
			UpdateDiscord();
			Discord_Respond(userId.data(), DISCORD_REPLY_YES);

			Scheduler::Once([partyId, overrideGeneration]
			{
				std::lock_guard resetLock(discordUpdateMutex);

				if (overrideGeneration == joinSecretOverrideGeneration && joinSecretOverridePartyId == partyId)
				{
					joinSecretOverride.clear();
					joinSecretOverridePartyId.clear();
					isPresenceUpdateForced = true;
					isJoinAuthorizationInFlight = false;
				}
			}, Scheduler::Pipeline::MAIN, 3s);
		});
	}

	static void ApplyZWNetDiscordParty(const ZWNetDiscordPartySnapshot& snapshot, std::string& partyId, std::string& joinSecret, int& partySize, int& partyMax, int& partyPrivacy, bool& canJoin)
	{
		partyId = "zwnet_" + snapshot.id;
		partySize = snapshot.members;
		partyMax = 4;
		partyPrivacy = DISCORD_PARTY_PRIVATE;

		if (snapshot.visibility == "OPEN")
		{
			partyPrivacy = DISCORD_PARTY_PUBLIC;
		}

		canJoin = snapshot.visibility != "CLOSED" && partySize < partyMax;
		joinSecret.clear();

		if (canJoin)
		{
			joinSecret = "zwnet:" + snapshot.id;
		}
	}

	static std::string BotsSuffix(const int bots)
	{
		if (bots == 1)
		{
			return " (with 1 bot)";
		}

		return std::format(" (with {} bots)", bots);
	}

	static void ApplyHostedParty(const char* prefix, const bool isClosed, std::string& partyId, std::string& joinSecret, bool& canJoin)
	{
		if (privateMatchNonce == 0)
		{
			privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();
		}

		const char* const publicIp = GetHostDiscordInviteIP();

		if (!isClosed && std::strcmp(publicIp, "0.0.0.0") != 0)
		{
			joinSecret = std::format("{}:28960", publicIp);
			partyId = std::format("{}_{}_{}", prefix, publicIp, privateMatchNonce);
			canJoin = true;
			return;
		}

		partyId = std::format("{}_pending_{}", prefix, privateMatchNonce);
		canJoin = false;
	}

	void Discord::UpdateDiscord()
	{
		std::lock_guard _(discordUpdateMutex);

		if (!isInitialized)
		{
			return;
		}

		Discord_RunCallbacks();

		bool isInGame = false;
		bool isPrivateLobby = false;
		bool isPartyLobby = false;
		bool isZWNetMatchmaking = false;
		bool isConnectMenu = false;
		bool isServerList = false;
		bool isMainMenu = false;
		bool isHosting = false;

		if (isGameInitialized)
		{
			isInGame = Game::CL_IsCgameInitialized(0);
			isPrivateLobby = IsPrivateMatchOpen();
			isPartyLobby = IsPartyLobbyOpen();
			isZWNetMatchmaking = IsZWNetMatchmakingOpen();
			isConnectMenu = IsConnectMenuOpen();
			isServerList = IsServerListOpen();
			isMainMenu = IsMainMenuOpen();
			isHosting = Party::IsHostingParty();
		}

		std::string details;
		std::string state;
		std::string partyId;
		std::string joinSecret;
		int partySize = 0;
		int partyMax = 0;
		int partyPrivacy = DISCORD_PARTY_PUBLIC;
		bool canJoin = false;

		if (!isGameInitialized)
		{
			details = "Launching game";
			canCurrentlyJoin = false;
		}
		else if (isConnectMenu)
		{
			const auto mapName = GetLoadingMapDisplayName();

			if (mapName.empty())
			{
				state = "Preparing game...";
			}
			else
			{
				state = std::format("Loading {}...", mapName);
			}

			const auto zwnetParty = GetZWNetDiscordPartySnapshot();

			if (zwnetParty)
			{
				details = std::format("In pre-game lobby ({})", VisibilityPrivacyName(zwnetParty->visibility));
				ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret, partySize, partyMax, partyPrivacy, canJoin);
			}
			else
			{
				details = "Loading map";
			}
		}
		else if (!isInGame && isServerList)
		{
			details = "Browsing servers";
		}
		else if (!isInGame && isZWNetMatchmaking)
		{
			const auto zwnetParty = GetZWNetDiscordPartySnapshot();

			if (zwnetParty)
			{
				const auto* privacyName = VisibilityPrivacyName(zwnetParty->visibility);

				if (IsZWNetPreGameState(zwnetParty->state))
				{
					details = std::format("In pre-game lobby ({})", privacyName);
				}
				else
				{
					details = std::format("In a public party ({})", privacyName);
				}

				ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret, partySize, partyMax, partyPrivacy, canJoin);
				state = GetZWNetPresenceState(zwnetParty->state, partySize, partyMax);
			}
			else
			{
				details = "Preparing ZW3 matchmaking";
			}
		}
		else if (!isInGame && isMainMenu)
		{
			details = "At the main menu";
		}
		else if (!isInGame && (isPrivateLobby || isPartyLobby))
		{
			int privacy = Dvar::Var("partyPrivacy").Get<int>();

			if (privacy < 0 || privacy > 2)
			{
				privacy = 0;
			}

			if (privacy != 0)
			{
				partyPrivacy = DISCORD_PARTY_PRIVATE;
			}

			if (isPrivateLobby)
			{
				details = std::format("In a private party ({})", GetPartyPrivacyName(privacy));
			}
			else
			{
				details = std::format("In a public party ({})", GetPartyPrivacyName(privacy));
			}

			const int realPlayers = Dvar::Var("party_realPlayers").Get<int>();
			const int totalPlayers = Dvar::Var("party_currentPlayers").Get<int>();
			const int bots = totalPlayers - realPlayers;

			partySize = std::max(realPlayers, 1);
			partyMax = 4;

			if (isPartyLobby)
			{
				const auto raw = Dvar::Var("party_lobbyPlayerCount").Get<std::string>();
				int lobbyRealPlayers = 0;
				int lobbyMaxPlayers = 0;

				std::sscanf(raw.data(), "%d/%d", &lobbyRealPlayers, &lobbyMaxPlayers);

				if (lobbyRealPlayers > 0)
				{
					partySize = lobbyRealPlayers;
				}

				if (lobbyMaxPlayers > 0)
				{
					partyMax = lobbyMaxPlayers;
				}
			}

			partyMax = std::max(partyMax, partySize);

			if (isHosting && isPrivateLobby && bots > 0)
			{
				state = "Setting up a private match" + BotsSuffix(bots);
			}
			else if (isHosting && isPrivateLobby)
			{
				state = "Setting up a private match";
			}
			else if (isHosting)
			{
				state = std::format("Waiting for players ({}/{})", partySize, partyMax);
			}
			else if (isPrivateLobby)
			{
				state = "Waiting for host to start a match";
			}
			else
			{
				state = std::format("Waiting in party ({}/{})", partySize, partyMax);
			}

			if (isHosting)
			{
				ApplyHostedParty("party", privacy == 2, partyId, joinSecret, canJoin);
			}
			else
			{
				const std::hash<Network::Address> hashFn;
				partyId = std::format("party_{}_{}", hashFn(Party::Target()), GetDiscordNonce());
			}
		}
		else if (isInGame)
		{
			const auto map = GetMapDisplayName(Dvar::Var("ui_mapname").Get<std::string>());
			const auto zwnetParty = GetZWNetDiscordPartySnapshot();

			if (zwnetParty)
			{
				details = std::format("ZW3 matchmaking on {}", map);
				state = GetZWNetPresenceState(zwnetParty->state, zwnetParty->members, 4);
				ApplyZWNetDiscordParty(*zwnetParty, partyId, joinSecret, partySize, partyMax, partyPrivacy, canJoin);
			}
			else if (isHosting)
			{
				static constexpr const char* zombieModeNames[] = { "Normal", "Classic", "Hardcore" };

				const int zombieMode = Dvar::Var("zombiemode").Get<int>();
				const char* zombieModeName = "Normal";

				if (zombieMode >= 0 && zombieMode < static_cast<int>(std::size(zombieModeNames)))
				{
					zombieModeName = zombieModeNames[zombieMode];
				}

				const int privacy = Dvar::Var("partyPrivacy").Get<int>();

				if (privacy != 0)
				{
					partyPrivacy = DISCORD_PARTY_PRIVATE;
				}

				details = std::format("{} on {} ({})", zombieModeName, map, GetPartyPrivacyName(privacy));

				const int totalPlayers = Dvar::Var("party_currentPlayers").Get<int>();
				const int realPlayers = Dvar::Var("party_realPlayers").Get<int>();
				const int bots = totalPlayers - realPlayers;

				state = "In a private match";

				if (bots > 0)
				{
					state += BotsSuffix(bots);
				}

				ApplyHostedParty("match", privacy == 2, partyId, joinSecret, canJoin);

				partySize = std::max(realPlayers, 1);
				partyMax = 4;
			}
			else
			{
				static constexpr const char* zombieModeNames[] = { "Normal", "Classic", "Hardcore" };

				const int zombieMode = Dvar::Var("zombiemode").Get<int>();
				const char* zombieModeName = "Normal";

				if (zombieMode >= 0 && zombieMode < static_cast<int>(std::size(zombieModeNames)))
				{
					zombieModeName = zombieModeNames[zombieMode];
				}

				details = std::format("{} on {}", zombieModeName, map);

				char hostNameBuffer[256]{};
				TextRenderer::StripColors(Party::GetHostName().data(), hostNameBuffer, sizeof(hostNameBuffer));
				TextRenderer::StripAllTextIcons(hostNameBuffer, hostNameBuffer, sizeof(hostNameBuffer));

				state = hostNameBuffer;

				const std::hash<Network::Address> hashFn;
				const auto address = Party::Target();

				partyId = std::format("{}_{}", hostNameBuffer, hashFn(address) ^ GetDiscordNonce());
				joinSecret = address.GetString();

				const auto* snap = *reinterpret_cast<const Game::snapshot_s* const*>(Utils::Hook::Rebase(cg_snap));
				partySize = 1;

				if (snap)
				{
					partySize = std::max(snap->numClients, 1);
				}

				partyMax = std::max(Party::GetMaxClients(), partySize);
				canJoin = !joinSecret.empty();
				partyPrivacy = DISCORD_PARTY_PUBLIC;
			}
		}

		if (details.empty())
		{
			details = "At the main menu";
		}

		if (!joinSecretOverride.empty() && partyId == "zwnet_" + joinSecretOverridePartyId)
		{
			joinSecret = joinSecretOverride;
			canJoin = true;
		}

		const auto now = std::time(nullptr);
		const bool hasActivityChanged = details != lastDetails || state != lastState;
		const bool hasDataChanged = partyId != lastPartyId || joinSecret != lastJoinSecret || partySize != lastPartySize
			|| partyMax != lastPartyMax || partyPrivacy != lastPartyPrivacy || canJoin != couldLastJoin;

		if (!isPresenceUpdateForced && !hasActivityChanged && !hasDataChanged && now - lastUpdateTime < 1)
		{
			return;
		}

		lastDetails = details;
		lastState = state;
		lastPartyId = partyId;
		lastJoinSecret = joinSecret;
		lastPartySize = partySize;
		lastPartyMax = partyMax;
		lastPartyPrivacy = partyPrivacy;
		couldLastJoin = canJoin;
		lastUpdateTime = now;
		isPresenceUpdateForced = false;

		if (hasActivityChanged)
		{
			const auto generation = ++presenceGeneration;
			const auto publishAt = std::chrono::steady_clock::now() + 100ms;

			discordSessionStart = 0;
			isTimestampResetPending = true;
			PublishDiscordPresence();

			Scheduler::Schedule([generation, publishAt]
			{
				if (std::chrono::steady_clock::now() < publishAt)
				{
					return false;
				}

				std::lock_guard lock(discordUpdateMutex);

				if (isInitialized && generation == presenceGeneration)
				{
					discordSessionStart = std::time(nullptr);
					isTimestampResetPending = false;
					PublishDiscordPresence();
				}

				return true;
			}, Scheduler::Pipeline::ASYNC, 25ms);

			return;
		}

		if (!isTimestampResetPending && !discordSessionStart)
		{
			discordSessionStart = now;
		}

		PublishDiscordPresence();
	}

	static bool IsMenuOpen(Game::UiContext* context, const char* name)
	{
		auto* const menu = Game::Menus_FindByName(context, name);
		return menu && Game::Menu_IsVisible(context, menu);
	}

	bool Discord::IsPrivateMatchOpen()
	{
		return IsMenuOpen(Game::uiContext, "menu_xboxlive_privatelobby") || IsMenuOpen(Game::uiContext, "createserver");
	}

	bool Discord::IsServerListOpen()
	{
		return IsMenuOpen(Game::uiContext, "pc_join_unranked");
	}

	bool Discord::IsMainMenuOpen()
	{
		return IsMenuOpen(Game::uiContext, "main_text") || IsMenuOpen(Game::uiContext, "pregame_loaderror");
	}

	bool Discord::IsPartyLobbyOpen()
	{
		return IsMenuOpen(Game::uiContext, "menu_xboxlive_lobby");
	}

	bool Discord::IsZWNetMatchmakingOpen()
	{
		return IsMenuOpen(Game::uiContext, "zwnet_matchmaking");
	}

	bool Discord::IsConnectMenuOpen()
	{
		return IsMenuOpen(Game::uiContext, "connect") || IsMenuOpen(Game::cgDC, "connect");
	}

	void Discord::InitializeDiscord()
	{
		DiscordEventHandlers handlers{};
		handlers.ready = Ready;
		handlers.errored = Errored;
		handlers.disconnected = Errored;
		handlers.joinGame = JoinGame;
		handlers.joinRequest = JoinRequest;

		Discord_Initialize(applicationId, &handlers, 1, nullptr);

		isInitialized = true;

		Scheduler::Schedule([]
		{
			if (!isInitialized || isGameInitialized)
			{
				return true;
			}

			std::lock_guard _(discordUpdateMutex);
			Discord_RunCallbacks();
			return false;
		}, Scheduler::Pipeline::ASYNC, 250ms);
	}

	Discord::Discord()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		InitializeDiscord();

		Scheduler::OnGameInitialized([]
		{
			isGameInitialized = true;

			{
				std::lock_guard _(discordUpdateMutex);
				isPresenceUpdateForced = true;
			}

			UpdateDiscord();
			Scheduler::Loop(UpdateDiscord, Scheduler::Pipeline::MAIN, 1s);
		}, Scheduler::Pipeline::MAIN);

		Scheduler::OnShutdown([]
		{
			if (!isInitialized)
			{
				return;
			}

			isInitialized = false;
			isJoinAuthorizationInFlight = false;

			{
				std::lock_guard _(discordUpdateMutex);
				joinSecretOverride.clear();
				joinSecretOverridePartyId.clear();
				++joinSecretOverrideGeneration;
			}

			++connectionGeneration;
			Discord_Shutdown();
		});
	}
}
