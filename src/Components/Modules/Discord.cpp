#include "Discord.hpp"
#include "Party.hpp"
#include "TextRenderer.hpp"

#include <discord_rpc.h>
#include <Utils/WebIO.hpp>

namespace Components
{
	static DiscordRichPresence DiscordPresence;

	bool Discord::Initialized_;

	static unsigned int privateMatchNonce = 0;
	static int64_t discordSessionStart = 0;
	static std::string lastActivityContext;

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
	static bool forcePresenceUpdate = false;

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
		lastUpdateTime = 0;

		forcePresenceUpdate = true;
	}

	static void JoinGame(const char* joinSecret)
	{
		Logger::Print("Discord: Attempting to join match via invite. Secret: {}\n", joinSecret);

		if (!joinSecret || !joinSecret[0])
			return;

		const char* connect_cmd = Utils::String::VA("connect %s\n", joinSecret);
		Game::Cbuf_AddText(0, connect_cmd);
	}

	static void JoinRequest(const DiscordUser* request)
	{
		Logger::Print("Discord: Join request from {} ({})\n", request->username, request->userId);

		if (!currentDiscordCanJoin)
		{
			Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
			return;
		}

		Discord_Respond(request->userId, DISCORD_REPLY_YES);
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

	void Discord::UpdateDiscord()
	{
		Discord_RunCallbacks();

		bool isInGame = Game::CL_IsCgameInitialized();
		bool isPrivateLobby = Discord::IsPrivateMatchOpen();
		bool isPartyLobby = Discord::IsPartyLobbyOpen();
		bool isServerList = Discord::IsServerListOpen();
		bool isMainMenu = Discord::IsMainMenuOpen();
		bool isHosting = Dvar::Var("party_host").get<bool>();
		bool isDedi = Dvar::Var("sv_running").get<bool>();

		std::string activityContext;

		if (isInGame)
			activityContext = isDedi ? "dedi_game" : "private_game";
		else if (isServerList)
			activityContext = "server_list";
		else if (isPrivateLobby || isPartyLobby)
			activityContext = "lobby";
		else if (isMainMenu)
			activityContext = "main_menu";
		else
			activityContext = "other";

		if (activityContext != lastActivityContext)
		{
			discordSessionStart = std::time(nullptr);
			lastActivityContext = activityContext;
		}

		if (!discordSessionStart)
			discordSessionStart = std::time(nullptr);

		DiscordRichPresence newPresence{};
		newPresence.instance = 1;
		newPresence.largeImageKey = "https://i.imghippo.com/files/wbSr4660zUs.png";
		newPresence.startTimestamp = discordSessionStart;

		std::string details;
		std::string state;
		std::string partyId;
		std::string joinSecret;
		int partySize = 0;
		int partyMax = 0;
		int partyPrivacy = DISCORD_PARTY_PUBLIC;
		bool canJoinDiscordParty = false;

		if (!isInGame)
		{
			if (isServerList)
			{
				details = "Browsing servers";
				state = "";
			}
			else if (isMainMenu)
			{
				details = "At the main menu";
				state = "";
			}
			else if (isPrivateLobby || isPartyLobby)
			{
				const int privacy = Dvar::Var("partyPrivacy").get<int>();
				const bool isOpen = privacy == 0;
				const bool isClosed = privacy == 2;
				const char* privacyName = GetPartyPrivacyName(privacy);

				partyPrivacy = isOpen ? DISCORD_PARTY_PUBLIC : DISCORD_PARTY_PRIVATE;
				details = Utils::String::Format("In a party ({})", privacyName);

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
					state = numBots > 0
						? Utils::String::Format("Setting up a private match (with {} bot{})", numBots, numBots == 1 ? "" : "s")
						: "Setting up a private match";

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
					state = "Waiting for host to start a match";

					std::hash<Network::Address> hashFn;
					const auto address = Party::Target();
					partyId = Utils::String::VA("party_%zu_%u", hashFn(address), GetDiscordNonce());
				}
			}
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

		if (details.empty())
			details = "At the main menu";

		newPresence.details = details.c_str();
		newPresence.state = state.empty() ? nullptr : state.c_str();
		newPresence.partyId = partyId.empty() ? nullptr : partyId.c_str();
		newPresence.joinSecret = joinSecret.empty() ? nullptr : joinSecret.c_str();
		newPresence.partySize = partySize;
		newPresence.partyMax = partyMax;
		newPresence.partyPrivacy = partyPrivacy;

		int64_t now = std::time(nullptr);
		if (forcePresenceUpdate || details != lastDetails || state != lastState || partyId != lastPartyId || joinSecret != lastJoinSecret
			|| partySize != lastPartySize || partyMax != lastPartyMax || partyPrivacy != lastPartyPrivacy || now - lastUpdateTime >= 1)
		{
			DiscordPresence = newPresence;
			Discord_UpdatePresence(&DiscordPresence);

			forcePresenceUpdate = false;

			lastDetails = details;
			lastState = state;
			lastPartyId = partyId;
			lastJoinSecret = joinSecret;
			lastPartySize = partySize;
			lastPartyMax = partyMax;
			lastPartyPrivacy = partyPrivacy;
			lastUpdateTime = now;

			currentDiscordCanJoin = canJoinDiscordParty;
		}
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

		Scheduler::Once(UpdateDiscord, Scheduler::Pipeline::MAIN);
		Scheduler::Loop(UpdateDiscord, Scheduler::Pipeline::MAIN, 1s);

		Initialized_ = true;
	}

	Discord::Discord()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
			return;

		Scheduler::OnGameInitialized(Discord::InitializeDiscord, Scheduler::Pipeline::MAIN);
	}

	void Discord::preDestroy()
	{
		if (!Initialized_)
			return;

		Discord_Shutdown();
	}
}
