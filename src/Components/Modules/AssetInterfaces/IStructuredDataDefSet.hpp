#pragma once

#include "../AssetHandler.hpp"

namespace Assets
{
	class IStructuredDataDefSet : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_STRUCTURED_DATA_DEF;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

		static void SaveStructuredDataEnumArray(const Game::StructuredDataEnum* enums, int numEnums, Components::ZoneBuilder::Zone* builder);
		static void SaveStructuredDataStructArray(const Game::StructuredDataStruct* structs, int numStructs, Components::ZoneBuilder::Zone* builder);
	};
}
