#pragma once

#include "Dvar.hpp"

namespace Components
{
	class ZoneBuilder : public Component
	{
	public:
		class Zone
		{
		public:
			class AssetRecursionMarker
			{
			public:
				AssetRecursionMarker(Zone* zone) : builder(zone)
				{
					this->builder->IncreaseAssetDepth();
				}

				~AssetRecursionMarker()
				{
					this->builder->DecreaseAssetDepth();
				}

			private:
				Zone* builder;
			};

			Zone(const std::string& name, const std::string& sourceName, const std::string& destinationPath);
			Zone(const std::string& name);
			~Zone();

			void Build();

			Utils::Stream* GetBuffer();
			Utils::Memory::Allocator* GetAllocator();

			bool HasPointer(const void* pointer);
			void StorePointer(const void* pointer);

			std::uint32_t GetPointer(const void* pointer);

			int FindAsset(Game::XAssetType type, std::string name);
			Game::XAssetHeader FindSubAsset(Game::XAssetType type, std::string name);
			Game::XAsset* GetAsset(int index);
			std::uint32_t GetAssetTableOffset(int index);

			bool HasAlias(Game::XAsset asset);
			std::uint32_t SaveSubAsset(Game::XAssetType type, void* ptr);
			bool LoadAssetByName(Game::XAssetType type, const std::string& name, bool isSubAsset = true);
			bool LoadAsset(Game::XAssetType type, void* data, bool isSubAsset = true);

			int AddScriptString(unsigned short gameIndex);
			int AddScriptString(const std::string& str);
			int FindScriptString(const std::string& str);
			void AddRawAsset(Game::XAssetType type, void* ptr);

			void MapScriptString(unsigned short& gameIndex);

			void RenameAsset(Game::XAssetType type, const std::string& asset, const std::string& newName);
			std::string GetAssetName(Game::XAssetType type, const std::string& asset);

			void Store(Game::XAssetHeader header);

			void IncrementExternalSize(unsigned int size);

			void IncreaseAssetDepth()
			{
				++this->assetDepth;
			}

			void DecreaseAssetDepth()
			{
				--this->assetDepth;
			}

			bool IsPrimaryAsset()
			{
				return this->assetDepth <= 1;
			}

		private:
			void LoadFastFiles() const;

			bool LoadAssets();
			bool LoadAssetByName(const std::string& typeName, std::string name, bool isSubAsset = true);

			bool TrySaveData();
			void WriteZone();

			std::uint32_t GetAlias(Game::XAsset asset);
			void StoreAlias(Game::XAsset asset);

			void AddBranding();

			std::uint32_t SafeGetPointer(const void* pointer);

			int indexStart;
			unsigned int externalSize;
			Utils::Stream buffer;

			std::string zoneName;
			std::string destination;
			Utils::CSV dataMap;

			Utils::Memory::Allocator memAllocator;

			std::vector<Game::XAsset> loadedAssets;
			std::vector<Game::XAsset> markedAssets;
			std::vector<Game::XAsset> loadedSubAssets;
			std::vector<std::string> scriptStrings;

			std::map<unsigned short, unsigned int> scriptStringMap;

			std::map<std::string, std::string> renameMap[Game::ASSET_TYPE_COUNT];

			std::map<const void*, std::uint32_t> pointerMap;
			std::vector<std::pair<Game::XAsset, std::uint32_t>> aliasList;

			Game::RawFile branding;

			std::size_t assetDepth;
		};

		struct NamedAsset
		{
			Game::XAssetType type;
			std::string name;

			bool operator==(const NamedAsset& other) const
			{
				return this->type == other.type && this->name == other.name;
			}

			struct Hash
			{
				std::size_t operator()(const NamedAsset& asset) const
				{
					return static_cast<std::size_t>(asset.type) ^ std::hash<std::string>{}(asset.name);
				}
			};
		};

		ZoneBuilder();

		static bool IsEnabled();

		static bool IsDumpingZone()
		{
			return !dumpingZone.empty();
		}

		static std::string traceZone;
		static std::vector<NamedAsset> traceAssets;

		static void BeginAssetTrace(const std::string& zone);
		static std::vector<NamedAsset> EndAssetTrace();

		static Dvar::Var zb_sp_to_mp;

		static Game::XAssetHeader GetEmptyAssetIfCommon(Game::XAssetType type, const std::string& name, Zone* builder);
		static std::string GetDumpingZonePath();

	private:
		static std::string dumpingZone;

		static bool ApplyEnginePatches();

		static void StoreTexture(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image);
		static void ReleaseTexture(Game::XAssetHeader header);

		static void DumpZone(const std::string& zone);

		static std::function<void()> LoadZoneWithTrace(const std::string& zone, std::vector<NamedAsset>& assets);
	};
}
