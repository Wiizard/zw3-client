#pragma once

namespace Components
{
	class Discord : public Component
	{
	public:
		enum class Backend
		{
			RPC,
			SocialSDK
		};

		Discord();

		static std::string GetDiscordServerLink() { return "https://discord.gg/QqnF2NFNVV"; }
		static const char* GetActiveBackendName();

		void preDestroy() override;

	private:
		static bool Initialized_;
		static Backend ActiveBackend_;
		static Backend ResolvePreferredBackend();
		static bool InitializeBackend(Backend backend);
		static void ShutdownBackend();

		static void UpdateDiscord();

		static bool IsPrivateMatchOpen();

		static bool IsServerListOpen();

		static bool IsMainMenuOpen();

		static bool IsPartyLobbyOpen();

		static const char* GetHostDiscordInviteIP();
	};
}
