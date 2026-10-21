#include "STDInclude.hpp"

#include "FixedFunction.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Iw4.hpp"
#include "Api.hpp"
#include "Camo.hpp"
#include "PatchCalls.hpp"
#include "PbrMaterials.hpp"
#include "Read.hpp"
#include "Scene.hpp"
#include "Sky.hpp"
#include "TextureTags.hpp"

namespace Components::Remix
{
	using TessBspSurfaces = void(__fastcall*)(void*, void**);
	using GroupDrawer = void(*)(IDirect3DDevice9*, void*, void**);
	using ChangeDepthRange = void(__fastcall*)(void*, int);

	struct WorldVertex
	{
		float position[3];
		float normal[3];
		D3DCOLOR colour;
		float texCoord[2];
	};

	static_assert(sizeof(WorldVertex) == 36);

	static constexpr DWORD worldVertexFormat = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

	struct WorldSource
	{
		const unsigned char* vertices = nullptr;
		const std::uint16_t* indices = nullptr;
		std::uint32_t vertexCount = 0;
		std::uint32_t indexCount = 0;
	};

	static TessBspSurfaces originalTess = nullptr;
	static TessBspSurfaces originalTessBModel = nullptr;
	static TessBspSurfaces originalTessRigid = nullptr;
	static TessBspSurfaces originalTessRigidSkinned = nullptr;
	static TessBspSurfaces originalTessSkinned = nullptr;
	static TessBspSurfaces originalStaticModelRigid = nullptr;
	static TessBspSurfaces originalStaticModelDeformed = nullptr;

	struct ModelBuffers
	{
		IDirect3DVertexBuffer9* vertexBuffer = nullptr;
		IDirect3DIndexBuffer9* indexBuffer = nullptr;
		std::uint16_t vertexCount = 0;
		std::uint16_t triCount = 0;
	};

	static std::unordered_map<std::uintptr_t, ModelBuffers> modelBuffers;
	static ChangeDepthRange changeDepthRange = nullptr;

	static IDirect3DDevice9* sceneDevice = nullptr;
	static IDirect3DVertexBuffer9* worldVertexBuffer = nullptr;
	static IDirect3DIndexBuffer9* worldIndexBuffer = nullptr;
	static IDirect3DIndexBuffer9* blendedIndexBuffer = nullptr;

	struct BatchDrawState
	{
		bool isKnown = false;
		bool isOpaque = false;
		bool isScrolling = false;
		DWORD isAlphaTested = FALSE;
		DWORD alphaReference = 0;
		DWORD alphaFunction = D3DCMP_ALWAYS;
		DWORD cullMode = D3DCULL_CCW;
		DWORD isDepthTested = TRUE;
		DWORD isDepthWritten = TRUE;
		DWORD depthFunction = D3DCMP_LESSEQUAL;
		DWORD colourOperation = D3DTOP_MODULATE;
		DWORD colourArgument1 = D3DTA_TEXTURE;
		DWORD colourArgument2 = D3DTA_CURRENT;
		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;
		DWORD addressU = D3DTADDRESS_WRAP;
		DWORD addressV = D3DTADDRESS_WRAP;
		float minZ = 0.0f;
		float maxZ = 1.0f;
	};

	struct MaterialBatch
	{
		UINT startIndex = 0;
		UINT triCount = 0;
		UINT firstVertex = 0;
		UINT vertexCount = 0;
		unsigned drawnScene = 0;
		BatchDrawState drawState;
	};

	static IDirect3DStateBlock9* completeSceneState = nullptr;
	static unsigned culledFillScene = 0;

	struct BatchSurface
	{
		std::uint32_t firstVertex;
		std::uint32_t baseIndex;
		std::uint16_t triCount;
	};

	static constexpr std::uint64_t maxBatchIndices = 64ull * 1024ull * 1024ull;
	static std::unordered_map<std::uintptr_t, MaterialBatch> materialBatches;
	static std::unordered_map<std::uintptr_t, std::vector<FixtureSpot>> surfaceSpots;
	static std::unordered_map<std::uintptr_t, std::vector<FixtureSpot>> propSpots;
	static unsigned surfaceSpotsVersion = 0;
	static constexpr float fixtureLift = 6.0f;
	static constexpr float fixtureMergeDistance = 64.0f;
	static constexpr std::size_t maxFixtureSpots = 384;

	struct StaticSurfaceRecord
	{
		std::uintptr_t material;
		std::uintptr_t surface;
		std::uint32_t instance;
		std::uint16_t vertexCount;
		std::uint16_t triCount;
	};

	static constexpr unsigned maxBakedModelVertices = 1000;
	static std::vector<StaticSurfaceRecord> bigPropRecords;
	static std::vector<unsigned> bigPropDrawnScene;
	static constexpr std::uint32_t opaqueBlendBits = 0x012;
	static constexpr std::uint32_t alphaTestOffBit = 0x800;
	static constexpr std::uint32_t alphaTestMask = 0x3000;
	static constexpr std::uint32_t alphaTestAboveZero = 0x1000;
	static constexpr std::uint32_t alphaTestBelowHalf = 0x2000;
	static constexpr float worldMinZ = 0.015625f;
	static constexpr unsigned maxSurfacesPerModel = 16;
	static constexpr std::uint64_t maxStaticBatchVertices = 4ull * 1024ull * 1024ull;
	static std::unordered_map<std::uintptr_t, MaterialBatch> staticModelBatches;
	static IDirect3DVertexBuffer9* staticBatchVertexBuffer = nullptr;
	static IDirect3DIndexBuffer9* staticBatchIndexBuffer = nullptr;
	static std::vector<std::uint8_t> isStaticModelBaked;
	static std::uintptr_t staticBatchDrawInsts = 0;
	static std::uint32_t staticBatchModelCount = 0;
	static std::vector<std::uint32_t> instanceBatchStart;
	static std::vector<std::uint32_t> instanceBatchIds;
	static std::vector<std::uint32_t> batchNearestInstance;
	static std::vector<float> batchNearestDistance;
	static std::vector<std::uint8_t> isKeptInstance;
	static std::vector<std::pair<std::uint32_t, std::uint8_t>> hiddenVisibility;

	using AddStaticModelSurfaces = void(__fastcall*)(void*);
	static AddStaticModelSurfaces originalAddStaticModelSurfaces = nullptr;
	static IDirect3DVertexBuffer9* batchVertexBuffer = nullptr;
	static std::uint32_t alienColourSeed = 0x9E3779B9u;
	static bool isAlienGroup = false;

	static std::uint32_t MixBits(std::uint32_t value);
	static D3DCOLOR AlienColour(std::uint32_t seed, const float* position);
	static IDirect3DIndexBuffer9* batchIndexBuffer = nullptr;
	static unsigned sceneNumber = 1;
	static unsigned sceneNumberAtLastHide = 0;

	static constexpr UINT skinnedRingBytes = 6u * 1024u * 1024u;
	static IDirect3DVertexBuffer9* skinnedVertexBuffer = nullptr;
	static UINT skinnedRingOffset = 0;

	static WorldSource builtFrom;
	static bool didBuildFail = false;

	static constexpr int maxSkyMaterials = 16;
	static std::uintptr_t skyMaterials[maxSkyMaterials];
	static int skyMaterialCount = 0;
	static unsigned viewSurfaces = 0;
	static unsigned viewSkyGroups = 0;
	static bool didLastViewHaveSky = true;
	static unsigned skyTransitionLogs = 0;
	static constexpr unsigned maxSkyTransitionLogs = 200;

	static WorldStats stats;

	struct DumpRow
	{
		const char* kind;
		char material[96];
		char techniqueSet[64];
		char image[64];
		std::uint64_t imageHash;
		unsigned draws;
		DWORD isBlending;
		DWORD sourceBlend;
		DWORD destinationBlend;
		DWORD isAlphaTested;
		DWORD alphaReference;
		DWORD alphaFunction;
		DWORD cullMode;
		float minZ;
		float maxZ;
	};

	struct ReticleShape
	{
		UINT width = 0;
		UINT height = 0;
		std::vector<std::uint32_t> texels;
	};

	struct ReticleProjection
	{
		float scaleU;
		float scaleV;
		bool isActive;
		const ReticleShape* shape;
	};

	struct LensFrame
	{
		float origin[3];
		float normal[3];
		float tangent[3];
		float bitangent[3];
		float distance;
	};

	static ReticleProjection reticle{};
	static constexpr D3DCOLOR reticleColour = D3DCOLOR_ARGB(255, 255, 76, 76);
	static constexpr D3DCOLOR reticleDotColour = D3DCOLOR_ARGB(255, 255, 40, 30);
	static constexpr float reticleGlowIntensity = 12.0f;
	static constexpr std::size_t maxLensVertices = 64;
	static std::unordered_map<std::uintptr_t, ReticleShape> reticleShapes;
	static IDirect3DTexture9* reticleDotTexture = nullptr;
	static bool hasTriedReticleDotTexture = false;
	static std::vector<WorldVertex> reticleDotVertices;
	static unsigned long long reticleMeshDraws = 0;

	using AddDObjToScene = void(__fastcall*)(void*, void*, int, unsigned int, const float*, float);
	using IsThirdPersonMode = bool(__fastcall*)(void*);

	struct BodyTextureSwap
	{
		IDirect3DBaseTexture9* groupTexture = nullptr;
		bool isBodyBound = false;
	};

	static constexpr float localBodyMarker = -1337.0f;
	static constexpr float bodyHeadRadius = 14.0f;
	static bool isPlayerBodyShadow = false;
	static AddDObjToScene originalAddPlayerDObj = nullptr;
	static IsThirdPersonMode originalBodyThirdPersonCheck = nullptr;
	static std::unordered_map<std::uintptr_t, IDirect3DTexture9*> bodyTwins;
	static std::string bodyTwinHashList;
	static std::unordered_map<std::uint64_t, IDirect3DTexture9*> layerTwins;

	static std::unordered_map<std::uintptr_t, IDirect3DTexture9*> camoTextures;
	static std::unordered_map<std::uintptr_t, IDirect3DTexture9*> reticleTextures;
	static std::unordered_map<std::uintptr_t, IDirect3DTexture9*> waterTextures;
	static bool isPulledTowardEye = false;
	static constexpr float illuminatedLayerKeep = 0.99f;
	static unsigned long long reticleDraws = 0;
	static unsigned long long reticleFailures = 0;
	static unsigned long long reticleLogScene = 0;
	static float bestCentreDistance = 1e9f;
	static float bestCentreU = 0.0f;
	static float bestCentreV = 0.0f;
	static bool didBuildCamoThisView = false;

	static bool isDumpRequested = false;
	static bool isDumping = false;
	static int dumpIndex = 0;
	static std::vector<DumpRow> dumpRows;
	static unsigned groupDraws = 0;

	static const D3DMATRIX identity = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	static bool TryReadWorldSource(WorldSource& source)
	{
		const std::uintptr_t worldDraw = ReadAt<std::uintptr_t>(
			Engine::RebaseAddress(Engine::CurrentBuild().g_worldDraw));

		if (!worldDraw)
		{
			return false;
		}

		source.vertices = ReadAt<const unsigned char*>(worldDraw + Iw4::worldDrawVertices);
		source.indices = ReadAt<const std::uint16_t*>(worldDraw + Iw4::worldDrawIndices);
		source.vertexCount = ReadAt<std::uint32_t>(worldDraw + Iw4::worldDrawVertexCount);
		source.indexCount = ReadAt<std::uint32_t>(worldDraw + Iw4::worldDrawIndexCount);

		return source.vertices && source.indices && source.vertexCount && source.indexCount;
	}

	static float UnpackTexCoord(std::uint16_t packed)
	{
		if (packed == 0)
		{
			return 0.0f;
		}

		const std::uint32_t value = packed;
		const std::uint32_t bits = ((value & 0x8000u) << 16)
			| (((((value & 0x3FFFu) << 14) - (~(value << 14) & 0x10000000u)) ^ 0x80000001u) >> 1);

		float result;
		std::memcpy(&result, &bits, sizeof(result));
		return result;
	}

	static void ReleaseModelBuffers()
	{
		for (auto& entry : modelBuffers)
		{
			if (entry.second.vertexBuffer)
			{
				entry.second.vertexBuffer->Release();
			}

			if (entry.second.indexBuffer)
			{
				entry.second.indexBuffer->Release();
			}
		}

		modelBuffers.clear();
		stats.staticModelSurfaces = 0;
	}

	static void UnpackNormal(const unsigned char* packed, float* normal)
	{
		const float scale = static_cast<float>(packed[3]) / 255.0f + 0.7529412f;

		for (int i = 0; i < 3; ++i)
		{
			normal[i] = (static_cast<float>(packed[i]) / 127.0f - 1.0f) * scale;
		}
	}

	static void ConvertWorldVertex(const unsigned char* packed, WorldVertex& vertex)
	{
		std::memcpy(vertex.position, packed, sizeof(vertex.position));
		UnpackNormal(packed + Iw4::worldVertexNormal, vertex.normal);
		std::memcpy(&vertex.colour, packed + Iw4::worldVertexColor, sizeof(vertex.colour));
		std::memcpy(vertex.texCoord, packed + Iw4::worldVertexTexCoord, sizeof(vertex.texCoord));
	}

	static bool TryBuildWorldBuffers(IDirect3DDevice9* device, const WorldSource& source)
	{
		const UINT vertexBytes = source.vertexCount * static_cast<UINT>(sizeof(WorldVertex));
		const UINT indexBytes = source.indexCount * static_cast<UINT>(sizeof(std::uint16_t));

		if (FAILED(device->CreateVertexBuffer(vertexBytes, D3DUSAGE_WRITEONLY, worldVertexFormat, D3DPOOL_DEFAULT,
				&worldVertexBuffer, nullptr)))
		{
			worldVertexBuffer = nullptr;
			return false;
		}

		if (FAILED(device->CreateIndexBuffer(indexBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
				&worldIndexBuffer, nullptr)))
		{
			worldIndexBuffer = nullptr;
			return false;
		}

		void* vertexData = nullptr;

		if (FAILED(worldVertexBuffer->Lock(0, 0, &vertexData, 0)))
		{
			return false;
		}

		auto* vertices = static_cast<WorldVertex*>(vertexData);

		for (std::uint32_t i = 0; i < source.vertexCount; ++i)
		{
			ConvertWorldVertex(source.vertices + i * Iw4::worldVertexStride, vertices[i]);
		}

		worldVertexBuffer->Unlock();

		void* indexData = nullptr;

		if (FAILED(worldIndexBuffer->Lock(0, 0, &indexData, 0)))
		{
			return false;
		}

		std::memcpy(indexData, source.indices, indexBytes);
		worldIndexBuffer->Unlock();

		const UINT blendedIndexBytes = source.indexCount * static_cast<UINT>(sizeof(std::uint32_t));

		if (FAILED(device->CreateIndexBuffer(blendedIndexBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT,
				&blendedIndexBuffer, nullptr)))
		{
			blendedIndexBuffer = nullptr;
			return true;
		}

		void* blendedIndexData = nullptr;

		if (FAILED(blendedIndexBuffer->Lock(0, 0, &blendedIndexData, 0)))
		{
			blendedIndexBuffer->Release();
			blendedIndexBuffer = nullptr;
			return true;
		}

		auto* blendedIndices = static_cast<std::uint32_t*>(blendedIndexData);

		for (std::uint32_t i = 0; i < source.indexCount; ++i)
		{
			blendedIndices[i] = source.indices[i];
		}

		blendedIndexBuffer->Unlock();

		return true;
	}

	static bool HasSkyTechniqueSet(std::uintptr_t material)
	{
		if (!material)
		{
			return false;
		}

		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (!techniqueSet)
		{
			return false;
		}

		const char* techniqueSetName = ReadAt<const char*>(techniqueSet);

		if (!techniqueSetName)
		{
			return false;
		}

		std::string_view name(techniqueSetName);
		const std::string_view suffix = "_sky";

		// some converted maps name their technique sets "wc_sky.3x"
		if (name.size() > 3 && name.substr(name.size() - 3) == ".3x")
		{
			name.remove_suffix(3);
		}

		if (name == "sky")
		{
			return true;
		}

		return name.size() >= suffix.size() && name.substr(name.size() - suffix.size()) == suffix;
	}

