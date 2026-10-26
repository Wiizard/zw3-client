#pragma once

#include <functional>
#include <optional>

namespace Components
{
	class Maps : public Component
	{
	public:
		class UserMapContainer
		{
		public:
			UserMapContainer() : wasFreed(false), hash(0), isHashComputed(false) {}
			UserMapContainer(const std::string& name) : wasFreed(false), hash(0), isHashComputed(false), mapname(name)
			{
				ZeroMemory(&this->searchPath, sizeof(this->searchPath));
				Maps::ForceRefreshArenas();
			}

			~UserMapContainer()
			{
				this->FreeIwd();
				// Destruction also runs after engine teardown. The string cleans
				// itself up; refreshing UI arenas here would read freed rawfiles.
			}

			unsigned int GetHash();
			std::string GetName() const { return this->mapname; }
			bool IsValid() const { return !this->mapname.empty(); }

			void Clear()
			{
				const bool wasValid = this->IsValid();
				this->mapname.clear();
				this->hash = 0;
				this->isHashComputed = false;

				if (wasValid)
				{
					Maps::ForceRefreshArenas();
				}
			}

			void LoadIwd();
			void FreeIwd();

			void ReloadIwd();

			void HandlePackfile(void* packfile);

		private:
			bool wasFreed;
			unsigned int hash;
			bool isHashComputed;
			std::string mapname;
			Game::searchpath_s searchPath{};
		};

		Maps();

		static std::string currentMainZone;
		static const char* userMapFiles[4];

		static UserMapContainer* GetUserMap();
		static unsigned int GetUsermapHash(const std::string& map);
		// Worker-safe content preflight. Missing non-usermap directories return
		// zero; unreadable, incomplete or changing files return no checksum.
		static std::optional<unsigned int> TryGetUsermapHash(const std::string& map,
			const std::function<bool()>& isCancelled = {});
		// Never starts content hashing: safe for the final UI-thread join check.
		static std::optional<unsigned int> TryGetCachedUsermapHash(const std::string& map);

		static bool IsUserMap(const std::string& mapname);

		static bool CheckMapInstalled(const std::string& mapname, bool error = false, bool dlcIsTrue = false);

		static bool IsDlcInstalled(int index);

		static int TriggerReconnectForMap(Game::msg_t* msg, const char* mapname);

		static void ScanCustomMaps();
		static std::string GetArenaPath(const std::string& mapName);
		static const std::vector<std::string>& GetCustomMaps();

		static std::unordered_map<std::string, std::string> ParseCustomMapArena(const std::string& singleMapArena);
		static void SynchronizeMapDvars(const std::string& rawMapName);
		static void PrefetchMapAssets(const std::string& map);
		static void LoadMapZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);
		// showLoadscreen false mounts a usermap's IWD without anything a real
		// level load does to the front end (the loading screen, the map dvars,
		// the renderer's map-load state): the menu backdrop loads maps that way.
		static void PrepareUsermap(const char* mapname, bool updateSelection = true, bool showLoadscreen = true);

		static void HandleAsSPMap();

	private:
		class DLC
		{
		public:
			int index;
			std::string name;
			std::vector<std::string> maps;
		};

		struct MapDependencies
		{
			std::vector<std::string> requiredMaps;
			std::pair<std::string, std::string> requiredTeams;
			bool requiresTeamZones;
		};

		static UserMapContainer userMap;
		static std::vector<DLC> dlcPacks;

		static std::vector<std::string> currentDependencies;
		static std::vector<std::string> foundCustomMaps;

		static void ForceRefreshArenas();

		static void GetBSPName(char* buffer, size_t size, const char* format, const char* mapname);
		static void LoadNewMapCommand(char* buffer, int size, const char* format, const char* mapname, const char* gametype);
		static void LoadAssetRestrict(unsigned int type, void* asset, const std::string& name, bool* restrict);
		static void UnloadMapZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);

		static MapDependencies GetDependenciesForMap(const std::string& map);

		static int IgnoreEntityStub(const char* entity);

		static void LoadLoadscreenZone_Stub(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);

		static const char* LoadArenaFileStub(const char* name, char* buffer, int size);

		static std::vector<DLC> StockDlcs();
		static void AddDlc(DLC dlc);
		static void UpdateDlcStatus();

		static void G_SpawnTurretHook(Game::gentity_s* ent, const char* weaponInfoName, int scriptSpawned);
		static unsigned short CM_TriggerModelBounds_Hk(unsigned int triggerIndex, Game::Bounds* bounds);
		static void G_InitGlass_Hk();

		static void GSCr_GetMapArenaInfo();
		static void GSCr_GetMapList();

	};
}
