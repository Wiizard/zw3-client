#include "STDInclude.hpp"

#include <Utils/Compression.hpp>

#include <proto/party.pb.h>

#include "Playlist.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Party.hpp"

namespace Components
{
	std::unordered_map<const void*, std::string> Playlist::mapRelocation;
	std::string Playlist::currentPlaylistBuffer;
	std::string Playlist::receivedPlaylistBuffer;

	Utils::Hook Playlist::hooks[5];

	constexpr std::uintptr_t s_havePlaylists = 0x141BDFE64;

	constexpr std::uintptr_t Playlist_ParsePlaylists = 0x14025B2D0;

	constexpr std::uintptr_t Live_GetMapIndex = 0x140280220;

	constexpr std::uintptr_t Dvar_SetStringByName = 0x140287A70;

	constexpr std::uintptr_t Com_ParseOnLine = 0x14028B6B0;

	constexpr std::uintptr_t I_strncpyz = 0x14028C390;

	constexpr std::uintptr_t Live_Init = 0x1402A3BE0;

	constexpr std::uintptr_t Com_Init_LiveInitCall = 0x1401F57B3;

	constexpr std::uintptr_t Playlist_ParsePlaylists_ParseCall = 0x14025B363;

	constexpr std::uintptr_t Playlist_ParsePlaylists_MapNameCall = 0x14025BBA1;

	constexpr std::uintptr_t Playlist_RunRules_MapNameCall = 0x14025BFBC;

	constexpr std::uintptr_t PartyHost_MapIsAcceptable_IndexCall = 0x140110B1C;

	constexpr std::uintptr_t Com_InitDvars_PlaylistFilenameLea = 0x1401F50E1;
	constexpr std::uintptr_t playlistsPatchName = 0x14038C9E8;
	constexpr std::size_t leaRdxRipLength = 7;

	static const std::uint8_t leaRdxRip[] = { 0x48, 0x8D, 0x15 };

	constexpr std::uintptr_t LiveStorage_FetchPlaylists = 0x1402A8A40;
	constexpr std::uintptr_t Playlist_ValidatePlaylistNum = 0x14025C0B0;
	constexpr std::uintptr_t Win_LoadPlaylistFastfile = 0x1402A8DF0;

	static const std::uint8_t fetchPlaylistsEntry[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20 };
	static const std::uint8_t validatePlaylistNumEntry[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x50 };
	static const std::uint8_t loadPlaylistFastfileEntry[] = { 0x48, 0x83, 0xEC, 0x28, 0x33, 0xD2 };

	constexpr std::uintptr_t PartyHost_HandleJoinPartyRequest_TooOld = 0x14010CF93;
	constexpr std::uintptr_t PartyHost_HandleJoinPartyRequest_TooNew = 0x14010CFAE;

	static const std::uint8_t tooOldJump[] = { 0x7D, 0x0C };
	static const std::uint8_t tooNewJump[] = { 0x7E, 0x0C };

	static bool IsPartyEnabledAtInit()
	{
		const Dvar::Var partyEnable("party_enable");

		if (!partyEnable.IsValid())
		{
			return Dedicated::IsEnabled();
		}

		if (partyEnable.Get()->type == Game::DVAR_TYPE_STRING)
		{
			return std::atoi(partyEnable.Get<const char*>()) != 0;
		}

		return partyEnable.Get<bool>();
	}

