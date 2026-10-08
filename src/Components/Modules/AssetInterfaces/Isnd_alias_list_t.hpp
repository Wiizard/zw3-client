#pragma once

namespace Assets
{
	class Isnd_alias_list_t : public Components::AssetHandler::IAsset
	{
	public:
		Isnd_alias_list_t();

		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_SOUND;
		}

		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;
	};
}
