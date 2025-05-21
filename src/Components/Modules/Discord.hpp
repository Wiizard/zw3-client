#pragma once

namespace Components
{
	class Discord : public Component
	{
	public:
		Discord();

		static std::string GetDiscordServerLink() { return "https://discord.gg/QqnF2NFNVV"; }

		void preDestroy() override;

	private:
		static bool Initialized_;

		static void UpdateDiscord();

		static bool IsPrivateMatchOpen();

		static bool IsServerListOpen();

		static bool IsMainMenuOpen();

		static bool IsPartyLobbyOpen();
	};
}
