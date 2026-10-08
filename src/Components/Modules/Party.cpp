#include "STDInclude.hpp"

#include <charconv>
#include <sys/stat.h>

#include "Party.hpp"
#include "Auth.hpp"
#include "CharacterAssignments.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Download.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Gamepad.hpp"
#include "LobbyScene.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Maps.hpp"
#include "ModList.hpp"
#include "Network.hpp"
#include "Node.hpp"
#include "Scheduler.hpp"
#include "ServerInfo.hpp"
#include "TextRenderer.hpp"
#include "UIScript.hpp"
#include "Voice.hpp"
#include "ZWNet.hpp"

#include "Steam/Interfaces/SteamUser.hpp"

namespace Components
{
	Party::JoinContainer Party::joinContainer;
	std::map<std::uint64_t, Network::Address> Party::lobbyMap;
	Dvar::Var Party::party_enable;

	enum class MatchType : int
	{
		NoMatch = 0,
		PartyLobby = 1,
		DedicatedMatch = 2,
		PrivateParty = 3,
	};

	Network::Address Party::Target()
	{
		return joinContainer.target;
	}

	const char* Party::GetLobbyInfo(::Steam::SteamID lobby, const std::string& key)
	{
		const auto entry = lobbyMap.find(lobby.bits);

		if (entry != lobbyMap.end())
		{
			if (key == "addr")
			{
				return Utils::String::VA("%d", static_cast<int>(entry->second.GetIP()));
			}

			if (key == "port")
			{
				return Utils::String::VA("%d", entry->second.GetPort());
			}
		}

		return "212";
	}

	void Party::RemoveLobby(::Steam::SteamID lobby)
	{
		lobbyMap.erase(lobby.bits);
	}

	constexpr std::size_t partyAreWeHost = 0x1808;
	constexpr std::size_t partyInParty = 0x1814;

	static bool IsHostOf(const Game::PartyData* party)
	{
		const auto* const bytes = reinterpret_cast<const std::uint8_t*>(party);

		return *reinterpret_cast<const int*>(bytes + partyAreWeHost) != 0 && *reinterpret_cast<const int*>(bytes + partyInParty) != 0;
	}

	bool Party::IsHostingParty()
	{
		if (IsHostOf(Game::g_lobbyData))
		{
			return true;
		}

		return IsHostOf(Game::g_partyData);
	}

	static bool isPrivatePartyJoin = false;

	bool Party::IsPrivateMatchClient()
	{
		if (!isPrivatePartyJoin || IsHostingParty())
		{
			return false;
		}

		const auto* const lobby = reinterpret_cast<const std::uint8_t*>(Game::g_lobbyData);
		const bool isInParty = *reinterpret_cast<const int*>(lobby + partyInParty) != 0;

		return isInParty && Dvar::Var("xblive_privateserver").Get<bool>();
	}

	bool Party::IsEnabled()
	{
		return party_enable.IsValid() && party_enable.Get<bool>();
	}

	bool Party::IsInLobby()
	{
		return !Dedicated::IsRunning() && IsEnabled() && IsHostingParty();
	}

	bool Party::IsInUserMapLobby()
	{
		return IsInLobby() && Maps::IsUserMap(Dvar::Var("ui_mapname").Get<std::string>());
	}

	static const char* InfoFlag(bool isSet)
	{
		if (isSet)
		{
			return "1";
		}

		return "0";
	}

	constexpr int maxPartySlots = CharacterAssignments::maxPartySize;

	static std::map<std::uint64_t, std::vector<Network::Address>> xuidAddresses;
	static std::string hostCharacter;
	static bool isRosterFrozen = false;
	static bool isDirectLaunchRoster = false;
	static bool isAutosaveLaunchPending = false;
	static bool arePreferencesReady = false;
	static bool isMapPreferenceReady = false;
	static bool isZombieModeRestorePending = false;
	static int zombieModeStartValue = 0;
	static Dvar::Var mapPreference;

	static Dvar::Var party_host;

	static const char* zombieModeNames[] = { "Normal", "Classic", "Hardcore", nullptr };
	static const char* partyPrivacyNames[] = { "Open", "Invite-Only", "Closed", nullptr };

	constexpr int zombieModeCount = 3;

	struct HostSetting
	{
		const char* name;
		const char* preferenceName;
		int defaultValue;
		int max;
		const char* description;
		const char* saveScript;
		Dvar::Var preference;
	};

	static HostSetting hostSettings[] =
	{
		{ "weather", "zw3_pref_weather", 1, 2, "Saved weather preference", "SaveWeatherSetting" },
		{ "dayNightCycle", "zw3_pref_dayNightCycle", 1, 1, "Saved day and night preference", "SaveDayNightCycleSetting" },
		{ "bg_omnimovement", "zw3_pref_bg_omnimovement", 1, 1, "Saved omnimovement preference", "SaveOmnimovementSetting" },
		{ "ui_hitmarker", "zw3_pref_ui_hitmarker", 1, 1, "Saved hitmarker preference", "SaveHitmarkerSetting" },
		{ "ui_zombiecounter", "zw3_pref_ui_zombiecounter", 0, 1, "Saved zombie counter preference", "SaveZombieCounterSetting" },
		{ "ui_showdamage", "zw3_pref_ui_showdamage", 1, 1, "Saved damage visibility preference", "SaveShowDamageSetting" },
		{ "ui_perklocations", "zw3_pref_ui_perklocations", 0, 1, "Saved perk location preference", "SavePerkLocationsSetting" },
		{ "thirdPerson", "zw3_pref_thirdPerson", 0, 1, "Saved third-person preference", "SaveThirdPersonSetting" },
		{ "zombiemode", "zw3_pref_zombiemode", 0, 2, "Saved zombie mode preference", nullptr },
		{ "addBots", "zw3_pref_addBots", 0, 3, "Saved bot count preference", nullptr },
		{ "partyPrivacy", "zw3_pref_partyPrivacy", 0, 2, "Saved party privacy preference", "SavePartyPrivacySetting" },
	};

	static int GetZombieModeIndex()
	{
		const auto* const zombieMode = Game::Dvar_FindVar("zombiemode");

		if (!zombieMode || zombieMode->type != Game::DVAR_TYPE_ENUM)
		{
			return 0;
		}

		return std::clamp(zombieMode->current.integer, 0, zombieModeCount - 1);
	}

	static bool SetZombieModeIndex(int index)
	{
		const auto* const zombieMode = Game::Dvar_FindVar("zombiemode");

		if (!zombieMode || zombieMode->type != Game::DVAR_TYPE_ENUM)
		{
			return false;
		}

		const int mode = std::clamp(index, 0, zombieModeCount - 1);
		Game::Dvar_SetFromStringByName("zombiemode", zombieModeNames[mode]);

		return zombieMode->current.integer == mode;
	}

	static void ApplyPendingZombieMode()
	{
		if (isZombieModeRestorePending)
		{
			SetZombieModeIndex(zombieModeStartValue);
		}
	}

	static int GetHostSetting(const HostSetting& setting)
	{
		if (std::strcmp(setting.name, "zombiemode") == 0)
		{
			return GetZombieModeIndex();
		}

		return Dvar::Var(setting.name).Get<int>();
	}

	static bool SetHostSetting(const HostSetting& setting, int value)
	{
		if (std::strcmp(setting.name, "zombiemode") == 0)
		{
			return SetZombieModeIndex(value);
		}

		const Dvar::Var dvar(setting.name);
		dvar.Set(value);

		return dvar.Get<int>() == value;
	}

	static bool CanUseLocalPreferences()
	{
		return Party::IsHostingParty() || !Dvar::Var("xblive_privateserver").Get<bool>();
	}

	static void SavePreference(HostSetting& setting)
	{
		if (Dedicated::IsEnabled() || !arePreferencesReady)
		{
			return;
		}

		setting.preference.Set(GetHostSetting(setting));
	}

	static void ApplyCustomizationSettings()
	{
		if (Dedicated::IsEnabled() || !arePreferencesReady || !CanUseLocalPreferences())
		{
			return;
		}

		for (const auto& setting : hostSettings)
		{
			SetHostSetting(setting, setting.preference.Get<int>());
		}

		const Dvar::Var cg_thirdPerson("cg_thirdPerson");

		if (cg_thirdPerson.IsValid())
		{
			cg_thirdPerson.Set(Dvar::Var("thirdPerson").Get<int>() != 0);
		}
	}

	static void SaveCustomizationSettings()
	{
		if (Dedicated::IsEnabled() || !arePreferencesReady || !CanUseLocalPreferences())
		{
			return;
		}

		for (auto& setting : hostSettings)
		{
			SavePreference(setting);
		}
	}

	static void SaveZombieModePreference()
	{
		if (Dedicated::IsEnabled() || !arePreferencesReady)
		{
			return;
		}

		Dvar::Var("zw3_pref_zombiemode").Set(GetZombieModeIndex());
	}

	static bool IsUsableMapName(const std::string& mapName)
	{
		return !mapName.empty() && mapName != "none" && mapName != "None";
	}

	static std::string GetSelectedMapName(bool preferRunningMap)
	{
		const auto runningMap = Dvar::Var("mapname").Get<std::string>();

		if (preferRunningMap && IsUsableMapName(runningMap))
		{
			return runningMap;
		}

		const auto selectedMap = Dvar::Var("ui_mapname").Get<std::string>();

		if (IsUsableMapName(selectedMap))
		{
			return selectedMap;
		}

		if (IsUsableMapName(runningMap))
		{
			return runningMap;
		}

		return {};
	}

	static void SaveSelectedMapPreference(bool preferRunningMap)
	{
		if (Dedicated::IsEnabled() || !isMapPreferenceReady)
		{
			return;
		}

		const auto selectedMap = GetSelectedMapName(preferRunningMap);

		if (IsUsableMapName(selectedMap))
		{
			mapPreference.Set(selectedMap);
		}
	}

	static void ApplySelectedMapPreference()
	{
		if (Dedicated::IsEnabled() || !isMapPreferenceReady)
		{
			return;
		}

		const auto selectedMap = mapPreference.Get<std::string>();

		if (IsUsableMapName(selectedMap))
		{
			Dvar::Var("ui_mapname").Set(selectedMap);
		}
	}

	static void RecreatePrivateMatchLobby()
	{
		Command::Execute("xrequirelivesignin", false);
		Command::Execute("set systemlink 0", false);
		Command::Execute("set splitscreen 0", false);
		Command::Execute("set onlinegame 1", false);
		Command::Execute("exec default_xboxlive.cfg", false);
		Command::Execute("set party_maxplayers 4", false);
		Command::Execute("set party_maxprivatepartyplayers 4", false);
		Command::Execute("set xblive_privateserver 0", false);
		Command::Execute("set xblive_rankedmatch 0", false);
		Command::Execute("xstartprivateparty", false);
		Command::Execute("set ui_mptype 0", false);
		Command::Execute("xcheckezpatch", false);
		Command::Execute("exec default_xboxlive.cfg", false);
		Command::Execute("set xblive_rankedmatch 0", false);
		Command::Execute("ui_enumeratesaved", false);
		Command::Execute("set xblive_privateserver 1", false);
		Command::Execute("xstartprivatematch", false);
		Command::Execute("openmenu menu_xboxlive_privatelobby", false);
	}

	static bool SetStringIfChanged(const char* name, const std::string& value)
	{
		const Dvar::Var dvar(name);

		if (!dvar.IsValid() || dvar.Get<std::string>() == value)
		{
			return false;
		}

		dvar.Set(value);
		return true;
	}

	static bool ApplyCharacterRosterSnapshot(const Utils::InfoString& info)
	{
		std::array<std::string, maxPartySlots> characters{};
		std::array<std::string, maxPartySlots> owners{};

		for (int slot = 0; slot < maxPartySlots; ++slot)
		{
			characters[slot] = info.Get(Utils::String::VA("character_%d", slot + 1));
			owners[slot] = info.Get(Utils::String::VA("character_%d_player", slot + 1));

			if (characters[slot].empty() || owners[slot].empty())
			{
				return false;
			}

			const bool isCharacterEmpty = characters[slot] == "None";
			const bool isOwnerEmpty = owners[slot] == "None";

			if (isCharacterEmpty != isOwnerEmpty)
			{
				return false;
			}
		}

		if (characters[0] == "None" || owners[0] == "None")
		{
			return false;
		}

		for (int slot = 0; slot < maxPartySlots; ++slot)
		{
			SetStringIfChanged(Utils::String::VA("character_%d", slot + 1), characters[slot]);
			SetStringIfChanged(Utils::String::VA("character_%d_player", slot + 1), owners[slot]);
		}

		Dvar::Var("party_roster_loading").Set(false);
		return true;
	}

	static bool TryParseXuid(const std::string& text, std::uint64_t& xuid)
	{
		if (text.empty() || text.size() > 16)
		{
			return false;
		}

		std::uint64_t parsed = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed, 16);

		if (error != std::errc{} || end != text.data() + text.size())
		{
			return false;
		}

