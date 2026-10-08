#include "STDInclude.hpp"

#include <charconv>

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "IclipMap_t.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#define IW4X_CLIPMAP_VERSION 3

namespace Assets
{
	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("clipmap/{}.iw4x.json", baseName);
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
	static T* AllocateArrayOrNull(Utils::Memory::Allocator* allocator, std::size_t count)
	{
		if (!count)
		{
			return nullptr;
		}

		return allocator->AllocateArray<T>(count);
	}

	static bool IsArrayOf(const rapidjson::Value& json, rapidjson::SizeType minimumSize)
	{
		return json.IsArray() && json.Size() >= minimumSize;
	}

	static bool HasArray(const rapidjson::Value& json, const char* name, rapidjson::SizeType minimumSize)
	{
		return json.IsObject() && json.HasMember(name) && IsArrayOf(json[name], minimumSize);
	}

	static bool HasString(const rapidjson::Value& json, const char* name)
	{
		return json.IsObject() && json.HasMember(name) && json[name].IsString();
	}

	static bool TryReadIndex(const rapidjson::Value& json, std::size_t count, std::size_t& index)
	{
		if (!json.IsString())
		{
			return false;
		}

		const std::string_view text(json.GetString(), json.GetStringLength());

		if (text.size() < 2 || text[0] != '#')
		{
			return false;
		}

		std::size_t value = 0;
		const auto* const end = text.data() + text.size();
		const auto [parsedEnd, error] = std::from_chars(text.data() + 1, end, value);

		if (error != std::errc() || parsedEnd != end || value >= count)
		{
			return false;
		}

		index = value;
		return true;
	}

	static bool TryReadBounds(const rapidjson::Value& json, Game::Bounds& bounds)
	{
		if (!HasArray(json, "midPoint", 3) || !HasArray(json, "halfSize", 3))
		{
			return false;
		}

		Utils::JSON::CopyArray(bounds.midPoint, json["midPoint"], 3);
		Utils::JSON::CopyArray(bounds.halfSize, json["halfSize"], 3);

		return true;
	}

	static bool TryReadLeaf(const rapidjson::Value& json, Game::cLeaf_t& leaf)
	{
		if (!json.IsObject() || !json.HasMember("bounds") || !TryReadBounds(json["bounds"], leaf.bounds))
		{
			return false;
		}

		leaf.firstCollAabbIndex = json["firstCollAabbIndex"].Get<std::uint16_t>();
		leaf.collAabbCount = json["collAabbCount"].Get<std::uint16_t>();
		leaf.brushContents = json["brushContents"].Get<std::int32_t>();
		leaf.terrainContents = json["terrainContents"].Get<std::int32_t>();
		leaf.leafBrushNode = json["leafBrushNode"].Get<std::int32_t>();

		return true;
	}

	static bool TryReadTriggers(const rapidjson::Value& json, Game::MapEnts* mapEnts, Utils::Memory::Allocator* allocator)
	{
		if (!json.HasMember("trigger") || !HasArray(json["trigger"], "models", 0) || !HasArray(json["trigger"], "hulls", 0) || !HasArray(json["trigger"], "slabs", 0) || !HasArray(json, "stages", 0))
		{
			return false;
		}

		const auto& jsonTrigger = json["trigger"];
		auto* const trigger = &mapEnts->trigger;

		const auto& jsonModels = jsonTrigger["models"];
		trigger->count = jsonModels.Size();
		trigger->models = allocator->AllocateArray<Game::TriggerModel>(trigger->count);

		for (unsigned int i = 0; i < trigger->count; ++i)
		{
			if (!jsonModels[i].IsObject())
			{
				return false;
			}

			trigger->models[i].contents = jsonModels[i]["contents"].Get<std::int32_t>();
			trigger->models[i].hullCount = jsonModels[i]["hullCount"].Get<std::uint16_t>();
			trigger->models[i].firstHull = jsonModels[i]["firstHull"].Get<std::uint16_t>();
		}

		const auto& jsonHulls = jsonTrigger["hulls"];
		trigger->hullCount = jsonHulls.Size();
		trigger->hulls = allocator->AllocateArray<Game::TriggerHull>(trigger->hullCount);

		for (unsigned int i = 0; i < trigger->hullCount; ++i)
		{
			if (!jsonHulls[i].IsObject() || !jsonHulls[i].HasMember("bounds") || !TryReadBounds(jsonHulls[i]["bounds"], trigger->hulls[i].bounds))
			{
				return false;
			}

			trigger->hulls[i].contents = jsonHulls[i]["contents"].Get<std::int32_t>();
			trigger->hulls[i].firstSlab = jsonHulls[i]["firstSlab"].Get<std::uint16_t>();
			trigger->hulls[i].slabCount = jsonHulls[i]["slabCount"].Get<std::uint16_t>();
		}

		const auto& jsonSlabs = jsonTrigger["slabs"];
		trigger->slabCount = jsonSlabs.Size();
		trigger->slabs = allocator->AllocateArray<Game::TriggerSlab>(trigger->slabCount);

		for (unsigned int i = 0; i < trigger->slabCount; ++i)
		{
			if (!HasArray(jsonSlabs[i], "dir", 3))
			{
				return false;
			}

			Utils::JSON::CopyArray(trigger->slabs[i].dir, jsonSlabs[i]["dir"], 3);
			trigger->slabs[i].midPoint = jsonSlabs[i]["midPoint"].Get<float>();
			trigger->slabs[i].halfSize = jsonSlabs[i]["halfSize"].Get<float>();
		}

		const auto& jsonStages = json["stages"];

		if (jsonStages.Size() > 0xFF)
		{
			return false;
		}

		mapEnts->stageCount = static_cast<char>(jsonStages.Size());
		mapEnts->stages = allocator->AllocateArray<Game::Stage>(jsonStages.Size());

		for (rapidjson::SizeType i = 0; i < jsonStages.Size(); ++i)
		{
			const auto& jsonStage = jsonStages[i];
			auto* const stage = &mapEnts->stages[i];

			if (!HasString(jsonStage, "name") || !HasArray(jsonStage, "origin", 3))
			{
				return false;
			}

			stage->name = allocator->DuplicateString(jsonStage["name"].GetString());
			Utils::JSON::CopyArray(stage->origin, jsonStage["origin"], 3);
			stage->triggerIndex = jsonStage["triggerIndex"].Get<std::uint16_t>();
			stage->sunPrimaryLightIndex = jsonStage["sunPrimaryLightIndex"].Get<char>();
		}

		return true;
	}

	template <typename T>
	static T* FindNamedAsset(const rapidjson::Value& json, const char* member, Game::XAssetType type, Components::ZoneBuilder::Zone* builder)
	{
		if (!HasString(json, member))
		{
			return nullptr;
		}

		return static_cast<T*>(Components::AssetHandler::FindAssetForZone(type, json[member].GetString(), builder).data);
	}

	static bool TryReadDynEntityDef(const rapidjson::Value& json, Game::DynEntityDef* entity, Components::ZoneBuilder::Zone* builder)
	{
		if (!json.IsObject() || !json.HasMember("dynEntityDef"))
		{
			return false;
		}

		const auto& jsonEntity = json["dynEntityDef"];

		if (!jsonEntity.IsObject() || !jsonEntity.HasMember("pose") || !HasArray(jsonEntity["pose"], "quat", 4) || !HasArray(jsonEntity["pose"], "origin", 3))
		{
			return false;
		}

		if (!jsonEntity.HasMember("mass"))
		{
			return false;
		}

		const auto& jsonMass = jsonEntity["mass"];

		if (!HasArray(jsonMass, "centerOfMass", 3) || !HasArray(jsonMass, "momentsOfInertia", 3) || !HasArray(jsonMass, "productsOfInertia", 3))
		{
			return false;
		}

		entity->type = static_cast<Game::DynEntityType>(jsonEntity["type"].Get<std::int32_t>());
		Utils::JSON::CopyArray(entity->pose.quat, jsonEntity["pose"]["quat"], 4);
		Utils::JSON::CopyArray(entity->pose.origin, jsonEntity["pose"]["origin"], 3);

		entity->xModel = FindNamedAsset<Game::XModel>(jsonEntity, "xModel", Game::ASSET_TYPE_XMODEL, builder);
		entity->brushModel = jsonEntity["brushModel"].Get<std::uint16_t>();
		entity->physicsBrushModel = jsonEntity["physicsBrushModel"].Get<std::uint16_t>();
		entity->destroyFx = FindNamedAsset<Game::FxEffectDef>(jsonEntity, "destroyFx", Game::ASSET_TYPE_FX, builder);
		entity->physPreset = FindNamedAsset<Game::PhysPreset>(jsonEntity, "physPreset", Game::ASSET_TYPE_PHYSPRESET, builder);
		entity->health = jsonEntity["health"].Get<std::int32_t>();

		Utils::JSON::CopyArray(entity->mass.centerOfMass, jsonMass["centerOfMass"], 3);
		Utils::JSON::CopyArray(entity->mass.momentsOfInertia, jsonMass["momentsOfInertia"], 3);
		Utils::JSON::CopyArray(entity->mass.productsOfInertia, jsonMass["productsOfInertia"], 3);

		entity->contents = jsonEntity["contents"].Get<std::int32_t>();

		return true;
	}

