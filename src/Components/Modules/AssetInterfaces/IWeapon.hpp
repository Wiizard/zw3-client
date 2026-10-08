#pragma once

namespace Assets
{
	class IWeapon : public Components::AssetHandler::IAsset
	{
	public:
		IWeapon();

		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_WEAPON;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;

	private:
		void WriteWeaponDef(const Game::WeaponCompleteDef* weapon, Components::ZoneBuilder::Zone* builder, Utils::Stream* buffer);
	};
}
