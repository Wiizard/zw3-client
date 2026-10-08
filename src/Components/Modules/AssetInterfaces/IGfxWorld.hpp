#pragma once

#include "Components/Modules/AssetHandler.hpp"

namespace Assets
{
	class IGfxWorld : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_GFXWORLD;
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
		void SaveGfxWorldDpvsPlanes(const Game::GfxWorld* world, const Game::GfxWorldDpvsPlanes* asset, Game::X86::GfxWorldDpvsPlanes* dest, Components::ZoneBuilder::Zone* builder);
		void SaveGfxWorldDraw(const Game::GfxWorldDraw* asset, Game::X86::GfxWorldDraw* dest, Components::ZoneBuilder::Zone* builder);
		void SaveGfxLightGrid(const Game::GfxLightGrid* asset, Game::X86::GfxLightGrid* dest, Components::ZoneBuilder::Zone* builder);
		void Savesunflare_t(const Game::sunflare_t* asset, Game::X86::sunflare_t* dest, Components::ZoneBuilder::Zone* builder);
		void SaveGfxWorldDpvsStatic(const Game::GfxWorld* world, const Game::GfxWorldDpvsStatic* asset, Game::X86::GfxWorldDpvsStatic* dest, int planeCount, Components::ZoneBuilder::Zone* builder);
		void SaveGfxWorldDpvsDynamic(const Game::GfxWorldDpvsDynamic* asset, Game::X86::GfxWorldDpvsDynamic* dest, int cellCount, Components::ZoneBuilder::Zone* builder);
	};
}