	static Game::clipMap_t* TryReadClipMap(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document json;
		json.Parse<rapidjson::kParseNanAndInfFlag>(file.GetBuffer().data());

		if (json.HasParseError() || !json.IsObject())
		{
			Components::Logger::Error("Invalid JSON for clipmap {}!\n", name);
			return nullptr;
		}

		const auto fail = [&name](const std::string_view what) -> Game::clipMap_t*
		{
			Components::Logger::Error("Malformed JSON for clipmap {}! {}\n", name, what);
			return nullptr;
		};

		if (json.HasMember("version") && json["version"].IsNumber() && json["version"].Get<std::int32_t>() > IW4X_CLIPMAP_VERSION)
		{
			return fail("its version is newer than this reader");
		}

		constexpr const char* arrayMembers[] =
		{
			"planes", "staticModelList", "materials", "brushsides", "brushEdges", "nodes", "leafs", "leafbrushNodes", "leafbrushes",
			"leafsurfaces", "verts", "triIndices", "triEdgeIsWalkable", "borders", "partitions", "aabbTrees", "cmodels", "brushes",
			"brushBounds", "brushContents", "smodelNodes",
		};

		for (const auto* const member : arrayMembers)
		{
			if (!HasArray(json, member, 0))
			{
				return fail(member);
			}
		}

		if (!HasString(json, "name") || !json.HasMember("mapEnts") || !HasArray(json, "dynEntities", 2))
		{
			return fail("name, mapEnts or dynEntities");
		}

		auto* const allocator = builder->GetAllocator();
		auto* const clipMap = allocator->Allocate<Game::clipMap_t>();

		clipMap->name = allocator->DuplicateString(json["name"].GetString());
		clipMap->isInUse = json["isInUse"].Get<std::int32_t>();

		const auto& jsonPlanes = json["planes"];
		clipMap->planeCount = jsonPlanes.Size();
		clipMap->planes = AllocateArrayOrNull<Game::cplane_s>(allocator, clipMap->planeCount);

		for (unsigned int i = 0; i < clipMap->planeCount; ++i)
		{
			const auto& jsonPlane = jsonPlanes[i];
			auto* const plane = &clipMap->planes[i];

			if (!HasArray(jsonPlane, "normal", 3))
			{
				return fail("a plane");
			}

			Utils::JSON::CopyArray(plane->normal, jsonPlane["normal"], 3);
			plane->dist = jsonPlane["dist"].Get<float>();
			plane->type = jsonPlane["type"].Get<std::uint8_t>();
		}

		const auto& jsonModels = json["staticModelList"];
		clipMap->numStaticModels = jsonModels.Size();
		clipMap->staticModelList = AllocateArrayOrNull<Game::cStaticModel_s>(allocator, clipMap->numStaticModels);

		for (unsigned int i = 0; i < clipMap->numStaticModels; ++i)
		{
			const auto& jsonModel = jsonModels[i];
			auto* const model = &clipMap->staticModelList[i];

			if (!HasString(jsonModel, "xmodel") || !HasArray(jsonModel, "origin", 3) || !HasArray(jsonModel, "invScaledAxis", 3) || !jsonModel.HasMember("absBounds"))
			{
				return fail("a static model");
			}

			model->xmodel = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_XMODEL, jsonModel["xmodel"].GetString(), builder).model;

			if (!model->xmodel)
			{
				return fail(std::format("static model '{}' could not be found", jsonModel["xmodel"].GetString()));
			}

			Utils::JSON::CopyArray(model->origin, jsonModel["origin"], 3);

			for (rapidjson::SizeType j = 0; j < 3; ++j)
			{
				const auto& row = jsonModel["invScaledAxis"][j];

				if (!IsArrayOf(row, 3))
				{
					return fail("a static model's invScaledAxis");
				}

				Utils::JSON::CopyArray(model->invScaledAxis[j], row, 3);
			}

			if (!TryReadBounds(jsonModel["absBounds"], model->absBounds))
			{
				return fail("a static model's absBounds");
			}
		}

		const auto& jsonMaterials = json["materials"];
		clipMap->numMaterials = jsonMaterials.Size();
		clipMap->materials = AllocateArrayOrNull<Game::ClipMaterial>(allocator, clipMap->numMaterials);

		for (unsigned int i = 0; i < clipMap->numMaterials; ++i)
		{
			const auto& jsonMaterial = jsonMaterials[i];
			auto* const material = &clipMap->materials[i];

			if (!HasString(jsonMaterial, "name"))
			{
				return fail("a material");
			}

			material->name = allocator->DuplicateString(jsonMaterial["name"].GetString());
			material->surfaceFlags = jsonMaterial["surfaceFlags"].Get<std::int32_t>();
			material->contents = jsonMaterial["contents"].Get<std::int32_t>();
		}

		const auto& jsonBrushSides = json["brushsides"];
		clipMap->numBrushSides = jsonBrushSides.Size();
		clipMap->brushsides = AllocateArrayOrNull<Game::cbrushside_t>(allocator, clipMap->numBrushSides);

		for (unsigned int i = 0; i < clipMap->numBrushSides; ++i)
		{
			const auto& jsonBrushSide = jsonBrushSides[i];
			auto* const brushSide = &clipMap->brushsides[i];
			std::size_t planeIndex = 0;

			if (!jsonBrushSide.IsObject() || !jsonBrushSide.HasMember("plane") || !TryReadIndex(jsonBrushSide["plane"], clipMap->planeCount, planeIndex))
			{
				return fail("a brush side's plane");
			}

			brushSide->plane = &clipMap->planes[planeIndex];
			brushSide->materialNum = jsonBrushSide["materialNum"].Get<std::uint16_t>();
			brushSide->firstAdjacentSideOffset = jsonBrushSide["firstAdjacentSideOffset"].Get<char>();
			brushSide->edgeCount = jsonBrushSide["edgeCount"].Get<char>();
		}

		const auto& jsonBrushEdges = json["brushEdges"];
		clipMap->numBrushEdges = jsonBrushEdges.Size();
		clipMap->brushEdges = AllocateArrayOrNull<unsigned char>(allocator, clipMap->numBrushEdges);

		for (unsigned int i = 0; i < clipMap->numBrushEdges; ++i)
		{
			clipMap->brushEdges[i] = jsonBrushEdges[i].Get<std::uint8_t>();
		}

		const auto& jsonNodes = json["nodes"];
		clipMap->numNodes = jsonNodes.Size();
		clipMap->nodes = AllocateArrayOrNull<Game::cNode_t>(allocator, clipMap->numNodes);

		for (unsigned int i = 0; i < clipMap->numNodes; ++i)
		{
			const auto& jsonNode = jsonNodes[i];
			auto* const node = &clipMap->nodes[i];
			std::size_t planeIndex = 0;

			if (!HasArray(jsonNode, "children", 2) || !jsonNode.HasMember("plane") || !TryReadIndex(jsonNode["plane"], clipMap->planeCount, planeIndex))
			{
				return fail("a node");
			}

			node->plane = &clipMap->planes[planeIndex];
			node->children[0] = jsonNode["children"][0].Get<short>();
			node->children[1] = jsonNode["children"][1].Get<short>();
		}

		const auto& jsonLeafs = json["leafs"];
		clipMap->numLeafs = jsonLeafs.Size();
		clipMap->leafs = AllocateArrayOrNull<Game::cLeaf_t>(allocator, clipMap->numLeafs);

		for (unsigned int i = 0; i < clipMap->numLeafs; ++i)
		{
			if (!TryReadLeaf(jsonLeafs[i], clipMap->leafs[i]))
			{
				return fail("a leaf");
			}
		}

		const auto& jsonLeafBrushNodes = json["leafbrushNodes"];
		clipMap->leafbrushNodesCount = jsonLeafBrushNodes.Size();
		clipMap->leafbrushNodes = AllocateArrayOrNull<Game::cLeafBrushNode_s>(allocator, clipMap->leafbrushNodesCount);

		const auto& jsonLeafBrushes = json["leafbrushes"];
		clipMap->numLeafBrushes = jsonLeafBrushes.Size();
		clipMap->leafbrushes = AllocateArrayOrNull<unsigned short>(allocator, clipMap->numLeafBrushes);

		for (unsigned int i = 0; i < clipMap->numLeafBrushes; ++i)
		{
			clipMap->leafbrushes[i] = jsonLeafBrushes[i].Get<std::uint16_t>();
		}

