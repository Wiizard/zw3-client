#include "STDInclude.hpp"

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#include "IXModel.hpp"

namespace Assets
{
	constexpr std::int32_t modelFileVersion = 10;
	constexpr std::int32_t modelFileOldestVersion = 9;
	constexpr char modelFileMagic[] = "IW4xModl";
	constexpr std::size_t modelFileMagicLength = 8;
	constexpr std::size_t planeSize = sizeof(Game::X86::cplane_s);

	static std::unordered_set<std::string> dumpedPaths;

	struct ModelFile
	{
		Utils::Stream::Reader& reader;
		Utils::Memory::Allocator& allocator;
		Components::ZoneBuilder::Zone* builder;
		std::unordered_map<const void*, void*> converted;
		std::unordered_set<const void*> singlePlanes;
	};

	template <typename Target, typename Source>
	static Target* ConvertOnce(ModelFile& file, const Source* records, std::size_t count)
	{
		const auto known = file.converted.find(records);

		if (known != file.converted.end())
		{
			return static_cast<Target*>(known->second);
		}

		auto* const targets = file.allocator.AllocateArray<Target>(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			targets[i] = Game::X86::Convert(records[i]);
		}

		file.converted[records] = targets;
		return targets;
	}

	static Game::XSurfaceCollisionTree* ReadCollisionTree(ModelFile& file)
	{
		const auto record = file.reader.Read<Game::X86::XSurfaceCollisionTree>();
		auto* const tree = file.allocator.Allocate<Game::XSurfaceCollisionTree>();
		*tree = Game::X86::Convert(record);

		if (record.nodes)
		{
			tree->nodes = file.reader.ReadArrayOnce<Game::XSurfaceCollisionNode>(record.nodeCount);
		}

		if (record.leafs)
		{
			tree->leafs = file.reader.ReadArrayOnce<Game::XSurfaceCollisionLeaf>(record.leafCount);
		}

		return tree;
	}

	static void ReadSurface(ModelFile& file, const Game::X86::XSurface& record, Game::XSurface& surface)
	{
		if (record.vertInfo.vertsBlend)
		{
			const auto& vertCount = surface.vertInfo.vertCount;
			const int blendCount = vertCount[0] + (vertCount[1] * 3) + (vertCount[2] * 5) + (vertCount[3] * 7);

			if (blendCount < 0)
			{
				throw std::runtime_error("negative blend count");
			}

			surface.vertInfo.vertsBlend = file.reader.ReadArrayOnce<unsigned short>(static_cast<std::size_t>(blendCount));
		}

		if (record.verts0)
		{
			surface.verts0 = file.reader.ReadArrayOnce<Game::GfxPackedVertex>(surface.vertCount);
		}

		if (record.vertList)
		{
			const auto* const lists = file.reader.ReadArrayOnce<Game::X86::XRigidVertList>(surface.vertListCount);
			surface.vertList = ConvertOnce<Game::XRigidVertList>(file, lists, surface.vertListCount);

			for (unsigned int i = 0; i < surface.vertListCount; ++i)
			{
				if (lists[i].collisionTree)
				{
					surface.vertList[i].collisionTree = ReadCollisionTree(file);
				}
			}
		}

		if (record.triIndices)
		{
			surface.triIndices = file.reader.ReadArrayOnce<unsigned short>(surface.triCount * 3u);
		}
	}

	static Game::XModelSurfs* ReadModelSurfs(ModelFile& file)
	{
		const auto record = file.reader.Read<Game::X86::XModelSurfs>();
		auto* const modelSurfs = file.allocator.Allocate<Game::XModelSurfs>();
		*modelSurfs = Game::X86::Convert(record);

		if (record.name)
		{
			modelSurfs->name = file.reader.ReadCString();
		}

		if (record.surfs)
		{
			const auto* const surfaces = file.reader.ReadArrayOnce<Game::X86::XSurface>(modelSurfs->numsurfs);
			modelSurfs->surfs = ConvertOnce<Game::XSurface>(file, surfaces, modelSurfs->numsurfs);

			for (unsigned short i = 0; i < modelSurfs->numsurfs; ++i)
			{
				ReadSurface(file, surfaces[i], modelSurfs->surfs[i]);
			}
		}

		return modelSurfs;
	}

	static bool TryJoinBrushPlanes(ModelFile& file, const Game::X86::BrushWrapper& record, const Game::X86::cbrushside_t* sides, Game::BrushWrapper* brush)
	{
		const auto numsides = brush->brush.numsides;
		auto* const planes = file.allocator.AllocateArray<Game::cplane_s>(numsides);
		std::vector<bool> isFilled(numsides, false);

		for (unsigned short j = 0; j < numsides; ++j)
		{
			if (!sides[j].plane)
			{
				continue;
			}

			const std::uint32_t distance = sides[j].plane - record.planes;
			const auto index = static_cast<std::uint32_t>(distance / planeSize);

			if (distance % planeSize || index >= numsides)
			{
				return false;
			}

			planes[index] = *brush->brush.sides[j].plane;
			brush->brush.sides[j].plane = &planes[index];
			isFilled[index] = true;
		}

		if (std::find(isFilled.begin(), isFilled.end(), false) != isFilled.end())
		{
			return false;
		}

		brush->planes = planes;
		return true;
	}

	static Game::BrushWrapper* ReadBrushWrapper(ModelFile& file)
	{
		const auto* const record = file.reader.ReadArrayOnce<Game::X86::BrushWrapper>(1);
		auto* const brush = ConvertOnce<Game::BrushWrapper>(file, record, 1);

		const Game::X86::cbrushside_t* sides = nullptr;

		if (record->brush.sides)
		{
			sides = file.reader.ReadArrayOnce<Game::X86::cbrushside_t>(brush->brush.numsides);
			brush->brush.sides = ConvertOnce<Game::cbrushside_t>(file, sides, brush->brush.numsides);

			for (unsigned short j = 0; j < brush->brush.numsides; ++j)
			{
				if (sides[j].plane)
				{
					brush->brush.sides[j].plane = file.reader.ReadArrayOnce<Game::cplane_s>(1);
					file.singlePlanes.insert(brush->brush.sides[j].plane);
				}
			}
		}

		if (record->brush.baseAdjacentSide)
		{
			brush->brush.baseAdjacentSide = file.reader.ReadArrayOnce<unsigned char>(static_cast<std::size_t>(brush->totalEdgeCount));
		}

		if (record->planes)
		{
			brush->planes = file.reader.ReadArrayOnce<Game::cplane_s>(brush->brush.numsides);

			const bool isJoinedToSide = file.singlePlanes.contains(brush->planes);

			if (isJoinedToSide && (!sides || !TryJoinBrushPlanes(file, *record, sides, brush)))
			{
				throw std::runtime_error("a brush's planes are a side's single plane and cannot be rebuilt");
			}
		}

		return brush;
	}

