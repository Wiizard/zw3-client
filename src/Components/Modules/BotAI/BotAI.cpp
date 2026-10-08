#include "STDInclude.hpp"

#include "BotAI.hpp"
#include "BotControl.hpp"
#include "Iw4.hpp"
#include "Control/Lobby.hpp"
#include "Control/Identity.hpp"

#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Logger.hpp"

namespace Components::BotAI
{
	bool BotAI::isInstalled = false;

	bool BotAI::Think(Game::client_s* client)
	{
		if (!OwnsBot(static_cast<int>(client - Game::svs_clients)))
		{
			return false;
		}

		ThinkBot(client);
		return true;
	}

	const char* BotAI::PendingName()
	{
		if (!isInstalled)
		{
			return nullptr;
		}

		return PendingBotName();
	}

	bool BotAI::OwnsBot(int clientNum)
	{
		if (!isInstalled)
		{
			return false;
		}

		return IsFillBot(clientNum);
	}

	bool BotAI::HasRoomForBot()
	{
		if (!isInstalled)
		{
			return true;
		}

		return HasScriptRoomForBot();
	}

	BotAI::BotAI()
	{
		if (!HasForcedBotNames())
		{
			TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
		}

		BindAddresses();

		if (!InstallControl())
		{
			Logger::Error("bots: the server frame or the isitemunlocked slot does not read as expected, the bot AI stays off\n");
			return;
		}

		isInstalled = true;

		if (Dedicated::IsEnabled())
		{
			return;
		}

		if (!InstallLobby())
		{
			Logger::Error("bots: the lobby's playercard or player count call does not read as expected, the lobby list shows members only\n");
		}
	}
}
