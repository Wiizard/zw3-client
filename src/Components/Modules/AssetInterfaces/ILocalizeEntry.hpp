#pragma once

#include "../AssetHandler.hpp"
#include "../FileSystem.hpp"

namespace Assets
{
	class ILocalizeEntry : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_LOCALIZE_ENTRY;
		}

		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

		static void ParseLocalizedStringsFile(Components::ZoneBuilder::Zone* builder, const std::string& name, const std::string& filename);
		static void ParseLocalizedStringsJSON(Components::ZoneBuilder::Zone* builder, Components::FileSystem::File& file);
	};
}
