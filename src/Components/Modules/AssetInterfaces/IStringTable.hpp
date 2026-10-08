#pragma once

#include "../AssetHandler.hpp"

namespace Assets
{
	class IStringTable : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_STRINGTABLE;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

	private:
		static void SaveStringTableCellArray(Components::ZoneBuilder::Zone* builder, const Game::StringTableCell* values, int count);
	};
}
