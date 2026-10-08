#include "STDInclude.hpp"

#include "Maps.hpp"
#include "ArenaLength.hpp"
#include "AssetHandler.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "RawFiles.hpp"
#include "Scheduler.hpp"
#include "SPLoadscreens.hpp"
#include "StartupMessages.hpp"
#include "Theatre.hpp"
#include "UIScript.hpp"
#include "Zones.hpp"

#include "GSC/Script.hpp"

#include "Utils/MapPreview.hpp"

namespace Components
{
	constexpr std::uintptr_t DB_SyncXAssets = 0x14012F990;
	constexpr std::uintptr_t SND_StopSounds = 0x14024A2E0;
	constexpr std::uintptr_t Sys_WaitForStream = 0x14020B260;
	constexpr auto streamCriticalSection = static_cast<Game::CriticalSection>(6);

	constexpr std::uintptr_t fsh = 0x146644BA0;
	constexpr int fileHandleCount = 64;
	constexpr std::size_t fileHandleSize = 0x130;
	constexpr std::size_t fileHandleFileSize = 0x14;
	constexpr std::size_t fileHandleIwd = 0x20;

	Maps::UserMapContainer Maps::userMap;

	struct UserMapFileStamp
	{
		std::uintmax_t size{};
		std::filesystem::file_time_type modified{};

		bool operator==(const UserMapFileStamp&) const = default;
	};

	struct UserMapFileHash
	{
		UserMapFileStamp stamp;
		std::string hash;
	};

	static std::unordered_map<std::string, UserMapFileHash> userMapHashCache;

	static std::optional<UserMapFileStamp> TryGetUserMapFileStamp(const std::filesystem::path& path)
	{
		std::error_code error;

		const auto size = std::filesystem::file_size(path, error);

		if (error)
		{
			return std::nullopt;
		}

		const auto modified = std::filesystem::last_write_time(path, error);

		if (error)
		{
			return std::nullopt;
		}

		return UserMapFileStamp{ size, modified };
	}

	static std::string HashUserMapFile(const std::filesystem::path& path)
	{
		hash_state state{};
		sha256_init(&state);

		const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file != INVALID_HANDLE_VALUE)
		{
			std::vector<unsigned char> buffer(1024 * 1024);
			DWORD bytesRead = 0;

			while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0)
			{
				sha256_process(&state, buffer.data(), bytesRead);
			}

