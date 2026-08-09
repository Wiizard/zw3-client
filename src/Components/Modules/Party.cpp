#include <Utils/InfoString.hpp>
#include "Party.hpp"
#include "Auth.hpp"
#include "Download.hpp"
#include "Friends.hpp"
#include "Gamepad.hpp"
#include "ModList.hpp"
#include "Node.hpp"
#include "ServerList.hpp"
#include "Stats.hpp"
#include "Localization.hpp"
#include "TextRenderer.hpp"
#include "Voice.hpp"
#include "Events.hpp"
#include "Bots.hpp"
#include "CharacterAssignments.hpp"
#include <version.hpp>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <charconv>
#include <functional>
#include <array>
#include <algorithm>

#define CL_MOD_LOADING

namespace Components
{
	class JoinContainer
	{
	public:
		enum MatchType : int32_t {
			NO_MATCH = 0,
			PARTY_LOBBY = 1,
			DEDICATED_MATCH = 2,
			PRIVATE_PARTY = 3,

			COUNT
		};

		Network::Address target;
		std::string challenge;
		std::string motd;
		DWORD joinTime;
		bool valid;
		bool downloadOnly;
		MatchType matchType;

		Utils::InfoString info;

		// Party-specific stuff
		DWORD requestTime;
		bool awaitingPlaylist;
	};

	static JoinContainer Container;
	std::map<std::uint64_t, Network::Address> Party::LobbyMap;

	Dvar::Var Party::PartyEnable;
	Dvar::Var Party::ServerVersion;

	std::map<uint64_t, std::vector<Components::Network::Address>> Party::g_xuidToPublicAddressMap;
	static std::string s_hostCharacter;
	static bool s_characterRosterFrozen = false;
	static bool s_directLaunchRoster = false;
	static bool s_autosaveMapLaunchPending = false;
	const int MAX_PARTY_SLOTS = 4;
	static bool SetStringDvarIfChanged(const char* name, const std::string& value)
	{
		auto* dvar = Game::Dvar_FindVar(name);
		if (!dvar)
		{
			return false;
		}

		const auto* current = dvar->current.string ? dvar->current.string : "";
		if (value == current)
		{
			return false;
		}

		Game::Dvar_SetString(dvar, value.c_str());
		return true;
	}

	static bool ApplyCharacterRosterSnapshot(const Utils::InfoString& info)
	{
		std::array<std::string, MAX_PARTY_SLOTS> characters{};
		std::array<std::string, MAX_PARTY_SLOTS> owners{};

		for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
		{
			characters[slot] = info.get(Utils::String::VA("character_%d", slot + 1));
			owners[slot] = info.get(Utils::String::VA("character_%d_player", slot + 1));

			if (characters[slot].empty() || owners[slot].empty())
			{
				return false;
			}

			const bool characterEmpty = characters[slot] == "None";
			const bool ownerEmpty = owners[slot] == "None";
			if (characterEmpty != ownerEmpty)
			{
				return false;
			}
		}

		if (characters[0] == "None" || owners[0] == "None")
		{
			return false;
		}

		for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
		{
			SetStringDvarIfChanged(Utils::String::VA("character_%d", slot + 1), characters[slot]);
			SetStringDvarIfChanged(Utils::String::VA("character_%d_player", slot + 1), owners[slot]);
		}

		Dvar::Var("party_roster_loading").set(false);
		return true;
	}

	static bool TryParseHexXuid(const std::string& value, std::uint64_t& xuid)
	{
		if (value.empty() || value.size() > 16)
		{
			return false;
		}

		std::uint64_t parsedXuid = 0;
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsedXuid, 16);
		if (error != std::errc{} || end != value.data() + value.size())
		{
			return false;
		}