		const auto& jsonLeafSurfaces = json["leafsurfaces"];
		clipMap->numLeafSurfaces = jsonLeafSurfaces.Size();
		clipMap->leafsurfaces = AllocateArrayOrNull<unsigned int>(allocator, clipMap->numLeafSurfaces);

		for (unsigned int i = 0; i < clipMap->numLeafSurfaces; ++i)
		{
			clipMap->leafsurfaces[i] = jsonLeafSurfaces[i].Get<std::uint32_t>();
		}

		const auto& jsonVerts = json["verts"];
		clipMap->vertCount = jsonVerts.Size();
		clipMap->verts = AllocateArrayOrNull<Game::vec3_t>(allocator, clipMap->vertCount);

		for (unsigned int i = 0; i < clipMap->vertCount; ++i)
		{
			if (!IsArrayOf(jsonVerts[i], 3))
			{
				return fail("a vertex");
			}

			Utils::JSON::CopyArray(clipMap->verts[i], jsonVerts[i], 3);
		}

		for (unsigned int i = 0; i < clipMap->leafbrushNodesCount; ++i)
		{
			const auto& jsonNode = jsonLeafBrushNodes[i];
			auto* const node = &clipMap->leafbrushNodes[i];

			if (!jsonNode.IsObject() || !jsonNode.HasMember("data"))
			{
				return fail("a leaf brush node");
			}

			node->axis = jsonNode["axis"].Get<std::uint8_t>();
			node->leafBrushCount = jsonNode["leafBrushCount"].Get<short>();
			node->contents = jsonNode["contents"].Get<std::int32_t>();

			const auto& jsonData = jsonNode["data"];

			if (node->leafBrushCount > 0)
			{
				const auto brushCount = static_cast<std::size_t>(node->leafBrushCount);
				std::size_t brushIndex = 0;

				if (jsonData.IsString())
				{
					if (!TryReadIndex(jsonData, clipMap->numLeafBrushes, brushIndex) || brushIndex + brushCount > clipMap->numLeafBrushes)
					{
						return fail("a leaf brush node's brushes");
					}

					node->data.leaf.brushes = &clipMap->leafbrushes[brushIndex];
					continue;
				}

				if (!IsArrayOf(jsonData, static_cast<rapidjson::SizeType>(brushCount)))
				{
					return fail("a leaf brush node's data");
				}

				node->data.leaf.brushes = allocator->AllocateArray<unsigned short>(brushCount);
				Utils::JSON::CopyArray(node->data.leaf.brushes, jsonData, brushCount);
				continue;
			}

			if (!HasArray(jsonData, "childOffset", 2))
			{
				return fail("a leaf brush node's children");
			}

			node->data.children.dist = jsonData["dist"].Get<float>();
			node->data.children.range = jsonData["range"].Get<float>();
			Utils::JSON::CopyArray(node->data.children.childOffset, jsonData["childOffset"], 2);
		}

		const auto& jsonTris = json["triIndices"];
		clipMap->triCount = jsonTris.Size();
		clipMap->triIndices = AllocateArrayOrNull<unsigned short>(allocator, clipMap->triCount * 3);

		for (unsigned int i = 0; i < clipMap->triCount; ++i)
		{
			if (!IsArrayOf(jsonTris[i], 3))
			{
				return fail("a triangle");
			}

			Utils::JSON::CopyArray(&clipMap->triIndices[i * 3], jsonTris[i], 3);
		}

		const auto& jsonWalkable = json["triEdgeIsWalkable"];
		const std::size_t walkableCount = 4 * ((3 * clipMap->triCount + 31) >> 5) * 3;
		const auto walkableRead = std::min<std::size_t>(walkableCount, jsonWalkable.Size());

		clipMap->triEdgeIsWalkable = AllocateArrayOrNull<unsigned char>(allocator, walkableCount);

		for (std::size_t i = 0; i < walkableRead; ++i)
		{
			clipMap->triEdgeIsWalkable[i] = jsonWalkable[static_cast<rapidjson::SizeType>(i)].Get<std::uint8_t>();
		}

		const auto& jsonBorders = json["borders"];
		clipMap->borderCount = jsonBorders.Size();
		clipMap->borders = AllocateArrayOrNull<Game::CollisionBorder>(allocator, clipMap->borderCount);

		for (unsigned int i = 0; i < clipMap->borderCount; ++i)
		{
			const auto& jsonBorder = jsonBorders[i];
			auto* const border = &clipMap->borders[i];

			if (!HasArray(jsonBorder, "distEq", 3))
			{
				return fail("a border");
			}

			Utils::JSON::CopyArray(border->distEq, jsonBorder["distEq"], 3);
			border->zBase = jsonBorder["zBase"].Get<float>();
			border->zSlope = jsonBorder["zSlope"].Get<float>();
			border->start = jsonBorder["start"].Get<float>();
			border->length = jsonBorder["length"].Get<float>();
		}

		const auto& jsonPartitions = json["partitions"];
		clipMap->partitionCount = jsonPartitions.Size();
		clipMap->partitions = AllocateArrayOrNull<Game::CollisionPartition>(allocator, clipMap->partitionCount);

		for (unsigned int i = 0; i < clipMap->partitionCount; ++i)
		{
			const auto& jsonPartition = jsonPartitions[i];
			auto* const partition = &clipMap->partitions[i];

			if (!jsonPartition.IsObject())
			{
				return fail("a partition");
			}

			partition->triCount = jsonPartition["triCount"].Get<std::uint8_t>();
			partition->firstVertSegment = jsonPartition["firstVertSegment"].Get<std::uint8_t>();
			partition->firstTri = jsonPartition["firstTri"].Get<std::int32_t>();
			partition->borderCount = jsonPartition["borderCount"].Get<std::uint8_t>();

			if (partition->borderCount > 0)
			{
				std::size_t borderIndex = 0;

				if (!jsonPartition.HasMember("firstBorder") || !TryReadIndex(jsonPartition["firstBorder"], clipMap->borderCount, borderIndex) || borderIndex + partition->borderCount > clipMap->borderCount)
				{
					return fail("a partition's borders");
				}

				partition->borders = &clipMap->borders[borderIndex];
			}
		}

		const auto& jsonTrees = json["aabbTrees"];
		clipMap->aabbTreeCount = jsonTrees.Size();
		clipMap->aabbTrees = AllocateArrayOrNull<Game::CollisionAabbTree>(allocator, clipMap->aabbTreeCount);

		for (unsigned int i = 0; i < clipMap->aabbTreeCount; ++i)
		{
			const auto& jsonTree = jsonTrees[i];
			auto* const tree = &clipMap->aabbTrees[i];

			if (!HasArray(jsonTree, "midPoint", 3) || !HasArray(jsonTree, "halfSize", 3))
			{
				return fail("an aabb tree");
			}

			Utils::JSON::CopyArray(tree->midPoint, jsonTree["midPoint"], 3);
			Utils::JSON::CopyArray(tree->halfSize, jsonTree["halfSize"], 3);
			tree->materialIndex = jsonTree["materialIndex"].Get<std::uint16_t>();
			tree->childCount = jsonTree["childCount"].Get<std::uint16_t>();
			tree->u.firstChildIndex = static_cast<int>(jsonTree["u"].Get<std::uint32_t>());
		}

		const auto& jsonModelsCollision = json["cmodels"];
		clipMap->numSubModels = jsonModelsCollision.Size();
		clipMap->cmodels = AllocateArrayOrNull<Game::cmodel_t>(allocator, clipMap->numSubModels);

		for (unsigned int i = 0; i < clipMap->numSubModels; ++i)
		{
			const auto& jsonModel = jsonModelsCollision[i];
			auto* const cmodel = &clipMap->cmodels[i];

			if (!jsonModel.IsObject() || !jsonModel.HasMember("bounds") || !jsonModel.HasMember("leaf"))
			{
				return fail("a cmodel");
			}

			if (!TryReadBounds(jsonModel["bounds"], cmodel->bounds) || !TryReadLeaf(jsonModel["leaf"], cmodel->leaf))
			{
				return fail("a cmodel's bounds or leaf");
			}

			cmodel->radius = jsonModel["radius"].Get<float>();
		}

		const auto& jsonBrushes = json["brushes"];

		if (jsonBrushes.Size() > 0xFFFF)
		{
			return fail("more brushes than a clipmap holds");
		}

		clipMap->numBrushes = static_cast<unsigned short>(jsonBrushes.Size());
		clipMap->brushes = AllocateArrayOrNull<Game::cbrush_t>(allocator, clipMap->numBrushes);

