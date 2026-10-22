#include "STDInclude.hpp"

#include "CollmapCollision.hpp"
#include "ClipMap.hpp"
#include "GSC/Script.hpp"
#include "Logger.hpp"
#include "ModelCache.hpp"
#include "Scheduler.hpp"
#include "ZombieCollision.hpp"

namespace Components
{
	using namespace ClipMap;

	constexpr std::uintptr_t SV_LinkEntity = 0x140233F90;
	constexpr std::uintptr_t CM_TransformedBrushModelTrace = 0x1401EF6F0;
	constexpr std::uintptr_t G_SetModel = 0x1401AC270;
	constexpr std::uintptr_t G_CallSpawnEntity = 0x1401A8B90;
	constexpr std::uintptr_t Scr_SetString = 0x140222090;
	constexpr std::uintptr_t Scr_GetEntity = 0x1401A9A00;
	constexpr std::uintptr_t Scr_AddVector = 0x1402295A0;
	constexpr std::uintptr_t G_EntLinkTo = 0x14018A150;
	constexpr std::uintptr_t G_EntUnlink = 0x14018A270;
	constexpr std::uintptr_t G_TraceCapsule = 0x14019F560;

	constexpr std::uintptr_t cached_models = 0x1419F2FC0;
	constexpr std::uintptr_t cgs_gameModels = 0x1405876A0;

