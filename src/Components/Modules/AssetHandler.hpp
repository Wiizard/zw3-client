#pragma once

#include "ZoneBuilder.hpp"

namespace Components
{
	class AssetHandler : public Component
	{
	public:
		class IAsset
		{
		public:
			virtual ~IAsset() = default;

			virtual Game::XAssetType GetType()
			{
				return Game::ASSET_TYPE_COUNT;
			}

			virtual void Mark(Game::XAssetHeader, ZoneBuilder::Zone*)
			{
			}

			virtual void Save(Game::XAssetHeader, ZoneBuilder::Zone*)
			{
			}

			virtual bool HasDump()
			{
				return false;
			}

			virtual void Dump(Game::XAssetHeader)
			{
			}

			virtual void Load(Game::XAssetHeader*, const std::string&, ZoneBuilder::Zone*)
			{
			}
		};

		typedef void(LoadCallback)(unsigned int type, void* asset, const std::string& name, bool* restrict);
		typedef void*(FindCallback)(unsigned int type, const std::string& name);

		AssetHandler();

		static bool IsInstalled();

		static void OnLoad(const std::function<LoadCallback>& callback);

		static bool OnFind(unsigned int type, const std::function<FindCallback>& callback);

		static void OnLoad(unsigned int type, const std::function<LoadCallback>& callback);

		static void ZoneSave(Game::XAsset asset, ZoneBuilder::Zone* builder);
		static void ZoneMark(Game::XAsset asset, ZoneBuilder::Zone* builder);
		static void DumpAsset(Game::XAsset asset);
		static bool CanDump(Game::XAssetType type);
		static void ForgetDumpedAssets();

		static Game::XAssetHeader FindOriginalAsset(Game::XAssetType type, const char* filename);
		static Game::XAssetHeader FindLoadedAsset(Game::XAssetType type, const char* name);
		static Game::XAssetHeader FindAssetForZone(Game::XAssetType type, const std::string& filename, ZoneBuilder::Zone* builder, bool isSubAsset = true);
		static Game::XAssetHeader FindTemporaryAsset(Game::XAssetType type, const char* filename);

		static void ClearTemporaryAssets();
		static void StoreTemporaryAsset(Game::XAssetType type, Game::XAssetHeader asset);
		static void RemoveTemporaryAsset(Game::XAssetType type, const char* name);

		static void ExposeTemporaryAssets(bool expose);

	private:
		static bool isInstalled;
		static std::vector<std::function<LoadCallback>> loadCallbacks;
		static std::unordered_map<unsigned int, std::vector<std::function<LoadCallback>>> typeLoadCallbacks;
		static std::map<unsigned int, std::vector<std::function<FindCallback>>> findCallbacks;

		static bool shouldSearchTempAssets;
		static std::map<std::string, Game::XAssetHeader> temporaryAssets[Game::ASSET_TYPE_COUNT];
		static std::map<Game::XAssetType, std::unique_ptr<IAsset>> assetInterfaces;

		static void RegisterInterface(IAsset* asset);

		static void ModifyAsset(unsigned int type, void* asset, const std::string& name);
		static void* DB_AddXAsset_Hook(unsigned int type, void** header);
		static void* DB_FindXAssetHeader_Hook(unsigned int type, const char* name);

		static void InstallFindHooks();
	};
}
