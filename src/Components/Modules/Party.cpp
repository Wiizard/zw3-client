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
#include <version.hpp>
#include <unordered_set>
#include <unordered_map>
#include <cctype>
#include <charconv>
#include <functional>
#include <array>

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
	static std::unordered_map<std::uint64_t, std::string> s_characterByXuid;
	static std::string s_hostCharacter;
	static int s_liveHostClientNum = -1;
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

	static bool SetStringDvarIfChanged(const std::string& name, const std::string& value)
	{
		return SetStringDvarIfChanged(name.c_str(), value);
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
		std::uint64_t identity = 0;
		std::string name;
		bool host = false;
		int clientNum = -1;
	};

	struct CharacterRosterEntry
	{
		std::string character;
		std::string owner;
		bool bot = false;
		std::uint64_t identity = 0;
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
		std::string result;
		result.reserve(name.size());

		for (std::size_t i = 0; i < name.size(); ++i)
		{
			if (name[i] == '^' && i + 1 < name.size())
			{
				++i;
				continue;
			}

			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(name[i]))));
		}

		const auto first = result.find_first_not_of(" \t");
		if (first == std::string::npos)
		{
			return {};
		}

		const auto last = result.find_last_not_of(" \t");
		return result.substr(first, last - first + 1);
	}

	static std::string CanonicalCharacterName(const std::string& name)
	{
		const auto normalized = NormalizePartyIdentityName(name);

		for (const auto* character : ZW3Characters)
		{
			if (normalized == NormalizePartyIdentityName(character))
			{
				return character;
			}
		}

		return {};
	}

	static bool IsBotCharacterOwner(const std::string& owner)
	{
		return NormalizePartyIdentityName(owner).rfind("[bot] ", 0) == 0;
	}

	static std::string CharacterFromBotOwner(const std::string& owner)
	{
		auto normalized = NormalizePartyIdentityName(owner);
		if (normalized.rfind("[bot] ", 0) != 0)
		{
			return {};
		}

		return CanonicalCharacterName(normalized.substr(6));
	}

	static bool IsSyntheticCharacterIdentity(const std::uint64_t identity)
	{
		const auto prefix = identity & 0xF000000000000000ull;
		return prefix == 0x6000000000000000ull || prefix == 0x8000000000000000ull;
	}

	static Game::dvar_t* EnsurePerClientCharacterDvar(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return nullptr;
		}

		const auto* name = Utils::String::VA("zw3_character_client_%d", clientNum);
		auto* dvar = Game::Dvar_FindVar(name);
		if (!dvar)
		{
			dvar = Game::Dvar_RegisterString(name, "None", Game::DVAR_NONE,
				"Authoritative ZW3 character for this server client");
		}

		return dvar;
	}

	static int FindLiveHostClientNum()
	{
		if (Dedicated::IsRunning() || !Dvar::Var("party_host").get<bool>())
		{
			s_liveHostClientNum = -1;
			return -1;
		}

		if (s_liveHostClientNum >= 0 && s_liveHostClientNum < Game::MAX_CLIENTS)
		{
			const auto& cached = Game::svs_clients[s_liveHostClientNum];
			if (cached.header.state >= Game::CS_CONNECTED && !cached.bIsTestClient)
			{
				return s_liveHostClientNum;
			}
		}

		s_liveHostClientNum = -1;
		const auto localName = NormalizePartyIdentityName(Dvar::Var("name").get<std::string>());
		int firstRealClient = -1;
		int bestNameMatch = -1;

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state < Game::CS_CONNECTED || client.bIsTestClient)
			{
				continue;
			}

			if (firstRealClient == -1)
			{
				firstRealClient = clientNum;
			}

			if (!localName.empty() &&
				_stricmp(NormalizePartyIdentityName(client.name).c_str(), localName.c_str()) == 0)
			{
				if (bestNameMatch == -1 ||
					client.ping < Game::svs_clients[bestNameMatch].ping ||
					(client.ping == Game::svs_clients[bestNameMatch].ping && clientNum < bestNameMatch))
				{
					bestNameMatch = clientNum;
				}
			}
		}

		s_liveHostClientNum = bestNameMatch != -1 ? bestNameMatch : firstRealClient;
		return s_liveHostClientNum;
	}

	static std::vector<RealCharacterParticipant> CollectRealCharacterParticipants()
	{
		std::vector<RealCharacterParticipant> result;
		if (!Dvar::Var("party_host").get<bool>())
		{
			return result;
		}

		const auto hostXuid = Party::GetLocalPlayerXUID();
		const auto hostName = Dvar::Var("name").get<std::string>();
		const auto normalizedHostName = NormalizePartyIdentityName(hostName);
		const std::uint64_t hostIdentity = hostXuid != 0
			? hostXuid
			: 0x4000000000000001ull;

		if (Game::CL_IsCgameInitialized())
		{
			const int hostClientNum = FindLiveHostClientNum();
			if (hostClientNum >= 0)
			{
				result.push_back({ hostIdentity,
					hostName.empty() ? std::string(Game::svs_clients[hostClientNum].name) : hostName,
					true, hostClientNum });
			}

			struct LobbyIdentity
			{
				std::uint64_t xuid = 0;
				std::string normalizedName;
				bool claimed = false;
			};

			std::vector<LobbyIdentity> lobbyIdentities;
			bool skippedHost = false;
			bool hasExactHostLobbyEntry = false;

			if (Game::g_lobbyData && hostXuid != 0)
			{
				for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
				{
					const auto& member = Game::g_lobbyData->partyMembers[slot];
					if (member.status != 0 && member.player == hostXuid)
					{
						hasExactHostLobbyEntry = true;
						break;
					}
				}
			}

			if (Game::g_lobbyData)
			{
				for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
				{
					auto& member = Game::g_lobbyData->partyMembers[slot];
					if (member.status == 0 || !member.gamertag || !member.gamertag[0])
					{
						continue;
					}

					const auto normalizedName = NormalizePartyIdentityName(member.gamertag);
					const bool isHost = hasExactHostLobbyEntry
						? (member.player != 0 && member.player == hostXuid)
						: (!normalizedHostName.empty() && normalizedName == normalizedHostName);

					if (!skippedHost && isHost)
					{
						skippedHost = true;
						continue;
					}

					lobbyIdentities.push_back({ member.player, normalizedName, false });
				}
			}

			std::vector<int> otherRealClients;
			for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
			{
				const auto& client = Game::svs_clients[clientNum];
				if (clientNum == hostClientNum || client.header.state < Game::CS_CONNECTED ||
					client.bIsTestClient || !client.name[0])
				{
					continue;
				}

				otherRealClients.push_back(clientNum);
			}

			std::sort(otherRealClients.begin(), otherRealClients.end());
			std::unordered_set<std::uint64_t> usedIdentities{ hostIdentity };

			for (const auto clientNum : otherRealClients)
			{
				const std::string name = Game::svs_clients[clientNum].name;
				const auto normalizedName = NormalizePartyIdentityName(name);
				std::uint64_t identity = 0;

				for (auto& lobbyIdentity : lobbyIdentities)
				{
					if (lobbyIdentity.claimed || lobbyIdentity.normalizedName != normalizedName)
					{
						continue;
					}

					lobbyIdentity.claimed = true;
					if (lobbyIdentity.xuid != 0 && !usedIdentities.contains(lobbyIdentity.xuid))
					{
						identity = lobbyIdentity.xuid;
					}
					break;
				}

				if (identity == 0)
				{
					const auto nameHash = static_cast<std::uint64_t>(std::hash<std::string>{}(normalizedName));
					identity = 0x8000000000000000ull |
						((nameHash << 8) & 0x7FFFFFFFFFFFFF00ull) |
						static_cast<std::uint64_t>((clientNum + 1) & 0xFF);

					while (usedIdentities.contains(identity))
					{
						++identity;
					}
				}

				usedIdentities.insert(identity);
				result.push_back({ identity, name, false, clientNum });
				if (result.size() >= MAX_PARTY_SLOTS)
				{
					break;
				}
			}

			if (!result.empty())
			{
				return result;
			}
		}

		result.push_back({ hostIdentity, hostName, true, -1 });
		std::unordered_set<std::uint64_t> usedIdentities{ hostIdentity };
		bool skippedHost = false;
		bool hasExactHostLobbyEntry = false;

		if (Game::g_lobbyData && hostXuid != 0)
		{
			for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
			{
				const auto& member = Game::g_lobbyData->partyMembers[slot];
				if (member.status != 0 && member.player == hostXuid)
				{
					hasExactHostLobbyEntry = true;
					break;
				}
			}
		}

		if (Game::g_lobbyData)
		{
			for (int slot = 0; slot < MAX_PARTY_SLOTS && result.size() < MAX_PARTY_SLOTS; ++slot)
			{
				auto& member = Game::g_lobbyData->partyMembers[slot];
				if (member.status == 0 || !member.gamertag || !member.gamertag[0])
				{
					continue;
				}

				const std::string memberName = member.gamertag;
				const auto normalizedName = NormalizePartyIdentityName(memberName);
				const bool isHost = hasExactHostLobbyEntry
					? (member.player != 0 && member.player == hostXuid)
					: (!normalizedHostName.empty() && normalizedName == normalizedHostName);

				if (!skippedHost && isHost)
				{
					skippedHost = true;
					continue;
				}

				std::uint64_t identity = member.player;
				if (identity == 0 || usedIdentities.contains(identity))
				{
					const auto nameHash = static_cast<std::uint64_t>(std::hash<std::string>{}(normalizedName));
					identity = 0x6000000000000000ull |
						((nameHash << 8) & 0x1FFFFFFFFFFFFF00ull) |
						static_cast<std::uint64_t>((slot + 1) & 0xFF);
					while (usedIdentities.contains(identity))
					{
						++identity;
					}
				}

				usedIdentities.insert(identity);
				result.push_back({ identity, memberName, false, -1 });
			}
		}

		return result;
	}

	static std::vector<CharacterRosterEntry> ReadPublishedCharacterRoster()
	{
		std::vector<CharacterRosterEntry> result;
		result.reserve(MAX_PARTY_SLOTS);

		for (int slot = 1; slot <= MAX_PARTY_SLOTS; ++slot)
		{
			auto character = CanonicalCharacterName(
				Dvar::Var(Utils::String::VA("character_%d", slot)).get<std::string>());
			const auto owner = Dvar::Var(Utils::String::VA("character_%d_player", slot)).get<std::string>();

			if (character.empty() && IsBotCharacterOwner(owner))
			{
				character = CharacterFromBotOwner(owner);
			}

			if (character.empty() || owner.empty() || owner == "None")
			{
				continue;
			}

			result.push_back({ character, owner, IsBotCharacterOwner(owner), 0 });
		}

		return result;
	}

	static std::string ChooseInitialHostCharacter()
	{
		if (!CanonicalCharacterName(s_hostCharacter).empty())
		{
			return CanonicalCharacterName(s_hostCharacter);
		}

		const auto existingOwner = Dvar::Var("character_1_player").get<std::string>();
		const auto existingCharacter = CanonicalCharacterName(Dvar::Var("character_1").get<std::string>());
		const auto hostName = Dvar::Var("name").get<std::string>();

		if (!existingCharacter.empty() && !existingOwner.empty() &&
			_stricmp(NormalizePartyIdentityName(existingOwner).c_str(),
				NormalizePartyIdentityName(hostName).c_str()) == 0)
		{
			s_hostCharacter = existingCharacter;
			return s_hostCharacter;
		}

		const auto index = static_cast<std::size_t>(Game::Sys_Milliseconds()) % MAX_PARTY_SLOTS;
		s_hostCharacter = ZW3Characters[index];
		return s_hostCharacter;
	}

	static void SetClientCharacter(const int clientNum, const std::string& character, const bool resetTransientState)
	{
		auto* dvar = EnsurePerClientCharacterDvar(clientNum);
		if (!dvar)
		{
			return;
		}

		const auto canonical = CanonicalCharacterName(character);
		const auto previous = CanonicalCharacterName(dvar->current.string ? dvar->current.string : "");

		if (_stricmp(previous.c_str(), canonical.c_str()) == 0)
		{
			return;
		}

		Game::Dvar_SetString(dvar, canonical.empty() ? "None" : canonical.c_str());

		if (resetTransientState)
		{
			Dvar::Var(Utils::String::VA("zw3_sb_down_%d", clientNum)).set(0);
			Dvar::Var(Utils::String::VA("zw3_sb_down_progress_%d", clientNum)).set(0.0f);
		}
	}

	static void SyncLiveClientCharacterDvars(
		const std::vector<RealCharacterParticipant>& participants,
		const std::vector<CharacterRosterEntry>& roster)
	{
		std::vector<int> resolvedClientNums(participants.size(), -1);
		std::unordered_set<int> assignedRealClients;
		static std::array<bool, Game::MAX_CLIENTS> wasConnected{};
		static std::array<bool, Game::MAX_CLIENTS> wasBot{};
		static std::array<int, Game::MAX_CLIENTS> previousState{};
		static std::array<std::string, Game::MAX_CLIENTS> occupantNames{};
		std::unordered_set<int> freshOccupants;

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			const int state = client.header.state;
			const bool connected = state >= Game::CS_CONNECTED;
			const bool bot = connected && client.bIsTestClient;
			std::string occupantName;

			if (connected && !bot)
			{
				occupantName = NormalizePartyIdentityName(client.name);
			}

			const bool lifecycleRestarted = connected && !bot && wasConnected[clientNum] &&
				previousState[clientNum] >= Game::CS_ACTIVE && state < Game::CS_ACTIVE;
			const bool realNameChanged = connected && !bot && wasConnected[clientNum] &&
				!wasBot[clientNum] && occupantNames[clientNum] != occupantName;
			const bool occupantChanged = connected &&
				(!wasConnected[clientNum] || wasBot[clientNum] != bot ||
					lifecycleRestarted || realNameChanged);

			if (!connected)
			{
				if (wasConnected[clientNum])
				{
					SetClientCharacter(clientNum, "None", false);
				}

				wasConnected[clientNum] = false;
				wasBot[clientNum] = false;
				previousState[clientNum] = state;
				occupantNames[clientNum].clear();
				continue;
			}

			if (occupantChanged)
			{
				freshOccupants.insert(clientNum);
				Dvar::Var(Utils::String::VA("zw3_sb_down_%d", clientNum)).set(0);
				Dvar::Var(Utils::String::VA("zw3_sb_down_progress_%d", clientNum)).set(0.0f);
			}

			wasConnected[clientNum] = true;
			wasBot[clientNum] = bot;
			previousState[clientNum] = state;
			occupantNames[clientNum] = occupantName;
		}

		for (std::size_t index = 0; index < participants.size(); ++index)
		{
			const auto clientNum = participants[index].clientNum;
			if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
			{
				continue;
			}

			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state < Game::CS_CONNECTED || client.bIsTestClient)
			{
				continue;
			}

			resolvedClientNums[index] = clientNum;
			assignedRealClients.insert(clientNum);
		}

		if (!participants.empty() && resolvedClientNums[0] == -1 && participants[0].host)
		{
			const auto hostClientNum = FindLiveHostClientNum();
			if (hostClientNum >= 0 && !assignedRealClients.contains(hostClientNum))
			{
				resolvedClientNums[0] = hostClientNum;
				assignedRealClients.insert(hostClientNum);
			}
		}

		for (std::size_t index = 0; index < participants.size(); ++index)
		{
			if (resolvedClientNums[index] != -1)
			{
				continue;
			}

			const auto normalizedParticipantName = NormalizePartyIdentityName(participants[index].name);
			if (normalizedParticipantName.empty())
			{
				continue;
			}

			for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
			{
				const auto& client = Game::svs_clients[clientNum];
				if (client.header.state < Game::CS_CONNECTED || client.bIsTestClient ||
					assignedRealClients.contains(clientNum) || !client.name[0])
				{
					continue;
				}

				if (NormalizePartyIdentityName(client.name) != normalizedParticipantName)
				{
					continue;
				}

				resolvedClientNums[index] = clientNum;
				assignedRealClients.insert(clientNum);
				break;
			}
		}

		std::size_t realRosterIndex = 0;
		for (const auto& entry : roster)
		{
			if (entry.bot)
			{
				continue;
			}

			if (realRosterIndex >= resolvedClientNums.size())
			{
				break;
			}

			const auto clientNum = resolvedClientNums[realRosterIndex++];
			if (clientNum >= 0)
			{
				SetClientCharacter(clientNum, entry.character, true);
			}
		}

		std::vector<std::string> desiredBotCharacters;
		std::unordered_set<std::string> desiredBotCharacterKeys;

		for (const auto& entry : roster)
		{
			if (!entry.bot)
			{
				continue;
			}

			const auto character = CanonicalCharacterName(entry.character);
			const auto key = NormalizePartyIdentityName(character);
			if (character.empty() || desiredBotCharacterKeys.contains(key))
			{
				continue;
			}

			desiredBotCharacters.push_back(character);
			desiredBotCharacterKeys.insert(key);
		}

		std::vector<int> liveBotClients;
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state < Game::CS_CONNECTED)
			{
				SetClientCharacter(clientNum, "None", false);
				continue;
			}

			if (!client.bIsTestClient && !assignedRealClients.contains(clientNum))
			{
				if (Game::CL_IsCgameInitialized())
				{
					SetClientCharacter(clientNum, "None", true);
				}
				continue;
			}

			if (client.bIsTestClient)
			{
				liveBotClients.push_back(clientNum);
			}
		}

		std::unordered_map<int, std::string> resolvedBotCharacters;
		std::unordered_set<std::string> claimedBotCharacterKeys;

		auto tryClaimBotCharacter = [&](const int clientNum, const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				const auto key = NormalizePartyIdentityName(character);

				if (character.empty() || !desiredBotCharacterKeys.contains(key) ||
					claimedBotCharacterKeys.contains(key))
				{
					return false;
				}

				resolvedBotCharacters[clientNum] = character;
				claimedBotCharacterKeys.insert(key);
				return true;
			};

		for (const auto clientNum : liveBotClients)
		{
			const auto reservedOwner = Bots::GetBotDisplayName(clientNum);
			const auto reservedCharacter = CharacterFromBotOwner(reservedOwner);

			if (!reservedCharacter.empty())
			{
				tryClaimBotCharacter(clientNum, reservedCharacter);
			}
		}

		for (const auto clientNum : liveBotClients)
		{
			if (resolvedBotCharacters.contains(clientNum))
			{
				continue;
			}

			const auto* dvar = EnsurePerClientCharacterDvar(clientNum);
			if (dvar && dvar->current.string)
			{
				tryClaimBotCharacter(clientNum, dvar->current.string);
			}
		}

		for (const auto clientNum : liveBotClients)
		{
			if (resolvedBotCharacters.contains(clientNum) ||
				!Bots::GetBotDisplayName(clientNum).empty())
			{
				continue;
			}

			for (const auto& character : desiredBotCharacters)
			{
				if (tryClaimBotCharacter(clientNum, character))
				{
					break;
				}
			}
		}

		for (const auto clientNum : liveBotClients)
		{
			const auto found = resolvedBotCharacters.find(clientNum);
			if (found != resolvedBotCharacters.end())
			{
				SetClientCharacter(clientNum, found->second, freshOccupants.contains(clientNum));
			}
			else
			{
				SetClientCharacter(clientNum, "None", true);
			}
		}
	}

	static std::string BuildCharacterRosterSignature(
		const std::vector<RealCharacterParticipant>& participants,
		const int botsToAdd)
	{
		std::string signature = std::to_string(botsToAdd);
		signature.append("|pending=");
		signature.append(NormalizePartyIdentityName(
			Dvar::Var("zw3_pending_replacement_character").get<std::string>()));

		for (const auto& participant : participants)
		{
			signature.append("|");
			signature.append(std::to_string(participant.identity));
			signature.append(":");
			signature.append(NormalizePartyIdentityName(participant.name));
			signature.append(":");
			signature.append(std::to_string(participant.clientNum));
		}

		return signature;
	}

	void Party::RandomizeCharactersForClients()
	{
		if (!Dvar::Var("party_host").get<bool>())
		{
			return;
		}

		const auto participants = CollectRealCharacterParticipants();
		if (participants.empty())
		{
			return;
		}

		const auto previousRoster = ReadPublishedCharacterRoster();
		const int realPlayers = std::min(static_cast<int>(participants.size()), MAX_PARTY_SLOTS);
		const int botsToAdd = std::clamp(Dvar::Var("addBots").get<int>(), 0,
			MAX_PARTY_SLOTS - realPlayers);

		int previousBotCount = 0;
		std::unordered_set<std::string> protectedBotCharacters;
		for (const auto& entry : previousRoster)
		{
			if (!entry.bot || CanonicalCharacterName(entry.character).empty())
			{
				continue;
			}

			++previousBotCount;
			protectedBotCharacters.insert(NormalizePartyIdentityName(entry.character));
		}

		int botsToRemove = std::max(0, previousBotCount - botsToAdd);

		std::unordered_set<std::uint64_t> activeIdentities;
		for (const auto& participant : participants)
		{
			activeIdentities.insert(participant.identity);
		}

		for (auto it = s_characterByXuid.begin(); it != s_characterByXuid.end();)
		{
			if (IsSyntheticCharacterIdentity(it->first) && !activeIdentities.contains(it->first))
			{
				it = s_characterByXuid.erase(it);
			}
			else
			{
				++it;
			}
		}

		std::vector<bool> previousConsumed(previousRoster.size(), false);
		std::unordered_set<std::string> usedCharacters;
		std::vector<CharacterRosterEntry> roster;
		roster.reserve(MAX_PARTY_SLOTS);

		auto isAvailable = [&](const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				return !character.empty() &&
					!usedCharacters.contains(NormalizePartyIdentityName(character));
			};

		auto useCharacter = [&](const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				usedCharacters.insert(NormalizePartyIdentityName(character));
				return character;
			};

		auto isAvailableWithoutStealingBot = [&](const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				return isAvailable(character) &&
					!protectedBotCharacters.contains(NormalizePartyIdentityName(character));
			};

		auto releaseProtectedBotCharacter = [&](const std::string& value)
			{
				const auto character = CanonicalCharacterName(value);
				if (character.empty())
				{
					return;
				}

				protectedBotCharacters.erase(NormalizePartyIdentityName(character));
				for (std::size_t i = 0; i < previousRoster.size(); ++i)
				{
					if (!previousConsumed[i] && previousRoster[i].bot &&
						_stricmp(previousRoster[i].character.c_str(), character.c_str()) == 0)
					{
						previousConsumed[i] = true;
						break;
					}
				}

				if (botsToRemove > 0)
				{
					--botsToRemove;
				}
			};

		auto firstAvailableCanonical = [&]() -> std::string
			{
				for (const auto* character : ZW3Characters)
				{
					if (isAvailable(character))
					{
						return character;
					}
				}
				return {};
			};

		auto firstAvailableWithoutStealingBot = [&]() -> std::string
			{
				for (const auto* character : ZW3Characters)
				{
					if (isAvailableWithoutStealingBot(character))
					{
						return character;
					}
				}
				return {};
			};

		auto findPreviousHumanCharacter = [&](const std::string& owner) -> std::string
			{
				const auto normalizedOwner = NormalizePartyIdentityName(owner);
				for (std::size_t i = 0; i < previousRoster.size(); ++i)
				{
					if (previousConsumed[i] || previousRoster[i].bot ||
						!isAvailable(previousRoster[i].character))
					{
						continue;
					}

					if (_stricmp(NormalizePartyIdentityName(previousRoster[i].owner).c_str(),
						normalizedOwner.c_str()) == 0)
					{
						previousConsumed[i] = true;
						return previousRoster[i].character;
					}
				}
				return {};
			};

		auto takeFirstPreviousBotCharacter = [&]() -> std::string
			{
				for (std::size_t i = 0; i < previousRoster.size(); ++i)
				{
					if (previousConsumed[i] || !previousRoster[i].bot ||
						!isAvailable(previousRoster[i].character))
					{
						continue;
					}

					previousConsumed[i] = true;
					return previousRoster[i].character;
				}
				return {};
			};

		const auto pendingReplacement = CanonicalCharacterName(
			Dvar::Var("zw3_pending_replacement_character").get<std::string>());
		bool pendingConsumed = false;

		for (int index = 0; index < realPlayers; ++index)
		{
			const auto& participant = participants[index];
			std::string character;

			auto tryRememberedCharacter = [&]()
				{
					const auto remembered = s_characterByXuid.find(participant.identity);
					if (remembered == s_characterByXuid.end())
					{
						return;
					}

					if (isAvailableWithoutStealingBot(remembered->second))
					{
						character = remembered->second;
						return;
					}

					if (!participant.host && botsToRemove > 0 && isAvailable(remembered->second) &&
						protectedBotCharacters.contains(NormalizePartyIdentityName(remembered->second)))
					{
						character = remembered->second;
						releaseProtectedBotCharacter(character);
					}
				};

			tryRememberedCharacter();

			if (character.empty())
			{
				character = findPreviousHumanCharacter(participant.name);
			}

			if (participant.host)
			{
				if (character.empty() && isAvailableWithoutStealingBot(ChooseInitialHostCharacter()))
				{
					character = ChooseInitialHostCharacter();
				}
			}
			else
			{
				if (character.empty() && !pendingConsumed && isAvailable(pendingReplacement))
				{
					character = pendingReplacement;
					pendingConsumed = true;
					if (protectedBotCharacters.contains(NormalizePartyIdentityName(character)))
					{
						releaseProtectedBotCharacter(character);
					}
				}

				if (character.empty() && botsToRemove > 0)
				{
					character = takeFirstPreviousBotCharacter();
					if (!character.empty())
					{
						protectedBotCharacters.erase(NormalizePartyIdentityName(character));
						--botsToRemove;
					}
				}
			}

			if (character.empty())
			{
				character = firstAvailableWithoutStealingBot();
			}

			if (character.empty())
			{
				character = firstAvailableCanonical();
			}

			if (character.empty())
			{
				continue;
			}

			character = useCharacter(character);
			s_characterByXuid[participant.identity] = character;
			if (participant.host)
			{
				s_hostCharacter = character;
			}

			roster.push_back({ character, participant.name, false, participant.identity });
		}

		if (pendingConsumed)
		{
			SetStringDvarIfChanged("zw3_pending_replacement_character", "");
		}

		int botsAssigned = 0;
		for (std::size_t i = 0; i < previousRoster.size() && botsAssigned < botsToAdd; ++i)
		{
			if (previousConsumed[i] || !previousRoster[i].bot ||
				!isAvailable(previousRoster[i].character))
			{
				continue;
			}

			const auto character = useCharacter(previousRoster[i].character);
			roster.push_back({ character,
				Utils::String::VA("[BOT] %s", character.c_str()), true, 0 });
			++botsAssigned;
		}

		while (botsAssigned < botsToAdd)
		{
			const auto available = firstAvailableCanonical();
			if (available.empty())
			{
				break;
			}

			const auto character = useCharacter(available);
			roster.push_back({ character,
				Utils::String::VA("[BOT] %s", character.c_str()), true, 0 });
			++botsAssigned;
		}

		for (int slot = 0; slot < MAX_PARTY_SLOTS; ++slot)
		{
			const auto characterDvar = std::string(Utils::String::VA("character_%d", slot + 1));
			const auto playerDvar = std::string(Utils::String::VA("character_%d_player", slot + 1));

			if (slot < static_cast<int>(roster.size()))
			{
				SetStringDvarIfChanged(characterDvar, roster[slot].character);
				SetStringDvarIfChanged(playerDvar, roster[slot].owner);
			}
			else
			{
				SetStringDvarIfChanged(characterDvar, "None");
				SetStringDvarIfChanged(playerDvar, "None");
			}
		}

		SyncLiveClientCharacterDvars(participants, roster);
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
				Dvar::Register<const char*>("character_1", "", Game::DVAR_CODINFO | Game::DVAR_INIT, "Character assigned to player 1");
				Dvar::Register<const char*>("character_2", "", Game::DVAR_CODINFO | Game::DVAR_INIT, "Character assigned to player 2");
				Dvar::Register<const char*>("character_3", "", Game::DVAR_CODINFO | Game::DVAR_INIT, "Character assigned to player 3");
				Dvar::Register<const char*>("character_4", "", Game::DVAR_CODINFO | Game::DVAR_INIT, "Character assigned to player 4");
				Dvar::Register<const char*>("character_1_player", "None", Game::DVAR_CODINFO | Game::DVAR_INIT, "Player name assigned to slot 1");
				Dvar::Register<const char*>("character_2_player", "None", Game::DVAR_CODINFO | Game::DVAR_INIT, "Player name assigned to slot 2");
				Dvar::Register<const char*>("character_3_player", "None", Game::DVAR_CODINFO | Game::DVAR_INIT, "Player name assigned to slot 3");
				Dvar::Register<const char*>("character_4_player", "None", Game::DVAR_CODINFO | Game::DVAR_INIT, "Player name assigned to slot 4");
				Dvar::Register<const char*>("zw3_pending_replacement_character", "", Game::DVAR_NONE, "Smart-bot character reserved for an incoming real player");
				for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
				{
					Game::Dvar_RegisterString(Utils::String::VA("zw3_character_client_%d", clientNum),
						"None", Game::DVAR_NONE, "Authoritative ZW3 character for this server client");
				}
				Dvar::Register<int>("party_currentPlayers", 0, 0, 4, Game::DVAR_CODINFO | Game::DVAR_INIT, "Total current players in the party");
				Dvar::Register<int>("party_realPlayers", 0, 0, 4, Game::DVAR_CODINFO | Game::DVAR_INIT, "Current real players in the party");
				Dvar::Register<const char*>("party_currentHost", "", Game::DVAR_NONE, "Current private-party host display name");
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

				const auto participants = CollectRealCharacterParticipants();
				const int realPlayers = std::clamp(static_cast<int>(participants.size()), 1, MAX_PARTY_SLOTS);
				const int bots = std::clamp(Dvar::Var("addBots").get<int>(), 0, MAX_PARTY_SLOTS - realPlayers);
				Dvar::Var("addBots").set(bots);
				Dvar::Var("party_realPlayers").set(realPlayers);
				Dvar::Var("party_currentPlayers").set(realPlayers + bots);

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

				const auto participants = CollectRealCharacterParticipants();
				const int realPlayers = std::clamp(static_cast<int>(participants.size()), 1, MAX_PARTY_SLOTS);
				const int maxBots = std::max(0, MAX_PARTY_SLOTS - realPlayers);
				const int currentBots = std::clamp(Dvar::Var("addBots").get<int>(), 0, maxBots);
				const int nextBots = maxBots == 0 ? 0 : (currentBots + 1) % (maxBots + 1);

				Dvar::Var("addBots").set(nextBots);
				Dvar::Var("party_realPlayers").set(realPlayers);
				Dvar::Var("party_currentPlayers").set(realPlayers + nextBots);
				RandomizeCharactersForClients();
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
				auto* map = Game::Dvar_FindVar("autosave_map");

				if (!map || !map->current.string || !map->current.string[0])
				{
					return;
				}

				Dvar::Var("autosave_load").set(true);

				Scheduler::Schedule([]() -> bool
					{
						if (!Game::CL_IsCgameInitialized())
						{
							return false;
						}

						Scheduler::Once([]()
							{
								Dvar::Var("autosave_load").set(false);
							}, Scheduler::Pipeline::MAIN, 5s);

						return true;
					}, Scheduler::Pipeline::MAIN, 100ms);

				std::string cmd = "map "s + map->current.string;
				Command::Execute(cmd.c_str());
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
				hostResponseInfo.set("character_1", Dvar::Var("character_1").get<std::string>());
				hostResponseInfo.set("character_2", Dvar::Var("character_2").get<std::string>());
				hostResponseInfo.set("character_3", Dvar::Var("character_3").get<std::string>());
				hostResponseInfo.set("character_4", Dvar::Var("character_4").get<std::string>());

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

						int new_party_currentPlayers = static_cast<int>(std::strtol(info.get("party_currentPlayers").data(), nullptr, 10));
						int new_party_realPlayers = static_cast<int>(std::strtol(info.get("party_realPlayers").data(), nullptr, 10));
						Dvar::Var("party_currentPlayers").set(new_party_currentPlayers);
						Dvar::Var("party_realPlayers").set(new_party_realPlayers);

						auto receivedHostName = info.get("party_currentHost");
						if (receivedHostName.empty())
						{
							receivedHostName = info.get("hostname");
						}
						if (!receivedHostName.empty())
						{
							SetStringDvarIfChanged("party_currentHost", receivedHostName);
						}

						for (int i = 1; i <= MAX_PARTY_SLOTS; ++i)
						{
							std::string charDvarName = Utils::String::VA("character_%d", i);
							std::string playerDvarName = Utils::String::VA("character_%d_player", i);

							const std::string charValue = info.get(charDvarName);
							SetStringDvarIfChanged(charDvarName, charValue.empty() ? "None" : charValue);

							const std::string playerValue = info.get(playerDvarName);
							SetStringDvarIfChanged(playerDvarName, playerValue.empty() ? "None" : playerValue);
						}

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
				int new_party_currentPlayers = static_cast<int>(std::strtol(info.get("party_currentPlayers").data(), nullptr, 10));
				int new_party_realPlayers = static_cast<int>(std::strtol(info.get("party_realPlayers").data(), nullptr, 10));
				Dvar::Var("party_currentPlayers").set(new_party_currentPlayers);
				Dvar::Var("party_realPlayers").set(new_party_realPlayers);

				const auto receivedHostName = info.get("party_currentHost");
				if (!receivedHostName.empty())
				{
					SetStringDvarIfChanged("party_currentHost", receivedHostName);
				}

				for (int i = 1; i <= MAX_PARTY_SLOTS; ++i)
				{
					std::string charDvarName = Utils::String::VA("character_%d", i);
					std::string playerDvarName = Utils::String::VA("character_%d_player", i);

					const std::string charValue = info.get(charDvarName);
					SetStringDvarIfChanged(charDvarName, charValue.empty() ? "None" : charValue);

					const std::string playerValue = info.get(playerDvarName);
					SetStringDvarIfChanged(playerDvarName, playerValue.empty() ? "None" : playerValue);
				}
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

					if (startedHosting)
					{
						s_characterByXuid.clear();
						s_hostCharacter.clear();
						s_liveHostClientNum = -1;
						SetStringDvarIfChanged("zw3_pending_replacement_character", "");
					}

					if (isCurrentlyHosting)
					{
						const auto participants = CollectRealCharacterParticipants();
						const int realPlayers = std::min(static_cast<int>(participants.size()), MAX_PARTY_SLOTS);
						int botsToAdd = Dvar::Var("addBots").get<int>();

						const int maxAllowedBots = std::max(0, MAX_PARTY_SLOTS - realPlayers);
						const int clampedBotsToAdd = std::clamp(botsToAdd, 0, maxAllowedBots);
						bool dvarChanged = false;

						if (botsToAdd != clampedBotsToAdd)
						{
							Dvar::Var("addBots").set(clampedBotsToAdd);
							botsToAdd = clampedBotsToAdd;
							dvarChanged = true;
							needsBroadcast = true;
							needsUpdatePartystate = true;
						}

						const int totalPlayers = realPlayers + botsToAdd;

						if (Dvar::Var("party_realPlayers").get<int>() != realPlayers)
						{
							Dvar::Var("party_realPlayers").set(realPlayers);
							dvarChanged = true;
						}

						if (Dvar::Var("party_currentPlayers").get<int>() != totalPlayers)
						{
							Dvar::Var("party_currentPlayers").set(totalPlayers);
							dvarChanged = true;
						}

						static std::string s_lastCharacterRosterSignature;
						const auto rosterSignature = BuildCharacterRosterSignature(participants, botsToAdd);

						if (startedHosting || rosterSignature != s_lastCharacterRosterSignature ||
							s_lastRealPlayers != realPlayers || s_lastBotsToAdd != botsToAdd)
						{
							RandomizeCharactersForClients();
							s_lastCharacterRosterSignature = BuildCharacterRosterSignature(CollectRealCharacterParticipants(), botsToAdd);
							needsBroadcast = true;
							needsUpdatePartystate = true;
						}

						SyncLiveClientCharacterDvars(participants, ReadPublishedCharacterRoster());

						s_lastRealPlayers = realPlayers;
						s_lastBotsToAdd = botsToAdd;

						if (dvarChanged)
						{
							needsBroadcast = true;
							needsUpdatePartystate = true;
						}

					}
					else if (!isCurrentlyHosting && s_wasHostingLastFrame) {
						Dvar::Var("party_currentPlayers").set(0);
						Dvar::Var("party_realPlayers").set(0);
						Dvar::Var("addBots").set(0);
						s_characterByXuid.clear();
						s_hostCharacter.clear();
						s_liveHostClientNum = -1;
						SetStringDvarIfChanged("zw3_pending_replacement_character", "");
						for (int slot = 1; slot <= MAX_PARTY_SLOTS; ++slot)
						{
							SetStringDvarIfChanged(Utils::String::VA("character_%d", slot), "None");
							SetStringDvarIfChanged(Utils::String::VA("character_%d_player", slot), "None");
						}
						needsBroadcast = true;
						needsUpdatePartystate = true;
					}

					s_lastRealPlayers = Dvar::Var("party_realPlayers").get<int>();

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
