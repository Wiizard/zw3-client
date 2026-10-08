#pragma once

#include "Network.hpp"
#include "UIScript.hpp"

namespace Components
{
	class ServerInfo : public Component
	{
	public:
		ServerInfo();

		static Utils::InfoString GetHostInfo();
		static Utils::InfoString GetInfo();

		static int GetProtocol();

		static std::string ParseChallenge(const std::string& data);

	private:
		class Container
		{
		public:
			class Player
			{
			public:
				int clientNum = -1;
				int ping = 0;
				int score = 0;
				int kills = 0;
				int downs = 0;
				int revives = 0;
				int deaths = 0;
				int down = 0;
				float downProgress = 0.0f;
				int rank = -1;
				int prestige = 0;
				std::string survivalTime;
				std::string name;
				std::string icon;
				std::string status;
			};

			unsigned int currentPlayer = 0;
			std::vector<Player> playerList;
			Network::Address target;
		};

		static Container playerContainer;

		static void ServerStatus(const UIScript::Token& token);
		static void RefreshScoreboard(const UIScript::Token& token);
		static void ApplyScoreboardSnapshot(const std::string& data);
		static void WriteScoreboardRowDvars();
		static void NormalisePlayerDownState(Container::Player& player);

		static unsigned int GetPlayerCount();
		static const char* GetPlayerText(unsigned int index, int column);
		static void SelectPlayer(unsigned int index);

		static void HandleGetStatus(Network::Address& address, const std::string& data);
		static void HandleStatusResponse(Network::Address& address, const std::string& data);
	};
}
