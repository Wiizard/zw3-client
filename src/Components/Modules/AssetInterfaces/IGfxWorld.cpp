#include "STDInclude.hpp"

#include "IGfxWorld.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#define IW4X_GFXMAP_VERSION 1

namespace Assets
{
	AssertSize(Game::GfxAabbTree, 56);
	AssertSize(Game::X86::GfxAabbTree, 44);

	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("gfxworld/{}.iw4xGfxWorld", baseName);
	}

	template <typename T>
	static auto* SaveConverted(Utils::Stream* buffer, const T* records, std::size_t count)
	{
		using Stored = decltype(Game::X86::Convert(*records));

		auto* const dest = buffer->Dest<Stored>();

		for (std::size_t i = 0; i < count; ++i)
		{
			const auto converted = Game::X86::Convert(records[i]);
			buffer->Save(&converted);
		}

		return dest;
	}

	template <typename T>
	static void SaveRaw(Utils::Stream* buffer, const T* records, std::size_t count)
	{
		static_assert(sizeof(T) == sizeof(decltype(Game::X86::Convert(*records))));

		buffer->SaveArray(records, count);
	}

	template <typename T>
	static T* ReadRaw(Utils::Stream::Reader& reader, std::size_t count)
	{
		static_assert(sizeof(T) == sizeof(decltype(Game::X86::Convert(std::declval<const T&>()))));

		return reader.ReadArray<T>(count);
	}

	template <typename T>
	static auto* ReadConverted(Utils::Memory::Allocator* allocator, const T* stored, std::size_t count)
	{
		using Loaded = decltype(Game::X86::Convert(*stored));

		auto* const records = allocator->AllocateArray<Loaded>(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			records[i] = Game::X86::Convert(stored[i]);
		}

		return records;
	}

	template <typename T>
	static void MarkRuntime(std::uint32_t stored, T*& field)
	{
		if (stored)
		{
			field = reinterpret_cast<T*>(UINTPTR_MAX);
		}
	}

	static void MarkPresent(std::uint32_t& field, const void* pointer)
	{
		if (pointer)
		{
			Utils::Stream::ClearPointer(&field);
		}
	}

	static Game::X86::GfxAabbTree ConvertAabbTree(const Game::GfxAabbTree& tree)
	{
		constexpr auto size64 = static_cast<int>(sizeof(Game::GfxAabbTree));
		constexpr auto size32 = static_cast<int>(sizeof(Game::X86::GfxAabbTree));

		if (tree.childrenOffset % size64 != 0)
		{
			Components::Logger::Fatal("GfxAabbTree childrenOffset {} is not a whole number of nodes", tree.childrenOffset);
		}

		auto converted = Game::X86::Convert(tree);
		converted.childrenOffset = tree.childrenOffset / size64 * size32;

		return converted;
	}

	static bool TryConvertStoredAabbTree(const Game::X86::GfxAabbTree& stored, Game::GfxAabbTree& tree)
	{
		constexpr auto size64 = static_cast<int>(sizeof(Game::GfxAabbTree));
		constexpr auto size32 = static_cast<int>(sizeof(Game::X86::GfxAabbTree));

		if (stored.childrenOffset % size32 != 0)
		{
			return false;
		}

		tree = Game::X86::Convert(stored);
		tree.childrenOffset = stored.childrenOffset / size32 * size64;

		return true;
	}

	template <typename T>
	static T* FindNamedAsset(Utils::Stream::Reader& reader, Game::XAssetType type, Components::ZoneBuilder::Zone* builder)
	{
		const auto assetName = reader.ReadString();
		return static_cast<T*>(Components::AssetHandler::FindAssetForZone(type, assetName, builder).data);
	}

	static bool TryReadGfxWorldDraw(const Game::X86::GfxWorldDraw& stored, Game::GfxWorldDraw* asset, Utils::Stream::Reader& reader, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();

		if (stored.reflectionProbes)
		{
			const auto* const storedProbes = reader.ReadArray<std::uint32_t>(asset->reflectionProbeCount);
			asset->reflectionProbes = allocator->AllocateArray<Game::GfxImage*>(asset->reflectionProbeCount);

			for (unsigned int i = 0; i < asset->reflectionProbeCount; ++i)
			{
				if (storedProbes[i])
				{
					asset->reflectionProbes[i] = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);
				}
			}
		}

		if (stored.reflectionProbeOrigins)
		{
			asset->reflectionProbeOrigins = ReadRaw<Game::GfxReflectionProbe>(reader, asset->reflectionProbeCount);
		}

		MarkRuntime(stored.reflectionProbeTextures, asset->reflectionProbeTextures);

		if (stored.lightmaps)
		{
			const auto* const storedLightmaps = reader.ReadArray<Game::X86::GfxLightmapArray>(asset->lightmapCount);
			asset->lightmaps = ReadConverted(allocator, storedLightmaps, asset->lightmapCount);

			for (int i = 0; i < asset->lightmapCount; ++i)
			{
				if (storedLightmaps[i].primary)
				{
					asset->lightmaps[i].primary = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);

					if (!asset->lightmaps[i].primary)
					{
						return false;
					}
				}

				if (storedLightmaps[i].secondary)
				{
					asset->lightmaps[i].secondary = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);

					if (!asset->lightmaps[i].secondary)
					{
						return false;
					}
				}
			}
		}

		MarkRuntime(stored.lightmapPrimaryTextures, asset->lightmapPrimaryTextures);
		MarkRuntime(stored.lightmapSecondaryTextures, asset->lightmapSecondaryTextures);

		if (stored.lightmapOverridePrimary)
		{
			asset->lightmapOverridePrimary = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);

			if (!asset->lightmapOverridePrimary)
			{
				return false;
			}
		}

		if (stored.lightmapOverrideSecondary)
		{
			asset->lightmapOverrideSecondary = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);

			if (!asset->lightmapOverrideSecondary)
			{
				return false;
			}
		}

		if (stored.vd.vertices)
		{
			asset->vd.vertices = ReadRaw<Game::GfxWorldVertex>(reader, asset->vertexCount);
		}

		if (stored.vld.data)
		{
			asset->vld.data = reader.ReadArray<char>(asset->vertexLayerDataSize);
		}

		if (stored.indices)
		{
			asset->indices = reader.ReadArray<unsigned short>(static_cast<std::size_t>(asset->indexCount));
		}

		return true;
	}

	static bool TryReadDpvsStatic(const Game::X86::GfxWorldDpvsStatic& stored, Game::GfxWorld* world, Utils::Stream::Reader& reader, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();
		auto* const asset = &world->dpvs;

		for (int i = 0; i < 3; ++i)
		{
			MarkRuntime(stored.smodelVisData[i], asset->smodelVisData[i]);
			MarkRuntime(stored.surfaceVisData[i], asset->surfaceVisData[i]);
		}

		if (stored.sortedSurfIndex)
		{
			asset->sortedSurfIndex = reader.ReadArray<unsigned short>(asset->staticSurfaceCount + asset->staticSurfaceCountNoDecal);
		}

		if (stored.smodelInsts)
		{
			asset->smodelInsts = ReadRaw<Game::GfxStaticModelInst>(reader, asset->smodelCount);
		}

		if (stored.surfaces)
		{
			const auto* const storedSurfaces = reader.ReadArray<Game::X86::GfxSurface>(world->surfaceCount);
			asset->surfaces = ReadConverted(allocator, storedSurfaces, world->surfaceCount);

			for (unsigned int i = 0; i < world->surfaceCount; ++i)
			{
				if (!storedSurfaces[i].material)
				{
					continue;
				}

				asset->surfaces[i].material = FindNamedAsset<Game::Material>(reader, Game::ASSET_TYPE_MATERIAL, builder);

				if (!asset->surfaces[i].material)
				{
					return false;
				}
			}
		}

		if (stored.surfacesBounds)
		{
			asset->surfacesBounds = ReadRaw<Game::GfxSurfaceBounds>(reader, world->surfaceCount);
		}

		if (stored.smodelDrawInsts)
		{
			const auto* const storedModels = reader.ReadArray<Game::X86::GfxStaticModelDrawInst>(asset->smodelCount);
			asset->smodelDrawInsts = ReadConverted(allocator, storedModels, asset->smodelCount);

			for (unsigned int i = 0; i < asset->smodelCount; ++i)
			{
				if (!storedModels[i].model)
				{
					continue;
				}

				asset->smodelDrawInsts[i].model = FindNamedAsset<Game::XModel>(reader, Game::ASSET_TYPE_XMODEL, builder);

				if (!asset->smodelDrawInsts[i].model)
				{
					return false;
				}
			}
		}

		MarkRuntime(stored.surfaceMaterials, asset->surfaceMaterials);
		MarkRuntime(stored.surfaceCastsSunShadow, asset->surfaceCastsSunShadow);

		return true;
	}

	static Game::GfxWorld* TryReadGfxWorld(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();
		Utils::Stream::Reader reader(allocator, file.GetBuffer());

		try
		{
			const auto magic = reader.Read<std::uint64_t>();

			if (std::memcmp(&magic, "IW4xGfxW", 8))
			{
				Components::Logger::Error("Reading gfxworld '{}' failed, header is invalid!\n", name);
				return nullptr;
			}

			const auto version = reader.Read<std::int32_t>();

			if (version > IW4X_GFXMAP_VERSION)
			{
				Components::Logger::Error("Reading gfxworld '{}' failed, expected version is {}, but it was {}!\n", name, IW4X_GFXMAP_VERSION, version);
				return nullptr;
			}

			const auto* const stored = reader.ReadObject<Game::X86::GfxWorld>();

			auto* const asset = allocator->Allocate<Game::GfxWorld>();
			*asset = Game::X86::Convert(*stored);

			if (stored->name)
			{
				asset->name = reader.ReadCString();
			}

			if (stored->baseName)
			{
				asset->baseName = reader.ReadCString();
			}

			if (stored->skies)
			{
				const auto* const storedSkies = reader.ReadArray<Game::X86::GfxSky>(asset->skyCount);
				asset->skies = ReadConverted(allocator, storedSkies, asset->skyCount);

				for (int i = 0; i < asset->skyCount; ++i)
				{
					auto* const sky = &asset->skies[i];

					if (storedSkies[i].skyStartSurfs)
					{
						sky->skyStartSurfs = reader.ReadArray<int>(sky->skySurfCount);
					}

					if (storedSkies[i].skyImage)
					{
						sky->skyImage = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);
					}
				}
			}

			if (!asset->name)
			{
				Components::Logger::Error("Reading gfxworld '{}' failed, it has no name\n", name);
				return nullptr;
			}

			if (stored->dpvsPlanes.planes)
			{
				asset->dpvsPlanes.planes = ReadRaw<Game::cplane_s>(reader, asset->planeCount);

				const auto* const clipMap = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_CLIPMAP_MP, asset->name, builder).clipMap;

				if (!clipMap || static_cast<int>(clipMap->planeCount) != asset->planeCount)
				{
					Components::Logger::Error("GfxWorld dpvs planes not mapped. This shouldn't happen. Make sure to load the ClipMap first! They are co-dependant\n");
					return nullptr;
				}

				asset->dpvsPlanes.planes = clipMap->planes;
			}

			if (stored->dpvsPlanes.nodes)
			{
				asset->dpvsPlanes.nodes = reader.ReadArray<unsigned short>(asset->nodeCount);
			}

			MarkRuntime(stored->dpvsPlanes.sceneEntCellBits, asset->dpvsPlanes.sceneEntCellBits);

			const auto cellCount = asset->dpvsPlanes.cellCount;

			if (stored->aabbTreeCounts)
			{
				asset->aabbTreeCounts = ReadRaw<Game::GfxCellTreeCount>(reader, cellCount);
			}

			if (stored->aabbTrees)
			{
				if (!asset->aabbTreeCounts)
				{
					Components::Logger::Error("Reading gfxworld '{}' failed, it has aabb trees and no counts\n", name);
					return nullptr;
				}

				const auto* const storedCellTrees = reader.ReadArray<Game::X86::GfxCellTree>(cellCount);
				asset->aabbTrees = allocator->AllocateArray<Game::GfxCellTree>(cellCount);

				std::unordered_map<std::uint32_t, unsigned short*> smodelIndexes;

				for (int i = 0; i < cellCount; ++i)
				{
					if (!storedCellTrees[i].aabbTree)
					{
						continue;
					}

					const auto treeCount = asset->aabbTreeCounts[i].aabbTreeCount;
					const auto* const storedTrees = reader.ReadArray<Game::X86::GfxAabbTree>(treeCount);

					auto* const trees = allocator->AllocateArray<Game::GfxAabbTree>(treeCount);
					asset->aabbTrees[i].aabbTree = trees;

					for (int j = 0; j < treeCount; ++j)
					{
						if (!TryConvertStoredAabbTree(storedTrees[j], trees[j]))
						{
							Components::Logger::Error("Reading gfxworld '{}' failed, an aabb tree's childrenOffset is not a whole number of nodes\n", name);
							return nullptr;
						}

						const auto storedIndexes = storedTrees[j].smodelIndexes;

						if (!storedIndexes)
						{
							continue;
						}

						const auto mapped = smodelIndexes.find(storedIndexes);

						if (mapped != smodelIndexes.end())
						{
							reader.ReadArray<unsigned short>(trees[j].smodelIndexCount);
							trees[j].smodelIndexes = mapped->second;
							continue;
						}

						trees[j].smodelIndexes = reader.ReadArray<unsigned short>(trees[j].smodelIndexCount);

						for (unsigned short k = 0; k < trees[j].smodelIndexCount; ++k)
						{
							smodelIndexes[storedIndexes + 2u * k] = &trees[j].smodelIndexes[k];
						}
					}
				}
			}

			if (stored->cells)
			{
				const auto* const storedCells = reader.ReadArray<Game::X86::GfxCell>(cellCount);
				asset->cells = ReadConverted(allocator, storedCells, cellCount);

				for (int i = 0; i < cellCount; ++i)
				{
					auto* const cell = &asset->cells[i];

					if (storedCells[i].portals)
					{
						const auto* const storedPortals = reader.ReadArray<Game::X86::GfxPortal>(cell->portalCount);
						cell->portals = ReadConverted(allocator, storedPortals, cell->portalCount);

						for (int j = 0; j < cell->portalCount; ++j)
						{
							if (storedPortals[j].vertices)
							{
								cell->portals[j].vertices = reader.ReadArray<Game::vec3_t>(static_cast<unsigned char>(cell->portals[j].vertexCount));
							}
						}
					}

					if (storedCells[i].reflectionProbes)
					{
						cell->reflectionProbes = reader.ReadArray<char>(static_cast<unsigned char>(cell->reflectionProbeCount));
					}
				}
			}

			if (!TryReadGfxWorldDraw(stored->draw, &asset->draw, reader, builder))
			{
				Components::Logger::Error("Could not read world draw for {}\n", name);
				return nullptr;
			}

			auto* const lightGrid = &asset->lightGrid;

			if (stored->lightGrid.rowDataStart)
			{
				if (lightGrid->rowAxis >= std::size(lightGrid->mins))
				{
					Components::Logger::Error("Reading gfxworld '{}' failed, its light grid row axis is {}\n", name, lightGrid->rowAxis);
					return nullptr;
				}

				lightGrid->rowDataStart = reader.ReadArray<unsigned short>(static_cast<std::size_t>(lightGrid->maxs[lightGrid->rowAxis] - lightGrid->mins[lightGrid->rowAxis]) + 1);
			}

			if (stored->lightGrid.rawRowData)
			{
				lightGrid->rawRowData = reader.ReadArray<char>(lightGrid->rawRowDataSize);
			}

			if (stored->lightGrid.entries)
			{
				lightGrid->entries = ReadRaw<Game::GfxLightGridEntry>(reader, lightGrid->entryCount);
			}

			if (stored->lightGrid.colors)
			{
				lightGrid->colors = ReadRaw<Game::GfxLightGridColors>(reader, lightGrid->colorCount);
			}

			if (stored->models)
			{
				asset->models = ReadRaw<Game::GfxBrushModel>(reader, asset->modelCount);
			}

			if (stored->materialMemory)
			{
				const auto* const storedMemory = reader.ReadArray<Game::X86::MaterialMemory>(asset->materialMemoryCount);
				asset->materialMemory = ReadConverted(allocator, storedMemory, asset->materialMemoryCount);

				for (int i = 0; i < asset->materialMemoryCount; ++i)
				{
					if (storedMemory[i].material)
					{
						asset->materialMemory[i].material = FindNamedAsset<Game::Material>(reader, Game::ASSET_TYPE_MATERIAL, builder);
					}
				}
			}

			if (stored->sun.spriteMaterial)
			{
				asset->sun.spriteMaterial = FindNamedAsset<Game::Material>(reader, Game::ASSET_TYPE_MATERIAL, builder);
			}

			if (stored->sun.flareMaterial)
			{
				asset->sun.flareMaterial = FindNamedAsset<Game::Material>(reader, Game::ASSET_TYPE_MATERIAL, builder);
			}

			if (stored->outdoorImage)
			{
				asset->outdoorImage = FindNamedAsset<Game::GfxImage>(reader, Game::ASSET_TYPE_IMAGE, builder);
			}

			MarkRuntime(stored->cellCasterBits, asset->cellCasterBits);
			MarkRuntime(stored->cellHasSunLitSurfsBits, asset->cellHasSunLitSurfsBits);
			MarkRuntime(stored->sceneDynModel, asset->sceneDynModel);
			MarkRuntime(stored->sceneDynBrush, asset->sceneDynBrush);
			MarkRuntime(stored->primaryLightEntityShadowVis, asset->primaryLightEntityShadowVis);
			MarkRuntime(stored->primaryLightDynEntShadowVis[0], asset->primaryLightDynEntShadowVis[0]);
			MarkRuntime(stored->primaryLightDynEntShadowVis[1], asset->primaryLightDynEntShadowVis[1]);
			MarkRuntime(stored->nonSunPrimaryLightForModelDynEnt, asset->nonSunPrimaryLightForModelDynEnt);

			if (asset->primaryLightCount > 0)
			{
				MarkRuntime(1, asset->primaryLightEntityShadowVis);
			}

			if (asset->dpvsDyn.dynEntClientCount[0] > 0)
			{
				MarkRuntime(1, asset->sceneDynModel);
				MarkRuntime(1, asset->primaryLightDynEntShadowVis[0]);
				MarkRuntime(1, asset->nonSunPrimaryLightForModelDynEnt);
			}

			if (asset->dpvsDyn.dynEntClientCount[1] > 0)
			{
				MarkRuntime(1, asset->sceneDynBrush);
				MarkRuntime(1, asset->primaryLightDynEntShadowVis[1]);
			}

			if (stored->shadowGeom)
			{
				const auto* const storedShadows = reader.ReadArray<Game::X86::GfxShadowGeometry>(asset->primaryLightCount);
				asset->shadowGeom = ReadConverted(allocator, storedShadows, asset->primaryLightCount);

				for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
				{
					auto* const shadowGeometry = &asset->shadowGeom[i];

					if (storedShadows[i].sortedSurfIndex)
					{
						shadowGeometry->sortedSurfIndex = reader.ReadArray<unsigned short>(shadowGeometry->surfaceCount);
					}

					if (storedShadows[i].smodelIndex)
					{
						shadowGeometry->smodelIndex = reader.ReadArray<unsigned short>(shadowGeometry->smodelCount);
					}
				}
			}

			if (stored->lightRegion)
			{
				const auto* const storedRegions = reader.ReadArray<Game::X86::GfxLightRegion>(asset->primaryLightCount);
				asset->lightRegion = ReadConverted(allocator, storedRegions, asset->primaryLightCount);

				for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
				{
					auto* const lightRegion = &asset->lightRegion[i];

					if (!storedRegions[i].hulls)
					{
						continue;
					}

					const auto* const storedHulls = reader.ReadArray<Game::X86::GfxLightRegionHull>(lightRegion->hullCount);
					lightRegion->hulls = ReadConverted(allocator, storedHulls, lightRegion->hullCount);

					for (unsigned int j = 0; j < lightRegion->hullCount; ++j)
					{
						if (storedHulls[j].axis)
						{
							lightRegion->hulls[j].axis = ReadRaw<Game::GfxLightRegionAxis>(reader, lightRegion->hulls[j].axisCount);
						}
					}
				}
			}

			if (!TryReadDpvsStatic(stored->dpvs, asset, reader, builder))
			{
				Components::Logger::Error("Could not read DPVS static for {}\n", name);
				return nullptr;
			}

			for (int i = 0; i < 2; ++i)
			{
				MarkRuntime(stored->dpvsDyn.dynEntCellBits[i], asset->dpvsDyn.dynEntCellBits[i]);

				for (int j = 0; j < 3; ++j)
				{
					MarkRuntime(stored->dpvsDyn.dynEntVisData[i][j], asset->dpvsDyn.dynEntVisData[i][j]);
				}
			}

			if (stored->heroOnlyLights)
			{
				asset->heroOnlyLights = ReadRaw<Game::GfxHeroOnlyLight>(reader, asset->heroOnlyLightCount);
			}

			return asset;
		}
		catch (const std::exception& ex)
		{
			Components::Logger::Error("Reading gfxworld '{}' failed: {}\n", name, ex.what());
			return nullptr;
		}
	}

	void IGfxWorld::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->gfxWorld = TryReadGfxWorld(name, builder);
	}

	void IGfxWorld::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.gfxWorld;

		if (asset->draw.reflectionProbes)
		{
			for (unsigned int i = 0; i < asset->draw.reflectionProbeCount; ++i)
			{
				if (asset->draw.reflectionProbes[i])
				{
					builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->draw.reflectionProbes[i]);
				}
			}
		}

		if (asset->draw.lightmaps)
		{
			for (int i = 0; i < asset->draw.lightmapCount; ++i)
			{
				if (asset->draw.lightmaps[i].primary)
				{
					builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->draw.lightmaps[i].primary);
				}

				if (asset->draw.lightmaps[i].secondary)
				{
					builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->draw.lightmaps[i].secondary);
				}
			}
		}

		if (asset->draw.lightmapOverridePrimary)
		{
			builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->draw.lightmapOverridePrimary);
		}

		if (asset->draw.lightmapOverrideSecondary)
		{
			builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->draw.lightmapOverrideSecondary);
		}

		if (asset->sun.spriteMaterial)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->sun.spriteMaterial);
		}

		if (asset->sun.flareMaterial)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->sun.flareMaterial);
		}

		if (asset->skies)
		{
			for (int i = 0; i < asset->skyCount; ++i)
			{
				if (asset->skies[i].skyImage)
				{
					builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->skies[i].skyImage);
				}
			}
		}

		if (asset->materialMemory)
		{
			for (int i = 0; i < asset->materialMemoryCount; ++i)
			{
				if (asset->materialMemory[i].material)
				{
					builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->materialMemory[i].material);
				}
			}
		}

		if (asset->outdoorImage)
		{
			builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->outdoorImage);
		}

		if (asset->dpvs.surfaces)
		{
			for (unsigned int i = 0; i < asset->surfaceCount; ++i)
			{
				if (asset->dpvs.surfaces[i].material)
				{
					builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->dpvs.surfaces[i].material);
				}
			}
		}

		if (asset->dpvs.smodelDrawInsts)
		{
			for (unsigned int i = 0; i < asset->dpvs.smodelCount; ++i)
			{
				if (asset->dpvs.smodelDrawInsts[i].model)
				{
					builder->LoadAsset(Game::ASSET_TYPE_XMODEL, asset->dpvs.smodelDrawInsts[i].model);
				}
			}
		}
	}

	void IGfxWorld::SaveGfxWorldDpvsPlanes(const Game::GfxWorld* world, const Game::GfxWorldDpvsPlanes* asset, Game::X86::GfxWorldDpvsPlanes* dest, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxWorldDpvsPlanes, 16);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("GfxWorldDpvsPlanes");

		if (asset->planes)
		{
			if (builder->HasPointer(asset->planes))
			{
				dest->planes = builder->GetPointer(asset->planes);
			}
			else
			{
				AssertSize(Game::X86::cplane_s, 20);

				buffer->Align(Utils::Stream::ALIGN_4);

				for (int i = 0; i < world->planeCount; ++i)
				{
					builder->StorePointer(&asset->planes[i]);
					SaveRaw(buffer, &asset->planes[i], 1);
				}

				Utils::Stream::ClearPointer(&dest->planes);
			}
		}

		if (asset->nodes)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->nodes, static_cast<std::size_t>(world->nodeCount));
			Utils::Stream::ClearPointer(&dest->nodes);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (asset->sceneEntCellBits)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->sceneEntCellBits, 1, static_cast<std::size_t>(asset->cellCount) << 11);
			Utils::Stream::ClearPointer(&dest->sceneEntCellBits);
		}

		buffer->PopBlock();
		SaveLogExit();
	}

	void IGfxWorld::SaveGfxWorldDraw(const Game::GfxWorldDraw* asset, Game::X86::GfxWorldDraw* dest, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxWorldDraw, 72);
		SaveLogEnter("GfxWorldDraw");

		auto* const buffer = builder->GetBuffer();

		if (asset->reflectionProbes)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const imageDest = buffer->Dest<std::uint32_t>();
			buffer->SaveNull(sizeof(std::uint32_t) * asset->reflectionProbeCount);

			for (unsigned int i = 0; i < asset->reflectionProbeCount; ++i)
			{
				if (asset->reflectionProbes[i])
				{
					imageDest[i] = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, asset->reflectionProbes[i]);
				}
			}

			Utils::Stream::ClearPointer(&dest->reflectionProbes);
		}

		if (asset->reflectionProbeOrigins)
		{
			AssertSize(Game::X86::GfxReflectionProbe, 12);
			SaveLogEnter("GfxReflectionProbe");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->reflectionProbeOrigins, asset->reflectionProbeCount);
			Utils::Stream::ClearPointer(&dest->reflectionProbeOrigins);

			SaveLogExit();
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (asset->reflectionProbeTextures)
		{
			AssertSize(Game::X86::GfxTexture, 4);
			SaveLogEnter("GfxRawTexture");

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->reflectionProbeTextures, sizeof(Game::X86::GfxTexture), asset->reflectionProbeCount);
			Utils::Stream::ClearPointer(&dest->reflectionProbeTextures);

			SaveLogExit();
		}

		buffer->PopBlock();

		if (asset->lightmaps)
		{
			AssertSize(Game::X86::GfxLightmapArray, 8);
			SaveLogEnter("GfxLightmapArray");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const lightmapArrayDestTable = SaveConverted(buffer, asset->lightmaps, static_cast<std::size_t>(asset->lightmapCount));

			for (int i = 0; i < asset->lightmapCount; ++i)
			{
				auto* const lightmapArrayDest = &lightmapArrayDestTable[i];
				const auto* const lightmapArray = &asset->lightmaps[i];

				if (lightmapArray->primary)
				{
					lightmapArrayDest->primary = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, lightmapArray->primary);
				}

				if (lightmapArray->secondary)
				{
					lightmapArrayDest->secondary = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, lightmapArray->secondary);
				}
			}

			Utils::Stream::ClearPointer(&dest->lightmaps);
			SaveLogExit();
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (asset->lightmapPrimaryTextures)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->lightmapPrimaryTextures, sizeof(Game::X86::GfxTexture), static_cast<std::size_t>(asset->lightmapCount));
			Utils::Stream::ClearPointer(&dest->lightmapPrimaryTextures);
		}

		if (asset->lightmapSecondaryTextures)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->lightmapSecondaryTextures, sizeof(Game::X86::GfxTexture), static_cast<std::size_t>(asset->lightmapCount));
			Utils::Stream::ClearPointer(&dest->lightmapSecondaryTextures);
		}

		buffer->PopBlock();

		if (asset->lightmapOverridePrimary)
		{
			dest->lightmapOverridePrimary = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, asset->lightmapOverridePrimary);
		}

		if (asset->lightmapOverrideSecondary)
		{
			dest->lightmapOverrideSecondary = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, asset->lightmapOverrideSecondary);
		}

		if (asset->vd.vertices)
		{
			AssertSize(Game::X86::GfxWorldVertex, 44);
			SaveLogEnter("GfxWorldVertex");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->vd.vertices, asset->vertexCount);
			Utils::Stream::ClearPointer(&dest->vd.vertices);

			SaveLogExit();
		}

		if (asset->vld.data)
		{
			buffer->SaveArray(asset->vld.data, asset->vertexLayerDataSize);
			Utils::Stream::ClearPointer(&dest->vld.data);
		}

		if (asset->indices)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->indices, static_cast<std::size_t>(asset->indexCount));
			Utils::Stream::ClearPointer(&dest->indices);
		}

		SaveLogExit();
	}

	void IGfxWorld::SaveGfxLightGrid(const Game::GfxLightGrid* asset, Game::X86::GfxLightGrid* dest, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxLightGrid, 56);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("GfxLightGrid");

		if (asset->rowDataStart)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->rowDataStart, static_cast<std::size_t>(asset->maxs[asset->rowAxis] - asset->mins[asset->rowAxis]) + 1);
			Utils::Stream::ClearPointer(&dest->rowDataStart);
		}

		if (asset->rawRowData)
		{
			buffer->SaveArray(asset->rawRowData, asset->rawRowDataSize);
			Utils::Stream::ClearPointer(&dest->rawRowData);
		}

		if (asset->entries)
		{
			AssertSize(Game::X86::GfxLightGridEntry, 4);
			SaveLogEnter("GfxLightGridEntry");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->entries, asset->entryCount);
			Utils::Stream::ClearPointer(&dest->entries);

			SaveLogExit();
		}

		if (asset->colors)
		{
			AssertSize(Game::X86::GfxLightGridColors, 168);
			SaveLogEnter("GfxLightGridColors");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->colors, asset->colorCount);
			Utils::Stream::ClearPointer(&dest->colors);

			SaveLogExit();
		}

		SaveLogExit();
	}

	void IGfxWorld::Savesunflare_t(const Game::sunflare_t* asset, Game::X86::sunflare_t* dest, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::sunflare_t, 96);
		SaveLogEnter("sunflare_t");

		if (asset->spriteMaterial)
		{
			dest->spriteMaterial = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->spriteMaterial);
		}

		if (asset->flareMaterial)
		{
			dest->flareMaterial = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->flareMaterial);
		}

		SaveLogExit();
	}

	void IGfxWorld::SaveGfxWorldDpvsStatic(const Game::GfxWorld* world, const Game::GfxWorldDpvsStatic* asset, Game::X86::GfxWorldDpvsStatic* dest, int, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxWorldDpvsStatic, 108);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("GfxWorldDpvsStatic");

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		for (int i = 0; i < 3; ++i)
		{
			if (asset->smodelVisData[i])
			{
				buffer->SaveArray(asset->smodelVisData[i], asset->smodelCount);
				Utils::Stream::ClearPointer(&dest->smodelVisData[i]);
			}
		}

		for (int i = 0; i < 3; ++i)
		{
			if (asset->surfaceVisData[i])
			{
				buffer->SaveArray(asset->surfaceVisData[i], asset->staticSurfaceCount);
				Utils::Stream::ClearPointer(&dest->surfaceVisData[i]);
			}
		}

		buffer->PopBlock();

		if (asset->sortedSurfIndex)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->sortedSurfIndex, asset->staticSurfaceCount + asset->staticSurfaceCountNoDecal);
			Utils::Stream::ClearPointer(&dest->sortedSurfIndex);
		}

		if (asset->smodelInsts)
		{
			AssertSize(Game::X86::GfxStaticModelInst, 36);
			SaveLogEnter("GfxStaticModelInst");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->smodelInsts, asset->smodelCount);
			Utils::Stream::ClearPointer(&dest->smodelInsts);

			SaveLogExit();
		}

		if (asset->surfaces)
		{
			AssertSize(Game::X86::GfxSurface, 24);
			SaveLogEnter("GfxSurface");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destSurfaceTable = SaveConverted(buffer, asset->surfaces, world->surfaceCount);

			for (unsigned int i = 0; i < world->surfaceCount; ++i)
			{
				if (asset->surfaces[i].material)
				{
					destSurfaceTable[i].material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->surfaces[i].material);
				}
			}

			Utils::Stream::ClearPointer(&dest->surfaces);
			SaveLogExit();
		}

		if (asset->surfacesBounds)
		{
			AssertSize(Game::X86::GfxSurfaceBounds, 24);
			SaveLogEnter("GfxSurfaceBounds");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->surfacesBounds, world->surfaceCount);
			Utils::Stream::ClearPointer(&dest->surfacesBounds);

			SaveLogExit();
		}

		if (asset->smodelDrawInsts)
		{
			AssertSize(Game::X86::GfxStaticModelDrawInst, 76);
			SaveLogEnter("GfxStaticModelDrawInst");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destModelTable = SaveConverted(buffer, asset->smodelDrawInsts, asset->smodelCount);

			for (unsigned int i = 0; i < asset->smodelCount; ++i)
			{
				if (asset->smodelDrawInsts[i].model)
				{
					destModelTable[i].model = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL, asset->smodelDrawInsts[i].model);
				}
			}

			Utils::Stream::ClearPointer(&dest->smodelDrawInsts);
			SaveLogExit();
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (asset->surfaceMaterials)
		{
			AssertSize(Game::X86::GfxDrawSurf, 8);
			SaveLogEnter("GfxDrawSurf");

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->surfaceMaterials, sizeof(Game::X86::GfxDrawSurf), world->surfaceCount);
			Utils::Stream::ClearPointer(&dest->surfaceMaterials);

			SaveLogExit();
		}

		if (asset->surfaceCastsSunShadow)
		{
			SaveLogEnter("GfxDrawSurf");

			buffer->Align(Utils::Stream::ALIGN_128);
			buffer->Save(asset->surfaceCastsSunShadow, 4, asset->surfaceVisDataCount);
			Utils::Stream::ClearPointer(&dest->surfaceCastsSunShadow);

			SaveLogExit();
		}

		buffer->PopBlock();
		SaveLogExit();
	}

	void IGfxWorld::SaveGfxWorldDpvsDynamic(const Game::GfxWorldDpvsDynamic* asset, Game::X86::GfxWorldDpvsDynamic* dest, int cellCount, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxWorldDpvsDynamic, 48);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("GfxWorldDpvsDynamic");

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		for (int i = 0; i < 2; ++i)
		{
			if (asset->dynEntCellBits[i])
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->Save(asset->dynEntCellBits[i], 4, static_cast<std::size_t>(asset->dynEntClientWordCount[i]) * static_cast<std::size_t>(cellCount));
				Utils::Stream::ClearPointer(&dest->dynEntCellBits[i]);
			}
		}

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 2; ++j)
			{
				if (asset->dynEntVisData[j][i])
				{
					buffer->Align(Utils::Stream::ALIGN_16);
					buffer->Save(asset->dynEntVisData[j][i], 32, asset->dynEntClientWordCount[j]);
					Utils::Stream::ClearPointer(&dest->dynEntVisData[j][i]);
				}
			}
		}

		buffer->PopBlock();
		SaveLogExit();
	}

	void IGfxWorld::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::GfxWorld, 628);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("GfxWorld");

		const auto* const asset = header.gfxWorld;
		auto* const dest = buffer->Dest<Game::X86::GfxWorld>();

		const auto world = Game::X86::Convert(*asset);
		buffer->Save(&world);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->baseName)
		{
			buffer->SaveString(asset->baseName);
			Utils::Stream::ClearPointer(&dest->baseName);
		}

		if (asset->skies)
		{
			AssertSize(Game::X86::GfxSky, 16);
			SaveLogEnter("GfxSky");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destSkyTable = SaveConverted(buffer, asset->skies, static_cast<std::size_t>(asset->skyCount));

			for (int i = 0; i < asset->skyCount; ++i)
			{
				auto* const destSky = &destSkyTable[i];
				const auto* const sky = &asset->skies[i];

				if (sky->skyStartSurfs)
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->SaveArray(sky->skyStartSurfs, static_cast<std::size_t>(sky->skySurfCount));
					Utils::Stream::ClearPointer(&destSky->skyStartSurfs);
				}

				if (sky->skyImage)
				{
					destSky->skyImage = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, sky->skyImage);
				}
			}

			Utils::Stream::ClearPointer(&dest->skies);
			SaveLogExit();
		}

		this->SaveGfxWorldDpvsPlanes(asset, &asset->dpvsPlanes, &dest->dpvsPlanes, builder);

		const int cellCount = asset->dpvsPlanes.cellCount;

		if (asset->aabbTreeCounts)
		{
			AssertSize(Game::X86::GfxCellTreeCount, 4);
			SaveLogEnter("GfxCellTreeCount");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->aabbTreeCounts, static_cast<std::size_t>(cellCount));
			Utils::Stream::ClearPointer(&dest->aabbTreeCounts);

			SaveLogExit();
		}

		if (asset->aabbTrees)
		{
			AssertSize(Game::X86::GfxCellTree, 4);
			SaveLogEnter("GfxCellTree");

			buffer->Align(Utils::Stream::ALIGN_128);

			auto* const destCellTreeTable = SaveConverted(buffer, asset->aabbTrees, static_cast<std::size_t>(cellCount));

			for (int i = 0; i < cellCount; ++i)
			{
				auto* const destCellTree = &destCellTreeTable[i];
				const auto* const cellTree = &asset->aabbTrees[i];

				if (!cellTree->aabbTree)
				{
					continue;
				}

				SaveLogEnter("GfxAabbTree");

				buffer->Align(Utils::Stream::ALIGN_4);

				const auto treeCount = asset->aabbTreeCounts[i].aabbTreeCount;
				auto* const destAabbTreeTable = buffer->Dest<Game::X86::GfxAabbTree>();

				for (int j = 0; j < treeCount; ++j)
				{
					const auto converted = ConvertAabbTree(cellTree->aabbTree[j]);
					buffer->Save(&converted);
				}

				for (int j = 0; j < treeCount; ++j)
				{
					auto* const destAabbTree = &destAabbTreeTable[j];
					const auto* const aabbTree = &cellTree->aabbTree[j];

					if (!aabbTree->smodelIndexes)
					{
						continue;
					}

					if (builder->HasPointer(aabbTree->smodelIndexes))
					{
						destAabbTree->smodelIndexes = builder->GetPointer(aabbTree->smodelIndexes);
						continue;
					}

					buffer->Align(Utils::Stream::ALIGN_2);

					for (unsigned short k = 0; k < aabbTree->smodelIndexCount; ++k)
					{
						builder->StorePointer(&aabbTree->smodelIndexes[k]);
						buffer->Save(&aabbTree->smodelIndexes[k]);
					}

					Utils::Stream::ClearPointer(&destAabbTree->smodelIndexes);
				}

				Utils::Stream::ClearPointer(&destCellTree->aabbTree);
				SaveLogExit();
			}

			Utils::Stream::ClearPointer(&dest->aabbTrees);
			SaveLogExit();
		}

		if (asset->cells)
		{
			AssertSize(Game::X86::GfxCell, 40);
			SaveLogEnter("GfxCell");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destCellTable = SaveConverted(buffer, asset->cells, static_cast<std::size_t>(cellCount));

			for (int i = 0; i < cellCount; ++i)
			{
				auto* const destCell = &destCellTable[i];
				const auto* const cell = &asset->cells[i];

				if (cell->portals)
				{
					AssertSize(Game::X86::GfxPortal, 60);
					SaveLogEnter("GfxPortal");

					buffer->Align(Utils::Stream::ALIGN_4);

					auto* const destPortalTable = SaveConverted(buffer, cell->portals, static_cast<std::size_t>(cell->portalCount));

					for (int j = 0; j < cell->portalCount; ++j)
					{
						const auto* const portal = &cell->portals[j];

						if (portal->vertices)
						{
							buffer->Align(Utils::Stream::ALIGN_4);
							buffer->SaveArray(portal->vertices, static_cast<unsigned char>(portal->vertexCount));
							Utils::Stream::ClearPointer(&destPortalTable[j].vertices);
						}
					}

					Utils::Stream::ClearPointer(&destCell->portals);
					SaveLogExit();
				}

				if (cell->reflectionProbes)
				{
					buffer->SaveArray(cell->reflectionProbes, static_cast<unsigned char>(cell->reflectionProbeCount));
					Utils::Stream::ClearPointer(&destCell->reflectionProbes);
				}
			}

			Utils::Stream::ClearPointer(&dest->cells);
			SaveLogExit();
		}

		this->SaveGfxWorldDraw(&asset->draw, &dest->draw, builder);
		this->SaveGfxLightGrid(&asset->lightGrid, &dest->lightGrid, builder);

		if (asset->models)
		{
			AssertSize(Game::X86::GfxBrushModel, 60);
			SaveLogEnter("GfxBrushModel");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->models, static_cast<std::size_t>(asset->modelCount));
			Utils::Stream::ClearPointer(&dest->models);

			SaveLogExit();
		}

		if (asset->materialMemory)
		{
			AssertSize(Game::X86::MaterialMemory, 8);
			SaveLogEnter("MaterialMemory");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destMaterialMemoryTable = SaveConverted(buffer, asset->materialMemory, static_cast<std::size_t>(asset->materialMemoryCount));

			for (int i = 0; i < asset->materialMemoryCount; ++i)
			{
				if (asset->materialMemory[i].material)
				{
					destMaterialMemoryTable[i].material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->materialMemory[i].material);
				}
			}

			Utils::Stream::ClearPointer(&dest->materialMemory);
			SaveLogExit();
		}

		this->Savesunflare_t(&asset->sun, &dest->sun, builder);

		if (asset->outdoorImage)
		{
			dest->outdoorImage = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, asset->outdoorImage);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		const auto cells = static_cast<std::size_t>(cellCount);

		if (asset->cellCasterBits)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->cellCasterBits, 4, cells * ((cells + 31) >> 5));
			Utils::Stream::ClearPointer(&dest->cellCasterBits);
		}

		if (asset->cellHasSunLitSurfsBits)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->cellHasSunLitSurfsBits, 4, (cells + 31) >> 5);
			Utils::Stream::ClearPointer(&dest->cellHasSunLitSurfsBits);
		}

		if (asset->sceneDynModel)
		{
			AssertSize(Game::X86::GfxSceneDynModel, 6);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->sceneDynModel, sizeof(Game::X86::GfxSceneDynModel), asset->dpvsDyn.dynEntClientCount[0]);
			Utils::Stream::ClearPointer(&dest->sceneDynModel);
		}

		if (asset->sceneDynBrush)
		{
			AssertSize(Game::X86::GfxSceneDynBrush, 4);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->sceneDynBrush, sizeof(Game::X86::GfxSceneDynBrush), asset->dpvsDyn.dynEntClientCount[1]);
			Utils::Stream::ClearPointer(&dest->sceneDynBrush);
		}

		const unsigned int nonSunLightCount = asset->primaryLightCount - 1 - asset->lastSunPrimaryLightIndex;

		if (asset->primaryLightEntityShadowVis)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(asset->primaryLightEntityShadowVis, 1, (asset->primaryLightCount + 0x1FFFF - asset->lastSunPrimaryLightIndex) << 15);
			Utils::Stream::ClearPointer(&dest->primaryLightEntityShadowVis);
		}

		for (int i = 0; i < 2; ++i)
		{
			if (asset->primaryLightDynEntShadowVis[i])
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->Save(asset->primaryLightDynEntShadowVis[i], 4, asset->dpvsDyn.dynEntClientCount[i] * nonSunLightCount);
				Utils::Stream::ClearPointer(&dest->primaryLightDynEntShadowVis[i]);
			}
		}

		if (asset->nonSunPrimaryLightForModelDynEnt)
		{
			buffer->SaveArray(asset->nonSunPrimaryLightForModelDynEnt, asset->dpvsDyn.dynEntClientCount[0]);
			Utils::Stream::ClearPointer(&dest->nonSunPrimaryLightForModelDynEnt);
		}

		buffer->PopBlock();

		if (asset->shadowGeom)
		{
			AssertSize(Game::X86::GfxShadowGeometry, 12);
			SaveLogEnter("GfxShadowGeometry");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destShadowGeometryTable = SaveConverted(buffer, asset->shadowGeom, asset->primaryLightCount);

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				auto* const destShadowGeometry = &destShadowGeometryTable[i];
				const auto* const shadowGeometry = &asset->shadowGeom[i];

				if (shadowGeometry->sortedSurfIndex)
				{
					buffer->Align(Utils::Stream::ALIGN_2);
					buffer->SaveArray(shadowGeometry->sortedSurfIndex, shadowGeometry->surfaceCount);
					Utils::Stream::ClearPointer(&destShadowGeometry->sortedSurfIndex);
				}

				if (shadowGeometry->smodelIndex)
				{
					buffer->Align(Utils::Stream::ALIGN_2);
					buffer->SaveArray(shadowGeometry->smodelIndex, shadowGeometry->smodelCount);
					Utils::Stream::ClearPointer(&destShadowGeometry->smodelIndex);
				}
			}

			Utils::Stream::ClearPointer(&dest->shadowGeom);
			SaveLogExit();
		}

		if (asset->lightRegion)
		{
			AssertSize(Game::X86::GfxLightRegion, 8);
			SaveLogEnter("GfxLightRegion");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destLightRegionTable = SaveConverted(buffer, asset->lightRegion, asset->primaryLightCount);

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				auto* const destLightRegion = &destLightRegionTable[i];
				const auto* const lightRegion = &asset->lightRegion[i];

				if (!lightRegion->hulls)
				{
					continue;
				}

				AssertSize(Game::X86::GfxLightRegionHull, 80);
				SaveLogEnter("GfxLightRegionHull");

				buffer->Align(Utils::Stream::ALIGN_4);

				auto* const destLightRegionHullTable = SaveConverted(buffer, lightRegion->hulls, lightRegion->hullCount);

				for (unsigned int j = 0; j < lightRegion->hullCount; ++j)
				{
					const auto* const lightRegionHull = &lightRegion->hulls[j];

					if (lightRegionHull->axis)
					{
						AssertSize(Game::X86::GfxLightRegionAxis, 20);
						SaveLogEnter("GfxLightRegionAxis");

						buffer->Align(Utils::Stream::ALIGN_4);
						SaveRaw(buffer, lightRegionHull->axis, lightRegionHull->axisCount);
						Utils::Stream::ClearPointer(&destLightRegionHullTable[j].axis);

						SaveLogExit();
					}
				}

				Utils::Stream::ClearPointer(&destLightRegion->hulls);
				SaveLogExit();
			}

			Utils::Stream::ClearPointer(&dest->lightRegion);
			SaveLogExit();
		}

		this->SaveGfxWorldDpvsStatic(asset, &asset->dpvs, &dest->dpvs, asset->dpvsPlanes.cellCount, builder);
		this->SaveGfxWorldDpvsDynamic(&asset->dpvsDyn, &dest->dpvsDyn, asset->dpvsPlanes.cellCount, builder);

		if (asset->heroOnlyLights)
		{
			AssertSize(Game::X86::GfxHeroOnlyLight, 56);

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->heroOnlyLights, asset->heroOnlyLightCount);
			Utils::Stream::ClearPointer(&dest->heroOnlyLights);
		}

		buffer->PopBlock();
		SaveLogExit();
	}

	static Game::X86::GfxWorld ToDisk(const Game::GfxWorld& asset)
	{
		auto world = Game::X86::Convert(asset);

		MarkPresent(world.name, asset.name);
		MarkPresent(world.baseName, asset.baseName);
		MarkPresent(world.skies, asset.skies);
		MarkPresent(world.dpvsPlanes.planes, asset.dpvsPlanes.planes);
		MarkPresent(world.dpvsPlanes.nodes, asset.dpvsPlanes.nodes);
		MarkPresent(world.dpvsPlanes.sceneEntCellBits, asset.dpvsPlanes.sceneEntCellBits);
		MarkPresent(world.aabbTreeCounts, asset.aabbTreeCounts);
		MarkPresent(world.aabbTrees, asset.aabbTrees);
		MarkPresent(world.cells, asset.cells);

		MarkPresent(world.draw.reflectionProbes, asset.draw.reflectionProbes);
		MarkPresent(world.draw.reflectionProbeOrigins, asset.draw.reflectionProbeOrigins);
		MarkPresent(world.draw.reflectionProbeTextures, asset.draw.reflectionProbeTextures);
		MarkPresent(world.draw.lightmaps, asset.draw.lightmaps);
		MarkPresent(world.draw.lightmapPrimaryTextures, asset.draw.lightmapPrimaryTextures);
		MarkPresent(world.draw.lightmapSecondaryTextures, asset.draw.lightmapSecondaryTextures);
		MarkPresent(world.draw.lightmapOverridePrimary, asset.draw.lightmapOverridePrimary);
		MarkPresent(world.draw.lightmapOverrideSecondary, asset.draw.lightmapOverrideSecondary);
		MarkPresent(world.draw.vd.vertices, asset.draw.vd.vertices);
		MarkPresent(world.draw.vld.data, asset.draw.vld.data);
		MarkPresent(world.draw.indices, asset.draw.indices);

		MarkPresent(world.lightGrid.rowDataStart, asset.lightGrid.rowDataStart);
		MarkPresent(world.lightGrid.rawRowData, asset.lightGrid.rawRowData);
		MarkPresent(world.lightGrid.entries, asset.lightGrid.entries);
		MarkPresent(world.lightGrid.colors, asset.lightGrid.colors);

		MarkPresent(world.models, asset.models);
		MarkPresent(world.materialMemory, asset.materialMemory);
		MarkPresent(world.sun.spriteMaterial, asset.sun.spriteMaterial);
		MarkPresent(world.sun.flareMaterial, asset.sun.flareMaterial);
		MarkPresent(world.outdoorImage, asset.outdoorImage);
		MarkPresent(world.cellCasterBits, asset.cellCasterBits);
		MarkPresent(world.cellHasSunLitSurfsBits, asset.cellHasSunLitSurfsBits);
		MarkPresent(world.sceneDynModel, asset.sceneDynModel);
		MarkPresent(world.sceneDynBrush, asset.sceneDynBrush);
		MarkPresent(world.primaryLightEntityShadowVis, asset.primaryLightEntityShadowVis);
		MarkPresent(world.primaryLightDynEntShadowVis[0], asset.primaryLightDynEntShadowVis[0]);
		MarkPresent(world.primaryLightDynEntShadowVis[1], asset.primaryLightDynEntShadowVis[1]);
		MarkPresent(world.nonSunPrimaryLightForModelDynEnt, asset.nonSunPrimaryLightForModelDynEnt);
		MarkPresent(world.shadowGeom, asset.shadowGeom);
		MarkPresent(world.lightRegion, asset.lightRegion);

		for (int i = 0; i < 3; ++i)
		{
			MarkPresent(world.dpvs.smodelVisData[i], asset.dpvs.smodelVisData[i]);
			MarkPresent(world.dpvs.surfaceVisData[i], asset.dpvs.surfaceVisData[i]);
		}

		MarkPresent(world.dpvs.sortedSurfIndex, asset.dpvs.sortedSurfIndex);
		MarkPresent(world.dpvs.smodelInsts, asset.dpvs.smodelInsts);
		MarkPresent(world.dpvs.surfaces, asset.dpvs.surfaces);
		MarkPresent(world.dpvs.surfacesBounds, asset.dpvs.surfacesBounds);
		MarkPresent(world.dpvs.smodelDrawInsts, asset.dpvs.smodelDrawInsts);
		MarkPresent(world.dpvs.surfaceMaterials, asset.dpvs.surfaceMaterials);
		MarkPresent(world.dpvs.surfaceCastsSunShadow, asset.dpvs.surfaceCastsSunShadow);

		for (int i = 0; i < 2; ++i)
		{
			MarkPresent(world.dpvsDyn.dynEntCellBits[i], asset.dpvsDyn.dynEntCellBits[i]);

			for (int j = 0; j < 3; ++j)
			{
				MarkPresent(world.dpvsDyn.dynEntVisData[i][j], asset.dpvsDyn.dynEntVisData[i][j]);
			}
		}

		MarkPresent(world.heroOnlyLights, asset.heroOnlyLights);

		return world;
	}

	static void WriteGfxWorldDraw(const Game::GfxWorldDraw* asset, Utils::Stream* buffer)
	{
		if (asset->reflectionProbes)
		{
			for (unsigned int i = 0; i < asset->reflectionProbeCount; ++i)
			{
				std::uint32_t probe = 0;
				MarkPresent(probe, asset->reflectionProbes[i]);
				buffer->Save(&probe);
			}

			for (unsigned int i = 0; i < asset->reflectionProbeCount; ++i)
			{
				if (asset->reflectionProbes[i])
				{
					buffer->SaveString(asset->reflectionProbes[i]->name);
				}
			}
		}

		if (asset->reflectionProbeOrigins)
		{
			SaveRaw(buffer, asset->reflectionProbeOrigins, asset->reflectionProbeCount);
		}

		if (asset->lightmaps)
		{
			for (int i = 0; i < asset->lightmapCount; ++i)
			{
				auto lightmap = Game::X86::Convert(asset->lightmaps[i]);
				MarkPresent(lightmap.primary, asset->lightmaps[i].primary);
				MarkPresent(lightmap.secondary, asset->lightmaps[i].secondary);
				buffer->Save(&lightmap);
			}

			for (int i = 0; i < asset->lightmapCount; ++i)
			{
				if (asset->lightmaps[i].primary)
				{
					buffer->SaveString(asset->lightmaps[i].primary->name);
				}

				if (asset->lightmaps[i].secondary)
				{
					buffer->SaveString(asset->lightmaps[i].secondary->name);
				}
			}
		}

		if (asset->lightmapOverridePrimary)
		{
			buffer->SaveString(asset->lightmapOverridePrimary->name);
		}

		if (asset->lightmapOverrideSecondary)
		{
			buffer->SaveString(asset->lightmapOverrideSecondary->name);
		}

		if (asset->vd.vertices)
		{
			SaveRaw(buffer, asset->vd.vertices, asset->vertexCount);
		}

		if (asset->vld.data)
		{
			buffer->SaveArray(asset->vld.data, asset->vertexLayerDataSize);
		}

		if (asset->indices)
		{
			buffer->SaveArray(asset->indices, static_cast<std::size_t>(asset->indexCount));
		}
	}

	static void WriteDpvsStatic(const Game::GfxWorld* world, Utils::Stream* buffer)
	{
		const auto* const asset = &world->dpvs;

		if (asset->sortedSurfIndex)
		{
			buffer->SaveArray(asset->sortedSurfIndex, asset->staticSurfaceCount + asset->staticSurfaceCountNoDecal);
		}

		if (asset->smodelInsts)
		{
			SaveRaw(buffer, asset->smodelInsts, asset->smodelCount);
		}

		if (asset->surfaces)
		{
			for (unsigned int i = 0; i < world->surfaceCount; ++i)
			{
				auto surface = Game::X86::Convert(asset->surfaces[i]);
				MarkPresent(surface.material, asset->surfaces[i].material);
				buffer->Save(&surface);
			}

			for (unsigned int i = 0; i < world->surfaceCount; ++i)
			{
				if (asset->surfaces[i].material)
				{
					buffer->SaveString(asset->surfaces[i].material->info.name);
				}
			}
		}

		if (asset->surfacesBounds)
		{
			SaveRaw(buffer, asset->surfacesBounds, world->surfaceCount);
		}

		if (asset->smodelDrawInsts)
		{
			for (unsigned int i = 0; i < asset->smodelCount; ++i)
			{
				auto model = Game::X86::Convert(asset->smodelDrawInsts[i]);
				MarkPresent(model.model, asset->smodelDrawInsts[i].model);
				buffer->Save(&model);
			}

			for (unsigned int i = 0; i < asset->smodelCount; ++i)
			{
				if (asset->smodelDrawInsts[i].model)
				{
					buffer->SaveString(asset->smodelDrawInsts[i].model->name);
				}
			}
		}
	}

	void IGfxWorld::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.gfxWorld;

		if (!asset->name)
		{
			return;
		}

		if (asset->aabbTrees && !asset->aabbTreeCounts)
		{
			Components::Logger::Error("Dumping gfxworld '{}' failed, it has aabb trees and no counts\n", asset->name);
			return;
		}

		Utils::Stream buffer;
		buffer.Save("IW4xGfxW", 8);
		buffer.SaveObject(IW4X_GFXMAP_VERSION);

		const auto world = ToDisk(*asset);
		buffer.Save(&world);

		buffer.SaveString(asset->name);

		if (asset->baseName)
		{
			buffer.SaveString(asset->baseName);
		}

		if (asset->skies)
		{
			for (int i = 0; i < asset->skyCount; ++i)
			{
				auto sky = Game::X86::Convert(asset->skies[i]);
				MarkPresent(sky.skyStartSurfs, asset->skies[i].skyStartSurfs);
				MarkPresent(sky.skyImage, asset->skies[i].skyImage);
				buffer.Save(&sky);
			}

			for (int i = 0; i < asset->skyCount; ++i)
			{
				const auto* const sky = &asset->skies[i];

				if (sky->skyStartSurfs)
				{
					buffer.SaveArray(sky->skyStartSurfs, static_cast<std::size_t>(sky->skySurfCount));
				}

				if (sky->skyImage)
				{
					buffer.SaveString(sky->skyImage->name);
				}
			}
		}

		if (asset->dpvsPlanes.planes)
		{
			SaveRaw(&buffer, asset->dpvsPlanes.planes, static_cast<std::size_t>(asset->planeCount));
		}

		if (asset->dpvsPlanes.nodes)
		{
			buffer.SaveArray(asset->dpvsPlanes.nodes, static_cast<std::size_t>(asset->nodeCount));
		}

		const int cellCount = asset->dpvsPlanes.cellCount;

		if (asset->aabbTreeCounts)
		{
			SaveRaw(&buffer, asset->aabbTreeCounts, static_cast<std::size_t>(cellCount));
		}

		if (asset->aabbTrees)
		{
			for (int i = 0; i < cellCount; ++i)
			{
				auto cellTree = Game::X86::Convert(asset->aabbTrees[i]);
				MarkPresent(cellTree.aabbTree, asset->aabbTrees[i].aabbTree);
				buffer.Save(&cellTree);
			}

			std::unordered_map<const unsigned short*, std::uint32_t> smodelIndexAddresses;
			std::uint32_t nextSmodelIndexAddress = 0x10000000;

			for (int i = 0; i < cellCount; ++i)
			{
				const auto* const trees = asset->aabbTrees[i].aabbTree;

				if (!trees)
				{
					continue;
				}

				const auto treeCount = asset->aabbTreeCounts[i].aabbTreeCount;

				for (int j = 0; j < treeCount; ++j)
				{
					auto tree = ConvertAabbTree(trees[j]);
					const auto* const indexes = trees[j].smodelIndexes;

					if (indexes)
					{
						const auto known = smodelIndexAddresses.find(indexes);

						if (known != smodelIndexAddresses.end())
						{
							tree.smodelIndexes = known->second;
						}
						else
						{
							tree.smodelIndexes = nextSmodelIndexAddress;

							for (unsigned short k = 0; k < trees[j].smodelIndexCount; ++k)
							{
								smodelIndexAddresses[&indexes[k]] = nextSmodelIndexAddress + 2u * k;
							}

							nextSmodelIndexAddress += 2u * trees[j].smodelIndexCount + 2u;
						}
					}

					buffer.Save(&tree);
				}

				for (int j = 0; j < treeCount; ++j)
				{
					if (trees[j].smodelIndexes)
					{
						buffer.SaveArray(trees[j].smodelIndexes, trees[j].smodelIndexCount);
					}
				}
			}
		}

		if (asset->cells)
		{
			for (int i = 0; i < cellCount; ++i)
			{
				auto cell = Game::X86::Convert(asset->cells[i]);
				MarkPresent(cell.portals, asset->cells[i].portals);
				MarkPresent(cell.reflectionProbes, asset->cells[i].reflectionProbes);
				buffer.Save(&cell);
			}

			for (int i = 0; i < cellCount; ++i)
			{
				const auto* const cell = &asset->cells[i];

				if (cell->portals)
				{
					for (int j = 0; j < cell->portalCount; ++j)
					{
						auto portal = Game::X86::Convert(cell->portals[j]);
						MarkPresent(portal.vertices, cell->portals[j].vertices);
						buffer.Save(&portal);
					}

					for (int j = 0; j < cell->portalCount; ++j)
					{
						if (cell->portals[j].vertices)
						{
							buffer.SaveArray(cell->portals[j].vertices, static_cast<unsigned char>(cell->portals[j].vertexCount));
						}
					}
				}

				if (cell->reflectionProbes)
				{
					buffer.SaveArray(cell->reflectionProbes, static_cast<unsigned char>(cell->reflectionProbeCount));
				}
			}
		}

		WriteGfxWorldDraw(&asset->draw, &buffer);

		const auto* const lightGrid = &asset->lightGrid;

		if (lightGrid->rowDataStart)
		{
			buffer.SaveArray(lightGrid->rowDataStart, static_cast<std::size_t>(lightGrid->maxs[lightGrid->rowAxis] - lightGrid->mins[lightGrid->rowAxis]) + 1);
		}

		if (lightGrid->rawRowData)
		{
			buffer.SaveArray(lightGrid->rawRowData, lightGrid->rawRowDataSize);
		}

		if (lightGrid->entries)
		{
			SaveRaw(&buffer, lightGrid->entries, lightGrid->entryCount);
		}

		if (lightGrid->colors)
		{
			SaveRaw(&buffer, lightGrid->colors, lightGrid->colorCount);
		}

		if (asset->models)
		{
			SaveRaw(&buffer, asset->models, static_cast<std::size_t>(asset->modelCount));
		}

		if (asset->materialMemory)
		{
			for (int i = 0; i < asset->materialMemoryCount; ++i)
			{
				auto memory = Game::X86::Convert(asset->materialMemory[i]);
				MarkPresent(memory.material, asset->materialMemory[i].material);
				buffer.Save(&memory);
			}

			for (int i = 0; i < asset->materialMemoryCount; ++i)
			{
				if (asset->materialMemory[i].material)
				{
					buffer.SaveString(asset->materialMemory[i].material->info.name);
				}
			}
		}

		if (asset->sun.spriteMaterial)
		{
			buffer.SaveString(asset->sun.spriteMaterial->info.name);
		}

		if (asset->sun.flareMaterial)
		{
			buffer.SaveString(asset->sun.flareMaterial->info.name);
		}

		if (asset->outdoorImage)
		{
			buffer.SaveString(asset->outdoorImage->name);
		}

		if (asset->shadowGeom)
		{
			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				auto shadowGeometry = Game::X86::Convert(asset->shadowGeom[i]);
				MarkPresent(shadowGeometry.sortedSurfIndex, asset->shadowGeom[i].sortedSurfIndex);
				MarkPresent(shadowGeometry.smodelIndex, asset->shadowGeom[i].smodelIndex);
				buffer.Save(&shadowGeometry);
			}

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				const auto* const shadowGeometry = &asset->shadowGeom[i];

				if (shadowGeometry->sortedSurfIndex)
				{
					buffer.SaveArray(shadowGeometry->sortedSurfIndex, shadowGeometry->surfaceCount);
				}

				if (shadowGeometry->smodelIndex)
				{
					buffer.SaveArray(shadowGeometry->smodelIndex, shadowGeometry->smodelCount);
				}
			}
		}

		if (asset->lightRegion)
		{
			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				auto lightRegion = Game::X86::Convert(asset->lightRegion[i]);
				MarkPresent(lightRegion.hulls, asset->lightRegion[i].hulls);
				buffer.Save(&lightRegion);
			}

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				const auto* const lightRegion = &asset->lightRegion[i];

				if (!lightRegion->hulls)
				{
					continue;
				}

				for (unsigned int j = 0; j < lightRegion->hullCount; ++j)
				{
					auto hull = Game::X86::Convert(lightRegion->hulls[j]);
					MarkPresent(hull.axis, lightRegion->hulls[j].axis);
					buffer.Save(&hull);
				}

				for (unsigned int j = 0; j < lightRegion->hullCount; ++j)
				{
					if (lightRegion->hulls[j].axis)
					{
						SaveRaw(&buffer, lightRegion->hulls[j].axis, lightRegion->hulls[j].axisCount);
					}
				}
			}
		}

		WriteDpvsStatic(asset, &buffer);

		if (asset->heroOnlyLights)
		{
			SaveRaw(&buffer, asset->heroOnlyLights, asset->heroOnlyLightCount);
		}

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(asset->name));

		if (!Utils::IO::WriteFile(path, buffer.ToBuffer()))
		{
			Components::Logger::Error("Dumping gfxworld '{}' failed, could not write {}\n", asset->name, path);
		}
	}
}