		xuid = parsed;
		return true;
	}

	static bool TryParseClientCount(const std::string& text, unsigned int& count)
	{
		if (text.empty())
		{
			return false;
		}

		unsigned int parsed = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed, 10);

		if (error != std::errc{} || end != text.data() + text.size())
		{
			return false;
		}

		count = parsed;
		return true;
	}

	static void TrackClientAddress(std::uint64_t xuid, const Network::Address& address)
	{
		if (xuid == 0)
		{
			return;
		}

		auto& addresses = xuidAddresses[xuid];
		const auto addressText = address.GetString();

		for (auto& known : addresses)
		{
			if (known.GetString() == addressText)
			{
				known = address;
				return;
			}
		}

		addresses.push_back(address);
	}

	static void BroadcastDvarUpdate()
	{
		if (!Party::IsHostingParty())
		{
			return;
		}

		int maxPartyMembers = Dvar::Var("party_maxplayers").Get<int>();

		if (maxPartyMembers <= 0)
		{
			maxPartyMembers = maxPartySlots;
		}

		if (Game::PartyClient_CountMembersEvenIfInactive(Game::g_lobbyData) <= 0)
		{
			return;
		}

		Utils::InfoString info;

		for (const auto& setting : hostSettings)
		{
			info.Set(setting.name, std::to_string(GetHostSetting(setting)));
		}

		info.Set("party_currentPlayers", std::to_string(Dvar::Var("party_currentPlayers").Get<int>()));
		info.Set("party_realPlayers", std::to_string(Dvar::Var("party_realPlayers").Get<int>()));
		info.Set("party_currentHost", Dvar::Var("party_currentHost").Get<std::string>());

		for (int slot = 1; slot <= maxPartySlots; ++slot)
		{
			info.Set(Utils::String::VA("character_%d", slot), Dvar::Var(Utils::String::VA("character_%d", slot)).Get<std::string>());
			info.Set(Utils::String::VA("character_%d_player", slot), Dvar::Var(Utils::String::VA("character_%d_player", slot)).Get<std::string>());
		}

		const auto update = info.Build();
		std::unordered_set<std::string> sentAddresses;

		for (int i = 0; i < std::min(maxPartyMembers, maxPartySlots); ++i)
		{
			const auto& member = Game::g_lobbyData->partyMembers[i];

			if (member.status == 0)
			{
				continue;
			}

			const auto addresses = xuidAddresses.find(member.player);

			if (addresses == xuidAddresses.end())
			{
				continue;
			}

			for (const auto& target : addresses->second)
			{
				if (!sentAddresses.insert(target.GetString()).second)
				{
					continue;
				}

				Network::SendCommand(target, "dvarUpdate", update);
			}
		}
	}

	struct RealCharacterParticipant
	{
		std::uint64_t xuid;
		std::string name;
		bool isHost;
	};

	static std::string GetMemberName(const Game::PartyMember& member)
	{
		return std::string(member.gamertag, strnlen(member.gamertag, sizeof(member.gamertag)));
	}

	static bool CollectLobbyParticipants(std::vector<RealCharacterParticipant>& participants)
	{
		participants.clear();

		if (!Party::IsHostingParty())
		{
			return false;
		}

		const auto hostXuid = Party::GetLocalPlayerXuid();

		if (hostXuid == 0)
		{
			return false;
		}

		participants.push_back({ hostXuid, Dvar::Var("name").Get<std::string>(), true });

		std::unordered_set<std::uint64_t> seen{ hostXuid };

		for (int slot = 0; slot < maxPartySlots && static_cast<int>(participants.size()) < maxPartySlots; ++slot)
		{
			const auto& member = Game::g_lobbyData->partyMembers[slot];

			if (member.status == 0 || !member.gamertag[0] || member.player == hostXuid)
			{
				continue;
			}

			if (member.player == 0)
			{
				return false;
			}

			if (!seen.insert(member.player).second)
			{
				continue;
			}

			participants.push_back({ member.player, GetMemberName(member), false });
		}

		return true;
	}

	static std::string CanonicalCharacterName(const std::string& name)
	{
		const auto character = CharacterAssignments::Parse(name);

		if (!CharacterAssignments::IsValid(character))
		{
			return {};
		}

		return CharacterAssignments::ToString(character);
	}

	static std::string ChooseInitialHostCharacter()
	{
		const auto kept = CanonicalCharacterName(hostCharacter);

		if (!kept.empty())
		{
			return kept;
		}

		const auto index = static_cast<std::size_t>(Game::Sys_Milliseconds()) % CharacterAssignments::characters.size();
		hostCharacter = CharacterAssignments::ToString(CharacterAssignments::characters[index]);

		return hostCharacter;
	}

	static std::string BuildRosterSignature(const std::vector<RealCharacterParticipant>& participants, int botsToAdd)
	{
		std::string signature = std::to_string(botsToAdd);

		for (const auto& participant : participants)
		{
			signature.append("|");
			signature.append(Utils::String::VA("%llX", participant.xuid));
			signature.append(":");
			signature.append(CharacterAssignments::Normalize(participant.name));
		}

		return signature;
	}

	static void PublishRosterSlot(int slot, const std::string& character, const std::string& player)
	{
		SetStringIfChanged(Utils::String::VA("character_%d", slot + 1), character);
		SetStringIfChanged(Utils::String::VA("character_%d_player", slot + 1), player);
	}

	static void RandomizeCharactersForClients()
	{
		if (!Party::IsHostingParty() || isRosterFrozen)
		{
			return;
		}

		std::vector<RealCharacterParticipant> participants;

		if (!CollectLobbyParticipants(participants) || participants.empty())
		{
			Dvar::Var("party_roster_loading").Set(true);
			return;
		}

		std::unordered_set<std::uint64_t> activeXuids;

		for (const auto& participant : participants)
		{
			activeXuids.insert(participant.xuid);
		}

		CharacterAssignments::PruneRealCharacters(activeXuids);

		std::vector<std::pair<std::string, std::string>> roster;
		std::unordered_set<int> usedCharacters;

		for (const auto& participant : participants)
		{
			auto preferred = CharacterAssignments::Character::None;

			if (participant.isHost)
			{
				preferred = CharacterAssignments::Parse(ChooseInitialHostCharacter());
			}

			const auto character = CharacterAssignments::EnsureRealCharacter(participant.xuid, preferred);

			if (!CharacterAssignments::IsValid(character))
			{
				continue;
			}

			if (participant.isHost)
			{
				hostCharacter = CharacterAssignments::ToString(character);
			}

			usedCharacters.insert(static_cast<int>(character));
			roster.emplace_back(CharacterAssignments::ToString(character), participant.name);
		}

		const int realPlayers = static_cast<int>(roster.size());
		const int botsToAdd = std::clamp(Dvar::Var("addBots").Get<int>(), 0, maxPartySlots - realPlayers);

		for (const auto character : CharacterAssignments::characters)
		{
			if (static_cast<int>(roster.size()) >= realPlayers + botsToAdd)
			{
				break;
			}

			if (usedCharacters.contains(static_cast<int>(character)))
			{
				continue;
			}

			const std::string characterName = CharacterAssignments::ToString(character);
			roster.emplace_back(characterName, Utils::String::VA("[BOT] %s", characterName.data()));
			usedCharacters.insert(static_cast<int>(character));
		}

		for (int slot = 0; slot < maxPartySlots; ++slot)
		{
			if (slot < static_cast<int>(roster.size()))
			{
				PublishRosterSlot(slot, roster[slot].first, roster[slot].second);
			}
			else
			{
				PublishRosterSlot(slot, "None", "None");
			}
		}

		const int partySize = std::clamp(realPlayers + botsToAdd, 1, maxPartySlots);
		CharacterAssignments::SetDesiredPartySize(partySize);
		Dvar::Var("party_realPlayers").Set(realPlayers);
		Dvar::Var("party_currentPlayers").Set(partySize);
		Dvar::Var("party_roster_loading").Set(false);
	}

	static bool HasCompletePublishedRoster()
	{
		const int partySize = std::clamp(Dvar::Var("party_currentPlayers").Get<int>(), 0, maxPartySlots);

		if (partySize < 1 || Dvar::Var("party_realPlayers").Get<int>() < 1)
		{
			return false;
		}

		for (int slot = 0; slot < partySize; ++slot)
		{
			const auto character = CanonicalCharacterName(Dvar::Var(Utils::String::VA("character_%d", slot + 1)).Get<std::string>());
			const auto playerName = Dvar::Var(Utils::String::VA("character_%d_player", slot + 1)).Get<std::string>();

			if (character.empty() || playerName.empty() || CharacterAssignments::Normalize(playerName) == "none")
			{
				return false;
			}
		}

		return true;
	}

	static int FindLocalRealClient()
	{
		const auto localXuid = Party::GetLocalPlayerXuid();

		if (localXuid != 0)
		{
			for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
			{
				const auto& client = Game::svs_clients[clientNum];

				if (client.header.state >= Game::CS_CONNECTED && !client.bIsTestClient && CharacterAssignments::GetClientXuid(client) == localXuid)
				{
					return clientNum;
				}
			}
		}

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];

			if (client.header.state >= Game::CS_CONNECTED && !client.bIsTestClient)
			{
				return clientNum;
			}
		}

		return -1;
	}

	static bool PublishDirectLaunchRoster()
	{
		const int clientNum = FindLocalRealClient();

		if (clientNum < 0)
		{
			return false;
		}

		auto character = CharacterAssignments::ResolveClientCharacter(clientNum);

		if (!CharacterAssignments::IsValid(character))
		{
			const auto preferred = CharacterAssignments::Parse(ChooseInitialHostCharacter());
			const auto xuid = CharacterAssignments::GetClientXuid(Game::svs_clients[clientNum]);

			if (xuid != 0)
			{
				character = CharacterAssignments::EnsureRealCharacter(xuid, preferred, clientNum);
			}
			else if (CharacterAssignments::IsValid(preferred))
			{
				character = preferred;
				CharacterAssignments::SetClientCharacter(clientNum, character);
			}
		}

		if (!CharacterAssignments::IsValid(character))
		{
			character = CharacterAssignments::Character::Richtofen;
			CharacterAssignments::SetClientCharacter(clientNum, character);
		}

		hostCharacter = CharacterAssignments::ToString(character);

		std::string hostName = Game::svs_clients[clientNum].name;

		if (hostName.empty())
		{
			hostName = Dvar::Var("name").Get<std::string>();
		}

		if (hostName.empty())
		{
			hostName = "Player";
		}

		PublishRosterSlot(0, CharacterAssignments::ToString(character), hostName);

		for (int slot = 1; slot < maxPartySlots; ++slot)
		{
			PublishRosterSlot(slot, "None", "None");
		}

		CharacterAssignments::SetDesiredPartySize(1);
		Dvar::Var("party_realPlayers").Set(1);
		Dvar::Var("party_currentPlayers").Set(1);
		Dvar::Var("party_currentHost").Set(hostName);
		Dvar::Var("party_roster_loading").Set(false);

		isDirectLaunchRoster = true;
		isRosterFrozen = true;
		return true;
	}

	static void FinalizeServerCharacterRoster(bool isAuthoritative)
	{
		if (Party::IsEnabled())
		{
			isDirectLaunchRoster = false;
			isRosterFrozen = true;

			if (HasCompletePublishedRoster())
			{
				CharacterAssignments::SetDesiredPartySize(Dvar::Var("party_currentPlayers").Get<int>());
				Dvar::Var("party_roster_loading").Set(false);
			}

			return;
		}

		if (!isAuthoritative && HasCompletePublishedRoster() && Dvar::Var("party_currentPlayers").Get<int>() > 1)
		{
			isRosterFrozen = true;
			return;
		}

		PublishDirectLaunchRoster();
	}

	struct RuntimeRosterEntry
	{
		int clientNum;
		bool isBot;
		bool isHost;
		std::uint64_t xuid;
		CharacterAssignments::Character character;
		std::string name;
	};

	static int CountRuntimeRealPlayers()
	{
		std::unordered_set<std::uint64_t> xuids;
		std::unordered_set<std::string> fallbackNames;

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];

			if (client.header.state < Game::CS_CONNECTED)
			{
				continue;
			}

			const auto xuid = CharacterAssignments::GetClientXuid(client);

			if (client.bIsTestClient && xuid == 0)
			{
				continue;
			}

			if (xuid != 0)
			{
				xuids.insert(xuid);
			}
			else if (client.name[0])
			{
				fallbackNames.insert(CharacterAssignments::Normalize(client.name));
			}
		}

		return std::clamp(static_cast<int>(xuids.size() + fallbackNames.size()), 0, maxPartySlots);
	}

	static int GetRosterGroup(const RuntimeRosterEntry& entry)
	{
		if (entry.isHost)
		{
			return 0;
		}

		if (entry.isBot)
		{
			return 2;
		}

		return 1;
	}

	static bool PublishRuntimeCharacterRoster()
	{
		static std::string candidateSignature;
		static int candidateTicks = 0;

		std::vector<RuntimeRosterEntry> entries;
		std::unordered_set<std::uint64_t> realXuids;
		const auto localXuid = Party::GetLocalPlayerXuid();
		const int realPlayers = CountRuntimeRealPlayers();
		bool hasChanged = false;

		if (Dvar::Var("party_realPlayers").Get<int>() != realPlayers)
		{
			Dvar::Var("party_realPlayers").Set(realPlayers);
			hasChanged = true;
		}

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];

			if (client.header.state < Game::CS_CONNECTED)
			{
				continue;
			}

			const auto xuid = CharacterAssignments::GetClientXuid(client);
			const bool isBot = client.bIsTestClient && xuid == 0;

			if (!isBot && xuid != 0 && !realXuids.insert(xuid).second)
			{
				continue;
			}

			auto character = CharacterAssignments::GetClientCharacterId(clientNum);

			if (!CharacterAssignments::IsValid(character))
			{
				character = CharacterAssignments::ResolveClientCharacter(clientNum);
			}

			if (!CharacterAssignments::IsValid(character))
			{
				continue;
			}

			RuntimeRosterEntry entry{ clientNum, isBot, !isBot && localXuid != 0 && xuid == localXuid, xuid, character, {} };

			if (isBot)
			{
				entry.name = Utils::String::VA("[BOT] %s", CharacterAssignments::ToString(character));
			}
			else
			{
				entry.name = client.name;

				if (entry.name.empty())
				{
					entry.name = "Player";
				}
			}

			entries.push_back(std::move(entry));
		}

		if (entries.empty())
		{
			return hasChanged;
		}

		const bool hasHost = std::ranges::any_of(entries, [](const RuntimeRosterEntry& entry)
		{
			return entry.isHost;
		});

		if (!hasHost)
		{
			const auto firstReal = std::ranges::find_if(entries, [](const RuntimeRosterEntry& entry)
			{
				return !entry.isBot;
			});

			if (firstReal != entries.end())
			{
				firstReal->isHost = true;
			}
		}

		std::ranges::stable_sort(entries, [](const RuntimeRosterEntry& a, const RuntimeRosterEntry& b)
		{
			const int groupA = GetRosterGroup(a);
			const int groupB = GetRosterGroup(b);

			if (groupA != groupB)
			{
				return groupA < groupB;
			}

			if (a.isBot && b.isBot && a.character != b.character)
			{
				return static_cast<int>(a.character) < static_cast<int>(b.character);
			}

			return a.clientNum < b.clientNum;
		});

		if (static_cast<int>(entries.size()) > maxPartySlots)
		{
			entries.resize(static_cast<std::size_t>(maxPartySlots));
		}

		std::string signature;

		for (const auto& entry : entries)
		{
			signature.append(Utils::String::VA("%d:%d:%llX:", entry.clientNum, static_cast<int>(entry.character), entry.xuid));
			signature.append(entry.name);
			signature.push_back('|');
		}

		if (signature != candidateSignature)
		{
			candidateSignature = signature;
			candidateTicks = 1;
			return hasChanged;
		}

		if (candidateTicks < 3)
		{
			++candidateTicks;
			return hasChanged;
		}

		for (int slot = 0; slot < maxPartySlots; ++slot)
		{
			std::string character = "None";
			std::string player = "None";

			if (slot < static_cast<int>(entries.size()))
			{
				character = CharacterAssignments::ToString(entries[slot].character);
				player = entries[slot].name;
			}

			hasChanged |= SetStringIfChanged(Utils::String::VA("character_%d", slot + 1), character);
			hasChanged |= SetStringIfChanged(Utils::String::VA("character_%d_player", slot + 1), player);
		}

		if (Dvar::Var("party_currentPlayers").Get<int>() != static_cast<int>(entries.size()))
		{
			Dvar::Var("party_currentPlayers").Set(static_cast<int>(entries.size()));
			hasChanged = true;
		}

		if (Dvar::Var("party_roster_loading").Get<bool>())
		{
			Dvar::Var("party_roster_loading").Set(false);
			hasChanged = true;
		}

		return hasChanged;
	}

	static void ApplyHostInfo(const Utils::InfoString& info)
	{
		for (const auto& setting : hostSettings)
		{
			const auto text = info.Get(setting.name);
			char* end = nullptr;
			const auto value = std::strtol(text.data(), &end, 10);

			if (end != text.data())
			{
				SetHostSetting(setting, static_cast<int>(value));
			}
		}

		const auto currentPlayers = info.Get("party_currentPlayers");
		const auto realPlayers = info.Get("party_realPlayers");
		const auto currentHost = info.Get("party_currentHost");

		if (!currentPlayers.empty())
		{
			Dvar::Var("party_currentPlayers").Set(static_cast<int>(std::strtol(currentPlayers.data(), nullptr, 10)));
		}

		if (!realPlayers.empty())
		{
			Dvar::Var("party_realPlayers").Set(static_cast<int>(std::strtol(realPlayers.data(), nullptr, 10)));
		}

		if (!currentHost.empty())
		{
			SetStringIfChanged("party_currentHost", currentHost);
		}

		ApplyCharacterRosterSnapshot(info);
	}

	static std::string GetAutosavePath()
	{
		return std::string((*Game::fs_basepath)->current.string) + "\\zw3\\core\\scriptdata\\autosave";
	}

	static std::string DecryptAutosave(const std::string& text)
	{
		static const std::string characterMap = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ";
		constexpr int shift = 16;

		const int mapSize = static_cast<int>(characterMap.size());
		std::string decrypted;
		decrypted.reserve(text.size());

		for (const char character : text)
		{
			const auto index = characterMap.find(character);

			if (index == std::string::npos)
			{
				decrypted.push_back(character);
				continue;
			}

			const int moved = (static_cast<int>(index) - shift + mapSize) % mapSize;
			decrypted.push_back(characterMap[moved]);
		}

		return decrypted;
	}

	static std::unordered_map<std::string, std::string> ParseAutosave(const std::string& text)
	{
		std::unordered_map<std::string, std::string> fields;
		std::size_t start = 0;

		while (true)
		{
			const auto end = text.find(';', start);

			if (end == std::string::npos)
			{
				break;
			}

			const auto item = text.substr(start, end - start);
			start = end + 1;

			const auto colon = item.find(':');

			if (colon == std::string::npos)
			{
				continue;
			}

			auto key = item.substr(0, colon);
			auto value = item.substr(colon + 1);

			Utils::String::Trim(key);
			Utils::String::Trim(value);

			if (!key.empty())
			{
				fields[key] = value;
			}
		}

		return fields;
	}

	static void SetAutosaveDvar(const char* name, const std::string& value)
	{
		auto* dvar = Game::Dvar_FindVar(name);

		if (!dvar)
		{
			dvar = Game::Dvar_RegisterString(name, "", Game::DVAR_INIT, "");
		}

		Game::Dvar_SetString(dvar, value.data());
	}

	static std::string FormatAutosaveTime(const std::string& text)
	{
		int hours = 0;
		int minutes = 0;
		int seconds = 0;

		if (std::sscanf(text.data(), "%d:%d:%d", &hours, &minutes, &seconds) != 3)
		{
			return "00:00:00";
		}

		return std::format("{:02}:{:02}:{:02}", hours, minutes, seconds);
	}

	static std::string GetUpgradeStatus(const std::string& weapon, const std::string& upgrade)
	{
		static const std::unordered_set<std::string> upgradeableWeapons =
		{
			"t7_raygun_mp", "t7_rgmk2_mp", "thundergun_mp", "apothicon_mp",
			"acidgat_mp", "blundergat_mp", "t5_spectre_mp", "t5_galil_mp",
			"t5_mp5k_mp", "t5_ak74u_mp", "t5_python_mp", "t5_1911_mp",
			"iw5_mp7_mp", "codo_ak104ss_mp", "codo_mg4ss_mp", "codo_mg4ss_alt_mp",
			"jetgun_mp", "t7_dg2_mp", "t5_rottweil72_mp", "t5_mp40_mp",
			"t4_bar_mp", "t5_commando_mp", "t4_thompson_mp", "bow_mp",
			"paralyzer_mp", "raymachine_mp",
		};

		const bool isUpgradedWeapon = weapon.find("_upgraded_mp") != std::string::npos || weapon.find("_upgrade_mp") != std::string::npos;
		const bool hasUpgrade = !upgrade.empty() && upgrade != "none" && upgrade != "0";

		if (isUpgradedWeapon || hasUpgrade)
		{
			return "^2Yes";
		}

		if (upgradeableWeapons.contains(weapon))
		{
			return "^3No";
		}

		return "^1Not Upgradable";
	}

	static std::string GetUpgradeEffect(const std::string& upgrade)
	{
		if (upgrade.empty() || upgrade == "none" || upgrade == "0")
		{
			return "None";
		}

		if (upgrade == "flame")
		{
			return "^1Flame";
		}

		if (upgrade == "turned")
		{
			return "^2Turned";
		}

		if (upgrade == "lightning")
		{
			return "^5Lightning";
		}

		return upgrade;
	}

	static std::string GetWeaponDisplayName(const std::string& weapon)
	{
		if (weapon.empty() || weapon == "none")
		{
			return "None";
		}

		static const std::unordered_map<std::string, std::string> weaponNames =
		{
			{ "t5_mp40_mp", "MP40" },
			{ "t5_mp40_upgraded_mp", "The Afterburner" },
			{ "t5_1911_mp", "M1911" },
			{ "t5_1911_upgraded_mp", "Mustang" },
			{ "t5_rottweil72_mp", "Olympia" },
			{ "t5_rottweil72_upgraded_mp", "Hades" },
			{ "t5_python_mp", "Python" },
			{ "t5_python_upgraded_mp", "Cobra" },
			{ "t5_galil_mp", "Galil" },
			{ "t5_galil_upgraded_mp", "Lamentation" },
			{ "t5_spectre_mp", "Spectre" },
			{ "t5_spectre_upgraded_mp", "Phantom" },
			{ "t5_mp5k_mp", "MP5K" },
			{ "t5_mp5k_upgraded_mp", "MP115 Kollider" },
			{ "t5_ak74u_mp", "AK74u" },
			{ "t5_ak74u_upgraded_mp", "AK74fu2" },
			{ "t5_commando_mp", "Commando" },
			{ "t5_commando_upgraded_mp", "Predator" },

			{ "t4_bar_mp", "BAR" },
			{ "t4_bar_upgraded_mp", "The Widow Maker" },
			{ "t4_thompson_mp", "Thompson" },
			{ "t4_thompson_upgraded_mp", "Speakeasy" },

			{ "iw5_mp7_mp", "MP7" },
			{ "iw5_mp7_upgraded_mp", "MP8" },

			{ "codo_ak104ss_mp", "CAR-T Lava" },
			{ "codo_ak104ss_upgraded_mp", "Car-Z Magma" },
			{ "codo_mg4ss_mp", "Gaia's Arm" },
			{ "codo_mg4ss_alt_mp", "Gaia's Arm Sentrymode" },
			{ "codo_mg4ss_upgraded_mp", "Kronos Arm" },
			{ "codo_mg4ss_alt_upgraded_mp", "Kronos Arm Sentrymode" },

			{ "t7_raygun_mp", "Ray Gun" },
			{ "t7_raygun_upgraded_mp", "Porter's X2 Ray Gun" },
			{ "t7_rgmk2_mp", "Ray Gun Mark 2" },
			{ "t7_rgmk2_upgraded_mp", "Porter's Mark II Ray Gun" },
			{ "t7_dg2_mp", "Wunderwaffe DG2" },
			{ "t7_dg2_upgraded_mp", "DG-3 JZ" },

			{ "thundergun_mp", "Thundergun" },
			{ "thundergun_upgrade_mp", "Zeus Cannon" },
			{ "thundergun_upgraded_mp", "Zeus Cannon" },

			{ "paralyzer_mp", "Paralyzer" },
			{ "paralyzer_upgraded_mp", "Petrifier" },

			{ "apothicon_mp", "Apothicon Servant" },
			{ "apothicon_upgraded_mp", "Estoom-oth" },

			{ "blundergat_mp", "Blundergat" },
			{ "blundergat_upgrade_mp", "The Sweeper" },
			{ "blundergat_upgraded_mp", "The Sweeper" },
			{ "acidgat_mp", "Acid Gat" },
			{ "acidgat_upgraded_mp", "Vitriolic Withering" },

			{ "jetgun_mp", "Thrustodyne Aeronautics Model 23" },
			{ "jetgun_upgraded_mp", "Thrustodyne M23" },

			{ "raymachine_mp", "Ray Machine" },
			{ "raymachine_upgraded_mp", "Porter's Ray Machine" },

			{ "bow_mp", "Bow" },
			{ "bow_upgraded_mp", "Upgraded Bow" },
			{ "skull_mp", "Skull of Nan Sapwe" },
			{ "skull_upgraded_mp", "Upgraded Skull of Nan Sapwe" },

			{ "c4_mp", "Monkey Bombs" },
			{ "throwingknife_mp", "Tazer" },
			{ "stabby_mp", "Knife" },
			{ "stabby_miss_mp", "Knife" },
			{ "deathhands_mp", "Death Hands" },
			{ "revive_mp", "Revive" },
			{ "paphands_mp", "Bare Hands" },
			{ "stinger_mp", "Stinger" },
		};

		static const std::unordered_map<std::string, std::string> baseWeaponNames =
		{
			{ "mp5k", "MP5K" }, { "uzi", "Mini-Uzi" }, { "p90", "P90" }, { "kriss", "Vector" }, { "ump45", "UMP45" },
			{ "striker", "Striker" }, { "aa12", "AA-12" }, { "m1014", "M1014" }, { "spas12", "SPAS-12" }, { "ranger", "Ranger" }, { "model1887", "Model 1887" },
			{ "ak47", "AK-47" }, { "m16", "M16A4" }, { "m4", "M4A1" }, { "fn2000", "F2000" }, { "masada", "ACR" }, { "famas", "FAMAS" }, { "fal", "FAL" }, { "scar", "SCAR-H" }, { "tavor", "TAR-21" }, { "peacekeeper", "Peacekeeper" },
			{ "aug", "AUG HBAR" }, { "rpd", "RPD" }, { "sa80", "L86 LSW" }, { "mg4", "MG4" }, { "m240", "M240" },
			{ "cheytac", "Intervention" }, { "m21", "M21 EBR" }, { "wa2000", "WA2000" }, { "barrett", "Barrett .50cal" }, { "dragunov", "Dragunov" }, { "m40a3", "M40A3" },
			{ "usp", "USP .45" }, { "deserteagle", "Desert Eagle" }, { "coltanaconda", "Colt Anaconda" }, { "beretta", "M9" },
			{ "rpg", "RPG-7" }, { "javelin", "Javelin" }, { "at4", "AT4" }, { "m79", "M79" },
			{ "glock", "G18" }, { "tmp", "TMP" }, { "beretta393", "M93 Raffica" }, { "pp2000", "PP2000" },
		};

		static const std::unordered_map<std::string, std::string> attachmentNames =
		{
			{ "acog", "ACOG" }, { "reflex", "Reflex" }, { "eotech", "Holographic" },
			{ "fmj", "FMJ" }, { "xmags", "Extended Mags" }, { "silencer", "Silencer" },
			{ "akimbo", "Akimbo" }, { "rof", "Rapid Fire" }, { "grip", "Grip" },
			{ "gl", "Grenade Launcher" }, { "shotgun", "Shotgun" }, { "heartbeat", "Heartbeat" },
			{ "thermal", "Thermal" }, { "tactical", "Tactical Knife" },
		};

		const auto named = weaponNames.find(weapon);

		if (named != weaponNames.end())
		{
			return named->second;
		}

		auto baseName = weapon;

		if (baseName.ends_with("_mp"))
		{
			baseName.resize(baseName.size() - 3);
		}

		std::vector<std::string> parts;

		for (const auto& part : Utils::String::Split(baseName, '_'))
		{
			if (!part.empty())
			{
				parts.push_back(part);
			}
		}

		if (parts.empty())
		{
			return weapon;
		}

		std::string displayName = parts[0];
		const auto base = baseWeaponNames.find(parts[0]);

		if (base != baseWeaponNames.end())
		{
			displayName = base->second;
		}

		std::string attachments;

		for (std::size_t i = 1; i < parts.size(); ++i)
		{
			const auto attachment = attachmentNames.find(parts[i]);

			if (attachment == attachmentNames.end())
			{
				continue;
			}

			if (!attachments.empty())
			{
				attachments += ", ";
			}

			attachments += attachment->second;
		}

		if (attachments.empty())
		{
			return displayName;
		}

		return displayName + " (" + attachments + ")";
	}

	static void ShowAutosave()
	{
		const auto path = GetAutosavePath();
		std::ifstream file(path);

		if (!file.is_open())
		{
			return;
		}

		struct _stat64 status{};

		if (_stat64(path.data(), &status) == 0)
		{
			tm modified{};
			localtime_s(&modified, &status.st_mtime);

			char formatted[128];
			std::strftime(formatted, sizeof(formatted), "%d %b %Y  %H:%M", &modified);
			SetAutosaveDvar("autosave_date", formatted);
		}
		else
		{
			SetAutosaveDvar("autosave_date", "Unknown date");
		}

		const std::string encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		const auto fields = ParseAutosave(DecryptAutosave(encrypted));

		if (!fields.contains("map"))
		{
			return;
		}

		const auto get = [&fields](const char* key, const char* fallback) -> std::string
		{
			const auto field = fields.find(key);

			if (field == fields.end() || field->second.empty())
			{
				return fallback;
			}

			return field->second;
		};

		auto zombieMode = get("zombiemode", "Normal");
		std::ranges::replace(zombieMode, '9', ' ');

		SetAutosaveDvar("autosave_round", get("round", "1"));
		SetAutosaveDvar("autosave_kills", get("kills", "0"));
		SetAutosaveDvar("autosave_score", get("score", "0"));
		SetAutosaveDvar("autosave_downs", get("downs", "0"));
		SetAutosaveDvar("autosave_revives", get("revives", "0"));
		SetAutosaveDvar("autosave_exfiltrated", get("exfiltrated", "0"));
		SetAutosaveDvar("autosave_zombiemode", zombieMode);
		SetAutosaveDvar("autosave_time", FormatAutosaveTime(get("time", "00:00:00")));

		const auto primary = get("weapon(0)", "None");
		const auto secondary = get("weapon(1)", "None");
		const auto primaryUpgrade = get("upgrade(0)", "none");
		const auto secondaryUpgrade = get("upgrade(1)", "none");

		SetAutosaveDvar("autosave_primary_weapon", GetWeaponDisplayName(primary));
		SetAutosaveDvar("autosave_primary_clip", get("clip(0)", "0"));
		SetAutosaveDvar("autosave_primary_stock", get("stock(0)", "0"));
		SetAutosaveDvar("autosave_primary_upgrade", GetUpgradeStatus(primary, primaryUpgrade));
		SetAutosaveDvar("autosave_primary_effect", GetUpgradeEffect(primaryUpgrade));

		SetAutosaveDvar("autosave_secondary_weapon", GetWeaponDisplayName(secondary));
		SetAutosaveDvar("autosave_secondary_clip", get("clip(1)", "0"));
		SetAutosaveDvar("autosave_secondary_stock", get("stock(1)", "0"));
		SetAutosaveDvar("autosave_secondary_upgrade", GetUpgradeStatus(secondary, secondaryUpgrade));
		SetAutosaveDvar("autosave_secondary_effect", GetUpgradeEffect(secondaryUpgrade));

		SetAutosaveDvar("autosave_has_quickrevive", get("perks('Quick Revive')", "0"));
		SetAutosaveDvar("autosave_has_juggernog", get("perks('Juggernog')", "0"));
		SetAutosaveDvar("autosave_has_speedcola", get("perks('Speed Cola')", "0"));
		SetAutosaveDvar("autosave_has_doubletap", get("perks('Double Tap')", "0"));
		SetAutosaveDvar("autosave_has_staminup", get("perks('Stamin Up')", "0"));
		SetAutosaveDvar("autosave_has_deadshot", get("perks('Deadshot Daiquiri')", "0"));
		SetAutosaveDvar("autosave_has_electriccherry", get("perks('Electric Cherry')", "0"));

		const auto map = get("map", "");
		const char* displayName = Game::UI_GetMapDisplayName(map.data());

		if (!displayName || !displayName[0] || map == displayName)
		{
			displayName = Localization::LocalizeMapName(map.data());
		}

		std::string mapDisplayName = map;

		if (displayName)
		{
			mapDisplayName = displayName;
		}

		SetAutosaveDvar("autosave_map", map);
		SetAutosaveDvar("autosave_mapname_display", mapDisplayName);

		Command::Execute("openmenu popup_autosave");
	}

	std::uint64_t Party::GetLocalPlayerXuid()
	{
		return ::Steam::User::LocalId().bits;
	}

	static std::string ReadGetInfoRequest(const Network::Address& address, const std::string& data)
	{
		std::string payload = data;
		Utils::String::Trim(payload);

		if (!payload.starts_with('\\'))
		{
			return ServerInfo::ParseChallenge(data);
		}

		const Utils::InfoString request(payload);
		std::uint64_t xuid = 0;

		if (!Dedicated::IsEnabled() && !Dedicated::IsRunning() && TryParseXuid(request.Get("xuid"), xuid))
		{
			TrackClientAddress(xuid, address);
		}

		return request.Get("challenge");
	}

	void Party::HandleGetInfo(Network::Address& address, const std::string& data)
	{
		const auto challenge = ReadGetInfoRequest(address, data);

		int botCount = 0;
		int effectiveClientCount = 0;
		int maxClientCount = *Game::svs_clientCount;
		const auto securityLevel = Dvar::Var("sv_securityLevel").Get<int>();
		const auto password = Dvar::Var("g_password").Get<std::string>();

		if (maxClientCount)
		{
			for (int i = 0; i < maxClientCount; ++i)
			{
				const auto& client = Game::svs_clients[i];

				if (client.header.state < Game::CS_ACTIVE || !client.gentity || !client.gentity->client)
				{
					continue;
				}

				const auto team = client.gentity->client->sess.cs.team;

				if (client.bIsTestClient || team == Game::TEAM_SPECTATOR)
				{
					++botCount;
				}
				else
				{
					++effectiveClientCount;
				}
			}
		}
		else
		{
			const Dvar::Var partyMaxPlayers("party_maxplayers");
			maxClientCount = 18;

			if (partyMaxPlayers.IsValid())
			{
				maxClientCount = partyMaxPlayers.Get<int>();
			}

			effectiveClientCount = Game::PartyClient_CountMembersEvenIfInactive(Game::g_lobbyData);

			const auto slotCount = std::min(static_cast<std::size_t>(maxClientCount), Game::MAX_CLIENTS);

			for (std::size_t i = 0; i < slotCount; ++i)
			{
				const auto& client = Game::svs_clients[i];

				if (client.header.state < Game::CS_ACTIVE || !client.gentity || !client.gentity->client)
				{
					continue;
				}

				const auto team = client.gentity->client->sess.cs.team;

				if (client.bIsTestClient || team == Game::TEAM_SPECTATOR)
				{
					++botCount;
					--effectiveClientCount;
				}
			}

			if (effectiveClientCount < 0)
			{
				effectiveClientCount = 0;
			}
		}

		Utils::InfoString info;
		info.Set("challenge", challenge);
		info.Set("gamename", "IW4");
		info.Set("hostname", Dvar::Var("sv_hostname").Get<std::string>());
		info.Set("gametype", Dvar::Var("g_gametype").Get<std::string>());
		info.Set("fs_game", (*Game::fs_gameDirVar)->current.string);
		info.Set("xuid", Utils::String::VA("%llX", Auth::GetKeyHash()));
		info.Set("clients", std::to_string(effectiveClientCount));
		info.Set("bots", std::to_string(botCount));
		info.Set("sv_maxclients", std::to_string(maxClientCount));
		info.Set("protocol", std::to_string(ServerInfo::GetProtocol()));
		info.Set("version", "2.0.13");
		info.Set("checksum", std::to_string(Game::Sys_Milliseconds()));
		info.Set("mapname", Dvar::Var("mapname").Get<std::string>());
		info.Set("isPrivate", InfoFlag(!password.empty()));
		info.Set("hc", InfoFlag(Dvar::Var("g_hardcore").Get<bool>()));
		info.Set("securityLevel", std::to_string(securityLevel));
		info.Set("sv_running", InfoFlag(Dedicated::IsRunning()));
		info.Set("aimAssist", InfoFlag(Gamepad::sv_allowAimAssist.Get<bool>()));
		info.Set("voiceChat", InfoFlag(Voice::SV_VoiceEnabled()));

		for (const auto& setting : hostSettings)
		{
			info.Set(setting.name, std::to_string(GetHostSetting(setting)));
		}

		auto currentHost = Dvar::Var("party_currentHost").Get<std::string>();

		if (currentHost.empty())
		{
			currentHost = Dvar::Var("name").Get<std::string>();
		}

		info.Set("party_currentHost", currentHost);
		info.Set("party_currentPlayers", std::to_string(Dvar::Var("party_currentPlayers").Get<int>()));
		info.Set("party_realPlayers", std::to_string(Dvar::Var("party_realPlayers").Get<int>()));

		for (int slot = 1; slot <= maxPartySlots; ++slot)
		{
			info.Set(Utils::String::VA("character_%d", slot), Dvar::Var(Utils::String::VA("character_%d", slot)).Get<std::string>());
			info.Set(Utils::String::VA("character_%d_player", slot), Dvar::Var(Utils::String::VA("character_%d_player", slot)).Get<std::string>());
		}

		if (info.Get("mapname").empty() || IsInLobby())
		{
			info.Set("mapname", Dvar::Var("ui_mapname").Get<std::string>());
		}

		if (Maps::GetUserMap()->IsValid())
		{
			info.Set("usermaphash", Utils::String::VA("%i", Maps::GetUserMap()->GetHash()));
		}
		else if (IsInUserMapLobby())
		{
			info.Set("usermaphash", Utils::String::VA("%i", Maps::GetUsermapHash(info.Get("mapname"))));
		}

		if (Dedicated::IsEnabled())
		{
			info.Set("sv_motd", Dedicated::sv_motd.Get<std::string>());
			info.Set("zwnet_show_in_server_browser", InfoFlag(Dedicated::zwnet_show_in_server_browser.Get<std::string>() != "0"));

			const auto matchId = Dvar::Var("zwnet_match_id").Get<std::string>();
			const bool isManagedSession = !matchId.empty() && matchId != "00000000-0000-0000-0000-000000000000";

			info.Set("zwnet_managed_session", InfoFlag(isManagedSession));
		}

		MatchType matchType = MatchType::NoMatch;

		if (IsHostingParty())
		{
			matchType = MatchType::PrivateParty;

			if (IsEnabled())
			{
				matchType = MatchType::PartyLobby;
			}
		}
		else if (Dvar::Var("sv_running").Get<bool>())
		{
			matchType = MatchType::DedicatedMatch;
		}

		info.Set("matchtype", std::to_string(static_cast<int>(matchType)));

		info.Set("wwwDownload", InfoFlag(Download::sv_wwwDownload.Get<bool>()));
		info.Set("wwwUrl", Download::sv_wwwBaseUrl.Get<std::string>());

		info.Set("x64", "1");

		Network::SendCommand(address, "infoResponse", info.Build());
	}

	std::string Party::GetHostName()
	{
		return joinContainer.info.Get("hostname");
	}

	std::string Party::GetMotd()
	{
		return joinContainer.motd;
	}

	int Party::GetMaxClients()
	{
		const auto value = joinContainer.info.Get("sv_maxclients");
		return std::strtol(value.data(), nullptr, 10);
	}

	void Party::Connect(const Network::Address& target, bool downloadOnly, bool isUnmanagedRequired)
	{
		LobbyScene::StopTransition();

		Node::Add(target);

		if (!target.IsValid())
		{
			return;
		}

		isPrivatePartyJoin = false;

		joinContainer.target = target;
		joinContainer.challenge = Utils::Cryptography::Rand::GenerateChallenge();
		joinContainer.startTime = Game::Sys_Milliseconds();
		joinContainer.isValid = true;
		joinContainer.isAwaitingPlaylist = false;
		joinContainer.isDownloadOnly = downloadOnly;
		joinContainer.isUnmanagedRequired = isUnmanagedRequired;

		Dvar::Var("party_roster_loading").Set(true);

		Utils::InfoString request;
		request.Set("challenge", joinContainer.challenge);
		request.Set("gamename", "IW4");
		request.Set("protocol", std::to_string(ServerInfo::GetProtocol()));
		request.Set("version", "2.0.13");
		request.Set("xuid", Utils::String::VA("%llX", GetLocalPlayerXuid()));

		Network::SendCommand(joinContainer.target, "getinfo", request.Build());

		Command::Execute("openmenu popup_reconnectingtoparty");
	}

	void Party::ConnectError(const std::string& message)
	{
		LobbyScene::StopTransition();

		joinContainer.isValid = false;

		Logger::Print("join failed: {}\n", message);

		Command::Execute("closemenu popup_reconnectingtoparty");

		Game::Dvar_SetFromStringByName("partyend_reason", message.data());

		Command::Execute("openmenu menu_xboxlive_partyended", false);
	}

	bool Party::HandleJoinResponse(const Network::Address& address, const Utils::InfoString& info)
	{
		if (!joinContainer.isValid || !(address == joinContainer.target))
		{
			return false;
		}

		joinContainer.isValid = false;
		joinContainer.info = info;

		if (joinContainer.isUnmanagedRequired && info.Get("zwnet_managed_session") == "1")
		{
			ConnectError("ZWNET matchmaking authorization is unavailable.");
			return true;
		}

		const auto matchType = static_cast<MatchType>(std::strtol(info.Get("matchtype").data(), nullptr, 10));

		if (!Dedicated::IsEnabled() && !Dedicated::IsRunning() && matchType == MatchType::PrivateParty)
		{
			const auto privacy = info.Get("partyPrivacy");
			unsigned int memberCount = 0;

			if (privacy == "2" || privacy == "Closed")
			{
				ConnectError("The lobby you are trying to join is closed.");
				return true;
			}

			if (!TryParseClientCount(info.Get("clients"), memberCount))
			{
				ConnectError("Invalid server info.");
				return true;
			}

			if (memberCount >= static_cast<unsigned int>(maxPartySlots))
			{
				ConnectError("The lobby you are trying to join is full.");
				return true;
			}

			joinContainer.info.Set("isPrivate", "0");
		}

		std::uint64_t hostXuid = 0;

		if (!TryParseXuid(info.Get("xuid"), hostXuid) || hostXuid == 0)
		{
			ConnectError("Invalid server info.");
			return true;
		}

		ApplyHostInfo(info);

		if (info.Get("party_currentHost").empty() && !info.Get("hostname").empty())
		{
			SetStringIfChanged("party_currentHost", info.Get("hostname"));
		}

		if (info.Get("wwwDownload") == "1")
		{
			Download::sv_wwwDownload.Set(true);
			Download::sv_wwwBaseUrl.Set(info.Get("wwwUrl"));
		}
		else
		{
			Download::sv_wwwDownload.Set(false);
			Download::sv_wwwBaseUrl.Set("");
		}

		std::string receivedChallenge;

		if (matchType == MatchType::DedicatedMatch || matchType == MatchType::PartyLobby)
		{
			receivedChallenge = joinContainer.challenge;
		}
		else if (matchType == MatchType::PrivateParty)
		{
			receivedChallenge = info.Get("challenge");
		}

		if (receivedChallenge != joinContainer.challenge)
		{
			ConnectError("Invalid join response: Challenge mismatch.");
			return true;
		}

		const auto wanted = static_cast<std::uint32_t>(
			std::strtoul(info.Get("securityLevel").data(), nullptr, 10));

		if (wanted > Auth::GetSecurityLevel())
		{
			Logger::Print("raising security level from {} to {} for {}, this takes a moment\n",
				Auth::GetSecurityLevel(), wanted, address.GetString());

			Command::Execute("closemenu popup_reconnectingtoparty");
			Auth::IncreaseSecurityLevel(wanted, "reconnect");

			return true;
		}

		const auto clients = std::strtol(info.Get("clients").data(), nullptr, 10);
		const auto maxClients = std::strtol(info.Get("sv_maxclients").data(), nullptr, 10);

		if (matchType == MatchType::NoMatch)
		{
			ConnectError("Server is not hosting a match.");
			return true;
		}

		if (matchType < MatchType::NoMatch || matchType > MatchType::PrivateParty)
		{
			ConnectError("Invalid join response: Unknown matchtype");
			return true;
		}

		if (info.Get("mapname").empty() || info.Get("gametype").empty())
		{
			ConnectError("Invalid map or gametype.");
			return true;
		}

		const bool isPrivate = joinContainer.info.Get("isPrivate") == "1";

		if (isPrivate && Dvar::Var("password").Get<std::string>().empty())
		{
			ConnectError("A password is required to join this server! Set it at the bottom of the serverlist.");
			return true;
		}

		const bool isUsermap = !info.Get("usermaphash").empty();
		const auto usermapHash = std::strtoul(info.Get("usermaphash").data(), nullptr, 10);

		if (isUsermap && usermapHash != Maps::GetUsermapHash(info.Get("mapname")))
		{
			Command::Execute("closemenu popup_reconnectingtoparty");
			Download::InitiateMapDownload(info.Get("mapname"), isPrivate);
			return true;
		}

		const std::string mod = (*Game::fs_gameDirVar)->current.string;
		const auto serverMod = info.Get("fs_game");

		if (!serverMod.empty() && Utils::String::ToLower(mod) != Utils::String::ToLower(serverMod))
		{
			Command::Execute("closemenu popup_reconnectingtoparty");
			Download::InitiateClientDownload(serverMod, isPrivate, false, joinContainer.isDownloadOnly);
			return true;
		}

		if (!mod.empty() && serverMod.empty())
		{
			Game::Dvar_SetString(*Game::fs_gameDirVar, "");

			if (ModList::cl_modVidRestart.Get<bool>())
			{
				Command::Execute("vid_restart", false);
			}

			Command::Execute("reconnect", false);
			return true;
		}

		joinContainer.motd = TextRenderer::StripMaterialTextIcons(info.Get("sv_motd"));

		if (matchType == MatchType::PartyLobby)
		{
			joinContainer.requestTime = Game::Sys_Milliseconds();
			joinContainer.isAwaitingPlaylist = true;
			Network::SendCommand(joinContainer.target, "getplaylist", Dvar::Var("password").Get<std::string>());

			if (Game::CL_IsCgameInitialized(0))
			{
				Command::Execute("disconnect", true);
			}

			return true;
		}

		if (matchType == MatchType::PrivateParty)
		{
			isPrivatePartyJoin = true;
			PlaylistContinue();
			return true;
		}

		if (maxClients && clients >= maxClients)
		{
			ConnectError("@EXE_SERVERISFULL");
			return true;
		}

		const auto connectMatch = [target = joinContainer.target, version = info.Get("version"), mapName = info.Get("mapname"),
			gameType = info.Get("gametype"), isX86 = info.Get("x64") != "1", serverName = address.GetString(), clients, maxClients]
		{
			Dvar::Var("xblive_privateserver").Set(true);
			Dvar::Var("sv_version").Set(version);

			Game::Menus_CloseAll(Game::uiContext);

			Game::_XSESSION_INFO hostInfo{};

			hostInfo.hostAddress.ina.S_un.S_addr = target.GetIP();

			auto raw = *target.Get();

			Network::RecordServerBuild(target, isX86);

			Game::LiveStorage_EnsureWeHaveStats(0);

			Logger::Print("connecting to {} on {} {}, {}/{} players\n", serverName, mapName, gameType, clients, maxClients);

			Game::CL_ConnectFromParty(0, &hostInfo, raw, 0, 0, mapName.data(), gameType.data());
		};

		if (!LobbyScene::DeferLaunch(connectMatch))
		{
			connectMatch();
		}

		return true;
	}

	static void SkipConfigStringChecksum()
	{
		struct Site
		{
			std::uintptr_t address;
			std::uint8_t expected;
		};

		static const Site sites[] =
		{
			{ 0x1400FFCFB, 0x75 },
			{ 0x1400FFD08, 0x74 },
		};

		for (const auto& site : sites)
		{
			const auto* const live = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(site.address));

			if (*live != site.expected)
			{
				Logger::Warning("configstring checksum site {:X} reads {:02X}, not {:02X}, left alone\n",
					site.address, *live, site.expected);
				continue;
			}

			Utils::Hook::Set<std::uint8_t>(site.address, 0xEB);
		}
	}

	constexpr std::uintptr_t SV_DirectConnect_MemberSlotTest = 0x140237AA0;
	constexpr std::uintptr_t SV_DirectConnect_MemberSlotJge = 0x140237AA6;

	static const std::uint8_t memberSlotTest[] = { 0x3B, 0x05, 0xE2, 0x71, 0x1D, 0x03, 0x0F, 0x8D, 0x9D, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t SV_DirectConnect_InviteTest = 0x140237B59;
	constexpr std::uintptr_t SV_DirectConnect_InviteJz = 0x140237B68;

	static const std::uint8_t inviteTest[] = { 0x48, 0x8B, 0x05, 0x18, 0xD5, 0xAC, 0x00, 0x40, 0x38, 0x78, 0x10, 0x74, 0x31, 0x39, 0x3B, 0x74, 0x2D };

	constexpr std::uintptr_t SV_UserinfoChanged_OnlineTest = 0x140239B5B;
	constexpr std::uintptr_t SV_UserinfoChanged_OnlineJnz = 0x140239B66;
	constexpr std::uintptr_t SV_UserinfoChanged_AddressTest = 0x140239BD4;
	constexpr std::uintptr_t SV_UserinfoChanged_AddressJnz = 0x140239BDC;

	static const std::uint8_t userinfoOnlineTest[] = { 0x48, 0x8B, 0x05, 0x16, 0xB5, 0xAC, 0x00, 0x80, 0x78, 0x10, 0x00, 0x75, 0x76 };
	static const std::uint8_t userinfoAddressTest[] = { 0x80, 0xBC, 0x24, 0x90, 0x00, 0x00, 0x00, 0x00, 0x75, 0x14 };

	constexpr std::uintptr_t SV_SpawnServer_ConstantTest = 0x14023B7A9;
	constexpr std::uintptr_t SV_SpawnServer_ConstantJz = 0x14023B7BE;

	static const std::uint8_t spawnConstantTest[] = { 0x48, 0x8B, 0x05, 0xC8, 0x98, 0xAC, 0x00, 0xC7, 0x05, 0x46, 0x27, 0x2C, 0x06, 0x01, 0x00, 0x00, 0x00, 0x40, 0x38, 0x70, 0x10, 0x74, 0x1D };

	constexpr std::uintptr_t PartyHost_AcceptJoinRequest_LobbyTest = 0x14010E5EF;
	constexpr std::uintptr_t PartyHost_AcceptJoinRequest_LobbyJnz = 0x14010E5F6;
	constexpr std::uintptr_t PartyHost_AcceptJoinRequest_LobbyIdTest = 0x14010E60F;
	constexpr std::uintptr_t PartyHost_AcceptJoinRequest_LobbyIdJnz = 0x14010E61B;
	constexpr std::uintptr_t PartyHost_HandleMemberJoinMsg_LobbyTest = 0x14010D29E;
	constexpr std::uintptr_t PartyHost_HandleMemberJoinMsg_LobbyJz = 0x14010D2A5;
	constexpr std::uintptr_t PartyHost_HandleMemberJoinMsg_LobbyIdTest = 0x14010D2A7;
	constexpr std::uintptr_t PartyHost_HandleMemberJoinMsg_LobbyIdJnz = 0x14010D2B3;

	static const std::uint8_t acceptLobbyTest[] = { 0xE8, 0xDC, 0xF6, 0x13, 0x00, 0x84, 0xC0, 0x75, 0x17 };
	static const std::uint8_t acceptLobbyIdTest[] = { 0xE8, 0x2C, 0xF3, 0x13, 0x00, 0x48, 0x3B, 0x87, 0x00, 0x18, 0x00, 0x00, 0x75, 0xDB };
	static const std::uint8_t memberLobbyTest[] = { 0xE8, 0x2D, 0x0A, 0x14, 0x00, 0x84, 0xC0, 0x74, 0xEF };
	static const std::uint8_t memberLobbyIdTest[] = { 0xE8, 0x94, 0x06, 0x14, 0x00, 0x48, 0x3B, 0x83, 0x00, 0x18, 0x00, 0x00, 0x75, 0xE1 };

	constexpr std::uintptr_t g_natType = 0x140428850;

	static const std::uint8_t natTypeStrict[] = { 0x03, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t PartyHost_Frame_BadHostTest = 0x14010EC7F;
	constexpr std::uintptr_t PartyHost_Frame_BadHostJz = 0x14010EC86;
	constexpr std::uintptr_t PartyClient_HostTimedOut_MigrationTest = 0x1401068A6;
	constexpr std::uintptr_t PartyClient_HostTimedOut_MigrationJz = 0x1401068AD;
	constexpr std::uintptr_t Party_Frame_PartyMigrate_FrameCall = 0x140109A89;
	constexpr std::uintptr_t PartyMigrate_Frame = 0x140114E00;
	constexpr std::uintptr_t PartyMigrate_HandlePacket = 0x140115300;
	constexpr std::uintptr_t CL_ServerTimedOut_MigrationShutdown = 0x1400FD54C;

	static const std::uint8_t badHostTest[] = { 0xE8, 0x1C, 0xB4, 0xFC, 0xFF, 0x84, 0xC0, 0x74, 0x33 };
	static const std::uint8_t hostTimedOutTest[] = { 0xE8, 0xF5, 0x37, 0xFD, 0xFF, 0x84, 0xC0, 0x74, 0x2B };
	static const std::uint8_t migrateHandlePacketEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10 };
	static const std::uint8_t migrationShutdown[] = { 0xC7, 0x03, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x69, 0x47, 0x10, 0x00 };
	static const std::uint8_t returnZero[] = { 0x33, 0xC0, 0xC3 };

	constexpr std::uintptr_t PartyHost_Frame_LobbyIdTest = 0x14010FD98;
	constexpr std::uintptr_t PartyHost_Frame_LobbyIdJnz = 0x14010FDA4;

	constexpr std::uintptr_t PartyHost_Frame_SteamLobbyTests = 0x14010FD7E;
	constexpr std::uintptr_t PartyHost_Frame_CreatingLobbyJnz = 0x14010FD85;
	constexpr std::uintptr_t PartyHost_Frame_InvalidLobbyJz = 0x14010FD92;

	static const std::uint8_t hostSteamLobbyTests[] =
	{
		0xE8, 0x3D, 0xDF, 0x13, 0x00, 0x84, 0xC0, 0x0F, 0x85, 0x35, 0x08, 0x00, 0x00,
		0xE8, 0x40, 0xDF, 0x13, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0x28, 0x08, 0x00, 0x00,
	};
	constexpr std::uintptr_t PartyHost_Frame_TimeoutTest = 0x14011022A;
	constexpr std::uintptr_t PartyHost_Frame_TimeoutJle = 0x140110234;

	static const std::uint8_t hostLobbyIdTest[] = { 0xE8, 0xA3, 0xDB, 0x13, 0x00, 0x48, 0x39, 0x87, 0x00, 0x18, 0x00, 0x00, 0x0F, 0x85, 0x16, 0x08, 0x00, 0x00 };
	static const std::uint8_t memberTimeoutTest[] = { 0x8B, 0xC5, 0x2B, 0x43, 0x34, 0x3D, 0xE0, 0x2E, 0x00, 0x00, 0x7E, 0x17 };

	constexpr std::uintptr_t PartyHost_UsingAssignedTeams_PrivateTest = 0x140112B87;
	constexpr std::uintptr_t PartyHost_UsingAssignedTeams_PrivateJnz = 0x140112B92;

	static const std::uint8_t assignedTeamsPrivateTest[] = { 0x48, 0x8B, 0x05, 0xC2, 0x44, 0x67, 0x06, 0x80, 0x78, 0x10, 0x00, 0x75, 0x05 };

	constexpr std::uintptr_t PartyHost_TestPotentialHost_LoopbackTest = 0x140112AEB;
	constexpr std::uintptr_t PartyHost_TestPotentialHost_LoopbackJnz = 0x140112AEE;

	static const std::uint8_t loopbackTest[] = { 0x80, 0x3F, 0x7F, 0x75, 0x12, 0x80, 0x7F, 0x01, 0x00 };

	constexpr std::uintptr_t Steam_JoinLobby_CurrentTest = 0x14024DEA2;
	constexpr std::uintptr_t Steam_JoinLobby_CurrentJnz = 0x14024DEA9;
	constexpr std::uintptr_t Steam_JoinLobby = 0x14024DE00;
	constexpr std::uintptr_t PartyClient_ParsePartyStateMsg_JoinCall = 0x140107330;
	constexpr std::uintptr_t PartyClient_ParsePartyStateMsg_JoinTest = 0x140107335;
	constexpr std::uintptr_t PartyClient_ParsePartyStateMsg_JoinJnz = 0x14010733F;

	constexpr std::uintptr_t CL_Live_PartyGo_PrivateTests = 0x1400FBF0C;
	constexpr std::uintptr_t CL_Live_PartyGo_NoHostJz = 0x1400FBF11;
	constexpr std::uintptr_t CL_Live_PartyGo_PrivateJz = 0x1400FBF17;

	static const std::uint8_t partyGoPrivateTests[] = { 0x75, 0x05, 0x38, 0x41, 0x10, 0x74, 0x28, 0x80, 0x79, 0x10, 0x00, 0x74, 0x22 };

	constexpr std::uintptr_t PartyHost_StartMatch_SyncCall = 0x140111FCC;
	constexpr std::uintptr_t Com_SyncThreads_Engine = 0x1401F6B30;

	static Utils::Hook startMatchSyncHook;

	static const std::uint8_t joinLobbyCurrentTest[] = { 0x48, 0x39, 0x99, 0xD0, 0x00, 0x00, 0x00, 0x75, 0x09 };
	static const std::uint8_t partyStateJoinTest[] = { 0x4C, 0x8B, 0xA4, 0x24, 0xF8, 0x04, 0x00, 0x00, 0x84, 0xC0, 0x75, 0x11 };

	constexpr std::size_t leaLength = 7;

	static const std::uint8_t leaRcx[] = { 0x48, 0x8D, 0x0D };

	static bool IsLeaRcxOf(std::uintptr_t site, std::uintptr_t target)
	{
		if (!Utils::Hook::MatchesBytes(site, leaRcx, sizeof(leaRcx)))
		{
			return false;
		}

		const auto displacement = Utils::Hook::Get<std::int32_t>(site + sizeof(leaRcx));

		return static_cast<std::uintptr_t>(site + leaLength + displacement) == target;
	}

	static void PointRipAt(std::uintptr_t site, const void* target)
	{
		const auto from = static_cast<std::int64_t>(Utils::Hook::Rebase(site) + leaLength);
		const auto distance = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) - from;

		Utils::Hook::Set<std::int32_t>(site + sizeof(leaRcx), static_cast<std::int32_t>(distance));
	}

	static void PointLeaAt(std::uintptr_t site, const char* text)
	{
		PointRipAt(site, text);
	}

	static const std::uint8_t movRax[] = { 0x48, 0x8B, 0x05 };

	static bool IsMovRaxOf(std::uintptr_t site, std::uintptr_t target)
	{
		if (!Utils::Hook::MatchesBytes(site, movRax, sizeof(movRax)))
		{
			return false;
		}

		const auto displacement = Utils::Hook::Get<std::int32_t>(site + sizeof(movRax));

		return static_cast<std::uintptr_t>(site + leaLength + displacement) == target;
	}

	constexpr std::uintptr_t xblive_privatematch_dvar = 0x146787050;

	struct PlaylistTest
	{
		std::uintptr_t load;
		std::uintptr_t jumpOpcode;
		std::uint8_t jumpExpected;
	};

	static const PlaylistTest playlistTests[] =
	{
		{ 0x140105548, 0x140105553, 0x75 },
		{ 0x1401050D3, 0x1401050DE, 0x75 },
		{ 0x140108EB6, 0x140108EC4, 0x85 },
	};

	constexpr std::uintptr_t Live_Init_PrivateMatch = 0x1402A3D90;
	constexpr std::uintptr_t Live_Init_PrivateMatchName = 0x1402A3D93;
	constexpr std::uintptr_t xblivePrivateMatchName = 0x1403A9068;

	static const std::uint8_t privateMatchRegister[] = { 0x45, 0x33, 0xC0, 0x48, 0x8D, 0x0D, 0xCE, 0x52, 0x10, 0x00, 0x33, 0xD2 };

	constexpr unsigned int dvarInit = 0x800;

	struct MaxClientsSite
	{
		std::uintptr_t nameLea;
		std::uintptr_t nameFlags;
		std::uintptr_t valueFlags;
		unsigned int valueFlagsExpected;
	};

	static const MaxClientsSite maxClientsSites[] =
	{
		{ 0x14023A3F8, 0x14023A3FF, 0x14023A421, 0x402 },
		{ 0x14019DCA9, 0x14019DCB0, 0x14019DCD2, 0x482 },
		{ 0x140239D28, 0x140239D20, 0x140239D49, 0x482 },
	};

	constexpr std::uintptr_t uiMaxClientsName = 0x140386AE0;
	constexpr std::uintptr_t svMaxClientsName = 0x140376100;
	constexpr unsigned int uiMaxClientsFlags = 3;

	static const std::uint8_t movRspFlags[] = { 0xC7, 0x44, 0x24, 0x20 };

	static bool IsFlagsMov(std::uintptr_t site, unsigned int expected)
	{
		return Utils::Hook::MatchesBytes(site, movRspFlags, sizeof(movRspFlags))
			&& Utils::Hook::Get<std::uint32_t>(site + sizeof(movRspFlags)) == expected;
	}

	static void PatchJumps()
	{
		if (Utils::Hook::MatchesBytes(SV_DirectConnect_MemberSlotTest, memberSlotTest, sizeof(memberSlotTest)))
		{
			Utils::Hook::Set<std::uint8_t>(SV_DirectConnect_MemberSlotJge + 1, 0xE9);
			Utils::Hook::Set<std::uint8_t>(SV_DirectConnect_MemberSlotJge, 0x90);
		}
		else
		{
			Logger::Error("party: SV_DirectConnect's member slot test does not read as expected, left alone\n");
		}

		if (Utils::Hook::MatchesBytes(SV_DirectConnect_InviteTest, inviteTest, sizeof(inviteTest)))
		{
			Utils::Hook::Set<std::uint8_t>(SV_DirectConnect_InviteJz, 0xEB);
		}
		else
		{
			Logger::Error("party: SV_DirectConnect's invite test does not read as expected, an online game still turns away joiners\n");
		}

		if (Utils::Hook::MatchesBytes(SV_UserinfoChanged_OnlineTest, userinfoOnlineTest, sizeof(userinfoOnlineTest))
			&& Utils::Hook::MatchesBytes(SV_UserinfoChanged_AddressTest, userinfoAddressTest, sizeof(userinfoAddressTest)))
		{
			Utils::Hook::Nop(SV_UserinfoChanged_OnlineJnz, 2);
			Utils::Hook::Set<std::uint8_t>(SV_UserinfoChanged_AddressJnz, 0xEB);
		}
		else
		{
			Logger::Error("party: SV_UserinfoChanged does not read as expected, joiners outside the session are still dropped\n");
		}

		if (Utils::Hook::MatchesBytes(SV_SpawnServer_ConstantTest, spawnConstantTest, sizeof(spawnConstantTest)))
		{
			Utils::Hook::Set<std::uint8_t>(SV_SpawnServer_ConstantJz, 0xEB);
		}
		else
		{
			Logger::Error("party: SV_SpawnServer's constant configstring test does not read as expected, left alone\n");
		}

		if (Utils::Hook::MatchesBytes(PartyHost_AcceptJoinRequest_LobbyTest, acceptLobbyTest, sizeof(acceptLobbyTest))
			&& Utils::Hook::MatchesBytes(PartyHost_AcceptJoinRequest_LobbyIdTest, acceptLobbyIdTest, sizeof(acceptLobbyIdTest))
			&& Utils::Hook::MatchesBytes(PartyHost_HandleMemberJoinMsg_LobbyTest, memberLobbyTest, sizeof(memberLobbyTest))
			&& Utils::Hook::MatchesBytes(PartyHost_HandleMemberJoinMsg_LobbyIdTest, memberLobbyIdTest, sizeof(memberLobbyIdTest)))
		{
			Utils::Hook::Set<std::uint8_t>(PartyHost_AcceptJoinRequest_LobbyJnz, 0xEB);
			Utils::Hook::Nop(PartyHost_AcceptJoinRequest_LobbyIdJnz, 2);
			Utils::Hook::Nop(PartyHost_HandleMemberJoinMsg_LobbyJz, 2);
			Utils::Hook::Nop(PartyHost_HandleMemberJoinMsg_LobbyIdJnz, 2);
		}
		else
		{
			Logger::Error("party: the party host's steam lobby tests do not read as expected, left alone\n");
		}
	}

	static void RenamePrivateMatch()
	{
		static constexpr char privateServer[] = "xblive_privateserver";

		if (!Utils::Hook::MatchesBytes(Live_Init_PrivateMatch, privateMatchRegister, sizeof(privateMatchRegister))
			|| !IsLeaRcxOf(Live_Init_PrivateMatchName, xblivePrivateMatchName))
		{
			Logger::Error("party: Live_Init's xblive_privatematch does not read as expected, it keeps its name\n");
			return;
		}

		auto* const name = static_cast<char*>(Utils::Hook::AllocateDataNear(Live_Init_PrivateMatchName, sizeof(privateServer)));

		if (!name)
		{
			Logger::Error("party: no room beside the image for xblive_privateserver, xblive_privatematch keeps its name\n");
			return;
		}

		std::memcpy(name, privateServer, sizeof(privateServer));
		PointLeaAt(Live_Init_PrivateMatchName, name);
	}

	static void UnlatchMaxClients()
	{
		for (const auto& site : maxClientsSites)
		{
			if (!IsLeaRcxOf(site.nameLea, uiMaxClientsName) || !IsFlagsMov(site.nameFlags, uiMaxClientsFlags)
				|| !IsFlagsMov(site.valueFlags, site.valueFlagsExpected))
			{
				Logger::Error("party: a ui_maxclients registration does not read as expected, ui_maxclients stays its own latched dvar\n");
				return;
			}
		}

		const char* const svMaxClients = reinterpret_cast<const char*>(Utils::Hook::Rebase(svMaxClientsName));

		for (const auto& site : maxClientsSites)
		{
			PointLeaAt(site.nameLea, svMaxClients);
			Utils::Hook::Xor<std::uint32_t>(site.nameFlags + sizeof(movRspFlags), Game::DVAR_LATCH);
			Utils::Hook::Xor<std::uint32_t>(site.valueFlags + sizeof(movRspFlags), Game::DVAR_LATCH);
		}
	}

	static void DisableHostMigration()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(PartyHost_Frame_BadHostTest, badHostTest, sizeof(badHostTest))
			&& Utils::Hook::MatchesBytes(PartyClient_HostTimedOut_MigrationTest, hostTimedOutTest, sizeof(hostTimedOutTest))
			&& Utils::Hook::BranchesTo(Party_Frame_PartyMigrate_FrameCall, PartyMigrate_Frame, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(PartyMigrate_HandlePacket, migrateHandlePacketEntry, sizeof(migrateHandlePacketEntry))
			&& Utils::Hook::MatchesBytes(CL_ServerTimedOut_MigrationShutdown, migrationShutdown, sizeof(migrationShutdown));

		if (!isExpected)
		{
			Logger::Error("party: the host migration code does not read as expected, host migration stays on\n");
			return;
		}

		Utils::Hook::Set<std::uint8_t>(PartyHost_Frame_BadHostJz, 0xEB);
		Utils::Hook::Set<std::uint8_t>(PartyClient_HostTimedOut_MigrationJz, 0xEB);
		Utils::Hook::Nop(Party_Frame_PartyMigrate_FrameCall, 5);
		Utils::Hook::Nop(CL_ServerTimedOut_MigrationShutdown, sizeof(migrationShutdown));

		for (std::size_t i = 0; i < sizeof(returnZero); ++i)
		{
			Utils::Hook::Set<std::uint8_t>(PartyMigrate_HandlePacket + i, returnZero[i]);
		}
	}

	static void PartyHost_StartMatch_SyncHook()
	{
		isRosterFrozen = true;
		ApplyPendingZombieMode();

		Game::Com_SyncThreads();

		ApplyPendingZombieMode();
		isZombieModeRestorePending = false;

		Game::RMsg_SendMessages();
	}

	static void PatchPartyGo()
	{
		if (Utils::Hook::MatchesBytes(CL_Live_PartyGo_PrivateTests, partyGoPrivateTests, sizeof(partyGoPrivateTests)))
		{
			Utils::Hook::Nop(CL_Live_PartyGo_NoHostJz, 2);
			Utils::Hook::Nop(CL_Live_PartyGo_PrivateJz, 2);
		}
		else
		{
			Logger::Error("party: CL_Live_PartyGo does not read as expected, xpartygo still needs a private match\n");
		}

		if (!Utils::Hook::BranchesTo(PartyHost_StartMatch_SyncCall, Com_SyncThreads_Engine, HOOK_CALL)
			|| !startMatchSyncHook.Initialize(PartyHost_StartMatch_SyncCall, reinterpret_cast<void*>(PartyHost_StartMatch_SyncHook), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("party: could not hook PartyHost_StartMatch's Com_SyncThreads, members may time out while the host loads\n");
			return;
		}

		startMatchSyncHook.Quick();
	}

	static void PatchLobbyJoins()
	{
		if (Utils::Hook::MatchesBytes(Steam_JoinLobby_CurrentTest, joinLobbyCurrentTest, sizeof(joinLobbyCurrentTest)))
		{
			Utils::Hook::Set<std::uint8_t>(Steam_JoinLobby_CurrentJnz, 0xEB);
		}
		else
		{
			Logger::Error("party: Steam_JoinLobby does not read as expected, a lobby we are in is not joined again\n");
		}

		if (Utils::Hook::BranchesTo(PartyClient_ParsePartyStateMsg_JoinCall, Steam_JoinLobby, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(PartyClient_ParsePartyStateMsg_JoinTest, partyStateJoinTest, sizeof(partyStateJoinTest)))
		{
			Utils::Hook::Nop(PartyClient_ParsePartyStateMsg_JoinCall, 5);
			Utils::Hook::Set<std::uint8_t>(PartyClient_ParsePartyStateMsg_JoinJnz, 0xEB);
		}
		else
		{
			Logger::Error("party: PartyClient_ParsePartyStateMsg does not read as expected, it still joins the host's steam lobby\n");
		}
	}

	static void PatchPartyHost()
	{
		if (Utils::Hook::MatchesBytes(PartyHost_UsingAssignedTeams_PrivateTest, assignedTeamsPrivateTest, sizeof(assignedTeamsPrivateTest)))
		{
			Utils::Hook::Set<std::uint8_t>(PartyHost_UsingAssignedTeams_PrivateJnz, 0xEB);
		}
		else
		{
			Logger::Error("party: PartyHost_UsingAssignedTeams does not read as expected, teams are still assigned outside a private match\n");
		}

		if (Utils::Hook::MatchesBytes(PartyHost_TestPotentialHost_LoopbackTest, loopbackTest, sizeof(loopbackTest)))
		{
			Utils::Hook::Set<std::uint8_t>(PartyHost_TestPotentialHost_LoopbackJnz, 0xEB);
		}
		else
		{
			Logger::Error("party: PartyHost_TestPotentialHost does not read as expected, a host at 127.0.0.1 is still turned down\n");
		}

		if (Utils::Hook::MatchesBytes(PartyHost_Frame_SteamLobbyTests, hostSteamLobbyTests, sizeof(hostSteamLobbyTests)))
		{
			Utils::Hook::Nop(PartyHost_Frame_CreatingLobbyJnz, 6);
			Utils::Hook::Nop(PartyHost_Frame_InvalidLobbyJz, 6);
		}
		else
		{
			Logger::Error("party: PartyHost_Frame's steam lobby tests do not read as expected, the host frame still needs a steam lobby\n");
		}

		if (Utils::Hook::MatchesBytes(PartyHost_Frame_LobbyIdTest, hostLobbyIdTest, sizeof(hostLobbyIdTest)))
		{
			Utils::Hook::Nop(PartyHost_Frame_LobbyIdJnz, 6);
		}
		else
		{
			Logger::Error("party: PartyHost_Frame's lobby id test does not read as expected, a mismatch still stops the host frame\n");
		}

		if (Utils::Hook::MatchesBytes(PartyHost_Frame_TimeoutTest, memberTimeoutTest, sizeof(memberTimeoutTest)))
		{
			Utils::Hook::Set<std::uint8_t>(PartyHost_Frame_TimeoutJle, 0xEB);
		}
		else
		{
			Logger::Error("party: PartyHost_Frame's member timeout does not read as expected, silent members are still dropped\n");
		}
	}

	static void PointPlaylistTests(Game::dvar_t* partyEnable)
	{
		for (const auto& test : playlistTests)
		{
			if (!IsMovRaxOf(test.load, xblive_privatematch_dvar) || Utils::Hook::Get<std::uint8_t>(test.jumpOpcode) != test.jumpExpected)
			{
				Logger::Error("party: a playlist test does not read as expected, the playlist rules still follow xblive_privatematch\n");
				return;
			}
		}

		auto** const partyEnableRef = static_cast<Game::dvar_t**>(Utils::Hook::AllocateDataNear(playlistTests[0].load, sizeof(Game::dvar_t*)));

		if (!partyEnableRef)
		{
			Logger::Error("party: no room beside the image for party_enable, the playlist rules still follow xblive_privatematch\n");
			return;
		}

		*partyEnableRef = partyEnable;

		for (const auto& test : playlistTests)
		{
			PointRipAt(test.load, partyEnableRef);
			Utils::Hook::Xor<std::uint8_t>(test.jumpOpcode, 1);
		}
	}

	constexpr std::uintptr_t g_lobbyCreateInProgress = 0x14651F082;

	::Steam::SteamID Party::GenerateLobbyId()
	{
		::Steam::SteamID id;

		id.accountID = Game::Sys_Milliseconds();
		id.universe = 1;
		id.accountType = 8;
		id.accountInstance = 0x40000;

		return id;
	}

	bool Party::PlaylistAwaiting()
	{
		return joinContainer.isAwaitingPlaylist;
	}

	void Party::PlaylistContinue()
	{
		Dvar::Var("xblive_privateserver").Set(false);

		Utils::Hook::Set<bool>(g_lobbyCreateInProgress, false);

		joinContainer.isAwaitingPlaylist = false;

		const ::Steam::SteamID id = GenerateLobbyId();

		if (joinContainer.target.IsLoopback())
		{
			if (*Game::numIP)
			{
				joinContainer.target.SetIP(Game::localIP[0].full);
				joinContainer.target.SetType(Game::NA_IP);

				Logger::Print("Trying to connect to party with loopback address, using a local ip instead: {}\n", joinContainer.target.GetString());
			}
			else
			{
				Logger::Print("Trying to connect to party with loopback address, but no local ip was found.\n");
			}
		}

		lobbyMap[id.bits] = joinContainer.target;

		reinterpret_cast<char(*)(std::uint64_t, char)>(Utils::Hook::Rebase(Steam_JoinLobby))(id.bits, 0);
	}

	void Party::PlaylistError(const std::string& error)
	{
		joinContainer.isValid = false;
		joinContainer.isAwaitingPlaylist = false;

		ConnectError(error);
	}

	Party::Party()
	{
		SkipConfigStringChecksum();
		PatchJumps();
		RenamePrivateMatch();
		UnlatchMaxClients();
		DisableHostMigration();
		PatchPartyHost();
		PatchLobbyJoins();
		PatchPartyGo();

		if (Utils::Hook::MatchesBytes(g_natType, natTypeStrict, sizeof(natTypeStrict)))
		{
			Utils::Hook::Set<int>(g_natType, 1);
		}
		else
		{
			Logger::Error("party: g_natType does not hold its image value, the NAT type is left alone\n");
		}

		Scheduler::Once([]
		{
			Dvar::Register("sv_version", "", Game::DVAR_SERVERINFO | dvarInit, "Server version");
			party_enable = Dvar::Register("party_enable", Dedicated::IsEnabled(), Game::DVAR_NONE, "Enable party system");
			Dvar::Register("xblive_privatematch", true, dvarInit, "private match");
			party_host = Dvar::Register("party_host", false, Game::DVAR_ROM, "True if we are the host of the party");

			PointPlaylistTests(party_enable.Get());
		}, Scheduler::Pipeline::MAIN);

		Scheduler::Loop([]
		{
			const bool isHost = IsHostingParty();

			if (party_host.IsValid() && party_host.Get<bool>() != isHost)
			{
				party_host.Set(isHost);
			}
		}, Scheduler::Pipeline::MAIN);

		Network::OnPacket("getinfo", HandleGetInfo);

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop([]
			{
				if (joinContainer.isValid && (Game::Sys_Milliseconds() - joinContainer.startTime) > 10'000)
				{
					joinContainer.isValid = false;

					if (!ZWNet::TryRelayAfterDirectTimeout(joinContainer.target.GetString()))
					{
						ConnectError("Server connection timed out.");
					}
				}

				if (joinContainer.isAwaitingPlaylist && (Game::Sys_Milliseconds() - joinContainer.requestTime) > 5'000)
				{
					joinContainer.isAwaitingPlaylist = false;
					ConnectError("Playlist request timed out.");
				}
			}, Scheduler::Pipeline::CLIENT);
		}

		Command::Add("connect", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			if (Game::CL_IsCgameInitialized(0))
			{
				isPrivatePartyJoin = false;
				Command::Execute("disconnect", false);
				Command::Execute(params->Join(0), false);
				return;
			}

			const Network::Address target(params->Get(1));

			if (!target.IsValid() || !ZWNet::BeginEndpointJoin(target.GetString()))
			{
				Connect(target);
			}
		});

		Command::Add("reconnect", []
		{
			if (ZWNet::BeginManagedReconnect(joinContainer.target.GetString()))
			{
				return;
			}

			Connect(joinContainer.target);
		});

		Events::OnDvarInit([]
		{
			Dvar::Register("character_1", "None", Game::DVAR_CODINFO, "Character assigned to player 1");
			Dvar::Register("character_2", "None", Game::DVAR_CODINFO, "Character assigned to player 2");
			Dvar::Register("character_3", "None", Game::DVAR_CODINFO, "Character assigned to player 3");
			Dvar::Register("character_4", "None", Game::DVAR_CODINFO, "Character assigned to player 4");
			Dvar::Register("character_1_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 1");
			Dvar::Register("character_2_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 2");
			Dvar::Register("character_3_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 3");
			Dvar::Register("character_4_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 4");
			Dvar::Register("party_currentPlayers", 0, 0, maxPartySlots, Game::DVAR_CODINFO, "Total current players in the party");
			Dvar::Register("party_realPlayers", 0, 0, maxPartySlots, Game::DVAR_CODINFO, "Current real players in the party");
			Dvar::Register("party_currentHost", "", Game::DVAR_NONE, "Current private-party host display name");
			Dvar::Register("party_roster_loading", true, Game::DVAR_NONE, "Waiting for a complete private-party roster snapshot");

			Dvar::Register("autosave_map", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_round", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_zombiemode", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_kills", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_score", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_time", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_date", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_mapname_display", "", Game::DVAR_INIT, "");
			Dvar::Register("autosave_load", false, Game::DVAR_INIT, "");

			mapPreference = Dvar::Register("zw3_pref_ui_mapname", "mp_cod5_prototype", Game::DVAR_ARCHIVE, "Saved private-match map preference");
			isMapPreferenceReady = true;

			Game::Dvar_RegisterEnum("zombiemode", zombieModeNames, 0, Game::DVAR_CODINFO, "Change the selected zombie mode");
			Dvar::Register("ui_hitmarker", 1, 0, 1, Game::DVAR_CODINFO, "Toggle hitmarkers");
			Dvar::Register("ui_zombiecounter", 0, 0, 1, Game::DVAR_CODINFO, "Toggle a zombie counter");
			Dvar::Register("ui_showdamage", 1, 0, 1, Game::DVAR_CODINFO, "Toggle damage visibility");
			Dvar::Register("ui_perklocations", 0, 0, 1, Game::DVAR_CODINFO, "Toggle perk locations");
			Dvar::Register("thirdPerson", 0, 0, 1, Game::DVAR_CODINFO, "Toggle third person");
			Dvar::Register("weather", 1, 0, 2, Game::DVAR_NONE, "Select weather effects");
			Dvar::Register("dayNightCycle", 1, 0, 1, Game::DVAR_NONE, "Toggle the day and night cycle");
			Dvar::Register("bg_omnimovement", 1, 0, 1, Game::DVAR_NONE, "Toggle omnimovement");
			Dvar::Register("addBots", 0, 0, 3, Game::DVAR_CODINFO, "Change the amount of bots");
			Game::Dvar_RegisterEnum("partyPrivacy", partyPrivacyNames, 0, Game::DVAR_CODINFO, "Party privacy");

			for (auto& setting : hostSettings)
			{
				setting.preference = Dvar::Register(setting.preferenceName, setting.defaultValue, 0, setting.max, Game::DVAR_ARCHIVE, setting.description);
			}

			arePreferencesReady = true;
		});

		UIScript::Add("ApplyCustomizationSettings", [](const UIScript::Token&)
		{
			ApplyCustomizationSettings();
		});

		UIScript::Add("SaveCustomizationSettings", [](const UIScript::Token&)
		{
			SaveCustomizationSettings();
		});

		for (auto& setting : hostSettings)
		{
			if (!setting.saveScript)
			{
				continue;
			}

			UIScript::Add(setting.saveScript, [&setting](const UIScript::Token&)
			{
				if (CanUseLocalPreferences())
				{
					SavePreference(setting);
				}
			});
		}

		UIScript::Add("SaveZombieModeSetting", [](const UIScript::Token&)
		{
			SaveZombieModePreference();
		});

		UIScript::Add("PreparePrivateMatchStart", [](const UIScript::Token&)
		{
			Dvar::Var("autosave_load").Set(false);
			isAutosaveLaunchPending = false;

			DeleteFileA(GetAutosavePath().data());

			SaveZombieModePreference();

			zombieModeStartValue = std::clamp(Dvar::Var("zw3_pref_zombiemode").Get<int>(), 0, zombieModeCount - 1);
			isZombieModeRestorePending = true;
			ApplyPendingZombieMode();
		});

		UIScript::Add("SaveMapSetting", [](const UIScript::Token&)
		{
			SaveSelectedMapPreference(false);
		});

		UIScript::Add("ApplyMapSetting", [](const UIScript::Token&)
		{
			ApplySelectedMapPreference();
		});

		UIScript::Add("SaveQuitToPrivateLobby", [](const UIScript::Token&)
		{
			const bool isPrivateMatchHost = IsHostingParty() && Dvar::Var("xblive_privatematch").Get<bool>();

			if (!isPrivateMatchHost)
			{
				Command::Execute("disconnect", false);
				return;
			}

			SaveCustomizationSettings();
			SaveSelectedMapPreference(true);
			Command::Execute("disconnect", false);

			const auto startedAt = Game::Sys_Milliseconds();

			Scheduler::Schedule([startedAt, disconnectedAt = -1]() mutable
			{
				const auto now = Game::Sys_Milliseconds();

				if (Game::CL_IsCgameInitialized(0))
				{
					return (now - startedAt) > 10'000;
				}

				if (disconnectedAt < 0)
				{
					disconnectedAt = now;
					return false;
				}

				if ((now - disconnectedAt) < 150)
				{
					return false;
				}

				RecreatePrivateMatchLobby();
				return true;
			}, Scheduler::Pipeline::MAIN, 50ms);
		});

		Scheduler::Once(ApplyCustomizationSettings, Scheduler::Pipeline::MAIN, 1s);

		Scheduler::Loop([]
		{
			static bool wasUsingRemoteSettings = false;
			const bool isUsingRemoteSettings = !IsHostingParty() && Dvar::Var("xblive_privateserver").Get<bool>();

			if (wasUsingRemoteSettings && !isUsingRemoteSettings)
			{
				ApplyCustomizationSettings();
			}

			wasUsingRemoteSettings = isUsingRemoteSettings;
		}, Scheduler::Pipeline::MAIN, 250ms);

		Events::OnSVInit([]
		{
			Scheduler::Once([]
			{
				FinalizeServerCharacterRoster(false);
			}, Scheduler::Pipeline::SERVER, 1s);

			Scheduler::Once([]
			{
				FinalizeServerCharacterRoster(true);
			}, Scheduler::Pipeline::SERVER, 3s);
		});

		UIScript::Add("RefreshCharacterRoster", [](const UIScript::Token&)
		{
			if (!IsHostingParty())
			{
				return;
			}

			isDirectLaunchRoster = false;
			isRosterFrozen = false;

			std::vector<RealCharacterParticipant> participants;

			if (!CollectLobbyParticipants(participants))
			{
				return;
			}

			RandomizeCharactersForClients();
			BroadcastDvarUpdate();
			Command::Execute("xupdatepartystate");
		});

		UIScript::Add("CycleSmartBots", [](const UIScript::Token&)
		{
			if (!IsHostingParty())
			{
				return;
			}

			isDirectLaunchRoster = false;
			isRosterFrozen = false;

			std::vector<RealCharacterParticipant> participants;

			if (!CollectLobbyParticipants(participants))
			{
				return;
			}

			const int realPlayers = std::clamp(static_cast<int>(participants.size()), 1, maxPartySlots);
			const int maxBots = std::max(0, maxPartySlots - realPlayers);
			const int currentBots = std::clamp(Dvar::Var("addBots").Get<int>(), 0, maxBots);
			int nextBots = 0;

			if (maxBots > 0)
			{
				nextBots = (currentBots + 1) % (maxBots + 1);
			}

			Dvar::Var("addBots").Set(nextBots);

			if (arePreferencesReady)
			{
				Dvar::Var("zw3_pref_addBots").Set(nextBots);
			}

			Dvar::Var("party_realPlayers").Set(realPlayers);
			Dvar::Var("party_currentPlayers").Set(realPlayers + nextBots);
			RandomizeCharactersForClients();
			Dvar::Var("party_roster_loading").Set(false);
			BroadcastDvarUpdate();
			Command::Execute("xupdatepartystate");
		});

		UIScript::Add("JoinParty", [](const UIScript::Token&)
		{
			const auto ip = Dvar::Var("partyconnect_ip").Get<std::string>();
			const auto port = Dvar::Var("partyconnect_port").Get<std::string>();

			if (!ip.empty() && !port.empty())
			{
				Connect(Network::Address(ip + ":" + port));
			}
		});

		UIScript::Add("LoadSave", [](const UIScript::Token&)
		{
			if (IsHostingParty())
			{
				ShowAutosave();
				return;
			}

			const auto startedAt = Game::Sys_Milliseconds();

			Scheduler::Schedule([startedAt]
			{
				if (IsHostingParty())
				{
					ShowAutosave();
					return true;
				}

				return (Game::Sys_Milliseconds() - startedAt) > 2000;
			}, Scheduler::Pipeline::MAIN, 50ms);
		});

		UIScript::Add("LoadSaveAccepted", [](const UIScript::Token&)
		{
			if (isAutosaveLaunchPending)
			{
				return;
			}

			const auto mapName = Dvar::Var("autosave_map").Get<std::string>();
			const bool isValidMapName = !mapName.empty() && mapName.size() <= 64 && std::ranges::all_of(mapName, [](const unsigned char character)
			{
				return std::isalnum(character) != 0 || character == '_' || character == '-';
			});

			if (!isValidMapName)
			{
				return;
			}

			isAutosaveLaunchPending = true;
			Dvar::Var("autosave_load").Set(true);
			Command::Execute("closemenu popup_autosave", false);

			Scheduler::Once([mapName]
			{
				if (!isAutosaveLaunchPending)
				{
					return;
				}

				Command::Execute("map " + mapName, false);
				isAutosaveLaunchPending = false;
			}, Scheduler::Pipeline::MAIN, 100ms);
		});

		Network::OnPacket("dvarUpdate", [](Network::Address&, const std::string& data)
		{
			if (Dedicated::IsEnabled())
			{
				return;
			}

			ApplyHostInfo(Utils::InfoString(data));
		});

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop([]
			{
				static std::vector<int> lastSettings(std::size(hostSettings), -1);
				static bool wasHosting = false;
				static bool needsPartyStateUpdate = false;
				static std::string lastRosterSignature;
				static std::string candidateRosterSignature;
				static int candidateRosterTicks = 0;

				bool needsBroadcast = false;
				const bool isHosting = IsHostingParty();
				const bool didStartHosting = isHosting && !wasHosting;

				if (didStartHosting)
				{
					ApplyCustomizationSettings();
				}

				for (std::size_t i = 0; i < std::size(hostSettings); ++i)
				{
					const int value = GetHostSetting(hostSettings[i]);

					if (value != lastSettings[i])
					{
						lastSettings[i] = value;
						needsBroadcast = true;
					}
				}

				if (didStartHosting && !isDirectLaunchRoster)
				{
					isRosterFrozen = false;
					CharacterAssignments::ResetAll();
					hostCharacter.clear();
					Dvar::Var("party_roster_loading").Set(true);
				}

				if (isHosting)
				{
					std::vector<RealCharacterParticipant> participants;

					if (!isRosterFrozen && !isDirectLaunchRoster && CollectLobbyParticipants(participants))
					{
						const int realPlayers = std::clamp(static_cast<int>(participants.size()), 1, maxPartySlots);
						const int botsToAdd = std::clamp(Dvar::Var("addBots").Get<int>(), 0, maxPartySlots - realPlayers);
						const auto signature = BuildRosterSignature(participants, botsToAdd);

						if (signature != candidateRosterSignature)
						{
							candidateRosterSignature = signature;
							candidateRosterTicks = 1;
						}
						else if (candidateRosterTicks < 3)
						{
							++candidateRosterTicks;
						}

						const bool isInitialRoster = Dvar::Var("party_roster_loading").Get<bool>();
						const bool hasSettled = candidateRosterTicks >= 3 && signature != lastRosterSignature;

						if (isInitialRoster || hasSettled)
						{
							RandomizeCharactersForClients();
							lastRosterSignature = signature;
							needsBroadcast = true;
							needsPartyStateUpdate = true;
						}
					}
				}
				else if (wasHosting && !isRosterFrozen && !isDirectLaunchRoster)
				{
					Dvar::Var("party_currentPlayers").Set(0);
					Dvar::Var("party_realPlayers").Set(0);
					Dvar::Var("addBots").Set(0);
					Dvar::Var("party_roster_loading").Set(true);
					CharacterAssignments::ResetAll();
					hostCharacter.clear();

					for (int slot = 0; slot < maxPartySlots; ++slot)
					{
						PublishRosterSlot(slot, "None", "None");
					}

					needsBroadcast = true;
					needsPartyStateUpdate = true;
				}

				if (isHosting && (isRosterFrozen || isDirectLaunchRoster) && PublishRuntimeCharacterRoster())
				{
					needsBroadcast = true;
				}

				if (isHosting)
				{
					const auto hostName = TextRenderer::StripColors(Dvar::Var("name").Get<std::string>());

					if (SetStringIfChanged("party_currentHost", "^7" + hostName))
					{
						needsBroadcast = true;
					}
				}
				else
				{
					auto remoteHost = Dvar::Var("party_hostname").Get<std::string>();

					if (remoteHost.empty())
					{
						remoteHost = joinContainer.info.Get("party_currentHost");
					}

					if (remoteHost.empty())
					{
						remoteHost = joinContainer.info.Get("hostname");
					}

					if (!remoteHost.empty())
					{
						SetStringIfChanged("party_currentHost", "^7" + TextRenderer::StripColors(remoteHost));
					}
				}

				if (didStartHosting)
				{
					needsBroadcast = true;
				}

				wasHosting = isHosting;

				if (needsBroadcast)
				{
					BroadcastDvarUpdate();
				}

				if (needsPartyStateUpdate)
				{
					Command::Execute("xupdatepartystate");
					needsPartyStateUpdate = false;
				}
			}, Scheduler::Pipeline::MAIN, 100ms);
		}
	}
}
