#include "STDInclude.hpp"

#include "Components/Modules/AssetHandler.hpp"

#include "IXModelSurfs.hpp"

namespace Assets
{
	void IXModelSurfs::SaveXSurfaceCollisionTree(const Game::XSurfaceCollisionTree* entry, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::XSurfaceCollisionTree, 40);

		auto* const buffer = builder->GetBuffer();

		auto* const destEntry = buffer->Dest<Game::X86::XSurfaceCollisionTree>();
		const auto record = Game::X86::Convert(*entry);
		buffer->Save(&record);

		if (entry->nodes)
		{
			AssertSize(Game::XSurfaceCollisionNode, 16);

			buffer->Align(Utils::Stream::ALIGN_16);
			buffer->SaveArray(entry->nodes, entry->nodeCount);
			Utils::Stream::ClearPointer(&destEntry->nodes);
		}

		if (entry->leafs)
		{
			AssertSize(Game::XSurfaceCollisionLeaf, 2);

			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(entry->leafs, entry->leafCount);
			Utils::Stream::ClearPointer(&destEntry->leafs);
		}
	}

	void IXModelSurfs::SaveXSurface(const Game::XSurface* surf, Game::X86::XSurface* destSurf, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		if (surf->vertInfo.vertsBlend)
		{
			if (builder->HasPointer(surf->vertInfo.vertsBlend))
			{
				destSurf->vertInfo.vertsBlend = builder->GetPointer(surf->vertInfo.vertsBlend);
			}
			else
			{
				const auto& vertCount = surf->vertInfo.vertCount;
				const int blendCount = vertCount[0] + (vertCount[1] * 3) + (vertCount[2] * 5) + (vertCount[3] * 7);

				buffer->Align(Utils::Stream::ALIGN_2);
				builder->StorePointer(surf->vertInfo.vertsBlend);
				buffer->SaveArray(surf->vertInfo.vertsBlend, static_cast<std::size_t>(blendCount));
				Utils::Stream::ClearPointer(&destSurf->vertInfo.vertsBlend);
			}
		}

		buffer->PushBlock(Game::XFILE_BLOCK_VERTEX);

		if (surf->verts0)
		{
			AssertSize(Game::GfxPackedVertex, 32);
			AssertSize(Game::X86::GfxPackedVertex, 32);

			if (builder->HasPointer(surf->verts0))
			{
				destSurf->verts0 = builder->GetPointer(surf->verts0);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_16);
				builder->StorePointer(surf->verts0);
				buffer->SaveArray(surf->verts0, surf->vertCount);
				Utils::Stream::ClearPointer(&destSurf->verts0);
			}
		}

		buffer->PopBlock();

		if (surf->vertList)
		{
			AssertSize(Game::X86::XRigidVertList, 12);

			if (builder->HasPointer(surf->vertList))
			{
				destSurf->vertList = builder->GetPointer(surf->vertList);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(surf->vertList);

				auto* const destLists = buffer->Dest<Game::X86::XRigidVertList>();

				for (unsigned int i = 0; i < surf->vertListCount; ++i)
				{
					const auto listRecord = Game::X86::Convert(surf->vertList[i]);
					buffer->Save(&listRecord);
				}

				for (unsigned int i = 0; i < surf->vertListCount; ++i)
				{
					auto* const destRigidVertList = &destLists[i];
					const auto* const rigidVertList = &surf->vertList[i];

					if (!rigidVertList->collisionTree)
					{
						continue;
					}

					if (builder->HasPointer(rigidVertList->collisionTree))
					{
						destRigidVertList->collisionTree = builder->GetPointer(rigidVertList->collisionTree);
					}
					else
					{
						buffer->Align(Utils::Stream::ALIGN_4);
						builder->StorePointer(rigidVertList->collisionTree);
						this->SaveXSurfaceCollisionTree(rigidVertList->collisionTree, builder);
						Utils::Stream::ClearPointer(&destRigidVertList->collisionTree);
					}
				}

				Utils::Stream::ClearPointer(&destSurf->vertList);
			}
		}

		buffer->PushBlock(Game::XFILE_BLOCK_INDEX);

		if (builder->HasPointer(surf->triIndices))
		{
			destSurf->triIndices = builder->GetPointer(surf->triIndices);
		}
		else
		{
			buffer->Align(Utils::Stream::ALIGN_16);
			builder->StorePointer(surf->triIndices);
			buffer->SaveArray(surf->triIndices, surf->triCount * 3);
			Utils::Stream::ClearPointer(&destSurf->triIndices);
		}

		buffer->PopBlock();
	}

	void IXModelSurfs::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::XModelSurfs, 36);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.modelSurfs;
		auto* const dest = buffer->Dest<Game::X86::XModelSurfs>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->surfs)
		{
			AssertSize(Game::X86::XSurface, 64);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destSurfaces = buffer->Dest<Game::X86::XSurface>();

			for (int i = 0; i < asset->numsurfs; ++i)
			{
				const auto surfaceRecord = Game::X86::Convert(asset->surfs[i]);
				buffer->Save(&surfaceRecord);
			}

			for (int i = 0; i < asset->numsurfs; ++i)
			{
				this->SaveXSurface(&asset->surfs[i], &destSurfaces[i], builder);
			}

			Utils::Stream::ClearPointer(&dest->surfs);
		}

		buffer->PopBlock();
	}
}
