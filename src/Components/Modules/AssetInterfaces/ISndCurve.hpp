#pragma once

namespace Assets
{
	class ISndCurve : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_SOUND_CURVE;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
	};
}
