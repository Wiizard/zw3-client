#include "STDInclude.hpp"

#include "CrateCollision.hpp"
#include "GSC/Script.hpp"
#include "Logger.hpp"

namespace Components
{
	// cm (0x141BD3180)
	constexpr std::uintptr_t cm_numMaterials = 0x141BD31A8;
	constexpr std::uintptr_t cm_materials = 0x141BD31B0;
	constexpr std::uintptr_t cm_leafbrushNodesCount = 0x141BD31F8;
	constexpr std::uintptr_t cm_leafbrushNodes = 0x141BD3200;
	constexpr std::uintptr_t cm_numSubModels = 0x141BD3280;
	constexpr std::uintptr_t cm_cmodels = 0x141BD3288;
	constexpr std::uintptr_t cm_numBrushes = 0x141BD3290;
	constexpr std::uintptr_t cm_brushes = 0x141BD3298;
	constexpr std::uintptr_t cm_brushBounds = 0x141BD32A0;
	constexpr std::uintptr_t cm_brushContents = 0x141BD32A8;

	constexpr std::uintptr_t rgp_world = 0x148CCC7A0;
	constexpr std::size_t gfxWorld_modelCount = 0x160;
	constexpr std::size_t gfxWorld_models = 0x168;

	constexpr std::uintptr_t CM_LoadMap = 0x1401E84C0;
	constexpr std::uintptr_t loadMapCalls[] = { 0x1400F3E8E /* CL_CM_LoadMap */, 0x14023B761 /* SV_SpawnServer */ };

	constexpr std::uintptr_t R_GetBrushModel = 0x14001F150;
	static const std::uint8_t getBrushModelBytes[] = { 0x8B, 0xC1, 0x48, 0x8B, 0x0D, 0x47, 0xD6, 0xCA, 0x08, 0x48, 0x6B, 0xC0, 0x3C, 0x48, 0x03, 0x81, 0x68, 0x01, 0x00, 0x00, 0xC3 };

	constexpr int contentsSolid = 1;

	// stock care_package brush
	constexpr Game::Bounds crateBounds = { { -1.0f, 0.25f, 0.0f }, { 30.0f, 17.75f, 16.0f } };

#pragma pack(push, 1)
	struct cLeaf_t
	{
		std::uint16_t firstCollAabbIndex;
		std::uint16_t collAabbCount;
		int brushContents;
		int terrainContents;
		Game::Bounds bounds;
		int leafBrushNode;
	};

	struct cmodel_t
	{
		Game::Bounds bounds;
		float radius;
		cLeaf_t leaf;
	};
#pragma pack(pop)

	struct cLeafBrushNode_s
	{
		std::uint8_t axis;
		std::int16_t leafBrushCount;
		int contents;
		std::uint16_t* brushes;
		char pad[0x8];
	};

	struct cbrush_t
	{
		std::uint16_t numsides;
		std::uint16_t glassPieceIndex;
		char pad0[0x14];
		std::int16_t axialMaterialNum[2][3];
		char pad1[0xC];
	};

	struct ClipMaterial
	{
		const char* name;
		int surfaceFlags;
		int contents;
	};

	struct GfxBrushModel
	{
		Game::Bounds writable;
		Game::Bounds bounds;
		float radius;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;
		std::uint16_t surfaceCountNoDecal;
		char pad[0x2];
	};

	AssertSize(cLeaf_t, 0x28);
	AssertSize(cmodel_t, 0x44);
	AssertOffset(cmodel_t, leaf, 0x1C);
	AssertSize(cLeafBrushNode_s, 0x18);
	AssertOffset(cLeafBrushNode_s, brushes, 0x8);
	AssertSize(cbrush_t, 0x30);
	AssertOffset(cbrush_t, axialMaterialNum, 0x18);
	AssertSize(ClipMaterial, 0x10);
	AssertSize(GfxBrushModel, 0x3C);
	AssertOffset(GfxBrushModel, surfaceCount, 0x34);

	static std::vector<cmodel_t> cmodels;
	static std::vector<cLeafBrushNode_s> leafbrushNodes;
	static std::vector<cbrush_t> brushes;
	static std::vector<Game::Bounds> brushBounds;
	static std::vector<int> brushContents;
	static std::uint16_t leafBrush;
	static unsigned int crateIndex;

	static GfxBrushModel crateGfxModel;

	static Utils::Hook loadMapHooks[std::size(loadMapCalls)];
	static Utils::Hook getBrushModelHook;

	unsigned int CrateCollision::GetIndex()
	{
		return crateIndex;
	}

	static std::int16_t FindSolidMaterial()
	{
		const auto count = Utils::Hook::Get<unsigned int>(cm_numMaterials);
		const auto* const materials = Utils::Hook::Get<const ClipMaterial*>(cm_materials);

		for (unsigned int i = 0; i < count; ++i)
		{
			if (materials[i].contents & contentsSolid)
			{
				return static_cast<std::int16_t>(i);
			}
		}

		return 0;
	}

