#include "Discord.hpp"
#include "Party.hpp"
#include "TextRenderer.hpp"

#include <discord_rpc.h>
#include <chrono>

namespace Components
{
	static DiscordRichPresence DiscordPresence;
	static long long DiscordPresenceStartTime = 0;

	bool Discord::Initialized_;

	static unsigned int GetDiscordNonce()
	{
		static auto nonce = Utils::Cryptography::Rand::GenerateInt();
		return nonce;
	}

	static void Ready([[maybe_unused]] const DiscordUser* request)
	{
		Logger::Print("Discord: Ready\n");
	}

	static void JoinGame(const char* joinSecret)
	{
		Game::Cbuf_AddText(0, Utils::String::VA("connect %s\n", joinSecret));
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

	static void ResetDiscordPresence()
	{
		ZeroMemory(&DiscordPresence, sizeof(DiscordPresence));
		DiscordPresence.instance = 1;
		DiscordPresenceStartTime = 0;
	}

	void Discord::UpdateDiscord()
	{
		Discord_RunCallbacks();

		ResetDiscordPresence();

		if (!Game::CL_IsCgameInitialized())
		{
			if (Discord::IsMainMenuOpen())
			{
				DiscordPresence.details = "At the main menu";
			}
			if (Discord::IsServerListOpen())
			{
				DiscordPresence.details = "Browsing servers";
			}
			if (Discord::IsPrivateMatchOpen())
			{
				DiscordPresence.details = "In a lobby";
				DiscordPresence.state = "Setting up a private match";
			}
			if (Discord::IsPartyLobbyOpen())
			{
				DiscordPresence.details = "In a lobby";
				DiscordPresence.state = "Rallying other survivors";
			}

			DiscordPresence.largeImageKey = "https://i.imghippo.com/files/iAOF6351ypo.png";
		}
		else
		{
			char hostNameBuffer[256]{};

			const auto* map = Game::UI_GetMapDisplayName((*Game::ui_mapname)->current.string);

			const Game::StringTable* table;
			Game::StringTable_GetAsset_FastFile("mp/gameTypesTable.csv", &table);
			const auto row = Game::StringTable_LookupRowNumForValue(table, 0, (*Game::ui_gametype)->current.string);

			if (row != -1)
			{
				const auto* value = Game::StringTable_GetColumnValueForRow(table, row, 1);
				const auto* localize = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_LOCALIZE_ENTRY, value).localize;
				DiscordPresence.details = Utils::String::Format("Playing Zombies on {1}", localize ? localize->value : "Zombies", map);
			}
			else
			{
				DiscordPresence.details = Utils::String::Format("Playing Zombies on {}", map);
			}

			if (std::strcmp(Game::cls->servername, "localhost") == 0)
			{
				DiscordPresence.state = Utils::String::Format("In a private match on {}", map);
				DiscordPresence.partyPrivacy = DISCORD_PARTY_PRIVATE;
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
			}

			DiscordPresence.partySize = Game::cgArray[0].snap ? Game::cgArray[0].snap->numClients : 1;
			DiscordPresence.partyMax = Party::GetMaxClients();
			DiscordPresence.largeImageKey = "https://i.imghippo.com/files/iAOF6351ypo.png";

			if (DiscordPresenceStartTime == 0)
			{
				DiscordPresenceStartTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			}
			DiscordPresence.startTimestamp = DiscordPresenceStartTime;
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
		if (!menu)
		{
			return false;
		}
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

		if (!menu)
		{
			return false;
		}
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

		ResetDiscordPresence();
		Discord_UpdatePresence(&DiscordPresence);
		Discord_Shutdown();
	}
}