	static void CollectSkyMaterials()
	{
		skyMaterialCount = 0;

		const Iw4::GameBuild& build = Engine::CurrentBuild();
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));
		const auto worldDpvs = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.g_worldDpvs));

		if (!world || !worldDpvs)
		{
			return;
		}

		const int skyCount = ReadAt<int>(world + Iw4::worldSkyCount);
		const auto skies = ReadAt<std::uintptr_t>(world + Iw4::worldSkies);
		const auto sortedSurfIndex = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSortedSurfIndex);
		const auto surfaces = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSurfaces);

		if (!skies || !sortedSurfIndex || !surfaces)
		{
			return;
		}

		for (int skyIndex = 0; skyIndex < skyCount; ++skyIndex)
		{
			const std::uintptr_t sky = skies + skyIndex * Iw4::gfxSkyStride;
			const int surfaceCount = ReadAt<int>(sky + Iw4::gfxSkySurfaceCount);
			const auto startSurfaces = ReadAt<std::uintptr_t>(sky + Iw4::gfxSkyStartSurfaces);

			for (int i = 0; i < surfaceCount && startSurfaces; ++i)
			{
				const int listIndex = ReadAt<int>(startSurfaces + i * sizeof(int));
				const auto surfaceIndex = ReadAt<std::uint16_t>(sortedSurfIndex + listIndex * sizeof(std::uint16_t));
				const auto material = ReadAt<std::uintptr_t>(
					surfaces + surfaceIndex * Iw4::gfxSurfaceStride + Iw4::gfxSurfaceMaterial);

				if (!HasSkyTechniqueSet(material))
				{
					continue;
				}

				bool isKnown = false;

				for (int known = 0; known < skyMaterialCount; ++known)
				{
					if (skyMaterials[known] == material)
					{
						isKnown = true;
					}
				}

				if (!isKnown && skyMaterialCount < maxSkyMaterials)
				{
					skyMaterials[skyMaterialCount] = material;
					++skyMaterialCount;
				}
			}
		}
	}

	static bool IsSkyMaterial(std::uintptr_t material)
	{
		for (int i = 0; i < skyMaterialCount; ++i)
		{
			if (skyMaterials[i] == material)
			{
				return true;
			}
		}

		return false;
	}

	static void ReleaseMaterialBatches()
	{
		if (batchVertexBuffer)
		{
			batchVertexBuffer->Release();
			batchVertexBuffer = nullptr;
		}

		if (batchIndexBuffer)
		{
			batchIndexBuffer->Release();
			batchIndexBuffer = nullptr;
		}

		materialBatches.clear();
		stats.materialBatches = 0;
	}

	static FixtureSpot SurfaceSpot(const WorldSource& source, const BatchSurface& surface)
	{
		FixtureSpot spot{};
		float normal[3] = {};
		const UINT cornerCount = 3u * surface.triCount;

		for (UINT index = 0; index < cornerCount; ++index)
		{
			std::uint32_t engineVertex = surface.firstVertex + source.indices[surface.baseIndex + index];

			if (engineVertex >= source.vertexCount)
			{
				engineVertex = surface.firstVertex;
			}

			const unsigned char* packed = source.vertices + engineVertex * Iw4::worldVertexStride;
			float position[3];
			float vertexNormal[3];
			std::memcpy(position, packed, sizeof(position));
			UnpackNormal(packed + Iw4::worldVertexNormal, vertexNormal);

			for (int axis = 0; axis < 3; ++axis)
			{
				spot.position[axis] += position[axis] / static_cast<float>(cornerCount);
				normal[axis] += vertexNormal[axis];
			}
		}

		const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);

		if (length > 0.001f)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				spot.position[axis] += normal[axis] / length * fixtureLift;
			}
		}

		return spot;
	}

	static bool TryGetMaterialColourHash(std::uintptr_t material, std::uint64_t& hash);
	static constexpr float floorFacing = 0.8f;
	static constexpr float floorShare = 0.6f;

	static bool IsMostlyFloor(const WorldSource& source, const std::vector<BatchSurface>& surfaces)
	{
		float upArea = 0.0f;
		float totalArea = 0.0f;

		for (const BatchSurface& surface : surfaces)
		{
			for (UINT triangle = 0; triangle < surface.triCount; ++triangle)
			{
				float corners[3][3];
				float upward = 0.0f;

				for (int corner = 0; corner < 3; ++corner)
				{
					std::uint32_t engineVertex = surface.firstVertex + source.indices[surface.baseIndex + triangle * 3 + corner];

					if (engineVertex >= source.vertexCount)
					{
						engineVertex = surface.firstVertex;
					}

					const unsigned char* packed = source.vertices + engineVertex * Iw4::worldVertexStride;
					std::memcpy(corners[corner], packed, sizeof(corners[corner]));

					float normal[3];
					UnpackNormal(packed + Iw4::worldVertexNormal, normal);
					upward += normal[2] / 3.0f;
				}

				const float edgeA[3] = { corners[1][0] - corners[0][0], corners[1][1] - corners[0][1], corners[1][2] - corners[0][2] };
				const float edgeB[3] = { corners[2][0] - corners[0][0], corners[2][1] - corners[0][1], corners[2][2] - corners[0][2] };
				const float cross[3] = {
					edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1],
					edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2],
					edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0],
				};
				const float area = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

				totalArea += area;

				if (upward >= floorFacing)
				{
					upArea += area;
				}
			}
		}

		return totalArea > 0.0f && upArea >= floorShare * totalArea;
	}

	static bool TryBuildMaterialBatches(IDirect3DDevice9* device, const WorldSource& source)
	{
		const std::uintptr_t worldDpvs = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().g_worldDpvs));

		if (!worldDpvs)
		{
			return false;
		}

		const std::uintptr_t surfaces = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSurfaces);

		if (!surfaces)
		{
			return false;
		}

		const std::uintptr_t ranges[3][2] = {
			{ Iw4::worldDpvsLitOpaqueBegin, Iw4::worldDpvsLitOpaqueEnd },
			{ Iw4::worldDpvsLitTransBegin, Iw4::worldDpvsLitTransEnd },
			{ Iw4::worldDpvsEmissiveBegin, Iw4::worldDpvsEmissiveEnd },
		};

		std::unordered_map<std::uintptr_t, std::vector<BatchSurface>> surfacesByMaterial;
		std::vector<std::uintptr_t> materialOrder;
		std::uint64_t totalIndices = 0;
		surfaceSpots.clear();
		++surfaceSpotsVersion;

		for (const auto& range : ranges)
		{
			const auto begin = ReadAt<std::uint32_t>(worldDpvs + range[0]);
			const auto end = ReadAt<std::uint32_t>(worldDpvs + range[1]);

			for (std::uint32_t surfaceIndex = begin; surfaceIndex < end; ++surfaceIndex)
			{
				const std::uintptr_t surface = surfaces + surfaceIndex * Iw4::gfxSurfaceStride;
				const auto material = ReadAt<std::uintptr_t>(surface + Iw4::gfxSurfaceMaterial);
				const auto firstVertex = ReadAt<std::uint32_t>(surface + Iw4::gfxSurfaceFirstVertex);
				const auto triCount = ReadAt<std::uint16_t>(surface + Iw4::gfxSurfaceTriCount);
				const auto baseIndex = ReadAt<std::uint32_t>(surface + Iw4::gfxSurfaceBaseIndex);

				const bool isUsable = material && triCount != 0 && !IsSkyMaterial(material)
					&& firstVertex < source.vertexCount
					&& static_cast<std::uint64_t>(baseIndex) + 3u * triCount <= source.indexCount;

				if (!isUsable)
				{
					continue;
				}

				std::vector<BatchSurface>& list = surfacesByMaterial[material];

				if (list.empty())
				{
					materialOrder.push_back(material);
				}

				list.push_back({ firstVertex, baseIndex, triCount });
				totalIndices += 3u * triCount;
				surfaceSpots[material].push_back(SurfaceSpot(source, list.back()));
			}
		}

		for (const std::uintptr_t material : materialOrder)
		{
			std::uint64_t colourHash = 0;

			if (IsMostlyFloor(source, surfacesByMaterial[material]) && TryGetMaterialColourHash(material, colourHash))
			{
				RegisterPbrFloor(colourHash);
			}
		}

		if (totalIndices == 0)
		{
			return false;
		}

		if (totalIndices > maxBatchIndices)
		{
			LogWarn() << "remix: the map's material batches would need " << totalIndices
					  << " indices, too many, surfaces are drawn one by one";
			return false;
		}

		const UINT capacity = static_cast<UINT>(totalIndices);

		if (FAILED(device->CreateVertexBuffer(capacity * static_cast<UINT>(sizeof(WorldVertex)), D3DUSAGE_WRITEONLY,
				worldVertexFormat, D3DPOOL_DEFAULT, &batchVertexBuffer, nullptr)))
		{
			batchVertexBuffer = nullptr;
			return false;
		}

		if (FAILED(device->CreateIndexBuffer(capacity * static_cast<UINT>(sizeof(std::uint32_t)), D3DUSAGE_WRITEONLY,
				D3DFMT_INDEX32, D3DPOOL_DEFAULT, &batchIndexBuffer, nullptr)))
		{
			batchIndexBuffer = nullptr;
			ReleaseMaterialBatches();
			return false;
		}

		void* vertexData = nullptr;
		void* indexData = nullptr;

		if (FAILED(batchVertexBuffer->Lock(0, 0, &vertexData, 0)))
		{
			ReleaseMaterialBatches();
			return false;
		}

		if (FAILED(batchIndexBuffer->Lock(0, 0, &indexData, 0)))
		{
			batchVertexBuffer->Unlock();
			ReleaseMaterialBatches();
			return false;
		}

		auto* vertices = static_cast<WorldVertex*>(vertexData);
		auto* indices = static_cast<std::uint32_t*>(indexData);
		constexpr std::uint32_t unmapped = 0xFFFFFFFFu;
		std::vector<std::uint32_t> remap(source.vertexCount, unmapped);
		std::vector<std::uint32_t> touched;
		UINT vertexCursor = 0;
		UINT indexCursor = 0;

		for (const std::uintptr_t material : materialOrder)
		{
			MaterialBatch batch;
			batch.startIndex = indexCursor;
			batch.firstVertex = vertexCursor;

			for (const BatchSurface& surface : surfacesByMaterial[material])
			{
				for (UINT index = 0; index < 3u * surface.triCount; ++index)
				{
					std::uint32_t engineVertex = surface.firstVertex + source.indices[surface.baseIndex + index];

					if (engineVertex >= source.vertexCount)
					{
						engineVertex = surface.firstVertex;
					}

					if (remap[engineVertex] == unmapped)
					{
						remap[engineVertex] = vertexCursor;
						touched.push_back(engineVertex);
						ConvertWorldVertex(source.vertices + engineVertex * Iw4::worldVertexStride,
							vertices[vertexCursor]);
						++vertexCursor;
					}

					indices[indexCursor] = remap[engineVertex];
					++indexCursor;
				}
			}

			for (const std::uint32_t engineVertex : touched)
			{
				remap[engineVertex] = unmapped;
			}

			touched.clear();

			batch.vertexCount = vertexCursor - batch.firstVertex;
			batch.triCount = (indexCursor - batch.startIndex) / 3u;
			materialBatches[material] = batch;
		}

		batchVertexBuffer->Unlock();
		batchIndexBuffer->Unlock();

		stats.materialBatches = static_cast<unsigned>(materialBatches.size());

		Log() << "remix: grouped the map's surfaces into " << stats.materialBatches << " material batches ("
			  << vertexCursor << " vertices, " << indexCursor << " indices), one draw per material instead of"
				 " one per surface";

		return true;
	}

	static bool EnsureWorldBuffers(IDirect3DDevice9* device)
	{
		WorldSource source;

		if (!TryReadWorldSource(source))
		{
			return false;
		}

		const bool isSameWorld = source.vertices == builtFrom.vertices && source.indices == builtFrom.indices
			&& source.vertexCount == builtFrom.vertexCount && source.indexCount == builtFrom.indexCount;

		if (isSameWorld && (worldVertexBuffer || didBuildFail))
		{
			return worldVertexBuffer != nullptr;
		}

		ReleaseWorldBuffers();
		builtFrom = source;

		if (!TryBuildWorldBuffers(device, source))
		{
			ReleaseWorldBuffers();
			didBuildFail = true;

			LogError() << "remix: could not build the fixed-function world (" << source.vertexCount << " vertices, "
					   << source.indexCount << " indices), the map stays on vertex capture";
			return false;
		}

		CollectSkyMaterials();

		if (!TryBuildMaterialBatches(device, source))
		{
			ReleaseMaterialBatches();
			LogWarn() << "remix: could not group the map into material batches, surfaces are drawn one by one";
		}

		++stats.builds;
		stats.vertexCount = source.vertexCount;
		stats.indexCount = source.indexCount;

		Device::NoteFrameEvent("world buffers built");
		Log() << "remix: built the fixed-function world from the loaded map, " << source.vertexCount
			  << " vertices and " << source.indexCount << " indices, " << skyMaterialCount << " sky materials";

		return true;
	}

	static bool IsDynamicLightList(void* context)
	{
		if (!stats.isHookInstalled || !sceneDevice)
		{
			return false;
		}

		const auto* info = ReadAt<const int*>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsInfo);
		const int baseTechType = *info;

		return baseTechType >= Iw4::TECHNIQUE_LIGHT_SPOT && baseTechType <= Iw4::TECHNIQUE_LIGHT_SPOT_SHADOW;
	}

	static bool ShouldDrawFixedFunction(void* context)
	{
		if (!stats.isHookInstalled || !sceneDevice)
		{
			return false;
		}

		const auto* info = ReadAt<const int*>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsInfo);
		const int baseTechType = *info;

		if (baseTechType <= Iw4::TECHNIQUE_BUILD_SHADOWMAP_COLOR)
		{
			return false;
		}

		return EnsureWorldBuffers(sceneDevice);
	}

	static void DrawWorldSurface(IDirect3DDevice9* device, std::uintptr_t surface)
	{
		const auto firstVertex = ReadAt<std::uint32_t>(surface + Iw4::gfxSurfaceFirstVertex);
		const auto vertexCount = ReadAt<std::uint16_t>(surface + Iw4::gfxSurfaceVertexCount);
		const auto triCount = ReadAt<std::uint16_t>(surface + Iw4::gfxSurfaceTriCount);
		const auto baseIndex = ReadAt<std::uint32_t>(surface + Iw4::gfxSurfaceBaseIndex);

		const bool isInRange = triCount != 0
			&& static_cast<std::uint64_t>(firstVertex) + vertexCount <= builtFrom.vertexCount
			&& static_cast<std::uint64_t>(baseIndex) + 3u * triCount <= builtFrom.indexCount;

		if (!isInRange)
		{
			return;
		}

		device->SetStreamSource(0, worldVertexBuffer, firstVertex * static_cast<UINT>(sizeof(WorldVertex)),
			static_cast<UINT>(sizeof(WorldVertex)));
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vertexCount, baseIndex, triCount);
		++viewSurfaces;
		++groupDraws;
	}

	static void DrawSurfaceGroup(IDirect3DDevice9* device, void*, void** iterator)
	{
		const std::uintptr_t worldDpvs = ReadAt<std::uintptr_t>(
			Engine::RebaseAddress(Engine::CurrentBuild().g_worldDpvs));
		const std::uintptr_t surfaces = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSurfaces);

		const auto* cursor = static_cast<const std::uint16_t*>(iterator[0]);

		do
		{
			const unsigned firstSurface = cursor[1];
			const unsigned lastSurface = firstSurface + (cursor[0] & Iw4::bspListRunMask);
			cursor += 2;

			for (unsigned surfaceIndex = firstSurface; surfaceIndex <= lastSurface; ++surfaceIndex)
			{
				DrawWorldSurface(device, surfaces + surfaceIndex * Iw4::gfxSurfaceStride);
			}
		}
		while ((cursor[0] & Iw4::bspListGroupStart) == 0);

		iterator[0] = const_cast<std::uint16_t*>(cursor);
	}

	static void SkipSurfaceGroup(void** iterator)
	{
		const auto* cursor = static_cast<const std::uint16_t*>(iterator[0]);

		do
		{
			cursor += 2;
		}
		while ((cursor[0] & Iw4::bspListGroupStart) == 0);

		iterator[0] = const_cast<std::uint16_t*>(cursor);
	}

	static void ChangeDepthRangeLikeEngine(void* context)
	{
		const auto contextAddress = reinterpret_cast<std::uintptr_t>(context);
		const auto source = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsSource);
		const auto state = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsState);

		int depthRange = -1;

		if (ReadAt<int>(source + Iw4::sourceSceneDepthRange) != 0)
		{
			depthRange = 0;
		}

		changeDepthRange(reinterpret_cast<void*>(state), depthRange);
	}

	static std::uintptr_t GroupMaterial(void* context)
	{
		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);
		return ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);
	}

	static bool IsSkyGroup(void* context)
	{
		return IsSkyMaterial(GroupMaterial(context));
	}

	static MaterialBatch* TryGetGroupBatch(void* context)
	{
		const auto found = materialBatches.find(GroupMaterial(context));

		if (found == materialBatches.end())
		{
			return nullptr;
		}

		return &found->second;
	}

	static void RecordBatchDrawState(IDirect3DDevice9* device, BatchDrawState& drawState)
	{
		if (drawState.isKnown)
		{
			return;
		}

		DWORD isBlending = FALSE;
		DWORD textureTransformFlags = D3DTTFF_DISABLE;
		D3DVIEWPORT9 viewport{};

		device->GetRenderState(D3DRS_ALPHABLENDENABLE, &isBlending);
		device->GetRenderState(D3DRS_ALPHATESTENABLE, &drawState.isAlphaTested);
		device->GetRenderState(D3DRS_ALPHAREF, &drawState.alphaReference);
		device->GetRenderState(D3DRS_ALPHAFUNC, &drawState.alphaFunction);
		device->GetRenderState(D3DRS_CULLMODE, &drawState.cullMode);
		device->GetRenderState(D3DRS_ZENABLE, &drawState.isDepthTested);
		device->GetRenderState(D3DRS_ZWRITEENABLE, &drawState.isDepthWritten);
		device->GetRenderState(D3DRS_ZFUNC, &drawState.depthFunction);
		device->GetTextureStageState(0, D3DTSS_COLOROP, &drawState.colourOperation);
		device->GetTextureStageState(0, D3DTSS_COLORARG1, &drawState.colourArgument1);
		device->GetTextureStageState(0, D3DTSS_COLORARG2, &drawState.colourArgument2);
		device->GetTextureStageState(0, D3DTSS_ALPHAOP, &drawState.alphaOperation);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &drawState.alphaArgument1);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &drawState.alphaArgument2);
		device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &textureTransformFlags);
		device->GetSamplerState(0, D3DSAMP_ADDRESSU, &drawState.addressU);
		device->GetSamplerState(0, D3DSAMP_ADDRESSV, &drawState.addressV);
		device->GetViewport(&viewport);

		drawState.isOpaque = isBlending == FALSE;
		drawState.isScrolling = textureTransformFlags != D3DTTFF_DISABLE;
		drawState.minZ = viewport.MinZ;
		drawState.maxZ = viewport.MaxZ;
		drawState.isKnown = true;
	}

	static void DrawMaterialBatchGroup(IDirect3DDevice9* device, void* context, void** iterator)
	{
		MaterialBatch* batch = TryGetGroupBatch(context);

		if (!batch)
		{
			DrawSurfaceGroup(device, context, iterator);
			return;
		}

		batch->drawnScene = sceneNumber;
		RecordBatchDrawState(device, batch->drawState);

		device->SetStreamSource(0, batchVertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
		device->SetIndices(batchIndexBuffer);
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, batch->firstVertex, batch->vertexCount, batch->startIndex,
			batch->triCount);

		++viewSurfaces;
		++groupDraws;
		++stats.batchDraws;

		SkipSurfaceGroup(iterator);
	}

	static void DrawSkyThroughEngine(void* context, void** iterator)
	{
		++viewSkyGroups;
		ChangeDepthRangeLikeEngine(context);
		NoteSkyMaterial(GroupMaterial(context));

		static bool hasLoggedSkyState = false;

		if (!hasLoggedSkyState)
		{
			hasLoggedSkyState = true;

			DWORD isSrgbWrite = FALSE;
			DWORD srgbTextures[4] = {};
			sceneDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &isSrgbWrite);

			for (DWORD sampler = 0; sampler < 4; ++sampler)
			{
				sceneDevice->GetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, &srgbTextures[sampler]);
			}

			Log() << "remix: sky draw sRGB write " << isSrgbWrite << ", sRGB texture on samplers 0-3 " << srgbTextures[0]
				  << " " << srgbTextures[1] << " " << srgbTextures[2] << " " << srgbTextures[3];
		}

		D3DVIEWPORT9 viewport{};
		sceneDevice->GetViewport(&viewport);

		D3DVIEWPORT9 skyViewport = viewport;
		skyViewport.MinZ = 1.0f;
		skyViewport.MaxZ = 1.0f;

		sceneDevice->SetViewport(&skyViewport);
		originalTess(context, iterator);
		sceneDevice->SetViewport(&viewport);
	}

	static D3DMATRIX PlacementMatrix(std::uintptr_t placement)
	{
		float quat[4];
		float origin[3];
		std::memcpy(quat, reinterpret_cast<const void*>(placement), sizeof(quat));
		std::memcpy(origin, reinterpret_cast<const void*>(placement + Iw4::gfxPlacementOrigin), sizeof(origin));

		const float x = quat[0];
		const float y = quat[1];
		const float z = quat[2];
		const float w = quat[3];

		D3DMATRIX matrix = identity;

		matrix._11 = 1.0f - 2.0f * (y * y + z * z);
		matrix._12 = 2.0f * (x * y + w * z);
		matrix._13 = 2.0f * (x * z - w * y);

		matrix._21 = 2.0f * (x * y - w * z);
		matrix._22 = 1.0f - 2.0f * (x * x + z * z);
		matrix._23 = 2.0f * (y * z + w * x);

		matrix._31 = 2.0f * (x * z + w * y);
		matrix._32 = 2.0f * (y * z - w * x);
		matrix._33 = 1.0f - 2.0f * (x * x + y * y);

		matrix._41 = origin[0];
		matrix._42 = origin[1];
		matrix._43 = origin[2];

		return matrix;
	}

	static void DrawBrushModelGroup(IDirect3DDevice9* device, void* context, void** iterator)
	{
		const auto source = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsSource);
		const auto backEnd = ReadAt<std::uintptr_t>(source + Iw4::sourceBackEndData);

		const auto* cursor = static_cast<const std::uint64_t*>(iterator[0]);
		const auto* end = static_cast<const std::uint64_t*>(iterator[1]);
		const std::uint64_t group = cursor[0] & Engine::CurrentDrawSurfLayout().groupMask;

		do
		{
			const std::uintptr_t record = backEnd + Iw4::backEndBModelSurfaces
				+ 4 * (cursor[0] & Iw4::drawSurfObjectMask);
			const auto placement = ReadAt<std::uintptr_t>(record + Iw4::bmodelSurfacePlacement);
			const auto surface = ReadAt<std::uintptr_t>(record + Iw4::bmodelSurfaceSurface);

			if (placement && surface)
			{
				const D3DMATRIX world = PlacementMatrix(placement);
				device->SetTransform(D3DTS_WORLD, &world);
				DrawWorldSurface(device, surface);
			}

			++cursor;
		}
		while (cursor != end && (cursor[0] & Engine::CurrentDrawSurfLayout().groupMask) == group);

		iterator[0] = const_cast<std::uint64_t*>(cursor);
	}

	static void ConvertPackedVertices(const unsigned char* packedVertices, std::uint16_t vertexCount, WorldVertex* vertices)
	{
		for (std::uint16_t i = 0; i < vertexCount; ++i)
		{
			const unsigned char* packed = packedVertices + i * Iw4::packedVertexStride;
			WorldVertex& vertex = vertices[i];

			std::uint32_t texCoord = 0;
			std::memcpy(vertex.position, packed, sizeof(vertex.position));
			std::memcpy(&vertex.colour, packed + Iw4::packedVertexColor, sizeof(vertex.colour));
			std::memcpy(&texCoord, packed + Iw4::packedVertexTexCoord, sizeof(texCoord));
			UnpackNormal(packed + Iw4::packedVertexNormal, vertex.normal);

			vertex.texCoord[0] = UnpackTexCoord(static_cast<std::uint16_t>(texCoord >> 16));
			vertex.texCoord[1] = UnpackTexCoord(static_cast<std::uint16_t>(texCoord & 0xFFFFu));
		}
	}

	static bool TryConvertModelSurface(const unsigned char* packedVertices, std::uint16_t vertexCount,
		const std::uint16_t* indices, UINT indexBytes, WorldVertex* vertices, void* indexData)
	{
		__try
		{
			ConvertPackedVertices(packedVertices, vertexCount, vertices);
			std::memcpy(indexData, indices, indexBytes);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void TransformPoint(const float* point, const D3DMATRIX& matrix, float* result)
	{
		for (int column = 0; column < 3; ++column)
		{
			result[column] = point[0] * matrix.m[0][column] + point[1] * matrix.m[1][column]
				+ point[2] * matrix.m[2][column] + matrix.m[3][column];
		}
	}

	static void TransformDirection(const float* direction, const D3DMATRIX& matrix, float* result)
	{
		for (int column = 0; column < 3; ++column)
		{
			result[column] = direction[0] * matrix.m[0][column] + direction[1] * matrix.m[1][column]
				+ direction[2] * matrix.m[2][column];
		}
	}

	static void Normalize(float* vector)
	{
		const float length = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);

		if (length <= 0.0f)
		{
			return;
		}

		vector[0] /= length;
		vector[1] /= length;
		vector[2] /= length;
	}

	static void ProjectReticleTexCoords(const unsigned char* packedVertices, std::uint16_t vertexCount,
		const D3DMATRIX& world, const D3DMATRIX& view, WorldVertex* vertices)
	{
		for (std::uint16_t i = 0; i < vertexCount; ++i)
		{
			const unsigned char* packed = packedVertices + i * Iw4::packedVertexStride;

			float position[3];
			std::memcpy(position, packed, sizeof(position));

			float worldPosition[3];
			float direction[3];
			TransformPoint(position, world, worldPosition);
			TransformPoint(worldPosition, view, direction);
			Normalize(direction);

			float packedNormal[3];
			float packedTangent[3];
			UnpackNormal(packed + Iw4::packedVertexNormal, packedNormal);
			UnpackNormal(packed + Iw4::packedVertexTangent, packedTangent);

			float worldNormal[3];
			float worldTangent[3];
			float normal[3];
			float tangent[3];
			TransformDirection(packedNormal, world, worldNormal);
			TransformDirection(packedTangent, world, worldTangent);
			TransformDirection(worldNormal, view, normal);
			TransformDirection(worldTangent, view, tangent);
			Normalize(normal);
			Normalize(tangent);

			const float bitangent[3] = {
				normal[1] * tangent[2] - normal[2] * tangent[1],
				normal[2] * tangent[0] - normal[0] * tangent[2],
				normal[0] * tangent[1] - normal[1] * tangent[0],
			};

			const float alongBitangent = bitangent[0] * direction[0] + bitangent[1] * direction[1]
				+ bitangent[2] * direction[2];
			const float alongTangent = tangent[0] * direction[0] + tangent[1] * direction[1] + tangent[2] * direction[2];

			vertices[i].texCoord[0] = alongBitangent * reticle.scaleU + 0.5f;
			vertices[i].texCoord[1] = alongTangent * reticle.scaleV + 0.5f;

			if (i == 0)
			{
				const float centreU = bitangent[2] * reticle.scaleU + 0.5f;
				const float centreV = tangent[2] * reticle.scaleV + 0.5f;
				const float centreDistance = std::fabs(centreU - 0.5f) + std::fabs(centreV - 0.5f);

				if (centreDistance < bestCentreDistance)
				{
					bestCentreDistance = centreDistance;
					bestCentreU = centreU;
					bestCentreV = centreV;
				}
			}
		}
	}

	static float Dot3(const float* left, const float* right)
	{
		return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
	}

	using LensPoint = std::array<float, 2>;

	static float Cross2(const LensPoint& origin, const LensPoint& first, const LensPoint& second)
	{
		return (first[0] - origin[0]) * (second[1] - origin[1]) - (first[1] - origin[1]) * (second[0] - origin[0]);
	}

	static std::size_t BuildConvexHull(LensPoint* points, std::size_t count, LensPoint* hull)
	{
		std::sort(points, points + count);

		std::size_t size = 0;

		for (std::size_t i = 0; i < count; ++i)
		{
			while (size >= 2 && Cross2(hull[size - 2], hull[size - 1], points[i]) <= 0.0f)
			{
				--size;
			}

			hull[size] = points[i];
			++size;
		}

		const std::size_t lowerSize = size + 1;

		for (std::size_t i = count; i-- > 0;)
		{
			while (size >= lowerSize && Cross2(hull[size - 2], hull[size - 1], points[i]) <= 0.0f)
			{
				--size;
			}

			hull[size] = points[i];
			++size;
		}

		if (size > 1)
		{
			--size;
		}

		return size;
	}

	static bool IsInsideHull(const LensPoint* hull, std::size_t size, const LensPoint& point)
	{
		if (size < 3)
		{
			return false;
		}

		for (std::size_t i = 0; i < size; ++i)
		{
			if (Cross2(hull[i], hull[(i + 1) % size], point) < 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	static bool TryPlaceOnLens(const LensFrame& lens, float u, float v, float* point)
	{
		const float along = (u - 0.5f) / reticle.scaleU;
		const float across = (v - 0.5f) / reticle.scaleV;
		const float remaining = 1.0f - along * along - across * across;

		if (remaining <= 0.0f)
		{
			return false;
		}

		float forward = std::sqrt(remaining);

		if (lens.distance < 0.0f)
		{
			forward = -forward;
		}

		const float rayLength = lens.distance / forward;

		for (int axis = 0; axis < 3; ++axis)
		{
			point[axis] = (along * lens.bitangent[axis] + across * lens.tangent[axis] + forward * lens.normal[axis]) * rayLength;
		}

		return true;
	}

	static IDirect3DTexture9* TryGetReticleDotTexture(IDirect3DDevice9* device)
	{
		if (reticleDotTexture || hasTriedReticleDotTexture)
		{
			return reticleDotTexture;
		}

		hasTriedReticleDotTexture = true;

		std::uint64_t hash = 0;
		reticleDotTexture = TryBuildSolidTexture(device, reticleDotColour, hash);

		if (reticleDotTexture)
		{
			RegisterPbrEmissive(hash, reticleDotTexture, reticleGlowIntensity);
		}

		return reticleDotTexture;
	}

	static void ReadReticleShape(IDirect3DTexture9* baked, ReticleShape& shape)
	{
		D3DSURFACE_DESC description{};
		D3DLOCKED_RECT locked{};

		if (FAILED(baked->GetLevelDesc(0, &description))
			|| FAILED(baked->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return;
		}

		shape.width = description.Width;
		shape.height = description.Height;

		for (UINT row = 0; row < description.Height; ++row)
		{
			const auto* line = reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(locked.pBits)
				+ static_cast<std::size_t>(row) * locked.Pitch);

			for (UINT column = 0; column < description.Width; ++column)
			{
				if ((line[column] >> 24) >= 0x80)
				{
					shape.texels.push_back(column | (row << 16));
				}
			}
		}

		baked->UnlockRect(0);
	}

	static bool TryBuildReticleDots(const unsigned char* packedVertices, std::uint16_t vertexCount, const D3DMATRIX& world,
		const D3DMATRIX& view)
	{
		const ReticleShape* shape = reticle.shape;

		if (!shape || shape->texels.empty() || reticle.scaleU == 0.0f || reticle.scaleV == 0.0f || vertexCount < 3)
		{
			return false;
		}

		float packedNormal[3];
		float packedTangent[3];
		float worldNormal[3];
		float worldTangent[3];
		float tangentGuess[3];
		LensFrame lens{};

		float position[3];
		float worldPosition[3];
		std::memcpy(position, packedVertices, sizeof(position));
		TransformPoint(position, world, worldPosition);
		TransformPoint(worldPosition, view, lens.origin);

		UnpackNormal(packedVertices + Iw4::packedVertexNormal, packedNormal);
		UnpackNormal(packedVertices + Iw4::packedVertexTangent, packedTangent);
		TransformDirection(packedNormal, world, worldNormal);
		TransformDirection(packedTangent, world, worldTangent);
		TransformDirection(worldNormal, view, lens.normal);
		TransformDirection(worldTangent, view, tangentGuess);
		Normalize(lens.normal);

		const float tangentLean = Dot3(tangentGuess, lens.normal);

		for (int axis = 0; axis < 3; ++axis)
		{
			lens.tangent[axis] = tangentGuess[axis] - lens.normal[axis] * tangentLean;
		}

		Normalize(lens.tangent);

		lens.bitangent[0] = lens.normal[1] * lens.tangent[2] - lens.normal[2] * lens.tangent[1];
		lens.bitangent[1] = lens.normal[2] * lens.tangent[0] - lens.normal[0] * lens.tangent[2];
		lens.bitangent[2] = lens.normal[0] * lens.tangent[1] - lens.normal[1] * lens.tangent[0];
		lens.distance = Dot3(lens.origin, lens.normal);

		if (std::fabs(lens.distance) < 0.001f)
		{
			return false;
		}

		const std::size_t outlineCount = std::min<std::size_t>(vertexCount, maxLensVertices);
		LensPoint outline[maxLensVertices];
		LensPoint hull[maxLensVertices * 2];

		for (std::size_t i = 0; i < outlineCount; ++i)
		{
			float vertexPosition[3];
			float vertexWorld[3];
			float vertexView[3];
			std::memcpy(vertexPosition, packedVertices + i * Iw4::packedVertexStride, sizeof(vertexPosition));
			TransformPoint(vertexPosition, world, vertexWorld);
			TransformPoint(vertexWorld, view, vertexView);

			const float offset[3] = {
				vertexView[0] - lens.origin[0],
				vertexView[1] - lens.origin[1],
				vertexView[2] - lens.origin[2],
			};

			outline[i][0] = Dot3(offset, lens.bitangent);
			outline[i][1] = Dot3(offset, lens.tangent);
		}

		const std::size_t hullSize = BuildConvexHull(outline, outlineCount, hull);

		reticleDotVertices.clear();

		float eyeSide = -1.0f;

		if (lens.distance < 0.0f)
		{
			eyeSide = 1.0f;
		}

		const float normalTowardEye[3] = {
			lens.normal[0] * eyeSide,
			lens.normal[1] * eyeSide,
			lens.normal[2] * eyeSide,
		};

		for (const std::uint32_t texel : shape->texels)
		{
			const float column = static_cast<float>(texel & 0xFFFF);
			const float row = static_cast<float>(texel >> 16);
			const float lowU = column / static_cast<float>(shape->width);
			const float highU = (column + 1.0f) / static_cast<float>(shape->width);
			const float lowV = row / static_cast<float>(shape->height);
			const float highV = (row + 1.0f) / static_cast<float>(shape->height);

			float centre[3];

			if (!TryPlaceOnLens(lens, 0.5f * (lowU + highU), 0.5f * (lowV + highV), centre))
			{
				continue;
			}

			const float centreOffset[3] = { centre[0] - lens.origin[0], centre[1] - lens.origin[1], centre[2] - lens.origin[2] };
			const LensPoint centreOnLens = { Dot3(centreOffset, lens.bitangent), Dot3(centreOffset, lens.tangent) };

			if (!IsInsideHull(hull, hullSize, centreOnLens))
			{
				continue;
			}

			float corners[4][3];

			if (!TryPlaceOnLens(lens, lowU, lowV, corners[0]) || !TryPlaceOnLens(lens, highU, lowV, corners[1])
				|| !TryPlaceOnLens(lens, lowU, highV, corners[2]) || !TryPlaceOnLens(lens, highU, highV, corners[3]))
			{
				continue;
			}

			static constexpr int quadOrder[6] = { 0, 1, 2, 2, 1, 3 };

			for (const int corner : quadOrder)
			{
				WorldVertex vertex{};
				std::memcpy(vertex.position, corners[corner], sizeof(vertex.position));
				std::memcpy(vertex.normal, normalTowardEye, sizeof(vertex.normal));
				vertex.colour = 0xFFFFFFFF;
				vertex.texCoord[0] = 0.5f;
				vertex.texCoord[1] = 0.5f;
				reticleDotVertices.push_back(vertex);
			}
		}

		return !reticleDotVertices.empty();
	}

	static bool TryDrawReticleDots(IDirect3DDevice9* device, const unsigned char* packedVertices, std::uint16_t vertexCount,
		const D3DMATRIX& world, const D3DMATRIX& view)
	{
		IDirect3DTexture9* dotTexture = TryGetReticleDotTexture(device);

		if (!dotTexture || !TryBuildReticleDots(packedVertices, vertexCount, world, view))
		{
			return false;
		}

		D3DMATRIX inverseView = identity;

		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
			{
				inverseView.m[row][column] = view.m[column][row];
			}
		}

		for (int column = 0; column < 3; ++column)
		{
			inverseView.m[3][column] = -(view._41 * view.m[column][0] + view._42 * view.m[column][1]
				+ view._43 * view.m[column][2]);
		}

		IDirect3DBaseTexture9* boundTexture = nullptr;
		DWORD cullMode = D3DCULL_CCW;
		DWORD isAlphaTested = FALSE;
		device->GetTexture(0, &boundTexture);
		device->GetRenderState(D3DRS_CULLMODE, &cullMode);
		device->GetRenderState(D3DRS_ALPHATESTENABLE, &isAlphaTested);

		device->SetTransform(D3DTS_WORLD, &inverseView);
		device->SetTexture(0, dotTexture);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(reticleDotVertices.size() / 3),
			reticleDotVertices.data(), static_cast<UINT>(sizeof(WorldVertex)));

		device->SetRenderState(D3DRS_ALPHATESTENABLE, isAlphaTested);
		device->SetRenderState(D3DRS_CULLMODE, cullMode);
		device->SetTexture(0, boundTexture);
		device->SetTransform(D3DTS_WORLD, &world);

		if (boundTexture)
		{
			boundTexture->Release();
		}

		++reticleMeshDraws;
		++groupDraws;
		return true;
	}

	static bool TryProjectReticle(const unsigned char* packedVertices, std::uint16_t vertexCount, const D3DMATRIX& world,
		const D3DMATRIX& view, WorldVertex* vertices)
	{
		__try
		{
			ProjectReticleTexCoords(packedVertices, vertexCount, world, view, vertices);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static bool TryConvertSkinnedVertices(const unsigned char* packedVertices, std::uint16_t vertexCount,
		WorldVertex* vertices)
	{
		__try
		{
			ConvertPackedVertices(packedVertices, vertexCount, vertices);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static const ModelBuffers* TryGetModelBuffers(IDirect3DDevice9* device, std::uintptr_t surface)
	{
		const auto found = modelBuffers.find(surface);

		if (found != modelBuffers.end())
		{
			if (!found->second.vertexBuffer)
			{
				return nullptr;
			}

			return &found->second;
		}

		ModelBuffers& buffers = modelBuffers[surface];

		const auto vertexCount = ReadAt<std::uint16_t>(surface + Iw4::xSurfaceVertCount);
		const auto triCount = ReadAt<std::uint16_t>(surface + Iw4::xSurfaceTriCount);
		const auto* packedVertices = ReadAt<const unsigned char*>(surface + Iw4::xSurfaceVerts0);
		const auto* indices = ReadAt<const std::uint16_t*>(surface + Iw4::xSurfaceTriIndices);

		if (vertexCount == 0 || triCount == 0 || !packedVertices || !indices)
		{
			return nullptr;
		}

		IDirect3DVertexBuffer9* vertexBuffer = nullptr;
		IDirect3DIndexBuffer9* indexBuffer = nullptr;
		const UINT vertexBytes = vertexCount * static_cast<UINT>(sizeof(WorldVertex));
		const UINT indexBytes = triCount * 3u * static_cast<UINT>(sizeof(std::uint16_t));

		if (FAILED(device->CreateVertexBuffer(vertexBytes, D3DUSAGE_WRITEONLY, worldVertexFormat, D3DPOOL_DEFAULT,
				&vertexBuffer, nullptr)))
		{
			return nullptr;
		}

		if (FAILED(device->CreateIndexBuffer(indexBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
				&indexBuffer, nullptr)))
		{
			vertexBuffer->Release();
			return nullptr;
		}

		void* vertexData = nullptr;
		void* indexData = nullptr;

		if (FAILED(vertexBuffer->Lock(0, 0, &vertexData, 0)))
		{
			vertexBuffer->Release();
			indexBuffer->Release();
			return nullptr;
		}

		if (FAILED(indexBuffer->Lock(0, 0, &indexData, 0)))
		{
			vertexBuffer->Unlock();
			vertexBuffer->Release();
			indexBuffer->Release();
			return nullptr;
		}

		const bool didConvert = TryConvertModelSurface(packedVertices, vertexCount, indices, indexBytes,
			static_cast<WorldVertex*>(vertexData), indexData);

		vertexBuffer->Unlock();
		indexBuffer->Unlock();

		if (!didConvert)
		{
			vertexBuffer->Release();
			indexBuffer->Release();
			return nullptr;
		}

		buffers.vertexBuffer = vertexBuffer;
		buffers.indexBuffer = indexBuffer;
		buffers.vertexCount = vertexCount;
		buffers.triCount = triCount;
		++stats.staticModelSurfaces;

		return &buffers;
	}

	static D3DMATRIX StaticModelMatrix(std::uintptr_t drawInst)
	{
		float origin[3];
		float axis[9];
		std::memcpy(origin, reinterpret_cast<const void*>(drawInst), sizeof(origin));
		std::memcpy(axis, reinterpret_cast<const void*>(drawInst + Iw4::smodelDrawInstAxis), sizeof(axis));
		const float scale = ReadAt<float>(drawInst + Iw4::smodelDrawInstScale);

		D3DMATRIX matrix = identity;

		matrix._11 = axis[0] * scale;
		matrix._12 = axis[1] * scale;
		matrix._13 = axis[2] * scale;

		matrix._21 = axis[3] * scale;
		matrix._22 = axis[4] * scale;
		matrix._23 = axis[5] * scale;

		matrix._31 = axis[6] * scale;
		matrix._32 = axis[7] * scale;
		matrix._33 = axis[8] * scale;

		matrix._41 = origin[0];
		matrix._42 = origin[1];
		matrix._43 = origin[2];

		return matrix;
	}

	// a lamp model's light goes at the middle of its glowing surface, in world space
	static bool TryGetPropSpot(const StaticSurfaceRecord& record, std::uintptr_t drawInsts, FixtureSpot& spot)
	{
		__try
		{
			const auto* packed = ReadAt<const unsigned char*>(record.surface + Iw4::xSurfaceVerts0);

			if (!packed || record.vertexCount == 0)
			{
				return false;
			}

			float centre[3] = {};

			for (std::uint16_t i = 0; i < record.vertexCount; ++i)
			{
				float position[3];
				std::memcpy(position, packed + i * Iw4::packedVertexStride, sizeof(position));

				for (int axis = 0; axis < 3; ++axis)
				{
					centre[axis] += position[axis] / static_cast<float>(record.vertexCount);
				}
			}

			const D3DMATRIX placement = StaticModelMatrix(drawInsts + record.instance * Iw4::smodelDrawInstStride);
			TransformPoint(centre, placement, spot.position);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void ReleaseStaticModelBatches()
	{
		if (staticBatchVertexBuffer)
		{
			staticBatchVertexBuffer->Release();
			staticBatchVertexBuffer = nullptr;
		}

		if (staticBatchIndexBuffer)
		{
			staticBatchIndexBuffer->Release();
			staticBatchIndexBuffer = nullptr;
		}

		staticModelBatches.clear();
		propSpots.clear();
		++surfaceSpotsVersion;
		bigPropRecords.clear();
		bigPropDrawnScene.clear();
		isStaticModelBaked.clear();
		instanceBatchStart.clear();
		instanceBatchIds.clear();
		batchNearestInstance.clear();
		batchNearestDistance.clear();
		isKeptInstance.clear();
		staticBatchDrawInsts = 0;
		staticBatchModelCount = 0;
		stats.staticModelBatches = 0;
		stats.bakedStaticModels = 0;
	}

	static bool TryCollectStaticSurfaces(std::uintptr_t drawInsts, std::uint32_t modelCount, StaticSurfaceRecord* records,
		std::size_t capacity, std::size_t& recordCount, std::uint8_t* baked)
	{
		__try
		{
			recordCount = 0;

			for (std::uint32_t instance = 0; instance < modelCount; ++instance)
			{
				const std::uintptr_t drawInst = drawInsts + instance * Iw4::smodelDrawInstStride;
				const auto model = ReadAt<std::uintptr_t>(drawInst + Iw4::smodelDrawInstModel);

				if (!model)
				{
					continue;
				}

				const auto lod = ReadAt<std::uint8_t>(model + Iw4::xModelFirstLod);
				const std::uintptr_t lodInfo = model + Iw4::xModelLodInfo + lod * Iw4::xModelLodInfoStride;
				const auto surfaceCount = ReadAt<std::uint16_t>(lodInfo + Iw4::lodInfoSurfaceCount);
				const auto surfaceIndex = ReadAt<std::uint16_t>(lodInfo + Iw4::lodInfoSurfaceIndex);
				const auto surfaces = ReadAt<std::uintptr_t>(lodInfo + Iw4::lodInfoSurfaces);
				const auto materialHandles = ReadAt<std::uintptr_t>(model + Iw4::xModelMaterialHandles);

				if (surfaceCount == 0 || surfaceCount > maxSurfacesPerModel || !surfaces || !materialHandles)
				{
					continue;
				}

				unsigned totalVertices = 0;

				for (std::uint16_t i = 0; i < surfaceCount; ++i)
				{
					totalVertices += ReadAt<std::uint16_t>(surfaces + i * Iw4::xSurfaceStride
						+ Iw4::xSurfaceVertCount);
				}

				if (totalVertices == 0 || recordCount + surfaceCount > capacity)
				{
					continue;
				}

				for (std::uint16_t i = 0; i < surfaceCount; ++i)
				{
					const std::uintptr_t surface = surfaces + i * Iw4::xSurfaceStride;
					const auto material = ReadAt<std::uintptr_t>(materialHandles + (surfaceIndex + i) * sizeof(std::uintptr_t));
					const auto vertexCount = ReadAt<std::uint16_t>(surface + Iw4::xSurfaceVertCount);
					const auto triCount = ReadAt<std::uint16_t>(surface + Iw4::xSurfaceTriCount);

					if (!material || vertexCount == 0 || triCount == 0
						|| ReadAt<std::uint8_t>(material + Iw4::materialCameraRegion) >= Iw4::cameraRegionNone)
					{
						continue;
					}

					records[recordCount] = { material, surface, instance, vertexCount, triCount };
					++recordCount;
				}

				if (totalVertices <= maxBakedModelVertices)
				{
					baked[instance] = 1;
				}
			}

			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static bool TryWriteStaticSurface(const StaticSurfaceRecord& record, const D3DMATRIX& placement, WorldVertex* vertices,
		std::uint32_t* indices, UINT baseVertex)
	{
		__try
		{
			const auto vertexCount = ReadAt<std::uint16_t>(record.surface + Iw4::xSurfaceVertCount);
			const auto triCount = ReadAt<std::uint16_t>(record.surface + Iw4::xSurfaceTriCount);
			const auto* packedVertices = ReadAt<const unsigned char*>(record.surface + Iw4::xSurfaceVerts0);
			const auto* sourceIndices = ReadAt<const std::uint16_t*>(record.surface + Iw4::xSurfaceTriIndices);

			if (vertexCount != record.vertexCount || triCount != record.triCount || !packedVertices || !sourceIndices)
			{
				return false;
			}

			ConvertPackedVertices(packedVertices, vertexCount, vertices);

			for (std::uint16_t i = 0; i < vertexCount; ++i)
			{
				float position[3];
				float normal[3];
				TransformPoint(vertices[i].position, placement, position);
				TransformDirection(vertices[i].normal, placement, normal);
				Normalize(normal);

				std::memcpy(vertices[i].position, position, sizeof(position));
				std::memcpy(vertices[i].normal, normal, sizeof(normal));
			}

			for (UINT i = 0; i < 3u * triCount; ++i)
			{
				const UINT local = sourceIndices[i];

				if (local < vertexCount)
				{
					indices[i] = baseVertex + local;
				}
				else
				{
					indices[i] = baseVertex;
				}
			}

			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void BuildStaticModelBatches(IDirect3DDevice9* device, std::uintptr_t drawInsts, std::uint32_t modelCount)
	{
		ReleaseStaticModelBatches();
		alienColourSeed = MixBits(static_cast<std::uint32_t>(GetTickCount64()) ^ alienColourSeed);
		staticBatchDrawInsts = drawInsts;
		staticBatchModelCount = modelCount;

		if (!drawInsts || modelCount == 0)
		{
			return;
		}

		std::vector<StaticSurfaceRecord> records(static_cast<std::size_t>(modelCount) * maxSurfacesPerModel);
		std::vector<std::uint8_t> baked(modelCount, 0);
		std::size_t recordCount = 0;

		if (!TryCollectStaticSurfaces(drawInsts, modelCount, records.data(), records.size(), recordCount, baked.data()))
		{
			LogWarn() << "remix: reading the map's static models faulted, props stay drawn one by one";
			return;
		}

		for (std::size_t i = 0; i < recordCount; ++i)
		{
			FixtureSpot spot{};

			if (TryGetPropSpot(records[i], drawInsts, spot))
			{
				propSpots[records[i].material].push_back(spot);
			}
		}

		++surfaceSpotsVersion;

		std::unordered_map<std::uintptr_t, std::vector<std::size_t>> recordsByMaterial;
		std::vector<std::uintptr_t> materialOrder;
		std::uint64_t totalVertices = 0;
		std::uint64_t totalIndices = 0;
		std::size_t bakedRecordCount = 0;

		for (std::size_t i = 0; i < recordCount; ++i)
		{
			if (baked[records[i].instance] == 0)
			{
				bigPropRecords.push_back(records[i]);
				continue;
			}

			records[bakedRecordCount] = records[i];
			++bakedRecordCount;
		}

		recordCount = bakedRecordCount;
		bigPropDrawnScene.assign(modelCount, 0);

		for (std::size_t i = 0; i < recordCount; ++i)
		{
			std::vector<std::size_t>& list = recordsByMaterial[records[i].material];

			if (list.empty())
			{
				materialOrder.push_back(records[i].material);
			}

			list.push_back(i);
			totalVertices += records[i].vertexCount;
			totalIndices += 3u * records[i].triCount;
		}

		if (totalIndices == 0 || totalVertices > maxStaticBatchVertices)
		{
			LogWarn() << "remix: static model batches would need " << totalVertices << " vertices, props stay drawn one by one";
			return;
		}

		if (FAILED(device->CreateVertexBuffer(static_cast<UINT>(totalVertices * sizeof(WorldVertex)), D3DUSAGE_WRITEONLY,
				worldVertexFormat, D3DPOOL_DEFAULT, &staticBatchVertexBuffer, nullptr)))
		{
			staticBatchVertexBuffer = nullptr;
			return;
		}

		if (FAILED(device->CreateIndexBuffer(static_cast<UINT>(totalIndices * sizeof(std::uint32_t)), D3DUSAGE_WRITEONLY,
				D3DFMT_INDEX32, D3DPOOL_DEFAULT, &staticBatchIndexBuffer, nullptr)))
		{
			staticBatchIndexBuffer = nullptr;
			ReleaseStaticModelBatches();
			staticBatchDrawInsts = drawInsts;
			staticBatchModelCount = modelCount;
			return;
		}

		void* vertexData = nullptr;
		void* indexData = nullptr;

		if (FAILED(staticBatchVertexBuffer->Lock(0, 0, &vertexData, 0)))
		{
			ReleaseStaticModelBatches();
			staticBatchDrawInsts = drawInsts;
			staticBatchModelCount = modelCount;
			return;
		}

		if (FAILED(staticBatchIndexBuffer->Lock(0, 0, &indexData, 0)))
		{
			staticBatchVertexBuffer->Unlock();
			ReleaseStaticModelBatches();
			staticBatchDrawInsts = drawInsts;
			staticBatchModelCount = modelCount;
			return;
		}

		auto* vertices = static_cast<WorldVertex*>(vertexData);
		auto* indices = static_cast<std::uint32_t*>(indexData);
		UINT vertexCursor = 0;
		UINT indexCursor = 0;
		bool didWriteAll = true;

		for (const std::uintptr_t material : materialOrder)
		{
			MaterialBatch batch;
			batch.startIndex = indexCursor;
			batch.firstVertex = vertexCursor;

			for (const std::size_t recordIndex : recordsByMaterial[material])
			{
				const StaticSurfaceRecord& record = records[recordIndex];
				const D3DMATRIX placement = StaticModelMatrix(drawInsts + record.instance * Iw4::smodelDrawInstStride);

				if (!TryWriteStaticSurface(record, placement, vertices + vertexCursor, indices + indexCursor, vertexCursor))
				{
					didWriteAll = false;
					break;
				}

				vertexCursor += record.vertexCount;
				indexCursor += 3u * record.triCount;
			}

			if (!didWriteAll)
			{
				break;
			}

			batch.vertexCount = vertexCursor - batch.firstVertex;
			batch.triCount = (indexCursor - batch.startIndex) / 3u;
			staticModelBatches[material] = batch;
		}

		staticBatchVertexBuffer->Unlock();
		staticBatchIndexBuffer->Unlock();

		if (!didWriteAll)
		{
			LogWarn() << "remix: a static model surface changed while baking, props stay drawn one by one";
			ReleaseStaticModelBatches();
			staticBatchDrawInsts = drawInsts;
			staticBatchModelCount = modelCount;
			return;
		}

		isStaticModelBaked.swap(baked);

		std::unordered_map<std::uintptr_t, std::uint32_t> batchIdOfMaterial;

		for (const std::uintptr_t material : materialOrder)
		{
			batchIdOfMaterial.emplace(material, static_cast<std::uint32_t>(batchIdOfMaterial.size()));
		}

		std::vector<std::pair<std::uint32_t, std::uint32_t>> instanceBatches;
		instanceBatches.reserve(recordCount);

		for (std::size_t i = 0; i < recordCount; ++i)
		{
			instanceBatches.emplace_back(records[i].instance, batchIdOfMaterial[records[i].material]);
		}

		std::sort(instanceBatches.begin(), instanceBatches.end());
		instanceBatches.erase(std::unique(instanceBatches.begin(), instanceBatches.end()), instanceBatches.end());

		instanceBatchStart.assign(static_cast<std::size_t>(modelCount) + 1, 0);
		instanceBatchIds.resize(instanceBatches.size());

		for (std::size_t i = 0; i < instanceBatches.size(); ++i)
		{
			++instanceBatchStart[instanceBatches[i].first + 1];
			instanceBatchIds[i] = instanceBatches[i].second;
		}

		for (std::uint32_t i = 0; i < modelCount; ++i)
		{
			instanceBatchStart[i + 1] += instanceBatchStart[i];
		}

		batchNearestInstance.assign(materialOrder.size(), 0);
		batchNearestDistance.assign(materialOrder.size(), FLT_MAX);
		isKeptInstance.assign(modelCount, 0);

		unsigned bakedCount = 0;

		for (const std::uint8_t isBaked : isStaticModelBaked)
		{
			bakedCount += isBaked;
		}

		stats.staticModelBatches = static_cast<unsigned>(staticModelBatches.size());
		stats.bakedStaticModels = bakedCount;

		Device::NoteFrameEvent("static models baked");
		Log() << "remix: baked " << bakedCount << " of " << modelCount << " static models (up to " << maxBakedModelVertices
			  << " vertices each) into " << staticModelBatches.size() << " material batches, " << vertexCursor
			  << " vertices; bigger props stay drawn per instance";
	}

	static void EnsureStaticModelBatches(IDirect3DDevice9* device)
	{
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!world)
		{
			return;
		}

		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);
		const auto modelCount = ReadAt<std::uint32_t>(world + Iw4::worldSModelCount);

		if (drawInsts == staticBatchDrawInsts && modelCount == staticBatchModelCount)
		{
			return;
		}

		BuildStaticModelBatches(device, drawInsts, modelCount);
	}

	static bool IsStaticModelBaked(std::uint16_t drawInstIndex)
	{
		return drawInstIndex < isStaticModelBaked.size() && isStaticModelBaked[drawInstIndex] != 0;
	}

	static bool HasUnbakedInstances(void** list)
	{
		const auto base = reinterpret_cast<std::uintptr_t>(list[0]);
		const auto* cursor = static_cast<const std::uint16_t*>(list[1]);
		const unsigned count = (cursor[1] >> Engine::CurrentDrawSurfLayout().smodelListCountShift) + 1;
		const std::uint16_t* entries = cursor + 2;

		for (unsigned i = 0; i < count; ++i)
		{
			const std::uintptr_t record = base + 4 * (entries[i] >> 4);
			const auto instanceCount = ReadAt<std::uint16_t>(record + Iw4::smodelRecordInstanceCount);

			for (std::uint16_t instance = 0; instance < instanceCount; ++instance)
			{
				const auto drawInstIndex = ReadAt<std::uint16_t>(record + Iw4::smodelRecordInstances
					+ instance * sizeof(std::uint16_t));

				if (!IsStaticModelBaked(drawInstIndex))
				{
					return true;
				}
			}
		}

		return false;
	}

	static void HideBakedPropsFromEngine()
	{
		hiddenVisibility.clear();

		if (GetAlienGlow().isEnabled && IsNeonFoliage())
		{
			return;
		}

		if (sceneNumber == sceneNumberAtLastHide)
		{
			return;
		}

		sceneNumberAtLastHide = sceneNumber;

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!world || instanceBatchStart.empty() || ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts)
			!= staticBatchDrawInsts)
		{
			return;
		}

		auto* visibility = ReadAt<std::uint8_t*>(world + Iw4::worldSModelVisibility);
		float camera[3];

		if (!visibility || !TryGetCameraOrigin(camera))
		{
			return;
		}

		std::fill(batchNearestDistance.begin(), batchNearestDistance.end(), FLT_MAX);

		for (std::uint32_t i = 0; i < staticBatchModelCount; ++i)
		{
			if (visibility[i] == 0 || isStaticModelBaked[i] == 0)
			{
				continue;
			}

			float origin[3];
			std::memcpy(origin, reinterpret_cast<const void*>(staticBatchDrawInsts + i * Iw4::smodelDrawInstStride
				+ Iw4::smodelDrawInstOrigin), sizeof(origin));

			const float dx = origin[0] - camera[0];
			const float dy = origin[1] - camera[1];
			const float dz = origin[2] - camera[2];
			const float distance = dx * dx + dy * dy + dz * dz;

			for (std::uint32_t k = instanceBatchStart[i]; k < instanceBatchStart[i + 1]; ++k)
			{
				const std::uint32_t batchId = instanceBatchIds[k];

				if (distance < batchNearestDistance[batchId])
				{
					batchNearestDistance[batchId] = distance;
					batchNearestInstance[batchId] = i;
				}
			}
		}

		unsigned kept = 0;

		for (std::size_t batchId = 0; batchId < batchNearestDistance.size(); ++batchId)
		{
			if (batchNearestDistance[batchId] < FLT_MAX && isKeptInstance[batchNearestInstance[batchId]] == 0)
			{
				isKeptInstance[batchNearestInstance[batchId]] = 1;
				++kept;
			}
		}

		for (std::uint32_t i = 0; i < staticBatchModelCount; ++i)
		{
			if (visibility[i] != 0 && isStaticModelBaked[i] != 0 && isKeptInstance[i] == 0)
			{
				hiddenVisibility.emplace_back(i, visibility[i]);
				visibility[i] = 0;
			}
		}

		for (std::size_t batchId = 0; batchId < batchNearestDistance.size(); ++batchId)
		{
			if (batchNearestDistance[batchId] < FLT_MAX)
			{
				isKeptInstance[batchNearestInstance[batchId]] = 0;
			}
		}

		stats.keptBakedProps = kept;
		stats.hiddenBakedProps = static_cast<unsigned>(hiddenVisibility.size());
	}

	static void RestoreBakedPropVisibility()
	{
		if (hiddenVisibility.empty())
		{
			return;
		}

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));
		auto* visibility = ReadAt<std::uint8_t*>(world + Iw4::worldSModelVisibility);

		for (const auto& hidden : hiddenVisibility)
		{
			visibility[hidden.first] = hidden.second;
		}

		hiddenVisibility.clear();
	}

	static void __fastcall AddStaticModelSurfacesDetour(void* view)
	{
		HideBakedPropsFromEngine();
		originalAddStaticModelSurfaces(view);
		RestoreBakedPropVisibility();
	}

	static MaterialBatch* TryGetStaticBatch(void* context)
	{
		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);
		const auto found = staticModelBatches.find(ReadAt<std::uintptr_t>(state + Iw4::stateMaterial));

		if (found == staticModelBatches.end())
		{
			return nullptr;
		}

		return &found->second;
	}

	static void DrawStaticModelGroup(IDirect3DDevice9* device, void* context, void** list)
	{
		MaterialBatch* staticBatch = TryGetStaticBatch(context);
		const bool isPerPlant = isAlienGroup;

		if (staticBatch && isPerPlant)
		{
			staticBatch->drawnScene = sceneNumber;
		}

		if (staticBatch && staticBatch->drawnScene != sceneNumber)
		{
			staticBatch->drawnScene = sceneNumber;
			RecordBatchDrawState(device, staticBatch->drawState);

			device->SetTransform(D3DTS_WORLD, &identity);
			device->SetStreamSource(0, staticBatchVertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
			device->SetIndices(staticBatchIndexBuffer);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, staticBatch->firstVertex, staticBatch->vertexCount,
				staticBatch->startIndex, staticBatch->triCount);

			++groupDraws;
			++stats.staticBatchDraws;
		}

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));
		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);
		const auto base = reinterpret_cast<std::uintptr_t>(list[0]);
		const auto* cursor = static_cast<const std::uint16_t*>(list[1]);

		const unsigned count = (cursor[1] >> Engine::CurrentDrawSurfLayout().smodelListCountShift) + 1;
		const std::uint16_t* entries = cursor + 2;

		for (unsigned i = 0; i < count; ++i)
		{
			const std::uintptr_t record = base + 4 * (entries[i] >> 4);
			const auto surfaces = ReadAt<std::uintptr_t>(record + Iw4::smodelRecordSurfaces);
			const std::uintptr_t surface = surfaces + Iw4::xSurfaceStride * (entries[i] & 0xF);
			const auto instanceCount = ReadAt<std::uint16_t>(record + Iw4::smodelRecordInstanceCount);

			const ModelBuffers* buffers = TryGetModelBuffers(device, surface);

			if (!buffers || !drawInsts)
			{
				continue;
			}

			device->SetStreamSource(0, buffers->vertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
			device->SetIndices(buffers->indexBuffer);

			for (std::uint16_t instance = 0; instance < instanceCount; ++instance)
			{
				const auto drawInstIndex = ReadAt<std::uint16_t>(record + Iw4::smodelRecordInstances
					+ instance * sizeof(std::uint16_t));

				if (!isPerPlant && IsStaticModelBaked(drawInstIndex))
				{
					continue;
				}

				if (drawInstIndex < bigPropDrawnScene.size())
				{
					bigPropDrawnScene[drawInstIndex] = sceneNumber;
				}

				const D3DMATRIX placement = StaticModelMatrix(drawInsts + drawInstIndex * Iw4::smodelDrawInstStride);

				if (isPerPlant)
				{
					const float origin[3] = { placement._41, placement._42, placement._43 };
					device->SetRenderState(D3DRS_TEXTUREFACTOR, AlienColour(drawInstIndex, origin));
				}

				device->SetTransform(D3DTS_WORLD, &placement);
				device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, buffers->vertexCount, 0, buffers->triCount);
				++groupDraws;
			}
		}

		list[1] = const_cast<std::uint16_t*>(entries + count);
	}

	static void SkipStaticModelGroup(void** list)
	{
		const auto* cursor = static_cast<const std::uint16_t*>(list[1]);
		const unsigned count = (cursor[1] >> Engine::CurrentDrawSurfLayout().smodelListCountShift) + 1;

		list[1] = const_cast<std::uint16_t*>(cursor + 2 + count);
	}

	static int EntityDepthRange(std::uintptr_t backEnd, std::uintptr_t record, std::uint64_t drawSurf, int cameraView)
	{
		if (cameraView == 0)
		{
			return -1;
		}

		if ((drawSurf & Engine::CurrentDrawSurfLayout().usesGfxEntity) == 0)
		{
			return 0;
		}

		const auto gfxEntity = ReadAt<std::uint16_t>(record + Iw4::modelSurfaceGfxEntity);
		const auto flags = ReadAt<std::uint32_t>(backEnd + Iw4::backEndGfxEntities
			+ gfxEntity * Iw4::gfxEntityStride);

		return static_cast<int>(flags & Iw4::gfxEntityDepthHack);
	}

	static WorldVertex* TryLockSkinnedVertices(IDirect3DDevice9* device, std::uint16_t vertexCount, UINT& offset);

	static void DrawReticleVertices(IDirect3DDevice9* device, std::uintptr_t surface, const ModelBuffers& buffers,
		UINT firstIndex, UINT triCount)
	{
		const auto* packedVertices = ReadAt<const unsigned char*>(surface + Iw4::xSurfaceVerts0);

		if (!packedVertices)
		{
			return;
		}

		UINT offset = 0;
		WorldVertex* vertices = TryLockSkinnedVertices(device, buffers.vertexCount, offset);

		if (!vertices)
		{
			return;
		}

		D3DMATRIX world = identity;
		D3DMATRIX view = identity;
		device->GetTransform(D3DTS_WORLD, &world);
		device->GetTransform(D3DTS_VIEW, &view);

		const bool didConvert = TryConvertSkinnedVertices(packedVertices, buffers.vertexCount, vertices)
			&& TryProjectReticle(packedVertices, buffers.vertexCount, world, view, vertices);

		float lowestU = 1e9f;
		float highestU = -1e9f;
		float lowestV = 1e9f;
		float highestV = -1e9f;
		const bool isLogDue = sceneNumber - reticleLogScene >= 600;

		if (didConvert && isLogDue)
		{
			for (UINT i = 0; i < buffers.vertexCount; ++i)
			{
				lowestU = std::min(lowestU, vertices[i].texCoord[0]);
				highestU = std::max(highestU, vertices[i].texCoord[0]);
				lowestV = std::min(lowestV, vertices[i].texCoord[1]);
				highestV = std::max(highestV, vertices[i].texCoord[1]);
			}
		}

		skinnedVertexBuffer->Unlock();

		if (!didConvert)
		{
			++reticleFailures;
			return;
		}

		++reticleDraws;

		if (isLogDue)
		{
			reticleLogScene = sceneNumber;
			Log() << "remix: reticle draws " << reticleDraws << " (" << reticleMeshDraws << " as lit texel geometry), vertex"
				  << " failures " << reticleFailures << ", last quad " << buffers.vertexCount << " vertices, " << triCount
				  << " triangles, u " << lowestU << " to " << highestU << ", v " << lowestV << " to " << highestV
				  << ", screen centre closest to the dot at uv " << bestCentreU << " " << bestCentreV << ", last mesh "
				  << reticleDotVertices.size() / 3 << " triangles";

			bestCentreDistance = 1e9f;
		}

		if (TryDrawReticleDots(device, packedVertices, buffers.vertexCount, world, view))
		{
			return;
		}

		device->SetStreamSource(0, skinnedVertexBuffer, offset, static_cast<UINT>(sizeof(WorldVertex)));
		device->SetIndices(buffers.indexBuffer);
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, buffers.vertexCount, firstIndex, triCount);
		++groupDraws;
	}

	static void PullTowardEye(IDirect3DDevice9* device, D3DMATRIX& world)
	{
		D3DMATRIX view = identity;
		device->GetTransform(D3DTS_VIEW, &view);

		float eye[3];

		for (int axis = 0; axis < 3; ++axis)
		{
			eye[axis] = -(view._41 * view.m[axis][0] + view._42 * view.m[axis][1] + view._43 * view.m[axis][2]);
		}

		for (int column = 0; column < 3; ++column)
		{
			world.m[0][column] *= illuminatedLayerKeep;
			world.m[1][column] *= illuminatedLayerKeep;
			world.m[2][column] *= illuminatedLayerKeep;
			world.m[3][column] = world.m[3][column] * illuminatedLayerKeep + eye[column] * (1.0f - illuminatedLayerKeep);
		}
	}

	static std::uintptr_t FindColorMapImage(std::uintptr_t material);

	static bool IsLocalBodyRecord(std::uintptr_t backEnd, std::uintptr_t record, bool hasEntity)
	{
		if (!isPlayerBodyShadow || !hasEntity)
		{
			return false;
		}

		const auto entityIndex = ReadAt<std::uint16_t>(record + Iw4::modelSurfaceGfxEntity);

		if (entityIndex == 0)
		{
			return false;
		}

		return ReadAt<float>(backEnd + Iw4::backEndGfxEntities + entityIndex * Iw4::gfxEntityStride
			+ Iw4::gfxEntityMaterialTime) == localBodyMarker;
	}

	static bool IsCentreNearEyeUnsafe(const unsigned char* packedVertices, std::uint16_t vertexCount, const D3DMATRIX& world,
		const float* eye)
	{
		float sum[3] = {};

		for (std::uint16_t i = 0; i < vertexCount; ++i)
		{
			const auto* position = reinterpret_cast<const float*>(packedVertices + i * Iw4::packedVertexStride);
			sum[0] += position[0];
			sum[1] += position[1];
			sum[2] += position[2];
		}

		const float count = static_cast<float>(vertexCount);
		const float centre[3] = { sum[0] / count, sum[1] / count, sum[2] / count };
		float worldCentre[3];
		TransformPoint(centre, world, worldCentre);

		const float dx = worldCentre[0] - eye[0];
		const float dy = worldCentre[1] - eye[1];
		const float dz = worldCentre[2] - eye[2];

		return dx * dx + dy * dy + dz * dz < bodyHeadRadius * bodyHeadRadius;
	}

	static bool IsBodyPartAtEye(IDirect3DDevice9* device, const unsigned char* packedVertices, std::uint16_t vertexCount,
		const D3DMATRIX& world)
	{
		if (!packedVertices || vertexCount == 0)
		{
			return false;
		}

		D3DMATRIX view = identity;
		device->GetTransform(D3DTS_VIEW, &view);

		float eye[3];

		for (int axis = 0; axis < 3; ++axis)
		{
			eye[axis] = -(view._41 * view.m[axis][0] + view._42 * view.m[axis][1] + view._43 * view.m[axis][2]);
		}

		__try
		{
			return IsCentreNearEyeUnsafe(packedVertices, vertexCount, world, eye);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static bool IsBlockCompressed(D3DFORMAT format)
	{
		switch (static_cast<DWORD>(format))
		{
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
		case MAKEFOURCC('A', 'T', 'I', '1'):
		case MAKEFOURCC('A', 'T', 'I', '2'):
			return true;
		default:
			return false;
		}
	}

	static bool TryCopyLevel(IDirect3DTexture9* source, IDirect3DTexture9* twin, UINT level, bool shouldMark,
		std::uint64_t& hash)
	{
		D3DSURFACE_DESC description{};

		if (FAILED(source->GetLevelDesc(level, &description)))
		{
			return false;
		}

		UINT rows = description.Height;

		if (IsBlockCompressed(description.Format))
		{
			rows = std::max(1u, (description.Height + 3) / 4);
		}

		D3DLOCKED_RECT sourceRect{};
		D3DLOCKED_RECT twinRect{};

		if (FAILED(source->LockRect(level, &sourceRect, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		if (FAILED(twin->LockRect(level, &twinRect, nullptr, 0)))
		{
			source->UnlockRect(level);
			return false;
		}

		const UINT rowBytes = static_cast<UINT>(std::min(sourceRect.Pitch, twinRect.Pitch));
		auto* twinBits = static_cast<unsigned char*>(twinRect.pBits);

		for (UINT row = 0; row < rows; ++row)
		{
			std::memcpy(twinBits + static_cast<std::size_t>(row) * twinRect.Pitch,
				static_cast<const unsigned char*>(sourceRect.pBits) + static_cast<std::size_t>(row) * sourceRect.Pitch,
				rowBytes);
		}

		bool didHash = true;

		if (shouldMark)
		{
			twinBits[static_cast<std::size_t>(rows - 1) * twinRect.Pitch + rowBytes - 1] ^= 0x01;
			didHash = Device::TryHashTopLevel(twinBits, description, hash);
		}

		twin->UnlockRect(level);
		source->UnlockRect(level);

		return didHash;
	}

	static IDirect3DTexture9* TryBuildBodyTwin(IDirect3DDevice9* device, IDirect3DTexture9* source, std::uint64_t& hash)
	{
		D3DSURFACE_DESC description{};

		if (FAILED(source->GetLevelDesc(0, &description)))
		{
			return nullptr;
		}

		const DWORD levels = source->GetLevelCount();
		IDirect3DTexture9* twin = nullptr;

		if (FAILED(device->CreateTexture(description.Width, description.Height, levels, 0, description.Format,
				D3DPOOL_MANAGED, &twin, nullptr)))
		{
			return nullptr;
		}

		for (DWORD level = 0; level < levels; ++level)
		{
			if (!TryCopyLevel(source, twin, level, level == 0, hash))
			{
				twin->Release();
				return nullptr;
			}
		}

		return twin;
	}

	static IDirect3DTexture9* TryGetBodyTwin(IDirect3DDevice9* device, std::uintptr_t material)
	{
		const auto found = bodyTwins.find(material);

		if (found != bodyTwins.end())
		{
			return found->second;
		}

		bodyTwins[material] = nullptr;

		IDirect3DBaseTexture9* bound = nullptr;
		device->GetTexture(0, &bound);

		if (!bound)
		{
			return nullptr;
		}

		IDirect3DTexture9* twin = nullptr;
		std::uint64_t twinHash = 0;

		if (bound->GetType() == D3DRTYPE_TEXTURE)
		{
			twin = TryBuildBodyTwin(device, static_cast<IDirect3DTexture9*>(bound), twinHash);
		}

		bound->Release();

		if (!twin)
		{
			return nullptr;
		}

		const remixapi_Interface* api = TryGetApi(device);

		if (api)
		{
			char text[24];
			std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(twinHash));

			if (bodyTwinHashList.find(text) == std::string::npos)
			{
				if (!bodyTwinHashList.empty())
				{
					bodyTwinHashList += ", ";
				}

				bodyTwinHashList += text;
			}

			api->SetConfigVariable("rtx.playerModelTextures", bodyTwinHashList.c_str());
			Device::NoteFrameEvent("remix option rtx.playerModelTextures");
		}

		const std::uintptr_t colourImage = FindColorMapImage(material);
		std::uint64_t colourHash = 0;

		if (colourImage && Device::TryGetTextureHash(ReadAt<const void*>(colourImage + Iw4::gfxImageTexture), colourHash))
		{
			RegisterPbrAlias(twinHash, colourHash);
		}

		bodyTwins[material] = twin;
		return twin;
	}

	static IDirect3DTexture9* TryGetLayerTwin(IDirect3DDevice9* device, std::uintptr_t material, IDirect3DTexture9* colorMap)
	{
		const std::uintptr_t colourImage = FindColorMapImage(material);
		std::uint64_t colourHash = 0;

		if (!colourImage
			|| !Device::TryGetTextureHash(ReadAt<const void*>(colourImage + Iw4::gfxImageTexture), colourHash)
			|| !IsSharedLayerTexture(colourHash))
		{
			return nullptr;
		}

		const auto found = layerTwins.find(colourHash);

		if (found != layerTwins.end())
		{
			return found->second;
		}

		layerTwins[colourHash] = nullptr;

		std::uint64_t twinHash = 0;
		IDirect3DTexture9* twin = TryBuildBodyTwin(device, colorMap, twinHash);

		if (!twin)
		{
			return nullptr;
		}

		TagGeneratedTexture(device, twinHash, TextureTag::Decal);
		RegisterPbrAlias(twinHash, colourHash);
		layerTwins[colourHash] = twin;
		++stats.layerTwins;
		return twin;
	}

	static void UpdateBodyTexture(IDirect3DDevice9* device, BodyTextureSwap& swap, bool isLocalBody, std::uintptr_t material)
	{
		if (isLocalBody == swap.isBodyBound)
		{
			return;
		}

		if (!isLocalBody)
		{
			device->SetTexture(0, swap.groupTexture);
			swap.isBodyBound = false;
			return;
		}

		IDirect3DTexture9* twin = TryGetBodyTwin(device, material);

		if (!twin)
		{
			return;
		}

		if (!swap.groupTexture)
		{
			device->GetTexture(0, &swap.groupTexture);
		}

		device->SetTexture(0, twin);
		swap.isBodyBound = true;
	}

	static void EndBodyTexture(IDirect3DDevice9* device, BodyTextureSwap& swap)
	{
		if (swap.isBodyBound)
		{
			device->SetTexture(0, swap.groupTexture);
		}

		if (swap.groupTexture)
		{
			swap.groupTexture->Release();
		}
	}

	static void DrawModelRecords(IDirect3DDevice9* device, void* context, void** iterator, bool isSinglePart)
	{
		const auto contextAddress = reinterpret_cast<std::uintptr_t>(context);
		const auto source = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsSource);
		const auto state = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsState);
		const auto groupMaterial = ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);
		BodyTextureSwap bodySwap;
		const auto info = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsInfo);
		const auto backEnd = ReadAt<std::uintptr_t>(source + Iw4::sourceBackEndData);
		const int cameraView = ReadAt<int>(info + Iw4::drawListInfoCameraView);

		const auto* cursor = static_cast<const std::uint64_t*>(iterator[0]);
		const auto* end = static_cast<const std::uint64_t*>(iterator[1]);
		const std::uint64_t group = cursor[0] & Engine::CurrentDrawSurfLayout().groupMask;

		do
		{
			const std::uint64_t drawSurf = cursor[0];
			const std::uintptr_t record = backEnd + Iw4::backEndBModelSurfaces
				+ 4 * (drawSurf & Iw4::drawSurfObjectMask);

			changeDepthRange(reinterpret_cast<void*>(state), EntityDepthRange(backEnd, record, drawSurf, cameraView));

			const bool hasEntity = ((drawSurf >> Engine::CurrentDrawSurfLayout().hasEntityBit) & 1) != 0;
			const bool isLocalBody = IsLocalBodyRecord(backEnd, record, hasEntity);
			UpdateBodyTexture(device, bodySwap, isLocalBody, groupMaterial);

			const auto surface = ReadAt<std::uintptr_t>(record + Iw4::modelSurfaceXSurface);
			const ModelBuffers* buffers = nullptr;

			if (surface)
			{
				buffers = TryGetModelBuffers(device, surface);
			}

			if (buffers)
			{
				int partCount = 1;

				if (!isSinglePart)
				{
					partCount = -3 - ReadAt<int>(record + Iw4::modelSurfacePartCount);
				}

				const float scale = ReadAt<float>(record + Iw4::modelSurfaceScale);

				device->SetStreamSource(0, buffers->vertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
				device->SetIndices(buffers->indexBuffer);

				for (int part = 0; part < partCount; ++part)
				{
					const std::uintptr_t placement = record + Iw4::modelSurfaceParts + part * Iw4::modelPartStride;
					UINT triOffset = 0;
					UINT triCount = buffers->triCount;

					if (!isSinglePart)
					{
						triOffset = ReadAt<std::uint16_t>(placement + Iw4::modelPartTriOffset);
						triCount = ReadAt<std::uint16_t>(placement + Iw4::modelPartTriCount);
					}

					if (triCount == 0 || triOffset + triCount > buffers->triCount)
					{
						continue;
					}

					D3DMATRIX world = PlacementMatrix(placement);

					for (int row = 0; row < 3; ++row)
					{
						for (int column = 0; column < 3; ++column)
						{
							world.m[row][column] *= scale;
						}
					}

					if (isPulledTowardEye)
					{
						PullTowardEye(device, world);
					}

					device->SetTransform(D3DTS_WORLD, &world);

					if (isLocalBody && IsBodyPartAtEye(device, ReadAt<const unsigned char*>(surface + Iw4::xSurfaceVerts0),
							buffers->vertexCount, world))
					{
						continue;
					}

					if (reticle.isActive)
					{
						if (!isLocalBody)
						{
							DrawReticleVertices(device, surface, *buffers, 3 * triOffset, triCount);
						}

						continue;
					}

					device->SetStreamSource(0, buffers->vertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
					device->SetIndices(buffers->indexBuffer);
					device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, buffers->vertexCount, 3 * triOffset, triCount);
					++groupDraws;
				}
			}

			++cursor;
		}
		while (cursor != end && (cursor[0] & Engine::CurrentDrawSurfLayout().groupMask) == group);

		EndBodyTexture(device, bodySwap);
		iterator[0] = const_cast<std::uint64_t*>(cursor);
	}

	static void DrawRigidModelGroup(IDirect3DDevice9* device, void* context, void** iterator)
	{
		DrawModelRecords(device, context, iterator, false);
	}

	static int SkinnedDepthRange(std::uintptr_t backEnd, std::uintptr_t record, int cameraView)
	{
		if (cameraView == 0)
		{
			return -1;
		}

		const auto gfxEntity = ReadAt<std::uint16_t>(record + Iw4::modelSurfaceGfxEntity);

		if (gfxEntity == 0)
		{
			return 0;
		}

		const auto flags = ReadAt<std::uint32_t>(backEnd + Iw4::backEndGfxEntities
			+ gfxEntity * Iw4::gfxEntityStride);

		return static_cast<int>(flags & Iw4::gfxEntityDepthHack);
	}

	static WorldVertex* TryLockSkinnedVertices(IDirect3DDevice9* device, std::uint16_t vertexCount, UINT& offset)
	{
		if (!skinnedVertexBuffer)
		{
			if (FAILED(device->CreateVertexBuffer(skinnedRingBytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
					worldVertexFormat, D3DPOOL_DEFAULT, &skinnedVertexBuffer, nullptr)))
			{
				skinnedVertexBuffer = nullptr;
				return nullptr;
			}

			skinnedRingOffset = 0;
		}

		const UINT bytes = vertexCount * static_cast<UINT>(sizeof(WorldVertex));

		if (bytes > skinnedRingBytes)
		{
			return nullptr;
		}

		DWORD lockFlags = D3DLOCK_NOOVERWRITE;

		if (skinnedRingOffset + bytes > skinnedRingBytes)
		{
			skinnedRingOffset = 0;
			lockFlags = D3DLOCK_DISCARD;
		}

		void* data = nullptr;

		if (FAILED(skinnedVertexBuffer->Lock(skinnedRingOffset, bytes, &data, lockFlags)))
		{
			return nullptr;
		}

		offset = skinnedRingOffset;
		skinnedRingOffset += bytes;

		return static_cast<WorldVertex*>(data);
	}

	static void DrawSkinnedSurface(IDirect3DDevice9* device, std::uintptr_t record, bool isLocalBody)
	{
		if (ReadAt<int>(record + Iw4::modelSurfaceSkinnedCachedOffset) >= 0)
		{
			++stats.cachedSkinnedSurfaces;
			return;
		}

		const auto surface = ReadAt<std::uintptr_t>(record + Iw4::modelSurfaceXSurface);
		const auto* packedVertices = ReadAt<const unsigned char*>(record + Iw4::modelSurfaceSkinnedVertices);

		if (!surface || !packedVertices)
		{
			return;
		}

		const ModelBuffers* buffers = TryGetModelBuffers(device, surface);

		if (!buffers)
		{
			return;
		}

		if (isLocalBody)
		{
			D3DMATRIX world = identity;
			device->GetTransform(D3DTS_WORLD, &world);

			if (IsBodyPartAtEye(device, packedVertices, buffers->vertexCount, world))
			{
				return;
			}
		}

		UINT offset = 0;
		WorldVertex* vertices = TryLockSkinnedVertices(device, buffers->vertexCount, offset);

		if (!vertices)
		{
			return;
		}

		bool didConvert = TryConvertSkinnedVertices(packedVertices, buffers->vertexCount, vertices);

		if (didConvert && reticle.isActive)
		{
			D3DMATRIX world = identity;
			D3DMATRIX view = identity;
			device->GetTransform(D3DTS_WORLD, &world);
			device->GetTransform(D3DTS_VIEW, &view);
			didConvert = TryProjectReticle(packedVertices, buffers->vertexCount, world, view, vertices);
		}

		skinnedVertexBuffer->Unlock();

		if (!didConvert)
		{
			return;
		}

		if (reticle.isActive)
		{
			D3DMATRIX world = identity;
			D3DMATRIX view = identity;
			device->GetTransform(D3DTS_WORLD, &world);
			device->GetTransform(D3DTS_VIEW, &view);

			if (TryDrawReticleDots(device, packedVertices, buffers->vertexCount, world, view))
			{
				return;
			}
		}

		device->SetStreamSource(0, skinnedVertexBuffer, offset, static_cast<UINT>(sizeof(WorldVertex)));
		device->SetIndices(buffers->indexBuffer);
		device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, buffers->vertexCount, 0, buffers->triCount);
		++groupDraws;
	}

	static void DrawSkinnedModelGroup(IDirect3DDevice9* device, void* context, void** iterator)
	{
		const auto contextAddress = reinterpret_cast<std::uintptr_t>(context);
		const auto source = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsSource);
		const auto state = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsState);
		const auto info = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsInfo);
		const auto backEnd = ReadAt<std::uintptr_t>(source + Iw4::sourceBackEndData);
		const int cameraView = ReadAt<int>(info + Iw4::drawListInfoCameraView);

		const auto groupMaterial = ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);
		BodyTextureSwap bodySwap;

		D3DMATRIX world = PlacementMatrix(source + Iw4::sourceSkinnedPlacement);

		if (isPulledTowardEye)
		{
			PullTowardEye(device, world);
		}

		device->SetTransform(D3DTS_WORLD, &world);

		const auto* cursor = static_cast<const std::uint64_t*>(iterator[0]);
		const auto* end = static_cast<const std::uint64_t*>(iterator[1]);
		const std::uint64_t group = cursor[0] & Engine::CurrentDrawSurfLayout().groupMask;

		do
		{
			const std::uintptr_t record = backEnd + Iw4::backEndBModelSurfaces
				+ 4 * (cursor[0] & Iw4::drawSurfObjectMask);

			changeDepthRange(reinterpret_cast<void*>(state), SkinnedDepthRange(backEnd, record, cameraView));
			const bool isLocalBody = IsLocalBodyRecord(backEnd, record, true);
			UpdateBodyTexture(device, bodySwap, isLocalBody, groupMaterial);
			DrawSkinnedSurface(device, record, isLocalBody);

			++cursor;
		}
		while (cursor != end && (cursor[0] & Engine::CurrentDrawSurfLayout().groupMask) == group);

		EndBodyTexture(device, bodySwap);
		iterator[0] = const_cast<std::uint64_t*>(cursor);
	}

	static void DrawRigidSkinnedModelGroup(IDirect3DDevice9* device, void* context, void** iterator)
	{
		DrawModelRecords(device, context, iterator, true);
	}

	static void SkipBrushModelGroup(void** iterator)
	{
		const auto* cursor = static_cast<const std::uint64_t*>(iterator[0]);
		const auto* end = static_cast<const std::uint64_t*>(iterator[1]);
		const std::uint64_t group = cursor[0] & Engine::CurrentDrawSurfLayout().groupMask;

		do
		{
			++cursor;
		}
		while (cursor != end && (cursor[0] & Engine::CurrentDrawSurfLayout().groupMask) == group);

		iterator[0] = const_cast<std::uint64_t*>(cursor);
	}

	static std::uintptr_t FindTextureDef(std::uintptr_t material, std::uint32_t nameHash)
	{
		const auto textureCount = ReadAt<std::uint8_t>(material + Iw4::materialTextureCount);
		const auto textures = ReadAt<std::uintptr_t>(material + Iw4::materialTextureTable);

		if (!textures)
		{
			return 0;
		}

		for (std::uint8_t i = 0; i < textureCount; ++i)
		{
			const std::uintptr_t definition = textures + i * Iw4::textureDefStride;

			if (ReadAt<std::uint32_t>(definition + Iw4::textureDefNameHash) == nameHash)
			{
				return definition;
			}
		}

		return 0;
	}

	static std::uintptr_t FindConstantDef(std::uintptr_t material, std::uint32_t nameHash)
	{
		const auto constantCount = ReadAt<std::uint8_t>(material + Iw4::materialConstantCount);
		const auto constants = ReadAt<std::uintptr_t>(material + Iw4::materialConstantTable);

		if (!constants)
		{
			return 0;
		}

		for (std::uint8_t i = 0; i < constantCount; ++i)
		{
			const std::uintptr_t constant = constants + i * Iw4::constantDefStride;

			if (ReadAt<std::uint32_t>(constant + Iw4::constantDefNameHash) == nameHash)
			{
				return constant;
			}
		}

		return 0;
	}

	static std::uintptr_t ImageOf2DTextureDef(std::uintptr_t definition)
	{
		if (ReadAt<std::uint8_t>(definition + Iw4::textureDefSemantic) == Iw4::textureSemanticWater)
		{
			return 0;
		}

		const auto image = ReadAt<std::uintptr_t>(definition + Iw4::textureDefImage);

		if (!image || ReadAt<std::uint8_t>(image + Iw4::gfxImageMapType) != Iw4::gfxImageMapType2D)
		{
			return 0;
		}

		return image;
	}

	static std::uintptr_t FindColorMapImage(std::uintptr_t material)
	{
		const std::uintptr_t definition = FindTextureDef(material, Iw4::colorMapNameHash);

		if (!definition)
		{
			return 0;
		}

		return ImageOf2DTextureDef(definition);
	}

	static bool IsDecalColorMap(std::uintptr_t material)
	{
		const std::uintptr_t image = FindColorMapImage(material);
		std::uint64_t hash = 0;

		if (!image || !Device::TryGetTextureHash(ReadAt<const void*>(image + Iw4::gfxImageTexture), hash))
		{
			return false;
		}

		return IsDecalTexture(hash);
	}

	static IDirect3DTexture9* TryGetColorMap(std::uintptr_t material)
	{
		const std::uintptr_t image = FindColorMapImage(material);

		if (!image)
		{
			return nullptr;
		}

		return Device::Unwrap(ReadAt<IDirect3DTexture9*>(image + Iw4::gfxImageTexture));
	}

	static std::uint32_t MixBits(std::uint32_t value)
	{
		value ^= value >> 16;
		value *= 0x7FEB352Du;
		value ^= value >> 15;
		value *= 0x846CA68Bu;
		value ^= value >> 16;
		return value;
	}

	static float UnitOf(std::uint32_t value)
	{
		return static_cast<float>(value & 0xFFFFFF) / 16777215.0f;
	}

	static void HueToRgb(float hue, float* rgb)
	{
		static constexpr float offsets[3] = { 0.0f, 2.0f / 3.0f, 1.0f / 3.0f };

		for (int channel = 0; channel < 3; ++channel)
		{
			const float shifted = hue + offsets[channel];
			const float wrapped = shifted - std::floor(shifted);
			rgb[channel] = std::clamp(std::fabs(wrapped * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
		}
	}

	static bool IsAlienActive()
	{
		return GetAlienGlow().isEnabled && IsNeonFoliage();
	}

	static bool TryGetMaterialColourHash(std::uintptr_t material, std::uint64_t& hash)
	{
		const std::uintptr_t image = FindColorMapImage(material);

		return image && Device::TryGetTextureHash(ReadAt<const void*>(image + Iw4::gfxImageTexture), hash);
	}

	static bool IsAlienFoliageMaterial(std::uintptr_t material)
	{
		std::uint64_t hash = 0;

		return IsAlienActive() && TryGetMaterialColourHash(material, hash) && IsNeonFoliageHash(hash);
	}

	static D3DCOLOR AlienColour(std::uint32_t seed, const float* position)
	{
		const AlienGlow glow = GetAlienGlow();
		const double seconds = static_cast<double>(GetTickCount64() % 3600000ull) / 1000.0;
		const std::uint32_t mixed = MixBits(seed);
		float phase = UnitOf(mixed);
		float spread = 0.7f + 0.6f * UnitOf(MixBits(mixed));

		if (glow.style == 1 && position)
		{
			phase = (position[0] * 0.7f + position[1] * 0.5f) * 0.002f * glow.scale + 0.15f * phase;
			spread = 1.0f;
		}

		const double cycles = seconds * glow.speed * spread + phase;
		const float wave = 0.5f + 0.5f * static_cast<float>(std::sin(cycles * 6.283185307179586));
		const float level = 1.0f - glow.depth * (1.0f - wave);
		float rgb[3] = { 1.0f, 1.0f, 1.0f };

		if (glow.colourMode == 1)
		{
			rgb[0] = glow.tint[0];
			rgb[1] = glow.tint[1];
			rgb[2] = glow.tint[2];
		}
		else if (glow.colourMode == 2)
		{
			HueToRgb(UnitOf(MixBits(seed ^ alienColourSeed)), rgb);
		}
		else if (glow.colourMode == 3)
		{
			const double hue = seconds * glow.speed * 0.1 + phase;
			HueToRgb(static_cast<float>(hue - std::floor(hue)), rgb);
		}

		const auto pack = [level](float channel)
		{
			return static_cast<D3DCOLOR>(std::clamp(channel * level, 0.0f, 1.0f) * 255.0f + 0.5f);
		};

		return D3DCOLOR_ARGB(255, pack(rgb[0]), pack(rgb[1]), pack(rgb[2]));
	}

	static bool TryGetFoliageGroupColour(std::uintptr_t material, D3DCOLOR& colour)
	{
		std::uint64_t hash = 0;

		if (IsAlienActive() && TryGetMaterialColourHash(material, hash) && IsNeonFoliageHash(hash))
		{
			colour = AlienColour(static_cast<std::uint32_t>(hash ^ (hash >> 32)), nullptr);
			return true;
		}

		return false;
	}

	static bool TryGetNeonPulseColour(std::uintptr_t material, D3DCOLOR& colour)
	{
		if (TryGetFoliageGroupColour(material, colour))
		{
			return true;
		}

		const std::uintptr_t image = FindColorMapImage(material);
		std::uint64_t hash = 0;

		if (!image || !Device::TryGetTextureHash(ReadAt<const void*>(image + Iw4::gfxImageTexture), hash))
		{
			return false;
		}

		float factor = 1.0f;

		if (!TryGetNeonPulseFactor(hash, factor))
		{
			return false;
		}

		const int level = static_cast<int>(std::clamp(factor, 0.0f, 1.0f) * 255.0f + 0.5f);
		colour = D3DCOLOR_ARGB(255, level, level, level);
		return true;
	}

	IDirect3DTexture9* GetMaterialColorMap(std::uintptr_t material)
	{
		if (!material)
		{
			return nullptr;
		}

		return TryGetColorMap(material);
	}

	static void CopyName(char* destination, std::size_t capacity, const char* source)
	{
		destination[0] = '\0';

		if (!source)
		{
			return;
		}

		std::snprintf(destination, capacity, "%s", source);
	}

	static DumpRow CaptureDumpRow(IDirect3DDevice9* device, std::uintptr_t material, const char* kind)
	{
		DumpRow row{};
		row.kind = kind;

		CopyName(row.material, sizeof(row.material), ReadAt<const char*>(material));

		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (techniqueSet)
		{
			CopyName(row.techniqueSet, sizeof(row.techniqueSet), ReadAt<const char*>(techniqueSet));
		}

		const std::uintptr_t image = FindColorMapImage(material);

		if (image)
		{
			CopyName(row.image, sizeof(row.image), ReadAt<const char*>(image + Iw4::gfxImageName));
			Device::TryGetTextureHash(ReadAt<const void*>(image + Iw4::gfxImageTexture), row.imageHash);
		}

		D3DVIEWPORT9 viewport{};
		device->GetViewport(&viewport);
		row.minZ = viewport.MinZ;
		row.maxZ = viewport.MaxZ;

		device->GetRenderState(D3DRS_ALPHABLENDENABLE, &row.isBlending);
		device->GetRenderState(D3DRS_SRCBLEND, &row.sourceBlend);
		device->GetRenderState(D3DRS_DESTBLEND, &row.destinationBlend);
		device->GetRenderState(D3DRS_ALPHATESTENABLE, &row.isAlphaTested);
		device->GetRenderState(D3DRS_ALPHAREF, &row.alphaReference);
		device->GetRenderState(D3DRS_ALPHAFUNC, &row.alphaFunction);
		device->GetRenderState(D3DRS_CULLMODE, &row.cullMode);

		return row;
	}

	static void RecordEngineGroup(void* context, const char* kind)
	{
		if (!isDumping)
		{
			return;
		}

		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);
		dumpRows.push_back(CaptureDumpRow(sceneDevice, ReadAt<std::uintptr_t>(state + Iw4::stateMaterial), kind));
	}

	static void WriteSceneDump()
	{
		++dumpIndex;

		const std::wstring fileName = L"zw3_remix_frame_" + std::to_wstring(dumpIndex) + L".txt";
		std::ofstream file(Core::ModuleDirectory() + fileName, std::ios::trunc);

		if (!file)
		{
			return;
		}

		file << "kind\tdraws\tmaterial\ttechset\timage\thash\tblend\tsrc\tdst\talphatest\tref\tfunc\tcull\tminz\tmaxz\n";

		for (const DumpRow& row : dumpRows)
		{
			char hash[24];
			std::snprintf(hash, sizeof(hash), "0x%016llX", static_cast<unsigned long long>(row.imageHash));

			file << row.kind << "\t" << row.draws << "\t" << row.material << "\t" << row.techniqueSet << "\t"
				 << row.image << "\t" << hash << "\t" << row.isBlending << "\t" << row.sourceBlend << "\t"
				 << row.destinationBlend << "\t" << row.isAlphaTested << "\t" << row.alphaReference << "\t"
				 << row.alphaFunction << "\t" << row.cullMode << "\t" << row.minZ << "\t" << row.maxZ << "\n";
		}

		Log() << "remix: wrote " << dumpRows.size() << " draw groups of one scene frame to zw3_remix_frame_"
			  << dumpIndex << ".txt";
	}

	static bool TryReadTextureScroll(std::uintptr_t source, std::uintptr_t material, D3DMATRIX& transform)
	{
		const std::uintptr_t constant = FindConstantDef(material, Iw4::uvAnimParmsNameHash);

		if (!constant)
		{
			return false;
		}

		const float gameTime = std::fmod(ReadAt<float>(source + Iw4::sourceFloatTime), Iw4::gameTimeWrap);
		const float scrollU = ReadAt<float>(constant + Iw4::constantDefLiteral) * gameTime;
		const float scrollV = ReadAt<float>(constant + Iw4::constantDefLiteral + sizeof(float)) * gameTime;

		if (scrollU == 0.0f && scrollV == 0.0f)
		{
			return false;
		}

		transform = identity;
		transform._31 = scrollU - std::floor(scrollU);
		transform._32 = scrollV - std::floor(scrollV);

		return true;
	}

	static bool IsReflexSightMaterial(std::uintptr_t material)
	{
		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (!techniqueSet)
		{
			return false;
		}

		const char* techniqueSetName = ReadAt<const char*>(techniqueSet);

		return techniqueSetName && std::strstr(techniqueSetName, "mc_reflexsight");
	}

	static bool IsCamoMaterial(std::uintptr_t material)
	{
		const char* name = ReadAt<const char*>(material);
		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (!name || !techniqueSet)
		{
			return false;
		}

		const char* techniqueSetName = ReadAt<const char*>(techniqueSet);

		return techniqueSetName && std::strstr(name, "mtl_weapon_") && std::strstr(techniqueSetName, "r0c0d0");
	}

	static IDirect3DTexture9* TryBuildCamo(IDirect3DDevice9* device, std::uintptr_t material, IDirect3DTexture9* colorMap)
	{
		const std::uintptr_t colorImage = FindColorMapImage(material);
		const std::uintptr_t detailDefinition = FindTextureDef(material, Iw4::detailMapNameHash);
		const std::uintptr_t detailScale = FindConstantDef(material, Iw4::detailScaleNameHash);

		if (!colorImage || !detailDefinition || !detailScale)
		{
			return nullptr;
		}

		if (ReadAt<std::uint8_t>(detailDefinition + Iw4::textureDefSemantic) != Iw4::textureSemanticDetail)
		{
			return nullptr;
		}

		const std::uintptr_t detailImage = ImageOf2DTextureDef(detailDefinition);

		if (!detailImage)
		{
			return nullptr;
		}

		const bool isLinear = ReadAt<std::uint8_t>(colorImage + Iw4::gfxImageUseSrgbReads) == 0
			&& ReadAt<std::uint8_t>(detailImage + Iw4::gfxImageUseSrgbReads) == 0;
		IDirect3DTexture9* detailMap = Device::Unwrap(ReadAt<IDirect3DTexture9*>(detailImage + Iw4::gfxImageTexture));

		if (!isLinear || !detailMap)
		{
			return nullptr;
		}

		CamoScale scale{};
		scale.u = ReadAt<float>(detailScale + Iw4::constantDefLiteral);
		scale.v = ReadAt<float>(detailScale + Iw4::constantDefLiteral + sizeof(float));

		std::uint64_t compositeHash = 0;
		IDirect3DTexture9* composite = TryBuildCamoTexture(device, colorMap, detailMap, scale, compositeHash);
		std::uint64_t colourHash = 0;

		if (composite && compositeHash != 0
			&& Device::TryGetTextureHash(ReadAt<const void*>(colorImage + Iw4::gfxImageTexture), colourHash))
		{
			RegisterPbrAlias(compositeHash, colourHash);
		}

		return composite;
	}

	static IDirect3DTexture9* TryGetCamoColorMap(IDirect3DDevice9* device, std::uintptr_t material,
		IDirect3DTexture9* colorMap)
	{
		const auto found = camoTextures.find(material);

		if (found != camoTextures.end())
		{
			return found->second;
		}

		if (!IsCamoMaterial(material))
		{
			camoTextures[material] = nullptr;
			return nullptr;
		}

		if (didBuildCamoThisView)
		{
			return nullptr;
		}

		didBuildCamoThisView = true;

		IDirect3DTexture9* camo = TryBuildCamo(device, material, colorMap);
		camoTextures[material] = camo;

		if (camo)
		{
			++stats.camoTextures;
		}

		return camo;
	}

	static IDirect3DTexture9* TryGetReticleColorMap(IDirect3DDevice9* device, std::uintptr_t material,
		IDirect3DTexture9* colorMap)
	{
		const auto found = reticleTextures.find(material);

		if (found != reticleTextures.end())
		{
			return found->second;
		}

		IDirect3DTexture9* baked = nullptr;
		const std::uintptr_t detailDefinition = FindTextureDef(material, Iw4::detailMapNameHash);

		if (detailDefinition && colorMap)
		{
			const std::uintptr_t detailImage = ImageOf2DTextureDef(detailDefinition);
			IDirect3DTexture9* detailMap = nullptr;

			if (detailImage)
			{
				detailMap = Device::Unwrap(ReadAt<IDirect3DTexture9*>(detailImage + Iw4::gfxImageTexture));
			}

			std::uint64_t hash = 0;

			if (detailMap)
			{
				baked = TryBuildReticleTexture(device, colorMap, detailMap, hash);
			}

			if (baked)
			{
				WithdrawGeneratedTag(device, hash, TextureTag::WorldSpaceUi);
				ExcludeFromOpacityMicromaps(device, hash);
				RegisterPbrEmissive(hash, baked, reticleGlowIntensity);
				ReadReticleShape(baked, reticleShapes[material]);
			}
		}

		reticleTextures[material] = baked;

		const char* materialName = ReadAt<const char*>(material);
		const std::uintptr_t colourImage = FindColorMapImage(material);
		const char* colourName = "none";
		const char* detailName = "none";

		if (!materialName)
		{
			materialName = "?";
		}

		if (colourImage && ReadAt<const char*>(colourImage + Iw4::gfxImageName))
		{
			colourName = ReadAt<const char*>(colourImage + Iw4::gfxImageName);
		}

		std::uintptr_t detailImage = 0;

		if (detailDefinition)
		{
			detailImage = ImageOf2DTextureDef(detailDefinition);
		}

		if (detailImage && ReadAt<const char*>(detailImage + Iw4::gfxImageName))
		{
			detailName = ReadAt<const char*>(detailImage + Iw4::gfxImageName);
		}

		Log() << "remix: reticle material " << materialName << ", colour map " << colourName << " (bound "
			  << (colorMap != nullptr) << "), detail map " << detailName << ", baked " << (baked != nullptr) << ", scale "
			  << reticle.scaleU << " " << reticle.scaleV;

		return baked;
	}

	static bool IsIlluminatedReticleMaterial(std::uintptr_t material)
	{
		const char* name = ReadAt<const char*>(material);
		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);

		if (!name || !techniqueSet)
		{
			return false;
		}

		const char* techniqueSetName = ReadAt<const char*>(techniqueSet);

		if (!techniqueSetName || !std::strstr(techniqueSetName, "mc_effect_replace"))
		{
			return false;
		}

		const std::size_t length = std::strlen(name);

		return length > 8 && std::strcmp(name + length - 8, "_reticle") == 0;
	}

	static std::uintptr_t FindWaterTextureDef(std::uintptr_t material)
	{
		const auto textureCount = ReadAt<std::uint8_t>(material + Iw4::materialTextureCount);
		const auto textures = ReadAt<std::uintptr_t>(material + Iw4::materialTextureTable);

		if (!textures)
		{
			return 0;
		}

		for (std::uint8_t i = 0; i < textureCount; ++i)
		{
			const std::uintptr_t definition = textures + i * Iw4::textureDefStride;

			if (ReadAt<std::uint8_t>(definition + Iw4::textureDefSemantic) == Iw4::textureSemanticWater)
			{
				return definition;
			}
		}

		return 0;
	}

	static std::uint8_t ColourByte(float value)
	{
		return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
	}

	static IDirect3DTexture9* TryGetWaterColorMap(IDirect3DDevice9* device, std::uintptr_t material)
	{
		const auto found = waterTextures.find(material);

		if (found != waterTextures.end())
		{
			return found->second;
		}

		waterTextures[material] = nullptr;

		const std::uintptr_t waterDefinition = FindWaterTextureDef(material);

		if (!waterDefinition)
		{
			return nullptr;
		}

		float waterColour[4] = { 0.1f, 0.2f, 0.25f, 1.0f };
		const std::uintptr_t colourConstant = FindConstantDef(material, Iw4::waterColorNameHash);

		if (colourConstant)
		{
			std::memcpy(waterColour, reinterpret_cast<const void*>(colourConstant + Iw4::constantDefLiteral),
				sizeof(waterColour));
		}

		const std::uint32_t colour = (0xFFu << 24) | (static_cast<std::uint32_t>(ColourByte(waterColour[0])) << 16)
			| (static_cast<std::uint32_t>(ColourByte(waterColour[1])) << 8) | ColourByte(waterColour[2]);

		std::uint64_t hash = 0;
		IDirect3DTexture9* texture = TryBuildSolidTexture(device, colour, hash);

		if (!texture)
		{
			return nullptr;
		}

		IDirect3DTexture9* heightTexture = nullptr;
		const auto water = ReadAt<std::uintptr_t>(waterDefinition + Iw4::textureDefImage);

		if (water)
		{
			const auto waterImage = ReadAt<std::uintptr_t>(water + Iw4::waterImage);

			if (waterImage)
			{
				heightTexture = Device::Unwrap(ReadAt<IDirect3DTexture9*>(waterImage + Iw4::gfxImageTexture));
			}
		}

		TagGeneratedTexture(device, hash, TextureTag::Water);
		RegisterPbrWater(hash, heightTexture);

		const char* name = ReadAt<const char*>(material);

		if (!name)
		{
			name = "";
		}

		char hashText[24];
		std::snprintf(hashText, sizeof(hashText), "%016llX", static_cast<unsigned long long>(hash));

		Log() << "remix: water material " << name << " draws with its own colour key " << hashText
			  << " (water colour " << waterColour[0] << " " << waterColour[1] << " " << waterColour[2]
			  << ") instead of the shared case64blue placeholder";

		waterTextures[material] = texture;
		return texture;
	}

	static void ReleaseCamoTextures()
	{
		for (auto& entry : camoTextures)
		{
			if (entry.second)
			{
				entry.second->Release();
			}
		}

		camoTextures.clear();
		stats.camoTextures = 0;

		for (auto& entry : reticleTextures)
		{
			if (entry.second)
			{
				entry.second->Release();
			}
		}

		reticleTextures.clear();
		reticleShapes.clear();

		for (auto& entry : bodyTwins)
		{
			if (entry.second)
			{
				entry.second->Release();
			}
		}

		bodyTwins.clear();
		bodyTwinHashList.clear();

		for (auto& entry : layerTwins)
		{
			if (entry.second)
			{
				entry.second->Release();
			}
		}

		layerTwins.clear();
		reticle.shape = nullptr;

		if (reticleDotTexture)
		{
			reticleDotTexture->Release();
			reticleDotTexture = nullptr;
		}

		hasTriedReticleDotTexture = false;

		for (auto& entry : waterTextures)
		{
			if (entry.second)
			{
				entry.second->Release();
			}
		}

		waterTextures.clear();
	}

	static void DrawFixedFunction(void* context, void** iterator, GroupDrawer drawGroup, const char* kind)
	{
		IDirect3DDevice9* device = sceneDevice;

		ChangeDepthRangeLikeEngine(context);

		const auto contextAddress = reinterpret_cast<std::uintptr_t>(context);
		const auto source = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsSource);
		const auto state = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsState);
		const auto material = ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);

		DumpRow dumpRow{};

		if (isDumping)
		{
			dumpRow = CaptureDumpRow(device, material, kind);
		}

		const unsigned drawsBefore = groupDraws;

		IDirect3DVertexShader9* vertexShader = nullptr;
		IDirect3DPixelShader9* pixelShader = nullptr;
		IDirect3DVertexDeclaration9* declaration = nullptr;
		IDirect3DVertexBuffer9* stream = nullptr;
		IDirect3DIndexBuffer9* indexBuffer = nullptr;
		UINT streamOffset = 0;
		UINT streamStride = 0;
		DWORD lighting = TRUE;
		DWORD isBlending = FALSE;
		DWORD sourceBlend = D3DBLEND_ONE;
		DWORD destinationBlend = D3DBLEND_ZERO;
		DWORD sourceBlendAlpha = D3DBLEND_ONE;
		DWORD destinationBlendAlpha = D3DBLEND_ZERO;
		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;
		DWORD isAlphaTested = FALSE;
		DWORD alphaReference = 0;
		DWORD alphaFunction = D3DCMP_ALWAYS;
		DWORD colourOperation = D3DTOP_MODULATE;
		DWORD colourArgument1 = D3DTA_TEXTURE;
		DWORD colourArgument2 = D3DTA_CURRENT;
		DWORD textureFactor = 0xFFFFFFFF;
		DWORD addressU = D3DTADDRESS_WRAP;
		DWORD addressV = D3DTADDRESS_WRAP;
		DWORD textureTransformFlags = D3DTTFF_DISABLE;
		D3DMATRIX textureTransform = identity;
		IDirect3DBaseTexture9* boundTexture = nullptr;

		device->GetRenderState(D3DRS_LIGHTING, &lighting);
		device->GetRenderState(D3DRS_ALPHABLENDENABLE, &isBlending);
		device->GetRenderState(D3DRS_SRCBLEND, &sourceBlend);
		device->GetRenderState(D3DRS_DESTBLEND, &destinationBlend);
		device->GetRenderState(D3DRS_SRCBLENDALPHA, &sourceBlendAlpha);
		device->GetRenderState(D3DRS_DESTBLENDALPHA, &destinationBlendAlpha);
		device->GetRenderState(D3DRS_ALPHATESTENABLE, &isAlphaTested);
		device->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOperation);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArgument1);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &alphaArgument2);
		device->GetVertexShader(&vertexShader);
		device->GetPixelShader(&pixelShader);
		device->GetVertexDeclaration(&declaration);
		device->GetStreamSource(0, &stream, &streamOffset, &streamStride);
		device->GetIndices(&indexBuffer);
		device->GetTexture(0, &boundTexture);

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(worldVertexFormat);
		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);

		const auto reticleScale = FindConstantDef(material, Iw4::detailScaleNameHash);
		const bool isReticle = IsReflexSightMaterial(material) && reticleScale != 0;

		const bool isIlluminatedLayer = !isReticle && isBlending && IsIlluminatedReticleMaterial(material);

		const bool isPremultipliedBlend = !isReticle && !isIlluminatedLayer && isBlending && sourceBlend == D3DBLEND_ONE
			&& destinationBlend == D3DBLEND_INVSRCALPHA;

		if (isPremultipliedBlend)
		{
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
			++stats.blendLayerLists;
		}

		const bool isAdditiveLayer = !isReticle && !isIlluminatedLayer && isBlending && destinationBlend == D3DBLEND_ONE
			&& (sourceBlend == D3DBLEND_ONE || sourceBlend == D3DBLEND_INVDESTCOLOR);

		if (isAdditiveLayer)
		{
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		}

		const bool isCutoutDecal = !isBlending && isAlphaTested && IsDecalColorMap(material);

		if (isCutoutDecal)
		{
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
			++stats.cutoutDecalLists;
		}

		if ((isBlending || isCutoutDecal) && blendedIndexBuffer)
		{
			device->SetIndices(blendedIndexBuffer);
		}
		else
		{
			device->SetIndices(worldIndexBuffer);
		}
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		if (isBlending)
		{
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		}
		else
		{
			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		}

		if (isIlluminatedLayer)
		{
			isPulledTowardEye = true;
			device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		}

		IDirect3DTexture9* colorMap = TryGetColorMap(material);
		IDirect3DTexture9* reticleColorMap = nullptr;

		if (isReticle)
		{
			reticle.scaleU = ReadAt<float>(reticleScale + Iw4::constantDefLiteral);
			reticle.scaleV = ReadAt<float>(reticleScale + Iw4::constantDefLiteral + sizeof(float));
			reticle.isActive = true;
			isPulledTowardEye = true;
			reticleColorMap = TryGetReticleColorMap(device, material, colorMap);

			const auto shape = reticleShapes.find(material);

			if (reticleColorMap && shape != reticleShapes.end())
			{
				reticle.shape = &shape->second;
			}

			device->GetTextureStageState(0, D3DTSS_COLOROP, &colourOperation);
			device->GetTextureStageState(0, D3DTSS_COLORARG1, &colourArgument1);
			device->GetTextureStageState(0, D3DTSS_COLORARG2, &colourArgument2);
			device->GetRenderState(D3DRS_TEXTUREFACTOR, &textureFactor);
			device->GetRenderState(D3DRS_ALPHAREF, &alphaReference);
			device->GetRenderState(D3DRS_ALPHAFUNC, &alphaFunction);
			device->GetSamplerState(0, D3DSAMP_ADDRESSU, &addressU);
			device->GetSamplerState(0, D3DSAMP_ADDRESSV, &addressV);

			device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

			if (reticleColorMap)
			{
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
				device->SetRenderState(D3DRS_ALPHAREF, 0x80);
				device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			}
			else
			{
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
				device->SetRenderState(D3DRS_TEXTUREFACTOR, reticleColour);
			}
		}

		IDirect3DTexture9* waterColorMap = nullptr;

		if (!reticleColorMap)
		{
			waterColorMap = TryGetWaterColorMap(device, material);
		}

		if (reticleColorMap)
		{
			device->SetTexture(0, reticleColorMap);
		}
		else if (waterColorMap)
		{
			device->SetTexture(0, waterColorMap);
		}
		else if (colorMap)
		{
			IDirect3DTexture9* camoColorMap = TryGetCamoColorMap(device, material, colorMap);

			if (camoColorMap)
			{
				colorMap = camoColorMap;
			}

			IDirect3DTexture9* layerTwin = nullptr;

			if (isBlending || isCutoutDecal)
			{
				layerTwin = TryGetLayerTwin(device, material, colorMap);
			}

			if (layerTwin)
			{
				colorMap = layerTwin;
			}

			device->SetTexture(0, colorMap);
		}

		D3DMATRIX scrollTransform = identity;
		const bool isScrolling = TryReadTextureScroll(source, material, scrollTransform);

		if (isScrolling)
		{
			device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &textureTransformFlags);
			device->GetTransform(D3DTS_TEXTURE0, &textureTransform);
			device->SetTransform(D3DTS_TEXTURE0, &scrollTransform);
			device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
			++stats.scrollingLists;
		}

		D3DCOLOR neonColour = 0xFFFFFFFF;
		const bool isNeonPulsing = !reticleColorMap && !waterColorMap && colorMap
			&& TryGetNeonPulseColour(material, neonColour);
		isAlienGroup = isNeonPulsing && IsAlienFoliageMaterial(material);
		DWORD neonOperation = D3DTOP_MODULATE;
		DWORD neonArgument1 = D3DTA_TEXTURE;
		DWORD neonArgument2 = D3DTA_CURRENT;
		DWORD neonFactor = 0xFFFFFFFF;

		if (isNeonPulsing)
		{
			device->GetTextureStageState(0, D3DTSS_COLOROP, &neonOperation);
			device->GetTextureStageState(0, D3DTSS_COLORARG1, &neonArgument1);
			device->GetTextureStageState(0, D3DTSS_COLORARG2, &neonArgument2);
			device->GetRenderState(D3DRS_TEXTUREFACTOR, &neonFactor);

			device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
			device->SetRenderState(D3DRS_TEXTUREFACTOR, neonColour);
		}

		drawGroup(device, context, iterator);
		isAlienGroup = false;

		if (isNeonPulsing)
		{
			device->SetRenderState(D3DRS_TEXTUREFACTOR, neonFactor);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, neonArgument2);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, neonArgument1);
			device->SetTextureStageState(0, D3DTSS_COLOROP, neonOperation);
		}

		if (isDumping)
		{
			dumpRow.draws = groupDraws - drawsBefore;
			dumpRows.push_back(dumpRow);
		}

		if (isScrolling)
		{
			device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, textureTransformFlags);
			device->SetTransform(D3DTS_TEXTURE0, &textureTransform);
		}

		device->SetTexture(0, boundTexture);

		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetVertexDeclaration(declaration);
		device->SetVertexShader(vertexShader);
		device->SetPixelShader(pixelShader);
		device->SetStreamSource(0, stream, streamOffset, streamStride);
		device->SetIndices(indexBuffer);
		device->SetRenderState(D3DRS_LIGHTING, lighting);

		if (isPremultipliedBlend)
		{
			device->SetRenderState(D3DRS_SRCBLEND, sourceBlend);
			device->SetRenderState(D3DRS_SRCBLENDALPHA, sourceBlendAlpha);
			device->SetRenderState(D3DRS_DESTBLENDALPHA, destinationBlendAlpha);
		}

		if (isAdditiveLayer)
		{
			device->SetRenderState(D3DRS_SRCBLEND, sourceBlend);
		}

		if (isCutoutDecal)
		{
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, isBlending);
			device->SetRenderState(D3DRS_SRCBLEND, sourceBlend);
			device->SetRenderState(D3DRS_DESTBLEND, destinationBlend);
			device->SetRenderState(D3DRS_SRCBLENDALPHA, sourceBlendAlpha);
			device->SetRenderState(D3DRS_DESTBLENDALPHA, destinationBlendAlpha);
		}

		if (isIlluminatedLayer)
		{
			isPulledTowardEye = false;
			device->SetRenderState(D3DRS_SRCBLEND, sourceBlend);
			device->SetRenderState(D3DRS_DESTBLEND, destinationBlend);
		}

		if (isReticle)
		{
			reticle.isActive = false;
			reticle.shape = nullptr;
			isPulledTowardEye = false;

			device->SetRenderState(D3DRS_ALPHABLENDENABLE, isBlending);
			device->SetRenderState(D3DRS_SRCBLEND, sourceBlend);
			device->SetRenderState(D3DRS_DESTBLEND, destinationBlend);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, isAlphaTested);
			device->SetRenderState(D3DRS_ALPHAREF, alphaReference);
			device->SetRenderState(D3DRS_ALPHAFUNC, alphaFunction);
			device->SetTextureStageState(0, D3DTSS_COLOROP, colourOperation);
			device->SetTextureStageState(0, D3DTSS_COLORARG1, colourArgument1);
			device->SetTextureStageState(0, D3DTSS_COLORARG2, colourArgument2);
			device->SetRenderState(D3DRS_TEXTUREFACTOR, textureFactor);
			device->SetSamplerState(0, D3DSAMP_ADDRESSU, addressU);
			device->SetSamplerState(0, D3DSAMP_ADDRESSV, addressV);
		}

		device->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOperation);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArgument1);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, alphaArgument2);

		if (vertexShader)
		{
			vertexShader->Release();
		}

		if (pixelShader)
		{
			pixelShader->Release();
		}

		if (declaration)
		{
			declaration->Release();
		}

		if (stream)
		{
			stream->Release();
		}

		if (indexBuffer)
		{
			indexBuffer->Release();
		}

		if (boundTexture)
		{
			boundTexture->Release();
		}
	}

	static bool IsShaderTintMaterial(std::uintptr_t material)
	{
		const auto entry = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsEntry + Iw4::techniqueLit);
		const auto stateBitsCount = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsCount);
		const auto stateBits = ReadAt<std::uintptr_t>(material + Iw4::materialStateBitsTable);

		if (entry >= stateBitsCount || !stateBits)
		{
			return false;
		}

		const auto firstBits = ReadAt<std::uint32_t>(stateBits + entry * Iw4::stateBitsStride);

		if ((firstBits & Iw4::stateBitsBlendMask) != Iw4::doubleMultiplyBlend)
		{
			return false;
		}

		const std::uintptr_t definition = FindTextureDef(material, Iw4::colorMapNameHash);

		if (!definition)
		{
			return false;
		}

		const auto image = ReadAt<std::uintptr_t>(definition + Iw4::textureDefImage);

		if (!image)
		{
			return false;
		}

		const char* name = ReadAt<const char*>(image + Iw4::gfxImageName);

		return name && name[0] == '$';
	}

	static void __fastcall TessBspDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipSurfaceGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			++stats.passedThroughLists;
			RecordEngineGroup(context, "engine");
			originalTess(context, iterator);
			return;
		}

		if (IsSkyGroup(context))
		{
			++stats.skyLists;
			RecordEngineGroup(context, "sky");
			DrawSkyThroughEngine(context, iterator);
			return;
		}

		if (IsShaderTintMaterial(GroupMaterial(context)))
		{
			++stats.shaderTintLists;
			SkipSurfaceGroup(iterator);
			return;
		}

		const MaterialBatch* batch = TryGetGroupBatch(context);

		if (batch && batch->drawnScene == sceneNumber)
		{
			SkipSurfaceGroup(iterator);
			return;
		}

		DrawFixedFunction(context, iterator, &DrawMaterialBatchGroup, "world");

		++stats.fixedFunctionLists;

		if (stats.fixedFunctionLists == 1)
		{
			Log() << "remix: the map's surfaces now reach Remix as fixed-function draws from a static copy of the"
					 " world, one draw per material batch; sky groups go to the engine at depth 1 so Remix takes them"
					 " as sky";
		}
	}

	static void __fastcall TessBspSecondPassDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipSurfaceGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalTess(context, iterator);
			return;
		}

		if (IsSkyGroup(context))
		{
			DrawSkyThroughEngine(context, iterator);
			return;
		}

		SkipSurfaceGroup(iterator);
	}

	static void __fastcall TessBModelDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipBrushModelGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalTessBModel(context, iterator);
			return;
		}

		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);

		if (ReadAt<int>(state + Iw4::statePassIndex) != 0)
		{
			SkipBrushModelGroup(iterator);
			return;
		}

		DrawFixedFunction(context, iterator, &DrawBrushModelGroup, "brushmodel");
		++stats.brushModelLists;
	}

	static void __fastcall StaticModelRigidDetour(void* context, void** list)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipStaticModelGroup(list);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalStaticModelRigid(context, list);
			return;
		}

		EnsureStaticModelBatches(sceneDevice);

		const MaterialBatch* staticBatch = TryGetStaticBatch(context);
		const bool isBatchPending = staticBatch && staticBatch->drawnScene != sceneNumber;

		if (!isBatchPending && !HasUnbakedInstances(list) && !IsAlienFoliageMaterial(GroupMaterial(context)))
		{
			SkipStaticModelGroup(list);
			return;
		}

		DrawFixedFunction(context, list, &DrawStaticModelGroup, "staticmodel");
		++stats.staticModelLists;
	}

	static void __fastcall StaticModelRigidSecondPassDetour(void* context, void** list)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipStaticModelGroup(list);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalStaticModelRigid(context, list);
			return;
		}

		SkipStaticModelGroup(list);
	}

	static void __fastcall StaticModelDeformedDetour(void* context, void** list)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipStaticModelGroup(list);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalStaticModelDeformed(context, list);
			return;
		}

		EnsureStaticModelBatches(sceneDevice);

		const MaterialBatch* staticBatch = TryGetStaticBatch(context);
		const bool isBatchPending = staticBatch && staticBatch->drawnScene != sceneNumber;

		if (!isBatchPending && !HasUnbakedInstances(list) && !IsAlienFoliageMaterial(GroupMaterial(context)))
		{
			SkipStaticModelGroup(list);
			return;
		}

		DrawFixedFunction(context, list, &DrawStaticModelGroup, "staticmodel-deformed");
		++stats.staticModelLists;
	}

	static void __fastcall StaticModelDeformedSecondPassDetour(void* context, void** list)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipStaticModelGroup(list);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalStaticModelDeformed(context, list);
			return;
		}

		SkipStaticModelGroup(list);
	}

	static bool TryPatchPair(const Iw4::CallSite& first, const Iw4::CallSite& second, const void* firstDetour,
		const void* secondDetour, TessBspSurfaces& original)
	{
		if (first.address == 0 || second.address == 0)
		{
			return false;
		}

		const std::uintptr_t firstSite = Engine::RebaseAddress(first.address);
		const std::uintptr_t secondSite = Engine::RebaseAddress(second.address);
		const std::uintptr_t target = Hook::ResolveCallTarget(firstSite);

		if (!target || Hook::ResolveCallTarget(secondSite) != target)
		{
			LogError() << "remix: the static model calls at 0x" << std::hex << firstSite << " and 0x" << secondSite
					   << std::dec << " do not reach the same function, refusing to patch";
			return false;
		}

		original = reinterpret_cast<TessBspSurfaces>(target);

		if (!Hook::PatchCall(secondSite, second.bytes, sizeof(second.bytes), secondDetour, nullptr))
		{
			return false;
		}

		return Hook::PatchCall(firstSite, first.bytes, sizeof(first.bytes), firstDetour, nullptr);
	}

	static void InstallStaticModelHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		const bool isRigidPatched = TryPatchPair(build.staticModelRigidCall, build.staticModelRigidSecondPassCall,
			reinterpret_cast<const void*>(&StaticModelRigidDetour),
			reinterpret_cast<const void*>(&StaticModelRigidSecondPassDetour), originalStaticModelRigid);

		const bool isDeformedPatched = TryPatchPair(build.staticModelDeformedCall, build.staticModelDeformedSecondPassCall,
			reinterpret_cast<const void*>(&StaticModelDeformedDetour),
			reinterpret_cast<const void*>(&StaticModelDeformedSecondPassDetour), originalStaticModelDeformed);

		stats.isStaticModelHookInstalled = isRigidPatched && isDeformedPatched;

		stats.isPropListFilterHooked = TryPatchCallGroup(&build.R_AddAllStaticModelSurfacesCameraCall, 1,
			reinterpret_cast<const void*>(&AddStaticModelSurfacesDetour),
			reinterpret_cast<void**>(&originalAddStaticModelSurfaces));

		Log() << "remix: baked props are left out of the engine's static model lists, one nearest prop per batch"
				 " still triggers its batch, so big models no longer overflow the lists (" << stats.isPropListFilterHooked
			  << ")";

		Log() << "remix: static model draws hooked (rigid " << isRigidPatched << ", deformed " << isDeformedPatched
			  << "), props are drawn fixed-function from per-surface buffers with their own placement";
	}

	static void __fastcall TessRigidDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipBrushModelGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalTessRigid(context, iterator);
			return;
		}

		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);

		if (ReadAt<int>(state + Iw4::statePassIndex) != 0)
		{
			SkipBrushModelGroup(iterator);
			return;
		}

		DrawFixedFunction(context, iterator, &DrawRigidModelGroup, "entity-rigid");
		++stats.entityModelLists;
	}

	static void __fastcall TessRigidSkinnedDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipBrushModelGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalTessRigidSkinned(context, iterator);
			return;
		}

		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);

		if (ReadAt<int>(state + Iw4::statePassIndex) != 0)
		{
			SkipBrushModelGroup(iterator);
			return;
		}

		DrawFixedFunction(context, iterator, &DrawRigidSkinnedModelGroup, "entity-rigidskinned");
		++stats.entityModelLists;
	}

	static void __fastcall TessSkinnedDetour(void* context, void** iterator)
	{
		if (IsDynamicLightList(context))
		{
			++stats.dynamicLightLists;
			SkipBrushModelGroup(iterator);
			return;
		}

		if (!ShouldDrawFixedFunction(context))
		{
			originalTessSkinned(context, iterator);
			return;
		}

		const auto state = ReadAt<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(context) + Iw4::drawListArgsState);

		if (ReadAt<int>(state + Iw4::statePassIndex) != 0)
		{
			SkipBrushModelGroup(iterator);
			return;
		}

		DrawFixedFunction(context, iterator, &DrawSkinnedModelGroup, "entity-skinned");
		++stats.skinnedModelLists;
	}

	static bool TryPatchTableSlot(std::uintptr_t slot, std::uintptr_t function, const void* detour, TessBspSurfaces& original)
	{
		if (slot == 0 || function == 0)
		{
			return false;
		}

		const std::uintptr_t current = Engine::RebaseAddress(function);
		const auto replacement = reinterpret_cast<std::uintptr_t>(detour);

		unsigned char expected[sizeof(std::uintptr_t)];
		unsigned char patched[sizeof(std::uintptr_t)];
		std::memcpy(expected, &current, sizeof(expected));
		std::memcpy(patched, &replacement, sizeof(patched));

		original = reinterpret_cast<TessBspSurfaces>(current);

		if (!Hook::PatchBytes(Engine::RebaseAddress(slot), expected, patched, sizeof(expected)))
		{
			original = nullptr;
			return false;
		}

		return true;
	}

	static void InstallBrushModelHook()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (!TryPatchTableSlot(build.tessTableBModelSlot, build.R_TessBModel,
				reinterpret_cast<const void*>(&TessBModelDetour), originalTessBModel))
		{
			return;
		}

		stats.isBrushModelHookInstalled = true;

		Log() << "remix: brush models (doors, movers) are now drawn fixed-function from the same world buffers";
	}

	static bool IsControllingLocalPlayer()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();
		const auto otherFlags = ReadAt<std::uint32_t>(Engine::RebaseAddress(build.cgOtherFlags));

		return (otherFlags & Iw4::cgFollowFlag) == 0 && ReadAt<int>(Engine::RebaseAddress(build.killCamState)) == 0;
	}

	static bool __fastcall BodyThirdPersonCheckDetour(void* cg)
	{
		if (originalBodyThirdPersonCheck(cg))
		{
			return true;
		}

		return isPlayerBodyShadow && IsControllingLocalPlayer();
	}

	static bool IsLocalFirstPersonBody(std::uintptr_t pose)
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();
		const auto otherFlags = ReadAt<std::uint32_t>(Engine::RebaseAddress(build.cgOtherFlags));

		if ((otherFlags & Iw4::cgFollowOrPlayerFlags) == 0 || !IsControllingLocalPlayer())
		{
			return false;
		}

		if (ReadAt<int>(pose + Iw4::centClientNum) != ReadAt<int>(Engine::RebaseAddress(build.predictedClientNum)))
		{
			return false;
		}

		if (ReadAt<int>(Engine::RebaseAddress(build.renderingThirdPerson)) != 0)
		{
			return false;
		}

		const auto isThirdPersonMode = reinterpret_cast<IsThirdPersonMode>(Engine::RebaseAddress(build.BG_IsThirdPersonMode));

		return !isThirdPersonMode(reinterpret_cast<void*>(Engine::RebaseAddress(build.cgArray)));
	}

	static void __fastcall AddPlayerDObjDetour(void* object, void* pose, int entityNumber, unsigned int renderFlags,
		const float* lightingOrigin, float materialTime)
	{
		if (isPlayerBodyShadow && pose && IsLocalFirstPersonBody(reinterpret_cast<std::uintptr_t>(pose)))
		{
			materialTime = localBodyMarker;
		}

		originalAddPlayerDObj(object, pose, entityNumber, renderFlags, lightingOrigin, materialTime);
	}

	static bool TryHookBodyThirdPersonCheck()
	{
		const Iw4::CallSite& call = Engine::CurrentBuild().localBodyThirdPersonCall;

		if (call.address == 0)
		{
			return false;
		}

		const std::uintptr_t site = Engine::RebaseAddress(call.address);
		const std::uintptr_t target = Hook::ResolveCallTarget(site);

		if (!target)
		{
			return false;
		}

		originalBodyThirdPersonCheck = reinterpret_cast<IsThirdPersonMode>(target);

		if (!Hook::PatchCall(site, call.bytes, sizeof(call.bytes), reinterpret_cast<const void*>(&BodyThirdPersonCheckDetour),
				nullptr))
		{
			originalBodyThirdPersonCheck = nullptr;
			return false;
		}

		return true;
	}

	static void InstallPlayerBodyHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.addPlayerDObjCall.address == 0)
		{
			return;
		}

		const std::uintptr_t site = Engine::RebaseAddress(build.addPlayerDObjCall.address);
		const std::uintptr_t target = Hook::ResolveCallTarget(site);

		if (!target)
		{
			return;
		}

		originalAddPlayerDObj = reinterpret_cast<AddDObjToScene>(target);

		if (!Hook::PatchCall(site, build.addPlayerDObjCall.bytes, sizeof(build.addPlayerDObjCall.bytes),
				reinterpret_cast<const void*>(&AddPlayerDObjDetour), nullptr))
		{
			originalAddPlayerDObj = nullptr;
			return;
		}

		if (!TryHookBodyThirdPersonCheck())
		{
			return;
		}

		Log() << "remix: the local player's body is drawn in first person with unique texture twins listed in"
				 " rtx.playerModelTextures, so Remix keeps its shadow and reflection and hides it from the camera";
	}

	void SetPlayerBodyShadow(bool isEnabled)
	{
		isPlayerBodyShadow = isEnabled;
	}

	bool IsPlayerBodyShadow()
	{
		return isPlayerBodyShadow;
	}

	static void InstallEntityModelHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		const bool isRigidPatched = TryPatchTableSlot(build.tessTableRigidSlot, build.R_TessXModelRigidDrawSurfList,
			reinterpret_cast<const void*>(&TessRigidDetour), originalTessRigid);

		const bool isRigidSkinnedPatched = TryPatchTableSlot(build.tessTableRigidSkinnedSlot,
			build.R_TessXModelRigidSkinnedDrawSurfList, reinterpret_cast<const void*>(&TessRigidSkinnedDetour),
			originalTessRigidSkinned);

		stats.isEntityModelHookInstalled = isRigidPatched && isRigidSkinnedPatched;

		Log() << "remix: rigid entity models hooked (rigid " << isRigidPatched << ", rigid-skinned " << isRigidSkinnedPatched
			  << "), vehicles, props and the gun's rigid parts are drawn fixed-function per part";

		const Iw4::BytePatch& skinCache = build.R_ToggleSmpFrame_SkinCacheImm;

		if (skinCache.address == 0)
		{
			return;
		}

		if (!Hook::PatchBytes(Engine::RebaseAddress(skinCache.address), skinCache.original, skinCache.replacement,
				skinCache.size))
		{
			LogWarn() << "remix: the skin cache switch did not match, skinned models stay on vertex capture";
			return;
		}

		stats.isSkinnedModelHookInstalled = TryPatchTableSlot(build.tessTableSkinnedSlot,
			build.R_TessXModelSkinnedDrawSurfList, reinterpret_cast<const void*>(&TessSkinnedDetour), originalTessSkinned);

		Log() << "remix: skin cache off and skinned models hooked (" << stats.isSkinnedModelHookInstalled
			  << "), players and the viewmodel arms are drawn fixed-function from the CPU skinned vertices";
	}

	bool InstallWorldHooks()
	{
		if (stats.isHookInstalled)
		{
			return true;
		}

		const Iw4::GameBuild& build = Engine::CurrentBuild();

		const bool isMapped = build.bspSurfaceListCall.address != 0 && build.bspSurfaceListSecondPassCall.address != 0
			&& build.g_worldDraw != 0 && build.g_worldDpvs != 0 && build.gfxWorld != 0 && build.rChangeDepthRange != 0;

		if (!isMapped)
		{
			LogWarn() << "remix: the world draw is not mapped for this build, the map stays on vertex capture";
			return false;
		}

		const std::uintptr_t firstSite = Engine::RebaseAddress(build.bspSurfaceListCall.address);
		const std::uintptr_t secondSite = Engine::RebaseAddress(build.bspSurfaceListSecondPassCall.address);

		const std::uintptr_t original = Hook::ResolveCallTarget(firstSite);

		if (!original || Hook::ResolveCallTarget(secondSite) != original)
		{
			LogError() << "remix: the two world draw calls do not reach the same function, refusing to patch";
			return false;
		}

		originalTess = reinterpret_cast<TessBspSurfaces>(original);
		changeDepthRange = reinterpret_cast<ChangeDepthRange>(Engine::RebaseAddress(build.rChangeDepthRange));

		if (!Hook::PatchCall(secondSite, build.bspSurfaceListSecondPassCall.bytes,
				sizeof(build.bspSurfaceListSecondPassCall.bytes),
				reinterpret_cast<const void*>(&TessBspSecondPassDetour), nullptr))
		{
			return false;
		}

		if (!Hook::PatchCall(firstSite, build.bspSurfaceListCall.bytes, sizeof(build.bspSurfaceListCall.bytes),
				reinterpret_cast<const void*>(&TessBspDetour), nullptr))
		{
			return false;
		}

		stats.isHookInstalled = true;

		Log() << "remix: world surface list hooked, the map will be drawn fixed-function in the scene view";

		InstallBrushModelHook();
		InstallStaticModelHooks();
		InstallEntityModelHooks();
		InstallPlayerBodyHooks();

		return true;
	}

	static bool TryGetMaterialDrawState(std::uintptr_t material, BatchDrawState& drawState)
	{
		const auto entry = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsEntry + Iw4::techniqueLit);
		const auto stateBitsCount = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsCount);
		const auto stateBits = ReadAt<std::uintptr_t>(material + Iw4::materialStateBitsTable);

		if (entry >= stateBitsCount || !stateBits)
		{
			return false;
		}

		const auto firstBits = ReadAt<std::uint32_t>(stateBits + entry * Iw4::stateBitsStride);

		if ((firstBits & Iw4::stateBitsBlendMask) != opaqueBlendBits)
		{
			return false;
		}

		drawState = BatchDrawState{};
		drawState.isKnown = true;
		drawState.isOpaque = true;
		drawState.cullMode = D3DCULL_NONE;
		drawState.minZ = worldMinZ;
		drawState.maxZ = 1.0f;

		const std::uint32_t alphaTest = firstBits & alphaTestMask;

		if ((firstBits & alphaTestOffBit) != 0 || alphaTest == 0)
		{
			return true;
		}

		drawState.isAlphaTested = TRUE;
		drawState.alphaReference = 128;
		drawState.alphaFunction = D3DCMP_GREATEREQUAL;

		if (alphaTest == alphaTestAboveZero)
		{
			drawState.alphaReference = 0;
			drawState.alphaFunction = D3DCMP_GREATER;
		}

		if (alphaTest == alphaTestBelowHalf)
		{
			drawState.alphaFunction = D3DCMP_LESS;
		}

		return true;
	}

	static void ApplyDrawState(IDirect3DDevice9* device, const BatchDrawState& drawState)
	{
		D3DVIEWPORT9 viewport{};
		device->GetViewport(&viewport);
		viewport.MinZ = drawState.minZ;
		viewport.MaxZ = drawState.maxZ;
		device->SetViewport(&viewport);

		device->SetRenderState(D3DRS_ALPHATESTENABLE, drawState.isAlphaTested);
		device->SetRenderState(D3DRS_ALPHAREF, drawState.alphaReference);
		device->SetRenderState(D3DRS_ALPHAFUNC, drawState.alphaFunction);
		device->SetRenderState(D3DRS_CULLMODE, drawState.cullMode);
		device->SetRenderState(D3DRS_ZENABLE, drawState.isDepthTested);
		device->SetRenderState(D3DRS_ZWRITEENABLE, drawState.isDepthWritten);
		device->SetRenderState(D3DRS_ZFUNC, drawState.depthFunction);
		device->SetTextureStageState(0, D3DTSS_COLOROP, drawState.colourOperation);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, drawState.colourArgument1);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, drawState.colourArgument2);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, drawState.alphaOperation);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, drawState.alphaArgument1);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, drawState.alphaArgument2);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, drawState.addressU);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, drawState.addressV);
	}

	static unsigned DrawKnownBatches(IDirect3DDevice9* device, std::unordered_map<std::uintptr_t, MaterialBatch>& batches,
		IDirect3DVertexBuffer9* vertexBuffer, IDirect3DIndexBuffer9* indexBuffer, const char* kind)
	{
		if (!vertexBuffer || !indexBuffer)
		{
			return 0;
		}

		device->SetStreamSource(0, vertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
		device->SetIndices(indexBuffer);

		unsigned drawn = 0;

		for (auto& entry : batches)
		{
			MaterialBatch& batch = entry.second;
			BatchDrawState drawState = batch.drawState;

			if (batch.drawnScene == sceneNumber)
			{
				continue;
			}

			if (!drawState.isKnown && !TryGetMaterialDrawState(entry.first, drawState))
			{
				continue;
			}

			if (!drawState.isOpaque || drawState.isScrolling)
			{
				continue;
			}

			IDirect3DTexture9* colorMap = TryGetColorMap(entry.first);

			if (!colorMap)
			{
				continue;
			}

			device->SetTexture(0, colorMap);
			ApplyDrawState(device, drawState);

			D3DCOLOR neonColour = 0xFFFFFFFF;

			if (TryGetNeonPulseColour(entry.first, neonColour))
			{
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
				device->SetRenderState(D3DRS_TEXTUREFACTOR, neonColour);
			}

			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, batch.firstVertex, batch.vertexCount, batch.startIndex,
				batch.triCount);

			batch.drawnScene = sceneNumber;
			++drawn;

			if (isDumping)
			{
				DumpRow row = CaptureDumpRow(device, entry.first, kind);
				row.draws = 1;
				dumpRows.push_back(row);
			}
		}

		return drawn;
	}

	static unsigned DrawCulledBigProps(IDirect3DDevice9* device)
	{
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(Engine::CurrentBuild().gfxWorld));

		if (!world || bigPropRecords.empty())
		{
			return 0;
		}

		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);

		if (!drawInsts || drawInsts != staticBatchDrawInsts)
		{
			return 0;
		}

		unsigned drawn = 0;

		for (const StaticSurfaceRecord& record : bigPropRecords)
		{
			if (record.instance < bigPropDrawnScene.size() && bigPropDrawnScene[record.instance] == sceneNumber)
			{
				continue;
			}

			BatchDrawState drawState;

			if (!TryGetMaterialDrawState(record.material, drawState))
			{
				continue;
			}

			IDirect3DTexture9* colorMap = TryGetColorMap(record.material);
			const ModelBuffers* buffers = TryGetModelBuffers(device, record.surface);

			if (!colorMap || !buffers)
			{
				continue;
			}

			const D3DMATRIX placement = StaticModelMatrix(drawInsts + record.instance * Iw4::smodelDrawInstStride);

			device->SetTexture(0, colorMap);
			ApplyDrawState(device, drawState);
			device->SetTransform(D3DTS_WORLD, &placement);
			device->SetStreamSource(0, buffers->vertexBuffer, 0, static_cast<UINT>(sizeof(WorldVertex)));
			device->SetIndices(buffers->indexBuffer);
			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, buffers->vertexCount, 0, buffers->triCount);
			++drawn;
		}

		device->SetTransform(D3DTS_WORLD, &identity);
		return drawn;
	}

	void DrawCulledBatches(IDirect3DDevice9* device)
	{
		if (!device || culledFillScene == sceneNumber || !EnsureWorldBuffers(device))
		{
			return;
		}

		culledFillScene = sceneNumber;

		EnsureStaticModelBatches(device);

		if (!completeSceneState && FAILED(device->CreateStateBlock(D3DSBT_ALL, &completeSceneState)))
		{
			completeSceneState = nullptr;
			return;
		}

		completeSceneState->Capture();

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(worldVertexFormat);
		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		stats.completedWorldBatches = DrawKnownBatches(device, materialBatches, batchVertexBuffer, batchIndexBuffer,
			"culled-world");
		stats.completedPropBatches = DrawKnownBatches(device, staticModelBatches, staticBatchVertexBuffer,
			staticBatchIndexBuffer, "culled-prop");
		stats.completedBigProps = DrawCulledBigProps(device);

		completeSceneState->Apply();
	}

	unsigned GetSurfaceSpotsVersion()
	{
		return surfaceSpotsVersion;
	}

	std::vector<FixtureSpot> CollectFixtureSpots()
	{
		std::vector<FixtureSpot> spots;
		std::vector<std::pair<std::uintptr_t, const std::vector<FixtureSpot>*>> candidates;

		for (const auto& entry : surfaceSpots)
		{
			candidates.emplace_back(entry.first, &entry.second);
		}

		for (const auto& entry : propSpots)
		{
			candidates.emplace_back(entry.first, &entry.second);
		}

		for (const auto& [material, materialSpots] : candidates)
		{
			std::uint64_t hash = 0;

			if (!TryGetMaterialColourHash(material, hash) || (!IsFixtureHash(hash) && !IsLampLightHash(hash)))
			{
				continue;
			}

			for (const FixtureSpot& candidate : *materialSpots)
			{
				bool isMerged = false;

				for (const FixtureSpot& kept : spots)
				{
					const float dx = kept.position[0] - candidate.position[0];
					const float dy = kept.position[1] - candidate.position[1];
					const float dz = kept.position[2] - candidate.position[2];

					if (dx * dx + dy * dy + dz * dz < fixtureMergeDistance * fixtureMergeDistance)
					{
						isMerged = true;
						break;
					}
				}

				if (!isMerged && spots.size() < maxFixtureSpots)
				{
					spots.push_back(candidate);
				}
			}
		}

		return spots;
	}

	void RerollAlienColours()
	{
		alienColourSeed = MixBits(static_cast<std::uint32_t>(GetTickCount64()) ^ alienColourSeed ^ 0x5BD1E995u);
	}

	static bool wasControllingLocalPlayer = true;

	void BeginSceneView(IDirect3DDevice9* device)
	{
		sceneDevice = device;
		viewSurfaces = 0;
		++sceneNumber;
		didBuildCamoThisView = false;

		const bool isControlling = IsControllingLocalPlayer();

		if (isControlling != wasControllingLocalPlayer)
		{
			wasControllingLocalPlayer = isControlling;

			if (isControlling)
			{
				Device::NoteFrameEvent("back in control of the local player");
			}
			else
			{
				Device::NoteFrameEvent("killcam or spectating started");
			}
		}

		if (isDumpRequested)
		{
			isDumpRequested = false;
			isDumping = true;
			dumpRows.clear();
		}
	}

	void EndSceneView()
	{
		const bool hasSky = viewSkyGroups > 0;

		if (hasSky != didLastViewHaveSky && skyTransitionLogs < maxSkyTransitionLogs)
		{
			++skyTransitionLogs;
			const char* change = "gone from";

			if (hasSky)
			{
				change = "back in";
			}

			Log() << "remix: scene " << sceneNumber << " draws " << viewSkyGroups << " sky groups ("
				  << viewSurfaces << " surface groups in view), sky " << change
				  << " the frame Remix builds its sky light from";
		}

		didLastViewHaveSky = hasSky;
		viewSkyGroups = 0;
		stats.lastViewSurfaces = viewSurfaces;
		sceneDevice = nullptr;

		if (isDumping)
		{
			isDumping = false;
			WriteSceneDump();
		}
	}

	void RequestSceneDump()
	{
		isDumpRequested = true;
	}

	void ReleaseWorldBuffers()
	{
		if (completeSceneState)
		{
			completeSceneState->Release();
			completeSceneState = nullptr;
		}

		ReleaseModelBuffers();
		ReleaseCamoTextures();
		ReleaseMaterialBatches();
		ReleaseStaticModelBatches();
		surfaceSpots.clear();
		++surfaceSpotsVersion;

		if (skinnedVertexBuffer)
		{
			skinnedVertexBuffer->Release();
			skinnedVertexBuffer = nullptr;
		}

		skinnedRingOffset = 0;

		if (worldVertexBuffer)
		{
			worldVertexBuffer->Release();
			worldVertexBuffer = nullptr;
		}

		if (worldIndexBuffer)
		{
			worldIndexBuffer->Release();
			worldIndexBuffer = nullptr;
		}

		if (blendedIndexBuffer)
		{
			blendedIndexBuffer->Release();
			blendedIndexBuffer = nullptr;
		}

		didBuildFail = false;
	}

	WorldStats GetWorldStats()
	{
		return stats;
	}
}
