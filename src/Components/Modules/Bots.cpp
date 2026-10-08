#include "STDInclude.hpp"

#include <Utils/InfoString.hpp>

#include "Bots.hpp"
#include "BotAI/BotAI.hpp"
#include "CharacterAssignments.hpp"
#include "Command.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Scheduler.hpp"

#include "GSC/Script.hpp"

namespace Components
{
	constexpr std::size_t maxNameLength = 16;

	constexpr std::size_t maxClanNameLength = 5;

	const Game::dvar_t* Bots::sv_randomBotNames;
	const Game::dvar_t* Bots::sv_replaceBots;

	std::size_t Bots::botDataIndex;

	std::vector<Bots::botData> Bots::remoteBotNames;

	std::array<std::string, Game::MAX_CLIENTS> Bots::botDisplayNames;
	std::array<std::string, Game::MAX_CLIENTS> Bots::botIcons;

	struct PendingBotIdentity
	{
		CharacterAssignments::Character character = CharacterAssignments::Character::None;
		int started = 0;
	};

	constexpr int pendingBotIdentityMs = 10000;

	static std::unordered_map<int, PendingBotIdentity> pendingBotIdentities;

	static void CleanupPendingBotIdentities()
	{
		const int now = Game::Sys_Milliseconds();

		for (auto it = pendingBotIdentities.begin(); it != pendingBotIdentities.end();)
		{
			if (it->second.started <= 0 || now - it->second.started >= pendingBotIdentityMs)
			{
				it = pendingBotIdentities.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	static bool IsPendingBotCharacter(const CharacterAssignments::Character character, const int ignoredQport = -1)
	{
		if (!CharacterAssignments::IsValid(character))
		{
			return false;
		}

		CleanupPendingBotIdentities();

		for (const auto& [qport, pending] : pendingBotIdentities)
		{
			if (qport != ignoredQport && pending.character == character)
			{
				return true;
			}
		}

		return false;
	}

	static CharacterAssignments::Character GetPendingBotCharacter(const int qport)
	{
		CleanupPendingBotIdentities();

		if (const auto found = pendingBotIdentities.find(qport); found != pendingBotIdentities.end())
		{
			return found->second.character;
		}

		return CharacterAssignments::Character::None;
	}

	static void SetPendingBotCharacter(const int qport, const CharacterAssignments::Character character)
	{
		if (qport < 0 || !CharacterAssignments::IsValid(character))
		{
			return;
		}

		pendingBotIdentities[qport] = { character, Game::Sys_Milliseconds() };
	}

	static void ClearPendingBotCharacter(const int qport)
	{
		if (qport >= 0)
		{
			pendingBotIdentities.erase(qport);
		}
	}

	static void ResetDownState(const int clientNum)
	{
		Game::Dvar_SetFromStringByName(Utils::String::VA("zw3_sb_down_%d", clientNum), "0");
		Game::Dvar_SetFromStringByName(Utils::String::VA("zw3_sb_down_progress_%d", clientNum), "0");
	}

	static std::string BotDisplayName(const CharacterAssignments::Character character)
	{
		return std::format("[BOT] {}", CharacterAssignments::ToString(character));
	}

	bool Bots::SynchronizeBotIdentity(const int clientNum, const bool refreshClientInfo)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return false;
		}

		auto& client = Game::svs_clients[clientNum];

		if (client.header.state < Game::CS_CONNECTED || !client.bIsTestClient)
		{
			return false;
		}

		auto character = CharacterAssignments::GetClientCharacterId(clientNum);

		if (!CharacterAssignments::IsValid(character))
		{
			character = CharacterAssignments::ResolveClientCharacter(clientNum);
		}

		if (!CharacterAssignments::IsValid(character))
		{
			return false;
		}

		const std::string characterName = CharacterAssignments::ToString(character);
		const std::string displayName = BotDisplayName(character);

		botDisplayNames[clientNum] = displayName;
		botIcons[clientNum] = characterName;

		for (int slot = 1; slot <= CharacterAssignments::maxPartySize; ++slot)
		{
			const auto owner = Dvar::Var(std::format("character_{}_player", slot)).Get<std::string>();

			if (owner == displayName || owner == "None" || owner.empty())
			{
				Dvar::Var(std::format("character_{}", slot)).Set(characterName);
				Dvar::Var(std::format("character_{}_player", slot)).Set(displayName);
				break;
			}
		}

		Utils::InfoString info(client.userinfo);

		const bool hasIdentityChanged = _stricmp(info.Get("name").data(), displayName.data()) != 0
			|| _stricmp(info.Get("zw3char").data(), characterName.data()) != 0
			|| _stricmp(client.name, displayName.data()) != 0;

		if (!hasIdentityChanged)
		{
			return false;
		}

		info.Set("name", displayName);
		info.Set("zw3char", characterName);
		Game::I_strncpyz(client.userinfo, info.Build().data(), sizeof(client.userinfo));

		if (refreshClientInfo && client.header.state >= Game::CS_ACTIVE && client.gentity && client.gentity->client)
		{
			Game::ClientUserinfoChanged(clientNum);
		}
		else
		{
			Game::I_strncpyz(client.name, displayName.data(), sizeof(client.name));
		}

		return true;
	}

	void Bots::ResetBotScoreboardData()
	{
		botDataIndex = 0;
		pendingBotIdentities.clear();

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			if (Game::svs_clients[clientNum].header.state != Game::CS_FREE)
			{
				continue;
			}

			botDisplayNames[clientNum].clear();
			botIcons[clientNum].clear();
			CharacterAssignments::ClearClientSlot(clientNum);
			ResetDownState(clientNum);
		}
	}