	static Game::PhysCollmap* ReadPhysCollmap(ModelFile& file)
	{
		const auto record = file.reader.Read<Game::X86::PhysCollmap>();
		auto* const collmap = file.allocator.Allocate<Game::PhysCollmap>();
		*collmap = Game::X86::Convert(record);

		if (record.name)
		{
			collmap->name = file.reader.ReadCString();
		}

		if (record.geoms)
		{
			const auto* const geoms = file.reader.ReadArray<Game::X86::PhysGeomInfo>(collmap->count);
			collmap->geoms = file.allocator.AllocateArray<Game::PhysGeomInfo>(collmap->count);

			for (unsigned int i = 0; i < collmap->count; ++i)
			{
				collmap->geoms[i] = Game::X86::Convert(geoms[i]);

				if (geoms[i].brushWrapper)
				{
					collmap->geoms[i].brushWrapper = ReadBrushWrapper(file);
				}
			}
		}

		return collmap;
	}

	static Game::XModel* ReadModel(ModelFile& file, const std::string& name)
	{
		auto& reader = file.reader;
		auto& allocator = file.allocator;

		const auto magic = reader.Read<std::uint64_t>();

		if (std::memcmp(&magic, modelFileMagic, modelFileMagicLength))
		{
			throw std::runtime_error("header is invalid");
		}

		const auto version = reader.Read<std::int32_t>();

		if (version < modelFileOldestVersion || version > modelFileVersion)
		{
			throw std::runtime_error(std::format("supported versions are {} to {}, but it was {}", modelFileOldestVersion, modelFileVersion, version));
		}

		const auto record = reader.Read<Game::X86::XModel>();

		if (record.numRootBones > record.numBones || record.numCollSurfs < 0)
		{
			throw std::runtime_error("its bone or collision surface counts are invalid");
		}

		auto* const model = allocator.Allocate<Game::XModel>();
		*model = Game::X86::Convert(record);

		if (record.name)
		{
			model->name = reader.ReadCString();
		}

		if (record.boneNames)
		{
			model->boneNames = allocator.AllocateArray<unsigned short>(model->numBones);

			for (unsigned char i = 0; i < model->numBones; ++i)
			{
				model->boneNames[i] = static_cast<unsigned short>(Game::SL_GetString(reader.ReadString().data(), 0));
			}
		}

		const auto childBoneCount = static_cast<std::size_t>(model->numBones - model->numRootBones);

		if (record.parentList)
		{
			model->parentList = reader.ReadArrayOnce<unsigned char>(childBoneCount);
		}

		if (record.quats)
		{
			model->quats = reader.ReadArrayOnce<short>(childBoneCount * 4);
		}

		if (record.trans)
		{
			model->trans = reader.ReadArrayOnce<float>(childBoneCount * 3);
		}

		if (record.partClassification)
		{
			model->partClassification = reader.ReadArrayOnce<unsigned char>(model->numBones);
		}

		if (record.baseMat)
		{
			model->baseMat = reader.ReadArrayOnce<Game::DObjAnimMat>(model->numBones);
		}

		if (record.materialHandles)
		{
			const auto* const handles = reader.ReadArray<std::uint32_t>(model->numsurfs);
			model->materialHandles = allocator.AllocateArray<Game::Material*>(model->numsurfs);

			for (unsigned char i = 0; i < model->numsurfs; ++i)
			{
				if (handles[i])
				{
					model->materialHandles[i] = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, reader.ReadString(), file.builder).material;
				}
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			if (!record.lodInfo[i].modelSurfs)
			{
				continue;
			}

			auto* modelSurfs = ReadModelSurfs(file);

			if (modelSurfs->name)
			{
				auto* const existingSurfs = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_XMODEL_SURFS, modelSurfs->name, file.builder).modelSurfs;

				if (existingSurfs)
				{
					modelSurfs = existingSurfs;
				}
			}

			model->lodInfo[i].modelSurfs = modelSurfs;
			model->lodInfo[i].surfs = modelSurfs->surfs;
		}

		if (record.collSurfs)
		{
			const auto* const collSurfs = reader.ReadArray<Game::X86::XModelCollSurf_s>(static_cast<std::size_t>(model->numCollSurfs));
			model->collSurfs = allocator.AllocateArray<Game::XModelCollSurf_s>(static_cast<std::size_t>(model->numCollSurfs));

			for (int i = 0; i < model->numCollSurfs; ++i)
			{
				model->collSurfs[i] = Game::X86::Convert(collSurfs[i]);

				if (collSurfs[i].collTris)
				{
					if (model->collSurfs[i].numCollTris < 0)
					{
						throw std::runtime_error("negative collision triangle count");
					}

					model->collSurfs[i].collTris = reader.ReadArray<Game::XModelCollTri_s>(static_cast<std::size_t>(model->collSurfs[i].numCollTris));
				}
			}
		}

		if (record.boneInfo)
		{
			model->boneInfo = reader.ReadArray<Game::XBoneInfo>(model->numBones);
		}

		if (record.physPreset)
		{
			Game::PhysPreset* embeddedPreset = nullptr;

			if (version == modelFileOldestVersion)
			{
				embeddedPreset = allocator.Allocate<Game::PhysPreset>();
				*embeddedPreset = Game::X86::Convert(reader.Read<Game::X86::PhysPreset>());
			}

			const auto presetName = reader.ReadString();
			auto* const preset = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_PHYSPRESET, presetName, file.builder).physPreset;