			CloseHandle(file);
		}

		std::array<unsigned char, 32> digest{};
		sha256_done(&state, digest.data());

		return { reinterpret_cast<const char*>(digest.data()), digest.size() };
	}
	std::vector<Maps::DLC> Maps::dlcPacks;
	std::string Maps::currentMainZone;
	std::vector<std::string> Maps::currentDependencies;
	std::vector<std::string> Maps::foundCustomMaps;

	const char* Maps::userMapFiles[4] =
	{
		".ff",
		"_load.ff",
		".iwd",
		".arena",
	};

	constexpr std::uintptr_t DB_LoadLevelXAssets_MapZoneCall = 0x14012EC0A;
	constexpr std::uintptr_t DB_LoadXAssets = 0x14012EC40;

	constexpr std::uintptr_t Com_GetBspFilename_SprintfJump = 0x14027483D;
	constexpr std::uintptr_t Com_sprintf = 0x14028BEB0;

	constexpr std::uintptr_t G_SpawnEntitiesFromString_IsDynClassnameCall = 0x1401A9294;
	constexpr std::uintptr_t IsDynClassname = 0x14028CBF0;

	constexpr std::uintptr_t updateArenas = 0x1465D284C;

	constexpr std::uintptr_t loadscreenZoneCalls[] = { 0x1400FE325, 0x1400FA08D, 0x1400FA379, 0x1400FDF43, 0x14023B318 };

	constexpr std::uintptr_t Com_AssetLoadUI_FreeLevelCall = 0x1401F36AA;

	constexpr std::uintptr_t UI_LoadArenas_ReadRawFileCall = 0x14026B4E6;
	constexpr std::uintptr_t DB_ReadRawFile = 0x14012F370;

	constexpr std::uintptr_t G_SpawnTurret = 0x14018F750;
	constexpr std::uintptr_t G_SpawnTurretCalls[] = { 0x14017A02E, 0x14018FD5F };

	constexpr std::uintptr_t SV_SetTriggerModel_BoundsCall = 0x140233AC4;
	constexpr std::uintptr_t CM_TriggerModelBounds = 0x1401E85C0;
	constexpr std::uintptr_t cm_mapEnts = 0x141BD32B0;

	constexpr std::uintptr_t G_InitGame_InitGlassCall = 0x14019D085;
	constexpr std::uintptr_t G_InitGlass = 0x140169720;
	constexpr std::uintptr_t G_InitGlass_GlassSource = 0x141780CD8;
	constexpr std::size_t gameWorldMpName = 8;
	constexpr std::size_t gameWorldSpGlassData = 104;
	static std::atomic<bool> isSPMap;

	constexpr std::uintptr_t staticModelSortJnz = 0x14005383A;
	static const std::uint8_t sortCountJnz[] = { 0x75, 0x07 };

	constexpr std::uintptr_t PMem_Init_SizeArg = 0x14028AB46;
	constexpr std::uintptr_t PMem_Init_SizeStore = 0x14028AB91;
	static const std::uint8_t sizeArg[] = { 0xBA, 0x00, 0x00, 0xC0, 0x12 };
	static const std::uint8_t sizeStore[] = { 0xC7, 0x05, 0xE9, 0xD1, 0x47, 0x06, 0x00, 0x00, 0xC0, 0x12 };
	constexpr std::uint32_t hunkSize = 0x40000000;

	static Utils::Hook hooks[3 + std::size(loadscreenZoneCalls) + 2 + std::size(G_SpawnTurretCalls) + 2];

	constexpr std::uintptr_t CL_ConnectionlessPacket_NewMapBody = 0x1400F9FCF;
	static const std::uint8_t newMapBody[] = { 0x33, 0xC9, 0xC6, 0x05, 0xD0, 0x41, 0x5D, 0x00, 0x01 };
	constexpr std::uintptr_t CL_ConnectionlessPacket_NewMapBodyNext = 0x1400F9FD8;
	constexpr std::uintptr_t CL_ConnectionlessPacket_ReturnTrue = 0x1400F9713;
	constexpr std::uintptr_t newMapFlag = 0x1406CE1A8;

	static Utils::Hook newMapHook;

	constexpr std::uintptr_t SV_SpawnServer_LoadingNewMapCall = 0x14023B3E0;

	static Utils::Hook loadNewMapHook;

	extern "C"
	{
		void LoadingNewMapStub();

		std::uintptr_t Maps_NewMapFlag = 0;
		std::uintptr_t Maps_NewMapBodyNext = 0;
		std::uintptr_t Maps_NewMapReturnTrue = 0;

		int Maps_TriggerReconnectForMap(Game::msg_t* msg, const char* mapname)
		{
			return Maps::TriggerReconnectForMap(msg, mapname);
		}
	}

	static Game::newMapArena_t* GetArenas()
	{
		return ArenaLength::newArenas;
	}

	Maps::UserMapContainer* Maps::GetUserMap()
	{
		return &userMap;
	}

	unsigned int Maps::UserMapContainer::GetHash()
	{
		if (!this->isHashComputed && this->IsValid())
		{
			this->hash = Maps::GetUsermapHash(this->mapname);
			this->isHashComputed = true;
		}

		return this->hash;
	}

	void Maps::UserMapContainer::LoadIwd()
	{
		if (this->IsValid() && !this->searchPath.iwd)
		{
			auto iwdName = std::format("{}.iwd", this->mapname);
			auto path = std::format("{}\\usermaps\\{}\\{}", (*Game::fs_basepath)->current.string, this->mapname, iwdName);

			if (Utils::IO::FileExists(path))
			{
				this->searchPath.iwd = Game::FS_LoadZipFile(path.data(), iwdName.data());

				if (this->searchPath.iwd)
				{
					this->searchPath.bLocalized = false;
					this->searchPath.ignore = 0;
					this->searchPath.ignorePureCheck = 0;
					this->searchPath.language = 0;
					this->searchPath.dir = nullptr;
					this->searchPath.next = *Game::fs_searchpaths;
					*Game::fs_searchpaths = &this->searchPath;
				}
			}
		}
	}

	void Maps::UserMapContainer::ReloadIwd()
	{
		if (this->IsValid() && this->wasFreed)
		{
			this->wasFreed = false;
			this->searchPath.iwd = nullptr;
			this->LoadIwd();
		}
	}

	void Maps::UserMapContainer::HandlePackfile(void* packfile)
	{
		if (this->IsValid() && this->searchPath.iwd == packfile)
		{
			this->wasFreed = true;
		}
	}

	static void DetachFileHandles(const Game::iwd_t* iwd)
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(DB_SyncXAssets))();
		reinterpret_cast<void(*)(char)>(Utils::Hook::Rebase(SND_StopSounds))(8);
		reinterpret_cast<unsigned long(*)(int)>(Utils::Hook::Rebase(Sys_WaitForStream))(0);

		Game::Sys_EnterCriticalSection(streamCriticalSection);

		for (int handle = 1; handle < fileHandleCount; ++handle)
		{
			auto* const entry = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(fsh)) + handle * fileHandleSize;
			auto* const handleIwd = reinterpret_cast<const Game::iwd_t**>(entry + fileHandleIwd);

			if (*handleIwd != iwd)
			{
				continue;
			}

			if (*reinterpret_cast<const int*>(entry + fileHandleFileSize))
			{
				Game::FS_FCloseFile(handle);
			}
			else
			{
				*handleIwd = nullptr;
			}
		}

		Game::Sys_LeaveCriticalSection(streamCriticalSection);
	}

	void Maps::UserMapContainer::FreeIwd()
	{
		if (this->IsValid() && this->searchPath.iwd && !this->wasFreed)
		{
			this->wasFreed = true;

			for (auto** pathPtr = Game::fs_searchpaths; *pathPtr; pathPtr = &(*pathPtr)->next)
			{
				if (*pathPtr == &this->searchPath)
				{
					*pathPtr = (*pathPtr)->next;
					break;
				}
			}

			DetachFileHandles(this->searchPath.iwd);

			Game::unzClose(this->searchPath.iwd->handle);

			Game::Z_FreeInternal(this->searchPath.iwd->buildBuffer);
			Game::Z_FreeInternal(this->searchPath.iwd);

			ZeroMemory(&this->searchPath, sizeof(this->searchPath));
		}
	}

	const char* Maps::LoadArenaFileStub(const char* name, char* buffer, int size)
	{
		std::string data;

		if (userMap.IsValid())
		{
			const auto mapname = userMap.GetName();
			const auto arena = GetArenaPath(mapname);

			if (Utils::IO::FileExists(arena))
			{
				data = Utils::IO::ReadFile(arena);
			}
		}
		else
		{
			const auto* raw = RawFiles::ReadRawFile(name, buffer, size);

			if (raw)
			{
				data = raw;
			}
		}

		strncpy_s(buffer, size, data.data(), _TRUNCATE);
		return buffer;
	}

	void Maps::UnloadMapZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		Game::DB_LoadXAssets(zoneInfo, zoneCount, sync);

		if (userMap.IsValid())
		{
			userMap.FreeIwd();
			userMap.Clear();
		}
	}

	void Maps::LoadMapZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		isSPMap = false;
		currentMainZone = zoneInfo->name;
		currentDependencies.clear();

		auto dependencies = GetDependenciesForMap(zoneInfo->name);

		std::vector<Game::XZoneInfo> data(zoneInfo, zoneInfo + zoneCount);
		Utils::Memory::Allocator allocator;

		if (dependencies.requiresTeamZones)
		{
			auto teams = dependencies.requiredTeams;

			Game::XZoneInfo team;
			team.allocFlags = zoneInfo->allocFlags;
			team.freeFlags = zoneInfo->freeFlags;

			team.name = allocator.DuplicateString(std::format("iw4x_team_{}", teams.first));
			data.push_back(team);

			team.name = allocator.DuplicateString(std::format("iw4x_team_{}", teams.second));
			data.push_back(team);
		}

		currentDependencies.insert(currentDependencies.end(), dependencies.requiredMaps.begin(), dependencies.requiredMaps.end());

		for (const auto& dependency : currentDependencies)
		{
			Game::XZoneInfo info;

			info.name = dependency.data();
			info.allocFlags = zoneInfo->allocFlags;
			info.freeFlags = zoneInfo->freeFlags;

			data.push_back(info);
		}

		const auto patchZone = std::format("patch_{}", zoneInfo->name);

		if (FastFiles::Exists(patchZone))
		{
			data.push_back({ patchZone.data(), zoneInfo->allocFlags, zoneInfo->freeFlags });
		}

		FastFiles::LoadLocalizeZones(data.data(), static_cast<unsigned int>(data.size()), sync);
	}

	constexpr std::uint32_t iw4xFirstVersion = 316;
	constexpr std::uint32_t iw4xBrokenMenusVersion = 359;

	void Maps::LoadAssetRestrict(unsigned int type, void* asset, const std::string& name, bool* restrict)
	{
		bool isDependencyWorldAsset = false;

		switch (type)
		{
		case Game::ASSET_TYPE_CLIPMAP_MP:
		case Game::ASSET_TYPE_CLIPMAP_SP:
		case Game::ASSET_TYPE_GAMEWORLD_SP:
		case Game::ASSET_TYPE_GAMEWORLD_MP:
		case Game::ASSET_TYPE_GFXWORLD:
		case Game::ASSET_TYPE_MAP_ENTS:
		case Game::ASSET_TYPE_COMWORLD:
		case Game::ASSET_TYPE_FXWORLD:
			isDependencyWorldAsset = true;
			break;
		default:
			break;
		}

		if (isDependencyWorldAsset && !currentDependencies.empty())
		{
			const auto currentZone = FastFiles::Current();

			if (std::find(currentDependencies.begin(), currentDependencies.end(), currentZone) != currentDependencies.end())
			{
				*restrict = true;
				return;
			}
		}

		if (type == Game::ASSET_TYPE_ADDON_MAP_ENTS)
		{
			*restrict = true;
			return;
		}

		if (type == Game::ASSET_TYPE_WEAPON)
		{
			if ((!std::strstr(name.data(), "_mp") && name != "none" && name != "destructible_car") || Zones::Version() >= iw4xFirstVersion)
			{
				*restrict = true;
				return;
			}
		}

		if ((type == Game::ASSET_TYPE_MENU || type == Game::ASSET_TYPE_MENULIST) && Zones::Version() >= iw4xBrokenMenusVersion)
		{
			*restrict = true;
			return;
		}

		if (type == Game::ASSET_TYPE_STRINGTABLE)
		{
			if (FastFiles::Current() == "mp_cross_fire")
			{
				*restrict = true;
				return;
			}
		}

		if (type == Game::ASSET_TYPE_MAP_ENTS)
		{
			std::string entitiesFilename = name;

			if (entitiesFilename.starts_with("maps/mp/maps/mp") && entitiesFilename.ends_with(".d3dbsp.d3dbsp"))
			{
				const auto prefixLength = "maps/mp/"s.size();
				const auto suffixLength = ".d3dbsp"s.size();
				entitiesFilename = entitiesFilename.substr(prefixLength, entitiesFilename.size() - prefixLength - suffixLength);
			}

			static std::string mapEntities;
			FileSystem::File ents(entitiesFilename + ".ents", Game::FS_THREAD_DATABASE);

			if (ents.Exists())
			{
				auto* mapEnts = static_cast<Game::MapEnts*>(asset);

				mapEntities = ents.GetBuffer();
				mapEnts->entityString = mapEntities.data();
				mapEnts->numEntityChars = static_cast<int>(mapEntities.size()) + 1;
			}
		}
	}

	void Maps::GetBSPName(char* buffer, size_t size, const char* format, const char* mapname)
	{
		if (!Utils::String::StartsWith(mapname, "mp_") && !Utils::String::StartsWith(mapname, "zm_") && !IsUserMap(mapname))
		{
			format = "maps/%s.d3dbsp";
		}

		_snprintf_s(buffer, size, _TRUNCATE, format, mapname);
	}

	void Maps::LoadNewMapCommand(char* buffer, int size, const char*, const char* mapname, const char* gametype)
	{
		const auto hash = GetUsermapHash(mapname);
		_snprintf_s(buffer, static_cast<std::size_t>(size), _TRUNCATE, "loadingnewmap\n%s\n%s\n%d", mapname, gametype, static_cast<int>(hash));
	}

	int Maps::IgnoreEntityStub(const char* entity)
	{
		return (Utils::String::StartsWith(entity, "dyn_") || Utils::String::StartsWith(entity, "node_") || Utils::String::StartsWith(entity, "actor_"));
	}

	void Maps::ForceRefreshArenas()
	{
		if (!FastFiles::Ready())
		{
			Logger::Print("Not refreshing arenas (fastfiles are not ready yet)\n");
			return;
		}

		if (Game::Sys_IsMainThread())
		{
			if (*Game::g_quitRequested)
			{
				return;
			}

			Utils::Hook::Set<int>(updateArenas, 1);
			Game::UI_UpdateArenas();
		}
		else
		{
			assert(false && "Arenas refreshed from wrong thread??");
			Logger::Print("Tried to refresh arenas from a thread that is NOT the main thread!! This could have lead to a crash. Please report this bug and how you got it!");
		}
	}

	std::unordered_map<std::string, std::string> Maps::ParseCustomMapArena(const std::string& singleMapArena)
	{
		static const std::regex regex("  (\\w*) *\"?((?:\\w| )*)\"?");
		std::unordered_map<std::string, std::string> arena;

		std::smatch m;

		std::string::const_iterator searchStart(singleMapArena.cbegin());

		while (std::regex_search(searchStart, singleMapArena.cend(), m, regex))
		{
			if (m.size() > 2)
			{
				arena.emplace(m[1].str(), m[2].str());
				searchStart = m.suffix().first;
			}
		}

		return arena;
	}

	constexpr std::size_t maxMapIdLength = 64;

	static bool IsPlainMapId(const std::string& mapName)
	{
		if (mapName.empty() || mapName.size() > maxMapIdLength)
		{
			return false;
		}

		return mapName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos;
	}

	void Maps::SynchronizeMapDvars(const std::string& rawMapName)
	{
		if (!IsPlainMapId(rawMapName) || rawMapName == "none" || rawMapName == "None")
		{
			return;
		}

		const std::string normalizedMap = Utils::MapPreview::Normalize(rawMapName);
		const char* const localizedName = Localization::LocalizeMapName(normalizedMap.data());
		std::string displayName = normalizedMap;

		if (localizedName[0])
		{
			displayName = localizedName;
		}

		if (displayName == normalizedMap)
		{
			const char* const rawLocalizedName = Localization::LocalizeMapName(rawMapName.data());

			if (rawLocalizedName[0])
			{
				displayName = rawLocalizedName;
			}
		}

		Dvar::Var("ui_mapname").Set(rawMapName);
		Dvar::Var("party_mapname").Set(displayName);
		Dvar::Var("zw3_leaderboard_map").Set(rawMapName);
		Dvar::Var("zw3_leaderboard_mapname_display").Set(displayName);
		Dvar::Var("uiDisplayMapName").Set(displayName);
	}

	void Maps::PrepareUsermap(const char* mapname)
	{
		if (!mapname || !*mapname)
		{
			return;
		}

		SynchronizeMapDvars(mapname);
		SPLoadscreens::SetLoadingMap(mapname);

		if (userMap.IsValid() && userMap.GetName() == mapname)
		{
			return;
		}

		if (userMap.IsValid())
		{
			userMap.FreeIwd();
			userMap.Clear();
		}

		if (IsUserMap(mapname))
		{
			userMap = UserMapContainer(mapname);
			userMap.LoadIwd();

			SPLoadscreens::SetLoadingMap(mapname);
		}
		else
		{
			userMap.Clear();
		}
	}

	void Maps::LoadLoadscreenZone_Stub(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		std::string mapname = zoneInfo->name;
		mapname.resize(mapname.size() - "_load"s.size());

		PrepareUsermap(mapname.data());

		Game::DB_LoadXAssets(zoneInfo, zoneCount, sync);
	}

	unsigned int Maps::GetUsermapHash(const std::string& map)
	{
		const auto mapDirectory = std::filesystem::path("usermaps") / map;

		if (Utils::IO::DirectoryExists(mapDirectory))
		{
			std::string hash;
			hash.reserve(std::size(userMapFiles) * 32);

			for (std::size_t i = 0; i < std::size(userMapFiles); ++i)
			{
				const auto filePath = mapDirectory / std::format("{}{}", map, userMapFiles[i]);

				if (!Utils::IO::FileExists(filePath.string()))
				{
					continue;
				}

				const auto stamp = TryGetUserMapFileStamp(filePath);
				const auto cacheKey = Utils::String::ToLower(filePath.lexically_normal().generic_string());
				const auto cached = userMapHashCache.find(cacheKey);

				if (stamp && cached != userMapHashCache.end() && cached->second.stamp == *stamp)
				{
					hash.append(cached->second.hash);
					continue;
				}

				auto fileHash = HashUserMapFile(filePath);
				hash.append(fileHash);

				if (stamp)
				{
					userMapHashCache.insert_or_assign(cacheKey, UserMapFileHash{ *stamp, std::move(fileHash) });
				}
			}

			return Utils::Cryptography::JenkinsOneAtATime::Compute(hash);
		}

		return 0;
	}

	bool Maps::IsUserMap(const std::string& mapname)
	{
		return Utils::IO::DirectoryExists(std::format("usermaps/{}", mapname)) && Utils::IO::FileExists(std::format("usermaps/{}/{}.ff", mapname, mapname));
	}

	bool Maps::CheckMapInstalled(const std::string& mapname, bool error, bool dlcIsTrue)
	{
		if (FastFiles::Exists(mapname))
		{
			return true;
		}

		for (const auto& pack : dlcPacks)
		{
			for (const auto& map : pack.maps)
			{
				if (map != mapname)
				{
					continue;
				}

				if (error)
				{
					Game::Com_Error(Game::ERR_DISCONNECT, "%s", Utils::String::Format("Missing DLC pack {} ({}) containing map {} ({}).\nPlease download it to play this map.",
						pack.name, pack.index, Localization::LocalizeMapName(mapname.data()), mapname));
				}

				return dlcIsTrue;
			}
		}

		if (error)
		{
			Game::Com_Error(Game::ERR_DISCONNECT, "%s", Utils::String::Format("Missing map file {}.\nYou may have a damaged installation or are attempting to load a non-existent map.", mapname));
		}

		return false;
	}

	int Maps::TriggerReconnectForMap(Game::msg_t* msg, const char* mapname)
	{
		Theatre::StopRecording();

		char hashBuf[100] = { 0 };
		const auto hash = static_cast<unsigned int>(std::atoi(Game::MSG_ReadStringLine(msg, hashBuf, sizeof(hashBuf))));

		if (!CheckMapInstalled(mapname, false, true) || (hash && hash != GetUsermapHash(mapname)))
		{
			Command::Execute("disconnect", false);
			Command::Execute("awaitDatabase", false);
			Command::Execute("wait 100", false);
			Command::Execute("openmenu popup_reconnectingtoparty", false);
			Command::Execute("delayReconnect", false);
			return true;
		}

		return false;
	}

	void Maps::ScanCustomMaps()
	{
		foundCustomMaps.clear();
		Logger::Print("Looking for custom maps...\n");

		std::filesystem::path basePath = (*Game::fs_basepath)->current.string;
		basePath /= "usermaps";

		if (!std::filesystem::exists(basePath))
		{
			return;
		}

		const auto entries = Utils::IO::ListFiles(basePath);

		for (const auto& entry : entries)
		{
			if (entry.is_directory())
			{
				const auto zoneName = entry.path().filename().string();
				const auto mapPath = std::format("{}\\{}.ff", entry.path().string(), zoneName);

				if (Utils::IO::FileExists(mapPath))
				{
					foundCustomMaps.push_back(zoneName);
					Logger::Print("Discovered custom map {}\n", zoneName);
				}
			}
		}
	}

	std::string Maps::GetArenaPath(const std::string& mapName)
	{
		return std::format("usermaps/{}/{}.arena", mapName, mapName);
	}

	const std::vector<std::string>& Maps::GetCustomMaps()
	{
		return foundCustomMaps;
	}

	std::vector<Maps::DLC> Maps::StockDlcs()
	{
		return
		{
			{ 1, "Stimulus Pack", {"mp_complex", "mp_compact", "mp_storm", "mp_overgrown", "mp_crash"} },
			{ 2, "Resurgence Pack", {"mp_abandon", "mp_vacant", "mp_trailerpark", "mp_strike", "mp_fuel2"} },
			{ 3, "IW4x Classics", {"mp_nuked", "mp_cross_fire", "mp_cargoship", "mp_bloc", "mp_killhouse", "mp_bog_sh", "mp_cargoship_sh", "mp_shipment", "mp_shipment_long", "mp_rust_long", "mp_firingrange", "mp_bloc_sh", "mp_crash_tropical", "mp_estate_tropical", "mp_fav_tropical", "mp_storm_spring"} },
			{ 4, "Call Of Duty 4 Pack", {"mp_farm", "mp_backlot", "mp_pipeline", "mp_countdown", "mp_crash_snow", "mp_carentan", "mp_broadcast", "mp_showdown", "mp_convoy", "mp_citystreets"} },
			{ 5, "Modern Warfare 3 Pack", {"mp_dome", "mp_hardhat", "mp_paris", "mp_seatown", "mp_bravo", "mp_underground", "mp_plaza2", "mp_village", "mp_alpha"} },
		};
	}

	bool Maps::IsDlcInstalled(int index)
	{
		for (const auto& pack : StockDlcs())
		{
			if (pack.index != index)
			{
				continue;
			}

			for (const auto& map : pack.maps)
			{
				if (!FastFiles::Exists(map))
				{
					return false;
				}
			}

			return true;
		}

		return false;
	}

	void Maps::AddDlc(DLC dlc)
	{
		for (auto& pack : dlcPacks)
		{
			if (pack.index == dlc.index)
			{
				pack.maps = dlc.maps;
				UpdateDlcStatus();
				return;
			}
		}

		Dvar::Register(Utils::String::VA("isDlcInstalled_%d", dlc.index), false, Game::DVAR_EXTERNAL | Game::DVAR_INIT, "");

		dlcPacks.push_back(dlc);
		UpdateDlcStatus();
	}

	void Maps::UpdateDlcStatus()
	{
		bool hasAllDlcs = true;
		std::vector<bool> hasDlc;

		for (auto& pack : dlcPacks)
		{
			bool hasAllMaps = true;

			for (const auto& map : pack.maps)
			{
				if (!FastFiles::Exists(map))
				{
					hasAllMaps = false;
					hasAllDlcs = false;
					break;
				}
			}

			hasDlc.push_back(hasAllMaps);
			Dvar::Var(Utils::String::VA("isDlcInstalled_%d", pack.index)).Set(hasAllMaps);
		}

		static bool didWarnPartialDlc = false;

		if (hasDlc.size() >= 5 && !didWarnPartialDlc)
		{
			const bool hasAnyOfThreeToFive = hasDlc[2] || hasDlc[3] || hasDlc[4];
			const bool hasAllOfThreeToFive = hasDlc[2] && hasDlc[3] && hasDlc[4];

			if (hasAnyOfThreeToFive && !hasAllOfThreeToFive)
			{
				StartupMessages::AddMessage("Warning:\n You only have some of DLCs 3-5 which are all required to be installed to work. There may be issues with those maps.");
				didWarnPartialDlc = true;
			}
		}

		Dvar::Var("isDlcInstalled_All").Set(hasAllDlcs);
	}

	void Maps::G_SpawnTurretHook(Game::gentity_s* ent, const char* weaponInfoName, int scriptSpawned)
	{
		if (currentMainZone == "mp_backlot_sh"s || currentMainZone == "mp_con_spring"s ||
			currentMainZone == "mp_mogadishu_sh"s || currentMainZone == "mp_nightshift_sh"s)
		{
			return;
		}

		reinterpret_cast<void(*)(Game::gentity_s*, const char*, int)>(Utils::Hook::Rebase(G_SpawnTurret))(ent, weaponInfoName, scriptSpawned);
	}

	void Maps::HandleAsSPMap()
	{
		isSPMap = true;
	}

	void Maps::G_InitGlass_Hk()
	{
		const auto glassInit = reinterpret_cast<void(*)()>(Utils::Hook::Rebase(G_InitGlass));
		auto* const glassSource = reinterpret_cast<void**>(Utils::Hook::Rebase(G_InitGlass_GlassSource));
		const auto* const mpName = *reinterpret_cast<const char**>(Utils::Hook::Rebase(G_InitGlass_GlassSource - gameWorldMpName));
		auto* const spWorld = static_cast<std::uint8_t*>(Game::DB_XAssetPool[Game::ASSET_TYPE_GAMEWORLD_SP]);
		const bool usesSpWorld = isSPMap || !mpName || !*glassSource;

		if (!usesSpWorld || !spWorld)
		{
			glassInit();
			return;
		}

		void* const saved = *glassSource;
		*glassSource = *reinterpret_cast<void**>(spWorld + gameWorldSpGlassData);
		glassInit();
		*glassSource = saved;
	}

	unsigned short Maps::CM_TriggerModelBounds_Hk(unsigned int triggerIndex, Game::Bounds* bounds)
	{
		auto* ents = Utils::Hook::Get<Game::MapEnts*>(cm_mapEnts);

		if (ents)
		{
			if (triggerIndex >= ents->trigger.count)
			{
				Logger::Fatal("Invalid trigger index ({}) in entities exceeds the maximum trigger count ({}) defined in the clipmap. Check your map ents, or your clipmap!", triggerIndex, ents->trigger.count);
			}

			return reinterpret_cast<unsigned short(*)(unsigned int, Game::Bounds*)>(Utils::Hook::Rebase(CM_TriggerModelBounds))(triggerIndex, bounds);
		}

		return 0;
	}

	Maps::MapDependencies Maps::GetDependenciesForMap(const std::string& map)
	{
		std::string teamAxis = "opforce_composite";
		std::string teamAllies = "us_army";

		Maps::MapDependencies dependencies;

		dependencies.requiresTeamZones = true;

		const auto* arenas = GetArenas();

		for (int i = 0; arenas && i < *Game::arenaCount; ++i)
		{
			const Game::newMapArena_t* arena = &arenas[i];

			if (arena->mapName == map)
			{
				dependencies.requiresTeamZones = false;

				for (std::size_t j = 0; j < std::extent_v<decltype(Game::newMapArena_t::keys)>; ++j)
				{
					const auto* key = arena->keys[j];
					const auto* value = arena->values[j];

					if (key == "dependency"s)
					{
						dependencies.requiredMaps = Utils::String::Split(arena->values[j], ' ');
					}
					else if (key == "allieschar"s)
					{
						teamAllies = value;
					}
					else if (key == "axischar"s)
					{
						teamAxis = value;
					}
					else if (key == "useteamzones"s)
					{
						dependencies.requiresTeamZones = Utils::String::ToLower(value) == "true"s;
					}
				}

				break;
			}
		}

		dependencies.requiredTeams = std::make_pair(teamAllies, teamAxis);

		return dependencies;
	}

	void Maps::GSCr_GetMapArenaInfo()
	{
		if (Game::Scr_GetNumParam() != 2)
		{
			GSC::Script::Scr_Error("GetMapArenaInfo: Needs a map name and a field name!");
			return;
		}

		const auto* mapName = Game::SL_ConvertToString(Game::Scr_GetConstString(0));
		const auto* fieldName = Game::SL_ConvertToString(Game::Scr_GetConstString(1));

		Game::UI_UpdateArenas();

		const auto* arenas = GetArenas();

		for (int i = 0; arenas && i < *Game::arenaCount; ++i)
		{
			const Game::newMapArena_t* arena = &arenas[i];

			if (std::strcmp(arena->mapName, mapName) == 0)
			{
				for (std::size_t j = 0; j < std::extent_v<decltype(Game::newMapArena_t::keys)>; ++j)
				{
					const auto* key = arena->keys[j];
					const auto* value = arena->values[j];

					if (std::strcmp(key, fieldName) == 0)
					{
						Game::Scr_AddString(value);
						return;
					}
				}

				GSC::Script::Scr_Error(Utils::String::VA("Could not find field %s in arena entry for map %s!\n", fieldName, mapName));
				return;
			}
		}

		GSC::Script::Scr_Error(Utils::String::VA("Map %s has no arena entry!\n", mapName));
	}

	void Maps::GSCr_GetMapList()
	{
		Game::Scr_MakeArray();

		Game::UI_UpdateArenas();

		const auto* arenas = GetArenas();

		for (int i = 0; arenas && i < *Game::arenaCount; ++i)
		{
			const Game::newMapArena_t* arena = &arenas[i];
			Game::Scr_AddString(arena->mapName);
			Game::Scr_AddArray();
		}
	}

	Maps::Maps()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
			bool isJump;
		};

		const HookSite sites[] =
		{
			{ DB_LoadLevelXAssets_MapZoneCall, DB_LoadXAssets, reinterpret_cast<void*>(LoadMapZones), HOOK_CALL },
			{ Com_GetBspFilename_SprintfJump, Com_sprintf, reinterpret_cast<void*>(GetBSPName), HOOK_JUMP },
			{ G_SpawnEntitiesFromString_IsDynClassnameCall, IsDynClassname, reinterpret_cast<void*>(IgnoreEntityStub), HOOK_CALL },
			{ loadscreenZoneCalls[0], DB_LoadXAssets, reinterpret_cast<void*>(LoadLoadscreenZone_Stub), HOOK_CALL },
			{ loadscreenZoneCalls[1], DB_LoadXAssets, reinterpret_cast<void*>(LoadLoadscreenZone_Stub), HOOK_CALL },
			{ loadscreenZoneCalls[2], DB_LoadXAssets, reinterpret_cast<void*>(LoadLoadscreenZone_Stub), HOOK_CALL },
			{ loadscreenZoneCalls[3], DB_LoadXAssets, reinterpret_cast<void*>(LoadLoadscreenZone_Stub), HOOK_CALL },
			{ loadscreenZoneCalls[4], DB_LoadXAssets, reinterpret_cast<void*>(LoadLoadscreenZone_Stub), HOOK_CALL },
			{ Com_AssetLoadUI_FreeLevelCall, DB_LoadXAssets, reinterpret_cast<void*>(UnloadMapZones), HOOK_CALL },
			{ UI_LoadArenas_ReadRawFileCall, DB_ReadRawFile, reinterpret_cast<void*>(LoadArenaFileStub), HOOK_CALL },
			{ G_SpawnTurretCalls[0], G_SpawnTurret, reinterpret_cast<void*>(G_SpawnTurretHook), HOOK_CALL },
			{ G_SpawnTurretCalls[1], G_SpawnTurret, reinterpret_cast<void*>(G_SpawnTurretHook), HOOK_CALL },
			{ SV_SetTriggerModel_BoundsCall, CM_TriggerModelBounds, reinterpret_cast<void*>(CM_TriggerModelBounds_Hk), HOOK_CALL },
			{ G_InitGame_InitGlassCall, G_InitGlass, reinterpret_cast<void*>(G_InitGlass_Hk), HOOK_CALL },
		};

		static_assert(std::size(sites) == std::size(hooks));

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, hookSite.isJump))
			{
				Logger::Error("maps: 0x{:X} no longer reaches 0x{:X}, maps load as the engine loads them\n", hookSite.site, hookSite.target);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, sites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("maps: could not seat every hook, maps load as the engine loads them\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		Command::Add("delayReconnect", []
		{
			Scheduler::Once([]
			{
				Command::Execute("closemenu popup_reconnectingtoparty", false);
				Command::Execute("reconnect", false);
			}, Scheduler::Pipeline::CLIENT, 10s);
		});

		Maps_NewMapFlag = Utils::Hook::Rebase(newMapFlag);
		Maps_NewMapBodyNext = Utils::Hook::Rebase(CL_ConnectionlessPacket_NewMapBodyNext);
		Maps_NewMapReturnTrue = Utils::Hook::Rebase(CL_ConnectionlessPacket_ReturnTrue);

		if (!Utils::Hook::MatchesBytes(CL_ConnectionlessPacket_NewMapBody, newMapBody, sizeof(newMapBody))
			|| !newMapHook.Initialize(CL_ConnectionlessPacket_NewMapBody, LoadingNewMapStub, HOOK_CALL)->Install()->IsInstalled())
		{
			newMapHook.Uninstall();
			Logger::Error("maps: loadingnewmap does not read as expected, a missing usermap is not fetched on rotation\n");
		}
		else
		{
			Utils::Hook::Nop(CL_ConnectionlessPacket_NewMapBody + 5, sizeof(newMapBody) - 5);
		}

		if (Dedicated::IsEnabled())
		{
			if (!Utils::Hook::BranchesTo(SV_SpawnServer_LoadingNewMapCall, Com_sprintf, HOOK_CALL)
				|| !loadNewMapHook.Initialize(SV_SpawnServer_LoadingNewMapCall, reinterpret_cast<void*>(LoadNewMapCommand), HOOK_CALL)->Install()->IsInstalled())
			{
				loadNewMapHook.Uninstall();
				Logger::Error("maps: SV_SpawnServer's loadingnewmap does not read as expected, clients are not sent the usermap hash\n");
			}
			else
			{
				loadNewMapHook.Quick();
			}
		}

		constexpr Game::XAssetType restrictedTypes[] =
		{
			Game::ASSET_TYPE_CLIPMAP_MP,
			Game::ASSET_TYPE_CLIPMAP_SP,
			Game::ASSET_TYPE_GAMEWORLD_SP,
			Game::ASSET_TYPE_GAMEWORLD_MP,
			Game::ASSET_TYPE_GFXWORLD,
			Game::ASSET_TYPE_MAP_ENTS,
			Game::ASSET_TYPE_COMWORLD,
			Game::ASSET_TYPE_FXWORLD,
			Game::ASSET_TYPE_ADDON_MAP_ENTS,
			Game::ASSET_TYPE_WEAPON,
			Game::ASSET_TYPE_STRINGTABLE,
			Game::ASSET_TYPE_MENU,
			Game::ASSET_TYPE_MENULIST,
		};

		for (const auto type : restrictedTypes)
		{
			AssetHandler::OnLoad(type, LoadAssetRestrict);
		}

		GSC::Script::AddFunction("GetMapList", GSCr_GetMapList);
		GSC::Script::AddFunction("GetMapArenaInfo", GSCr_GetMapArenaInfo);

		if (Utils::Hook::MatchesBytes(staticModelSortJnz, sortCountJnz, sizeof(sortCountJnz)))
		{
			Utils::Hook::Nop(staticModelSortJnz, sizeof(sortCountJnz));
		}
		else
		{
			Logger::Error("maps: the static model sort does not read as expected, static models stay sorted\n");
		}

		if (Utils::Hook::MatchesBytes(PMem_Init_SizeArg, sizeArg, sizeof(sizeArg))
			&& Utils::Hook::MatchesBytes(PMem_Init_SizeStore, sizeStore, sizeof(sizeStore)))
		{
			Utils::Hook::Set<std::uint32_t>(PMem_Init_SizeArg + 1, hunkSize);
			Utils::Hook::Set<std::uint32_t>(PMem_Init_SizeStore + 6, hunkSize);
		}
		else
		{
			Logger::Error("maps: PMem_Init does not read as expected, the hunk stays 300 MiB\n");
		}

		Scheduler::Once([]
		{
			Dvar::Register("isDlcInstalled_All", false, Game::DVAR_EXTERNAL | Game::DVAR_INIT, "");

			for (const auto& dlc : StockDlcs())
			{
				AddDlc(dlc);
			}

			UpdateDlcStatus();

			UIScript::Add("downloadDLC", [](const UIScript::Token& token)
			{
				const auto dlc = token.Get<int>();

				Game::ShowMessageBox(Utils::String::VA("DLC %d does not exist!", dlc), "ERROR");
			});
		}, Scheduler::Pipeline::MAIN);
	}
}
