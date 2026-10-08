#include "STDInclude.hpp"

#include "IGameWorldSp.hpp"

namespace Assets
{
	static Game::X86::pathnode_tree_t ConvertNodeTree(const Game::pathnode_tree_t& nodeTree)
	{
		auto converted = Game::X86::Convert(nodeTree);
		std::memset(&converted.u, 0, sizeof(converted.u));

		if (nodeTree.axis < 0)
		{
			converted.u.s.nodeCount = nodeTree.u.s.nodeCount;
		}

		return converted;
	}

	void IGameWorldSp::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.gameWorldSp;

		if (!asset->path.nodes)
		{
			return;
		}

		for (unsigned int i = 0; i < asset->path.nodeCount; ++i)
		{
			const auto& constant = asset->path.nodes[i].constant;

			builder->AddScriptString(constant.targetname);
			builder->AddScriptString(constant.script_linkName);
			builder->AddScriptString(constant.script_noteworthy);
			builder->AddScriptString(constant.target);
			builder->AddScriptString(constant.animscript);
		}
	}

	void IGameWorldSp::Savepathnode_tree_info_t(const Game::pathnode_tree_t* nodeTree, Game::X86::pathnode_tree_t* destNodeTree, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::pathnode_tree_info_t, 8);

		auto* const buffer = builder->GetBuffer();

		if (nodeTree->axis < 0)
		{
			AssertSize(Game::X86::pathnode_tree_nodes_t, 8);

			if (nodeTree->u.s.nodes)
			{
				buffer->Align(Utils::Stream::ALIGN_2);
				buffer->SaveArray(nodeTree->u.s.nodes, nodeTree->u.s.nodeCount);
				Utils::Stream::ClearPointer(&destNodeTree->u.s.nodes);
			}

			return;
		}

		for (int i = 0; i < 2; ++i)
		{
			auto* const destChildNodeTreePtr = &destNodeTree->u.child[i];
			const auto* const childNodeTree = nodeTree->u.child[i];

			if (!childNodeTree)
			{
				continue;
			}

			if (builder->HasPointer(childNodeTree))
			{
				*destChildNodeTreePtr = builder->GetPointer(childNodeTree);
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);
			builder->StorePointer(childNodeTree);

			auto* const destChildNodeTree = buffer->Dest<Game::X86::pathnode_tree_t>();
			const auto converted = ConvertNodeTree(*childNodeTree);
			buffer->Save(&converted);

			this->Savepathnode_tree_info_t(childNodeTree, destChildNodeTree, builder);
			Utils::Stream::ClearPointer(destChildNodeTreePtr);
		}
	}

	void IGameWorldSp::SaveVehicleTrackSegment_ptrArray(Game::VehicleTrackSegment* const* trackSegmentPtrs, unsigned int count, Components::ZoneBuilder::Zone* builder)
	{
		if (!trackSegmentPtrs)
		{
			return;
		}

		auto* const buffer = builder->GetBuffer();

		auto* const destTrackSegmentPtrs = buffer->Dest<std::uint32_t>();
		buffer->SaveNull(sizeof(std::uint32_t) * count);

		for (unsigned int i = 0; i < count; ++i)
		{
			auto* const destTrackSegmentPtr = &destTrackSegmentPtrs[i];
			const auto* const trackSegment = trackSegmentPtrs[i];

			if (!trackSegment)
			{
				continue;
			}

			if (builder->HasPointer(trackSegment))
			{
				*destTrackSegmentPtr = builder->GetPointer(trackSegment);
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);
			builder->StorePointer(trackSegment);

			auto* const destTrackSegment = buffer->Dest<Game::X86::VehicleTrackSegment>();
			const auto converted = Game::X86::Convert(*trackSegment);
			buffer->Save(&converted);

			this->SaveVehicleTrackSegment(trackSegment, destTrackSegment, builder);

			Utils::Stream::ClearPointer(destTrackSegmentPtr);
		}
	}

	void IGameWorldSp::SaveVehicleTrackSegment(const Game::VehicleTrackSegment* trackSegment, Game::X86::VehicleTrackSegment* destTrackSegment, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		if (trackSegment->targetName)
		{
			buffer->SaveString(trackSegment->targetName);
			Utils::Stream::ClearPointer(&destTrackSegment->targetName);
		}

		if (trackSegment->sectors)
		{
			AssertSize(Game::X86::VehicleTrackSector, 60);
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destTrackSectors = buffer->Dest<Game::X86::VehicleTrackSector>();

			for (unsigned int i = 0; i < trackSegment->sectorCount; ++i)
			{
				const auto converted = Game::X86::Convert(trackSegment->sectors[i]);
				buffer->Save(&converted);
			}

			for (unsigned int i = 0; i < trackSegment->sectorCount; ++i)
			{
				const auto* const trackSector = &trackSegment->sectors[i];

				if (trackSector->obstacles)
				{
					AssertSize(Game::X86::VehicleTrackObstacle, 12);
					static_assert(sizeof(Game::VehicleTrackObstacle) == sizeof(Game::X86::VehicleTrackObstacle));

					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->SaveArray(trackSector->obstacles, trackSector->obstacleCount);
					Utils::Stream::ClearPointer(&destTrackSectors[i].obstacles);
				}
			}

			Utils::Stream::ClearPointer(&destTrackSegment->sectors);
		}

		if (trackSegment->nextBranches)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			this->SaveVehicleTrackSegment_ptrArray(trackSegment->nextBranches, trackSegment->nextBranchesCount, builder);
			Utils::Stream::ClearPointer(&destTrackSegment->nextBranches);
		}

		if (trackSegment->prevBranches)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			this->SaveVehicleTrackSegment_ptrArray(trackSegment->prevBranches, trackSegment->prevBranchesCount, builder);
			Utils::Stream::ClearPointer(&destTrackSegment->prevBranches);
		}
	}

	void IGameWorldSp::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GameWorldSp, 0x38);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.gameWorldSp;
		auto* const dest = buffer->Dest<Game::X86::GameWorldSp>();

		const auto world = Game::X86::Convert(*asset);
		buffer->Save(&world);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		AssertSize(Game::X86::PathData, 40);

		const auto& path = asset->path;

		if (path.nodes)
		{
			AssertSize(Game::X86::pathnode_t, 136);
			AssertSize(Game::X86::pathnode_constant_t, 64);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destNodes = buffer->Dest<Game::X86::pathnode_t>();

			for (unsigned int i = 0; i < path.nodeCount; ++i)
			{
				const auto converted = Game::X86::Convert(path.nodes[i]);
				buffer->Save(&converted);
			}

			for (unsigned int i = 0; i < path.nodeCount; ++i)
			{
				auto* const destNode = &destNodes[i];
				const auto* const node = &path.nodes[i];

				builder->MapScriptString(destNode->constant.targetname);
				builder->MapScriptString(destNode->constant.script_linkName);
				builder->MapScriptString(destNode->constant.script_noteworthy);
				builder->MapScriptString(destNode->constant.target);
				builder->MapScriptString(destNode->constant.animscript);

				if (node->constant.Links)
				{
					AssertSize(Game::X86::pathlink_s, 12);
					static_assert(sizeof(Game::pathlink_s) == sizeof(Game::X86::pathlink_s));

					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->SaveArray(node->constant.Links, node->constant.totalLinkCount);
					Utils::Stream::ClearPointer(&destNode->constant.Links);
				}
			}

			Utils::Stream::ClearPointer(&dest->path.nodes);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (path.basenodes)
		{
			AssertSize(Game::X86::pathbasenode_t, 16);

			buffer->Align(Utils::Stream::ALIGN_16);
			buffer->Save(path.basenodes, sizeof(Game::X86::pathbasenode_t), path.nodeCount);
			Utils::Stream::ClearPointer(&dest->path.basenodes);
		}

		buffer->PopBlock();

		if (path.chainNodeForNode)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(path.chainNodeForNode, path.nodeCount);
			Utils::Stream::ClearPointer(&dest->path.chainNodeForNode);
		}

		if (path.nodeForChainNode)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(path.nodeForChainNode, path.nodeCount);
			Utils::Stream::ClearPointer(&dest->path.nodeForChainNode);
		}

		if (path.pathVis)
		{
			buffer->SaveArray(path.pathVis, path.visBytes);
			Utils::Stream::ClearPointer(&dest->path.pathVis);
		}

		if (path.nodeTree)
		{
			AssertSize(Game::X86::pathnode_tree_t, 16);
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destNodeTrees = buffer->Dest<Game::X86::pathnode_tree_t>();

			for (int i = 0; i < path.nodeTreeCount; ++i)
			{
				const auto converted = ConvertNodeTree(path.nodeTree[i]);
				buffer->Save(&converted);
			}

			for (int i = 0; i < path.nodeTreeCount; ++i)
			{
				this->Savepathnode_tree_info_t(&path.nodeTree[i], &destNodeTrees[i], builder);
			}

			Utils::Stream::ClearPointer(&dest->path.nodeTree);
		}

		AssertSize(Game::X86::VehicleTrack, 8);

		if (asset->vehicleTrack.segments)
		{
			if (builder->HasPointer(asset->vehicleTrack.segments))
			{
				dest->vehicleTrack.segments = builder->GetPointer(asset->vehicleTrack.segments);
			}
			else
			{
				AssertSize(Game::X86::VehicleTrackSegment, 44);

				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destTrackSegments = buffer->Dest<Game::X86::VehicleTrackSegment>();

				for (unsigned int i = 0; i < asset->vehicleTrack.segmentCount; ++i)
				{
					builder->StorePointer(&asset->vehicleTrack.segments[i]);

					const auto converted = Game::X86::Convert(asset->vehicleTrack.segments[i]);
					buffer->Save(&converted);
				}

				for (unsigned int i = 0; i < asset->vehicleTrack.segmentCount; ++i)
				{
					this->SaveVehicleTrackSegment(&asset->vehicleTrack.segments[i], &destTrackSegments[i], builder);
				}

				Utils::Stream::ClearPointer(&dest->vehicleTrack.segments);
			}
		}

		if (asset->g_glassData)
		{
			AssertSize(Game::X86::G_GlassData, 128);
			buffer->Align(Utils::Stream::ALIGN_4);

			const auto* const glassData = asset->g_glassData;
			auto* const destGlass = buffer->Dest<Game::X86::G_GlassData>();

			const auto storedGlass = Game::X86::Convert(*glassData);
			buffer->Save(&storedGlass);

			if (glassData->glassPieces)
			{
				AssertSize(Game::X86::G_GlassPiece, 12);
				static_assert(sizeof(Game::G_GlassPiece) == sizeof(Game::X86::G_GlassPiece));

				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->SaveArray(glassData->glassPieces, glassData->pieceCount);
				Utils::Stream::ClearPointer(&destGlass->glassPieces);
			}

			if (glassData->glassNames)
			{
				AssertSize(Game::X86::G_GlassName, 12);
				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destGlassNames = buffer->Dest<Game::X86::G_GlassName>();

				for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
				{
					const auto glassName = Game::X86::Convert(glassData->glassNames[i]);
					buffer->Save(&glassName);
				}

				for (unsigned int i = 0; i < glassData->glassNameCount; ++i)
				{
					auto* const destGlassName = &destGlassNames[i];
					const auto* const glassName = &glassData->glassNames[i];

					if (glassName->nameStr)
					{
						buffer->SaveString(glassName->nameStr);
						Utils::Stream::ClearPointer(&destGlassName->nameStr);
					}

					if (glassName->pieceIndices)
					{
						buffer->Align(Utils::Stream::ALIGN_2);
						buffer->SaveArray(glassName->pieceIndices, glassName->pieceCount);
						Utils::Stream::ClearPointer(&destGlassName->pieceIndices);
					}
				}

				Utils::Stream::ClearPointer(&destGlass->glassNames);
			}

			Utils::Stream::ClearPointer(&dest->g_glassData);
		}

		buffer->PopBlock();
	}
}
