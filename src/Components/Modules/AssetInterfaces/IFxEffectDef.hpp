#pragma once

namespace Assets
{
	class IFxEffectDef : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_FX;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		bool HasDump() override
		{
			return true;
		}

		void Dump(Game::XAssetHeader header) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;

	private:
		void MarkFxElemVisuals(const Game::FxElemVisuals* visuals, char elemType, Components::ZoneBuilder::Zone* builder);
		void SaveFxElemVisuals(const Game::FxElemVisuals* visuals, Game::X86::FxElemVisuals* destVisuals, char elemType, Components::ZoneBuilder::Zone* builder);

		void LoadNative(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder);
		void LoadFromIW4OF(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder);
	};
}
