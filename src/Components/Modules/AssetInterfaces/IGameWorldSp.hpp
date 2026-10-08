#pragma once

#include "Components/Modules/AssetHandler.hpp"

namespace Assets
{
	class IGameWorldSp : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_GAMEWORLD_SP;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;

	private:
		void Savepathnode_tree_info_t(const Game::pathnode_tree_t* nodeTree, Game::X86::pathnode_tree_t* destNodeTree, Components::ZoneBuilder::Zone* builder);
		void SaveVehicleTrackSegment(const Game::VehicleTrackSegment* trackSegment, Game::X86::VehicleTrackSegment* destTrackSegment, Components::ZoneBuilder::Zone* builder);
		void SaveVehicleTrackSegment_ptrArray(Game::VehicleTrackSegment* const* trackSegmentPtrs, unsigned int count, Components::ZoneBuilder::Zone* builder);
	};
}
