#pragma once

namespace Components
{
	class Maps : public Component
	{
	public:
		class UserMapContainer
		{
		public:
			UserMapContainer() : wasFreed(false), hash(0), hashComputed(false) {}
			UserMapContainer(const std::string& _mapname)
				: wasFreed(false), hash(0), hashComputed(false), mapname(_mapname)
			{
				ZeroMemory(&this->searchPath, sizeof(this->searchPath));
				Maps::ForceRefreshArenas();
			}

			~UserMapContainer()
			{
				this->freeIwd();
				this->clear();
			}

			// Map hashes are only needed for network compatibility checks. Keep
			// them out of the normal local map-loading path until first requested.
			unsigned int getHash();
			const std::string& getName() const { return this->mapname; }
			bool isValid() const { return !this->mapname.empty(); }
			void clear()
			{
				bool wasValid = this->isValid();
				this->mapname.clear();
				this->hash = 0;
				this->hashComputed = false;
				if (wasValid)
				{
					Maps::ForceRefreshArenas();
				}
			}

			void loadIwd();
			void freeIwd();

			void reloadIwd();

			void handlePackfile(void* packfile);

		private:
			bool wasFreed;
			unsigned int hash;
			bool hashComputed;
			std::string mapname;
			Game::searchpath_s searchPath;
		};

		Maps();
		~Maps();

		static void HandleAsSPMap();

		static std::string CurrentMainZone;
		static const char* UserMapFiles[4];

		static bool CheckMapInstalled(const std::string& mapname, bool error = false, bool dlcIsTrue = false);

		static UserMapContainer* GetUserMap();
		static unsigned int GetUsermapHash(const std::string& map);

		static Game::XAssetEntry* GetAssetEntryPool();
		static bool IsCustomMap();
		static bool IsUserMap(const std::string& mapname);

		static void ScanCustomMaps();
		static std::string GetArenaPath(const std::string& mapName);
		static const std::vector<std::string>& GetCustomMaps();

		static std::unordered_map<std::string, std::string> ParseCustomMapArena(const std::string& singleMapArena);

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

		static bool SPMap;
		static UserMapContainer UserMap;
		static std::vector<DLC> DlcPacks;

		static std::vector<std::pair<std::string, std::string>> DependencyList;
		static std::vector<std::string> CurrentDependencies;
		static std::vector<std::string> FoundCustomMaps;

		static Dvar::Var RListSModels;

		static void ForceRefreshArenas();

		static void GetBSPName(char* buffer, size_t size, const char* format, const char* mapname);
		static void LoadAssetRestrict(Game::XAssetType type, Game::XAssetHeader asset, std::string_view name, bool* restrict);
		static void LoadMapZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);
		static void UnloadMapZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);

		static void OverrideMapEnts(Game::MapEnts* ents);
		static MapDependencies GetDependenciesForMap(const std::string& map);

		static int IgnoreEntityStub(const char* entity);

		static Game::G_GlassData* GetWorldData();
		static void GetWorldDataStub();

		static void LoadRawSun();

		static void AddDlc(DLC dlc);
		static void UpdateDlcStatus();

		static void PrepareUsermap(const char* mapname);
		static void SpawnServerStub();
		static void LoadMapLoadscreenStub();

		static int TriggerReconnectForMap(Game::msg_t* msg, const char* mapname);
		static void RotateCheckStub();
		static void LoadNewMapCommand(char* buffer, size_t size, const char* format, const char* mapname, const char* gametype);

		static const char* LoadArenaFileStub(const char* name, char* buffer, int size);

		static void HideModel();
		static void HideModelStub();

		static void G_SpawnTurretHook(Game::gentity_s* ent, int unk, int unk2);
		static bool SV_SetTriggerModelHook(Game::gentity_s* ent);
		static unsigned short CM_TriggerModelBounds_Hk(unsigned int brushModelPointer, Game::Bounds* bounds);
	};
}
