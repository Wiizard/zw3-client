#include "STDInclude.hpp"

#include "CrateCollision.hpp"
#include "ClipMap.hpp"
#include "CollmapCollision.hpp"
#include "GSC/Script.hpp"
#include "Logger.hpp"

namespace Components
{
	using namespace ClipMap;

	constexpr std::uintptr_t rgp_world = 0x148CCC7A0;
	constexpr std::size_t gfxWorld_modelCount = 0x160;
	constexpr std::size_t gfxWorld_models = 0x168;

	constexpr std::uintptr_t CM_LoadMap = 0x1401E84C0;
	constexpr std::uintptr_t loadMapCalls[] = { 0x1400F3E8E , 0x14023B761  };

	constexpr std::uintptr_t R_GetBrushModel = 0x14001F150;
	static const std::uint8_t getBrushModelBytes[] = { 0x8B, 0xC1, 0x48, 0x8B, 0x0D, 0x47, 0xD6, 0xCA, 0x08, 0x48, 0x6B, 0xC0, 0x3C, 0x48, 0x03, 0x81, 0x68, 0x01, 0x00, 0x00, 0xC3 };

	constexpr float crateScale = 0.75f;
	constexpr Game::Bounds crateBounds =
	{
		{ -1.0f * crateScale, 0.25f * crateScale, 0.0f * crateScale },
		{ 30.0f * crateScale, 17.75f * crateScale, 16.0f * crateScale },
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

	AssertSize(GfxBrushModel, 0x3C);
	AssertOffset(GfxBrushModel, surfaceCount, 0x34);

	static std::vector<cmodel_t> cmodels;
	static std::vector<cLeafBrushNode_s> leafbrushNodes;
	static std::vector<Game::cbrush_t> brushes;
	static std::vector<Game::Bounds> brushBounds;
	static std::vector<int> brushContents;
	static std::uint16_t leafBrush;
	static unsigned int crateIndex;

	static GfxBrushModel crateGfxModel;
	static GfxBrushModel emptyGfxModel;

	static Utils::Hook loadMapHooks[std::size(loadMapCalls)];
	static Utils::Hook getBrushModelHook;

	unsigned int CrateCollision::GetIndex()
	{
		return crateIndex;
	}

	void CrateCollision::Inject()
	{
		const auto* const oldCmodels = Utils::Hook::Get<const cmodel_t*>(cm_cmodels);

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
		const auto* const oldBrushes = Utils::Hook::Get<const Game::cbrush_t*>(cm_brushes);
		const auto* const oldBounds = Utils::Hook::Get<const Game::Bounds*>(cm_brushBounds);
		const auto* const oldContents = Utils::Hook::Get<const int*>(cm_brushContents);

		cmodels.assign(oldCmodels, oldCmodels + numSubModels);
		leafbrushNodes.assign(oldNodes, oldNodes + numNodes);
		brushes.assign(oldBrushes, oldBrushes + numBrushes);
		brushBounds.assign(oldBounds, oldBounds + numBrushes);
		brushContents.assign(oldContents, oldContents + numBrushes);

		Game::cbrush_t brush{};
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

		crateGfxModel = {};
		crateGfxModel.writable = crateBounds;
		crateGfxModel.bounds = crateBounds;
		crateGfxModel.radius = model.radius;
	}

	void CrateCollision::CM_LoadMap_Hk(const char* name, unsigned int* checksum)
	{
		reinterpret_cast<void(*)(const char*, unsigned int*)>(Utils::Hook::Rebase(CM_LoadMap))(name, checksum);

		if (CollmapCollision::IsInjected())
		{
			return;
		}

		Inject();
		CollmapCollision::Inject();
	}

	void* CrateCollision::R_GetBrushModel_Hk(const unsigned int index)
	{
		const auto* const world = Utils::Hook::Get<const std::uint8_t*>(rgp_world);
		const auto modelCount = *reinterpret_cast<const unsigned int*>(world + gfxWorld_modelCount);

		if (crateIndex && index == crateIndex && index >= modelCount)
		{
			return &crateGfxModel;
		}

		if (index >= modelCount)
		{
			emptyGfxModel = {};
			return &emptyGfxModel;
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

		GSC::Script::AddFunction("GetCrateCollisionIndex", []
		{
			Game::Scr_AddInt(static_cast<int>(crateIndex));
		});
	}
}
