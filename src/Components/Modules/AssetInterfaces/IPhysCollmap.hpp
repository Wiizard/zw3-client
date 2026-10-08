#pragma once

namespace Assets
{
	class IPhysCollmap : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_PHYSCOLLMAP;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

	private:
		void SavePhysGeomInfoArray(Components::ZoneBuilder::Zone* builder, const Game::PhysGeomInfo* geoms, unsigned int count);
		void SaveBrushWrapper(Components::ZoneBuilder::Zone* builder, const Game::BrushWrapper* brush);
	};
}
