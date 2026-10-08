#pragma once

namespace Assets
{
	class IXAnimParts : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_XANIMPARTS;
		}

		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;

	private:
		void SaveXAnimDeltaPart(const Game::XAnimDeltaPart* delta, unsigned short framecount, Components::ZoneBuilder::Zone* builder);
	};
}