	std::string Bots::GetBotDisplayName(const int clientNum)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return {};
		}

		if (!botDisplayNames[clientNum].empty())
		{
			return botDisplayNames[clientNum];
		}

		if (Game::svs_clients[clientNum].bIsTestClient && Game::svs_clients[clientNum].name[0])
		{
			return Game::svs_clients[clientNum].name;
		}

		return {};
	}

	std::string Bots::GetBotIcon(const int clientNum)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return {};
		}

		const auto character = CharacterAssignments::GetClientCharacterId(clientNum);

		if (CharacterAssignments::IsValid(character))
		{
			return CharacterAssignments::ToString(character);
		}

		return botIcons[clientNum];
	}

	bool Bots::IsBotClient(const int clientNum)
	{
		if (!CharacterAssignments::IsClientNum(clientNum))
		{
			return false;
		}

		return Game::svs_clients[clientNum].bIsTestClient != 0;
	}

	static const Utils::Hook::LeaSite connectStringLea = { 0x140236D85, Utils::Hook::leaRdx, 0x14039E1E0 };

	constexpr std::uintptr_t SV_AddTestClient_SprintfCall = 0x140236DAB;
	static const std::uint8_t sprintfCall[] = { 0xE8, 0x10, 0x97, 0xDF, 0xFF };

	constexpr std::uintptr_t SV_BotUserMove_Begin = 0x14023D711;
	constexpr std::uintptr_t SV_BotUserMove_Next = 0x14023D93D;
	static const std::uint8_t botUserMoveBegin[] = { 0x33, 0xC0, 0x0F, 0x57, 0xC0 };
	static const std::uint8_t botUserMoveNext[] = { 0x41, 0xFF, 0xC6 };

	constexpr std::uintptr_t G_SelectWeaponIndex = 0x140188710;
	constexpr std::uintptr_t G_SelectWeaponIndexCalls[] = { 0x140164780, 0x14016F036, 0x14019741E };

	constexpr std::uintptr_t Player_UpdateActivate_IsClientBotCall = 0x14018BE03;
	constexpr std::uintptr_t SV_IsClientBot = 0x14023C8F0;

	constexpr std::uintptr_t SV_GetClientPingCalls[] = { 0x1401999A6, 0x140199209 };
	constexpr std::uintptr_t SV_GetClientPing = 0x140233420;

	constexpr std::uintptr_t SV_Netchan_Transmit = 0x14023EE60;
	constexpr std::uintptr_t SV_Netchan_TransmitNextFragment = 0x14023EF80;
	constexpr std::uintptr_t SV_SendMessageToClient_TransmitCall = 0x14024253F;
	constexpr std::uintptr_t SV_Netchan_TransmitNextFragmentCalls[] = { 0x140238CC1, 0x140241CA2 };
	constexpr std::uintptr_t sentFrameBytes = 0x14650D188;
	constexpr std::uintptr_t sentWindowBytes = 0x14650D18C;

	static Utils::Hook hooks[11];

	struct BotMovementInfo
	{
		std::int32_t buttons;
		std::int8_t forward;
		std::int8_t right;
		std::uint16_t weapon;
		std::uint16_t lastAltWeapon;
		std::uint8_t meleeDist;
		float meleeYaw;
		std::int8_t remoteAngles[2];
		float angles[3];
		bool active;
	};

	static BotMovementInfo botAi[Game::MAX_CLIENTS];

	struct BotAction
	{
		std::string action;
		std::int32_t key;
	};

	static const BotAction botActions[] =
	{
		{ "gostand", Game::CMD_BUTTON_UP },
		{ "gocrouch", Game::CMD_BUTTON_CROUCH },
		{ "goprone", Game::CMD_BUTTON_PRONE },
		{ "fire", Game::CMD_BUTTON_ATTACK },
		{ "melee", Game::CMD_BUTTON_MELEE },
		{ "frag", Game::CMD_BUTTON_FRAG },
		{ "smoke", Game::CMD_BUTTON_OFFHAND_SECONDARY },
		{ "reload", Game::CMD_BUTTON_RELOAD },
		{ "sprint", Game::CMD_BUTTON_SPRINT },
		{ "leanleft", Game::CMD_BUTTON_LEAN_LEFT },
		{ "leanright", Game::CMD_BUTTON_LEAN_RIGHT },
		{ "ads", Game::CMD_BUTTON_ADS | Game::CMD_BUTTON_THROW },
		{ "holdbreath", Game::CMD_BUTTON_BREATH },
		{ "usereload", Game::CMD_BUTTON_USE_RELOAD },
		{ "activate", Game::CMD_BUTTON_ACTIVATE },
		{ "remote", Game::CMD_BUTTON_REMOTE },
	};

	extern "C"
	{
		void SV_BotUserMove_Stub();
		std::uintptr_t Bots_BotUserMoveNext = 0;

		bool Bots_BotUserMove(Game::client_s* cl)
		{
			return Bots::BotAiAction(cl);
		}
	}

	static int AngleToShort(float angle)
	{
		return static_cast<int>(angle * (USHRT_MAX + 1) / 360.0f) & USHRT_MAX;
	}

	static bool IsServerRunning()
	{
		return Dvar::Var("sv_running").Get<bool>();
	}

	void Bots::UpdateBotNames()
	{
		const auto masterPort = (*Game::com_masterPort)->current.integer;
		const auto* masterServerName = (*Game::com_masterServerName)->current.string;

		Network::Address master(Utils::String::VA("%s:%u", masterServerName, masterPort));

		Logger::Print("Getting bots...\n");
		Network::Send(master, "getbots");
	}

	std::vector<Bots::botData> Bots::LoadBotNames()
	{
		std::vector<botData> result;

		FileSystem::File bots("bots.txt");

		if (!bots.Exists())
		{
			return result;
		}

		auto data = Utils::String::Split(bots.GetBuffer(), '\n');

		for (auto& entry : data)
		{
			Utils::String::Replace(entry, "\r", "");
			Utils::String::Trim(entry);

			if (entry.empty())
			{
				continue;
			}

			std::string clanAbbrev;

			if (const auto pos = entry.find(','); pos != std::string::npos)
			{
				if ((pos + 1) < entry.size())
				{
					clanAbbrev = entry.substr(pos + 1, maxClanNameLength - 1);
				}

				entry = entry.substr(0, pos);
			}

			entry = entry.substr(0, maxNameLength - 1);

			result.emplace_back(entry, clanAbbrev);
		}

		return result;
	}

	int Bots::BuildConnectString(char* buffer, const char* connectString, int num, int, int protocol, int checksum, int statVer, int stats, int port)
	{
		const char* pendingName = BotAI::BotAI::PendingName();
		if (pendingName)
		{
			return _snprintf_s(buffer, 0x400, _TRUNCATE, connectString, num, pendingName, "None", "", protocol, checksum, statVer, stats, port);
		}

		auto selected = GetPendingBotCharacter(port);

		if (!CharacterAssignments::IsValid(selected))
		{
			for (int slot = 1; slot <= CharacterAssignments::maxPartySize; ++slot)
			{
				const auto owner = Dvar::Var(std::format("character_{}_player", slot)).Get<std::string>();

				if (!owner.starts_with("[BOT]"))
				{
					continue;
				}

				const auto candidate = CharacterAssignments::Parse(Dvar::Var(std::format("character_{}", slot)).Get<std::string>());

				if (CharacterAssignments::IsValid(candidate) && !CharacterAssignments::IsCharacterUsed(candidate) && !IsPendingBotCharacter(candidate, port))
				{
					selected = candidate;
					break;
				}
			}
		}

		if (!CharacterAssignments::IsValid(selected))
		{
			for (const auto candidate : CharacterAssignments::characters)
			{
				if (!CharacterAssignments::IsCharacterUsed(candidate) && !IsPendingBotCharacter(candidate, port))
				{
					selected = candidate;
					break;
				}
			}
		}

		std::string botName = std::format("[BOT] Bot{}", num);
		std::string botIcon = "None";

		if (CharacterAssignments::IsValid(selected))
		{
			SetPendingBotCharacter(port, selected);
			botIcon = CharacterAssignments::ToString(selected);
			botName = BotDisplayName(selected);
		}

		return _snprintf_s(buffer, 0x400, _TRUNCATE, connectString, num, botName.data(), botIcon.data(), "", protocol, checksum, statVer, stats, port);
	}

	void Bots::Spawn(unsigned int count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			Scheduler::Once([]
			{
				if (!BotAI::BotAI::HasRoomForBot())
				{
					Logger::Warning("the script VM has no room for another player, not adding a bot\n");
					return;
				}

				auto* ent = Game::SV_AddTestClient();

				if (!ent)
				{
					return;
				}

				Scheduler::Once([ent]
				{
					Game::Scr_AddString("autoassign");
					Game::Scr_AddString("team_marinesopfor");
					Game::Scr_Notify(ent, static_cast<std::uint16_t>(Game::SL_GetString("menuresponse", 0)), 2);

					Scheduler::Once([ent]
					{
						Game::Scr_AddString(Utils::String::Format("class{}", std::rand() % 5));
						Game::Scr_AddString("changeclass");
						Game::Scr_Notify(ent, static_cast<std::uint16_t>(Game::SL_GetString("menuresponse", 0)), 2);
					}, Scheduler::Pipeline::SERVER, 1s);

				}, Scheduler::Pipeline::SERVER, 1s);

			}, Scheduler::Pipeline::SERVER, 500ms * (i + 1));
		}
	}

	void Bots::GScr_isTestClient(const Game::scr_entref_t entref)
	{
		const auto* ent = Game::GetEntity(entref);

		if (!ent->client)
		{
			GSC::Script::Scr_Error("isTestClient: entity must be a player entity");
			return;
		}

		Game::Scr_AddBool(Game::SV_IsTestClient(ent->s.number) != 0);
	}

	void Bots::AddScriptMethods()
	{
		GSC::Script::AddMethod("GetZW3Character", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!ent)
			{
				Game::Scr_AddString("None");
				return;
			}

			auto character = CharacterAssignments::ResolveClientCharacter(ent->s.number);

			if (!CharacterAssignments::IsValid(character))
			{
				character = CharacterAssignments::GetClientCharacterId(ent->s.number);
			}

			Game::Scr_AddString(CharacterAssignments::ToString(character));
		});

		GSC::Script::AddFunction("SetZW3BotTarget", []
		{
			const int requestedBots = std::clamp(Game::Scr_GetInt(0), 0, CharacterAssignments::maxPartySize - 1);
			int capacity = 0;

			if (Game::Dvar_FindVar("party_currentPlayers"))
			{
				capacity = Dvar::Var("party_currentPlayers").Get<int>();
			}

			if (capacity <= 0)
			{
				capacity = CharacterAssignments::CountConnectedRealPlayers() + requestedBots;
			}

			CharacterAssignments::SetDesiredPartySize(capacity);
		});

		GSC::Script::AddFunction("GetZW3BotDeficit", []
		{
			const int desired = CharacterAssignments::GetDesiredBotCount(Game::Sys_Milliseconds());
			const int current = CharacterAssignments::CountReservedBots();
			Game::Scr_AddInt(std::max(0, desired - current));
		});

		GSC::Script::AddMethMultiple(GScr_isTestClient, false, {"IsTestClient", "IsBot"});

		GSC::Script::AddMethod("BotStop", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotStop: Can only call on a bot!");
				return;
			}

			ZeroMemory(&botAi[entref.entnum], sizeof(BotMovementInfo));
			botAi[entref.entnum].weapon = static_cast<std::uint16_t>(ent->client->ps.weapCommon.weapon);
			botAi[entref.entnum].angles[0] = ent->client->ps.viewangles[0];
			botAi[entref.entnum].angles[1] = ent->client->ps.viewangles[1];
			botAi[entref.entnum].angles[2] = ent->client->ps.viewangles[2];
			botAi[entref.entnum].active = true;
		});

		GSC::Script::AddMethod("BotWeapon", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotWeapon: Can only call on a bot!");
				return;
			}

			const auto* weapon = Game::Scr_GetString(0);

			if (!weapon || !*weapon)
			{
				botAi[entref.entnum].weapon = 1;
				return;
			}

			const auto weapId = Game::G_GetWeaponIndexForName(weapon);
			botAi[entref.entnum].weapon = static_cast<std::uint16_t>(weapId);
			botAi[entref.entnum].active = true;
		});

		GSC::Script::AddMethod("BotAction", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotAction: Can only call on a bot!");
				return;
			}

			const auto* action = Game::Scr_GetString(0);

			if (!action)
			{
				GSC::Script::Scr_ParamError(0, "BotAction: Illegal parameter!");
				return;
			}

			if (action[0] != '+' && action[0] != '-')
			{
				GSC::Script::Scr_ParamError(0, "BotAction: Sign for action must be '+' or '-'");
				return;
			}

			for (std::size_t i = 0; i < std::extent_v<decltype(botActions)>; ++i)
			{
				if (Utils::String::ToLower(&action[1]) != botActions[i].action)
				{
					continue;
				}

				if (action[0] == '+')
				{
					botAi[entref.entnum].buttons |= botActions[i].key;
				}
				else
				{
					botAi[entref.entnum].buttons &= ~botActions[i].key;
				}

				botAi[entref.entnum].active = true;
				return;
			}

			GSC::Script::Scr_ParamError(0, "BotAction: Unknown action");
		});

		GSC::Script::AddMethod("BotMovement", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotMovement: Can only call on a bot!");
				return;
			}

			const auto forwardInt = std::clamp<int>(Game::Scr_GetInt(0), std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
			const auto rightInt = std::clamp<int>(Game::Scr_GetInt(1), std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

			botAi[entref.entnum].forward = static_cast<std::int8_t>(forwardInt);
			botAi[entref.entnum].right = static_cast<std::int8_t>(rightInt);
			botAi[entref.entnum].active = true;
		});

		GSC::Script::AddMethod("BotMeleeParams", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotMeleeParams: Can only call on a bot!");
				return;
			}

			GSC::Script::Scr_Error("BotMeleeParams: perk_extendedMeleeRange is not registered!");
		});

		GSC::Script::AddMethod("BotRemoteAngles", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotRemoteAngles: Can only call on a bot!");
				return;
			}

			const auto pitch = std::clamp<int>(Game::Scr_GetInt(0), std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
			const auto yaw = std::clamp<int>(Game::Scr_GetInt(1), std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

			botAi[entref.entnum].remoteAngles[0] = static_cast<std::int8_t>(pitch);
			botAi[entref.entnum].remoteAngles[1] = static_cast<std::int8_t>(yaw);
			botAi[entref.entnum].active = true;
		});

		GSC::Script::AddMethod("BotAngles", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			if (!Game::SV_IsTestClient(ent->s.number))
			{
				GSC::Script::Scr_Error("BotAngles: Can only call on a bot!");
				return;
			}

			const auto pitch = Game::Scr_GetFloat(0);
			const auto yaw = Game::Scr_GetFloat(1);
			const auto roll = Game::Scr_GetFloat(2);

			botAi[entref.entnum].angles[0] = pitch;
			botAi[entref.entnum].angles[1] = yaw;
			botAi[entref.entnum].angles[2] = roll;

			botAi[entref.entnum].active = true;
		});
	}

	bool Bots::BotAiAction(Game::client_s* cl)
	{
		const auto clientNum = cl - Game::svs_clients;

		if (!botAi[clientNum].active)
		{
			return BotAI::BotAI::Think(cl);
		}

		Game::usercmd_s userCmd{};

		userCmd.serverTime = *Game::svs_time;

		userCmd.buttons = botAi[clientNum].buttons;
		userCmd.forwardmove = botAi[clientNum].forward;
		userCmd.rightmove = botAi[clientNum].right;
		userCmd.weapon = botAi[clientNum].weapon;
		userCmd.primaryWeaponForAltMode = botAi[clientNum].lastAltWeapon;
		userCmd.meleeChargeYaw = botAi[clientNum].meleeYaw;
		userCmd.meleeChargeDist = botAi[clientNum].meleeDist;
		userCmd.remoteControlAngles[0] = botAi[clientNum].remoteAngles[0];
		userCmd.remoteControlAngles[1] = botAi[clientNum].remoteAngles[1];

		userCmd.angles[0] = AngleToShort(botAi[clientNum].angles[0] - cl->gentity->client->ps.delta_angles[0]);
		userCmd.angles[1] = AngleToShort(botAi[clientNum].angles[1] - cl->gentity->client->ps.delta_angles[1]);
		userCmd.angles[2] = AngleToShort(botAi[clientNum].angles[2] - cl->gentity->client->ps.delta_angles[2]);

		Game::SV_ClientThink(cl, &userCmd);

		return true;
	}

	void Bots::G_SelectWeaponIndex(int clientNum, unsigned int iWeaponIndex)
	{
		if (botAi[clientNum].active)
		{
			botAi[clientNum].weapon = static_cast<std::uint16_t>(iWeaponIndex);
			botAi[clientNum].lastAltWeapon = 0;

			auto* def = Game::BG_GetWeaponCompleteDef(iWeaponIndex);

			if (def && def->weapDef->inventoryType == Game::WEAPINVENTORY_ALTMODE)
			{
				auto* ps = &Game::g_entities[clientNum].client->ps;
				auto numWeaps = Game::BG_GetNumWeapons();

				for (auto i = 1u; i < numWeaps; i++)
				{
					if (!Game::BG_PlayerHasWeapon(ps, i))
					{
						continue;
					}

					auto* thisDef = Game::BG_GetWeaponCompleteDef(i);

					if (!thisDef || thisDef->altWeaponIndex != iWeaponIndex)
					{
						continue;
					}

					botAi[clientNum].lastAltWeapon = static_cast<std::uint16_t>(i);
					break;
				}
			}
		}
	}

	void Bots::G_SelectWeaponIndex_Hk(int clientNum, unsigned int iWeaponIndex)
	{
		G_SelectWeaponIndex(clientNum, iWeaponIndex);

		reinterpret_cast<void(*)(int, unsigned int)>(Utils::Hook::Rebase(Components::G_SelectWeaponIndex))(clientNum, iWeaponIndex);
	}

	struct SentBytes
	{
		int frame;
		int window;
	};

	static SentBytes ReadSentBytes()
	{
		return { Utils::Hook::Get<int>(sentFrameBytes), Utils::Hook::Get<int>(sentWindowBytes) };
	}

	static void RestoreSentBytes(const SentBytes& sent)
	{
		*reinterpret_cast<int*>(Utils::Hook::Rebase(sentFrameBytes)) = sent.frame;
		*reinterpret_cast<int*>(Utils::Hook::Rebase(sentWindowBytes)) = sent.window;
	}

	static bool SV_Netchan_Transmit_Hk(Game::client_s* client, unsigned char* data, int length)
	{
		const SentBytes before = ReadSentBytes();
		const bool isSent = reinterpret_cast<bool(*)(Game::client_s*, unsigned char*, int)>(Utils::Hook::Rebase(SV_Netchan_Transmit))(client, data, length);

		if (client->bIsTestClient)
		{
			RestoreSentBytes(before);
		}

		return isSent;
	}

	static bool SV_Netchan_TransmitNextFragment_Hk(Game::client_s* client, void* netchan)
	{
		const SentBytes before = ReadSentBytes();
		const bool isSent = reinterpret_cast<bool(*)(Game::client_s*, void*)>(Utils::Hook::Rebase(SV_Netchan_TransmitNextFragment))(client, netchan);

		if (client->bIsTestClient)
		{
			RestoreSentBytes(before);
		}

		return isSent;
	}

	static bool SV_SendClientMessages_TransmitNextFragment_Hk(Game::client_s* client, void* netchan)
	{
		if (client->header.netchan.remoteAddress.type != Game::NA_LOOPBACK)
		{
			return SV_Netchan_TransmitNextFragment_Hk(client, netchan);
		}

		bool isSent = false;

		do
		{
			isSent = SV_Netchan_TransmitNextFragment_Hk(client, netchan);
		}
		while (client->header.state != Game::CS_FREE && client->header.netchan.unsentFragments != 0);

		client->nextSnapshotTime = *Game::svs_time;

		return isSent;
	}

	int Bots::ScoreboardPing(const int clientNum)
	{
		AssertIn(clientNum, Game::MAX_CLIENTS);

		if (Game::SV_IsTestClient(clientNum) && !BotAI::BotAI::OwnsBot(clientNum))
		{
			return -1;
		}

		return Game::svs_clients[clientNum].ping;
	}

	int Bots::SV_GetClientPing_Hk(const int clientNum)
	{
		return ScoreboardPing(clientNum);
	}

	bool Bots::IsFull()
	{
		auto i = 0;

		while (i < *Game::svs_clientCount)
		{
			if (Game::svs_clients[i].header.state == Game::CS_FREE)
			{
				break;
			}

			++i;
		}

		return i == *Game::svs_clientCount;
	}

	void Bots::SV_DirectConnect_Full_Check()
	{
		if (!sv_replaceBots || !sv_replaceBots->current.enabled)
		{
			return;
		}

		const Command::ServerParams params;

		if (params.Size() < 3)
		{
			return;
		}

		const Utils::InfoString incoming(params.Get(2));
		const auto xuidText = incoming.Get("xuid");
		std::uint64_t xuid = 0;

		if (!xuidText.empty())
		{
			xuid = std::strtoull(xuidText.data(), nullptr, 16);
		}

		if (xuid == 0 || CharacterAssignments::IsXuidConnected(xuid))
		{
			return;
		}

		const int now = Game::Sys_Milliseconds();

		if (CharacterAssignments::IsReplacementPendingOrRecent(xuid, now))
		{
			return;
		}

		const int connectedRealPlayers = CharacterAssignments::CountConnectedRealPlayers();
		const int desiredBotsAfterAdmission = std::clamp(CharacterAssignments::GetDesiredPartySize() - connectedRealPlayers - 1, 0, CharacterAssignments::maxPartySize - 1);
		const int currentBots = CharacterAssignments::CountReservedBots();

		if (currentBots <= desiredBotsAfterAdmission)
		{
			CharacterAssignments::BeginReplacement(xuid, CharacterAssignments::Character::None, now);
			return;
		}

		const int maxClients = (*Game::sv_maxclients)->current.integer;
		int selectedClientNum = -1;

		for (int clientNum = 0; clientNum < maxClients; ++clientNum)
		{
			if (CharacterAssignments::IsBotReserved(clientNum) && Game::svs_clients[clientNum].header.state < Game::CS_ACTIVE)
			{
				selectedClientNum = clientNum;
				break;
			}
		}

		if (selectedClientNum == -1)
		{
			for (int clientNum = 0; clientNum < maxClients; ++clientNum)
			{
				if (Game::svs_clients[clientNum].bIsTestClient && CharacterAssignments::IsBotReserved(clientNum))
				{
					selectedClientNum = clientNum;
					break;
				}
			}
		}

		if (selectedClientNum == -1)
		{
			return;
		}

		const auto character = CharacterAssignments::GetClientCharacterId(selectedClientNum);

		if (!CharacterAssignments::BeginReplacement(xuid, character, now))
		{
			return;
		}

		auto* const cl = &Game::svs_clients[selectedClientNum];
		const int selectedState = cl->header.state;

		CharacterAssignments::ClearClientSlot(selectedClientNum);
		botDisplayNames[selectedClientNum].clear();
		botIcons[selectedClientNum].clear();
		ZeroMemory(&botAi[selectedClientNum], sizeof(botAi[selectedClientNum]));
		botAi[selectedClientNum].weapon = 1;

		if (selectedState != Game::CS_FREE)
		{
			Game::SV_DropClient(cl, "EXE_DISCONNECTED", false);
			cl->header.state = Game::CS_FREE;
		}
	}

	void Bots::CleanBotArray()
	{
		ZeroMemory(&botAi, sizeof(botAi));

		for (std::size_t i = 0; i < std::extent_v<decltype(botAi)>; ++i)
		{
			botAi[i].weapon = 1;
		}
	}

	void Bots::AddServerCommands()
	{
		Command::AddSV("spawnBot", [](const Command::Params* params)
		{
			if (!IsServerRunning())
			{
				Logger::Print("Server is not running.\n");
				return;
			}

			if (IsFull())
			{
				Logger::Warning("Server is full.\n");
				return;
			}

			std::size_t count = 1;

			if (params->Size() > 1)
			{
				if (params->Get(1) == "all"s)
				{
					count = Game::MAX_CLIENTS;
				}
				else
				{
					char* end;
					const auto* input = params->Get(1);
					count = std::strtoul(input, &end, 10);

					if (input == end)
					{
						Logger::Warning("{} is not a valid input\nUsage: {} optional <number of bots> or optional <\"all\">\n", input, params->Get(0));
						return;
					}
				}
			}

			count = std::clamp<std::size_t>(count, 1, Game::MAX_CLIENTS);

			const char* noun = "bots";

			if (count == 1)
			{
				noun = "bot";
			}

			Logger::Print("Spawning {} {}\n", count, noun);

			Spawn(static_cast<unsigned int>(count));
		});
	}

	bool Bots::Player_UpdateActivate_stub(int)
	{
		return false;
	}

	Bots::Bots()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
		};

		const HookSite sites[] =
		{
			{ Player_UpdateActivate_IsClientBotCall, SV_IsClientBot, reinterpret_cast<void*>(Player_UpdateActivate_stub) },
			{ G_SelectWeaponIndexCalls[0], Components::G_SelectWeaponIndex, reinterpret_cast<void*>(G_SelectWeaponIndex_Hk) },
			{ G_SelectWeaponIndexCalls[1], Components::G_SelectWeaponIndex, reinterpret_cast<void*>(G_SelectWeaponIndex_Hk) },
			{ G_SelectWeaponIndexCalls[2], Components::G_SelectWeaponIndex, reinterpret_cast<void*>(G_SelectWeaponIndex_Hk) },
			{ SV_GetClientPingCalls[0], SV_GetClientPing, reinterpret_cast<void*>(SV_GetClientPing_Hk) },
			{ SV_GetClientPingCalls[1], SV_GetClientPing, reinterpret_cast<void*>(SV_GetClientPing_Hk) },
			{ SV_SendMessageToClient_TransmitCall, SV_Netchan_Transmit, reinterpret_cast<void*>(SV_Netchan_Transmit_Hk) },
			{ SV_Netchan_TransmitNextFragmentCalls[0], SV_Netchan_TransmitNextFragment, reinterpret_cast<void*>(SV_Netchan_TransmitNextFragment_Hk) },
			{ SV_Netchan_TransmitNextFragmentCalls[1], SV_Netchan_TransmitNextFragment, reinterpret_cast<void*>(SV_SendClientMessages_TransmitNextFragment_Hk) },
		};

		static_assert(std::size(sites) + 2 == std::size(hooks));

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, HOOK_CALL))
			{
				Logger::Error("bots: 0x{:X} no longer calls 0x{:X}, bots stay the engine's\n", hookSite.site, hookSite.target);
				return;
			}
		}

		const bool isAddTestClientIntact = Utils::Hook::IsLeaIntact(connectStringLea)
			&& Utils::Hook::MatchesBytes(SV_AddTestClient_SprintfCall, sprintfCall, sizeof(sprintfCall));

		const bool isBotLoopIntact = Utils::Hook::MatchesBytes(SV_BotUserMove_Begin, botUserMoveBegin, sizeof(botUserMoveBegin))
			&& Utils::Hook::MatchesBytes(SV_BotUserMove_Next, botUserMoveNext, sizeof(botUserMoveNext));

		if (!isAddTestClientIntact || !isBotLoopIntact)
		{
			Logger::Error("bots: SV_AddTestClient or the bot loop does not read as expected, bots stay the engine's\n");
			return;
		}

		const auto* connectString = Utils::Hook::PlaceNearImage("connect bot%d \"\\cg_predictItems\\1\\cl_anonymous\\0\\color\\4\\head\\default\\model\\multi\\snaps\\20\\rate\\5000\\name\\%s\\zw3char\\%s\\clanAbbrev\\%s\\protocol\\%d\\checksum\\%d\\statver\\%d %u\\qport\\%d\"");

		if (!connectString || !Utils::Hook::CanLeaReach(connectStringLea, connectString))
		{
			Logger::Error("bots: no room beside the image for the connect string, bots stay the engine's\n");
			return;
		}

		Bots_BotUserMoveNext = Utils::Hook::Rebase(SV_BotUserMove_Next);

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[std::size(sites)].Initialize(SV_AddTestClient_SprintfCall, reinterpret_cast<void*>(BuildConnectString), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[std::size(sites) + 1].Initialize(SV_BotUserMove_Begin, SV_BotUserMove_Stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("bots: could not seat every hook, bots stay the engine's\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		Utils::Hook::PointLeaAt(connectStringLea, connectString);

		Events::OnDvarInit([]
		{
			sv_randomBotNames = Game::Dvar_RegisterBool("sv_randomBotNames", false, Game::DVAR_NONE, "Randomize the bots' names");
			sv_replaceBots = Game::Dvar_RegisterBool("sv_replaceBots", false, Game::DVAR_NONE, "Test clients will be replaced by connecting players when the server is full.");
		});

		Scheduler::OnGameInitialized(UpdateBotNames, Scheduler::Pipeline::MAIN);

		Network::OnPacket("getbotsResponse", [](Network::Address& address, const std::string& data)
		{
			const auto masterPort = (*Game::com_masterPort)->current.integer;
			const auto* masterServerName = (*Game::com_masterServerName)->current.string;

			Network::Address master(Utils::String::VA("%s:%u", masterServerName, masterPort));

			if (master == address)
			{
				auto botNames = Utils::String::Split(data, '\n');
				Logger::Print("Got {} names from the master server\n", botNames.size());

				for (const auto& entry : botNames)
				{
					remoteBotNames.emplace_back(entry, "BOT");
				}
			}
		});

		Events::OnClientConnect([](Game::client_s* client)
		{
			if (!client)
			{
				return;
			}

			const int clientNum = static_cast<int>(client - Game::svs_clients);
			const Utils::InfoString info(client->userinfo);

			const auto encodedCharacter = CharacterAssignments::Parse(info.Get("zw3char"));
			const auto qportText = info.Get("qport");
			int qport = -1;

			if (!qportText.empty())
			{
				qport = static_cast<int>(std::strtol(qportText.data(), nullptr, 10));
			}

			ResetDownState(clientNum);

			const bool isSmartBot = client->bIsTestClient || CharacterAssignments::IsValid(encodedCharacter);

			if (!isSmartBot)
			{
				CharacterAssignments::ResolveClientCharacter(clientNum);
				return;
			}

			auto character = CharacterAssignments::GetClientCharacterId(clientNum);

			if (!CharacterAssignments::IsValid(character))
			{
				character = encodedCharacter;
			}

			if (!CharacterAssignments::IsValid(character))
			{
				character = GetPendingBotCharacter(qport);
			}

			if (CharacterAssignments::IsValid(character) && CharacterAssignments::IsCharacterUsed(character, clientNum))
			{
				character = CharacterAssignments::Character::None;
			}

			if (!CharacterAssignments::IsValid(character))
			{
				for (const auto candidate : CharacterAssignments::characters)
				{
					if (!CharacterAssignments::IsCharacterUsed(candidate, clientNum) && !IsPendingBotCharacter(candidate, qport))
					{
						character = candidate;
						break;
					}
				}
			}

			if (CharacterAssignments::IsValid(character))
			{
				CharacterAssignments::SetClientCharacter(clientNum, character);
				CharacterAssignments::SetBotReserved(clientNum, true);

				botDisplayNames[clientNum] = BotDisplayName(character);
				botIcons[clientNum] = CharacterAssignments::ToString(character);
				SynchronizeBotIdentity(clientNum, false);
			}

			ClearPendingBotCharacter(qport);
		});

		Scheduler::Loop([]
		{
			for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
			{
				SynchronizeBotIdentity(clientNum, true);
			}
		}, Scheduler::Pipeline::SERVER, 250ms);

		Events::OnClientDisconnect([](const int clientNum) -> void
		{
			if (!CharacterAssignments::IsClientNum(clientNum))
			{
				return;
			}

			const auto& client = Game::svs_clients[clientNum];
			const bool wasBot = client.bIsTestClient != 0;
			const auto xuid = CharacterAssignments::GetClientXuid(client);

			if (wasBot && !botDisplayNames[clientNum].empty())
			{
				for (int slot = 1; slot <= CharacterAssignments::maxPartySize; ++slot)
				{
					if (Dvar::Var(std::format("character_{}_player", slot)).Get<std::string>() == botDisplayNames[clientNum])
					{
						Dvar::Var(std::format("character_{}", slot)).Set("None");
						Dvar::Var(std::format("character_{}_player", slot)).Set("None");
					}
				}
			}

			botAi[clientNum].active = false;
			botDisplayNames[clientNum].clear();
			botIcons[clientNum].clear();
			CharacterAssignments::ClearClientSlot(clientNum);

			if (!wasBot && xuid != 0 && !CharacterAssignments::IsXuidConnected(xuid, clientNum))
			{
				CharacterAssignments::ForgetRealCharacter(xuid);
			}

			ResetDownState(clientNum);
		});

		Events::OnSVInit([]
		{
			ResetBotScoreboardData();
			AddServerCommands();
		});

		CleanBotArray();

		AddScriptMethods();

		Events::OnVMShutdown(CleanBotArray);
	}
}
