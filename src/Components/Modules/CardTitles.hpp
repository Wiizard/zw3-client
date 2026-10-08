#pragma once

#include "Dvar.hpp"

namespace Components
{
	class CardTitles : public Component
	{
	public:
		AssertOffset(Game::PlayerCardData, name, 0x1C);

		static void SendCustomTitlesToClients();

		CardTitles();

		static int GetPlayerCardClientInfo(int lookupResult, Game::PlayerCardData* data);

	private:
		static Dvar::Var customTitle;
		static char customTitles[Game::MAX_CLIENTS][18];

		static Game::clientInfo_t* GetClientByIndex(std::uint32_t index);
		static const char* TableLookupByRow_Hk(const Game::StringTable* table, int row, int column);

		static void ParseCustomTitles(const char* msg);
	};
}
