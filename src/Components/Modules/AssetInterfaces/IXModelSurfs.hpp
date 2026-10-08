#pragma once

namespace Assets
{
	class IXModelSurfs : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_XMODEL_SURFS;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

	private:
		void SaveXSurface(const Game::XSurface* surf, Game::X86::XSurface* destSurf, Components::ZoneBuilder::Zone* builder);
		void SaveXSurfaceCollisionTree(const Game::XSurfaceCollisionTree* entry, Components::ZoneBuilder::Zone* builder);
	};
}
