#include "STDInclude.hpp"

#include "ScriptPatches.hpp"
#include "Script.hpp"
#include "UsePress.hpp"
#include "../Events.hpp"
#include "../Logger.hpp"

namespace Components::GSC
{
	namespace
	{
		constexpr std::uintptr_t ClientThinkReal = 0x140193190;
		constexpr std::uintptr_t ClientThinkRealCalls[] = { 0x140193140, 0x1401942E7, 0x14019687B };
		constexpr int useButtons = Game::CMD_BUTTON_ACTIVATE | Game::CMD_BUTTON_USE_RELOAD;
		std::array<UsePress, Game::MAX_CLIENTS> usePresses;
		std::array<Utils::Hook, std::size(ClientThinkRealCalls)> useInputHooks;
		bool useInputInstalled = false;

		void ClientThinkReal_Hk(Game::gentity_s* entity, Game::usercmd_s* command)
		{
			Utils::Hook::Call<void(Game::gentity_s*, Game::usercmd_s*)>(ClientThinkReal)(entity, command);
			const auto clientNum = entity - Game::g_entities;
			if (entity->client && clientNum >= 0 && clientNum < static_cast<std::ptrdiff_t>(usePresses.size()))
			{
				// Observe every accepted command, including releases between script polls.
				usePresses[clientNum].Update((entity->client->buttons & useButtons) != 0);
			}
		}

		void InstallUseInput()
		{
			for (const auto site : ClientThinkRealCalls)
				if (!Utils::Hook::BranchesTo(site, ClientThinkReal, HOOK_CALL)) return;
			for (std::size_t index = 0; index < useInputHooks.size(); ++index)
			{
				if (!useInputHooks[index].Initialize(ClientThinkRealCalls[index], ClientThinkReal_Hk, HOOK_CALL)->Install()->IsInstalled())
				{
					for (auto& hook : useInputHooks) hook.Uninstall();
					return;
				}
			}
			for (auto& hook : useInputHooks) hook.Quick();
			useInputInstalled = true;
			Events::OnVMShutdown([] { usePresses = {}; });
			Events::OnClientDisconnect([](const int clientNum)
			{
				if (clientNum >= 0 && clientNum < static_cast<int>(usePresses.size())) usePresses[clientNum] = {};
			});
		}

		void ConsumeUse(const Game::scr_entref_t entref)
		{
			auto* const entity = Script::Scr_GetPlayerEntity(entref);
			if (!entity || entref.entnum >= usePresses.size())
			{
				Game::Scr_AddBool(false);
				return;
			}
			const auto* const client = entity->client;
			const bool active = ((client->buttons | client->buttonsSinceLastFrame) & useButtons) != 0;
			Game::Scr_AddBool(useInputInstalled ? usePresses[entref.entnum].Consume(active) : active);
		}
	}

	constexpr std::uintptr_t g_hudelems = 0x141781120;
	constexpr std::size_t hudElemSize = 180;
	constexpr std::size_t hudElemText = 132;
	constexpr int offset = 511;

	constexpr std::uintptr_t tableLookupIStringByRowEntry = 0x1404233E0;
	constexpr std::uintptr_t Scr_TableLookupByRow = 0x14017FBC0;

	constexpr std::uintptr_t StringTable_GetAsset = 0x140280D50;
	constexpr std::uintptr_t StringTable_GetColumnValueForRow = 0x140280D70;

	constexpr std::uintptr_t SV_SetConfigstring = 0x14023ACB0;

	int* ScriptPatches::HECmd_GetHudElemText(Game::scr_entref_t entref)
	{
		if (entref.classnum != 1)
		{
			Script::Scr_ObjectError("not a hud element");
			return nullptr;
		}

		assert(entref.entnum < 1024);

		const std::uintptr_t hud = Utils::Hook::Rebase(g_hudelems) + hudElemSize * entref.entnum;
		return reinterpret_cast<int*>(hud + hudElemText);
	}

	void ScriptPatches::Scr_TableLookupIStringByRow_Hk()
	{
		if (Game::Scr_GetNumParam() < 3)
		{
			Script::Scr_Error("USAGE: tableLookupIStringByRow( filename, rowNum, returnValueColumnNum )");
			return;
		}

		const auto* fileName = Game::Scr_GetString(0);
		const auto rowNum = Game::Scr_GetInt(1);
		const auto returnValueColumnNum = Game::Scr_GetInt(2);

		const Game::StringTable* table = nullptr;
		reinterpret_cast<void(*)(const char*, const Game::StringTable**)>(Utils::Hook::Rebase(StringTable_GetAsset))(fileName, &table);

		if (!table)
		{
			Script::Scr_ParamError(0, Utils::String::VA("%s does not exist", fileName));
			return;
		}

		const auto* value = reinterpret_cast<const char*(*)(const Game::StringTable*, int, int)>(
			Utils::Hook::Rebase(StringTable_GetColumnValueForRow))(table, rowNum, returnValueColumnNum);

		Game::Scr_AddIString(value);
	}

	constexpr std::uintptr_t SV_MapExists = 0x1402362A0;
	constexpr std::uintptr_t levelExitState = 0x141869910;
	constexpr std::uintptr_t levelSavePersist = 0x141868078;
	constexpr int levelExitStateMap = 2;

	static void GScr_Map()
	{
		if (!Game::Scr_GetNumParam())
		{
			return;
		}

		const char* const name = Game::Scr_GetString(0);

		if (!reinterpret_cast<int(*)(const char*)>(Utils::Hook::Rebase(SV_MapExists))(name))
		{
			return;
		}

		int* const exitState = reinterpret_cast<int*>(Utils::Hook::Rebase(levelExitState));
		int* const savePersist = reinterpret_cast<int*>(Utils::Hook::Rebase(levelSavePersist));

		if (*exitState == levelExitStateMap)
		{
			Script::Scr_Error("map already called");
		}
		else if (*exitState)
		{
			Script::Scr_Error("exitlevel already called");
		}

		*exitState = levelExitStateMap;
		*savePersist = 0;

		if (Game::Scr_GetNumParam() > 1)
		{
			*savePersist = Game::Scr_GetInt(1);
		}

		Game::Cbuf_AddText(0, Utils::String::VA("map %s\n", name));
	}

	ScriptPatches::ScriptPatches()
	{
		InstallUseInput();
		Script::AddMethod("ZW3ConsumeUse", ConsumeUse);
		Script::AddFunction("map", GScr_Map);

		if (Utils::Hook::Get<std::uintptr_t>(tableLookupIStringByRowEntry + 8) != Utils::Hook::Rebase(Scr_TableLookupByRow))
		{
			Logger::Error("scriptpatches: tablelookupistringbyrow does not read as expected, it stays the engine's\n");
		}
		else
		{
			Utils::Hook::Set<Game::BuiltinFunction>(tableLookupIStringByRowEntry + 8, Scr_TableLookupIStringByRow_Hk);
		}

		Script::AddMethod("ClearHudText", [](Game::scr_entref_t entref) -> void
		{
			const int* text = HECmd_GetHudElemText(entref);

			if (*text)
			{
				reinterpret_cast<void(*)(int, const char*)>(Utils::Hook::Rebase(SV_SetConfigstring))(*text + offset, nullptr);
			}
		});
	}
}
