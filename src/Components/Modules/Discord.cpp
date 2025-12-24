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

	static std::string lastDetails;
	static std::string lastState;
	static std::string lastPartyId;
	static std::string lastJoinSecret;
	static int lastPartySize = 0;
	static int lastPartyMax = 0;
	static int64_t lastUpdateTime = 0;

	static unsigned int GetDiscordNonce()
	{
		static auto nonce = Utils::Cryptography::Rand::GenerateInt();
		return nonce;
	}

	static void Ready([[maybe_unused]] const DiscordUser* request)
	{
		ZeroMemory(&DiscordPresence, sizeof(DiscordPresence));
		DiscordPresence.instance = 1;
		Discord_UpdatePresence(&DiscordPresence);
	}

	static void JoinGame(const char* joinSecret)
	{
		const char* connect_cmd = Utils::String::VA("connect %s\n", joinSecret);
		Game::Cbuf_AddText(0, connect_cmd);
	}

	static void JoinRequest(const DiscordUser* request)
	{
		Discord_Respond(request->userId, DISCORD_REPLY_IGNORE);
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
		hostIP = success ? ip : "0.0.0.0";
	}

	const char* Discord::GetHostDiscordInviteIP()
	{
		if (!hostIP.empty() && hostIP != "0.0.0.0")
			return hostIP.c_str();

		if (!ipFetchInitiated)
		{
			ipFetchInitiated = true;
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
			privateMatchNonce = 0;
		}

		DiscordRichPresence newPresence{};
		newPresence.instance = 1;
		newPresence.largeImageKey = "https://i.imghippo.com/files/iAOF6351ypo.png";
		newPresence.startTimestamp = discordSessionStart;

		std::string details;
		std::string state;
		std::string partyId;
		std::string joinSecret;
		int partySize = 0;
		int partyMax = 0;

		if (!isInGame)
		{
			if (isServerList)
				details = "Browsing servers";
			else if (isMainMenu)
				details = "At the main menu";
			else if (isPrivateLobby || isPartyLobby)
			{
				const bool isPrivate = Dvar::Var("partyPrivacy").get<int>() == 1;
				details = Utils::String::Format("In a party ({})", isPrivate ? "Private" : "Public");

				int realPlayers = Dvar::Var("party_realPlayers").get<int>();
				int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
				int numBots = totalPlayers - realPlayers;

				partySize = realPlayers;
				partyMax = 4;

				if (isPartyLobby)
				{
					std::string raw = Dvar::Var("party_lobbyPlayerCount").get<std::string>();
					int lobbyRealPlayers = 0;
					int lobbyMaxPlayers = 0;
					sscanf(raw.c_str(), "%d/%d", &lobbyRealPlayers, &lobbyMaxPlayers);
					partyMax = lobbyMaxPlayers;
				}

				if (isHosting)
				{
					state = numBots > 0
						? Utils::String::Format("Setting up a private match (with {} bot{})", numBots, numBots == 1 ? "" : "s")
						: "Setting up a private match";

					const char* publicIp = Discord::GetHostDiscordInviteIP();
					if (std::strcmp(publicIp, "0.0.0.0") != 0)
					{
						if (privateMatchNonce == 0)
							privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();
						joinSecret = Utils::String::VA("%s", publicIp);
					}
				}
				else
					state = "Waiting for host to start a match";

				if (privateMatchNonce == 0)
					privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();

				std::hash<Network::Address> hashFn;
				const auto address = Party::Target();
				partyId = Utils::String::VA("party_%zu_%u", hashFn(address), privateMatchNonce);
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
				const bool isPrivate = Dvar::Var("partyPrivacy").get<int>() == 1;
				details += isPrivate ? " (Private)" : " (Public)";
			}

			if (isHosting)
			{
				int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
				int realPlayers = Dvar::Var("party_realPlayers").get<int>();
				int numBots = totalPlayers - realPlayers;

				state = numBots > 0
					? Utils::String::Format("In a private match (with {} bot{})", numBots, numBots == 1 ? "" : "s")
					: "In a private match";

				if (privateMatchNonce == 0)
					privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();

				std::hash<Network::Address> hashFn;
				const auto address = Party::Target();
				partyId = Utils::String::VA("match_%zu_%u", hashFn(address), privateMatchNonce);

				const char* publicIp = Discord::GetHostDiscordInviteIP();
				if (std::strcmp(publicIp, "0.0.0.0") != 0)
					joinSecret = Utils::String::VA("%s", publicIp);

				partySize = realPlayers;
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
			}
		}

		newPresence.details = details.c_str();
		newPresence.state = state.c_str();
		newPresence.partyId = partyId.c_str();
		newPresence.joinSecret = joinSecret.empty() ? nullptr : joinSecret.c_str();
		newPresence.partySize = partySize;
		newPresence.partyMax = partyMax;

		int64_t now = std::time(nullptr);
		if (details != lastDetails || state != lastState || partyId != lastPartyId || joinSecret != lastJoinSecret
			|| partySize != lastPartySize || partyMax != lastPartyMax || now - lastUpdateTime >= 7)
		{
			DiscordPresence = newPresence;
			Discord_UpdatePresence(&DiscordPresence);

			lastDetails = details;
			lastState = state;
			lastPartyId = partyId;
			lastJoinSecret = joinSecret;
			lastPartySize = partySize;
			lastPartyMax = partyMax;
			lastUpdateTime = now;
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

	Discord::Discord()
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

	void Discord::preDestroy()
	{
		if (!Initialized_)
			return;

		Discord_Shutdown();
	}
}