		for (unsigned int i = 0; i < clipMap->numBrushes; ++i)
		{
			const auto& jsonBrush = jsonBrushes[i];
			auto* const brush = &clipMap->brushes[i];

			if (!HasArray(jsonBrush, "axialMaterialNum", 2) || !HasArray(jsonBrush, "firstAdjacentSideOffsets", 2) || !HasArray(jsonBrush, "edgeCount", 2) || !jsonBrush.HasMember("baseAdjacentSide"))
			{
				return fail("a brush");
			}

			brush->glassPieceIndex = jsonBrush["glassPieceIndex"].Get<std::uint16_t>();
			brush->numsides = jsonBrush["numsides"].Get<std::uint16_t>();

			if (brush->numsides)
			{
				std::size_t sideIndex = 0;

				if (!jsonBrush.HasMember("firstSide") || !TryReadIndex(jsonBrush["firstSide"], clipMap->numBrushSides, sideIndex) || sideIndex + brush->numsides > clipMap->numBrushSides)
				{
					return fail("a brush's sides");
				}

				brush->sides = &clipMap->brushsides[sideIndex];
			}

			if (jsonBrush["baseAdjacentSide"].IsString())
			{
				std::size_t edgeIndex = 0;

				if (!TryReadIndex(jsonBrush["baseAdjacentSide"], clipMap->numBrushEdges, edgeIndex))
				{
					return fail("a brush's baseAdjacentSide");
				}

				brush->baseAdjacentSide = &clipMap->brushEdges[edgeIndex];
			}

			for (rapidjson::SizeType x = 0; x < 2; ++x)
			{
				const auto& axialMaterialNum = jsonBrush["axialMaterialNum"][x];
				const auto& firstAdjacentSideOffsets = jsonBrush["firstAdjacentSideOffsets"][x];
				const auto& edgeCount = jsonBrush["edgeCount"][x];

				if (!IsArrayOf(axialMaterialNum, 3) || !IsArrayOf(firstAdjacentSideOffsets, 3) || !IsArrayOf(edgeCount, 3))
				{
					return fail("a brush's axial arrays");
				}

				Utils::JSON::CopyArray(brush->axialMaterialNum[x], axialMaterialNum, 3);
				Utils::JSON::CopyArray(brush->firstAdjacentSideOffsets[x], firstAdjacentSideOffsets, 3);
				Utils::JSON::CopyArray(brush->edgeCount[x], edgeCount, 3);
			}
		}

		const auto& jsonBrushBounds = json["brushBounds"];
		const auto& jsonBrushContents = json["brushContents"];

		if (jsonBrushBounds.Size() != clipMap->numBrushes || jsonBrushContents.Size() != clipMap->numBrushes)
		{
			return fail("brushBounds or brushContents does not match the brush count");
		}

		clipMap->brushBounds = allocator->AllocateArray<Game::Bounds>(clipMap->numBrushes);

		for (unsigned int i = 0; i < clipMap->numBrushes; ++i)
		{
			if (!TryReadBounds(jsonBrushBounds[i], clipMap->brushBounds[i]))
			{
				return fail("a brush's bounds");
			}
		}

		clipMap->brushContents = allocator->AllocateArray<int>(clipMap->numBrushes);

		for (unsigned int i = 0; i < clipMap->numBrushes; ++i)
		{
			clipMap->brushContents[i] = jsonBrushContents[i].Get<std::int32_t>();
		}

		const auto& jsonEnts = json["mapEnts"];

		if (!jsonEnts.IsNull())
		{
			if (!HasString(jsonEnts, "name"))
			{
				return fail("mapEnts");
			}

			clipMap->mapEnts = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MAP_ENTS, jsonEnts["name"].GetString(), builder).mapEnts;

			if (!clipMap->mapEnts)
			{
				return fail(std::format("map ents '{}' could not be found", jsonEnts["name"].GetString()));
			}

