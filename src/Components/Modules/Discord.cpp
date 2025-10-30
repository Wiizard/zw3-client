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
	std::string hostIP = "";
	bool ipFetchInitiated = false;

	static unsigned int GetDiscordNonce()
	{
		static auto nonce = Utils::Cryptography::Rand::GenerateInt();
		return nonce;
	}

	static void Ready([[maybe_unused]] const DiscordUser* request)
	{
		ZeroMemory(&DiscordPresence, sizeof(DiscordPresence));
		DiscordPresence.instance = 1;
		Logger::Print("Discord: Ready\n");
		Discord_UpdatePresence(&DiscordPresence);
	}

	static void JoinGame(const char* joinSecret)
	{
		const char* connect_cmd = Utils::String::VA("connect %s\n", joinSecret);
		Game::Cbuf_AddText(0, connect_cmd);
	}

	static void JoinRequest(const DiscordUser* request)
	{
		Logger::Debug("Discord: Join request from {} ({})\n", request->username, request->userId);
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

		if (success)
		{
			hostIP = ip;
		}
		else
		{
			hostIP = "0.0.0.0";
		}
	}

	const char* Discord::GetHostDiscordInviteIP()
	{
		if (!hostIP.empty() && hostIP != "0.0.0.0")
		{
			return hostIP.c_str();
		}
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

		auto oldTimestamp = DiscordPresence.startTimestamp;
		ZeroMemory(&DiscordPresence, sizeof(DiscordPresence));
		DiscordPresence.instance = 1;
		DiscordPresence.largeImageKey = "https://i.imghippo.com/files/iAOF6351ypo.png";

		if(oldTimestamp != 0)
			DiscordPresence.startTimestamp = oldTimestamp;

		if (!Game::CL_IsCgameInitialized())
		{
			bool isInPrivateMatch = Discord::IsPrivateMatchOpen();
			bool isInPartyLobby = Discord::IsPartyLobbyOpen();

			if (!isInPrivateMatch && !isInPartyLobby) {
				privateMatchNonce = 0;
				hostIP = "";
				ipFetchInitiated = false;
			}

			if (Discord::IsMainMenuOpen())
			{
				DiscordPresence.details = "At the main menu";
			}
			if (Discord::IsServerListOpen())
			{
				DiscordPresence.details = "Browsing servers";
			}
			if (isInPrivateMatch || isInPartyLobby)
			{
				const auto address = Party::Target();
				const bool isHosting = Dvar::Var("party_host").get<bool>();

				const bool isPrivate = Dvar::Var("partyPrivacy").get<int>() == 1;
				const char* lobbyType = isPrivate ? "a party (Private)" : "a party (Public)";

				DiscordPresence.details = Utils::String::Format("In {}", lobbyType);

				int realPlayers = Dvar::Var("party_realPlayers").get<int>();
				int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
				int numBots = totalPlayers - realPlayers;

				int numMaxPlayers = 4;
				if (isInPartyLobby) {
					std::string raw = Dvar::Var("party_lobbyPlayerCount").get<std::string>();
					int lobbyRealPlayers = 0;
					int lobbyMaxPlayers = 0;
					sscanf(raw.c_str(), "%d/%d", &lobbyRealPlayers, &lobbyMaxPlayers);
					numMaxPlayers = lobbyMaxPlayers;
				}

				DiscordPresence.partySize = realPlayers;
				DiscordPresence.partyMax = numMaxPlayers;

				if (isHosting)
				{
					if (numBots > 0) {
						if(numBots == 1) DiscordPresence.state = Utils::String::Format("Setting up a private match (with {} bot)", numBots);
						else DiscordPresence.state = Utils::String::Format("Setting up a private match (with {} bots)", numBots);
					}
					else {
						DiscordPresence.state = "Setting up a private match";
					}

					const char* publicIp = Discord::GetHostDiscordInviteIP();
					if (std::strcmp(publicIp, "0.0.0.0") != 0) {
						if (privateMatchNonce == 0) {
							privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();
						}
						const auto address = Party::Target();
						DiscordPresence.joinSecret = Utils::String::VA("%s", publicIp);
					}
					else {
						DiscordPresence.joinSecret = nullptr;
					}
				}
				else
				{
					DiscordPresence.state = "Waiting for host to start a match";
				}

				if (isHosting) {
					DiscordPresence.partyPrivacy = isPrivate ? DISCORD_PARTY_PRIVATE : DISCORD_PARTY_PUBLIC;
				}

				std::hash<Network::Address> hashFn;
				if (privateMatchNonce == 0) {
					privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();
				}
				DiscordPresence.partyId = Utils::String::VA("private_match_%zu_%u", hashFn(address), privateMatchNonce);
			}

			Discord_UpdatePresence(&DiscordPresence);
			return;
		}

		char hostNameBuffer[256]{};
		const auto* map = Game::UI_GetMapDisplayName((*Game::ui_mapname)->current.string);
		const Game::StringTable* table;
		Game::StringTable_GetAsset_FastFile("mp/gameTypesTable.csv", &table);
		const auto row = Game::StringTable_LookupRowNumForValue(table, 0, (*Game::ui_gametype)->current.string);

		const int zModeVal = Dvar::Var("zombiemode").get<int>();
		static const char* zModeNames[]  = {"Normal", "Classic", "Hardcore"};
		const int zModeCount = sizeof(zModeNames) / sizeof(zModeNames[0]);
		const char* zMode = (zModeVal >= 0 && zModeVal < zModeCount) ? zModeNames[zModeVal] : "Normal";

		if (row != -1) {
			const auto* value = Game::StringTable_GetColumnValueForRow(table, row, 1);
			const auto* localize = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_LOCALIZE_ENTRY, value).localize;
			const bool isPrivate = Dvar::Var("partyPrivacy").get<int>() == 1;
			const char* privacySuffix = isPrivate ? " (Private)" : " (Public)";
			DiscordPresence.details = Utils::String::Format("{1} on {2}{3}", localize ? localize->value : "Zombies", zMode, map, privacySuffix);
		}
		else {
			const bool isPrivate = Dvar::Var("partyPrivacy").get<int>() == 1;
			const char* privacySuffix = isPrivate ? " (Private)" : " (Public)";
			DiscordPresence.details = Utils::String::Format("{} on {}{}", zMode, map, privacySuffix);
		}

		const bool isHosting = Dvar::Var("party_host").get<bool>();

		if (isHosting)
		{
			int numMaxPlayers = 4;
			int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
			int realPlayers = Dvar::Var("party_realPlayers").get<int>();
			int numBots = totalPlayers - realPlayers;

			if (numBots > 0) {
				if(numBots == 1) DiscordPresence.state = Utils::String::Format("In a private match (with {} bot)", numBots);
				else DiscordPresence.state = Utils::String::Format("In a private match (with {} bots)", numBots);
			}
			else {
				DiscordPresence.state = "In a private match";
			}

			int partyPrivacyDvar = Dvar::Var("partyPrivacy").get<int>();
			DiscordPresence.partyPrivacy = partyPrivacyDvar == 1 ? DISCORD_PARTY_PRIVATE : DISCORD_PARTY_PUBLIC;

			if (privateMatchNonce == 0) {
				privateMatchNonce = Utils::Cryptography::Rand::GenerateInt();
			}

			const auto address = Party::Target();
			std::hash<Network::Address> hashFn;
			DiscordPresence.partyId = Utils::String::VA("private_match_%zu_%u", hashFn(address), privateMatchNonce);

			const char* publicIp = Discord::GetHostDiscordInviteIP();
			if (std::strcmp(publicIp, "0.0.0.0") != 0) {
				DiscordPresence.joinSecret = Utils::String::VA("%s", publicIp);
			}
			else {
				DiscordPresence.joinSecret = nullptr;
			}

			DiscordPresence.partySize = realPlayers;
			DiscordPresence.partyMax = numMaxPlayers;
		}
		else
		{
			TextRenderer::StripColors(Party::GetHostName().data(), hostNameBuffer, sizeof(hostNameBuffer));
			TextRenderer::StripAllTextIcons(hostNameBuffer, hostNameBuffer, sizeof(hostNameBuffer));
			DiscordPresence.state = hostNameBuffer;
			DiscordPresence.partyPrivacy = DISCORD_PARTY_PUBLIC;

			std::hash<Network::Address> hashFn;
			const auto address = Party::Target();

			DiscordPresence.partyId = Utils::String::VA("%s - %zu", hostNameBuffer, hashFn(address) ^ GetDiscordNonce());
			DiscordPresence.joinSecret = address.getCString();

			DiscordPresence.partySize = Game::cgArray[0].snap ? Game::cgArray[0].snap->numClients : 1;
			DiscordPresence.partyMax = Party::GetMaxClients();
		}

		Discord_UpdatePresence(&DiscordPresence);
	}

	bool Discord::IsPrivateMatchOpen()
	{
		auto* menuPrivateLobby = Game::Menus_FindByName(Game::uiContext, "menu_xboxlive_privatelobby");
		auto* menuCreateServer = Game::Menus_FindByName(Game::uiContext, "createserver");

		if ((menuPrivateLobby && Game::Menu_IsVisible(Game::uiContext, menuPrivateLobby)) ||
			(menuCreateServer && Game::Menu_IsVisible(Game::uiContext, menuCreateServer)))
		{
			return true;
		}
		return false;
	}

	bool Discord::IsServerListOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "pc_join_unranked");
		if (!menu) return false;
		return Game::Menu_IsVisible(Game::uiContext, menu);
	}

	bool Discord::IsMainMenuOpen()
	{
		auto* menuMain = Game::Menus_FindByName(Game::uiContext, "main_text");
		auto* menuMainZW3 = Game::Menus_FindByName(Game::uiContext, "pregame_loaderror");

		if ((menuMain && Game::Menu_IsVisible(Game::uiContext, menuMain)) ||
			(menuMainZW3 && Game::Menu_IsVisible(Game::uiContext, menuMainZW3)))
		{
			return true;
		}
		return false;
	}

	bool Discord::IsPartyLobbyOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "menu_xboxlive_lobby");
		if (!menu) return false;
		return Game::Menu_IsVisible(Game::uiContext, menu);
	}

	Discord::Discord()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
		{
			return;
		}

		DiscordEventHandlers handlers;
		ZeroMemory(&handlers, sizeof(handlers));
		handlers.ready = Ready;
		handlers.errored = Errored;
		handlers.disconnected = Errored;
		handlers.joinGame = JoinGame;
		handlers.spectateGame = nullptr;
		handlers.joinRequest = JoinRequest;

		Discord_Initialize("1047291181404528660", &handlers, 1, nullptr);

		Scheduler::Once(UpdateDiscord, Scheduler::Pipeline::MAIN);
		Scheduler::Loop(UpdateDiscord, Scheduler::Pipeline::MAIN, 15s);

		Initialized_ = true;
	}

	void Discord::preDestroy()
	{
		if (!Initialized_)
		{
			return;
		}
		Discord_Shutdown();
	}
}
