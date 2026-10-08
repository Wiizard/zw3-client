#pragma once

namespace Components
{
	class ClanTags : public Component
	{
	public:
		static constexpr std::size_t MAX_CLAN_NAME_LENGTH = 5;

		ClanTags();

		static const char* GetClanTagWithName(int clientNum, const char* playerName);

		static void SendClanTagsToClients();

		static void CL_SanitizeClanName();

		static void ClientUserinfoChanged(const char* s, int clientNum);

	private:
		static const Game::dvar_t* clanName;

		static char clientState[Game::MAX_CLIENTS][MAX_CLAN_NAME_LENGTH];

		static void ParseClanTags(const char* infoString);

		static int CL_FilterChar(unsigned char input);

		static char* GamerProfile_GetClanName(int controllerIndex);

		static void Dvar_InfoString_Stub(char* s, const char* key, const char* value);

		static int PartyClient_Frame_Stub(const char* s0, const char* s1);
		static void Party_UpdateClanName_Stub(Game::PartyData* party, const char* clanAbbrev);

		static void PlayerCards_SetCachedPlayerData(Game::PlayerCardData* data, int clientNum);
		static void PlayerCards_SetCachedPlayerData_Stub(char* name, const char* source, int size);

		static Game::PlayerCardData* PlayerCards_GetLiveProfileDataForClient_Stub(unsigned int clientIndex);
		static Game::PlayerCardData* PlayerCards_GetLiveProfileDataForController_Stub(unsigned int controllerIndex);
	};
}