	void Playlist::LoadPlaylist()
	{
		if (Utils::Hook::Get<bool>(s_havePlaylists))
		{
			return;
		}

		if (Dedicated::IsEnabled() && !IsPartyEnabledAtInit())
		{
			Utils::Hook::Set<bool>(s_havePlaylists, true);
			Dvar::Var("xblive_privateserver").Set(true);
			return;
		}

		Dvar::Var("xblive_privateserver").Set(false);

		const auto playlistFilename = Dvar::Var("playlistFilename").Get<std::string>();

		void* buffer = nullptr;
		const int length = Game::FS_ReadFile(playlistFilename.data(), &buffer);

		if (length <= 0)
		{
			if (buffer)
			{
				Game::FS_FreeFile(buffer);
			}

			Logger::Print("Unable to load playlist '{}'!\n", playlistFilename);
			return;
		}

		Logger::Print("Parsing playlist '{}'...\n", playlistFilename);

		reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Playlist_ParsePlaylists))(static_cast<const char*>(buffer));
		Game::FS_FreeFile(buffer);

		Utils::Hook::Set<bool>(s_havePlaylists, true);
	}

	void Playlist::Live_Init_Hook()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Live_Init))();

		LoadPlaylist();
	}

	char* Playlist::Com_ParseOnLine_Hook(const char** data)
	{
		mapRelocation.clear();
		currentPlaylistBuffer = Utils::Compression::ZLib::Compress(*data);

		return reinterpret_cast<char*(*)(const char**)>(Utils::Hook::Rebase(Com_ParseOnLine))(data);
	}

	void Playlist::PlaylistRequest(Network::Address& address, const std::string& data)
	{
		const auto password = Dvar::Var("g_password").Get<std::string>();

		if (!password.empty() && password != data)
		{
			Network::SendCommand(address, "playlistInvalidPassword", "");
			return;
		}

		Logger::Print("Received playlist request, sending currently stored buffer.\n");

		Proto::Party::Playlist list;
		list.set_hash(Utils::Cryptography::JenkinsOneAtATime::Compute(currentPlaylistBuffer));
		list.set_buffer(currentPlaylistBuffer);

		Network::SendCommand(address, "playlistResponse", list.SerializeAsString());
	}

	void Playlist::PlaylistResponse(Network::Address& address, const std::string& data)
	{
		if (!Party::PlaylistAwaiting())
		{
			Logger::Print("Received stray playlist response, ignoring it.\n");
			return;
		}

		if (!(address == Party::Target()))
		{
			Logger::Print("Received playlist from someone else than our target host, ignoring it.\n");
			return;
		}

		Proto::Party::Playlist list;

		if (!list.ParseFromString(data))
		{
			Party::PlaylistError(std::format("Received playlist response from {}, but it is invalid.", address.GetString()));
			receivedPlaylistBuffer.clear();
			return;
		}

		const auto& compressedData = list.buffer();
		const auto hash = Utils::Cryptography::JenkinsOneAtATime::Compute(compressedData);

		if (hash != list.hash())
		{
			Party::PlaylistError(std::format("Received playlist response from {}, but the checksum did not match ({} != {}).", address.GetString(), list.hash(), hash));
			receivedPlaylistBuffer.clear();
			return;
		}

		receivedPlaylistBuffer = Utils::Compression::ZLib::Decompress(compressedData);

		Logger::Print("Received playlist, loading and continuing connection...\n");
		reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Playlist_ParsePlaylists))(receivedPlaylistBuffer.data());
		Party::PlaylistContinue();
	}

	void Playlist::PlaylistInvalidPassword([[maybe_unused]] Network::Address& address, [[maybe_unused]] const std::string& data)
	{
		Party::PlaylistError("Error: Invalid Password for Party.");
	}

	void Playlist::MapNameCopy(char* dest, const char* src, const int destsize)
	{
		Game::I_strncpyz(dest, src, destsize);
		mapRelocation[dest] = src;
	}

	void Playlist::SetMapName(const char* dvarName, const char* value)
	{
		const auto relocated = mapRelocation.find(value);

		if (relocated != mapRelocation.end())
		{
			value = relocated->second.data();
		}

		reinterpret_cast<void(*)(const char*, const char*)>(Utils::Hook::Rebase(Dvar_SetStringByName))(dvarName, value);
	}

	int Playlist::GetMapIndex(const char* mapname)
	{
		const auto relocated = mapRelocation.find(mapname);

		if (relocated != mapRelocation.end())
		{
			mapname = relocated->second.data();
		}

		return reinterpret_cast<int(*)(const char*)>(Utils::Hook::Rebase(Live_GetMapIndex))(mapname);
	}

	Playlist::Playlist()
	{
		struct CallSite
		{
			std::uintptr_t site;
			std::uintptr_t callee;
			void* replacement;
		};

		const CallSite callSites[] =
		{
			{ Com_Init_LiveInitCall, Live_Init, reinterpret_cast<void*>(Live_Init_Hook) },
			{ Playlist_ParsePlaylists_ParseCall, Com_ParseOnLine, reinterpret_cast<void*>(Com_ParseOnLine_Hook) },
			{ Playlist_ParsePlaylists_MapNameCall, I_strncpyz, reinterpret_cast<void*>(MapNameCopy) },
			{ Playlist_RunRules_MapNameCall, Dvar_SetStringByName, reinterpret_cast<void*>(SetMapName) },
			{ PartyHost_MapIsAcceptable_IndexCall, Live_GetMapIndex, reinterpret_cast<void*>(GetMapIndex) },
		};

		static_assert(sizeof(callSites) / sizeof(callSites[0]) == sizeof(hooks) / sizeof(hooks[0]));

		for (const auto& callSite : callSites)
		{
			if (!Utils::Hook::BranchesTo(callSite.site, callSite.callee, HOOK_CALL))
			{
				Logger::Error("playlist: 0x{:X} is not the call it should be, the stock playlists stay\n", callSite.site);
				return;
			}
		}

		const bool isExpected = Utils::Hook::MatchesBytes(LiveStorage_FetchPlaylists, fetchPlaylistsEntry, sizeof(fetchPlaylistsEntry))
			&& Utils::Hook::MatchesBytes(Playlist_ValidatePlaylistNum, validatePlaylistNumEntry, sizeof(validatePlaylistNumEntry))
			&& Utils::Hook::MatchesBytes(Win_LoadPlaylistFastfile, loadPlaylistFastfileEntry, sizeof(loadPlaylistFastfileEntry))
			&& Utils::Hook::MatchesBytes(PartyHost_HandleJoinPartyRequest_TooOld, tooOldJump, sizeof(tooOldJump))
			&& Utils::Hook::MatchesBytes(PartyHost_HandleJoinPartyRequest_TooNew, tooNewJump, sizeof(tooNewJump))
			&& Utils::Hook::MatchesBytes(Com_InitDvars_PlaylistFilenameLea, leaRdxRip, sizeof(leaRdxRip));

		if (!isExpected)
		{
			Logger::Error("playlist: the playlist code does not read as expected, the stock playlists stay\n");
			return;
		}

		const auto displacement = Utils::Hook::Get<std::int32_t>(Com_InitDvars_PlaylistFilenameLea + sizeof(leaRdxRip));

		if (Com_InitDvars_PlaylistFilenameLea + leaRdxRipLength + displacement != playlistsPatchName)
		{
			Logger::Error("playlist: playlistFilename's default is not where it should be, the stock playlists stay\n");
			return;
		}

		auto* const defaultName = static_cast<char*>(Utils::Hook::AllocateDataNear(Com_InitDvars_PlaylistFilenameLea, 32));

		if (!defaultName)
		{
			Logger::Error("playlist: no room beside the image for the playlist name, the stock playlists stay\n");
			return;
		}

		std::strcpy(defaultName, "data/playlists_default.info");

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(callSites); ++i)
		{
			isSeated = hooks[i].Initialize(callSites[i].site, callSites[i].replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("playlist: could not seat every hook, the stock playlists stay\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		const auto from = Utils::Hook::Rebase(Com_InitDvars_PlaylistFilenameLea) + leaRdxRipLength;
		Utils::Hook::Set<std::int32_t>(Com_InitDvars_PlaylistFilenameLea + sizeof(leaRdxRip),
			static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(defaultName) - from));

		Utils::Hook::Set<std::uint8_t>(LiveStorage_FetchPlaylists, 0xC3);
		Utils::Hook::Set<std::uint8_t>(Playlist_ValidatePlaylistNum, 0xC3);
		Utils::Hook::Set<std::uint8_t>(Win_LoadPlaylistFastfile, 0xC3);

		Utils::Hook::Set<std::uint8_t>(PartyHost_HandleJoinPartyRequest_TooOld, 0xEB);
		Utils::Hook::Set<std::uint8_t>(PartyHost_HandleJoinPartyRequest_TooNew, 0xEB);

		Network::OnPacket("getPlaylist", PlaylistRequest);
		Network::OnPacket("playlistResponse", PlaylistResponse);
		Network::OnPacket("playlistInvalidPassword", PlaylistInvalidPassword);
	}
}