	void CrateCollision::Inject()
	{
		const auto* const oldCmodels = Utils::Hook::Get<const cmodel_t*>(cm_cmodels);

		// already added (listen server, map_restart)
		if (!cmodels.empty() && oldCmodels == cmodels.data())
		{
			return;
		}

		crateIndex = 0;

		const auto numSubModels = Utils::Hook::Get<unsigned int>(cm_numSubModels);
		const auto numBrushes = Utils::Hook::Get<std::uint16_t>(cm_numBrushes);
		const auto numNodes = Utils::Hook::Get<unsigned int>(cm_leafbrushNodesCount);

		if (!oldCmodels || !numSubModels || numBrushes == std::numeric_limits<std::uint16_t>::max() || !Utils::Hook::Get<unsigned int>(cm_numMaterials))
		{
			Logger::Print("cratecollision: the clip map has no room for a crate brush\n");
			return;
		}

		const auto* const oldNodes = Utils::Hook::Get<const cLeafBrushNode_s*>(cm_leafbrushNodes);
		const auto* const oldBrushes = Utils::Hook::Get<const cbrush_t*>(cm_brushes);
		const auto* const oldBounds = Utils::Hook::Get<const Game::Bounds*>(cm_brushBounds);
		const auto* const oldContents = Utils::Hook::Get<const int*>(cm_brushContents);

		cmodels.assign(oldCmodels, oldCmodels + numSubModels);
		leafbrushNodes.assign(oldNodes, oldNodes + numNodes);
		brushes.assign(oldBrushes, oldBrushes + numBrushes);
		brushBounds.assign(oldBounds, oldBounds + numBrushes);
		brushContents.assign(oldContents, oldContents + numBrushes);

		// no sides = box
		cbrush_t brush{};
		const auto material = FindSolidMaterial();

		for (auto& side : brush.axialMaterialNum)
		{
			std::ranges::fill(side, material);
		}

		brushes.push_back(brush);
		brushBounds.push_back(crateBounds);
		brushContents.push_back(contentsSolid);
		leafBrush = numBrushes;

		cLeafBrushNode_s node{};
		node.leafBrushCount = 1;
		node.contents = contentsSolid;
		node.brushes = &leafBrush;
		leafbrushNodes.push_back(node);

		const float* const half = crateBounds.halfSize;

		cmodel_t model{};
		model.bounds = crateBounds;
		model.radius = std::sqrt(half[0] * half[0] + half[1] * half[1] + half[2] * half[2]);
		model.leaf.brushContents = contentsSolid;
		model.leaf.bounds = crateBounds;
		model.leaf.leafBrushNode = static_cast<int>(numNodes);
		cmodels.push_back(model);

		Utils::Hook::Set<const void*>(cm_cmodels, cmodels.data());
		Utils::Hook::Set<const void*>(cm_leafbrushNodes, leafbrushNodes.data());
		Utils::Hook::Set<const void*>(cm_brushes, brushes.data());
		Utils::Hook::Set<const void*>(cm_brushBounds, brushBounds.data());
		Utils::Hook::Set<const void*>(cm_brushContents, brushContents.data());
		Utils::Hook::Set<unsigned int>(cm_leafbrushNodesCount, numNodes + 1);
		Utils::Hook::Set<std::uint16_t>(cm_numBrushes, static_cast<std::uint16_t>(numBrushes + 1));
		Utils::Hook::Set<unsigned int>(cm_numSubModels, numSubModels + 1);

		crateIndex = numSubModels;

		// CG_UpdateBModelWorldBounds writes to it
		crateGfxModel = {};
		crateGfxModel.writable = crateBounds;
		crateGfxModel.bounds = crateBounds;
		crateGfxModel.radius = model.radius;
	}

	void CrateCollision::CM_LoadMap_Hk(const char* name, unsigned int* checksum)
	{
		reinterpret_cast<void(*)(const char*, unsigned int*)>(Utils::Hook::Rebase(CM_LoadMap))(name, checksum);
		Inject();
	}

	void* CrateCollision::R_GetBrushModel_Hk(const unsigned int index)
	{
		const auto* const world = Utils::Hook::Get<const std::uint8_t*>(rgp_world);
		const auto modelCount = *reinterpret_cast<const unsigned int*>(world + gfxWorld_modelCount);

		if (crateIndex && index == crateIndex && index >= modelCount)
		{
			return &crateGfxModel;
		}

		return *reinterpret_cast<std::uint8_t* const*>(world + gfxWorld_models) + sizeof(GfxBrushModel) * index;
	}

	CrateCollision::CrateCollision()
	{
		bool isExpected = Utils::Hook::MatchesBytes(R_GetBrushModel, getBrushModelBytes, sizeof(getBrushModelBytes));

		for (const auto call : loadMapCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, CM_LoadMap, HOOK_CALL);
		}

		if (!isExpected)
		{
			Logger::Error("cratecollision: CM_LoadMap or R_GetBrushModel does not read as expected, no crate brush\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(loadMapCalls); ++i)
		{
			isSeated = loadMapHooks[i].Initialize(loadMapCalls[i], reinterpret_cast<void*>(CM_LoadMap_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = getBrushModelHook.Initialize(R_GetBrushModel, reinterpret_cast<void*>(R_GetBrushModel_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : loadMapHooks)
			{
				hook.Uninstall();
			}

			getBrushModelHook.Uninstall();

			Logger::Error("cratecollision: could not seat every hook, no crate brush\n");
			return;
		}

		for (auto& hook : loadMapHooks)
		{
			hook.Quick();
		}

		getBrushModelHook.Quick();

		// 0 = none
		GSC::Script::AddFunction("GetCrateCollisionIndex", []
		{
			Game::Scr_AddInt(static_cast<int>(crateIndex));
		});
	}
}