			if (preset)
			{
				model->physPreset = preset;
			}
			else if (embeddedPreset)
			{
				embeddedPreset->name = allocator.DuplicateString(presetName);
				model->physPreset = embeddedPreset;
			}
			else
			{
				Components::Logger::Error("Reading model '{}': physpreset '{}' was not found\n", name, presetName);
			}
		}

		if (record.physCollmap)
		{
			model->physCollmap = ReadPhysCollmap(file);
		}

		if (!reader.End())
		{
			throw std::runtime_error("remaining raw data found");
		}

		return model;
	}

	static Game::XModel* TryReadModelFile(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File modelFile(std::format("xmodel/{}.iw4xModel", name));

		if (!modelFile.Exists())
		{
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();
		Utils::Stream::Reader reader(allocator, modelFile.GetBuffer());
		ModelFile file{ reader, *allocator, builder, {}, {} };

		try
		{
			return ReadModel(file, name);
		}
		catch (const std::runtime_error& error)
		{
			Components::Logger::Error("Reading model '{}' failed, {}\n", name, error.what());
			return nullptr;
		}
	}

	static std::uint32_t StoredPointer(const void* pointer)
	{
		if (!pointer)
		{
			return 0;
		}

		const auto low = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));

		if (!low)
		{
			return 0xFFFFFFFF;
		}

		return low;
	}

	static bool SaveMarker(Utils::Stream& buffer, const void* data)
	{
		const auto known = buffer.dataPointers.find(data);

		if (known != buffer.dataPointers.end())
		{
			buffer.SaveByte(Utils::POINTER);
			buffer.SaveObject(known->second);
			return false;
		}

		buffer.SaveByte(Utils::FOLLOWING);
		buffer.dataPointers.insert_or_assign(data, static_cast<std::uint32_t>(buffer.Length()));
		return true;
	}

	static void WriteCollisionTree(Utils::Stream& buffer, const Game::XSurfaceCollisionTree* tree)
	{
		auto record = Game::X86::Convert(*tree);
		record.nodes = StoredPointer(tree->nodes);
		record.leafs = StoredPointer(tree->leafs);
		buffer.SaveObject(record);

		if (tree->nodes)
		{
			buffer.SaveArrayIfNotExisting(tree->nodes, tree->nodeCount);
		}

		if (tree->leafs)
		{
			buffer.SaveArrayIfNotExisting(tree->leafs, tree->leafCount);
		}
	}

	static void WriteSurface(Utils::Stream& buffer, const Game::XSurface* surface)
	{
		if (surface->vertInfo.vertsBlend)
		{
			const auto& vertCount = surface->vertInfo.vertCount;
			const int blendCount = vertCount[0] + (vertCount[1] * 3) + (vertCount[2] * 5) + (vertCount[3] * 7);
			buffer.SaveArrayIfNotExisting(surface->vertInfo.vertsBlend, static_cast<std::size_t>(blendCount));
		}

		if (surface->verts0)
		{
			buffer.SaveArrayIfNotExisting(surface->verts0, surface->vertCount);
		}

		if (surface->vertList)
		{
			if (SaveMarker(buffer, surface->vertList))
			{
				for (unsigned int i = 0; i < surface->vertListCount; ++i)
				{
					auto record = Game::X86::Convert(surface->vertList[i]);
					record.collisionTree = StoredPointer(surface->vertList[i].collisionTree);
					buffer.SaveObject(record);
				}
			}

			for (unsigned int i = 0; i < surface->vertListCount; ++i)
			{
				if (surface->vertList[i].collisionTree)
				{
					WriteCollisionTree(buffer, surface->vertList[i].collisionTree);
				}
			}
		}

		if (surface->triIndices)
		{
			buffer.SaveArrayIfNotExisting(surface->triIndices, surface->triCount * 3u);
		}
	}

	static void WriteModelSurfs(Utils::Stream& buffer, const Game::XModelSurfs* modelSurfs)
	{
		auto record = Game::X86::Convert(*modelSurfs);
		record.name = StoredPointer(modelSurfs->name);
		record.surfs = StoredPointer(modelSurfs->surfs);
		buffer.SaveObject(record);

		if (modelSurfs->name)
		{
			buffer.SaveString(modelSurfs->name);
		}

		if (!modelSurfs->surfs)
		{
			return;
		}

		if (SaveMarker(buffer, modelSurfs->surfs))
		{
			for (unsigned short i = 0; i < modelSurfs->numsurfs; ++i)
			{
				const auto* const surface = &modelSurfs->surfs[i];

				auto surfaceRecord = Game::X86::Convert(*surface);
				surfaceRecord.triIndices = StoredPointer(surface->triIndices);
				surfaceRecord.vertInfo.vertsBlend = StoredPointer(surface->vertInfo.vertsBlend);
				surfaceRecord.verts0 = StoredPointer(surface->verts0);
				surfaceRecord.vertList = StoredPointer(surface->vertList);
				buffer.SaveObject(surfaceRecord);
			}
		}

		for (unsigned short i = 0; i < modelSurfs->numsurfs; ++i)
		{
			WriteSurface(buffer, &modelSurfs->surfs[i]);
		}
	}

	static void WriteBrushWrapper(Utils::Stream& buffer, const Game::BrushWrapper* brush)
	{
		if (SaveMarker(buffer, brush))
		{
			auto record = Game::X86::Convert(*brush);
			record.brush.sides = StoredPointer(brush->brush.sides);
			record.brush.baseAdjacentSide = StoredPointer(brush->brush.baseAdjacentSide);
			record.planes = StoredPointer(brush->planes);
			buffer.SaveObject(record);
		}

		if (brush->brush.sides)
		{
			if (SaveMarker(buffer, brush->brush.sides))
			{
				for (unsigned short j = 0; j < brush->brush.numsides; ++j)
				{
					auto sideRecord = Game::X86::Convert(brush->brush.sides[j]);
					sideRecord.plane = StoredPointer(brush->brush.sides[j].plane);
					buffer.SaveObject(sideRecord);
				}
			}

			for (unsigned short j = 0; j < brush->brush.numsides; ++j)
			{
				if (brush->brush.sides[j].plane)
				{
					buffer.SaveArrayIfNotExisting(brush->brush.sides[j].plane, 1);
				}
			}
		}

		if (brush->brush.baseAdjacentSide)
		{
			buffer.SaveArrayIfNotExisting(brush->brush.baseAdjacentSide, static_cast<std::size_t>(brush->totalEdgeCount));
		}

		if (brush->planes)
		{
			buffer.SaveArrayIfNotExisting(brush->planes, brush->brush.numsides);
		}
	}

	static void WritePhysCollmap(Utils::Stream& buffer, const Game::PhysCollmap* collmap)
	{
		auto record = Game::X86::Convert(*collmap);
		record.name = StoredPointer(collmap->name);
		record.geoms = StoredPointer(collmap->geoms);
		buffer.SaveObject(record);

		if (collmap->name)
		{
			buffer.SaveString(collmap->name);
		}

		if (!collmap->geoms)
		{
			return;
		}

		for (unsigned int i = 0; i < collmap->count; ++i)
		{
			auto geomRecord = Game::X86::Convert(collmap->geoms[i]);
			geomRecord.brushWrapper = StoredPointer(collmap->geoms[i].brushWrapper);
			buffer.SaveObject(geomRecord);
		}

		for (unsigned int i = 0; i < collmap->count; ++i)
		{
			if (collmap->geoms[i].brushWrapper)
			{
				WriteBrushWrapper(buffer, collmap->geoms[i].brushWrapper);
			}
		}
	}

	template <typename Visit>
	static void VisitBlendIndices(const Game::XSurface* surface, Visit visit)
	{
		const auto& vertCount = surface->vertInfo.vertCount;
		unsigned int offset = 0;

		for (int i = 0; i < vertCount[0]; ++i)
		{
			visit(offset);

			offset += 1;
		}

		for (int i = 0; i < vertCount[1]; ++i)
		{
			visit(offset);
			visit(offset + 1);

			offset += 3;
		}

		for (int i = 0; i < vertCount[2]; ++i)
		{
			visit(offset);
			visit(offset + 1);
			visit(offset + 3);

			offset += 5;
		}

		for (int i = 0; i < vertCount[3]; ++i)
		{
			visit(offset);
			visit(offset + 1);
			visit(offset + 3);
			visit(offset + 5);

			offset += 7;
		}
	}

	void IXModel::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->model = TryReadModelFile(name, builder);

		if (!header->model)
		{
			header->model = Components::AssetHandler::FindLoadedAsset(Game::ASSET_TYPE_XMODEL, name.data()).model;
		}

		if (!header->model)
		{
			return;
		}

		if (Components::ZoneBuilder::zb_sp_to_mp.Get<bool>())
		{
			ConvertPlayerModelFromSingleplayerToMultiplayer(header->model, *builder->GetAllocator());
		}

		if (header->model->physCollmap)
		{
			Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_PHYSCOLLMAP, { header->model->physCollmap });
		}

		if (header->model->physPreset)
		{
			Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_PHYSPRESET, { header->model->physPreset });
		}

		for (unsigned char i = 0; i < header->model->numLods; ++i)
		{
			const auto& info = header->model->lodInfo[i];

			if (info.modelSurfs)
			{
				Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_XMODEL_SURFS, { info.modelSurfs });
			}
		}
	}

	void IXModel::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.model;

		if (asset->boneNames)
		{
			for (unsigned char i = 0; i < asset->numBones; ++i)
			{
				builder->AddScriptString(asset->boneNames[i]);
			}
		}

		if (asset->materialHandles)
		{
			for (unsigned char i = 0; i < asset->numsurfs; ++i)
			{
				if (asset->materialHandles[i])
				{
					builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->materialHandles[i]);
				}
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			if (asset->lodInfo[i].modelSurfs)
			{
				builder->LoadAsset(Game::ASSET_TYPE_XMODEL_SURFS, asset->lodInfo[i].modelSurfs);
			}
		}

		if (asset->physPreset)
		{
			builder->LoadAsset(Game::ASSET_TYPE_PHYSPRESET, asset->physPreset);
		}

		if (asset->physCollmap)
		{
			builder->LoadAsset(Game::ASSET_TYPE_PHYSCOLLMAP, asset->physCollmap);
		}
	}

	void IXModel::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::XModel, 304);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.model;
		auto* const dest = buffer->Dest<Game::X86::XModel>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->boneNames)
		{
			buffer->Align(Utils::Stream::ALIGN_2);

			auto* const destBoneNames = buffer->Dest<unsigned short>();
			buffer->SaveArray(asset->boneNames, asset->numBones);

			for (unsigned char i = 0; i < asset->numBones; ++i)
			{
				builder->MapScriptString(destBoneNames[i]);
			}

			Utils::Stream::ClearPointer(&dest->boneNames);
		}

		const auto childBoneCount = static_cast<std::size_t>(asset->numBones - asset->numRootBones);

		if (asset->parentList)
		{
			if (builder->HasPointer(asset->parentList))
			{
				dest->parentList = builder->GetPointer(asset->parentList);
			}
			else
			{
				builder->StorePointer(asset->parentList);
				buffer->Save(asset->parentList, childBoneCount);
				Utils::Stream::ClearPointer(&dest->parentList);
			}
		}

		if (asset->quats)
		{
			if (builder->HasPointer(asset->quats))
			{
				dest->quats = builder->GetPointer(asset->quats);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_2);
				builder->StorePointer(asset->quats);
				buffer->SaveArray(asset->quats, childBoneCount * 4);
				Utils::Stream::ClearPointer(&dest->quats);
			}
		}

		if (asset->trans)
		{
			if (builder->HasPointer(asset->trans))
			{
				dest->trans = builder->GetPointer(asset->trans);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(asset->trans);
				buffer->SaveArray(asset->trans, childBoneCount * 3);
				Utils::Stream::ClearPointer(&dest->trans);
			}
		}

		if (asset->partClassification)
		{
			if (builder->HasPointer(asset->partClassification))
			{
				dest->partClassification = builder->GetPointer(asset->partClassification);
			}
			else
			{
				builder->StorePointer(asset->partClassification);
				buffer->Save(asset->partClassification, asset->numBones);
				Utils::Stream::ClearPointer(&dest->partClassification);
			}
		}

		if (asset->baseMat)
		{
			AssertSize(Game::DObjAnimMat, 32);
			AssertSize(Game::X86::DObjAnimMat, 32);

			if (builder->HasPointer(asset->baseMat))
			{
				dest->baseMat = builder->GetPointer(asset->baseMat);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(asset->baseMat);
				buffer->SaveArray(asset->baseMat, asset->numBones);
				Utils::Stream::ClearPointer(&dest->baseMat);
			}
		}

		if (asset->materialHandles)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destMaterials = buffer->Dest<std::uint32_t>();
			const std::vector<std::uint32_t> emptyHandles(asset->numsurfs, 0);
			buffer->SaveArray(emptyHandles.data(), emptyHandles.size());

			for (unsigned char i = 0; i < asset->numsurfs; ++i)
			{
				if (asset->materialHandles[i])
				{
					destMaterials[i] = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->materialHandles[i]);
				}
			}

			Utils::Stream::ClearPointer(&dest->materialHandles);
		}

		AssertSize(Game::X86::XModelLodInfo, 44);

		for (int i = 0; i < 4; ++i)
		{
			if (asset->lodInfo[i].modelSurfs)
			{
				dest->lodInfo[i].modelSurfs = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL_SURFS, asset->lodInfo[i].modelSurfs);
			}
		}

		if (asset->collSurfs)
		{
			AssertSize(Game::X86::XModelCollSurf_s, 44);
			AssertSize(Game::XModelCollTri_s, 48);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destCollSurfs = buffer->Dest<Game::X86::XModelCollSurf_s>();

			for (int i = 0; i < asset->numCollSurfs; ++i)
			{
				const auto collSurfRecord = Game::X86::Convert(asset->collSurfs[i]);
				buffer->Save(&collSurfRecord);
			}

			for (int i = 0; i < asset->numCollSurfs; ++i)
			{
				const auto* const collSurf = &asset->collSurfs[i];

				if (collSurf->collTris)
				{
					buffer->Align(Utils::Stream::ALIGN_4);

					buffer->Save(collSurf->collTris, sizeof(Game::XModelCollTri_s), static_cast<std::size_t>(collSurf->numCollTris));
					Utils::Stream::ClearPointer(&destCollSurfs[i].collTris);
				}
			}

			Utils::Stream::ClearPointer(&dest->collSurfs);
		}

		if (asset->boneInfo)
		{
			AssertSize(Game::XBoneInfo, 28);
			AssertSize(Game::X86::XBoneInfo, 28);

			buffer->Align(Utils::Stream::ALIGN_4);

			buffer->SaveArray(asset->boneInfo, asset->numBones);
			Utils::Stream::ClearPointer(&dest->boneInfo);
		}

		if (asset->physPreset)
		{
			dest->physPreset = builder->SaveSubAsset(Game::ASSET_TYPE_PHYSPRESET, asset->physPreset);
		}

		if (asset->physCollmap)
		{
			dest->physCollmap = builder->SaveSubAsset(Game::ASSET_TYPE_PHYSCOLLMAP, asset->physCollmap);
		}

		buffer->PopBlock();
	}

	void IXModel::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.model;
		const auto path = std::format("{}/xmodel/{}.iw4xModel", Components::ZoneBuilder::GetDumpingZonePath(), asset->name);

		if (!dumpedPaths.insert(path).second)
		{
			return;
		}

		Utils::Stream buffer;

		buffer.Save(modelFileMagic, modelFileMagicLength);
		buffer.SaveObject(modelFileVersion);

		auto record = Game::X86::Convert(*asset);
		record.name = StoredPointer(asset->name);
		record.boneNames = StoredPointer(asset->boneNames);
		record.parentList = StoredPointer(asset->parentList);
		record.quats = StoredPointer(asset->quats);
		record.trans = StoredPointer(asset->trans);
		record.partClassification = StoredPointer(asset->partClassification);
		record.baseMat = StoredPointer(asset->baseMat);
		record.materialHandles = StoredPointer(asset->materialHandles);
		record.collSurfs = StoredPointer(asset->collSurfs);
		record.boneInfo = StoredPointer(asset->boneInfo);
		record.physCollmap = StoredPointer(asset->physCollmap);

		if (asset->physPreset && asset->physPreset->name)
		{
			record.physPreset = StoredPointer(asset->physPreset);
		}

		for (int i = 0; i < 4; ++i)
		{
			record.lodInfo[i].modelSurfs = StoredPointer(asset->lodInfo[i].modelSurfs);
			record.lodInfo[i].surfs = StoredPointer(asset->lodInfo[i].surfs);
		}

		buffer.SaveObject(record);

		if (asset->name)
		{
			buffer.SaveString(asset->name);
		}

		if (asset->boneNames)
		{
			for (unsigned char i = 0; i < asset->numBones; ++i)
			{
				buffer.SaveString(Game::SL_ConvertToString(asset->boneNames[i]));
			}
		}

		const auto childBoneCount = static_cast<std::size_t>(asset->numBones - asset->numRootBones);

		if (asset->parentList)
		{
			buffer.SaveArrayIfNotExisting(asset->parentList, childBoneCount);
		}

		if (asset->quats)
		{
			buffer.SaveArrayIfNotExisting(asset->quats, childBoneCount * 4);
		}

		if (asset->trans)
		{
			buffer.SaveArrayIfNotExisting(asset->trans, childBoneCount * 3);
		}

		if (asset->partClassification)
		{
			buffer.SaveArrayIfNotExisting(asset->partClassification, asset->numBones);
		}

		if (asset->baseMat)
		{
			buffer.SaveArrayIfNotExisting(asset->baseMat, asset->numBones);
		}

		if (asset->materialHandles)
		{
			for (unsigned char i = 0; i < asset->numsurfs; ++i)
			{
				buffer.SaveObject(StoredPointer(asset->materialHandles[i]));
			}

			for (unsigned char i = 0; i < asset->numsurfs; ++i)
			{
				if (asset->materialHandles[i])
				{
					buffer.SaveString(asset->materialHandles[i]->info.name);
					Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_MATERIAL, asset->materialHandles[i] });
				}
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			if (asset->lodInfo[i].modelSurfs)
			{
				WriteModelSurfs(buffer, asset->lodInfo[i].modelSurfs);
			}
		}

		if (asset->collSurfs)
		{
			for (int i = 0; i < asset->numCollSurfs; ++i)
			{
				auto collSurfRecord = Game::X86::Convert(asset->collSurfs[i]);
				collSurfRecord.collTris = StoredPointer(asset->collSurfs[i].collTris);
				buffer.SaveObject(collSurfRecord);
			}

			for (int i = 0; i < asset->numCollSurfs; ++i)
			{
				const auto* const collSurf = &asset->collSurfs[i];

				if (collSurf->collTris)
				{
					buffer.SaveArray(collSurf->collTris, static_cast<std::size_t>(collSurf->numCollTris));
				}
			}
		}

		if (asset->boneInfo)
		{
			buffer.SaveArray(asset->boneInfo, asset->numBones);
		}

		if (asset->physPreset && asset->physPreset->name)
		{
			buffer.SaveString(asset->physPreset->name);
			Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_PHYSPRESET, asset->physPreset });
		}

		if (asset->physCollmap)
		{
			WritePhysCollmap(buffer, asset->physCollmap);
		}

		if (!Utils::IO::WriteFile(path, buffer.ToBuffer()))
		{
			Components::Logger::Error("xmodel: could not write {}\n", path);
		}
	}

	std::uint8_t IXModel::GetIndexOfBone(const Game::XModel* model, const std::string& name)
	{
		for (std::uint8_t i = 0; i < model->numBones; ++i)
		{
			if (name == Game::SL_ConvertToString(model->boneNames[i]))
			{
				return i;
			}
		}

		return UCHAR_MAX;
	}

	std::uint8_t IXModel::GetParentIndexOfBone(const Game::XModel* model, std::uint8_t index)
	{
		const auto parentIndex = index - model->parentList[index - model->numRootBones];
		return static_cast<std::uint8_t>(parentIndex);
	}

	void IXModel::SetParentIndexOfBone(Game::XModel* model, std::uint8_t boneIndex, std::uint8_t parentIndex)
	{
		if (boneIndex == UCHAR_MAX)
		{
			return;
		}

		model->parentList[boneIndex - model->numRootBones] = static_cast<unsigned char>(boneIndex - parentIndex);
	}

	std::string IXModel::GetParentOfBone(const Game::XModel* model, std::uint8_t index)
	{
		assert(index > 0);

		const auto parentIndex = GetParentIndexOfBone(model, index);
		return Game::SL_ConvertToString(model->boneNames[parentIndex]);
	}

	std::uint8_t IXModel::GetHighestAffectingBoneIndex(const Game::XModelLodInfo* lod)
	{
		std::uint8_t highestBoneIndex = 0;

		for (unsigned short surfIndex = 0; surfIndex < lod->numsurfs; ++surfIndex)
		{
			const auto* const surface = &lod->surfs[surfIndex];

			VisitBlendIndices(surface, [&](unsigned int offset)
			{
				const auto index = static_cast<std::uint8_t>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::DObjSkelMat));
				highestBoneIndex = std::max(highestBoneIndex, index);
			});

			for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; ++vertListIndex)
			{
				const auto index = static_cast<std::uint8_t>(surface->vertList[vertListIndex].boneOffset / sizeof(Game::DObjSkelMat));
				highestBoneIndex = std::max(highestBoneIndex, index);
			}
		}

		return highestBoneIndex;
	}

	void IXModel::RebuildPartBits(Game::XModel* model)
	{
		constexpr int partBitsLength = 6;

		for (unsigned char i = 0; i < model->numLods; ++i)
		{
			auto* const lod = &model->lodInfo[i];
			std::uint32_t lodPartBits[partBitsLength]{};

			for (unsigned short surfIndex = 0; surfIndex < lod->numsurfs; ++surfIndex)
			{
				auto* const surface = &lod->surfs[surfIndex];

				std::uint32_t rebuiltPartBits[partBitsLength]{};
				std::unordered_set<std::uint8_t> affectingBones{};

				VisitBlendIndices(surface, [&](unsigned int offset)
				{
					const auto index = static_cast<std::uint8_t>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::DObjSkelMat));

					assert(index < model->numBones);

					affectingBones.emplace(index);
				});

				for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; ++vertListIndex)
				{
					affectingBones.emplace(static_cast<std::uint8_t>(surface->vertList[vertListIndex].boneOffset / sizeof(Game::DObjSkelMat)));
				}

				for (const auto boneIndex : affectingBones)
				{
					const auto bitPosition = 31 - boneIndex % 32;
					const auto groupIndex = boneIndex / 32;

					assert(groupIndex < partBitsLength);

					rebuiltPartBits[groupIndex] |= 1u << bitPosition;
					lodPartBits[groupIndex] |= 1u << bitPosition;
				}

				std::memcpy(surface->partBits, rebuiltPartBits, sizeof(rebuiltPartBits));
			}

			std::memcpy(lod->partBits, lodPartBits, sizeof(lodPartBits));
			std::memcpy(lod->modelSurfs->partBits, lodPartBits, sizeof(lodPartBits));

			lod->partBits[partBitsLength - 1] |= 0x1;
			lod->modelSurfs->partBits[partBitsLength - 1] |= 0x1;
		}
	}

	std::uint8_t IXModel::InsertBone(Game::XModel* model, const std::string& boneName, const std::string& parentName, Utils::Memory::Allocator& allocator)
	{
		assert(GetIndexOfBone(model, boneName) == UCHAR_MAX);

		std::map<std::string, std::string> parentsToRestore{};

		for (std::uint8_t i = model->numRootBones; i < model->numBones; ++i)
		{
			parentsToRestore[Game::SL_ConvertToString(model->boneNames[i])] = GetParentOfBone(model, i);
		}

		const auto newBoneCount = static_cast<std::uint8_t>(model->numBones + 1);
		const auto newBoneCountMinusRoot = static_cast<std::uint8_t>(newBoneCount - model->numRootBones);

		const auto parentIndex = GetIndexOfBone(model, parentName);

		assert(parentIndex != UCHAR_MAX);

		const auto atPosition = static_cast<std::uint8_t>(parentIndex + 1);

		const std::uint8_t newBoneIndex = atPosition;
		const auto newBoneIndexMinusRoot = static_cast<std::uint8_t>(atPosition - model->numRootBones);

		auto* const newBoneNames = allocator.AllocateArray<std::uint16_t>(newBoneCount);
		auto* const newMats = allocator.AllocateArray<Game::DObjAnimMat>(newBoneCount);
		auto* const newBoneInfo = allocator.AllocateArray<Game::XBoneInfo>(newBoneCount);
		auto* const newPartsClassification = allocator.AllocateArray<std::uint8_t>(newBoneCount);
		auto* const newQuats = allocator.AllocateArray<std::int16_t>(4 * newBoneCountMinusRoot);
		auto* const newTrans = allocator.AllocateArray<float>(3 * newBoneCountMinusRoot);
		auto* const newParentList = allocator.AllocateArray<std::uint8_t>(newBoneCountMinusRoot);

		const std::uint8_t lengthOfFirstPart = atPosition;
		const auto lengthOfSecondPart = static_cast<std::uint8_t>(model->numBones - atPosition);

		const auto lengthOfFirstPartMinusRoot = static_cast<std::uint8_t>(atPosition - model->numRootBones);
		const auto lengthOfSecondPartMinusRoot = static_cast<std::uint8_t>(model->numBones - model->numRootBones - (atPosition - model->numRootBones));

		const auto atPositionMinusRoot = static_cast<std::uint8_t>(atPosition - model->numRootBones);

		if (lengthOfFirstPart > 0)
		{
			std::memcpy(newBoneNames, model->boneNames, sizeof(std::uint16_t) * lengthOfFirstPart);
			std::memcpy(newMats, model->baseMat, sizeof(Game::DObjAnimMat) * lengthOfFirstPart);
			std::memcpy(newPartsClassification, model->partClassification, lengthOfFirstPart);
			std::memcpy(newBoneInfo, model->boneInfo, sizeof(Game::XBoneInfo) * lengthOfFirstPart);
			std::memcpy(newQuats, model->quats, sizeof(std::uint16_t) * 4 * lengthOfFirstPartMinusRoot);
			std::memcpy(newTrans, model->trans, sizeof(float) * 3 * lengthOfFirstPartMinusRoot);
		}

		{
			const auto name = Game::SL_GetString(boneName.data(), 0);

			auto mat = model->baseMat[parentIndex];
			const auto boneInfo = model->boneInfo[parentIndex];

			std::uint16_t quat[4]{};
			quat[3] = SHRT_MAX;

			const float trans[3]{};

			mat.transWeight = 1.9999f;

			newMats[newBoneIndex] = mat;
			newBoneInfo[newBoneIndex] = boneInfo;
			newBoneNames[newBoneIndex] = static_cast<std::uint16_t>(name);

			std::memcpy(&newQuats[newBoneIndexMinusRoot * 4], quat, sizeof(quat));
			std::memcpy(&newTrans[newBoneIndexMinusRoot * 3], trans, sizeof(trans));
		}

		if (lengthOfSecondPart > 0)
		{
			std::memcpy(&newBoneNames[atPosition + 1], &model->boneNames[atPosition], sizeof(std::uint16_t) * lengthOfSecondPart);
			std::memcpy(&newMats[atPosition + 1], &model->baseMat[atPosition], sizeof(Game::DObjAnimMat) * lengthOfSecondPart);
			std::memcpy(&newPartsClassification[atPosition + 1], &model->partClassification[atPosition], lengthOfSecondPart);
			std::memcpy(&newBoneInfo[atPosition + 1], &model->boneInfo[atPosition], sizeof(Game::XBoneInfo) * lengthOfSecondPart);
			std::memcpy(&newQuats[(atPositionMinusRoot + 1) * 4], &model->quats[atPositionMinusRoot * 4], sizeof(std::uint16_t) * 4 * lengthOfSecondPartMinusRoot);
			std::memcpy(&newTrans[(atPositionMinusRoot + 1) * 3], &model->trans[atPositionMinusRoot * 3], sizeof(float) * 3 * lengthOfSecondPartMinusRoot);
		}

		model->baseMat = newMats;
		model->boneInfo = newBoneInfo;
		model->boneNames = newBoneNames;
		model->quats = newQuats;
		model->trans = newTrans;
		model->parentList = newParentList;

		model->numBones = newBoneCount;

		for (std::uint8_t lodIndex = 0; lodIndex < model->numLods; ++lodIndex)
		{
			auto* const lod = &model->lodInfo[lodIndex];

			if ((lod->modelSurfs->partBits[5] & 0x1) == 0x1)
			{
				std::memcpy(lod->partBits, lod->modelSurfs->partBits, sizeof(lod->partBits));
				continue;
			}

			if (GetHighestAffectingBoneIndex(lod) >= model->numBones)
			{
				continue;
			}

			for (unsigned short surfIndex = 0; surfIndex < lod->modelSurfs->numsurfs; ++surfIndex)
			{
				auto* const surface = &lod->modelSurfs->surfs[surfIndex];

				static_assert(sizeof(Game::DObjSkelMat) == 64);

				if (surface->vertList)
				{
					for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; ++vertListIndex)
					{
						auto* const vertList = &surface->vertList[vertListIndex];
						auto index = static_cast<int>(vertList->boneOffset / sizeof(Game::DObjSkelMat));

						if (index < atPosition)
						{
							continue;
						}

						++index;

						if (index >= model->numBones)
						{
							Components::Logger::Print("Unexpected 'bone index' {} out of {} bones while working vertex list of: xmodel {} lod {} xmodelsurf {} surf #{}\n", index, model->numBones, model->name, lodIndex, lod->modelSurfs->name, surfIndex);
							assert(false);
						}

						vertList->boneOffset = static_cast<unsigned short>(index * sizeof(Game::DObjSkelMat));
					}
				}

				VisitBlendIndices(surface, [&](unsigned int offset)
				{
					auto index = static_cast<int>(surface->vertInfo.vertsBlend[offset] / sizeof(Game::DObjSkelMat));

					if (index < atPosition)
					{
						return;
					}

					++index;

					if (index >= model->numBones)
					{
						Components::Logger::Print("Unexpected 'bone index' {} out of {} bones while working vertex blend of: xmodel {} lod {} xmodelsurf {} surf #{}\n", index, model->numBones, model->name, lodIndex, lod->modelSurfs->name, surfIndex);
						assert(false);
					}

					surface->vertInfo.vertsBlend[offset] = static_cast<unsigned short>(index * sizeof(Game::DObjSkelMat));
				});
			}
		}

		SetParentIndexOfBone(model, atPosition, parentIndex);

		for (const auto& [boneToFix, parentBefore] : parentsToRestore)
		{
			const auto parent = GetIndexOfBone(model, parentBefore);
			const auto index = GetIndexOfBone(model, boneToFix);
			SetParentIndexOfBone(model, index, parent);
		}

		return atPosition;
	}

	void IXModel::TransferWeights(Game::XModel* model, std::uint8_t origin, std::uint8_t destination)
	{
		const auto originalWeights = model->baseMat[origin].transWeight;
		model->baseMat[origin].transWeight = model->baseMat[destination].transWeight;
		model->baseMat[destination].transWeight = originalWeights;

		for (unsigned char i = 0; i < model->numLods; ++i)
		{
			auto* const lod = &model->lodInfo[i];

			if ((lod->partBits[5] & 0x1) == 0x1)
			{
				continue;
			}

			for (unsigned short surfIndex = 0; surfIndex < lod->modelSurfs->numsurfs; ++surfIndex)
			{
				auto* const surface = &lod->modelSurfs->surfs[surfIndex];

				if (surface->vertList)
				{
					for (unsigned int vertListIndex = 0; vertListIndex < surface->vertListCount; ++vertListIndex)
					{
						auto* const vertList = &surface->vertList[vertListIndex];

						if (vertList->boneOffset / sizeof(Game::DObjSkelMat) == static_cast<std::size_t>(origin))
						{
							vertList->boneOffset = static_cast<unsigned short>(destination * sizeof(Game::DObjSkelMat));
						}
					}
				}

				VisitBlendIndices(surface, [&](unsigned int offset)
				{
					if (surface->vertInfo.vertsBlend[offset] / sizeof(Game::DObjSkelMat) == static_cast<std::size_t>(origin))
					{
						surface->vertInfo.vertsBlend[offset] = static_cast<unsigned short>(destination * sizeof(Game::DObjSkelMat));
					}
				});
			}
		}
	}

	void IXModel::SetBoneTrans(Game::XModel* model, std::uint8_t boneIndex, bool baseMat, float x, float y, float z)
	{
		if (baseMat)
		{
			model->baseMat[boneIndex].trans[0] = x;
			model->baseMat[boneIndex].trans[1] = y;
			model->baseMat[boneIndex].trans[2] = z;
			return;
		}

		assert(boneIndex >= model->numRootBones);

		const auto index = boneIndex - model->numRootBones;

		model->trans[index * 3 + 0] = x;
		model->trans[index * 3 + 1] = y;
		model->trans[index * 3 + 2] = z;
	}

	void IXModel::SetBoneQuaternion(Game::XModel* model, std::uint8_t boneIndex, bool baseMat, float x, float y, float z, float w)
	{
		if (baseMat)
		{
			model->baseMat[boneIndex].quat[0] = x;
			model->baseMat[boneIndex].quat[1] = y;
			model->baseMat[boneIndex].quat[2] = z;
			model->baseMat[boneIndex].quat[3] = w;
			return;
		}

		assert(boneIndex >= model->numRootBones);

		const auto index = boneIndex - model->numRootBones;

		model->quats[index * 4 + 0] = static_cast<short>(x * SHRT_MAX);
		model->quats[index * 4 + 1] = static_cast<short>(y * SHRT_MAX);
		model->quats[index * 4 + 2] = static_cast<short>(z * SHRT_MAX);
		model->quats[index * 4 + 3] = static_cast<short>(w * SHRT_MAX);
	}

	void IXModel::ConvertPlayerModelFromSingleplayerToMultiplayer(Game::XModel* model, Utils::Memory::Allocator& allocator)
	{
		const std::string requiredBonesForHumanoid[] =
		{
			"j_spinelower",
			"j_spineupper",
			"j_spine4",
			"j_mainroot",
		};

		for (const auto& required : requiredBonesForHumanoid)
		{
			if (GetIndexOfBone(model, required) == UCHAR_MAX)
			{
				return;
			}
		}

		if (GetIndexOfBone(model, "torso_stabilizer") != UCHAR_MAX)
		{
			return;
		}

		Components::Logger::Print("Converting {} skeleton from SP to MP...\n", model->name);

		const auto root = GetIndexOfBone(model, "j_mainroot");

		if (root == UCHAR_MAX)
		{
			return;
		}

		const std::uint8_t indexOfPelvis = InsertBone(model, "pelvis", "j_mainroot", allocator);
		SetBoneQuaternion(model, indexOfPelvis, true, -0.494f, -0.506f, -0.506f, 0.494f);

		TransferWeights(model, root, indexOfPelvis);

		SetParentIndexOfBone(model, GetIndexOfBone(model, "j_hip_le"), indexOfPelvis);
		SetParentIndexOfBone(model, GetIndexOfBone(model, "j_hip_ri"), indexOfPelvis);
		SetParentIndexOfBone(model, GetIndexOfBone(model, "tag_stowed_hip_rear"), indexOfPelvis);

		if (GetIndexOfBone(model, "j_coatfront_le") == UCHAR_MAX)
		{
			InsertBone(model, "j_coatfront_le", "pelvis", allocator);
		}

		if (GetIndexOfBone(model, "j_coatfront_ri") == UCHAR_MAX)
		{
			InsertBone(model, "j_coatfront_ri", "pelvis", allocator);
		}

		const std::uint8_t torsoStabilizer = InsertBone(model, "torso_stabilizer", "pelvis", allocator);
		const std::uint8_t lowerSpine = GetIndexOfBone(model, "j_spinelower");
		SetParentIndexOfBone(model, lowerSpine, torsoStabilizer);

		const std::uint8_t backLow = InsertBone(model, "back_low", "j_spinelower", allocator);
		TransferWeights(model, lowerSpine, backLow);
		SetParentIndexOfBone(model, GetIndexOfBone(model, "j_spineupper"), backLow);

		const std::uint8_t backMid = InsertBone(model, "back_mid", "j_spineupper", allocator);
		TransferWeights(model, GetIndexOfBone(model, "j_spineupper"), backMid);
		SetParentIndexOfBone(model, GetIndexOfBone(model, "j_spine4"), backMid);

		assert(root == GetIndexOfBone(model, "j_mainroot"));
		assert(indexOfPelvis == GetIndexOfBone(model, "pelvis"));
		assert(backLow == GetIndexOfBone(model, "back_low"));
		assert(backMid == GetIndexOfBone(model, "back_mid"));

		SetBoneQuaternion(model, lowerSpine, false, -0.492f, -0.507f, -0.507f, 0.492f);
		SetBoneQuaternion(model, torsoStabilizer, false, 0.494f, 0.506f, 0.506f, 0.494f);

		SetBoneTrans(model, GetIndexOfBone(model, "j_spinelower"), false, 0.07f, 0.0f, 5.2f);

		auto stowedBack = GetIndexOfBone(model, "tag_stowed_back");

		if (stowedBack == UCHAR_MAX)
		{
			stowedBack = InsertBone(model, "tag_stowed_back", "j_spine4", allocator);
		}

		SetBoneTrans(model, stowedBack, false, -0.32f, -6.27f, -2.65F);
		SetBoneQuaternion(model, stowedBack, false, -0.044f, 0.088f, -0.995f, 0.025f);
		SetBoneTrans(model, stowedBack, true, -9.571f, -2.654f, 51.738f);
		SetBoneQuaternion(model, stowedBack, true, -0.071f, 0.0f, -0.997f, 0.0f);

		auto stowedRear = GetIndexOfBone(model, "tag_stowed_hip_rear");

		if (stowedRear == UCHAR_MAX)
		{
			stowedRear = InsertBone(model, "tag_stowed_hip_rear", "pelvis", allocator);
		}

		SetBoneTrans(model, stowedRear, false, -0.75f, -6.45f, -4.99f);
		SetBoneQuaternion(model, stowedRear, false, -0.553f, -0.062f, -0.049f, 0.830f);
		SetBoneTrans(model, stowedBack, true, -9.866f, -4.989f, 36.315f);
		SetBoneQuaternion(model, stowedRear, true, -0.054f, -0.025f, -0.975f, 0.214f);

		RebuildPartBits(model);
	}
}
