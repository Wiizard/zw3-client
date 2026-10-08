#pragma once

namespace Components::BotAI
{
	class BotAI : public Component
	{
	public:
		BotAI();

		static bool Think(Game::client_s* client);

		static const char* PendingName();

		static bool HasRoomForBot();

		static bool OwnsBot(int clientNum);

	private:
		static bool isInstalled;
	};
}
