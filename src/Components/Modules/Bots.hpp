#pragma once

namespace Components
{
	class Bots : public Component
	{
	public:
		Bots();

		static void SV_DirectConnect_Full_Check();

		static bool BotAiAction(Game::client_s* cl);

		static int ScoreboardPing(int clientNum);

		static std::string GetBotDisplayName(int clientNum);
		static std::string GetBotIcon(int clientNum);
		static bool IsBotClient(int clientNum);
		static void ResetBotScoreboardData();

	private:
		static std::array<std::string, Game::MAX_CLIENTS> botDisplayNames;
		static std::array<std::string, Game::MAX_CLIENTS> botIcons;

		static bool SynchronizeBotIdentity(int clientNum, bool refreshClientInfo);

		using botData = std::pair<std::string, std::string>;

		static const Game::dvar_t* sv_randomBotNames;
		static const Game::dvar_t* sv_replaceBots;

		static std::size_t botDataIndex;

		static std::vector<botData> remoteBotNames;

		static void UpdateBotNames();

		static std::vector<botData> LoadBotNames();
		static int BuildConnectString(char* buffer, const char* connectString, int num, int, int protocol, int checksum, int statVer, int stats, int port);

		static void Spawn(unsigned int count);

		static void GScr_isTestClient(Game::scr_entref_t entref);
		static void AddScriptMethods();

		static void G_SelectWeaponIndex(int clientNum, unsigned int iWeaponIndex);
		static void G_SelectWeaponIndex_Hk(int clientNum, unsigned int iWeaponIndex);

		static bool Player_UpdateActivate_stub(int);

		static int SV_GetClientPing_Hk(int clientNum);

		static bool IsFull();

		static void CleanBotArray();

		static void AddServerCommands();
	};
}
