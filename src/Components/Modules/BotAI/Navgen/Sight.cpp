#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Components::BotAI::Navgen
{
	static constexpr int cmName = 0;
	static constexpr int cmNumStaticModels = 24;
	static constexpr int cmStaticModelList = 32;
	static constexpr int cmNumMaterials = 40;
	static constexpr int cmMaterials = 48;
	static constexpr int cmAabbTreeCount = 240;
	static constexpr int cmAabbTrees = 248;
	static constexpr int cmNumBrushes = 272;
	static constexpr int cmBrushes = 280;
	static constexpr int clipMaterialStride = 16;
	static constexpr int clipMaterialContents = 12;
	static constexpr int brushStride = 48;
	static constexpr int brushSides = 8;
	static constexpr int brushAxialMaterials = 24;
	static constexpr int brushSideStride = 16;
	static constexpr int brushSideMaterial = 8;
	static constexpr int aabbTreeStride = 32;
	static constexpr int aabbTreeMaterial = 12;
	static constexpr int staticModelStride = 80;
	static constexpr int staticModelOrigin = 8;
	static constexpr int worldDpvsOffset = 672;
	static constexpr int dpvsSmodelCount = 0;
	static constexpr int dpvsSurfaceCount = 4;
	static constexpr int dpvsSurfaces = 120;
	static constexpr int dpvsSmodelDrawInsts = 136;
	static constexpr int surfaceStride = 32;
	static constexpr int surfaceFirstVertex = 4;
	static constexpr int surfaceTriCount = 10;
	static constexpr int surfaceBaseIndex = 12;
	static constexpr int surfaceMaterial = 16;
	static constexpr int drawVertexCount = 80;
	static constexpr int drawVertices = 88;
	static constexpr int drawIndexCount = 128;
	static constexpr int drawIndices = 136;
	static constexpr int worldVertexStride = 44;
	static constexpr int drawInstStride = 88;
	static constexpr int drawInstAxis = 12;
	static constexpr int drawInstScale = 48;
	static constexpr int drawInstModel = 56;
	static constexpr int materialTechniqueSet = 88;
	static constexpr int xmodelNumSurfs = 10;
	static constexpr int xmodelMaterials = 88;
	static constexpr int xmodelBounds = 356;
	static constexpr int modelMaterialsRead = 8;

	static constexpr float gridCellUnits = 256.0f;
	static constexpr float playableMarginXY = 512.0f;
	static constexpr float playableMarginBelow = 128.0f;
	static constexpr float playableMarginAbove = 512.0f;
	static constexpr float flatNormalZ = 0.9f;
	static constexpr float hugeHalfUnits = 512.0f;
	static constexpr float hugeTriangleUnits = 8192.0f;
	static constexpr float tinyHalfUnits = 8.0f;
	static constexpr float thinHalfUnits = 6.0f;
	static constexpr float sheetThickHalfUnits = 6.0f;
	static constexpr float sheetWideHalfUnits = 12.0f;
	static constexpr float softShrink = 0.85f;
	static constexpr float softDepthMin = 12.0f;
	static constexpr float hardInsideMin = 0.5f;
	static constexpr float grazingDot = -0.125f;
	static constexpr float grazingAdvanceScale = 8.0f;
	static constexpr float segmentEndSlack = 0.001f;
	static constexpr int gridCellCap = 262144;

	struct SightTriangle
	{
		float origin[3];
		float edgeA[3];
		float edgeB[3];
	};

	struct SightBox
	{
		float centre[3];
		float axis[3][3];
		float half[3];
		float softDepth;
		bool isSoft;
	};

	enum DrawMode
	{
		DrawSkip = 0,
		DrawOpaque,
		DrawCutout,
	};

	static std::vector<SightTriangle> sightTriangles;
	static std::vector<SightBox> sightBoxes;
	static std::vector<int> cellFirst;
	static std::vector<int> cellItems;
	static std::vector<int> itemStamps;
	static int queryStamp = 0;
	static float gridOrigin[2] = { 0.0f, 0.0f };
	static int gridColumns = 0;
	static int gridRows = 0;
	static bool isSightReady = false;
	static std::uintptr_t builtWorld = 0;
	static int builtGraphCount = -1;
	static char builtMap[64] = "";
	static char checkedMap[64] = "";
	static int renderBlockedCount = 0;

	static float Dot3(const float* a, const float* b)
	{
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	}

	static void Cross3(const float* a, const float* b, float* out)
	{
		out[0] = a[1] * b[2] - a[2] * b[1];
		out[1] = a[2] * b[0] - a[0] * b[2];
		out[2] = a[0] * b[1] - a[1] * b[0];
	}

	static bool Contains(const char* text, const char* needle)
	{
		return std::strstr(text, needle) != nullptr;
	}

	static const char* NameAt(const char* record)
	{
		if (!record)
		{
			return nullptr;
		}
		return *reinterpret_cast<const char* const*>(record);
	}

	static DrawMode DrawModeOf(const char* material)
	{
		if (!material)
		{
			return DrawSkip;
		}
		const char* techniqueSet = *reinterpret_cast<const char* const*>(material + materialTechniqueSet);
		const char* name = NameAt(techniqueSet);
		if (!name)
		{
			return DrawSkip;
		}
		if (*name == ',')
		{
			++name;
		}
		static const char* const skipped[] = { "sky", "water", "shadowcaster", "distort", "multiply", "_add",
												"blend", "_b0", "_a0", "decal", "effect" };
		for (const char* token : skipped)
		{
			if (Contains(name, token))
			{
				return DrawSkip;
			}
		}
		if (Contains(name, "_t0"))
		{
			return DrawCutout;
		}
		if (Contains(name, "_r0") || Contains(name, "replace"))
		{
			return DrawOpaque;
		}
		return DrawSkip;
	}

	static bool IsSeeThroughCutout(const char* name)
	{
		static const char* const tokens[] = { "wire", "cable", "fence", "chain", "grate", "mesh", "net" };
		for (const char* token : tokens)
		{
			if (Contains(name, token))
			{
				return true;
			}
		}
		return false;
	}

	static void CollectCollisionMaterials(std::unordered_set<std::string>& out)
	{
		const unsigned int materialCount = *reinterpret_cast<const unsigned int*>(cm + cmNumMaterials);
		const char* materials = *reinterpret_cast<const char* const*>(cm + cmMaterials);
		if (!materials || materialCount == 0)
		{
			return;
		}
		std::vector<unsigned char> isUsed(materialCount, 0);

		const unsigned short brushCount = *reinterpret_cast<const unsigned short*>(cm + cmNumBrushes);
		const char* brushes = *reinterpret_cast<const char* const*>(cm + cmBrushes);
		for (int b = 0; brushes && b < brushCount; ++b)
		{
			const char* brush = brushes + brushStride * b;
			const unsigned short* axial = reinterpret_cast<const unsigned short*>(brush + brushAxialMaterials);
			for (int k = 0; k < 6; ++k)
			{
				if (axial[k] < materialCount)
				{
					isUsed[axial[k]] = 1;
				}
			}
			const unsigned short sideCount = *reinterpret_cast<const unsigned short*>(brush);
			const char* sides = *reinterpret_cast<const char* const*>(brush + brushSides);
			for (int s = 0; sides && s < sideCount; ++s)
			{
				const unsigned short material = *reinterpret_cast<const unsigned short*>(
					sides + brushSideStride * s + brushSideMaterial);
				if (material < materialCount)
				{
					isUsed[material] = 1;
				}
			}
		}

		const unsigned int treeCount = *reinterpret_cast<const unsigned int*>(cm + cmAabbTreeCount);
		const char* trees = *reinterpret_cast<const char* const*>(cm + cmAabbTrees);
		for (unsigned int t = 0; trees && t < treeCount; ++t)
		{
			const unsigned short material = *reinterpret_cast<const unsigned short*>(
				trees + aabbTreeStride * t + aabbTreeMaterial);
			if (material < materialCount)
			{
				isUsed[material] = 1;
			}
		}

		for (unsigned int m = 0; m < materialCount; ++m)
		{
			const char* entry = materials + clipMaterialStride * m;
			const int contents = *reinterpret_cast<const int*>(entry + clipMaterialContents);
			const char* name = NameAt(entry);
			if (isUsed[m] && contents != 0 && name)
			{
				out.insert(name);
			}
		}
	}

	static bool IsInsideBox(const float* low, const float* high, const float* point, float slack)
	{
		for (int k = 0; k < 3; ++k)
		{
			if (point[k] < low[k] - slack || point[k] > high[k] + slack)
			{
				return false;
			}
		}
		return true;
	}

	static void CollectWorldTriangles(std::uintptr_t dpvs, std::uintptr_t draw,
									  const std::unordered_set<std::string>& collided,
									  const float* low, const float* high)
	{
		const unsigned int surfaceCount = *reinterpret_cast<const unsigned int*>(dpvs + dpvsSurfaceCount);
		const char* surfaces = *reinterpret_cast<const char* const*>(dpvs + dpvsSurfaces);
		const unsigned int vertexCount = *reinterpret_cast<const unsigned int*>(draw + drawVertexCount);
		const char* vertices = *reinterpret_cast<const char* const*>(draw + drawVertices);
		const unsigned int indexCount = *reinterpret_cast<const unsigned int*>(draw + drawIndexCount);
		const unsigned short* indices = *reinterpret_cast<const unsigned short* const*>(draw + drawIndices);
		if (!surfaces || !vertices || !indices)
		{
			return;
		}

		std::string key;
		for (unsigned int s = 0; s < surfaceCount; ++s)
		{
			const char* surface = surfaces + surfaceStride * s;
			const char* material = *reinterpret_cast<const char* const*>(surface + surfaceMaterial);
			const DrawMode mode = DrawModeOf(material);
			if (mode == DrawSkip)
			{
				continue;
			}
			const char* materialName = NameAt(material);
			if (!materialName)
			{
				continue;
			}
			if (std::strncmp(materialName, "wc/", 3) == 0)
			{
				materialName += 3;
			}
			key.assign(materialName);
			if (collided.count(key) != 0)
			{
				continue;
			}
			if (mode == DrawCutout && IsSeeThroughCutout(materialName))
			{
				continue;
			}

			const unsigned int firstVertex = *reinterpret_cast<const unsigned int*>(surface + surfaceFirstVertex);
			const unsigned int triCount = *reinterpret_cast<const unsigned short*>(surface + surfaceTriCount);
			const unsigned int baseIndex = *reinterpret_cast<const unsigned int*>(surface + surfaceBaseIndex);
			if (baseIndex + 3u * triCount > indexCount)
			{
				continue;
			}
			for (unsigned int t = 0; t < triCount; ++t)
			{
				const unsigned int corner[3] = {
					firstVertex + indices[baseIndex + 3u * t],
					firstVertex + indices[baseIndex + 3u * t + 1u],
					firstVertex + indices[baseIndex + 3u * t + 2u],
				};
				if (corner[0] >= vertexCount || corner[1] >= vertexCount || corner[2] >= vertexCount)
				{
					continue;
				}
				const float* a = reinterpret_cast<const float*>(vertices + worldVertexStride * corner[0]);
				const float* b = reinterpret_cast<const float*>(vertices + worldVertexStride * corner[1]);
				const float* c = reinterpret_cast<const float*>(vertices + worldVertexStride * corner[2]);
				SightTriangle triangle = {};
				for (int k = 0; k < 3; ++k)
				{
					triangle.origin[k] = a[k];
					triangle.edgeA[k] = b[k] - a[k];
					triangle.edgeB[k] = c[k] - a[k];
				}
				float normal[3];
				Cross3(triangle.edgeA, triangle.edgeB, normal);
				const float length = std::sqrt(Dot3(normal, normal));
				if (!(length > 1.0f) || std::fabs(normal[2]) / length > flatNormalZ)
				{
					continue;
				}
				bool isNear = true;
				for (int k = 0; k < 3; ++k)
				{
					const float triangleLow = std::min(a[k], std::min(b[k], c[k]));
					const float triangleHigh = std::max(a[k], std::max(b[k], c[k]));
					if (triangleHigh < low[k] || triangleLow > high[k] || triangleHigh - triangleLow > hugeTriangleUnits)
					{
						isNear = false;
					}
				}
				if (!isNear)
				{
					continue;
				}
				sightTriangles.push_back(triangle);
			}
		}
	}

	static bool HasCollisionTwin(const std::unordered_multimap<std::uintptr_t, const float*>& collision,
								 std::uintptr_t model, const float* origin)
	{
		const auto range = collision.equal_range(model);
		for (auto it = range.first; it != range.second; ++it)
		{
			const float* other = it->second;
			if (std::fabs(other[0] - origin[0]) < 1.0f && std::fabs(other[1] - origin[1]) < 1.0f
				&& std::fabs(other[2] - origin[2]) < 1.0f)
			{
				return true;
			}
		}
		return false;
	}

	static void CollectModelBoxes(std::uintptr_t dpvs, const float* low, const float* high)
	{
		std::unordered_multimap<std::uintptr_t, const float*> collision;
		const unsigned int collisionCount = *reinterpret_cast<const unsigned int*>(cm + cmNumStaticModels);
		const char* collisionList = *reinterpret_cast<const char* const*>(cm + cmStaticModelList);
		for (unsigned int i = 0; collisionList && i < collisionCount; ++i)
		{
			const char* entry = collisionList + staticModelStride * i;
			collision.emplace(*reinterpret_cast<const std::uintptr_t*>(entry),
							  reinterpret_cast<const float*>(entry + staticModelOrigin));
		}

		const unsigned int instCount = *reinterpret_cast<const unsigned int*>(dpvs + dpvsSmodelCount);
		const char* insts = *reinterpret_cast<const char* const*>(dpvs + dpvsSmodelDrawInsts);
		for (unsigned int i = 0; insts && i < instCount; ++i)
		{
			const char* inst = insts + drawInstStride * i;
			const char* model = *reinterpret_cast<const char* const*>(inst + drawInstModel);
			if (!model)
			{
				continue;
			}
			const float* origin = reinterpret_cast<const float*>(inst);
			if (HasCollisionTwin(collision, reinterpret_cast<std::uintptr_t>(model), origin))
			{
				continue;
			}
			const float* axis = reinterpret_cast<const float*>(inst + drawInstAxis);
			const float scale = *reinterpret_cast<const float*>(inst + drawInstScale);
			const float* bounds = reinterpret_cast<const float*>(model + xmodelBounds);
			float half[3];
			for (int k = 0; k < 3; ++k)
			{
				half[k] = std::fabs(bounds[3 + k]) * scale;
			}
			float ordered[3] = { half[0], half[1], half[2] };
			std::sort(ordered, ordered + 3);
			if (!(ordered[2] <= hugeHalfUnits) || ordered[2] < tinyHalfUnits || ordered[1] < thinHalfUnits)
			{
				continue;
			}

			bool hasOpaque = false;
			bool hasCutout = false;
			const int surfaceCount = *reinterpret_cast<const unsigned char*>(model + xmodelNumSurfs);
			const char* const* materials = *reinterpret_cast<const char* const* const*>(model + xmodelMaterials);
			for (int m = 0; materials && m < surfaceCount && m < modelMaterialsRead; ++m)
			{
				const DrawMode mode = DrawModeOf(materials[m]);
				if (mode == DrawOpaque)
				{
					hasOpaque = true;
				}
				else if (mode == DrawCutout)
				{
					hasCutout = true;
				}
			}
			const char* modelName = NameAt(model);
			const bool isFoliage = modelName && Contains(modelName, "foliage");
			const bool isSoft = isFoliage || (hasCutout && !hasOpaque);
			const bool isSheet = ordered[0] <= sheetThickHalfUnits && ordered[1] >= sheetWideHalfUnits;
			if (!isSoft && !(isSheet && (hasOpaque || hasCutout)))
			{
				continue;
			}

			SightBox box = {};
			for (int k = 0; k < 3; ++k)
			{
				box.centre[k] = origin[k] + scale * (bounds[0] * axis[k] + bounds[1] * axis[3 + k] + bounds[2] * axis[6 + k]);
			}
			if (!IsInsideBox(low, high, box.centre, ordered[2]))
			{
				continue;
			}
			for (int row = 0; row < 3; ++row)
			{
				for (int k = 0; k < 3; ++k)
				{
					box.axis[row][k] = axis[3 * row + k];
				}
				box.half[row] = half[row];
			}
			box.isSoft = isSoft;
			if (isSoft)
			{
				for (int k = 0; k < 3; ++k)
				{
					box.half[k] *= softShrink;
				}
				box.softDepth = std::min(box.half[0], box.half[1]);
				if (box.softDepth < softDepthMin)
				{
					box.softDepth = softDepthMin;
				}
			}
			sightBoxes.push_back(box);
		}
	}

	static void ItemBounds2D(int item, float* outLow, float* outHigh)
	{
		const int triangleCount = static_cast<int>(sightTriangles.size());
		if (item < triangleCount)
		{
			const SightTriangle& triangle = sightTriangles[item];
			for (int k = 0; k < 2; ++k)
			{
				const float b = triangle.origin[k] + triangle.edgeA[k];
				const float c = triangle.origin[k] + triangle.edgeB[k];
				outLow[k] = std::min(triangle.origin[k], std::min(b, c));
				outHigh[k] = std::max(triangle.origin[k], std::max(b, c));
			}
			return;
		}
		const SightBox& box = sightBoxes[item - triangleCount];
		for (int k = 0; k < 2; ++k)
		{
			const float reach = std::fabs(box.axis[0][k]) * box.half[0] + std::fabs(box.axis[1][k]) * box.half[1]
				+ std::fabs(box.axis[2][k]) * box.half[2];
			outLow[k] = box.centre[k] - reach;
			outHigh[k] = box.centre[k] + reach;
		}
	}

	static void BuildGrid()
	{
		const int itemCount = static_cast<int>(sightTriangles.size() + sightBoxes.size());
		cellFirst.clear();
		cellItems.clear();
		itemStamps.assign(itemCount, 0);
		queryStamp = 0;
		gridColumns = 0;
		gridRows = 0;
		if (itemCount == 0)
		{
			return;
		}

		float low[2] = { 1.0e30f, 1.0e30f };
		float high[2] = { -1.0e30f, -1.0e30f };
		for (int item = 0; item < itemCount; ++item)
		{
			float itemLow[2];
			float itemHigh[2];
			ItemBounds2D(item, itemLow, itemHigh);
			for (int k = 0; k < 2; ++k)
			{
				low[k] = std::min(low[k], itemLow[k]);
				high[k] = std::max(high[k], itemHigh[k]);
			}
		}
		gridOrigin[0] = low[0];
		gridOrigin[1] = low[1];
		gridColumns = static_cast<int>((high[0] - low[0]) / gridCellUnits) + 1;
		gridRows = static_cast<int>((high[1] - low[1]) / gridCellUnits) + 1;
		if (gridColumns * gridRows > gridCellCap)
		{
			sightTriangles.clear();
			sightBoxes.clear();
			itemStamps.clear();
			gridColumns = 0;
			gridRows = 0;
			return;
		}

		const int cellCount = gridColumns * gridRows;
		std::vector<int> counts(cellCount + 1, 0);
		for (int pass = 0; pass < 2; ++pass)
		{
			for (int item = 0; item < itemCount; ++item)
			{
				float itemLow[2];
				float itemHigh[2];
				ItemBounds2D(item, itemLow, itemHigh);
				const int columnLow = static_cast<int>((itemLow[0] - gridOrigin[0]) / gridCellUnits);
				const int columnHigh = static_cast<int>((itemHigh[0] - gridOrigin[0]) / gridCellUnits);
				const int rowLow = static_cast<int>((itemLow[1] - gridOrigin[1]) / gridCellUnits);
				const int rowHigh = static_cast<int>((itemHigh[1] - gridOrigin[1]) / gridCellUnits);
				for (int row = std::max(0, rowLow); row <= std::min(gridRows - 1, rowHigh); ++row)
				{
					for (int column = std::max(0, columnLow); column <= std::min(gridColumns - 1, columnHigh); ++column)
					{
						const int cell = row * gridColumns + column;
						if (pass == 0)
						{
							++counts[cell];
						}
						else
						{
							cellItems[cellFirst[cell] + counts[cell]] = item;
							++counts[cell];
						}
					}
				}
			}
			if (pass == 0)
			{
				cellFirst.assign(cellCount + 1, 0);
				for (int cell = 0; cell < cellCount; ++cell)
				{
					cellFirst[cell + 1] = cellFirst[cell] + counts[cell];
				}
				cellItems.assign(cellFirst[cellCount], 0);
				std::fill(counts.begin(), counts.end(), 0);
			}
		}
	}

	static bool PlayableBounds(float* outLow, float* outHigh)
	{
		const int count = Waypoints::Count();
		if (count <= 0)
		{
			return false;
		}
		for (int k = 0; k < 3; ++k)
		{
			outLow[k] = 1.0e30f;
			outHigh[k] = -1.0e30f;
		}
		for (int n = 0; n < count; ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				continue;
			}
			const float* origin = Waypoints::Origin(n);
			for (int k = 0; k < 3; ++k)
			{
				outLow[k] = std::min(outLow[k], origin[k]);
				outHigh[k] = std::max(outHigh[k], origin[k]);
			}
		}
		if (outLow[0] > outHigh[0])
		{
			return false;
		}
		outLow[0] -= playableMarginXY;
		outLow[1] -= playableMarginXY;
		outHigh[0] += playableMarginXY;
		outHigh[1] += playableMarginXY;
		outLow[2] -= playableMarginBelow;
		outHigh[2] += playableMarginAbove;
		return true;
	}

	static bool TryGetWorld(const char* mapName, std::uintptr_t* outDpvs, std::uintptr_t* outDraw)
	{
		if (!g_worldDpvs || !g_worldDraw || !cm || !mapName || !*mapName)
		{
			return false;
		}
		const std::uintptr_t dpvs = *reinterpret_cast<const std::uintptr_t*>(g_worldDpvs);
		const std::uintptr_t draw = *reinterpret_cast<const std::uintptr_t*>(g_worldDraw);
		if (!dpvs || !draw)
		{
			return false;
		}
		const char* worldName = NameAt(reinterpret_cast<const char*>(dpvs - worldDpvsOffset));
		const char* clipName = *reinterpret_cast<const char* const*>(cm + cmName);
		if (!worldName || !clipName)
		{
			return false;
		}
		char expected[96];
		_snprintf_s(expected, sizeof(expected), _TRUNCATE, "maps/mp/%s.d3dbsp", mapName);
		if (_stricmp(worldName, expected) != 0 || _stricmp(clipName, expected) != 0)
		{
			return false;
		}
		*outDpvs = dpvs;
		*outDraw = draw;
		return true;
	}

	void RefreshSightBlockers(const char* mapName)
	{
		if (!mapName || !*mapName)
		{
			return;
		}
		if (std::strcmp(mapName, checkedMap) != 0)
		{
			_snprintf_s(checkedMap, sizeof(checkedMap), _TRUNCATE, "%s", mapName);
			isSightReady = false;
			sightTriangles.clear();
			sightBoxes.clear();
			cellFirst.clear();
			cellItems.clear();
			itemStamps.clear();
			builtWorld = 0;
			builtGraphCount = -1;
			builtMap[0] = 0;
		}

		std::uintptr_t dpvs = 0;
		std::uintptr_t draw = 0;
		if (!TryGetWorld(mapName, &dpvs, &draw))
		{
			return;
		}
		const int graphCount = Waypoints::Count();
		if (isSightReady && builtWorld == dpvs && builtGraphCount == graphCount)
		{
			return;
		}
		float low[3];
		float high[3];
		if (!PlayableBounds(low, high))
		{
			return;
		}

		isSightReady = false;
		sightTriangles.clear();
		sightBoxes.clear();
		std::unordered_set<std::string> collided;
		CollectCollisionMaterials(collided);
		CollectWorldTriangles(dpvs, draw, collided, low, high);
		CollectModelBoxes(dpvs, low, high);
		BuildGrid();

		builtWorld = dpvs;
		builtGraphCount = graphCount;
		_snprintf_s(builtMap, sizeof(builtMap), _TRUNCATE, "%s", mapName);
		renderBlockedCount = 0;
		int softCount = 0;
		for (const SightBox& box : sightBoxes)
		{
			if (box.isSoft)
			{
				++softCount;
			}
		}
		isSightReady = true;
		Report("navgen: sight blockers for %s: %d drawn triangle(s) with no collision, %d foliage box(es), %d sheet box(es), grid %dx%d\n",
			   mapName, static_cast<int>(sightTriangles.size()), softCount,
			   static_cast<int>(sightBoxes.size()) - softCount, gridColumns, gridRows);
	}

	static float TriangleBlockAt(const float* from, const float* delta, const SightTriangle& triangle)
	{
		float p[3];
		Cross3(delta, triangle.edgeB, p);
		const float determinant = Dot3(triangle.edgeA, p);
		if (std::fabs(determinant) < 1.0e-8f)
		{
			return -1.0f;
		}
		const float inverse = 1.0f / determinant;
		const float offset[3] = { from[0] - triangle.origin[0], from[1] - triangle.origin[1], from[2] - triangle.origin[2] };
		const float u = Dot3(offset, p) * inverse;
		if (u < 0.0f || u > 1.0f)
		{
			return -1.0f;
		}
		float q[3];
		Cross3(offset, triangle.edgeA, q);
		const float v = Dot3(delta, q) * inverse;
		if (v < 0.0f || u + v > 1.0f)
		{
			return -1.0f;
		}
		const float t = Dot3(triangle.edgeB, q) * inverse;
		if (t > segmentEndSlack && t < 1.0f - segmentEndSlack)
		{
			return t;
		}
		return -1.0f;
	}

	static float BoxBlockAt(const float* from, const float* delta, float length, const SightBox& box)
	{
		const float offset[3] = { from[0] - box.centre[0], from[1] - box.centre[1], from[2] - box.centre[2] };
		float enter = 0.0f;
		float leave = 1.0f;
		for (int k = 0; k < 3; ++k)
		{
			const float start = Dot3(offset, box.axis[k]);
			const float step = Dot3(delta, box.axis[k]);
			if (std::fabs(step) < 1.0e-8f)
			{
				if (start < -box.half[k] || start > box.half[k])
				{
					return -1.0f;
				}
				continue;
			}
			float t0 = (-box.half[k] - start) / step;
			float t1 = (box.half[k] - start) / step;
			if (t0 > t1)
			{
				std::swap(t0, t1);
			}
			enter = std::max(enter, t0);
			leave = std::min(leave, t1);
			if (enter >= leave)
			{
				return -1.0f;
			}
		}
		const float inside = (leave - enter) * length;
		if (box.isSoft)
		{
			if (inside >= box.softDepth)
			{
				return enter + box.softDepth / length;
			}
			return -1.0f;
		}
		if (inside >= hardInsideMin)
		{
			return enter;
		}
		return -1.0f;
	}

	static float ItemBlockAt(int item, const float* from, const float* delta, float length)
	{
		const int triangleCount = static_cast<int>(sightTriangles.size());
		if (item < triangleCount)
		{
			return TriangleBlockAt(from, delta, sightTriangles[item]);
		}
		return BoxBlockAt(from, delta, length, sightBoxes[item - triangleCount]);
	}

	static float WalkBlockers(const float* from, const float* to, bool isAnyEnough)
	{
		const float noBlock = 2.0f;
		if (!isSightReady || gridColumns == 0 || cellFirst.empty())
		{
			return noBlock;
		}
		const float delta[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
		const float length = std::sqrt(Dot3(delta, delta));
		if (length < 1.0f)
		{
			return noBlock;
		}

		if (queryStamp == INT_MAX)
		{
			std::fill(itemStamps.begin(), itemStamps.end(), 0);
			queryStamp = 0;
		}
		++queryStamp;

		const float gridHigh[2] = { gridOrigin[0] + gridColumns * gridCellUnits, gridOrigin[1] + gridRows * gridCellUnits };
		float enter = 0.0f;
		float leave = 1.0f;
		for (int k = 0; k < 2; ++k)
		{
			if (std::fabs(delta[k]) < 1.0e-6f)
			{
				if (from[k] < gridOrigin[k] || from[k] > gridHigh[k])
				{
					return noBlock;
				}
				continue;
			}
			float t0 = (gridOrigin[k] - from[k]) / delta[k];
			float t1 = (gridHigh[k] - from[k]) / delta[k];
			if (t0 > t1)
			{
				std::swap(t0, t1);
			}
			enter = std::max(enter, t0);
			leave = std::min(leave, t1);
		}
		if (enter > leave)
		{
			return noBlock;
		}

		const float startX = from[0] + delta[0] * enter;
		const float startY = from[1] + delta[1] * enter;
		int column = static_cast<int>((startX - gridOrigin[0]) / gridCellUnits);
		int row = static_cast<int>((startY - gridOrigin[1]) / gridCellUnits);
		column = std::max(0, std::min(gridColumns - 1, column));
		row = std::max(0, std::min(gridRows - 1, row));

		int stepColumn = 0;
		int stepRow = 0;
		float nextColumnT = 1.0e30f;
		float nextRowT = 1.0e30f;
		float columnT = 1.0e30f;
		float rowT = 1.0e30f;
		if (delta[0] > 1.0e-6f)
		{
			stepColumn = 1;
			columnT = gridCellUnits / delta[0];
			nextColumnT = (gridOrigin[0] + (column + 1) * gridCellUnits - from[0]) / delta[0];
		}
		else if (delta[0] < -1.0e-6f)
		{
			stepColumn = -1;
			columnT = -gridCellUnits / delta[0];
			nextColumnT = (gridOrigin[0] + column * gridCellUnits - from[0]) / delta[0];
		}
		if (delta[1] > 1.0e-6f)
		{
			stepRow = 1;
			rowT = gridCellUnits / delta[1];
			nextRowT = (gridOrigin[1] + (row + 1) * gridCellUnits - from[1]) / delta[1];
		}
		else if (delta[1] < -1.0e-6f)
		{
			stepRow = -1;
			rowT = -gridCellUnits / delta[1];
			nextRowT = (gridOrigin[1] + row * gridCellUnits - from[1]) / delta[1];
		}

		float nearest = noBlock;
		float cellEnterT = enter;
		const int maxSteps = gridColumns + gridRows + 2;
		for (int steps = 0; steps < maxSteps; ++steps)
		{
			if (cellEnterT > nearest)
			{
				break;
			}
			const int cell = row * gridColumns + column;
			for (int slot = cellFirst[cell]; slot < cellFirst[cell + 1]; ++slot)
			{
				const int item = cellItems[slot];
				if (itemStamps[item] == queryStamp)
				{
					continue;
				}
				itemStamps[item] = queryStamp;
				const float blockAt = ItemBlockAt(item, from, delta, length);
				if (blockAt < 0.0f || blockAt >= nearest)
				{
					continue;
				}
				nearest = blockAt;
				if (isAnyEnough)
				{
					return nearest;
				}
			}
			if (nextColumnT > leave && nextRowT > leave)
			{
				break;
			}
			if (nextColumnT < nextRowT)
			{
				cellEnterT = nextColumnT;
				column += stepColumn;
				nextColumnT += columnT;
			}
			else
			{
				cellEnterT = nextRowT;
				row += stepRow;
				nextRowT += rowT;
			}
			if (column < 0 || column >= gridColumns || row < 0 || row >= gridRows)
			{
				break;
			}
		}
		return nearest;
	}

	bool IsRenderBlocked(const float* from, const float* to)
	{
		if (WalkBlockers(from, to, true) <= 1.0f)
		{
			++renderBlockedCount;
			return true;
		}
		return false;
	}

	float RenderBlockedFraction(const float* from, const float* to)
	{
		const float blockAt = WalkBlockers(from, to, false);
		if (blockAt > 1.0f)
		{
			return 1.0f;
		}
		return blockAt;
	}

	int SightTriangleCount()
	{
		if (!isSightReady)
		{
			return 0;
		}
		return static_cast<int>(sightTriangles.size());
	}

	bool SightTriangleAt(int index, float outCorners[3][3])
	{
		if (!isSightReady || index < 0 || index >= static_cast<int>(sightTriangles.size()))
		{
			return false;
		}
		const SightTriangle& triangle = sightTriangles[index];
		for (int k = 0; k < 3; ++k)
		{
			outCorners[0][k] = triangle.origin[k];
			outCorners[1][k] = triangle.origin[k] + triangle.edgeA[k];
			outCorners[2][k] = triangle.origin[k] + triangle.edgeB[k];
		}
		return true;
	}

	int SightBoxCount()
	{
		if (!isSightReady)
		{
			return 0;
		}
		return static_cast<int>(sightBoxes.size());
	}

	bool SightBoxAt(int index, float outCorners[8][3], bool* outIsSoft)
	{
		if (!isSightReady || index < 0 || index >= static_cast<int>(sightBoxes.size()))
		{
			return false;
		}
		const SightBox& box = sightBoxes[index];
		for (int corner = 0; corner < 8; ++corner)
		{
			float signs[3] = { -1.0f, -1.0f, -1.0f };
			for (int k = 0; k < 3; ++k)
			{
				if (corner & (1 << k))
				{
					signs[k] = 1.0f;
				}
			}
			for (int k = 0; k < 3; ++k)
			{
				outCorners[corner][k] = box.centre[k] + box.axis[0][k] * box.half[0] * signs[0]
					+ box.axis[1][k] * box.half[1] * signs[1] + box.axis[2][k] * box.half[2] * signs[2];
			}
		}
		*outIsSoft = box.isSoft;
		return true;
	}

	int RenderBlockedCount()
	{
		return renderBlockedCount;
	}


	struct ShotHit
	{
		bool isHit;
		bool isStartSolid;
		bool isAllSolid;
		float position[3];
		float normal[3];
		int surfaceType;
	};

	static ShotHit TraceShot(const float* start, const float* end)
	{
		unsigned char trace[128] = {};
		const int ignore[4] = { -1, -1, 0, 0 };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		SV_Trace(trace, start, end, bounds, ignore, maskWorldOnly, 0, nullptr, 1);

		ShotHit hit = {};
		const float fraction = *reinterpret_cast<const float*>(trace + traceFraction);
		const float* normal = reinterpret_cast<const float*>(trace + traceNormal);
		hit.isHit = fraction < 1.0f;
		hit.isStartSolid = trace[traceStartSolid] != 0;
		hit.isAllSolid = trace[traceAllSolid] != 0;
		for (int k = 0; k < 3; ++k)
		{
			hit.position[k] = start[k] + (end[k] - start[k]) * fraction;
			hit.normal[k] = normal[k];
		}
		const int surfaceFlags = *reinterpret_cast<const int*>(trace + traceSurfaceFlags);
		hit.surfaceType = (surfaceFlags >> surfaceTypeShift) & surfaceTypeBits;
		if (surfaceFlags & surfNoPenetrate)
		{
			hit.surfaceType = 0;
		}
		return hit;
	}

	static float DepthFor(int penetrateType, int surfaceType)
	{
		if (!penetrationDepthTable || penetrateType <= 0 || penetrateType >= penetrateTypeCount
			|| surfaceType <= 0 || surfaceType >= surfaceTypeCount)
		{
			return 0.0f;
		}
		const float* table = reinterpret_cast<const float*>(penetrationDepthTable);
		return table[surfaceTypeCount * penetrateType + surfaceType];
	}

	static bool IsPenetrationEnabled()
	{
		if (!bullet_penetration_enabled)
		{
			return false;
		}
		const char* dvar = *reinterpret_cast<const char* const*>(bullet_penetration_enabled);
		if (!dvar)
		{
			return false;
		}
		return *reinterpret_cast<const bool*>(dvar + dvarCurrentValue);
	}

	static float Distance3Of(const float* a, const float* b)
	{
		const float dx = b[0] - a[0];
		const float dy = b[1] - a[1];
		const float dz = b[2] - a[2];
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	float PenetrationShare(const float* from, const float* to, int penetrateType, float depthScale)
	{
		ShotHit hit = TraceShot(from, to);
		if (hit.isStartSolid)
		{
			return 0.0f;
		}
		if (!hit.isHit)
		{
			return 1.0f;
		}
		if (penetrateType <= 0 || depthScale <= 0.0f || !IsPenetrationEnabled())
		{
			return 0.0f;
		}
		const float length = Distance3Of(from, to);
		if (length < 1.0f)
		{
			return 1.0f;
		}
		const float direction[3] = { (to[0] - from[0]) / length, (to[1] - from[1]) / length, (to[2] - from[2]) / length };

		float power = 1.0f;
		for (int surface = 0; surface < maxBulletPenetrations; ++surface)
		{
			const float entryDepth = DepthFor(penetrateType, hit.surfaceType) * depthScale;
			if (entryDepth <= 0.0f)
			{
				return 0.0f;
			}
			const float entryDot = Dot3(direction, hit.normal);
			if (entryDot > grazingDot)
			{
				return 0.0f;
			}
			const float advance = bulletAdvanceThroughSurface / -entryDot;
			const float entry[3] = { hit.position[0], hit.position[1], hit.position[2] };
			const float next[3] = { entry[0] + direction[0] * advance, entry[1] + direction[1] * advance,
									entry[2] + direction[2] * advance };
			const ShotHit forward = TraceShot(next, to);

			float backStart[3] = { to[0], to[1], to[2] };
			if (forward.isHit)
			{
				const float backDot = Dot3(direction, forward.normal);
				float backAdvance = bulletAdvancePastExit * grazingAdvanceScale;
				if (backDot <= grazingDot)
				{
					backAdvance = bulletAdvancePastExit / -backDot;
				}
				for (int k = 0; k < 3; ++k)
				{
					backStart[k] = forward.position[k] - direction[k] * backAdvance;
				}
			}
			const float backEnd[3] = { entry[0] - direction[0] * bulletAdvancePastExit,
									   entry[1] - direction[1] * bulletAdvancePastExit,
									   entry[2] - direction[2] * bulletAdvancePastExit };
			const ShotHit back = TraceShot(backStart, backEnd);

			const bool isUnresolved = (back.isHit && back.isAllSolid) || (forward.isStartSolid && back.isStartSolid);
			if (isUnresolved || back.isHit)
			{
				float thickness = 0.0f;
				float depth = entryDepth;
				if (isUnresolved)
				{
					thickness = Distance3Of(backStart, backEnd);
				}
				else
				{
					thickness = Distance3Of(entry, back.position);
					const float exitDepth = DepthFor(penetrateType, back.surfaceType) * depthScale;
					if (exitDepth < depth)
					{
						depth = exitDepth;
					}
				}
				if (depth <= 0.0f)
				{
					return 0.0f;
				}
				if (thickness < 1.0f)
				{
					thickness = 1.0f;
				}
				power -= thickness / depth;
				if (power <= 0.0f)
				{
					return 0.0f;
				}
			}
			if (!forward.isHit)
			{
				return power;
			}
			hit = forward;
		}
		return 0.0f;
	}
}
