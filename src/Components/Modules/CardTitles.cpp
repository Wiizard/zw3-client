#include "STDInclude.hpp"

#include "CardTitles.hpp"
#include "ClientSlots.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "ServerCommands.hpp"

namespace Components
{
	extern "C"
	{
		void GetPlayerCardClientInfoStub();
		std::uintptr_t CardTitles_GetPlayerCardClientInfoNext = 0;

		int CardTitles_GetPlayerCardClientInfo(int lookupResult, Game::PlayerCardData* data)
		{
			return CardTitles::GetPlayerCardClientInfo(lookupResult, data);
		}
	}

	char CardTitles::customTitles[Game::MAX_CLIENTS][18];
	Dvar::Var CardTitles::customTitle;


	constexpr std::uintptr_t GetPlayerCardClientData_Title = 0x140251B4A;
	constexpr std::uintptr_t GetPlayerCardClientData_StoreResult = 0x140251C13;
	static const std::uint8_t cardTitleCase[] = { 0x8B, 0x43, 0x04, 0xE9, 0xC1, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t RunOp_TableLookupByRowCall = 0x140256EEF;
	constexpr std::uintptr_t StringTable_GetColumnValueForRow = 0x140280D70;

	static Utils::Hook cardTitleHook;
	static Utils::Hook tableLookupHook;

	Game::clientInfo_t* CardTitles::GetClientByIndex(std::uint32_t index)
	{
		return ClientSlots::CgameClientInfo(index);
	}

	int CardTitles::GetPlayerCardClientInfo(int lookupResult, Game::PlayerCardData* data)
	{
		auto result = static_cast<std::uint32_t>(lookupResult);

		const auto* username = Dvar::Name.Get<const char*>();

		if (std::strcmp(data->name, username) == 0)
		{
			result += 0xFE000000;
		}
		else
		{
			for (std::size_t i = 0; i < ClientSlots::CgameClientCount(); ++i)
			{
				const auto* c = GetClientByIndex(static_cast<std::uint32_t>(i));

				if (!std::strcmp(data->name, c->name))
				{
					result += 0xFF000000;
					result += static_cast<std::uint32_t>(i) * 0x10000;
					break;
				}
			}
		}

		return static_cast<int>(result);
	}

	const char* CardTitles::TableLookupByRow_Hk(const Game::StringTable* table, int row, int column)
	{
		const std::uint8_t prefix = (row >> (8 * 3)) & 0xFF;
		const std::uint8_t data = (row >> (8 * 2)) & 0xFF;

		const auto getColumnValueForRow = reinterpret_cast<const char*(*)(const Game::StringTable*, int, int)>(Utils::Hook::Rebase(StringTable_GetColumnValueForRow));

		if (data >= Game::MAX_CLIENTS)
		{
			return getColumnValueForRow(table, row, column);
		}

		if (prefix != 0x00 && table && _stricmp(table->name, "mp/cardTitleTable.csv") == 0)
		{
			if (column == 1)
			{
				if (prefix == 0xFE)
				{
					if (!customTitle.Get<std::string>().empty())
					{
						return Utils::String::VA("\x15%s", customTitle.Get<const char*>());
					}
				}
				else if (prefix == 0xFF)
				{
					if (customTitles[data][0] != '\0')
					{
						return Utils::String::VA("\x15%s", customTitles[data]);
					}
				}
			}

			row = static_cast<std::int32_t>(static_cast<std::uint16_t>(row));
		}

		return getColumnValueForRow(table, row, column);
	}

	void CardTitles::SendCustomTitlesToClients()
	{
		constexpr std::size_t titlesPerCommand = 16;

		const auto count = ClientSlots::SentClientCount();
		const bool isWide = ClientSlots::IsServerWide();
		const auto step = isWide ? titlesPerCommand : count;

		for (std::size_t first = 0; first < count; first += step)
		{
			const auto last = std::min(first + step, count);
			std::string list;

			if (isWide)
			{
				list.append(std::format("\\r\\{}-{}", first, last));
			}

			for (std::size_t i = first; i < last; ++i)
			{
				char playerTitle[18]{};

				if (Game::svs_clients[i].userinfo[0] != '\0')
				{
					strncpy_s(playerTitle, Game::Info_ValueForKey(Game::svs_clients[i].userinfo, "customTitle"), _TRUNCATE);
				}
				else
				{
					playerTitle[0] = '\0';
				}

				list.append(std::format("\\{}\\{}", i, playerTitle));
			}

			Game::SV_GameSendServerCommand(-1, Game::SV_CMD_CAN_IGNORE, Utils::String::Format("{:c} customTitles \"{}\"", 21, list));
		}
	}

	void CardTitles::ParseCustomTitles(const char* msg)
	{
		std::size_t first = 0;
		std::size_t last = Game::MAX_CLIENTS;

		const auto* range = Game::Info_ValueForKey(msg, "r");

		if (range[0] != '\0')
		{
			const auto* separator = std::strchr(range, '-');
			first = std::min<std::size_t>(std::strtoul(range, nullptr, 10), Game::MAX_CLIENTS);
			last = separator ? std::min<std::size_t>(std::strtoul(separator + 1, nullptr, 10), Game::MAX_CLIENTS) : first;
		}

		for (std::size_t i = first; i < last; ++i)
		{
			const auto index = std::to_string(i);
			const auto* playerTitle = Game::Info_ValueForKey(msg, index.data());

			if (playerTitle[0] == '\0')
			{
				customTitles[i][0] = '\0';
			}
			else
			{
				Game::I_strncpyz(customTitles[i], playerTitle, sizeof(customTitles[0]) / sizeof(char));
			}
		}
	}

	CardTitles::CardTitles()
	{
		Events::OnDvarInit([]
		{
			customTitle = Dvar::Register("customTitle", "", Game::DVAR_USERINFO | Game::DVAR_ARCHIVE, "Custom card title");
		});

		std::memset(&customTitles, 0, sizeof(char[Game::MAX_CLIENTS][18]));

		ServerCommands::OnCommand(21, [](const Command::Params* params)
		{
			if (std::strcmp(params->Get(1), "customTitles") == 0)
			{
				if (params->Size() == 3)
				{
					ParseCustomTitles(params->Get(2));
					return true;
				}
			}

			return false;
		});

		if (!Utils::Hook::MatchesBytes(GetPlayerCardClientData_Title, cardTitleCase, sizeof(cardTitleCase))
			|| !Utils::Hook::BranchesTo(RunOp_TableLookupByRowCall, StringTable_GetColumnValueForRow, false))
		{
			Logger::Error("cardtitles: GetPlayerCardClientData or RunOp does not read as expected, no custom titles\n");
			return;
		}

		CardTitles_GetPlayerCardClientInfoNext = Utils::Hook::Rebase(GetPlayerCardClientData_StoreResult);

		bool isSeated = cardTitleHook.Initialize(GetPlayerCardClientData_Title, GetPlayerCardClientInfoStub, HOOK_CALL)->Install()->IsInstalled();

		isSeated = tableLookupHook.Initialize(RunOp_TableLookupByRowCall, reinterpret_cast<void*>(TableLookupByRow_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			cardTitleHook.Uninstall();
			tableLookupHook.Uninstall();

			Logger::Error("cardtitles: could not seat every hook, no custom titles\n");
			return;
		}

		cardTitleHook.Quick();
		tableLookupHook.Quick();
	}
}