			if (!TryReadTriggers(jsonEnts, clipMap->mapEnts, allocator))
			{
				return fail("mapEnts' triggers or stages");
			}
		}

		const auto& jsonSmodelNodes = json["smodelNodes"];

		if (jsonSmodelNodes.Size() > 0xFFFF)
		{
			return fail("more smodel nodes than a clipmap holds");
		}

		clipMap->smodelNodeCount = static_cast<unsigned short>(jsonSmodelNodes.Size());
		clipMap->smodelNodes = AllocateArrayOrNull<Game::SModelAabbNode>(allocator, clipMap->smodelNodeCount);

		for (unsigned int i = 0; i < clipMap->smodelNodeCount; ++i)
		{
			const auto& jsonNode = jsonSmodelNodes[i];
			auto* const node = &clipMap->smodelNodes[i];

			if (!jsonNode.IsObject() || !jsonNode.HasMember("bounds") || !TryReadBounds(jsonNode["bounds"], node->bounds))
			{
				return fail("an smodel node");
			}

			node->firstChild = jsonNode["firstChild"].Get<std::uint16_t>();
			node->childCount = jsonNode["childCount"].Get<std::uint16_t>();
		}

		for (rapidjson::SizeType i = 0; i < 2; ++i)
		{
			const auto& jsonEntities = json["dynEntities"][i];

			if (jsonEntities.IsNull())
			{
				continue;
			}

			if (!jsonEntities.IsArray() || jsonEntities.Size() > 0xFFFF)
			{
				return fail("dynEntities");
			}

			const auto count = static_cast<unsigned short>(jsonEntities.Size());
			clipMap->dynEntCount[i] = count;
			clipMap->dynEntClientList[i] = AllocateArrayOrNull<Game::DynEntityClient>(allocator, count);
			clipMap->dynEntCollList[i] = AllocateArrayOrNull<Game::DynEntityColl>(allocator, count);
			clipMap->dynEntPoseList[i] = AllocateArrayOrNull<Game::DynEntityPose>(allocator, count);
			clipMap->dynEntDefList[i] = AllocateArrayOrNull<Game::DynEntityDef>(allocator, count);

			for (unsigned int j = 0; j < count; ++j)
			{
				if (!TryReadDynEntityDef(jsonEntities[j], &clipMap->dynEntDefList[i][j], builder))
				{
					return fail("a dyn entity");
				}
			}
		}

		clipMap->checksum = json["checksum"].Get<std::uint32_t>();

		return clipMap;
	}

	void IclipMap_t::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->clipMap = TryReadClipMap(name, builder);
	}

	void IclipMap_t::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.clipMap;

		for (unsigned int i = 0; i < asset->numStaticModels; ++i)
		{
			auto* const model = asset->staticModelList[i].xmodel;

			if (model)
			{
				builder->LoadAsset(Game::ASSET_TYPE_XMODEL, model);
			}
		}

		for (int j = 0; j < 2; ++j)
		{
			const auto* const defs = asset->dynEntDefList[j];

			for (int i = 0; i < asset->dynEntCount[j]; ++i)
			{
				if (defs[i].xModel)
				{
					builder->LoadAsset(Game::ASSET_TYPE_XMODEL, defs[i].xModel);
				}

				if (defs[i].destroyFx)
				{
					builder->LoadAsset(Game::ASSET_TYPE_FX, defs[i].destroyFx);
				}

				if (defs[i].physPreset)
				{
					builder->LoadAsset(Game::ASSET_TYPE_PHYSPRESET, defs[i].physPreset);
				}
			}
		}

		if (asset->mapEnts)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MAP_ENTS, asset->mapEnts);
		}
	}

	void IclipMap_t::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::clipMap_t, 256);
		SaveLogEnter("clipMap_t");

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.clipMap;
		auto* const dest = buffer->Dest<Game::X86::clipMap_t>();

		const auto clipMap = Game::X86::Convert(*asset);
		buffer->Save(&clipMap);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->planes)
		{
			AssertSize(Game::X86::cplane_s, 20);
			SaveLogEnter("cplane_t");

			if (builder->HasPointer(asset->planes))
			{
				dest->planes = builder->GetPointer(asset->planes);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);

				for (unsigned int i = 0; i < asset->planeCount; ++i)
				{
					builder->StorePointer(&asset->planes[i]);
					SaveRaw(buffer, &asset->planes[i], 1);
				}

				Utils::Stream::ClearPointer(&dest->planes);
			}

			SaveLogExit();
		}

		if (asset->staticModelList)
		{
			AssertSize(Game::X86::cStaticModel_s, 76);
			SaveLogEnter("cStaticModel_t");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destStaticModelList = SaveConverted(buffer, asset->staticModelList, asset->numStaticModels);

			for (unsigned int i = 0; i < asset->numStaticModels; ++i)
			{
				if (asset->staticModelList[i].xmodel)
				{
					destStaticModelList[i].xmodel = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL, asset->staticModelList[i].xmodel);
				}
			}

			Utils::Stream::ClearPointer(&dest->staticModelList);
			SaveLogExit();
		}

		if (asset->materials)
		{
			AssertSize(Game::X86::ClipMaterial, 12);
			SaveLogEnter("ClipMaterial");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const materials = SaveConverted(buffer, asset->materials, asset->numMaterials);

			for (unsigned int i = 0; i < asset->numMaterials; ++i)
			{
				if (asset->materials[i].name)
				{
					buffer->SaveString(asset->materials[i].name);
					Utils::Stream::ClearPointer(&materials[i].name);
				}
			}

			Utils::Stream::ClearPointer(&dest->materials);
			SaveLogExit();
		}

		if (asset->brushsides)
		{
			AssertSize(Game::X86::cbrushside_t, 8);
			SaveLogEnter("cbrushside_t");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const sides = buffer->Dest<Game::X86::cbrushside_t>();

			for (unsigned int i = 0; i < asset->numBrushSides; ++i)
			{
				builder->StorePointer(&asset->brushsides[i]);

				const auto side = Game::X86::Convert(asset->brushsides[i]);
				buffer->Save(&side);
			}

			for (unsigned int i = 0; i < asset->numBrushSides; ++i)
			{
				const auto* const plane = asset->brushsides[i].plane;

				if (!plane)
				{
					continue;
				}

				if (builder->HasPointer(plane))
				{
					sides[i].plane = builder->GetPointer(plane);
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					builder->StorePointer(plane);

					SaveRaw(buffer, plane, 1);
					Utils::Stream::ClearPointer(&sides[i].plane);
				}
			}

			Utils::Stream::ClearPointer(&dest->brushsides);
			SaveLogExit();
		}

		if (asset->brushEdges)
		{
			SaveLogEnter("cBrushEdge");

			for (unsigned int i = 0; i < asset->numBrushEdges; ++i)
			{
				builder->StorePointer(&asset->brushEdges[i]);
				buffer->Save(&asset->brushEdges[i]);
			}

			Utils::Stream::ClearPointer(&dest->brushEdges);
			SaveLogExit();
		}

		if (asset->nodes)
		{
			AssertSize(Game::X86::cNode_t, 8);
			SaveLogEnter("cNode_t");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const nodes = SaveConverted(buffer, asset->nodes, asset->numNodes);

			for (unsigned int i = 0; i < asset->numNodes; ++i)
			{
				const auto* const plane = asset->nodes[i].plane;

				if (!plane)
				{
					AssertUnreachable;
					Components::Logger::Fatal("node {} of clipmap {} has no plane!", i, asset->name);
				}

				if (builder->HasPointer(plane))
				{
					nodes[i].plane = builder->GetPointer(plane);
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					builder->StorePointer(plane);

					SaveRaw(buffer, plane, 1);
					Utils::Stream::ClearPointer(&nodes[i].plane);
				}
			}

			Utils::Stream::ClearPointer(&dest->nodes);
			SaveLogExit();
		}

		if (asset->leafs)
		{
			AssertSize(Game::X86::cLeaf_t, 40);
			SaveLogEnter("cLeaf_t");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->leafs, asset->numLeafs);
			Utils::Stream::ClearPointer(&dest->leafs);

			SaveLogExit();
		}

		if (asset->leafbrushes)
		{
			SaveLogEnter("cLeafBrush_t");

			buffer->Align(Utils::Stream::ALIGN_2);

			for (unsigned int i = 0; i < asset->numLeafBrushes; ++i)
			{
				builder->StorePointer(&asset->leafbrushes[i]);
				buffer->Save(&asset->leafbrushes[i]);
			}

			Utils::Stream::ClearPointer(&dest->leafbrushes);
			SaveLogExit();
		}

		if (asset->leafbrushNodes)
		{
			AssertSize(Game::X86::cLeafBrushNode_s, 20);
			SaveLogEnter("cLeafBrushNode_t");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const nodes = buffer->Dest<Game::X86::cLeafBrushNode_s>();

			for (unsigned int i = 0; i < asset->leafbrushNodesCount; ++i)
			{
				auto node = Game::X86::Convert(asset->leafbrushNodes[i]);

				if (node.leafBrushCount > 0)
				{
					std::memset(&node.data, 0, sizeof(node.data));
				}

				buffer->Save(&node);
			}

			for (unsigned int i = 0; i < asset->leafbrushNodesCount; ++i)
			{
				const auto* const node = &asset->leafbrushNodes[i];

				if (node->leafBrushCount <= 0 || !node->data.leaf.brushes)
				{
					continue;
				}

				if (builder->HasPointer(node->data.leaf.brushes))
				{
					nodes[i].data.leaf.brushes = builder->GetPointer(node->data.leaf.brushes);
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_2);

					for (short j = 0; j < node->leafBrushCount; ++j)
					{
						builder->StorePointer(&node->data.leaf.brushes[j]);
						buffer->Save(&node->data.leaf.brushes[j]);
					}

					Utils::Stream::ClearPointer(&nodes[i].data.leaf.brushes);
				}
			}

			Utils::Stream::ClearPointer(&dest->leafbrushNodes);
			SaveLogExit();
		}

		if (asset->leafsurfaces)
		{
			SaveLogEnter("cLeafSurface_t");

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->leafsurfaces, asset->numLeafSurfaces);
			Utils::Stream::ClearPointer(&dest->leafsurfaces);

			SaveLogExit();
		}

		if (asset->verts)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->verts, asset->vertCount);
			Utils::Stream::ClearPointer(&dest->verts);
		}

		if (asset->triIndices)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->Save(asset->triIndices, 6, asset->triCount);
			Utils::Stream::ClearPointer(&dest->triIndices);
		}

		if (asset->triEdgeIsWalkable)
		{
			buffer->Save(asset->triEdgeIsWalkable, 1, 4 * ((3 * asset->triCount + 31) >> 5));
			Utils::Stream::ClearPointer(&dest->triEdgeIsWalkable);
		}

		if (asset->borders)
		{
			AssertSize(Game::X86::CollisionBorder, 28);
			SaveLogEnter("CollisionBorder");

			buffer->Align(Utils::Stream::ALIGN_4);

			for (unsigned int i = 0; i < asset->borderCount; ++i)
			{
				builder->StorePointer(&asset->borders[i]);
				SaveRaw(buffer, &asset->borders[i], 1);
			}

			Utils::Stream::ClearPointer(&dest->borders);
			SaveLogExit();
		}

		if (asset->partitions)
		{
			AssertSize(Game::X86::CollisionPartition, 12);
			SaveLogEnter("CollisionPartition");

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destPartitions = SaveConverted(buffer, asset->partitions, asset->partitionCount);

			for (unsigned int i = 0; i < asset->partitionCount; ++i)
			{
				const auto* const borders = asset->partitions[i].borders;

				if (!borders)
				{
					continue;
				}

				if (builder->HasPointer(borders))
				{
					destPartitions[i].borders = builder->GetPointer(borders);
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					builder->StorePointer(borders);

					SaveRaw(buffer, borders, 1);
					Utils::Stream::ClearPointer(&destPartitions[i].borders);
				}
			}

			Utils::Stream::ClearPointer(&dest->partitions);
			SaveLogExit();
		}

		if (asset->aabbTrees)
		{
			AssertSize(Game::X86::CollisionAabbTree, 32);
			SaveLogEnter("CollisionAabbTree");

			buffer->Align(Utils::Stream::ALIGN_16);
			SaveRaw(buffer, asset->aabbTrees, asset->aabbTreeCount);
			Utils::Stream::ClearPointer(&dest->aabbTrees);

			SaveLogExit();
		}

		if (asset->cmodels)
		{
			AssertSize(Game::X86::cmodel_t, 68);
			SaveLogEnter("cmodel_t");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->cmodels, asset->numSubModels);
			Utils::Stream::ClearPointer(&dest->cmodels);

			SaveLogExit();
		}

		if (asset->brushes)
		{
			AssertSize(Game::X86::cbrush_t, 36);
			SaveLogEnter("cbrush_t");

			buffer->Align(Utils::Stream::ALIGN_128);

			auto* const destBrushes = SaveConverted(buffer, asset->brushes, asset->numBrushes);

			for (unsigned short i = 0; i < asset->numBrushes; ++i)
			{
				auto* const destBrush = &destBrushes[i];
				const auto* const brush = &asset->brushes[i];

				if (brush->sides)
				{
					if (builder->HasPointer(brush->sides))
					{
						destBrush->sides = builder->GetPointer(brush->sides);
					}
					else
					{
						AssertSize(Game::X86::cbrushside_t, 8);

						Components::Logger::Warning("BrushSide shouldn't be written in cBrush!\n");

						buffer->Align(Utils::Stream::ALIGN_4);
						builder->StorePointer(brush->sides);

						auto* const side = buffer->Dest<Game::X86::cbrushside_t>();
						const auto storedSide = Game::X86::Convert(*brush->sides);
						buffer->Save(&storedSide);

						if (brush->sides->plane)
						{
							if (builder->HasPointer(brush->sides->plane))
							{
								side->plane = builder->GetPointer(brush->sides->plane);
							}
							else
							{
								buffer->Align(Utils::Stream::ALIGN_4);
								builder->StorePointer(brush->sides->plane);

								SaveRaw(buffer, brush->sides->plane, 1);
								Utils::Stream::ClearPointer(&side->plane);
							}
						}

						Utils::Stream::ClearPointer(&destBrush->sides);
					}
				}

				if (brush->baseAdjacentSide)
				{
					if (builder->HasPointer(brush->baseAdjacentSide))
					{
						destBrush->baseAdjacentSide = builder->GetPointer(brush->baseAdjacentSide);
					}
					else
					{
						builder->StorePointer(brush->baseAdjacentSide);
						buffer->Save(brush->baseAdjacentSide);
						Utils::Stream::ClearPointer(&destBrush->baseAdjacentSide);
					}
				}
			}

			Utils::Stream::ClearPointer(&dest->brushes);
			SaveLogExit();
		}

		if (asset->brushBounds)
		{
			AssertSize(Game::X86::Bounds, 24);
			SaveLogEnter("Bounds");

			buffer->Align(Utils::Stream::ALIGN_128);
			SaveRaw(buffer, asset->brushBounds, asset->numBrushes);
			Utils::Stream::ClearPointer(&dest->brushBounds);

			SaveLogExit();
		}

		if (asset->brushContents)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->brushContents, asset->numBrushes);
			Utils::Stream::ClearPointer(&dest->brushContents);
		}

		if (asset->smodelNodes)
		{
			AssertSize(Game::X86::SModelAabbNode, 28);
			SaveLogEnter("SModelAabbNode");

			buffer->Align(Utils::Stream::ALIGN_4);
			SaveRaw(buffer, asset->smodelNodes, asset->smodelNodeCount);
			Utils::Stream::ClearPointer(&dest->smodelNodes);

			SaveLogExit();
		}

		if (asset->mapEnts)
		{
			dest->mapEnts = builder->SaveSubAsset(Game::ASSET_TYPE_MAP_ENTS, asset->mapEnts);
		}

		for (int i = 0; i < 2; ++i)
		{
			if (!asset->dynEntDefList[i])
			{
				continue;
			}

			AssertSize(Game::X86::DynEntityDef, 92);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const dynEntDest = SaveConverted(buffer, asset->dynEntDefList[i], asset->dynEntCount[i]);
			const auto* const dynEnt = asset->dynEntDefList[i];

			for (int j = 0; j < asset->dynEntCount[i]; ++j)
			{
				if (dynEnt[j].xModel)
				{
					dynEntDest[j].xModel = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL, dynEnt[j].xModel);
				}

				if (dynEnt[j].destroyFx)
				{
					dynEntDest[j].destroyFx = builder->SaveSubAsset(Game::ASSET_TYPE_FX, dynEnt[j].destroyFx);
				}

				if (dynEnt[j].physPreset)
				{
					dynEntDest[j].physPreset = builder->SaveSubAsset(Game::ASSET_TYPE_PHYSPRESET, dynEnt[j].physPreset);
				}
			}

			Utils::Stream::ClearPointer(&dest->dynEntDefList[i]);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		for (int i = 0; i < 2; ++i)
		{
			if (asset->dynEntPoseList[i])
			{
				AssertSize(Game::X86::DynEntityPose, 32);

				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->Save(asset->dynEntPoseList[i], sizeof(Game::X86::DynEntityPose), asset->dynEntCount[i]);
				Utils::Stream::ClearPointer(&dest->dynEntPoseList[i]);
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (asset->dynEntClientList[i])
			{
				AssertSize(Game::X86::DynEntityClient, 12);

				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->Save(asset->dynEntClientList[i], sizeof(Game::X86::DynEntityClient), asset->dynEntCount[i]);
				Utils::Stream::ClearPointer(&dest->dynEntClientList[i]);
			}
		}

		for (int i = 0; i < 2; ++i)
		{
			if (asset->dynEntCollList[i])
			{
				AssertSize(Game::X86::DynEntityColl, 20);

				buffer->Align(Utils::Stream::ALIGN_4);
				buffer->Save(asset->dynEntCollList[i], sizeof(Game::X86::DynEntityColl), asset->dynEntCount[i]);
				Utils::Stream::ClearPointer(&dest->dynEntCollList[i]);
			}
		}

		buffer->PopBlock();
		buffer->PopBlock();

		SaveLogExit();
	}

	static rapidjson::Value StringValue(const char* text)
	{
		if (!text)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return rapidjson::Value(rapidjson::StringRef(text));
	}

	template <typename T>
	static rapidjson::Value NameValue(const T* asset)
	{
		if (!asset)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return StringValue(asset->name);
	}

	template <typename T>
	static rapidjson::Value IndexValue(const std::unordered_map<const T*, std::size_t>& indices, const T* pointer, Utils::JSON::Allocator& allocator)
	{
		std::size_t index = 0;
		const auto found = indices.find(pointer);

		if (found != indices.end())
		{
			index = found->second;
		}

		return rapidjson::Value(std::format("#{}", index), allocator);
	}

	static rapidjson::Value LeafToJson(const Game::cLeaf_t& leaf, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);

		json.AddMember("firstCollAabbIndex", leaf.firstCollAabbIndex, allocator);
		json.AddMember("collAabbCount", leaf.collAabbCount, allocator);
		json.AddMember("brushContents", leaf.brushContents, allocator);
		json.AddMember("terrainContents", leaf.terrainContents, allocator);
		json.AddMember("leafBrushNode", leaf.leafBrushNode, allocator);
		json.AddMember("bounds", Utils::JSON::ToJson(leaf.bounds, allocator), allocator);

		return json;
	}

	static rapidjson::Value TriggersToJson(const Game::MapEnts* ents, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value jsonEnts(rapidjson::kObjectType);
		jsonEnts.AddMember("name", StringValue(ents->name), allocator);

		rapidjson::Value jsonTrigger(rapidjson::kObjectType);
		rapidjson::Value jsonModels(rapidjson::kArrayType);

		for (unsigned int i = 0; i < ents->trigger.count; ++i)
		{
			rapidjson::Value jsonModel(rapidjson::kObjectType);
			jsonModel.AddMember("contents", ents->trigger.models[i].contents, allocator);
			jsonModel.AddMember("hullCount", ents->trigger.models[i].hullCount, allocator);
			jsonModel.AddMember("firstHull", ents->trigger.models[i].firstHull, allocator);

			jsonModels.PushBack(jsonModel, allocator);
		}

		jsonTrigger.AddMember("models", jsonModels, allocator);

		rapidjson::Value jsonHulls(rapidjson::kArrayType);

		for (unsigned int i = 0; i < ents->trigger.hullCount; ++i)
		{
			rapidjson::Value jsonHull(rapidjson::kObjectType);
			jsonHull.AddMember("bounds", Utils::JSON::ToJson(ents->trigger.hulls[i].bounds, allocator), allocator);
			jsonHull.AddMember("contents", ents->trigger.hulls[i].contents, allocator);
			jsonHull.AddMember("slabCount", ents->trigger.hulls[i].slabCount, allocator);
			jsonHull.AddMember("firstSlab", ents->trigger.hulls[i].firstSlab, allocator);

			jsonHulls.PushBack(jsonHull, allocator);
		}

		jsonTrigger.AddMember("hulls", jsonHulls, allocator);

		rapidjson::Value jsonSlabs(rapidjson::kArrayType);

		for (unsigned int i = 0; i < ents->trigger.slabCount; ++i)
		{
			rapidjson::Value jsonSlab(rapidjson::kObjectType);
			jsonSlab.AddMember("dir", Utils::JSON::MakeArray(ents->trigger.slabs[i].dir, 3, allocator), allocator);
			jsonSlab.AddMember("midPoint", ents->trigger.slabs[i].midPoint, allocator);
			jsonSlab.AddMember("halfSize", ents->trigger.slabs[i].halfSize, allocator);

			jsonSlabs.PushBack(jsonSlab, allocator);
		}

		jsonTrigger.AddMember("slabs", jsonSlabs, allocator);
		jsonEnts.AddMember("trigger", jsonTrigger, allocator);

		rapidjson::Value jsonStages(rapidjson::kArrayType);

		const auto stageCount = static_cast<unsigned char>(ents->stageCount);

		for (unsigned char i = 0; i < stageCount; ++i)
		{
			const auto* const stage = &ents->stages[i];

			rapidjson::Value jsonStage(rapidjson::kObjectType);
			jsonStage.AddMember("name", StringValue(stage->name), allocator);
			jsonStage.AddMember("origin", Utils::JSON::MakeArray(stage->origin, 3, allocator), allocator);
			jsonStage.AddMember("triggerIndex", stage->triggerIndex, allocator);
			jsonStage.AddMember("sunPrimaryLightIndex", stage->sunPrimaryLightIndex, allocator);

			jsonStages.PushBack(jsonStage, allocator);
		}

		jsonEnts.AddMember("stages", jsonStages, allocator);

		return jsonEnts;
	}

	static rapidjson::Value DynEntityDefToJson(const Game::DynEntityDef* def, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);

		json.AddMember("type", static_cast<int>(def->type), allocator);

		rapidjson::Value pose(rapidjson::kObjectType);
		pose.AddMember("quat", Utils::JSON::MakeArray(def->pose.quat, 4, allocator), allocator);
		pose.AddMember("origin", Utils::JSON::MakeArray(def->pose.origin, 3, allocator), allocator);
		json.AddMember("pose", pose, allocator);

		json.AddMember("xModel", NameValue(def->xModel), allocator);
		json.AddMember("brushModel", def->brushModel, allocator);
		json.AddMember("physicsBrushModel", def->physicsBrushModel, allocator);
		json.AddMember("destroyFx", NameValue(def->destroyFx), allocator);
		json.AddMember("physPreset", NameValue(def->physPreset), allocator);
		json.AddMember("health", def->health, allocator);

		rapidjson::Value mass(rapidjson::kObjectType);
		mass.AddMember("centerOfMass", Utils::JSON::MakeArray(def->mass.centerOfMass, 3, allocator), allocator);
		mass.AddMember("momentsOfInertia", Utils::JSON::MakeArray(def->mass.momentsOfInertia, 3, allocator), allocator);
		mass.AddMember("productsOfInertia", Utils::JSON::MakeArray(def->mass.productsOfInertia, 3, allocator), allocator);
		json.AddMember("mass", mass, allocator);

		json.AddMember("contents", def->contents, allocator);

		rapidjson::Value pack(rapidjson::kObjectType);
		pack.AddMember("dynEntityDef", json, allocator);

		return pack;
	}

	void IclipMap_t::Dump(Game::XAssetHeader header)
	{
		const auto* const clipMap = header.clipMap;

		if (!clipMap || !clipMap->name)
		{
			return;
		}

		std::unordered_map<const Game::cplane_s*, std::size_t> planes;
		std::unordered_map<const Game::cbrushside_t*, std::size_t> brushSides;
		std::unordered_map<const unsigned char*, std::size_t> brushEdges;
		std::unordered_map<const unsigned short*, std::size_t> leafBrushes;
		std::unordered_map<const Game::CollisionBorder*, std::size_t> borders;

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_CLIPMAP_VERSION, allocator);
		output.AddMember("name", StringValue(clipMap->name), allocator);
		output.AddMember("isInUse", clipMap->isInUse, allocator);

		rapidjson::Value jsonPlanes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->planeCount; ++i)
		{
			const auto* const plane = &clipMap->planes[i];

			rapidjson::Value jsonPlane(rapidjson::kObjectType);
			jsonPlane.AddMember("normal", Utils::JSON::MakeArray(plane->normal, 3, allocator), allocator);
			jsonPlane.AddMember("dist", plane->dist, allocator);
			jsonPlane.AddMember("type", plane->type, allocator);

			jsonPlanes.PushBack(jsonPlane, allocator);
			planes[plane] = i;
		}

		output.AddMember("planes", jsonPlanes, allocator);

		rapidjson::Value jsonStaticModels(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numStaticModels; ++i)
		{
			const auto* const staticModel = &clipMap->staticModelList[i];

			rapidjson::Value jsonStaticModel(rapidjson::kObjectType);
			jsonStaticModel.AddMember("xmodel", NameValue(staticModel->xmodel), allocator);
			jsonStaticModel.AddMember("origin", Utils::JSON::MakeArray(staticModel->origin, 3, allocator), allocator);

			rapidjson::Value invScaledAxis(rapidjson::kArrayType);

			for (const auto& row : staticModel->invScaledAxis)
			{
				invScaledAxis.PushBack(Utils::JSON::MakeArray(row, 3, allocator), allocator);
			}

			jsonStaticModel.AddMember("invScaledAxis", invScaledAxis, allocator);
			jsonStaticModel.AddMember("absBounds", Utils::JSON::ToJson(staticModel->absBounds, allocator), allocator);

			jsonStaticModels.PushBack(jsonStaticModel, allocator);
		}

		output.AddMember("staticModelList", jsonStaticModels, allocator);

		rapidjson::Value jsonMaterials(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numMaterials; ++i)
		{
			const auto* const material = &clipMap->materials[i];

			rapidjson::Value jsonMaterial(rapidjson::kObjectType);
			jsonMaterial.AddMember("name", StringValue(material->name), allocator);
			jsonMaterial.AddMember("surfaceFlags", material->surfaceFlags, allocator);
			jsonMaterial.AddMember("contents", material->contents, allocator);

			jsonMaterials.PushBack(jsonMaterial, allocator);
		}

		output.AddMember("materials", jsonMaterials, allocator);

		rapidjson::Value jsonBrushSides(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numBrushSides; ++i)
		{
			const auto* const brushSide = &clipMap->brushsides[i];

			rapidjson::Value jsonBrushSide(rapidjson::kObjectType);
			jsonBrushSide.AddMember("plane", IndexValue(planes, brushSide->plane, allocator), allocator);
			jsonBrushSide.AddMember("materialNum", brushSide->materialNum, allocator);
			jsonBrushSide.AddMember("firstAdjacentSideOffset", brushSide->firstAdjacentSideOffset, allocator);
			jsonBrushSide.AddMember("edgeCount", brushSide->edgeCount, allocator);

			jsonBrushSides.PushBack(jsonBrushSide, allocator);
			brushSides[brushSide] = i;
		}

		output.AddMember("brushsides", jsonBrushSides, allocator);

		rapidjson::Value jsonBrushEdges(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numBrushEdges; ++i)
		{
			jsonBrushEdges.PushBack(clipMap->brushEdges[i], allocator);
			brushEdges[&clipMap->brushEdges[i]] = i;
		}

		output.AddMember("brushEdges", jsonBrushEdges, allocator);

		rapidjson::Value jsonNodes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numNodes; ++i)
		{
			const auto* const node = &clipMap->nodes[i];

			rapidjson::Value jsonNode(rapidjson::kObjectType);
			jsonNode.AddMember("plane", IndexValue(planes, node->plane, allocator), allocator);
			jsonNode.AddMember("children", Utils::JSON::MakeArray(node->children, 2, allocator), allocator);

			jsonNodes.PushBack(jsonNode, allocator);
		}

		output.AddMember("nodes", jsonNodes, allocator);

		rapidjson::Value jsonLeafs(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numLeafs; ++i)
		{
			jsonLeafs.PushBack(LeafToJson(clipMap->leafs[i], allocator), allocator);
		}

		output.AddMember("leafs", jsonLeafs, allocator);

		rapidjson::Value jsonLeafBrushes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numLeafBrushes; ++i)
		{
			jsonLeafBrushes.PushBack(clipMap->leafbrushes[i], allocator);
			leafBrushes[&clipMap->leafbrushes[i]] = i;
		}

		output.AddMember("leafbrushes", jsonLeafBrushes, allocator);

		rapidjson::Value jsonLeafBrushNodes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->leafbrushNodesCount; ++i)
		{
			const auto* const leafBrushNode = &clipMap->leafbrushNodes[i];

			rapidjson::Value jsonLeafBrushNode(rapidjson::kObjectType);
			jsonLeafBrushNode.AddMember("axis", leafBrushNode->axis, allocator);
			jsonLeafBrushNode.AddMember("leafBrushCount", leafBrushNode->leafBrushCount, allocator);
			jsonLeafBrushNode.AddMember("contents", leafBrushNode->contents, allocator);

			if (leafBrushNode->leafBrushCount > 0)
			{
				const auto* const brushes = leafBrushNode->data.leaf.brushes;

				if (leafBrushes.contains(brushes))
				{
					jsonLeafBrushNode.AddMember("data", IndexValue(leafBrushes, brushes, allocator), allocator);
				}
				else
				{
					jsonLeafBrushNode.AddMember("data", Utils::JSON::MakeArray(brushes, static_cast<std::size_t>(leafBrushNode->leafBrushCount), allocator), allocator);
				}
			}
			else
			{
				rapidjson::Value data(rapidjson::kObjectType);
				data.AddMember("dist", leafBrushNode->data.children.dist, allocator);
				data.AddMember("range", leafBrushNode->data.children.range, allocator);
				data.AddMember("childOffset", Utils::JSON::MakeArray(leafBrushNode->data.children.childOffset, 2, allocator), allocator);

				jsonLeafBrushNode.AddMember("data", data, allocator);
			}

			jsonLeafBrushNodes.PushBack(jsonLeafBrushNode, allocator);
		}

		output.AddMember("leafbrushNodes", jsonLeafBrushNodes, allocator);

		rapidjson::Value jsonLeafSurfaces(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numLeafSurfaces; ++i)
		{
			jsonLeafSurfaces.PushBack(clipMap->leafsurfaces[i], allocator);
		}

		output.AddMember("leafsurfaces", jsonLeafSurfaces, allocator);

		rapidjson::Value jsonVertices(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->vertCount; ++i)
		{
			jsonVertices.PushBack(Utils::JSON::MakeArray(clipMap->verts[i], 3, allocator), allocator);
		}

		output.AddMember("verts", jsonVertices, allocator);

		rapidjson::Value jsonTris(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->triCount; ++i)
		{
			jsonTris.PushBack(Utils::JSON::MakeArray(&clipMap->triIndices[i * 3], 3, allocator), allocator);
		}

		output.AddMember("triIndices", jsonTris, allocator);

		rapidjson::Value jsonWalkable(rapidjson::kArrayType);
		const std::size_t walkableCount = 4 * ((3 * clipMap->triCount + 31) >> 5);

		for (std::size_t i = 0; i < walkableCount * 3; ++i)
		{
			unsigned char walkable = 0;

			if (clipMap->triEdgeIsWalkable && i < walkableCount)
			{
				walkable = clipMap->triEdgeIsWalkable[i];
			}

			jsonWalkable.PushBack(walkable, allocator);
		}

		output.AddMember("triEdgeIsWalkable", jsonWalkable, allocator);

		rapidjson::Value jsonBorders(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->borderCount; ++i)
		{
			const auto* const border = &clipMap->borders[i];

			rapidjson::Value jsonBorder(rapidjson::kObjectType);
			jsonBorder.AddMember("distEq", Utils::JSON::MakeArray(border->distEq, 3, allocator), allocator);
			jsonBorder.AddMember("zBase", border->zBase, allocator);
			jsonBorder.AddMember("zSlope", border->zSlope, allocator);
			jsonBorder.AddMember("start", border->start, allocator);
			jsonBorder.AddMember("length", border->length, allocator);

			jsonBorders.PushBack(jsonBorder, allocator);
			borders[border] = i;
		}

		output.AddMember("borders", jsonBorders, allocator);

		rapidjson::Value jsonPartitions(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->partitionCount; ++i)
		{
			const auto* const partition = &clipMap->partitions[i];

			rapidjson::Value jsonPartition(rapidjson::kObjectType);
			jsonPartition.AddMember("triCount", partition->triCount, allocator);
			jsonPartition.AddMember("firstVertSegment", partition->firstVertSegment, allocator);
			jsonPartition.AddMember("firstTri", partition->firstTri, allocator);
			jsonPartition.AddMember("borderCount", partition->borderCount, allocator);

			if (partition->borderCount)
			{
				jsonPartition.AddMember("firstBorder", IndexValue(borders, partition->borders, allocator), allocator);
			}

			jsonPartitions.PushBack(jsonPartition, allocator);
		}

		output.AddMember("partitions", jsonPartitions, allocator);

		rapidjson::Value jsonTrees(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->aabbTreeCount; ++i)
		{
			const auto* const tree = &clipMap->aabbTrees[i];

			rapidjson::Value jsonTree(rapidjson::kObjectType);
			jsonTree.AddMember("midPoint", Utils::JSON::MakeArray(tree->midPoint, 3, allocator), allocator);
			jsonTree.AddMember("halfSize", Utils::JSON::MakeArray(tree->halfSize, 3, allocator), allocator);
			jsonTree.AddMember("materialIndex", tree->materialIndex, allocator);
			jsonTree.AddMember("childCount", tree->childCount, allocator);
			jsonTree.AddMember("u", static_cast<std::uint32_t>(tree->u.firstChildIndex), allocator);

			jsonTrees.PushBack(jsonTree, allocator);
		}

		output.AddMember("aabbTrees", jsonTrees, allocator);

		rapidjson::Value jsonModels(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numSubModels; ++i)
		{
			const auto* const cmodel = &clipMap->cmodels[i];

			rapidjson::Value jsonModel(rapidjson::kObjectType);
			jsonModel.AddMember("bounds", Utils::JSON::ToJson(cmodel->bounds, allocator), allocator);
			jsonModel.AddMember("radius", cmodel->radius, allocator);
			jsonModel.AddMember("leaf", LeafToJson(cmodel->leaf, allocator), allocator);

			jsonModels.PushBack(jsonModel, allocator);
		}

		output.AddMember("cmodels", jsonModels, allocator);

		rapidjson::Value jsonBrushes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numBrushes; ++i)
		{
			const auto* const brush = &clipMap->brushes[i];

			rapidjson::Value jsonBrush(rapidjson::kObjectType);
			jsonBrush.AddMember("glassPieceIndex", brush->glassPieceIndex, allocator);
			jsonBrush.AddMember("numsides", brush->numsides, allocator);

			if (brush->numsides > 0)
			{
				jsonBrush.AddMember("firstSide", IndexValue(brushSides, brush->sides, allocator), allocator);
			}

			if (brush->baseAdjacentSide)
			{
				jsonBrush.AddMember("baseAdjacentSide", IndexValue(brushEdges, brush->baseAdjacentSide, allocator), allocator);
			}
			else
			{
				jsonBrush.AddMember("baseAdjacentSide", rapidjson::Value(rapidjson::kNullType), allocator);
			}

			rapidjson::Value axialMaterialNum(rapidjson::kArrayType);
			rapidjson::Value firstAdjacentSideOffsets(rapidjson::kArrayType);
			rapidjson::Value edgeCount(rapidjson::kArrayType);

			for (int x = 0; x < 2; ++x)
			{
				axialMaterialNum.PushBack(Utils::JSON::MakeArray(brush->axialMaterialNum[x], 3, allocator), allocator);
				firstAdjacentSideOffsets.PushBack(Utils::JSON::MakeArray(brush->firstAdjacentSideOffsets[x], 3, allocator), allocator);
				edgeCount.PushBack(Utils::JSON::MakeArray(brush->edgeCount[x], 3, allocator), allocator);
			}

			jsonBrush.AddMember("axialMaterialNum", axialMaterialNum, allocator);
			jsonBrush.AddMember("firstAdjacentSideOffsets", firstAdjacentSideOffsets, allocator);
			jsonBrush.AddMember("edgeCount", edgeCount, allocator);

			jsonBrushes.PushBack(jsonBrush, allocator);
		}

		output.AddMember("brushes", jsonBrushes, allocator);

		rapidjson::Value jsonBrushBounds(rapidjson::kArrayType);
		rapidjson::Value jsonBrushContents(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->numBrushes; ++i)
		{
			jsonBrushBounds.PushBack(Utils::JSON::ToJson(clipMap->brushBounds[i], allocator), allocator);
			jsonBrushContents.PushBack(clipMap->brushContents[i], allocator);
		}

		output.AddMember("brushBounds", jsonBrushBounds, allocator);
		output.AddMember("brushContents", jsonBrushContents, allocator);

		if (clipMap->mapEnts)
		{
			output.AddMember("mapEnts", TriggersToJson(clipMap->mapEnts, allocator), allocator);
		}
		else
		{
			output.AddMember("mapEnts", rapidjson::Value(rapidjson::kNullType), allocator);
		}

		rapidjson::Value jsonSmodelNodes(rapidjson::kArrayType);

		for (unsigned int i = 0; i < clipMap->smodelNodeCount; ++i)
		{
			const auto* const node = &clipMap->smodelNodes[i];

			rapidjson::Value jsonNode(rapidjson::kObjectType);
			jsonNode.AddMember("bounds", Utils::JSON::ToJson(node->bounds, allocator), allocator);
			jsonNode.AddMember("firstChild", node->firstChild, allocator);
			jsonNode.AddMember("childCount", node->childCount, allocator);

			jsonSmodelNodes.PushBack(jsonNode, allocator);
		}

		output.AddMember("smodelNodes", jsonSmodelNodes, allocator);

		rapidjson::Value jsonDynEntities(rapidjson::kArrayType);

		for (int i = 0; i < 2; ++i)
		{
			const auto* const defList = clipMap->dynEntDefList[i];

			if (!defList)
			{
				jsonDynEntities.PushBack(rapidjson::Value(rapidjson::kNullType), allocator);
				continue;
			}

			rapidjson::Value jsonDefList(rapidjson::kArrayType);

			for (int j = 0; j < clipMap->dynEntCount[i]; ++j)
			{
				jsonDefList.PushBack(DynEntityDefToJson(&defList[j], allocator), allocator);
			}

			jsonDynEntities.PushBack(jsonDefList, allocator);
		}

		output.AddMember("dynEntities", jsonDynEntities, allocator);
		output.AddMember("checksum", clipMap->checksum, allocator);

		rapidjson::StringBuffer stringBuffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfNullFlag | rapidjson::kWriteNanAndInfFlag> writer(stringBuffer);
		writer.SetFormatOptions(rapidjson::PrettyFormatOptions::kFormatSingleLineArray);
		output.Accept(writer);

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(clipMap->name));

		if (!Utils::IO::WriteFile(path, stringBuffer.GetString()))
		{
			Components::Logger::Error("Dumping clipmap '{}' failed, could not write {}\n", clipMap->name, path);
		}
	}
}