		xuid = parsedXuid;
		return true;
	}

	static bool TryParseClientCount(const std::string& value, unsigned int& count)
	{
		if (value.empty())
		{
			return false;
		}

		unsigned int parsedCount = 0;
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsedCount, 10);
		if (error != std::errc{} || end != value.data() + value.size())
		{
			return false;
		}

		count = parsedCount;
		return true;
	}

	void Party::TrackClientAddress(uint64_t xuid, const Network::Address& address)
	{
		if (xuid == 0)
		{
			return;
		}

		auto& addresses = g_xuidToPublicAddressMap[xuid];
		const auto addressString = address.getString();
		auto existing = std::find_if(addresses.begin(), addresses.end(), [&addressString](const Network::Address& knownAddress)
			{
				return knownAddress.getString() == addressString;
			});

		if (existing != addresses.end())
		{
			*existing = address;
			return;
		}

		addresses.push_back(address);
	}

	uint64_t Party::GetLocalPlayerXUID() {
		return Steam::SteamUser()->GetSteamID().bits;
	}

	SteamID Party::GenerateLobbyId()
	{
		SteamID id;

		id.accountID = Game::Sys_Milliseconds();
		id.universe = 1;
		id.accountType = 8;
		id.accountInstance = 0x40000;

		return id;
	}

	Network::Address Party::Target()
	{
		return Container.target;
	}

	void Party::Connect(Network::Address target, bool downloadOnly)
	{
		Node::Add(target);

		Container.valid = true;
		Container.awaitingPlaylist = false;
		Container.joinTime = Game::Sys_Milliseconds();
		Container.target = target;
		Container.challenge = Utils::Cryptography::Rand::GenerateChallenge();
		Container.downloadOnly = downloadOnly;

		Dvar::Var("party_roster_loading").set(true);

		Utils::InfoString clientRequestInfo;
		clientRequestInfo.set("challenge", Container.challenge.c_str());
		clientRequestInfo.set("gamename", "IW4");
		clientRequestInfo.set("protocol", std::to_string(PROTOCOL));
		clientRequestInfo.set("version", REVISION_STR);

		const auto localClientSteamID = GetLocalPlayerXUID();
		clientRequestInfo.set("xuid", Utils::String::VA("%llX", localClientSteamID));

		Network::SendCommand(Container.target, "getinfo", clientRequestInfo.build());

		Command::Execute("openmenu popup_reconnectingtoparty");
	}

	const char* Party::GetLobbyInfo(SteamID lobby, const std::string& key)
	{
		if (LobbyMap.contains(lobby.bits))
		{
			Network::Address address = LobbyMap[lobby.bits];

			if (key == "addr"s)
			{
				return Utils::String::VA("%d", address.getIP().full);
			}

			if (key == "port"s)
			{
				return Utils::String::VA("%d", address.getPort());
			}
		}

		return "212";
	}

	void Party::RemoveLobby(SteamID lobby)
	{
		LobbyMap.erase(lobby.bits);
	}

	void Party::ConnectError(const std::string& message)
	{
		Command::Execute("closemenu popup_reconnectingtoparty");
		Dvar::Var("partyend_reason").set(message);
		Command::Execute("openmenu menu_xboxlive_partyended");
	}

	std::string Party::GetMotd()
	{
		return Container.motd;
	}

	std::string Party::GetHostName()
	{
		return Container.info.get("hostname");
	}

	int Party::GetMaxClients()
	{
		const auto value = Container.info.get("sv_maxclients");
		return std::strtol(value.data(), nullptr, 10);
	}

	bool Party::PlaylistAwaiting()
	{
		return Container.awaitingPlaylist;
	}

	void Party::PlaylistContinue()
	{
		Dvar::Var("xblive_privateserver").set(false);

		// Ensure we can join
		*Game::g_lobbyCreateInProgress = false;

		Container.awaitingPlaylist = false;

		SteamID id = GenerateLobbyId();

		// Temporary workaround
		// TODO: Patch the 127.0.0.1 -> loopback mapping in the party code
		if (Container.target.isLoopback())
		{
			if (*Game::numIP)
			{
				Container.target.setIP(*Game::localIP);
				Container.target.setType(Game::netadrtype_t::NA_IP);

				Logger::Print("Trying to connect to party with loopback address, using a local ip instead: {}\n", Container.target.getString());
			}
			else
			{
				Logger::Print("Trying to connect to party with loopback address, but no local ip was found.\n");
			}
		}

		LobbyMap[id.bits] = Container.target;

		Game::Steam_JoinLobby(id, 0);
	}

	void Party::PlaylistError(const std::string& error)
	{
		Container.valid = false;
		Container.awaitingPlaylist = false;

		ConnectError(error);
	}

	DWORD Party::UIDvarIntStub(char* dvar)
	{
		if (!_stricmp(dvar, "onlinegame") && !Stats::IsMaxLevel())
		{
			return 0x649E660;
		}

		return Utils::Hook::Call<DWORD(char*)>(0x4D5390)(dvar);
	}

	bool Party::IsInLobby()
	{
		return (!Dedicated::IsRunning() && PartyEnable.get<bool>() && Dvar::Var("party_host").get<bool>());
	}

	bool Party::IsInUserMapLobby()
	{
		return (IsInLobby() && Maps::IsUserMap((*Game::ui_mapname)->current.string));
	}

	bool Party::IsEnabled()
	{
		return PartyEnable.get<bool>();
	}

	void Party::BroadcastDvarUpdate()
	{
		if (!Dvar::Var("party_host").get<bool>())
		{
			return;
		}

		int maxPartyMembers = (*Game::party_maxplayers)->current.integer;
		if (maxPartyMembers <= 0) {
			maxPartyMembers = MAX_PARTY_SLOTS;
		}

		int memberCount = Game::PartyHost_CountMembers(Game::g_lobbyData);
		if (memberCount <= 0)
		{
			return;
		}

		Utils::InfoString info;
		int zombieModeVal = Dvar::Var("zombiemode").get<int>();
		int hitmarkersVal = Dvar::Var("ui_hitmarker").get<int>();
		int showDamageVal = Dvar::Var("ui_showdamage").get<int>();
		int zombieCounterVal = Dvar::Var("ui_zombiecounter").get<int>();
		int perkLocationsVal = Dvar::Var("ui_perklocations").get<int>();
		int thirdPersonVal = Dvar::Var("thirdPerson").get<int>();
		int addBotsVal = Dvar::Var("addBots").get<int>();
		int partyPrivacyVal = Dvar::Var("partyPrivacy").get<int>();
		std::string character1Val = Dvar::Var("character_1").get<std::string>();
		std::string character2Val = Dvar::Var("character_2").get<std::string>();
		std::string character3Val = Dvar::Var("character_3").get<std::string>();
		std::string character4Val = Dvar::Var("character_4").get<std::string>();

		info.set("zombiemode", std::to_string(zombieModeVal));
		info.set("ui_hitmarker", std::to_string(hitmarkersVal));
		info.set("ui_showdamage", std::to_string(showDamageVal));
		info.set("ui_zombiecounter", std::to_string(zombieCounterVal));
		info.set("ui_perklocations", std::to_string(perkLocationsVal));
		info.set("thirdPerson", std::to_string(thirdPersonVal));
		info.set("addBots", std::to_string(addBotsVal));
		info.set("partyPrivacy", std::to_string(partyPrivacyVal));
		info.set("character_1", character1Val);
		info.set("character_2", character2Val);
		info.set("character_3", character3Val);
		info.set("character_4", character4Val);

		int totalPlayers = Dvar::Var("party_currentPlayers").get<int>();
		int realPlayers = Dvar::Var("party_realPlayers").get<int>();
		info.set("party_currentPlayers", std::to_string(totalPlayers));
		info.set("party_realPlayers", std::to_string(realPlayers));
		info.set("party_currentHost", Dvar::Var("party_currentHost").get<std::string>());

		info.set("character_1_player", Dvar::Var("character_1_player").get<std::string>());
		info.set("character_2_player", Dvar::Var("character_2_player").get<std::string>());
		info.set("character_3_player", Dvar::Var("character_3_player").get<std::string>());
		info.set("character_4_player", Dvar::Var("character_4_player").get<std::string>());

		const std::string builtDvarString = info.build();
		int totalSent = 0;
		std::unordered_set<std::string> sentAddresses;

		for (int i = 0; i < maxPartyMembers; ++i)
		{
			if (i >= MAX_PARTY_SLOTS) {
				break;
			}

			auto& member = Game::g_lobbyData->partyMembers[i];

			if (member.status == 0)
			{
				continue;
			}

			uint64_t memberXuid = member.player;

			/*if (memberXuid == GetLocalPlayerXUID())
			{
				continue;
			}

			Components::Network::Address targetAddr;*/

			/*auto it = Party::g_xuidToPublicAddressMap.find(memberXuid);
			if (it != Party::g_xuidToPublicAddressMap.end())
			{
				targetAddr = it->second;
			}
			else
			{
				continue;
			}*/

			auto it = Party::g_xuidToPublicAddressMap.find(memberXuid);

			if (it == Party::g_xuidToPublicAddressMap.end())
			{
				continue;
			}

			/*if (strcmp(targetAddr.getString().c_str(), "127.0.0.1") == 0)
			{
				continue;
			}*/

			for (const auto& targetAddr : it->second)
			{
				const auto targetAddressString = targetAddr.getString();
				if (targetAddressString == "127.0.0.1" || sentAddresses.contains(targetAddressString))
				{
					continue;
				}

				Network::SendCommand(targetAddr, "dvarUpdate", builtDvarString);
				sentAddresses.insert(targetAddressString);
				++totalSent;
			}

			//Network::SendCommand(targetAddr, "dvarUpdate", builtDvarString);
			//++totalSent;
		}
	}

	struct RealCharacterParticipant
	{
		std::uint64_t xuid = 0;
		std::string name;
		bool host = false;
	};

	static constexpr const char* ZW3Characters[MAX_PARTY_SLOTS]
	{
		"Richtofen",
		"Dempsey",
		"Nikolai",
		"Takeo"
	};

	static std::string NormalizePartyIdentityName(const std::string& name)
	{
		return CharacterAssignments::Normalize(name);
	}

	static std::string CanonicalCharacterName(const std::string& name)
	{
		const auto character = CharacterAssignments::Parse(name);
		return CharacterAssignments::IsValid(character)
			? CharacterAssignments::ToString(character)
			: std::string();
	}

	static bool CollectStableLobbyParticipants(
		std::vector<RealCharacterParticipant>& participants)
	{
		participants.clear();
		if (!Dvar::Var("party_host").get<bool>())
		{
			return false;
		}

		const auto hostXuid = Party::GetLocalPlayerXUID();
		if (hostXuid == 0)
		{
			return false;
		}

		const auto hostName = Dvar::Var("name").get<std::string>();
		participants.push_back({ hostXuid, hostName, true });

		std::unordered_set<std::uint64_t> seen{ hostXuid };
		if (!Game::g_lobbyData)
		{
			return true;
		}

		for (int slot = 0; slot < MAX_PARTY_SLOTS &&
			participants.size() < MAX_PARTY_SLOTS; ++slot)
		{
			const auto& member = Game::g_lobbyData->partyMembers[slot];
			if (member.status == 0 || !member.gamertag[0])
			{
				continue;
			}

			if (member.player == hostXuid)
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

			participants.push_back({ member.player, member.gamertag, false });
		}

		return true;
	}

	static std::string ChooseInitialHostCharacter()
	{
		if (!CanonicalCharacterName(s_hostCharacter).empty())
		{
			return CanonicalCharacterName(s_hostCharacter);
		}

		const auto index = static_cast<std::size_t>(
			Game::Sys_Milliseconds()) % CharacterAssignments::Characters.size();
		s_hostCharacter = CharacterAssignments::ToString(
			CharacterAssignments::Characters[index]);
		return s_hostCharacter;
	}

	static std::string BuildCharacterRosterSignature(
		const std::vector<RealCharacterParticipant>& participants,
		const int botsToAdd)
	{
		std::string signature = std::to_string(botsToAdd);
		for (const auto& participant : participants)
		{
			signature.append("|");
			signature.append(Utils::String::VA("%llX", participant.xuid));
			signature.append(":");
			signature.append(NormalizePartyIdentityName(participant.name));
		}
		return signature;
	}

	void Party::RandomizeCharactersForClients()
	{
		if (!Dvar::Var("party_host").get<bool>() ||
			s_characterRosterFrozen)
		{
			return;
		}

		std::vector<RealCharacterParticipant> participants;
		if (!CollectStableLobbyParticipants(participants) || participants.empty())
		{
			Dvar::Var("party_roster_loading").set(true);
			return;
		}

		std::unordered_set<std::uint64_t> activeXuids;
		for (const auto& participant : participants)
		{
			activeXuids.insert(participant.xuid);
		}
		CharacterAssignments::PruneRealCharacters(activeXuids);

		std::vector<std::pair<std::string, std::string>> roster;
		roster.reserve(MAX_PARTY_SLOTS);
		std::unordered_set<int> usedCharacters;

		for (const auto& participant : participants)
		{
			const auto preferred = participant.host
				? CharacterAssignments::Parse(ChooseInitialHostCharacter())
				: CharacterAssignments::Character::None;
			const auto character = CharacterAssignments::EnsureRealCharacter(
				participant.xuid, preferred);
			if (!CharacterAssignments::IsValid(character))
			{
				continue;
			}

			if (participant.host)
			{
				s_hostCharacter = CharacterAssignments::ToString(character);
			}

			usedCharacters.insert(static_cast<int>(character));
			roster.emplace_back(CharacterAssignments::ToString(character),
				participant.name);
		}

		const int realPlayers = static_cast<int>(roster.size());
		const int botsToAdd = std::clamp(Dvar::Var("addBots").get<int>(),
			0, MAX_PARTY_SLOTS - realPlayers);

		for (const auto character : CharacterAssignments::Characters)
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
			roster.emplace_back(characterName,
				Utils::String::VA("[BOT] %s", characterName.c_str()));
			usedCharacters.insert(static_cast<int>(character));
		}

		for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
		{
			if (slot < static_cast<int>(roster.size()))
			{
				SetStringDvarIfChanged(Utils::String::VA("character_%d", slot + 1),
					roster[slot].first);
				SetStringDvarIfChanged(Utils::String::VA("character_%d_player", slot + 1),
					roster[slot].second);
			}
			else
			{
				SetStringDvarIfChanged(Utils::String::VA("character_%d", slot + 1), "None");
				SetStringDvarIfChanged(Utils::String::VA("character_%d_player", slot + 1), "None");
			}
		}

		const int desiredPartySize = std::clamp(realPlayers + botsToAdd, 1, MAX_PARTY_SLOTS);
		CharacterAssignments::SetDesiredPartySize(desiredPartySize);
		Dvar::Var("party_realPlayers").set(realPlayers);
		Dvar::Var("party_currentPlayers").set(desiredPartySize);
		Dvar::Var("party_roster_loading").set(false);
	}

	std::string Party::GetPlayerName(int slot_index)
	{
		if (Game::g_lobbyData && slot_index >= 0 && slot_index < MAX_PARTY_SLOTS) {
			auto& member = Game::g_lobbyData->partyMembers[slot_index];
			if (member.status != 0) {
				return std::string(member.gamertag);
			}
		}

		return "None";
	}


	static bool HasCompletePublishedRoster()
	{
		const int publishedPartySize = std::clamp(
			Dvar::Var("party_currentPlayers").get<int>(), 0, MAX_PARTY_SLOTS);

		if (publishedPartySize < 1 ||
			Dvar::Var("party_realPlayers").get<int>() < 1)
		{
			return false;
		}

		for (int slot = 0; slot < publishedPartySize; ++slot)
		{
			const auto character = CanonicalCharacterName(
				Dvar::Var(Utils::String::VA("character_%d", slot + 1))
				.get<std::string>());
			const auto playerName = Dvar::Var(
				Utils::String::VA("character_%d_player", slot + 1))
				.get<std::string>();

			if (character.empty() || playerName.empty() ||
				NormalizePartyIdentityName(playerName) == "none")
			{
				return false;
			}
		}

		return true;
	}


	static int FindConnectedLocalRealClient()
	{
		const auto localXuid = Party::GetLocalPlayerXUID();

		if (localXuid != 0)
		{
			for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
			{
				const auto& client = Game::svs_clients[clientNum];
				if (client.header.state >= Game::CS_CONNECTED &&
					!client.bIsTestClient &&
					CharacterAssignments::GetClientXuid(client) == localXuid)
				{
					return clientNum;
				}
			}
		}

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state >= Game::CS_CONNECTED &&
				!client.bIsTestClient)
			{
				return clientNum;
			}
		}

		return -1;
	}

	static bool PublishDirectLaunchRosterFromRuntime()
	{
		const int clientNum = FindConnectedLocalRealClient();
		if (clientNum < 0)
		{
			return false;
		}

		auto character =
			CharacterAssignments::ResolveClientCharacter(clientNum);

		if (!CharacterAssignments::IsValid(character))
		{
			const auto preferred = CharacterAssignments::Parse(
				ChooseInitialHostCharacter());
			const auto xuid = CharacterAssignments::GetClientXuid(
				Game::svs_clients[clientNum]);

			if (xuid != 0)
			{
				character = CharacterAssignments::EnsureRealCharacter(
					xuid, preferred, clientNum);
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

		s_hostCharacter = CharacterAssignments::ToString(character);

		std::string hostName = Game::svs_clients[clientNum].name;
		if (hostName.empty())
		{
			hostName = Dvar::Var("name").get<std::string>();
		}
		if (hostName.empty())
		{
			hostName = "Player";
		}

		SetStringDvarIfChanged("character_1",
			CharacterAssignments::ToString(character));
		SetStringDvarIfChanged("character_1_player", hostName);

		for (int slot = 2; slot <= MAX_PARTY_SLOTS; ++slot)
		{
			SetStringDvarIfChanged(
				Utils::String::VA("character_%d", slot), "None");
			SetStringDvarIfChanged(
				Utils::String::VA("character_%d_player", slot), "None");
		}

		CharacterAssignments::SetDesiredPartySize(1);
		Dvar::Var("party_realPlayers").set(1);
		Dvar::Var("party_currentPlayers").set(1);
		Dvar::Var("party_currentHost").set(hostName);
		Dvar::Var("party_roster_loading").set(false);

		s_directLaunchRoster = true;
		s_characterRosterFrozen = true;
		return true;
	}

	static void FinalizeServerCharacterRoster(const bool authoritativePass)
	{
		if (Party::IsEnabled())
		{
			s_directLaunchRoster = false;
			s_characterRosterFrozen = true;

			if (HasCompletePublishedRoster())
			{
				CharacterAssignments::SetDesiredPartySize(
					Dvar::Var("party_currentPlayers").get<int>());
				Dvar::Var("party_roster_loading").set(false);
			}
			return;
		}

		if (!authoritativePass && HasCompletePublishedRoster() &&
			Dvar::Var("party_currentPlayers").get<int>() > 1)
		{
			s_characterRosterFrozen = true;
			return;
		}

		PublishDirectLaunchRosterFromRuntime();
	}


	struct RuntimeCharacterRosterEntry
	{
		int clientNum = -1;
		bool bot = false;
		bool host = false;
		std::uint64_t xuid = 0;
		CharacterAssignments::Character character =
			CharacterAssignments::Character::None;
		std::string name;
	};

	static int CountRuntimeRealPlayers()
	{
		std::unordered_set<std::uint64_t> xuids;
		std::unordered_set<std::string> fallbackNames;

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
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
				fallbackNames.insert(NormalizePartyIdentityName(client.name));
			}
		}

		return std::clamp(static_cast<int>(
			xuids.size() + fallbackNames.size()), 0, MAX_PARTY_SLOTS);
	}

	static bool PublishRuntimeCharacterRoster()
	{
		std::vector<RuntimeCharacterRosterEntry> entries;
		std::unordered_set<std::uint64_t> realXuids;
		const auto localXuid = Party::GetLocalPlayerXUID();
		const int runtimeRealPlayers = CountRuntimeRealPlayers();
		bool changed = false;

		if (Dvar::Var("party_realPlayers").get<int>() != runtimeRealPlayers)
		{
			Dvar::Var("party_realPlayers").set(runtimeRealPlayers);
			changed = true;
		}

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state < Game::CS_CONNECTED)
			{
				continue;
			}

			const auto xuid = CharacterAssignments::GetClientXuid(client);
			const bool bot = client.bIsTestClient && xuid == 0;

			if (!bot && xuid != 0 && !realXuids.insert(xuid).second)
			{
				continue;
			}

			auto character =
				CharacterAssignments::GetClientCharacterId(clientNum);
			if (!CharacterAssignments::IsValid(character))
			{
				character =
					CharacterAssignments::ResolveClientCharacter(clientNum);
			}
			if (!CharacterAssignments::IsValid(character))
			{
				continue;
			}

			RuntimeCharacterRosterEntry entry;
			entry.clientNum = clientNum;
			entry.bot = bot;
			entry.host = !bot && localXuid != 0 && xuid == localXuid;
			entry.xuid = xuid;
			entry.character = character;

			if (bot)
			{
				entry.name = Utils::String::VA("[BOT] %s",
					CharacterAssignments::ToString(character));
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
			return changed;
		}

		if (std::none_of(entries.begin(), entries.end(),
			[](const RuntimeCharacterRosterEntry& entry)
			{
				return entry.host;
			}))
		{
			const auto firstReal = std::find_if(entries.begin(), entries.end(),
				[](const RuntimeCharacterRosterEntry& entry)
				{
					return !entry.bot;
				});
			if (firstReal != entries.end())
			{
				firstReal->host = true;
			}
		}

		std::stable_sort(entries.begin(), entries.end(),
			[](const RuntimeCharacterRosterEntry& a,
				const RuntimeCharacterRosterEntry& b)
			{
				const int aGroup = a.host ? 0 : (a.bot ? 2 : 1);
				const int bGroup = b.host ? 0 : (b.bot ? 2 : 1);

				if (aGroup != bGroup)
				{
					return aGroup < bGroup;
				}

				if (a.bot && b.bot && a.character != b.character)
				{
					return static_cast<int>(a.character) <
						static_cast<int>(b.character);
				}

				return a.clientNum < b.clientNum;
			});

		if (entries.size() > MAX_PARTY_SLOTS)
		{
			entries.resize(MAX_PARTY_SLOTS);
		}

		std::string signature;
		for (const auto& entry : entries)
		{
			signature.append(Utils::String::VA("%d:%d:%llX:",
				entry.clientNum,
				static_cast<int>(entry.character),
				entry.xuid));
			signature.append(entry.name);
			signature.push_back('|');
		}

		static std::string candidateSignature;
		static int candidateTicks = 0;

		if (signature != candidateSignature)
		{
			candidateSignature = signature;
			candidateTicks = 1;
			return changed;
		}

		if (candidateTicks < 3)
		{
			++candidateTicks;
			return changed;
		}

		for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
		{
			if (slot < static_cast<int>(entries.size()))
			{
				const auto& entry = entries[slot];
				changed |= SetStringDvarIfChanged(
					Utils::String::VA("character_%d", slot + 1),
					CharacterAssignments::ToString(entry.character));
				changed |= SetStringDvarIfChanged(
					Utils::String::VA("character_%d_player", slot + 1),
					entry.name);
			}
			else
			{
				changed |= SetStringDvarIfChanged(
					Utils::String::VA("character_%d", slot + 1), "None");
				changed |= SetStringDvarIfChanged(
					Utils::String::VA("character_%d_player", slot + 1), "None");
			}
		}

		if (Dvar::Var("party_currentPlayers").get<int>() !=
			static_cast<int>(entries.size()))
		{
			Dvar::Var("party_currentPlayers").set(
				static_cast<int>(entries.size()));
			changed = true;
		}

		if (Dvar::Var("party_roster_loading").get<bool>())
		{
			Dvar::Var("party_roster_loading").set(false);
			changed = true;
		}

		return changed;
	}

	__declspec(naked) void PartyMigrate_HandlePacket()
	{
		__asm
		{
			mov eax, 0;
			retn;
		}
	}

	void SV_SpawnServer_Com_SyncThreads_Hook()
	{
		s_characterRosterFrozen = true;
		Game::Com_SyncThreads(); // Com_SyncThreads

		// Whenever the game starts a server,
		// RMsg_SendMessages so that everybody gets the PartyGo message!
		// (Otherwise Com_Try_Block doesn't send it until we're done loading, which times out some people)
		Game::RMesg_SendMessages();
	}

	bool Party::IsServerBrowserOpen()
	{
		auto* menu = Game::Menus_FindByName(Game::uiContext, "pc_join_unranked");
		if (!menu)
		{
			return false;
		}
		return Game::Menu_IsVisible(Game::uiContext, menu);
	}

	std::string getCharMap() {
		return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ";
	}
	std::string decryptString(const std::string& encryptedText, int shiftKey) {
		std::string map = getCharMap();
		std::string decrypted = "";
		int mapSize = static_cast<int>(map.size());

		for (size_t i = 0; i < encryptedText.size(); i++) {
			char currentChar = encryptedText[i];

			size_t index = map.find(currentChar);

			if (index != std::string::npos) {
				int newIndex = (static_cast<int>(index) - shiftKey);

				while (newIndex < 0) {
					newIndex += mapSize;
				}
				newIndex %= mapSize;

				decrypted += map[newIndex];
			}
			else {
				decrypted += currentChar;
			}
		}
		return decrypted;
	}

	Party::Party()
	{
		if (ZoneBuilder::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
			{
				ServerVersion = Dvar::Register<const char*>("sv_version", "", Game::DVAR_SERVERINFO | Game::DVAR_INIT, "Server version");
				Dvar::Register<const char*>("character_1", "None", Game::DVAR_CODINFO, "Character assigned to player 1");
				Dvar::Register<const char*>("character_2", "None", Game::DVAR_CODINFO, "Character assigned to player 2");
				Dvar::Register<const char*>("character_3", "None", Game::DVAR_CODINFO, "Character assigned to player 3");
				Dvar::Register<const char*>("character_4", "None", Game::DVAR_CODINFO, "Character assigned to player 4");
				Dvar::Register<const char*>("character_1_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 1");
				Dvar::Register<const char*>("character_2_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 2");
				Dvar::Register<const char*>("character_3_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 3");
				Dvar::Register<const char*>("character_4_player", "None", Game::DVAR_CODINFO, "Player name assigned to slot 4");
				Dvar::Register<int>("party_currentPlayers", 0, 0, 4, Game::DVAR_CODINFO, "Total current players in the party");
				Dvar::Register<int>("party_realPlayers", 0, 0, 4, Game::DVAR_CODINFO, "Current real players in the party");
				Dvar::Register<const char*>("party_currentHost", "", Game::DVAR_NONE, "Current private-party host display name");
				Dvar::Register<bool>("party_roster_loading", true, Game::DVAR_NONE, "Waiting for a complete private-party roster snapshot");
				Dvar::Register<const char*>("autosave_map", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_round", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_zombiemode", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_kills", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_score", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_time", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_date", "", Game::DVAR_INIT, "");
				Dvar::Register<const char*>("autosave_mapname_display", "", Game::DVAR_INIT, "");
				Dvar::Register<bool>("autosave_load", false, Game::DVAR_INIT, "");
			});

		PartyEnable = Dvar::Register<bool>("party_enable", Dedicated::IsEnabled(), Game::DVAR_NONE, "Enable party system");
		Dvar::Register<bool>("xblive_privatematch", true, Game::DVAR_INIT, "private match");

		static const char* zombieModeValues[] = { "Normal", "Classic", "Hardcore", nullptr };
		Game::Dvar_RegisterEnum("zombiemode", zombieModeValues, 0, Game::DVAR_CODINFO, "Change the selected zombie mode");
		Dvar::Register<int>("ui_hitmarker", 1, 0, 1, Game::DVAR_CODINFO, "Toggle hitmarkers");
		Dvar::Register<int>("ui_zombiecounter", 0, 0, 1, Game::DVAR_CODINFO, "Toggle a zombie counter");
		Dvar::Register<int>("ui_showdamage", 1, 0, 1, Game::DVAR_CODINFO, "Toggle damage visibility");
		Dvar::Register<int>("ui_perklocations", 0, 0, 1, Game::DVAR_CODINFO, "Toggle perk locations");
		Dvar::Register<int>("thirdPerson", 0, 0, 1, Game::DVAR_CODINFO, "Toggle third person");
		Dvar::Register<int>("addBots", 0, 0, 3, Game::DVAR_CODINFO, "Change the amount of bots");
		static const char* partyPrivacyValues[] = { "Open", "Invite-Only", "Closed", nullptr };
		Game::Dvar_RegisterEnum("partyPrivacy", partyPrivacyValues, 0, Game::DVAR_CODINFO, "Party privacy");

		// Kill the party migrate handler - it's not necessary and has apparently been used in the past for trickery?
		Utils::Hook(0x46AB70, PartyMigrate_HandlePacket, HOOK_JUMP).install()->quick();

		// various changes to SV_DirectConnect-y stuff to allow non-party joinees
		Utils::Hook::Set<WORD>(0x460D96, 0x90E9);
		Utils::Hook::Set<BYTE>(0x460F0A, 0xEB);
		Utils::Hook::Set<BYTE>(0x401CA4, 0xEB);
		Utils::Hook::Set<BYTE>(0x401C15, 0xEB);

		// disable configstring checksum matching (it's unreliable at most)
		Utils::Hook::Set<BYTE>(0x4A75A7, 0xEB); // SV_SpawnServer
		Utils::Hook::Set<BYTE>(0x5AC2CF, 0xEB); // CL_ParseGamestate
		Utils::Hook::Set<BYTE>(0x5AC2C3, 0xEB); // CL_ParseGamestate

		// AnonymousAddRequest
		Utils::Hook::Set<BYTE>(0x5B5E18, 0xEB);
		Utils::Hook::Set<BYTE>(0x5B5E64, 0xEB);
		Utils::Hook::Nop(0x5B5E5C, 2);

		// HandleClientHandshake
		Utils::Hook::Set<BYTE>(0x5B6EA5, 0xEB);
		Utils::Hook::Set<BYTE>(0x5B6EF3, 0xEB);
		Utils::Hook::Nop(0x5B6EEB, 2);

		// Allow local connections
		Utils::Hook::Set<BYTE>(0x4D43DA, 0xEB);

		// LobbyID mismatch
		Utils::Hook::Nop(0x4E50D6, 2);
		Utils::Hook::Set<BYTE>(0x4E50DA, 0xEB);

		// causes 'does current Steam lobby match' calls in Steam_JoinLobby to be ignored
		Utils::Hook::Set<BYTE>(0x49D007, 0xEB);

		// function checking party heartbeat timeouts, cause random issues
		Utils::Hook::Nop(0x4E532D, 5); // PartyHost_TimeoutMembers

		// Steam_JoinLobby call causes migration
		Utils::Hook::Nop(0x5AF851, 5);
		Utils::Hook::Set<BYTE>(0x5AF85B, 0xEB);

		// Allow xpartygo in public lobbies
		Utils::Hook::Set<BYTE>(0x5A969E, 0xEB);
		Utils::Hook::Nop(0x5A96BE, 2);

		// Always open lobby menu when connecting
		// It's not possible to entirely patch it via code
		//Utils::Hook::Set<BYTE>(0x5B1698, 0xEB);
		//Utils::Hook::Nop(0x5029F2, 6);
		//Utils::Hook::SetString(0x70573C, "menu_xboxlive_lobby");

		// Disallow selecting team in private match
		//Utils::Hook::Nop(0x5B2BD8, 6);

		// Force teams, even if not private match
		Utils::Hook::Set<BYTE>(0x487BB2, 0xEB);

		// Force xblive_privatematch 0 and rename it
		//Utils::Hook::Set<BYTE>(0x420A6A, 4);
		Utils::Hook::Set<BYTE>(0x420A6C, 0);
		Utils::Hook::Set<const char*>(0x420A6E, "xblive_privateserver");

		// Remove migration shutdown, it causes crashes and will be destroyed when erroring anyways
		Utils::Hook::Nop(0x5A8E1C, 12);
		Utils::Hook::Nop(0x5A8E33, 11);

		// Enable XP Bar
		Utils::Hook(0x62A2A7, UIDvarIntStub, HOOK_CALL).install()->quick();

		// Set NAT to open
		Utils::Hook::Set<int>(0x79D898, 1);

		// Disable host migration
		Utils::Hook::Set<BYTE>(0x5B58B2, 0xEB);
		Utils::Hook::Set<BYTE>(0x4D6171, 0);
		Utils::Hook::Nop(0x4077A1, 5); // PartyMigrate_Frame

		// Patch playlist stuff for non-party behavior
		static Game::dvar_t* partyEnable = PartyEnable.get<Game::dvar_t*>();
		Utils::Hook::Set<Game::dvar_t**>(0x4A4093, &partyEnable);
		Utils::Hook::Set<Game::dvar_t**>(0x4573F1, &partyEnable);
		Utils::Hook::Set<Game::dvar_t**>(0x5B1A0C, &partyEnable);

		// Invert corresponding jumps
		Utils::Hook::Xor<BYTE>(0x4A409B, 1);
		Utils::Hook::Xor<BYTE>(0x4573FA, 1);
		Utils::Hook::Xor<BYTE>(0x5B1A17, 1);

		// Set ui_maxclients to sv_maxclients
		Utils::Hook::Set<const char*>(0x42618F, "sv_maxclients");
		Utils::Hook::Set<const char*>(0x4D3756, "sv_maxclients");
		Utils::Hook::Set<const char*>(0x5E3772, "sv_maxclients");

		// Unlatch maxclient dvars
		Utils::Hook::Xor<BYTE>(0x426187, Game::DVAR_LATCH);
		Utils::Hook::Xor<BYTE>(0x4D374E, Game::DVAR_LATCH);
		Utils::Hook::Xor<DWORD>(0x5E376A, Game::DVAR_LATCH); // Corrected: was BYTE
		Utils::Hook::Xor<DWORD>(0x4261A1, Game::DVAR_LATCH);
		Utils::Hook::Xor<DWORD>(0x4D376D, Game::DVAR_LATCH);
		Utils::Hook::Xor<DWORD>(0x5E3789, Game::DVAR_LATCH);

		// Synchronize / Send network messages when the server starts so that clients can load the game with us
		// Otherwise they timeout while the host load
		Utils::Hook(0x5B34D0, SV_SpawnServer_Com_SyncThreads_Hook, HOOK_CALL).install()->quick();

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

		Command::Add("connect", [](const Command::Params* params)
			{
				if (params->size() < 2)
				{
					return;
				}

				if (Game::CL_IsCgameInitialized())
				{
					Command::Execute("disconnect", false);
					Command::Execute(Utils::String::VA("%s", params->join(0).data()), false);
				}
				else
				{
					Connect(Network::Address(params->get(1)));
				}
			});

		Command::Add("reconnect", []()
			{
				Connect(Container.target);
			});

		UIScript::Add("RefreshCharacterRoster", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				if (!Dvar::Var("party_host").get<bool>())
				{
					return;
				}

				s_directLaunchRoster = false;
				s_characterRosterFrozen = false;

				std::vector<RealCharacterParticipant> participants;
				if (!CollectStableLobbyParticipants(participants))
				{
					return;
				}

				RandomizeCharactersForClients();
				BroadcastDvarUpdate();
				Command::Execute("xupdatepartystate");
			});

		UIScript::Add("CycleSmartBots", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				if (!Dvar::Var("party_host").get<bool>())
				{
					return;
				}

				s_directLaunchRoster = false;
				s_characterRosterFrozen = false;
				std::vector<RealCharacterParticipant> participants;
				if (!CollectStableLobbyParticipants(participants))
				{
					return;
				}
				const int realPlayers = std::clamp(static_cast<int>(participants.size()), 1, MAX_PARTY_SLOTS);
				const int maxBots = std::max(0, MAX_PARTY_SLOTS - realPlayers);
				const int currentBots = std::clamp(Dvar::Var("addBots").get<int>(), 0, maxBots);
				const int nextBots = maxBots == 0 ? 0 : (currentBots + 1) % (maxBots + 1);

				Dvar::Var("addBots").set(nextBots);
				Dvar::Var("party_realPlayers").set(realPlayers);
				Dvar::Var("party_currentPlayers").set(realPlayers + nextBots);
				RandomizeCharactersForClients();
				Dvar::Var("party_roster_loading").set(false);
				BroadcastDvarUpdate();
				Command::Execute("xupdatepartystate");
			});

		UIScript::Add("JoinParty", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				auto* ip_dvar = Game::Dvar_FindVar("partyconnect_ip");
				auto* port_dvar = Game::Dvar_FindVar("partyconnect_port");

				if (ip_dvar && ip_dvar->current.string && strlen(ip_dvar->current.string) > 0 &&
					port_dvar && port_dvar->current.string && strlen(port_dvar->current.string) > 0)
				{
					std::string address = ip_dvar->current.string;
					address += ":";
					address += port_dvar->current.string;
					Party::Connect(address);
				}
			});

		UIScript::Add("LoadSave", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				auto doLoadSave = []()
					{
						std::string path = (*Game::fs_basepath)->current.string + "\\zw3\\core\\scriptdata\\autosave"s;

						std::ifstream f(path);
						if (!f.is_open())
						{
							return;
						}

						auto ensureAutosaveDvar = [](const char* name, const char* value)
							{
								auto* var = Game::Dvar_FindVar(name);
								if (!var)
								{
									var = Game::Dvar_RegisterString(name, "", Game::DVAR_INIT, "");
								}

								Game::Dvar_SetString(var, value ? value : "");
							};

						auto cleanDisplayText = [](std::string value) -> std::string
							{
								std::replace(value.begin(), value.end(), '9', ' ');
								return value;
							};

						auto formatAutosaveTime = [](const std::string& value) -> std::string
							{
								int h = 0, m = 0, s = 0;

								if (std::sscanf(value.c_str(), "%d:%d:%d", &h, &m, &s) == 3)
								{
									char buffer[16];
									std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
									return buffer;
								}

								return "00:00:00"s;
							};

						auto isRawWeaponUpgraded = [](const std::string& rawWeapon) -> bool
							{
								return rawWeapon.find("_upgraded_mp") != std::string::npos || rawWeapon.find("_upgrade_mp") != std::string::npos;
							};

						auto hasUpgradeAvailable = [](const std::string& rawWeapon) -> bool
							{
								static const std::unordered_set<std::string> upgradeableWeapons =
								{
									"t7_raygun_mp", "t7_rgmk2_mp", "thundergun_mp", "apothicon_mp",
									"acidgat_mp", "blundergat_mp", "t5_spectre_mp", "t5_galil_mp",
									"t5_mp5k_mp", "t5_ak74u_mp", "t5_python_mp", "t5_1911_mp",
									"iw5_mp7_mp", "codo_ak104ss_mp", "codo_mg4ss_mp", "codo_mg4ss_alt_mp",
									"jetgun_mp", "t7_dg2_mp", "t5_rottweil72_mp", "t5_mp40_mp",
									"t4_bar_mp", "t5_commando_mp", "t4_thompson_mp", "bow_mp",
									"paralyzer_mp", "raymachine_mp"
								};

								return upgradeableWeapons.find(rawWeapon) != upgradeableWeapons.end();
							};

						auto getUpgradeStatus = [&](const std::string& rawWeapon, const std::string& upgradeValue) -> std::string
							{
								if (isRawWeaponUpgraded(rawWeapon))
								{
									return "^2Yes"s;
								}

								if (!upgradeValue.empty() && upgradeValue != "none" && upgradeValue != "0")
								{
									return "^2Yes"s;
								}

								if (hasUpgradeAvailable(rawWeapon))
								{
									return "^3No"s;
								}

								return "^1Not Upgradable"s;
							};

						auto getUpgradeEffect = [](const std::string& value) -> std::string
							{
								if (value.empty() || value == "none" || value == "0")
								{
									return "None"s;
								}

								if (value == "flame")
								{
									return "^1Flame"s;
								}

								if (value == "turned")
								{
									return "^2Turned"s;
								}

								if (value == "lightning")
								{
									return "^5Lightning"s;
								}

								return value;
							};

						auto cleanWeaponName = [](std::string value) -> std::string
							{
								if (value.empty() || value == "none")
								{
									return "None"s;
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
									{ "stinger_mp", "Stinger" }
								};

								const auto found = weaponNames.find(value);
								if (found != weaponNames.end())
								{
									return found->second;
								}

								static const std::unordered_map<std::string, std::string> baseWeaponNames =
								{
									{ "mp5k", "MP5K" }, { "uzi", "Mini-Uzi" }, { "p90", "P90" }, { "kriss", "Vector" }, { "ump45", "UMP45" },
									{ "striker", "Striker" }, { "aa12", "AA-12" }, { "m1014", "M1014" }, { "spas12", "SPAS-12" }, { "ranger", "Ranger" }, { "model1887", "Model 1887" },
									{ "ak47", "AK-47" }, { "m16", "M16A4" }, { "m4", "M4A1" }, { "fn2000", "F2000" }, { "masada", "ACR" }, { "famas", "FAMAS" }, { "fal", "FAL" }, { "scar", "SCAR-H" }, { "tavor", "TAR-21" }, { "peacekeeper", "Peacekeeper" },
									{ "aug", "AUG HBAR" }, { "rpd", "RPD" }, { "sa80", "L86 LSW" }, { "mg4", "MG4" }, { "m240", "M240" },
									{ "cheytac", "Intervention" }, { "m21", "M21 EBR" }, { "wa2000", "WA2000" }, { "barrett", "Barrett .50cal" }, { "dragunov", "Dragunov" }, { "m40a3", "M40A3" },
									{ "usp", "USP .45" }, { "deserteagle", "Desert Eagle" }, { "coltanaconda", "Colt Anaconda" }, { "beretta", "M9" },
									{ "rpg", "RPG-7" }, { "javelin", "Javelin" }, { "at4", "AT4" }, { "m79", "M79" },
									{ "glock", "G18" }, { "tmp", "TMP" }, { "beretta393", "M93 Raffica" }, { "pp2000", "PP2000" }
								};

								static const std::unordered_map<std::string, std::string> attachmentNames =
								{
									{ "acog", "ACOG" }, { "reflex", "Reflex" }, { "eotech", "Holographic" },
									{ "fmj", "FMJ" }, { "xmags", "Extended Mags" }, { "silencer", "Silencer" },
									{ "akimbo", "Akimbo" }, { "rof", "Rapid Fire" }, { "grip", "Grip" },
									{ "gl", "Grenade Launcher" }, { "shotgun", "Shotgun" }, { "heartbeat", "Heartbeat" },
									{ "thermal", "Thermal" }, { "tactical", "Tactical Knife" }
								};

								auto baseNameOnly = value;
								if (baseNameOnly.size() > 3 && baseNameOnly.substr(baseNameOnly.size() - 3) == "_mp")
								{
									baseNameOnly = baseNameOnly.substr(0, baseNameOnly.size() - 3);
								}

								std::vector<std::string> parts;
								std::string current;

								for (const auto ch : baseNameOnly)
								{
									if (ch == '_')
									{
										if (!current.empty())
										{
											parts.push_back(current);
											current.clear();
										}
									}
									else
									{
										current.push_back(ch);
									}
								}

								if (!current.empty())
								{
									parts.push_back(current);
								}

								if (parts.empty())
								{
									return value;
								}

								const auto baseIt = baseWeaponNames.find(parts[0]);
								std::string displayName = baseIt != baseWeaponNames.end() ? baseIt->second : parts[0];

								std::string attachments;
								for (size_t i = 1; i < parts.size(); ++i)
								{
									const auto attIt = attachmentNames.find(parts[i]);
									if (attIt == attachmentNames.end())
									{
										continue;
									}

									if (!attachments.empty())
									{
										attachments += ", ";
									}

									attachments += attIt->second;
								}

								if (!attachments.empty())
								{
									return displayName + " (" + attachments + ")";
								}

								return displayName;
							};

						struct _stat64 st {};
						if (_stat64(path.c_str(), &st) == 0)
						{
							tm t{};
							localtime_s(&t, &st.st_mtime);

							char formatted[128];
							strftime(formatted, sizeof(formatted), "%d %b %Y  %H:%M", &t);
							ensureAutosaveDvar("autosave_date", formatted);
						}
						else
						{
							ensureAutosaveDvar("autosave_date", "Unknown date");
						}

						std::string fdata((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
						f.close();

						const int key = 16;
						const std::string data = decryptString(fdata, key);

						std::unordered_map<std::string, std::string> parsed;
						size_t start = 0;

						while (true)
						{
							const size_t s = data.find(';', start);
							if (s == std::string::npos) break;

							std::string item = data.substr(start, s - start);
							start = s + 1;

							const size_t c = item.find(':');
							if (c == std::string::npos) continue;

							std::string k = item.substr(0, c);
							std::string v = item.substr(c + 1);

							auto trim = [](std::string& x)
								{
									while (!x.empty() && std::strchr(" \n\r	", x.back())) x.pop_back();
									while (!x.empty() && std::strchr(" \n\r	", x.front())) x.erase(x.begin());
								};

							trim(k);
							trim(v);

							if (!k.empty())
							{
								parsed[k] = v;
							}
						}

						if (!parsed.count("map"))
						{
							return;
						}

						auto getParsed = [&](const char* keyName, const char* fallback = "") -> std::string
							{
								const auto it = parsed.find(keyName);
								if (it != parsed.end() && !it->second.empty())
								{
									return it->second;
								}

								return fallback;
							};

						const auto roundDisplay = getParsed("round", "1");
						const auto killsDisplay = getParsed("kills", "0");
						const auto scoreDisplay = getParsed("score", "0");
						const auto downsDisplay = getParsed("downs", "0");
						const auto revivesDisplay = getParsed("revives", "0");
						const auto exfiltratedDisplay = getParsed("exfiltrated", "0");
						const auto zombieModeDisplay = cleanDisplayText(getParsed("zombiemode", "Normal"));
						const auto timeDisplay = formatAutosaveTime(getParsed("time", "00:00:00"));

						ensureAutosaveDvar("autosave_round", roundDisplay.c_str());
						ensureAutosaveDvar("autosave_kills", killsDisplay.c_str());
						ensureAutosaveDvar("autosave_score", scoreDisplay.c_str());
						ensureAutosaveDvar("autosave_downs", downsDisplay.c_str());
						ensureAutosaveDvar("autosave_revives", revivesDisplay.c_str());
						ensureAutosaveDvar("autosave_exfiltrated", exfiltratedDisplay.c_str());
						ensureAutosaveDvar("autosave_zombiemode", zombieModeDisplay.c_str());
						ensureAutosaveDvar("autosave_time", timeDisplay.c_str());

						const auto primaryRaw = getParsed("weapon(0)", "None");
						const auto secondaryRaw = getParsed("weapon(1)", "None");

						const auto primaryUpgradeRaw = getParsed("upgrade(0)", "none");
						const auto secondaryUpgradeRaw = getParsed("upgrade(1)", "none");

						const auto primaryWeaponDisplay = cleanWeaponName(primaryRaw);
						const auto secondaryWeaponDisplay = cleanWeaponName(secondaryRaw);

						const auto primaryUpgradeDisplay = getUpgradeStatus(primaryRaw, primaryUpgradeRaw);
						const auto secondaryUpgradeDisplay = getUpgradeStatus(secondaryRaw, secondaryUpgradeRaw);

						const auto primaryEffectDisplay = getUpgradeEffect(primaryUpgradeRaw);
						const auto secondaryEffectDisplay = getUpgradeEffect(secondaryUpgradeRaw);

						const auto primaryClipDisplay = getParsed("clip(0)", "0");
						const auto primaryStockDisplay = getParsed("stock(0)", "0");
						const auto secondaryClipDisplay = getParsed("clip(1)", "0");
						const auto secondaryStockDisplay = getParsed("stock(1)", "0");

						ensureAutosaveDvar("autosave_primary_weapon", primaryWeaponDisplay.c_str());
						ensureAutosaveDvar("autosave_primary_clip", primaryClipDisplay.c_str());
						ensureAutosaveDvar("autosave_primary_stock", primaryStockDisplay.c_str());
						ensureAutosaveDvar("autosave_primary_upgrade", primaryUpgradeDisplay.c_str());
						ensureAutosaveDvar("autosave_primary_effect", primaryEffectDisplay.c_str());

						ensureAutosaveDvar("autosave_secondary_weapon", secondaryWeaponDisplay.c_str());
						ensureAutosaveDvar("autosave_secondary_clip", secondaryClipDisplay.c_str());
						ensureAutosaveDvar("autosave_secondary_stock", secondaryStockDisplay.c_str());
						ensureAutosaveDvar("autosave_secondary_upgrade", secondaryUpgradeDisplay.c_str());
						ensureAutosaveDvar("autosave_secondary_effect", secondaryEffectDisplay.c_str());

						const auto quickReviveDisplay = getParsed("perks('Quick Revive')", "0");
						const auto juggernogDisplay = getParsed("perks('Juggernog')", "0");
						const auto speedColaDisplay = getParsed("perks('Speed Cola')", "0");
						const auto doubleTapDisplay = getParsed("perks('Double Tap')", "0");
						const auto staminUpDisplay = getParsed("perks('Stamin Up')", "0");
						const auto deadshotDisplay = getParsed("perks('Deadshot Daiquiri')", "0");
						const auto electricCherryDisplay = getParsed("perks('Electric Cherry')", "0");

						ensureAutosaveDvar("autosave_has_quickrevive", quickReviveDisplay.c_str());
						ensureAutosaveDvar("autosave_has_juggernog", juggernogDisplay.c_str());
						ensureAutosaveDvar("autosave_has_speedcola", speedColaDisplay.c_str());
						ensureAutosaveDvar("autosave_has_doubletap", doubleTapDisplay.c_str());
						ensureAutosaveDvar("autosave_has_staminup", staminUpDisplay.c_str());
						ensureAutosaveDvar("autosave_has_deadshot", deadshotDisplay.c_str());
						ensureAutosaveDvar("autosave_has_electriccherry", electricCherryDisplay.c_str());

						const auto rawMap = getParsed("map", "");
						const char* displayName = Game::UI_GetMapDisplayName(rawMap.c_str());

						if (!displayName || !displayName[0] || std::strcmp(displayName, rawMap.c_str()) == 0)
						{
							displayName = Localization::LocalizeMapName(rawMap.c_str());
						}

						const std::string mapDisplayName = displayName ? displayName : rawMap;
						ensureAutosaveDvar("autosave_map", rawMap.c_str());
						ensureAutosaveDvar("autosave_mapname_display", mapDisplayName.c_str());

						Command::Execute("openmenu popup_autosave");
					};

				auto* partyHostVarNow = Game::Dvar_FindVar("party_host");
				if (partyHostVarNow && Dvar::Var("party_host").get<bool>())
				{
					doLoadSave();
				}
				else
				{
					const auto startMs = Game::Sys_Milliseconds();

					Scheduler::Schedule([startMs, doLoadSave]() mutable -> bool
						{
							auto* ph = Game::Dvar_FindVar("party_host");
							if (ph && Dvar::Var("party_host").get<bool>())
							{
								doLoadSave();
								return true;
							}

							if ((Game::Sys_Milliseconds() - startMs) > 2000)
							{
								return true;
							}

							return false;
						}, Scheduler::Pipeline::MAIN, 50ms);
				}
			});

		UIScript::Add("LoadSaveAccepted", [](const UIScript::Token&, const Game::uiInfo_s*)
			{
				if (s_autosaveMapLaunchPending)
				{
					return;
				}

				auto* map = Game::Dvar_FindVar("autosave_map");
				if (!map || !map->current.string || !map->current.string[0])
				{
					return;
				}

				const std::string mapName = map->current.string;
				const bool validMapName = mapName.size() <= 64 &&
					std::all_of(mapName.begin(), mapName.end(), [](const unsigned char c)
						{
							return std::isalnum(c) || c == '_' || c == '-';
						});

				if (!validMapName)
				{
					return;
				}

				s_autosaveMapLaunchPending = true;

				Dvar::Var("autosave_load").set(true);

				Command::Execute("closemenu popup_autosave", false);
				Scheduler::Once([mapName]
					{
						if (!s_autosaveMapLaunchPending)
						{
							return;
						}

						const std::string command = "map "s + mapName;
						Command::Execute(command.c_str(), false);
						s_autosaveMapLaunchPending = false;
					}, Scheduler::Pipeline::MAIN, 100ms);
			});

		if (!Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled())
		{
			Scheduler::Loop([]
				{
					if (Container.valid)
					{
						if ((Game::Sys_Milliseconds() - Container.joinTime) > 10'000)
						{
							Container.valid = false;
							ConnectError("Server connection timed out.");
						}
					}

					if (Container.awaitingPlaylist)
					{
						if ((Game::Sys_Milliseconds() - Container.requestTime) > 5'000)
						{
							Container.awaitingPlaylist = false;
							ConnectError("Playlist request timed out.");
						}
					}
				}, Scheduler::Pipeline::CLIENT);
		}

		// Basic info handler
		Network::OnClientPacket("getInfo", [](const Network::Address& address, [[maybe_unused]] const std::string& data)
			{
				Utils::InfoString clientInfo(data);

				std::string receivedChallenge;

				if (!Dedicated::IsEnabled() && !Dedicated::IsRunning())
				{
					receivedChallenge = clientInfo.get("challenge");
					uint64_t clientXuid = 0;
					const auto hasClientXuid = TryParseHexXuid(clientInfo.get("xuid"), clientXuid);

					if (hasClientXuid && clientXuid != 0)
					{
						//Party::g_xuidToPublicAddressMap[clientXuid] = address;

						Party::TrackClientAddress(clientXuid, address);

						Game::netIP_t clientIpUnion = address.getIP();
						struct in_addr temp_addr;
						temp_addr.s_addr = clientIpUnion.full;
					}
				}
				else
				{
					receivedChallenge = Utils::ParseChallenge(data);
				}

				Utils::InfoString hostResponseInfo;

				hostResponseInfo.set("challenge", receivedChallenge);
				hostResponseInfo.set("gamename", "IW4");
				hostResponseInfo.set("hostname", (*Game::sv_hostname)->current.string);
				hostResponseInfo.set("gametype", (*Game::sv_gametype)->current.string);
				hostResponseInfo.set("fs_game", (*Game::fs_gameDirVar)->current.string);
				hostResponseInfo.set("xuid", Utils::String::VA("%llX", GetLocalPlayerXUID()));

				auto botCount = 0;
				auto effectiveClientCount = 0;
				auto maxClientCount = *Game::svs_clientCount;
				const auto securityLevel = Dvar::Var("sv_securityLevel").get<int>();
				[[maybe_unused]] const auto* password = *Game::g_password ? (*Game::g_password)->current.string : "";
				if (maxClientCount)
				{
					for (int i = 0; i < maxClientCount; ++i)
					{
						if (Game::svs_clients[i].header.state < Game::CS_ACTIVE) continue;
						if (!Game::svs_clients[i].gentity || !Game::svs_clients[i].gentity->client) continue;

						const auto* client = Game::svs_clients[i].gentity->client;
						const auto team = client->sess.cs.team;

						if (Game::svs_clients[i].bIsTestClient || team == Game::TEAM_SPECTATOR)
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
					maxClientCount = *Game::party_maxplayers ? (*Game::party_maxplayers)->current.integer : 18;
					effectiveClientCount = Game::PartyHost_CountMembers(Game::g_lobbyData);
				}

				hostResponseInfo.set("clients", std::to_string(effectiveClientCount));
				hostResponseInfo.set("bots", std::to_string(botCount));
				hostResponseInfo.set("sv_maxclients", std::to_string(maxClientCount));
				hostResponseInfo.set("protocol", std::to_string(PROTOCOL));
				hostResponseInfo.set("version", REVISION_STR);
				hostResponseInfo.set("checksum", std::to_string(Game::Sys_Milliseconds()));
				hostResponseInfo.set("mapname", Dvar::Var("mapname").get<std::string>());
				if (Container.matchType == JoinContainer::MatchType::DEDICATED_MATCH)
				{
					hostResponseInfo.set("isPrivate", *Game::g_password ? "1"s : "0"s);
				}
				hostResponseInfo.set("hc", (Dvar::Var("g_hardcore").get<bool>() ? "1"s : "0"s));
				hostResponseInfo.set("securityLevel", std::to_string(securityLevel));
				hostResponseInfo.set("sv_running", (Dedicated::IsRunning() ? "1"s : "0"s));
				hostResponseInfo.set("aimAssist", (Gamepad::sv_allowAimAssist.get<bool>() ? "1"s : "0"s));
				hostResponseInfo.set("voiceChat", (Voice::SV_VoiceEnabled() ? "1"s : "0"s));
				hostResponseInfo.set("zombiemode", std::to_string(Dvar::Var("zombiemode").get<int>()));
				hostResponseInfo.set("ui_zombiecounter", std::to_string(Dvar::Var("ui_zombiecounter").get<int>()));
				hostResponseInfo.set("ui_hitmarker", std::to_string(Dvar::Var("ui_hitmarker").get<int>()));
				hostResponseInfo.set("ui_showdamage", std::to_string(Dvar::Var("ui_showdamage").get<int>()));
				hostResponseInfo.set("ui_perklocations", std::to_string(Dvar::Var("ui_perklocations").get<int>()));
				hostResponseInfo.set("thirdPerson", std::to_string(Dvar::Var("thirdPerson").get<int>()));
				hostResponseInfo.set("addBots", std::to_string(Dvar::Var("addBots").get<int>()));
				hostResponseInfo.set("partyPrivacy", std::to_string(Dvar::Var("partyPrivacy").get<int>()));
				auto currentHostName = Dvar::Var("party_currentHost").get<std::string>();
				if (currentHostName.empty())
				{
					currentHostName = Dvar::Var("name").get<std::string>();
				}
				hostResponseInfo.set("party_currentHost", currentHostName);
				hostResponseInfo.set("party_currentPlayers", std::to_string(Dvar::Var("party_currentPlayers").get<int>()));
				hostResponseInfo.set("party_realPlayers", std::to_string(Dvar::Var("party_realPlayers").get<int>()));
				hostResponseInfo.set("character_1", Dvar::Var("character_1").get<std::string>());
				hostResponseInfo.set("character_2", Dvar::Var("character_2").get<std::string>());
				hostResponseInfo.set("character_3", Dvar::Var("character_3").get<std::string>());
				hostResponseInfo.set("character_4", Dvar::Var("character_4").get<std::string>());
				hostResponseInfo.set("character_1_player", Dvar::Var("character_1_player").get<std::string>());
				hostResponseInfo.set("character_2_player", Dvar::Var("character_2_player").get<std::string>());
				hostResponseInfo.set("character_3_player", Dvar::Var("character_3_player").get<std::string>());
				hostResponseInfo.set("character_4_player", Dvar::Var("character_4_player").get<std::string>());

				if (hostResponseInfo.get("mapname").empty() || IsInLobby())
				{
					hostResponseInfo.set("mapname", Dvar::Var("ui_mapname").get<const char*>());
				}
				if (Maps::GetUserMap()->isValid())
				{
					hostResponseInfo.set("usermaphash", Utils::String::VA("%i", Maps::GetUserMap()->getHash()));
				}
				else if (IsInUserMapLobby())
				{
					hostResponseInfo.set("usermaphash", Utils::String::VA("%i", Maps::GetUsermapHash(hostResponseInfo.get("mapname"))));
				}
				if (Dedicated::IsEnabled())
				{
					hostResponseInfo.set("sv_motd", Dedicated::SVMOTD.get<std::string>());
				}
				bool partyHost = Dvar::Var("party_host").get<bool>();
				if (partyHost)
				{
					if (PartyEnable.get<bool>())
					{
						hostResponseInfo.set("matchtype", std::to_string(JoinContainer::MatchType::PARTY_LOBBY));
					}
					else
					{
						hostResponseInfo.set("matchtype", std::to_string(JoinContainer::MatchType::PRIVATE_PARTY));
					}
				}
				else if (Dvar::Var("sv_running").get<bool>())
				{
					hostResponseInfo.set("matchtype", std::to_string(JoinContainer::MatchType::DEDICATED_MATCH));
				}
				else
				{
					hostResponseInfo.set("matchtype", std::to_string(JoinContainer::MatchType::NO_MATCH));
				}

				hostResponseInfo.set("wwwDownload", (Download::SV_wwwDownload.get<bool>() ? "1" : "0"));
				hostResponseInfo.set("wwwUrl", Download::SV_wwwBaseUrl.get<std::string>());

				Network::SendCommand(address, "infoResponse", hostResponseInfo.build());
			});

		Network::OnClientPacket("infoResponse", [](const Network::Address& address, [[maybe_unused]] const std::string& data)
			{
				const Utils::InfoString info(data);

				// Handle connection
				if (Container.valid)
				{
					if (Container.target == address)
					{
						Container.valid = false;
						Container.info = info;

						Container.matchType = static_cast<JoinContainer::MatchType>(std::strtol(info.get("matchtype").data(), nullptr, 10));
						if (!Dedicated::IsEnabled() && !Dedicated::IsRunning() && Container.matchType == JoinContainer::MatchType::PRIVATE_PARTY)
						{
							std::string party_privacy = info.get("partyPrivacy");
							std::string client_count = info.get("clients");
							unsigned int clientCount = 0;
							if (party_privacy == "2" || party_privacy == "Closed")
							{
								ConnectError("The lobby you are trying to join is closed.");
								return;
							}
							else if (!TryParseClientCount(client_count, clientCount))
							{
								ConnectError("Invalid server info.");
								return;
							}
							else if (clientCount >= MAX_PARTY_SLOTS)
							{
								ConnectError("The lobby you are trying to join is full.");
								return;
							}
							else
							{
								Container.info.set("isPrivate", "0"s);
								PlaylistContinue();
							}
						}

						uint64_t hostXuid = 0;
						if (!TryParseHexXuid(info.get("xuid"), hostXuid) || hostXuid == 0)
						{
							ConnectError("Invalid server info.");
							return;
						}

						Dvar::Var("zombiemode").set(static_cast<int>(std::strtol(info.get("zombiemode").data(), nullptr, 10)));
						Dvar::Var("ui_zombiecounter").set(static_cast<int>(std::strtol(info.get("ui_zombiecounter").data(), nullptr, 10)));
						Dvar::Var("ui_hitmarker").set(static_cast<int>(std::strtol(info.get("ui_hitmarker").data(), nullptr, 10)));
						Dvar::Var("ui_showdamage").set(static_cast<int>(std::strtol(info.get("ui_showdamage").data(), nullptr, 10)));
						Dvar::Var("ui_perklocations").set(static_cast<int>(std::strtol(info.get("ui_perklocations").data(), nullptr, 10)));
						Dvar::Var("thirdPerson").set(static_cast<int>(std::strtol(info.get("thirdPerson").data(), nullptr, 10)));
						Dvar::Var("addBots").set(static_cast<int>(std::strtol(info.get("addBots").data(), nullptr, 10)));
						Dvar::Var("partyPrivacy").set(static_cast<int>(std::strtol(info.get("partyPrivacy").data(), nullptr, 10)));

						const auto currentPlayersValue = info.get("party_currentPlayers");
						const auto realPlayersValue = info.get("party_realPlayers");
						if (!currentPlayersValue.empty())
						{
							Dvar::Var("party_currentPlayers").set(static_cast<int>(std::strtol(currentPlayersValue.data(), nullptr, 10)));
						}
						if (!realPlayersValue.empty())
						{
							Dvar::Var("party_realPlayers").set(static_cast<int>(std::strtol(realPlayersValue.data(), nullptr, 10)));
						}

						auto receivedHostName = info.get("party_currentHost");
						if (receivedHostName.empty())
						{
							receivedHostName = info.get("hostname");
						}
						if (!receivedHostName.empty())
						{
							SetStringDvarIfChanged("party_currentHost", receivedHostName);
						}

						ApplyCharacterRosterSnapshot(info);

						auto securityLevel = std::strtoul(info.get("securityLevel").data(), nullptr, 10);
						bool isUsermap = !info.get("usermaphash").empty();
						auto usermapHash = std::strtoul(info.get("usermaphash").data(), nullptr, 10);
#ifdef CL_MOD_LOADING
						std::string mod = (*Game::fs_gameDirVar)->current.string;
#endif
						if (info.get("wwwDownload") == "1"s)
						{
							Download::SV_wwwDownload.set(true);
							Download::SV_wwwBaseUrl.set(info.get("wwwUrl"));
						}
						else
						{
							Download::SV_wwwDownload.set(false);
							Download::SV_wwwBaseUrl.set("");
						}

						std::string receivedChallenge;
						if (Container.matchType == JoinContainer::MatchType::DEDICATED_MATCH
							|| Container.matchType == JoinContainer::MatchType::PARTY_LOBBY)
						{
							receivedChallenge = Container.challenge;
						}
						else if (Container.matchType == JoinContainer::MatchType::PRIVATE_PARTY)
						{
							receivedChallenge = info.get("challenge");
						}

						if (receivedChallenge != Container.challenge)
						{
							ConnectError("Invalid join response: Challenge mismatch.");
						}
						else if (securityLevel > Auth::GetSecurityLevel())
						{
							Command::Execute("closemenu popup_reconnectingtoparty");
							Auth::IncreaseSecurityLevel(securityLevel, "reconnect");
						}
						else if (Container.matchType == JoinContainer::MatchType::NO_MATCH)
						{
							ConnectError("Server is not hosting a match.");
						}
						else if (Container.matchType >= JoinContainer::MatchType::COUNT || Container.matchType < JoinContainer::MatchType::NO_MATCH)
						{
							ConnectError("Invalid join response: Unknown matchtype");
						}
						else if (Container.info.get("mapname").empty() || Container.info.get("gametype").empty())
						{
							ConnectError("Invalid map or gametype.");
						}
						else if (Container.info.get("isPrivate") == "1"s && Dvar::Var("password").get<std::string>().empty())
						{
							ConnectError("A password is required to join this server! Set it at the bottom of the serverlist.");
						}
						else if (isUsermap && usermapHash != Maps::GetUsermapHash(info.get("mapname")))
						{
							Command::Execute("closemenu popup_reconnectingtoparty");
							Download::InitiateMapDownload(info.get("mapname"), info.get("isPrivate") == "1");
						}
#ifdef CL_MOD_LOADING
						else if (!info.get("fs_game").empty() && Utils::String::ToLower(mod) != Utils::String::ToLower(info.get("fs_game")))
						{
							Command::Execute("closemenu popup_reconnectingtoparty");
							Download::InitiateClientDownload(info.get("fs_game"), info.get("isPrivate") == "1"s, false, Container.downloadOnly);
						}
						else if ((*Game::fs_gameDirVar)->current.string[0] != '\0' && info.get("fs_game").empty())
						{
							Game::Dvar_SetString(*Game::fs_gameDirVar, "");

							if (ModList::cl_modVidRestart.get<bool>())
							{
								Command::Execute("vid_restart", false);
							}

							Command::Execute("reconnect", false);
						}
#endif
						else
						{
							if (!Maps::CheckMapInstalled(Container.info.get("mapname"), true)) return;

							Container.motd = TextRenderer::StripMaterialTextIcons(info.get("sv_motd"));

							switch (Container.matchType)
							{
							case JoinContainer::MatchType::DEDICATED_MATCH:
							{
								int clients;
								int maxClients;
								std::string version;

								try
								{
									clients = std::stoi(Container.info.get("clients"));
									maxClients = std::stoi(Container.info.get("sv_maxclients"));
									version = Container.info.get("version");
								}
								catch ([[maybe_unused]] const std::exception& ex)
								{
									ConnectError("Invalid info string");
									return;
								}

								if (clients >= maxClients)
								{
									ConnectError("@EXE_SERVERISFULL");
								}
								else
								{
									Dvar::Var("xblive_privateserver").set(true);
									ServerVersion.set(version);
									Game::Menus_CloseAll(Game::uiContext);

									Game::_XSESSION_INFO hostInfo;
									Game::CL_ConnectFromParty(0, &hostInfo, *Container.target.get(), 0, 0, Container.info.get("mapname").data(), Container.info.get("gametype").data());
								}
							}
							break;

							case JoinContainer::MatchType::PARTY_LOBBY:
							{
								Container.requestTime = Game::Sys_Milliseconds();
								Container.awaitingPlaylist = true;
								Network::SendCommand(Container.target, "getplaylist", Dvar::Var("password").get<std::string>());

								if (Game::CL_IsCgameInitialized())
								{
									Command::Execute("disconnect", true);
								}
							}
							break;

							case JoinContainer::MatchType::PRIVATE_PARTY:
							{
								PlaylistContinue();
							}
							break;
							}
						}
					}
				}

				ServerList::Insert(address, info);
				Friends::UpdateServer(address, info.get("hostname"), info.get("mapname"));
			});

		Network::OnClientPacket("dvarUpdate", [](const Network::Address& /*address*/, const std::string& data)
			{
				Utils::InfoString info(data);

				auto parseAndSetDvar = [&](const std::string& dvarName, const std::string& infoKey) -> bool
					{
						int oldValue = Dvar::Var(dvarName).get<int>();
						int newValue = oldValue;
						const std::string& receivedValueStr = info.get(infoKey);
						char* endptr;
						long convertedValue = std::strtol(receivedValueStr.c_str(), &endptr, 10);
						if (receivedValueStr.c_str() != endptr)
						{
							newValue = static_cast<int>(convertedValue);
						}
						Dvar::Var(dvarName).set(newValue);
						int currentValue = Dvar::Var(dvarName).get<int>();
						if (currentValue != newValue) {
							return false;
						}
						return true;
					};

				bool allDvarsSuccessfullySet = true;

				allDvarsSuccessfullySet &= parseAndSetDvar("zombiemode", "zombiemode");
				allDvarsSuccessfullySet &= parseAndSetDvar("ui_hitmarker", "ui_hitmarker");
				allDvarsSuccessfullySet &= parseAndSetDvar("ui_showdamage", "ui_showdamage");
				allDvarsSuccessfullySet &= parseAndSetDvar("ui_zombiecounter", "ui_zombiecounter");
				allDvarsSuccessfullySet &= parseAndSetDvar("ui_perklocations", "ui_perklocations");
				allDvarsSuccessfullySet &= parseAndSetDvar("thirdPerson", "thirdPerson");
				allDvarsSuccessfullySet &= parseAndSetDvar("addBots", "addBots");
				allDvarsSuccessfullySet &= parseAndSetDvar("partyPrivacy", "partyPrivacy");
				const auto currentPlayersValue = info.get("party_currentPlayers");
				const auto realPlayersValue = info.get("party_realPlayers");
				if (!currentPlayersValue.empty())
				{
					Dvar::Var("party_currentPlayers").set(static_cast<int>(std::strtol(currentPlayersValue.data(), nullptr, 10)));
				}
				if (!realPlayersValue.empty())
				{
					Dvar::Var("party_realPlayers").set(static_cast<int>(std::strtol(realPlayersValue.data(), nullptr, 10)));
				}

				const auto receivedHostName = info.get("party_currentHost");
				if (!receivedHostName.empty())
				{
					SetStringDvarIfChanged("party_currentHost", receivedHostName);
				}

				ApplyCharacterRosterSnapshot(info);
			});

		if (!Dedicated::IsEnabled())
		{
			static int s_lastDvarValues[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

			Scheduler::Loop([]()
				{
					bool needsBroadcast = false;
					static bool needsUpdatePartystate = false;
					static bool s_wasHostingLastFrame = false;
					static int s_lastRealPlayers = 0;
					static int s_lastBotsToAdd = 0;

					int currentDvarValues[8] = {
						Dvar::Var("zombiemode").get<int>(),
						Dvar::Var("ui_hitmarker").get<int>(),
						Dvar::Var("ui_showdamage").get<int>(),
						Dvar::Var("ui_zombiecounter").get<int>(),
						Dvar::Var("ui_perklocations").get<int>(),
						Dvar::Var("thirdPerson").get<int>(),
						Dvar::Var("addBots").get<int>(),
						Dvar::Var("partyPrivacy").get<int>()
					};

					for (int i = 0; i < 8; i++)
					{
						if (currentDvarValues[i] != s_lastDvarValues[i])
						{
							s_lastDvarValues[i] = currentDvarValues[i];
							needsBroadcast = true;
						}
					}

					bool isCurrentlyHosting = Dvar::Var("party_host").get<bool>();
					const bool startedHosting = isCurrentlyHosting && !s_wasHostingLastFrame;

					if (startedHosting && !s_directLaunchRoster)
					{
						s_characterRosterFrozen = false;
						CharacterAssignments::ResetAll();
						s_hostCharacter.clear();
						Dvar::Var("party_roster_loading").set(true);
					}

					if (isCurrentlyHosting)
					{
						if (!s_characterRosterFrozen && !s_directLaunchRoster)
						{
							std::vector<RealCharacterParticipant> participants;
							if (CollectStableLobbyParticipants(participants))
							{
								const int realPlayers = std::clamp(
									static_cast<int>(participants.size()), 1, MAX_PARTY_SLOTS);
								const int botsToAdd = std::clamp(
									Dvar::Var("addBots").get<int>(), 0,
									MAX_PARTY_SLOTS - realPlayers);
								const auto rosterSignature =
									BuildCharacterRosterSignature(participants, botsToAdd);

								static std::string s_lastCharacterRosterSignature;
								static std::string s_candidateCharacterRosterSignature;
								static int s_candidateCharacterRosterTicks = 0;

								if (rosterSignature != s_candidateCharacterRosterSignature)
								{
									s_candidateCharacterRosterSignature = rosterSignature;
									s_candidateCharacterRosterTicks = 1;
								}
								else if (s_candidateCharacterRosterTicks < 3)
								{
									++s_candidateCharacterRosterTicks;
								}

								const bool initialRoster =
									Dvar::Var("party_roster_loading").get<bool>();
								if ((initialRoster || s_candidateCharacterRosterTicks >= 3) &&
									(initialRoster || rosterSignature != s_lastCharacterRosterSignature))
								{
									RandomizeCharactersForClients();
									s_lastCharacterRosterSignature = rosterSignature;
									needsBroadcast = true;
									needsUpdatePartystate = true;
								}

								s_lastRealPlayers = realPlayers;
								s_lastBotsToAdd = botsToAdd;
							}
						}

					}
					else if (!isCurrentlyHosting && s_wasHostingLastFrame &&
						!s_characterRosterFrozen && !s_directLaunchRoster)
					{
						Dvar::Var("party_currentPlayers").set(0);
						Dvar::Var("party_realPlayers").set(0);
						Dvar::Var("addBots").set(0);
						Dvar::Var("party_roster_loading").set(true);
						s_directLaunchRoster = false;
						s_characterRosterFrozen = false;
						CharacterAssignments::ResetAll();
						s_hostCharacter.clear();
						for (int slot = 1; slot <= MAX_PARTY_SLOTS; ++slot)
						{
							SetStringDvarIfChanged(Utils::String::VA("character_%d", slot), "None");
							SetStringDvarIfChanged(Utils::String::VA("character_%d_player", slot), "None");
						}
						needsBroadcast = true;
						needsUpdatePartystate = true;
					}

					s_lastRealPlayers = Dvar::Var("party_realPlayers").get<int>();

					if (isCurrentlyHosting &&
						(s_characterRosterFrozen || s_directLaunchRoster) &&
						PublishRuntimeCharacterRoster())
					{
						needsBroadcast = true;
					}

					if (isCurrentlyHosting)
					{
						const auto currentHost = Dvar::Var("name").get<std::string>();
						char hostNameBuffer[256]{};
						TextRenderer::StripColors(currentHost.c_str(), hostNameBuffer, sizeof(hostNameBuffer));
						if (SetStringDvarIfChanged("party_currentHost", std::string("^7") + hostNameBuffer))
						{
							needsBroadcast = true;
						}
					}
					else
					{
						auto remoteHost = Dvar::Var("party_hostname").get<std::string>();
						if (remoteHost.empty())
						{
							remoteHost = Container.info.get("party_currentHost");
						}
						if (remoteHost.empty())
						{
							remoteHost = Container.info.get("hostname");
						}

						if (!remoteHost.empty())
						{
							char hostNameBuffer[256]{};
							TextRenderer::StripColors(remoteHost.c_str(), hostNameBuffer, sizeof(hostNameBuffer));
							SetStringDvarIfChanged("party_currentHost", std::string("^7") + hostNameBuffer);
						}
					}

					if (startedHosting) {
						needsBroadcast = true;
					}

					s_wasHostingLastFrame = isCurrentlyHosting;

					if (needsBroadcast)
					{
						BroadcastDvarUpdate();
					}

					if (needsUpdatePartystate) {
						Command::Execute("xupdatepartystate");
						needsUpdatePartystate = false;
					}
				}, Scheduler::Pipeline::MAIN, 100ms);
		}
	}
}
