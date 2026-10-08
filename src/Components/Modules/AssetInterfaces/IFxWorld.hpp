#pragma once

#include "Components/Modules/AssetHandler.hpp"

namespace Assets
{
	class IFxWorld : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_FXWORLD;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;

		void LoadFromDisk(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder);
		void Generate(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder);
	};
}
