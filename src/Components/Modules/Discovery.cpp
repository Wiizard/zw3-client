#include "STDInclude.hpp"

#include "Discovery.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "ServerInfo.hpp"
#include "ServerList.hpp"

namespace Components
{
	std::atomic_bool Discovery::isPerforming = false;
	std::jthread Discovery::thread;
	std::string Discovery::challenge;

	Dvar::Var Discovery::net_discoveryPortRangeMin;
	Dvar::Var Discovery::net_discoveryPortRangeMax;

	void Discovery::Perform()
	{
		isPerforming = true;
	}

	Discovery::Discovery()
	{
		Events::OnDvarInit([]
		{
			net_discoveryPortRangeMin = Dvar::Register("net_discoveryPortRangeMin", 25000, 0, 65535, Game::DVAR_NONE, "Minimum scan range port for local server discovery");
			net_discoveryPortRangeMax = Dvar::Register("net_discoveryPortRangeMax", 35000, 1, 65536, Game::DVAR_NONE, "Maximum scan range port for local server discovery");
		});

		isPerforming = false;
		thread = std::jthread([](const std::stop_token& stopToken)
		{
			Game::Com_InitThreadData();

			while (!stopToken.stop_requested())
			{
				if (isPerforming)
				{
					const auto start = Game::Sys_Milliseconds();

					Logger::Print("Starting local server discovery...\n");

					challenge = Utils::Cryptography::Rand::GenerateChallenge();

					const auto minPort = net_discoveryPortRangeMin.Get<int>();
					const auto maxPort = net_discoveryPortRangeMax.Get<int>();
					Network::BroadcastRange(minPort, maxPort, std::format("discovery {}", challenge));

					Logger::Print("Discovery sent within {}ms, awaiting responses...\n", Game::Sys_Milliseconds() - start);

					isPerforming = false;
				}

				std::this_thread::sleep_for(50ms);
			}
		});

		Network::OnPacket("discovery", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			if (address.IsSelf())
			{
				return;
			}

			if (!address.IsLocal())
			{
				Logger::Print("Received discovery request from non-local address: {}\n", address.GetString());
				return;
			}

			Logger::Print("Received discovery request from {}\n", address.GetString());
			Network::SendCommand(address, "discoveryResponse", data);
		});

		Network::OnPacket("discoveryResponse", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			if (address.IsSelf())
			{
				return;
			}

			if (!address.IsLocal())
			{
				Logger::Print("Received discovery response from non-local address: {}\n", address.GetString());
				return;
			}

			if (ServerInfo::ParseChallenge(data) != challenge)
			{
				Logger::Print("Received discovery with invalid challenge from: {}\n", address.GetString());
				return;
			}

			Logger::Print("Received discovery response from: {}\n", address.GetString());

			if (ServerList::IsOfflineList())
			{
				ServerList::InsertRequest(address);
			}
		});
	}
}