	static const std::uint8_t linkEntityPrologue[] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x20, 0x56, 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00 };
	constexpr std::size_t absoluteJumpSize = 14;

	constexpr int contentsPlayerClip = 0x10000;
	constexpr int contentsBody = 0x2000000;
	constexpr int collmapContents = contentsSolid | contentsPlayerClip | contentsBody;

	constexpr char modelTypeModel = 0;
	constexpr char modelTypeBrush = 4;
	constexpr int brushSolid = 0xFFFFFF;

	constexpr float maxClientRadius = 222.0f;

	constexpr unsigned int maxNetworkedSubModel = 1023;

	struct BoxSet
	{
		std::span<const float> halfSizes;
		std::span<const float> heights;

		constexpr std::size_t Count() const
		{
			return halfSizes.size() * (halfSizes.size() + 1) / 2 * heights.size();
		}
	};

	constexpr float fineHalfSizes[] = { 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 24.0f, 28.0f, 32.0f, 40.0f, 48.0f, 56.0f, 64.0f };
	constexpr float fineHeights[] = { 8.0f, 16.0f, 24.0f, 32.0f, 40.0f, 48.0f, 56.0f, 64.0f, 72.0f, 80.0f, 88.0f, 96.0f, 112.0f, 128.0f, 160.0f, 192.0f, 224.0f, 256.0f };
	constexpr float mediumHalfSizes[] = { 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 32.0f, 40.0f, 48.0f, 64.0f };
	constexpr float mediumHeights[] = { 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 128.0f, 160.0f, 192.0f };
	constexpr float coarseHalfSizes[] = { 4.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 48.0f, 64.0f };
	constexpr float coarseHeights[] = { 16.0f, 32.0f, 48.0f, 64.0f, 96.0f, 128.0f, 192.0f };
	constexpr float roughHalfSizes[] = { 8.0f, 16.0f, 24.0f, 32.0f, 48.0f, 64.0f };
	constexpr float roughHeights[] = { 32.0f, 64.0f, 128.0f, 192.0f };

	constexpr BoxSet boxSets[] =
	{
		{ fineHalfSizes, fineHeights },
		{ mediumHalfSizes, mediumHeights },
		{ coarseHalfSizes, coarseHeights },
		{ roughHalfSizes, roughHeights },
	};

	static unsigned int boxBase = 0;
	static const BoxSet* boxSet = nullptr;

	struct Collmap
	{
		unsigned int handle;
		float radius;
	};

	struct AppendedBrush
	{
		std::size_t firstSide;
		std::size_t firstAdjacentSide;
		bool hasAdjacentSides;
	};

	static std::vector<cmodel_t> cmodels;
	static std::vector<cLeafBrushNode_s> leafbrushNodes;
	static std::vector<Game::cbrush_t> brushes;
	static std::vector<Game::Bounds> brushBounds;
	static std::vector<int> brushContents;
	static std::vector<Game::cbrushside_t> sides;
	static std::vector<std::size_t> sidePlanes;
	static std::vector<Game::cplane_s> planes;
	static std::vector<std::uint8_t> adjacentSides;
	static std::vector<std::uint16_t> leafBrushes;
	static std::vector<std::size_t> nodeLeafBrushes;
	static std::vector<AppendedBrush> appendedBrushes;

	static std::unordered_map<const Game::XModel*, Collmap> collmaps;

	static void(*linkEntity)(Game::gentity_s*) = nullptr;
	static Utils::Hook linkEntityHook;

	static const Game::XModel* GetServerModel(const unsigned int index)
	{
		if (ModelCache::cachedModelsReallocated)
		{
			return index < ModelCache::G_MODELINDEX_LIMIT ? ModelCache::cachedModelsReallocated[index] : nullptr;
		}

		return index < ModelCache::BASE_GMODEL_COUNT ? reinterpret_cast<Game::XModel* const*>(Utils::Hook::Rebase(cached_models))[index] : nullptr;
	}

	static const Game::XModel* GetClientModel(const unsigned int index)
	{
		if (ModelCache::gameModelsReallocated)
		{
			return index < ModelCache::G_MODELINDEX_LIMIT ? ModelCache::gameModelsReallocated[index] : nullptr;
		}

		return index < ModelCache::BASE_GMODEL_COUNT ? reinterpret_cast<Game::XModel* const*>(Utils::Hook::Rebase(cgs_gameModels))[index] : nullptr;
	}

	static const Collmap* FindCollmap(const Game::XModel* model)
	{
		if (!model || !model->physCollmap)
		{
			return nullptr;
		}

		const auto collmap = collmaps.find(model);
		return collmap != collmaps.end() ? &collmap->second : nullptr;
	}

	static bool IsCollmapType(const int eType)
	{
		return eType == Game::ET_GENERAL || eType == Game::ET_SCRIPTMOVER;
	}

	static const Collmap* FindForServer(const Game::gentity_s* ent)
	{
		if (collmaps.empty() || ent->r.modelType != modelTypeModel || !IsCollmapType(ent->s.eType) || !(ent->r.contents & collmapContents) || !ent->model)
		{
			return nullptr;
		}

		return FindCollmap(GetServerModel(ent->model));
	}

	static const Collmap* FindForClient(const Game::centity_s* cent)
	{
		const auto& es = cent->nextState;

		if (collmaps.empty() || !es.solid || es.solid == brushSolid || !IsCollmapType(es.eType))
		{
			return nullptr;
		}

		const int index = es.eType == Game::ET_SCRIPTMOVER ? es.lerp.u.scriptMover.xModelIndex : es.index;
		return index > 0 ? FindCollmap(GetClientModel(static_cast<unsigned int>(index))) : nullptr;
	}

	bool CollmapCollision::ClipToEntity(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const int contents, const float* origin)
	{
		if (collmaps.empty())
		{
			return false;
		}

		const Collmap* collmap = nullptr;
		const float* angles = nullptr;

		if (const auto* const ent = ServerEntityAt(origin))
		{
			collmap = FindForServer(ent);
			angles = ent->r.currentAngles;
		}
		else if (const auto* const cent = ClientEntityAt(origin))
		{
			collmap = FindForClient(cent);
			angles = cent->pose.angles;
		}

		if (!collmap)
		{
			return false;
		}

		const float oldFraction = results->fraction;

		reinterpret_cast<void(*)(Game::trace_t*, const float*, const float*, const Game::Bounds*, unsigned int, int, const float*, const float*)>(Utils::Hook::Rebase(CM_TransformedBrushModelTrace))
			(results, start, end, bounds, collmap->handle, contentsSolid, origin, angles);

		if (results->fraction < oldFraction)
		{
			results->contents = contents;
		}

		return true;
	}

	static void RefitBox(Game::gentity_s* owner);

	void CollmapCollision::SV_LinkEntity_Hk(Game::gentity_s* ent)
	{
		const auto* const collmap = FindForServer(ent);

		if (collmap)
		{
			std::fill(std::begin(ent->r.boxMidPoint), std::end(ent->r.boxMidPoint), 0.0f);
			std::fill(std::begin(ent->r.boxHalfSize), std::end(ent->r.boxHalfSize), collmap->radius);
		}

		linkEntity(ent);

		if (collmap)
		{
			Game::Bounds reach{};
			std::fill(std::begin(reach.halfSize), std::end(reach.halfSize), collmap->radius);
			ent->s.solid = EncodeSolid(reach);
		}
		else
		{
			ZombieCollision::PostLink(ent);
		}

		RefitBox(ent);
	}

	static void Include(Game::Bounds& total, const Game::Bounds& bounds, bool& isEmpty)
	{
		if (isEmpty)
		{
			total = bounds;
			isEmpty = false;
			return;
		}

		for (int i = 0; i < 3; ++i)
		{
			const float low = std::min(total.midPoint[i] - total.halfSize[i], bounds.midPoint[i] - bounds.halfSize[i]);
			const float high = std::max(total.midPoint[i] + total.halfSize[i], bounds.midPoint[i] + bounds.halfSize[i]);

			total.midPoint[i] = (low + high) * 0.5f;
			total.halfSize[i] = (high - low) * 0.5f;
		}
	}

	static float RadiusOf(const Game::Bounds& bounds)
	{
		float lengthSq = 0.0f;

		for (int i = 0; i < 3; ++i)
		{
			const float reach = std::fabs(bounds.midPoint[i]) + bounds.halfSize[i];
			lengthSq += reach * reach;
		}

		return std::sqrt(lengthSq);
	}

	static void AppendBrushWrapper(const Game::BrushWrapper& wrapper, const std::uint16_t material)
	{
		Game::cbrush_t brush = wrapper.brush;
		brush.glassPieceIndex = 0;

		AppendedBrush appended{ sides.size(), adjacentSides.size(), wrapper.brush.baseAdjacentSide && wrapper.totalEdgeCount > 0 };

		for (unsigned short i = 0; i < wrapper.brush.numsides; ++i)
		{
			Game::cbrushside_t side = wrapper.brush.sides[i];
			side.materialNum = material;

			sidePlanes.push_back(planes.size());
			planes.push_back(*side.plane);
			sides.push_back(side);
		}

		if (appended.hasAdjacentSides)
		{
			adjacentSides.insert(adjacentSides.end(), wrapper.brush.baseAdjacentSide, wrapper.brush.baseAdjacentSide + wrapper.totalEdgeCount);
		}

		for (auto& axial : brush.axialMaterialNum)
		{
			std::ranges::fill(axial, material);
		}

		brushes.push_back(brush);
		brushBounds.push_back(wrapper.bounds);
		brushContents.push_back(contentsSolid);
		appendedBrushes.push_back(appended);
	}

	static void AppendGeomBox(const Game::PhysGeomInfo& geom, const std::uint16_t material)
	{
		float axis[3][3]{};
		bool isZero = true;
		bool isAxial = true;

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				axis[i][j] = geom.orientation[i][j];
				isZero = isZero && axis[i][j] == 0.0f;
				isAxial = isAxial && std::fabs(axis[i][j] - (i == j ? 1.0f : 0.0f)) < 0.0001f;
			}
		}

		Game::cbrush_t brush{};
		Game::Bounds bounds = geom.bounds;
		AppendedBrush appended{ sides.size(), adjacentSides.size(), false };

		if (!isZero && !isAxial)
		{
			for (int j = 0; j < 3; ++j)
			{
				bounds.halfSize[j] = 0.0f;

				for (int i = 0; i < 3; ++i)
				{
					bounds.halfSize[j] += std::fabs(axis[i][j]) * geom.bounds.halfSize[i];
				}
			}

			for (int i = 0; i < 3; ++i)
			{
				for (const float sign : { 1.0f, -1.0f })
				{
					Game::cplane_s plane{};

					for (int j = 0; j < 3; ++j)
					{
						plane.normal[j] = axis[i][j] * sign;
						plane.dist += plane.normal[j] * geom.bounds.midPoint[j];
					}

					plane.dist += geom.bounds.halfSize[i];
					plane.type = 3;

					Game::cbrushside_t side{};
					side.materialNum = material;

					sidePlanes.push_back(planes.size());
					planes.push_back(plane);
					sides.push_back(side);
				}
			}

			brush.numsides = 6;
		}

		for (auto& axial : brush.axialMaterialNum)
		{
			std::ranges::fill(axial, material);
		}

		brushes.push_back(brush);
		brushBounds.push_back(bounds);
		brushContents.push_back(contentsSolid);
		appendedBrushes.push_back(appended);
	}

	static unsigned int AppendSubModel(const std::size_t firstBrush, const Game::Bounds& bounds, const float radius)
	{
		const auto firstLeafBrush = leafBrushes.size();

		for (auto i = firstBrush; i < brushes.size(); ++i)
		{
			leafBrushes.push_back(static_cast<std::uint16_t>(i));
		}

		cLeafBrushNode_s node{};
		node.leafBrushCount = static_cast<std::int16_t>(brushes.size() - firstBrush);
		node.contents = contentsSolid;
		nodeLeafBrushes.push_back(firstLeafBrush);
		leafbrushNodes.push_back(node);

		cmodel_t cmodel{};
		cmodel.bounds = bounds;
		cmodel.radius = radius;
		cmodel.leaf.brushContents = contentsSolid;
		cmodel.leaf.bounds = bounds;
		cmodel.leaf.leafBrushNode = static_cast<int>(leafbrushNodes.size() - 1);
		cmodels.push_back(cmodel);

		return static_cast<unsigned int>(cmodels.size() - 1);
	}

	static void AppendBoxes(const BoxSet& set, const std::uint16_t material)
	{
		const auto& boxHalfSizes = set.halfSizes;

		for (std::size_t i = 0; i < boxHalfSizes.size(); ++i)
		{
			for (std::size_t j = 0; j <= i; ++j)
			{
				for (const float height : set.heights)
				{
					Game::cbrush_t brush{};

					for (auto& axial : brush.axialMaterialNum)
					{
						std::ranges::fill(axial, material);
					}

					Game::Bounds bounds{};
					bounds.midPoint[2] = height * 0.5f;
					bounds.halfSize[0] = boxHalfSizes[i];
					bounds.halfSize[1] = boxHalfSizes[j];
					bounds.halfSize[2] = height * 0.5f;

					const auto firstBrush = brushes.size();
					brushes.push_back(brush);
					brushBounds.push_back(bounds);
					brushContents.push_back(contentsSolid);
					appendedBrushes.push_back({ sides.size(), adjacentSides.size(), false });

					AppendSubModel(firstBrush, bounds, RadiusOf(bounds));
				}
			}
		}
	}

	static std::size_t Nearest(const std::span<const float> values, const float value)
	{
		std::size_t nearest = 0;

		for (std::size_t i = 1; i < values.size(); ++i)
		{
			if (std::fabs(values[i] - value) < std::fabs(values[nearest] - value))
			{
				nearest = i;
			}
		}

		return nearest;
	}

	unsigned int CollmapCollision::FindBox(float halfLength, float halfWidth, const float height, bool* isTurned)
	{
		*isTurned = halfWidth > halfLength;

		if (*isTurned)
		{
			std::swap(halfLength, halfWidth);
		}

		if (!boxBase || !boxSet || !IsInjected())
		{
			return 0;
		}

		const auto i = Nearest(boxSet->halfSizes, halfLength);
		const auto j = std::min(Nearest(boxSet->halfSizes, halfWidth), i);
		const auto k = Nearest(boxSet->heights, height);

		return boxBase + static_cast<unsigned int>((i * (i + 1) / 2 + j) * boxSet->heights.size() + k);
	}

	bool CollmapCollision::IsBox(const unsigned int model)
	{
		return boxBase && boxSet && model >= boxBase && model < boxBase + boxSet->Count();
	}

	unsigned int CollmapCollision::ClientCollmapModel(const Game::centity_s* cent)
	{
		const auto* const collmap = FindForClient(cent);
		return collmap ? collmap->handle : 0;
	}

	bool CollmapCollision::HasLinkHook()
	{
		return linkEntity != nullptr;
	}

	bool CollmapCollision::IsInjected()
	{
		return !cmodels.empty() && Utils::Hook::Get<const cmodel_t*>(cm_cmodels) == cmodels.data();
	}

	void CollmapCollision::Inject()
	{
		if (IsInjected())
		{
			return;
		}

		boxBase = 0;
		boxSet = nullptr;
		collmaps.clear();
		cmodels.clear();
		leafbrushNodes.clear();
		brushes.clear();
		brushBounds.clear();
		brushContents.clear();
		sides.clear();
		sidePlanes.clear();
		planes.clear();
		adjacentSides.clear();
		leafBrushes.clear();
		nodeLeafBrushes.clear();
		appendedBrushes.clear();

		if (!linkEntity)
		{
			return;
		}

		const auto* const oldCmodels = Utils::Hook::Get<const cmodel_t*>(cm_cmodels);
		const auto numSubModels = Utils::Hook::Get<unsigned int>(cm_numSubModels);
		const auto numBrushes = Utils::Hook::Get<std::uint16_t>(cm_numBrushes);
		const auto numNodes = Utils::Hook::Get<unsigned int>(cm_leafbrushNodesCount);

		if (!oldCmodels || !numSubModels || !Utils::Hook::Get<unsigned int>(cm_numMaterials))
		{
			Logger::Print("collmapcollision: the clip map has no submodels or materials, no collmaps\n");
			return;
		}

		std::vector<const Game::XModel*> models;

		Game::DB_EnumXAssets_FastFile(Game::ASSET_TYPE_XMODEL, [](void* header, void* data)
		{
			const auto* const model = static_cast<const Game::XModel*>(header);

			if (model && model->physCollmap && model->physCollmap->count && model->physCollmap->geoms)
			{
				static_cast<std::vector<const Game::XModel*>*>(data)->push_back(model);
			}
		}, &models, false);

		if (models.empty())
		{
			Logger::Print("collmapcollision: no loaded model has a physics collmap\n");
		}

		const auto* const oldNodes = Utils::Hook::Get<const cLeafBrushNode_s*>(cm_leafbrushNodes);
		const auto* const oldBrushes = Utils::Hook::Get<const Game::cbrush_t*>(cm_brushes);
		const auto* const oldBounds = Utils::Hook::Get<const Game::Bounds*>(cm_brushBounds);
		const auto* const oldContents = Utils::Hook::Get<const int*>(cm_brushContents);

		cmodels.assign(oldCmodels, oldCmodels + numSubModels);
		leafbrushNodes.assign(oldNodes, oldNodes + numNodes);
		brushes.assign(oldBrushes, oldBrushes + numBrushes);
		brushBounds.assign(oldBounds, oldBounds + numBrushes);
		brushContents.assign(oldContents, oldContents + numBrushes);

		const auto material = FindSolidMaterial();

		for (const auto& set : boxSets)
		{
			if (cmodels.size() + set.Count() <= maxNetworkedSubModel + 1 && brushes.size() + set.Count() < std::numeric_limits<std::uint16_t>::max())
			{
				boxSet = &set;
				break;
			}
		}

		if (boxSet)
		{
			boxBase = static_cast<unsigned int>(cmodels.size());
			AppendBoxes(*boxSet, material);
		}
		else
		{
			Logger::Print("collmapcollision: the map has {} submodels, no room under {} for the collision boxes\n", cmodels.size(), maxNetworkedSubModel + 1);
		}

		std::unordered_map<const Game::PhysCollmap*, Collmap> handles;
		bool isFull = false;

		for (const auto* const model : models)
		{
			const auto* const physCollmap = model->physCollmap;

			if (const auto existing = handles.find(physCollmap); existing != handles.end())
			{
				collmaps.emplace(model, existing->second);
				Logger::Print("collmapcollision: {} shares collmap {}\n", model->name, physCollmap->name);
				continue;
			}

			if (brushes.size() + physCollmap->count >= std::numeric_limits<std::uint16_t>::max() || physCollmap->count > static_cast<unsigned int>(std::numeric_limits<std::int16_t>::max()))
			{
				isFull = true;
				continue;
			}

			const auto firstBrush = brushes.size();
			const auto firstLeafBrush = leafBrushes.size();
			Game::Bounds total{};
			bool isEmpty = true;
			float radius = 0.0f;

			for (unsigned int i = 0; i < physCollmap->count; ++i)
			{
				const auto& geom = physCollmap->geoms[i];

				if (geom.brushWrapper)
				{
					AppendBrushWrapper(*geom.brushWrapper, material);
				}
				else
				{
					AppendGeomBox(geom, material);
				}

				const auto& bounds = brushBounds.back();
				Include(total, bounds, isEmpty);
				radius = std::max(radius, RadiusOf(bounds));
				leafBrushes.push_back(static_cast<std::uint16_t>(brushes.size() - 1));
			}

			cLeafBrushNode_s node{};
			node.leafBrushCount = static_cast<std::int16_t>(brushes.size() - firstBrush);
			node.contents = contentsSolid;
			nodeLeafBrushes.push_back(firstLeafBrush);
			leafbrushNodes.push_back(node);

			cmodel_t cmodel{};
			cmodel.bounds = total;
			cmodel.radius = radius;
			cmodel.leaf.brushContents = contentsSolid;
			cmodel.leaf.bounds = total;
			cmodel.leaf.leafBrushNode = static_cast<int>(leafbrushNodes.size() - 1);
			cmodels.push_back(cmodel);

			if (radius > maxClientRadius)
			{
				Logger::Print("collmapcollision: {} reaches {} units from its origin, clients cull it past {}\n", physCollmap->name, radius, maxClientRadius);
			}

			const Collmap collmap{ static_cast<unsigned int>(cmodels.size() - 1), radius };
			handles.emplace(physCollmap, collmap);
			collmaps.emplace(model, collmap);

			Logger::Print("collmapcollision: {} uses collmap {}, {} geoms, radius {}\n", model->name, physCollmap->name, physCollmap->count, radius);
		}

		if (isFull)
		{
			Logger::Print("collmapcollision: the clip map has no room for every collmap, the rest collide as boxes\n");
		}

		if (collmaps.empty() && !boxBase)
		{
			cmodels.clear();
			return;
		}

		for (std::size_t i = 0; i < appendedBrushes.size(); ++i)
		{
			auto& brush = brushes[numBrushes + i];
			const auto& appended = appendedBrushes[i];

			brush.sides = brush.numsides ? &sides[appended.firstSide] : nullptr;
			brush.baseAdjacentSide = appended.hasAdjacentSides ? &adjacentSides[appended.firstAdjacentSide] : nullptr;
		}

		for (std::size_t i = 0; i < sides.size(); ++i)
		{
			sides[i].plane = &planes[sidePlanes[i]];
		}

		for (std::size_t i = 0; i < nodeLeafBrushes.size(); ++i)
		{
			leafbrushNodes[numNodes + i].brushes = &leafBrushes[nodeLeafBrushes[i]];
		}

		Utils::Hook::Set<const void*>(cm_cmodels, cmodels.data());
		Utils::Hook::Set<const void*>(cm_leafbrushNodes, leafbrushNodes.data());
		Utils::Hook::Set<const void*>(cm_brushes, brushes.data());
		Utils::Hook::Set<const void*>(cm_brushBounds, brushBounds.data());
		Utils::Hook::Set<const void*>(cm_brushContents, brushContents.data());
		Utils::Hook::Set<unsigned int>(cm_leafbrushNodesCount, static_cast<unsigned int>(leafbrushNodes.size()));
		Utils::Hook::Set<std::uint16_t>(cm_numBrushes, static_cast<std::uint16_t>(brushes.size()));
		Utils::Hook::Set<unsigned int>(cm_numSubModels, static_cast<unsigned int>(cmodels.size()));

		Logger::Print("collmapcollision: {} models collide with {} collmaps, {} collision boxes from submodel {} (set {})\n",
			collmaps.size(), handles.size(), boxSet ? boxSet->Count() : 0, boxBase, boxSet ? boxSet - boxSets : -1);
	}

	constexpr unsigned char cameraRegionLit = 0;

	constexpr float boundsMatchTolerance = 1.5f;

	struct Extent
	{
		float mins[3] = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		float maxs[3] = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
		bool hasVerts = false;

		void Add(const float* point)
		{
			for (int j = 0; j < 3; ++j)
			{
				mins[j] = std::min(mins[j], point[j]);
				maxs[j] = std::max(maxs[j], point[j]);
			}

			hasVerts = true;
		}

		Game::Bounds ToBounds() const
		{
			Game::Bounds bounds{};

			for (int j = 0; j < 3; ++j)
			{
				bounds.midPoint[j] = (mins[j] + maxs[j]) * 0.5f;
				bounds.halfSize[j] = (maxs[j] - mins[j]) * 0.5f;
			}

			return bounds;
		}

		bool Matches(const Game::Bounds& bounds) const
		{
			for (int j = 0; j < 3; ++j)
			{
				if (std::fabs(mins[j] - (bounds.midPoint[j] - bounds.halfSize[j])) > boundsMatchTolerance
					|| std::fabs(maxs[j] - (bounds.midPoint[j] + bounds.halfSize[j])) > boundsMatchTolerance)
				{
					return false;
				}
			}

			return true;
		}
	};

	static void BoneToModel(const Game::DObjAnimMat& bone, const float* point, float* out)
	{
		const float length = std::sqrt(bone.quat[0] * bone.quat[0] + bone.quat[1] * bone.quat[1] + bone.quat[2] * bone.quat[2] + bone.quat[3] * bone.quat[3]);
		const float scale = length > 0.0f ? 1.0f / length : 0.0f;
		const float q[3] = { bone.quat[0] * scale, bone.quat[1] * scale, bone.quat[2] * scale };
		const float w = bone.quat[3] * scale;

		const float inner[3] =
		{
			q[1] * point[2] - q[2] * point[1] + w * point[0],
			q[2] * point[0] - q[0] * point[2] + w * point[1],
			q[0] * point[1] - q[1] * point[0] + w * point[2],
		};

		out[0] = point[0] + 2.0f * (q[1] * inner[2] - q[2] * inner[1]) + bone.trans[0];
		out[1] = point[1] + 2.0f * (q[2] * inner[0] - q[0] * inner[2]) + bone.trans[1];
		out[2] = point[2] + 2.0f * (q[0] * inner[1] - q[1] * inner[0]) + bone.trans[2];
	}

	static Extent SurfaceExtent(const Game::XModel* model, const bool isLitOnly, const bool isBoneSpace)
	{
		Extent extent{};
		const auto& lod = model->lodInfo[0];
		const auto* const surfs = lod.surfs ? lod.surfs : (lod.modelSurfs ? lod.modelSurfs->surfs : nullptr);

		if (!surfs || !lod.numsurfs || !model->materialHandles)
		{
			return extent;
		}

		for (unsigned short i = 0; i < lod.numsurfs; ++i)
		{
			const auto* const material = model->materialHandles[lod.surfIndex + i];
			const auto& surf = surfs[i];

			if (!surf.verts0 || (isLitOnly && (!material || material->cameraRegion != cameraRegionLit)))
			{
				continue;
			}

			if (!isBoneSpace || !surf.vertListCount || !surf.vertList || !model->baseMat)
			{
				for (unsigned short v = 0; v < surf.vertCount; ++v)
				{
					extent.Add(surf.verts0[v].xyz);
				}

				continue;
			}

			unsigned int vertex = 0;

			for (unsigned int list = 0; list < surf.vertListCount; ++list)
			{
				const auto& vertList = surf.vertList[list];
				const auto bone = vertList.boneOffset / sizeof(Game::DObjSkelMat);

				for (unsigned short v = 0; v < vertList.vertCount && vertex < surf.vertCount; ++v, ++vertex)
				{
					float point[3]{};

					if (bone < model->numBones)
					{
						BoneToModel(model->baseMat[bone], surf.verts0[vertex].xyz, point);
					}
					else
					{
						std::memcpy(point, surf.verts0[vertex].xyz, sizeof(point));
					}

					extent.Add(point);
				}
			}
		}

		return extent;
	}

	static Game::Bounds SolidBounds(const Game::XModel* model)
	{
		static std::unordered_set<const Game::XModel*> logged;
		const bool isFirst = logged.insert(model).second;

		for (const bool isBoneSpace : { false, true })
		{
			if (!SurfaceExtent(model, false, isBoneSpace).Matches(model->bounds))
			{
				continue;
			}

			const auto lit = SurfaceExtent(model, true, isBoneSpace);

			if (!lit.hasVerts)
			{
				break;
			}

			const auto bounds = lit.ToBounds();

			if (isFirst)
			{
				Logger::Print("collmapcollision: {} solid {:.0f}x{:.0f}x{:.0f}, whole model {:.0f}x{:.0f}x{:.0f}{}\n", model->name,
					bounds.halfSize[0] * 2.0f, bounds.halfSize[1] * 2.0f, bounds.halfSize[2] * 2.0f,
					model->bounds.halfSize[0] * 2.0f, model->bounds.halfSize[1] * 2.0f, model->bounds.halfSize[2] * 2.0f, isBoneSpace ? ", by its bones" : "");
			}

			return bounds;
		}

		if (isFirst)
		{
			Logger::Print("collmapcollision: {} surfaces do not add up to its bounds (or are not loaded), using the whole model {:.0f}x{:.0f}x{:.0f}\n", model->name,
				model->bounds.halfSize[0] * 2.0f, model->bounds.halfSize[1] * 2.0f, model->bounds.halfSize[2] * 2.0f);
		}

		return model->bounds;
	}

	struct BoxOwner
	{
		int box;
		unsigned short model;
		float up;
		float down;
		bool isFitted;
	};

	static std::vector<int> boxOwnerNumbers;

	static float DownToFloor(const int owner, const float* origin, const float* angles, const Game::Bounds& bounds, const float down)
	{
		if (down <= 0.0f)
		{
			return down;
		}

		constexpr float toRadians = 3.14159265f / 180.0f;
		const float sp = std::sin(angles[0] * toRadians), cp = std::cos(angles[0] * toRadians);
		const float sy = std::sin(angles[1] * toRadians), cy = std::cos(angles[1] * toRadians);
		const float sr = std::sin(angles[2] * toRadians), cr = std::cos(angles[2] * toRadians);

		const float forward[3] = { cp * cy, cp * sy, -sp };
		const float right[3] = { -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
		const float up[3] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };

		const float local[3] = { bounds.midPoint[0], bounds.midPoint[1], bounds.midPoint[2] - bounds.halfSize[2] };
		const float reach = bounds.halfSize[2] * 2.0f * down;

		float start[3]{};

		for (int i = 0; i < 3; ++i)
		{
			start[i] = origin[i] + forward[i] * local[0] - right[i] * local[1] + up[i] * local[2];
		}

		start[2] += 1.0f;
		const float end[3] = { start[0], start[1], start[2] - 1.0f - reach };

		Game::trace_t trace{};
		trace.fraction = 1.0f;
		constexpr Game::Bounds point{};

		Utils::Hook::Call<void(Game::trace_t*, const float*, const float*, const Game::Bounds*, int, int)>(G_TraceCapsule)
			(&trace, start, end, &point, owner, contentsSolid);

		const float floor = std::max(0.0f, trace.fraction * (1.0f + reach) - 1.0f);
		return down * floor / reach;
	}

	static Game::Bounds Stretch(const Game::Bounds& bounds, const float up, const float down)
	{
		const float height = bounds.halfSize[2] * 2.0f;
		const float floor = bounds.midPoint[2] - bounds.halfSize[2];
		const float bottom = floor - height * down;
		const float top = floor + height * up;

		Game::Bounds stretched = bounds;
		stretched.midPoint[2] = (bottom + top) * 0.5f;
		stretched.halfSize[2] = (top - bottom) * 0.5f;
		return stretched;
	}

	static std::array<BoxOwner, Game::MAX_GENTITIES> boxOwners = []
	{
		std::array<BoxOwner, Game::MAX_GENTITIES> owners{};

		for (auto& owner : owners)
		{
			owner.box = -1;
		}

		return owners;
	}();

	static void FitBox(Game::gentity_s* box, Game::gentity_s* owner, const float* origin, const float* angles, const Game::Bounds& bounds)
	{
		bool isTurned = false;
		const auto index = CollmapCollision::FindBox(bounds.halfSize[0], bounds.halfSize[1], bounds.halfSize[2] * 2.0f, &isTurned);

		if (!index)
		{
			return;
		}

		const float local[3] = { bounds.midPoint[0], bounds.midPoint[1], bounds.midPoint[2] - bounds.halfSize[2] };

		constexpr float toRadians = 3.14159265f / 180.0f;
		const float sp = std::sin(angles[0] * toRadians), cp = std::cos(angles[0] * toRadians);
		const float sy = std::sin(angles[1] * toRadians), cy = std::cos(angles[1] * toRadians);
		const float sr = std::sin(angles[2] * toRadians), cr = std::cos(angles[2] * toRadians);

		const float forward[3] = { cp * cy, cp * sy, -sp };
		const float right[3] = { -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
		const float up[3] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };

		Utils::Hook::Call<void(Game::gentity_s*)>(G_EntUnlink)(box);
		Game::SV_UnlinkEntity(box);

		for (int i = 0; i < 3; ++i)
		{
			box->r.currentOrigin[i] = origin[i] + forward[i] * local[0] - right[i] * local[1] + up[i] * local[2];
			box->r.currentAngles[i] = angles[i];
		}

		if (isTurned)
		{
			const float turnedForward[3] = { -right[0], -right[1], -right[2] };
			const float turnedRight[3] = { forward[0], forward[1], forward[2] };
			constexpr float toDegrees = 180.0f / 3.14159265f;

			const float horizontal = std::sqrt(turnedForward[0] * turnedForward[0] + turnedForward[1] * turnedForward[1]);
			box->r.currentAngles[0] = std::atan2(-turnedForward[2], horizontal) * toDegrees;

			if (horizontal < 0.001f)
			{
				box->r.currentAngles[1] = std::atan2(turnedRight[0], -turnedRight[1]) * toDegrees;
				box->r.currentAngles[2] = 0.0f;
			}
			else
			{
				box->r.currentAngles[1] = std::atan2(turnedForward[1], turnedForward[0]) * toDegrees;
				box->r.currentAngles[2] = std::atan2(-turnedRight[2], up[2]) * toDegrees;
			}
		}

		box->s.index = static_cast<int>(index);
		box->r.modelType = modelTypeBrush;
		box->r.ownerNum = static_cast<unsigned short>(owner->s.number + 1);
		Game::SV_SetBrushModel(box);
		Game::SV_LinkEntity(box);

		Utils::Hook::Call<int(Game::gentity_s*, Game::gentity_s*, unsigned int)>(G_EntLinkTo)(box, owner, 0);
	}

	static bool IsOwnedBox(const Game::gentity_s* box, const int owner)
	{
		return box->r.isInUse && box->r.modelType == modelTypeBrush && CollmapCollision::IsBox(static_cast<unsigned int>(box->s.index))
			&& box->r.ownerNum == owner + 1;
	}

	constexpr float mapModelReach = 48.0f;
	constexpr float mapModelRise = 64.0f;
	constexpr float fallbackHalfWidth = 16.0f;
	constexpr float fallbackHeight = 72.0f;

	struct MapModel
	{
		Game::Bounds bounds;
		float origin[3];
		float angles[3];
	};

	static void AxisToAngles(const float (&axis)[3][3], float* angles)
	{
		constexpr float toDegrees = 180.0f / 3.14159265f;
		const float* const forward = axis[0];
		const float* const left = axis[1];
		const float* const up = axis[2];
		const float horizontal = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1]);

		angles[0] = std::atan2(-forward[2], horizontal) * toDegrees;

		if (horizontal < 0.001f)
		{
			angles[1] = std::atan2(-left[0], left[1]) * toDegrees;
			angles[2] = 0.0f;
			return;
		}

		angles[1] = std::atan2(forward[1], forward[0]) * toDegrees;
		angles[2] = std::atan2(left[2], up[2]) * toDegrees;
	}

	static std::optional<MapModel> FindMapModelAt(const Game::gentity_s* ent)
	{
		std::optional<MapModel> found;
		const char* foundName = nullptr;
		float nearest = mapModelReach;
		const float* const at = ent->r.currentOrigin;

		const auto consider = [&](const Game::XModel* model, const float* origin, const float (&axis)[3][3], const float scale)
		{
			const float dx = origin[0] - at[0];
			const float dy = origin[1] - at[1];
			const float distance = std::sqrt(dx * dx + dy * dy);

			if (!model || distance >= nearest || std::fabs(origin[2] - at[2]) > mapModelRise)
			{
				return;
			}

			nearest = distance;

			MapModel mapModel{};
			mapModel.bounds = SolidBounds(model);

			for (int i = 0; i < 3; ++i)
			{
				mapModel.bounds.midPoint[i] *= scale;
				mapModel.bounds.halfSize[i] *= scale;
				mapModel.origin[i] = origin[i];
			}

			AxisToAngles(axis, mapModel.angles);
			found = mapModel;
			foundName = model->name;
		};

		const auto count = Utils::Hook::Get<unsigned int>(cm_numStaticModels);
		const auto* const staticModels = Utils::Hook::Get<const cStaticModel_s*>(cm_staticModelList);

		for (unsigned int i = 0; staticModels && i < count; ++i)
		{
			const auto& staticModel = staticModels[i];

			const float* const row = staticModel.invScaledAxis[0];
			const float inverseScale = std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]);

			if (inverseScale < 0.0001f)
			{
				continue;
			}

			const float scale = 1.0f / inverseScale;
			float axis[3][3]{};

			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 3; ++c)
				{
					axis[r][c] = staticModel.invScaledAxis[c][r] * scale;
				}
			}

			consider(staticModel.xmodel, staticModel.origin, axis, scale);
		}

		for (int i = 0; i < Game::level->num_entities; ++i)
		{
			const auto* const other = &Game::g_entities[i];

			if (other == ent || !other->r.isInUse || other->r.modelType != modelTypeModel || !other->model
				|| (other->s.eType != Game::ET_GENERAL && other->s.eType != Game::ET_SCRIPTMOVER))
			{
				continue;
			}

			const float* const angles = other->r.currentAngles;
			constexpr float toRadians = 3.14159265f / 180.0f;
			const float sp = std::sin(angles[0] * toRadians), cp = std::cos(angles[0] * toRadians);
			const float sy = std::sin(angles[1] * toRadians), cy = std::cos(angles[1] * toRadians);
			const float sr = std::sin(angles[2] * toRadians), cr = std::cos(angles[2] * toRadians);

			const float axis[3][3] =
			{
				{ cp * cy, cp * sy, -sp },
				{ sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp },
				{ cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp },
			};

			consider(GetServerModel(other->model), other->r.currentOrigin, axis, 1.0f);
		}

		if (found)
		{
			Logger::Print("collmapcollision: entity {} has no model, its box fits the map's {} {:.0f} units away, turned {:.0f} {:.0f} {:.0f}\n",
				ent->s.number, foundName, nearest, found->angles[0], found->angles[1], found->angles[2]);
		}

		return found;
	}

	static void RefitBox(Game::gentity_s* owner)
	{
		auto& entry = boxOwners[owner->s.number];

		if (entry.box < 0 || !entry.isFitted || entry.model == owner->model)
		{
			return;
		}

		auto* const box = &Game::g_entities[entry.box];
		const auto* const model = owner->model ? GetServerModel(owner->model) : nullptr;

		if (!IsOwnedBox(box, owner->s.number) || !model)
		{
			entry.box = -1;
			return;
		}

		entry.model = owner->model;

		const auto bounds = SolidBounds(model);
		const float down = DownToFloor(owner->s.number, owner->r.currentOrigin, owner->r.currentAngles, bounds, entry.down);
		FitBox(box, owner, owner->r.currentOrigin, owner->r.currentAngles, Stretch(bounds, entry.up, down));
	}

	static void FreeOrphanedBoxes()
	{
		std::erase_if(boxOwnerNumbers, [](const int owner)
		{
			auto& entry = boxOwners[owner];

			if (entry.box < 0)
			{
				return true;
			}

			auto* const box = &Game::g_entities[entry.box];

			if (!IsOwnedBox(box, owner))
			{
				entry.box = -1;
				return true;
			}

			if (!Game::g_entities[owner].r.isInUse)
			{
				Game::G_FreeEntity(box);
				entry.box = -1;
				return true;
			}

			return false;
		});
	}

	void CollmapCollision::AddScriptFunctions()
	{
		GSC::Script::AddFunction("SpawnCollision", []
		{
			if (Game::Scr_GetNumParam() != 4)
			{
				GSC::Script::Scr_Error("usage: SpawnCollision( <model>, <targetname>, <origin>, <angles> )\n");
				return;
			}

			const auto* const modelName = Game::Scr_GetString(0);
			const auto targetname = Game::Scr_GetConstString(1);
			float origin[3]{};
			float angles[3]{};
			Game::Scr_GetVector(2, origin);
			Game::Scr_GetVector(3, angles);

			auto* const ent = Game::G_Spawn();
			Utils::Hook::Call<void(unsigned short*, unsigned int)>(Scr_SetString)(&ent->classname, Game::scr_const->script_model);
			Utils::Hook::Call<void(Game::gentity_s*, const char*)>(G_SetModel)(ent, modelName);

			if (!ent->model)
			{
				Game::G_FreeEntity(ent);
				GSC::Script::Scr_ParamError(0, Utils::String::VA("SpawnCollision: Collision model name %s is not valid.", modelName));
				return;
			}

			Utils::Hook::Call<void(unsigned short*, unsigned int)>(Scr_SetString)(&ent->targetname, targetname);
			std::memcpy(ent->r.currentOrigin, origin, sizeof(origin));
			std::memcpy(ent->r.currentAngles, angles, sizeof(angles));
			Utils::Hook::Call<int(Game::gentity_s*)>(G_CallSpawnEntity)(ent);

			const auto* const model = GetServerModel(ent->model);
			const int modelContents = model ? model->contents & collmapContents : 0;
			ent->r.contents |= modelContents ? modelContents : contentsSolid;
			Game::SV_LinkEntity(ent);

			if (!FindForServer(ent))
			{
				Logger::Warning("SpawnCollision: {} has no collmap, it collides as its box\n", modelName);
			}

			Game::Scr_AddEntity(ent);
		});

		GSC::Script::AddFunction("GetModelBounds", []
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				GSC::Script::Scr_Error("usage: GetModelBounds( <model> )\n");
				return;
			}

			const auto* const model = static_cast<const Game::XModel*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, Game::Scr_GetString(0)));

			if (!model)
			{
				GSC::Script::Scr_ParamError(0, "GetModelBounds: no such model");
				return;
			}

			const auto bounds = SolidBounds(model);
			float mins[3]{};
			float maxs[3]{};

			for (int i = 0; i < 3; ++i)
			{
				mins[i] = bounds.midPoint[i] - bounds.halfSize[i];
				maxs[i] = bounds.midPoint[i] + bounds.halfSize[i];
			}

			Game::Scr_MakeArray();
			Utils::Hook::Call<void(const float*)>(Scr_AddVector)(mins);
			Game::Scr_AddArray();
			Utils::Hook::Call<void(const float*)>(Scr_AddVector)(maxs);
			Game::Scr_AddArray();
		});

		GSC::Script::AddFunction("GetBoxCollisionIndex", []
		{
			if (Game::Scr_GetNumParam() != 3)
			{
				GSC::Script::Scr_Error("usage: GetBoxCollisionIndex( <length>, <width>, <height> )\n");
				return;
			}

			bool isTurned = false;
			Game::Scr_AddInt(static_cast<int>(FindBox(Game::Scr_GetFloat(0) * 0.5f, Game::Scr_GetFloat(1) * 0.5f, Game::Scr_GetFloat(2), &isTurned)));
		});

		GSC::Script::AddMethod("CreateBoxCollision", [](const Game::scr_entref_t entref)
		{
			const auto numParam = Game::Scr_GetNumParam();

			if (numParam != 0 && numParam != 2 && numParam != 3)
			{
				GSC::Script::Scr_Error("usage: <entity> CreateBoxCollision( [<up>, <down>] ) or <entity> CreateBoxCollision( <length>, <width>, <height> )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);
			Game::Bounds bounds{};
			const float* origin = ent->r.currentOrigin;
			const float* angles = ent->r.currentAngles;
			std::optional<MapModel> mapModel;
			const float up = numParam == 2 ? Game::Scr_GetFloat(0) : 1.0f;
			const float down = numParam == 2 ? Game::Scr_GetFloat(1) : 0.0f;

			if (up < 0.0f || down < 0.0f || up + down <= 0.0f)
			{
				GSC::Script::Scr_ParamError(0, "CreateBoxCollision: up and down cannot be below 0, nor both 0");
				return;
			}

			if (numParam == 3)
			{
				const float height = Game::Scr_GetFloat(2);
				bounds.halfSize[0] = Game::Scr_GetFloat(0) * 0.5f;
				bounds.halfSize[1] = Game::Scr_GetFloat(1) * 0.5f;
				bounds.midPoint[2] = height * 0.5f;
				bounds.halfSize[2] = height * 0.5f;
			}
			else
			{
				const auto* const model = ent->r.modelType == modelTypeModel && ent->model ? GetServerModel(ent->model) : nullptr;

				if (model)
				{
					const auto solid = SolidBounds(model);
					bounds = Stretch(solid, up, DownToFloor(ent->s.number, origin, angles, solid, down));
				}
				else if (mapModel = FindMapModelAt(ent); mapModel)
				{
					origin = mapModel->origin;
					angles = mapModel->angles;
					bounds = Stretch(mapModel->bounds, up, DownToFloor(ent->s.number, origin, angles, mapModel->bounds, down));
				}
				else
				{
					Logger::Warning("CreateBoxCollision: entity {} has no model and no map model stands within {} of it, giving it a {:.0f}x{:.0f}x{:.0f} box\n",
						ent->s.number, mapModelReach, fallbackHalfWidth * 2.0f, fallbackHalfWidth * 2.0f, fallbackHeight);

					bounds.halfSize[0] = fallbackHalfWidth;
					bounds.halfSize[1] = fallbackHalfWidth;
					bounds.midPoint[2] = fallbackHeight * 0.5f;
					bounds.halfSize[2] = fallbackHeight * 0.5f;
					bounds = Stretch(bounds, up, down);
				}
			}

			if (!boxBase)
			{
				GSC::Script::Scr_Error("CreateBoxCollision: the clip map has no collision boxes\n");
				return;
			}

			auto* const box = Game::G_Spawn();
			Utils::Hook::Call<void(unsigned short*, unsigned int)>(Scr_SetString)(&box->classname, Game::scr_const->script_model);
			std::memcpy(box->r.currentOrigin, ent->r.currentOrigin, sizeof(box->r.currentOrigin));
			std::memcpy(box->r.currentAngles, ent->r.currentAngles, sizeof(box->r.currentAngles));
			Utils::Hook::Call<int(Game::gentity_s*)>(G_CallSpawnEntity)(box);

			FitBox(box, ent, origin, angles, bounds);

			boxOwners[ent->s.number] = { box->s.number, ent->model, up, down, numParam != 3 && !mapModel };

			if (std::ranges::find(boxOwnerNumbers, ent->s.number) == boxOwnerNumbers.end())
			{
				boxOwnerNumbers.push_back(ent->s.number);
			}

			Game::Scr_AddEntity(box);
		});

		GSC::Script::AddMethod("SetCollisionOwner", [](const Game::scr_entref_t entref)
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				GSC::Script::Scr_Error("usage: <entity> SetCollisionOwner( <owner> )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);
			const auto* const owner = Utils::Hook::Call<Game::gentity_s*(unsigned int)>(Scr_GetEntity)(0);

			ent->r.ownerNum = static_cast<unsigned short>(owner->s.number + 1);
		});
	}

	CollmapCollision::CollmapCollision()
	{
		if (!Utils::Hook::MatchesBytes(SV_LinkEntity, linkEntityPrologue, sizeof(linkEntityPrologue)))
		{
			Logger::Error("collmapcollision: SV_LinkEntity does not read as expected, collmaps collide as boxes\n");
			return;
		}

		auto* const continuation = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, sizeof(linkEntityPrologue) + absoluteJumpSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));

		if (!continuation)
		{
			Logger::Error("collmapcollision: no memory for the SV_LinkEntity continuation, collmaps collide as boxes\n");
			return;
		}

		const std::uintptr_t resume = Utils::Hook::Rebase(SV_LinkEntity) + sizeof(linkEntityPrologue);
		std::memcpy(continuation, linkEntityPrologue, sizeof(linkEntityPrologue));

		auto* const jump = continuation + sizeof(linkEntityPrologue);
		jump[0] = 0xFF;
		jump[1] = 0x25;
		std::memset(&jump[2], 0, 4);
		std::memcpy(&jump[6], &resume, sizeof(resume));
		FlushInstructionCache(GetCurrentProcess(), continuation, sizeof(linkEntityPrologue) + absoluteJumpSize);

		linkEntity = reinterpret_cast<void(*)(Game::gentity_s*)>(continuation);

		if (!linkEntityHook.Initialize(SV_LinkEntity, reinterpret_cast<void*>(SV_LinkEntity_Hk), HOOK_JUMP)->Install()->IsInstalled())
		{
			VirtualFree(continuation, 0, MEM_RELEASE);
			linkEntity = nullptr;
			Logger::Error("collmapcollision: could not hook SV_LinkEntity, collmaps collide as boxes\n");
			return;
		}

		linkEntityHook.Quick();

		AddScriptFunctions();
		Scheduler::Loop(FreeOrphanedBoxes, Scheduler::Pipeline::SERVER);
	}
}
