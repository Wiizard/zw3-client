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
#include "TextRenderer.hpp"
#include "Voice.hpp"
#include "Events.hpp"
#include <version.hpp>

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

	std::map<uint64_t, Components::Network::Address> Party::g_xuidToPublicAddressMap;
	static int s_lastTotalPlayers = -1;
	static int s_lastRealPlayers = -1;
	static std::string s_lastHostName = "";
	static bool s_wasHostingLastFrame = false;
	static std::array<int, 4> s_assigned_character_ids = { -1, -1, -1, -1 };
	const int MAX_PARTY_SLOTS = 4;

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

		info.set("character_1_player", Dvar::Var("character_1_player").get<std::string>());
		info.set("character_2_player", Dvar::Var("character_2_player").get<std::string>());
		info.set("character_3_player", Dvar::Var("character_3_player").get<std::string>());
		info.set("character_4_player", Dvar::Var("character_4_player").get<std::string>());

		const std::string builtDvarString = info.build();
		int totalSent = 0;

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

			if (memberXuid == GetLocalPlayerXUID())
			{
				continue;
			}

			Components::Network::Address targetAddr;

			auto it = Party::g_xuidToPublicAddressMap.find(memberXuid);
			if (it != Party::g_xuidToPublicAddressMap.end())
			{
				targetAddr = it->second;
			}
			else
			{
				continue;
			}

			if (strcmp(targetAddr.getString().c_str(), "127.0.0.1") == 0)
			{
				continue;
			}

			Network::SendCommand(targetAddr, "dvarUpdate", builtDvarString);
			++totalSent;
		}
	}

	const char* GetCharacterNameFromId(int id)
	{
		switch (id)
		{
		case 0: return "Richtofen";
		case 1: return "Dempsey";
		case 2:	return "Nikolai";
		case 3:	return "Takeo";
		default: return "None";
		}
	}
	int GetCharacterIdFromName(const char* name)
	{
		if (strcmp(name, "Richtofen") == 0) return 0;
		if (strcmp(name, "Dempsey") == 0) return 1;
		if (strcmp(name, "Nikolai") == 0) return 2;
		if (strcmp(name, "Takeo") == 0) return 3;
		return -1;
	}

	void Party::RandomizeCharactersForClients() {
		if (!Game::g_lobbyData) {
			return;
		}

		int realPlayers = Dvar::Var("party_realPlayers").get<int>();
		int botsToAdd = Dvar::Var("addBots").get<int>();
		int totalPlayers = realPlayers + botsToAdd;

		std::vector<int> allCharacterIds = { 0, 1, 2, 3 };
		std::vector<int> assignedCharacterIds;

		for (int i = 0; i < totalPlayers; ++i) {
			std::string charDvarName = Utils::String::VA("character_%d", i + 1);
			std::string currentCharacter = Dvar::Var(charDvarName).get<std::string>();

			int characterId = GetCharacterIdFromName(currentCharacter.c_str());

			bool isAlreadyAssigned = std::find(assignedCharacterIds.begin(), assignedCharacterIds.end(), characterId) != assignedCharacterIds.end();

			if (characterId != -1 && !isAlreadyAssigned) {
				assignedCharacterIds.push_back(characterId);
			}
		}

		std::vector<int> availableCharacterIds;
		for (int id : allCharacterIds) {
			if (std::find(assignedCharacterIds.begin(), assignedCharacterIds.end(), id) == assignedCharacterIds.end()) {
				availableCharacterIds.push_back(id);
			}
		}

		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
		std::shuffle(availableCharacterIds.begin(), availableCharacterIds.end(), std::default_random_engine(seed));

		int availableIndex = 0;

		for (int i = 0; i < totalPlayers; ++i) {
			std::string charDvarName = Utils::String::VA("character_%d", i + 1);
			std::string playerDvarName = Utils::String::VA("character_%d_player", i + 1);
			std::string currentCharacter = Dvar::Var(charDvarName).get<std::string>();

			if (currentCharacter.empty() || currentCharacter == "None" || GetCharacterIdFromName(currentCharacter.c_str()) == -1) {
				if (availableIndex < availableCharacterIds.size()) {
					const char* newCharacterName = GetCharacterNameFromId(availableCharacterIds[availableIndex++]);
					Dvar::Var(charDvarName).set(newCharacterName);
				}
				else {
					Dvar::Var(charDvarName).set("None");
				}
			}

			if (i < realPlayers) {
				Dvar::Var(playerDvarName).set(Game::g_lobbyData->partyMembers[i].gamertag);
			}
			else {
				std::string assignedCharacter = Dvar::Var(charDvarName).get<std::string>();
				if (!assignedCharacter.empty() && assignedCharacter != "None") {
					std::string botName = Utils::String::VA("[BOT] %s", assignedCharacter.c_str());
					Dvar::Var(playerDvarName).set(botName.c_str());
				}
				else {
					Dvar::Var(playerDvarName).set("None");
				}
			}
		}

		for (int i = totalPlayers; i < MAX_PARTY_SLOTS; ++i) {
			Dvar::Var(Utils::String::VA("character_%d", i + 1)).set("None");
			Dvar::Var(Utils::String::VA("character_%d_player", i + 1)).set("None");
		}
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
				Dvar::Register<int>("party_currentPlayers", 0, 0, 4, Game::DVAR_CODINFO | Game::DVAR_INIT, "Total current players in the party");
				Dvar::Register<int>("party_realPlayers", 0, 0, 4, Game::DVAR_CODINFO | Game::DVAR_INIT, "Current real players in the party");
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
		static const char* partyPrivacyValues[] = { "Public", "Private", nullptr };
		Game::Dvar_RegisterEnum("partyPrivacy", partyPrivacyValues, 0, Game::DVAR_CODINFO, "Toggle party privacy");

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
				std::string path = (*Game::fs_basepath)->current.string + "\\userraw\\scriptdata\\autosave"s;

				std::ifstream f(path);
				if (!f.is_open())
				{
					return;
				}

				{
					struct _stat64 st {};
					if (_stat64(path.c_str(), &st) == 0)
					{
						tm t{};
						localtime_s(&t, &st.st_mtime);

						char formatted[128];
						strftime(formatted, sizeof(formatted), "%d %b %Y  %H:%M", &t);

						if (!Game::Dvar_FindVar("autosave_date"))
						{
							Game::Dvar_RegisterString("autosave_date", "", 0, "");
						}

						Game::Dvar_SetString(Game::Dvar_FindVar("autosave_date"), formatted);
					}
					else
					{
						return;
					}
				}

				std::string fdata((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
				f.close();

				std::string read = fdata;
				int key = 16;

				std::string data = decryptString(read, key);

				std::unordered_map<std::string, std::string> parsed;
				size_t start = 0;

				while (true)
				{
					size_t s = data.find(';', start);
					if (s == std::string::npos) break;

					std::string t = data.substr(start, s - start);
					start = s + 1;

					size_t c = t.find(':');
					if (c == std::string::npos) continue;

					std::string k = t.substr(0, c);
					std::string v = t.substr(c + 1);

					auto trim = [](std::string& x)
						{
							while (!x.empty() && strchr(" \n\r\t", x.back())) x.pop_back();
							while (!x.empty() && strchr(" \n\r\t", x.front())) x.erase(x.begin());
						};

					trim(k);
					trim(v);

					parsed[k] = v;
				}

				if (!parsed.count("map"))
				{
					return;
				}

				for (const auto& p : parsed)
				{
					std::string dvarName = "autosave_" + p.first;

					auto var = Game::Dvar_FindVar(dvarName.c_str());
					if (!var)
					{
						var = Game::Dvar_RegisterString(dvarName.c_str(), "", 0, "");
					}

					Game::Dvar_SetString(var, p.second.c_str());
				}

				Command::Execute("openmenu popup_autosave");
			});


		UIScript::Add("LoadSaveAccepted", [](const UIScript::Token&, const Game::uiInfo_s*)
			{
				auto* map = Game::Dvar_FindVar("autosave_map");

				if (!map || !map->current.string || !map->current.string[0])
				{
					return;
				}

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
					clientXuid = std::stoull(clientInfo.get("xuid"), nullptr, 16);

					if (clientXuid != 0)
					{
						Party::g_xuidToPublicAddressMap[clientXuid] = address;

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
				const auto* password = *Game::g_password ? (*Game::g_password)->current.string : "";
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
							if (party_privacy == "1" || party_privacy == "Private")
							{
								ConnectError("The lobby you are trying to join has been set to Private.");
								return;
							}
							else if (std::stoi(client_count) >= MAX_PARTY_SLOTS)
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
						hostXuid = std::stoull(info.get("xuid"), nullptr, 16);

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

						for (int i = 1; i <= MAX_PARTY_SLOTS; ++i)
						{
							std::string charDvarName = Utils::String::VA("character_%d", i);
							std::string playerDvarName = Utils::String::VA("character_%d_player", i);

							std::string charValue = info.get(charDvarName);
							if (!charValue.empty())
							{
								Dvar::Var(charDvarName).set(charValue.c_str());
							}
							else {
								Dvar::Var(charDvarName).set("None");
							}

							std::string playerValue = info.get(playerDvarName);
							if (!playerValue.empty()) {
								Dvar::Var(playerDvarName).set(playerValue.c_str());
							}
							else {
								Dvar::Var(playerDvarName).set("None");
							}
						}

						auto securityLevel = std::strtoul(info.get("securityLevel").data(), nullptr, 10);
						bool isUsermap = !info.get("usermaphash").empty();
						auto usermapHash = std::strtoul(info.get("usermaphash").data(), nullptr, 10);
#ifdef CL_MOD_LOADING
						std::string mod = (*Game::fs_gameDirVar)->current.string;
#endif
						// set fast server stuff here so its updated when we go to download stuff
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
								// Send playlist request
								Container.requestTime = Game::Sys_Milliseconds();
								Container.awaitingPlaylist = true;
								Network::SendCommand(Container.target, "getplaylist", Dvar::Var("password").get<std::string>());

								// This is not a safe method
								// TODO: Fix actual error!
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

		Network::OnClientPacket("dvarUpdate", [](const Network::Address& address, const std::string& data)
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
				for (int i = 1; i <= MAX_PARTY_SLOTS; ++i)
				{
					std::string charDvarName = Utils::String::VA("character_%d", i);
					std::string playerDvarName = Utils::String::VA("character_%d_player", i);

					std::string charValue = info.get(charDvarName);
					if (!charValue.empty())
					{
						Dvar::Var(charDvarName).set(charValue.c_str());
					}
					else {
						Dvar::Var(charDvarName).set("None");
					}

					std::string playerValue = info.get(playerDvarName);
					if (!playerValue.empty()) {
						Dvar::Var(playerDvarName).set(playerValue.c_str());
					}
					else {
						Dvar::Var(playerDvarName).set("None");
					}
				}
			});

		if (!Dedicated::IsEnabled())
		{
			static int s_lastDvarValues[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
			static int s_lastTotalPlayers = 0;
			static int s_lastRealPlayers = 0;
			static std::string s_lastHostName = "";
			static bool s_wasHostingLastFrame = false;

			Scheduler::Loop([]()
				{
					bool needsBroadcast = false;
					static bool needsUpdatePartystate = false;
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

					if (isCurrentlyHosting) {
						std::vector<std::string> participants;
						if (Game::g_lobbyData) {
							for (int i = 0; i < MAX_PARTY_SLOTS; ++i) {
								if (Game::g_lobbyData->partyMembers[i].status != 0 && Game::g_lobbyData->partyMembers[i].gamertag && Game::g_lobbyData->partyMembers[i].gamertag[0] != '\0') {
									participants.push_back(Game::g_lobbyData->partyMembers[i].gamertag);
								}
							}
						}

						int realPlayers = participants.size();
						int botsToAdd = Dvar::Var("addBots").get<int>();
						int totalPlayers = realPlayers + std::min(botsToAdd, MAX_PARTY_SLOTS - realPlayers);

						bool dvarChanged = false;

						for (int i = 0; i < MAX_PARTY_SLOTS; ++i) {
							std::string nameDvarName = Utils::String::VA("character_%d_player", i + 1);
							if (i < participants.size()) {
								if (Dvar::Var(nameDvarName).get<std::string>() != participants[i]) {
									Dvar::Var(nameDvarName).set(participants[i].c_str());
									dvarChanged = true;
								}
							}
							else if (Dvar::Var(nameDvarName).get<std::string>().find("[BOT]") == std::string::npos && Dvar::Var(nameDvarName).get<std::string>() != "None") {
								Dvar::Var(nameDvarName).set("None");
								dvarChanged = true;
							}
						}

						if (Dvar::Var("party_realPlayers").get<int>() != realPlayers) {
							Dvar::Var("party_realPlayers").set(realPlayers);
							dvarChanged = true;
						}
						if (Dvar::Var("party_currentPlayers").get<int>() != totalPlayers) {
							Dvar::Var("party_currentPlayers").set(totalPlayers);
							dvarChanged = true;
						}

						if (s_lastRealPlayers != realPlayers || s_lastBotsToAdd != botsToAdd) {
							RandomizeCharactersForClients();
							needsBroadcast = true;
							needsUpdatePartystate = true;
						}
						s_lastRealPlayers = realPlayers;
						s_lastBotsToAdd = botsToAdd;

						if (dvarChanged) {
							needsBroadcast = true;
							needsUpdatePartystate = true;
						}

						s_lastTotalPlayers = Dvar::Var("party_currentPlayers").get<int>();
					}
					else if (!isCurrentlyHosting && s_wasHostingLastFrame) {
						Dvar::Var("party_currentPlayers").set(0);
						Dvar::Var("party_realPlayers").set(0);
						Dvar::Var("addBots").set(0);
						RandomizeCharactersForClients();
						for (int i = 0; i < MAX_PARTY_SLOTS; ++i) {
							Dvar::Var(Utils::String::VA("character_%d_player", i + 1)).set("None");
						}
						needsBroadcast = true;
						needsUpdatePartystate = true;
					}

					s_lastRealPlayers = Dvar::Var("party_realPlayers").get<int>();
					std::string currentHost = isCurrentlyHosting ? Dvar::Var("name").get<std::string>() : Dvar::Var("party_hostname").get<std::string>();
					Dvar::Var("party_currentHost").set(currentHost.c_str());
					s_lastHostName = currentHost;
					s_wasHostingLastFrame = isCurrentlyHosting;

					if (isCurrentlyHosting && !s_wasHostingLastFrame) {
						needsBroadcast = true;
					}

					if (needsBroadcast)
					{
						BroadcastDvarUpdate();
					}

					if (needsUpdatePartystate) {
						Command::Execute("xupdatepartystate");
						needsUpdatePartystate = false;
					}
				}, Scheduler::Pipeline::MAIN, 5ms);
		}
	}
}
