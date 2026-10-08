#pragma once

struct DiscordUser;

namespace Components
{
	class Discord : public Component
	{
	public:
		Discord();

		static std::string GetDiscordServerLink() { return "https://discord.gg/QqnF2NFNVV"; }

	private:
		static void InitializeDiscord();

		static void UpdateDiscord();
		static void JoinGame(const char* joinSecret);
		static void JoinRequest(const DiscordUser* request);

		static bool IsPrivateMatchOpen();
		static bool IsServerListOpen();
		static bool IsMainMenuOpen();
		static bool IsPartyLobbyOpen();
		static bool IsZWNetMatchmakingOpen();
		static bool IsZWNetPreGameState(const std::string& state);
		static bool IsConnectMenuOpen();
	};
}
