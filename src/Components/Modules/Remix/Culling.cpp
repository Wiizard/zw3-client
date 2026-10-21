#include "STDInclude.hpp"

#include "Culling.hpp"

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Core.hpp"
#include "Engine.hpp"
#include "Hook.hpp"
#include "Iw4.hpp"
#include "MapSettings.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	using AddWorldSurfacesDpvs = void(__fastcall*)(unsigned int, float*, float*, unsigned int);
	using GetLodForDist = int(__fastcall*)(void*, float, float);

	struct PatchGroup
	{
		const Iw4::BytePatch* patches;
		int count;
		CullingLevel level;
		bool isApplied;
	};

	static constexpr unsigned int farPlaneIndex = 5;
	static constexpr unsigned int planeWithFarPlaneCount = 6;
	static constexpr int watchedWarnings[] = { 4, 13, 14, 15, 16, 17, 19, 51 };

	struct EngineLimit
	{
		int id;
		const char* name;
	};

	static constexpr EngineLimit engineLimits[] = {
		{ 1, "scene models" },
		{ 3, "special models" },
		{ 6, "temp skin buffer" },
		{ 7, "skinned cache vertices" },
		{ 8, "scene surface buffer" },
		{ 9, "skinned surface buffer" },
		{ 10, "portal plane buffer" },
		{ 11, "particle clouds (256)" },
		{ 12, "dynamic lights (32)" },
		{ 13, "static model lighting" },
		{ 14, "visible static models (vis records)" },
		{ 15, "static model draw lists" },
		{ 16, "static model delayed materials" },
		{ 17, "BSP surface list full" },
		{ 18, "BSP pretess surfaces" },
		{ 19, "draw surfaces (16384)" },
		{ 22, "glass surfaces (768)" },
		{ 23, "mark surfaces (1536)" },
		{ 24, "spark surfaces (64)" },
		{ 25, "scene entity draw surfaces" },
		{ 41, "glass mesh indices" },
		{ 42, "glass mesh vertices" },
		{ 43, "mark mesh indices" },
		{ 44, "mark mesh vertices" },
		{ 46, "spark mesh vertices" },
		{ 48, "FX spot lights" },
		{ 49, "FX elements" },
		{ 50, "FX bolts" },
		{ 51, "worker command size" },
	};

	static constexpr unsigned engineLimitLogFrames = 600;
	static std::uint64_t hitEngineLimits = 0;
	static unsigned engineLimitFrames = 0;

	static AddWorldSurfacesDpvs originalAddWorldSurfacesDpvs = nullptr;
	static GetLodForDist originalGetLodForDist = nullptr;
	static CullingStats stats;
	static PatchGroup groups[3]{};
	static PatchGroup entityFrustumGroup{};
	static PatchGroup lodFovGroup{};
	static CullingLevel appliedLevel = CullingLevel::Engine;
	static std::optional<int> mapLevel;
	static CullingTuning tuning;
	static float appliedRadius = 1000.0f;
	static float appliedPropScale = 1.0f;
	static std::unordered_set<std::uintptr_t> keptModels;
	static unsigned keptModelsVersion = 0;
	static std::uintptr_t keptModelsWorld = 0;

	static void __fastcall AddWorldSurfacesDpvsDetour(unsigned int cell, float* viewPlane, float* planes, unsigned int count)
	{
		if (appliedLevel != CullingLevel::Engine)
		{
			viewPlane[3] += appliedRadius;

			for (unsigned int i = 0; i < count; ++i)
			{
				if (count == planeWithFarPlaneCount && i == farPlaneIndex)
				{
					continue;
				}

				planes[i * 4 + 3] += appliedRadius;
			}
		}

		originalAddWorldSurfacesDpvs(cell, viewPlane, planes, count);
	}

	static int __fastcall GetLodForDistDetour(void* model, float lodDistance, float cullDistance)
	{
		const bool isKept = !keptModels.empty() && keptModels.count(reinterpret_cast<std::uintptr_t>(model)) != 0;

		if (isKept || (appliedLevel != CullingLevel::Engine && appliedLevel != CullingLevel::NearbyRooms))
		{
			return originalGetLodForDist(model, lodDistance, -1.0f);
		}

		return originalGetLodForDist(model, lodDistance, cullDistance / appliedPropScale);
	}

	static bool TryApply(const Iw4::BytePatch& patch)
	{
		if (patch.address == 0)
		{
			return false;
		}

		return Hook::PatchBytes(Engine::RebaseAddress(patch.address), patch.original, patch.replacement, patch.size);
	}

	static bool TryHookCall(const Iw4::CallSite& call, const void* detour, void** original)
	{
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

		*original = reinterpret_cast<void*>(target);

		if (!Hook::PatchCall(site, call.bytes, sizeof(call.bytes), detour, nullptr))
		{
			*original = nullptr;
			return false;
		}

		return true;
	}

	static bool TrySetGroupApplied(PatchGroup& group, bool isApplied)
	{
		if (group.isApplied == isApplied)
		{
			return true;
		}

		for (int i = 0; i < group.count; ++i)
		{
			if (group.patches[i].address == 0)
			{
				return false;
			}
		}

		for (int i = 0; i < group.count; ++i)
		{
			const std::uintptr_t site = Engine::RebaseAddress(group.patches[i].address);

			if (isApplied)
			{
				TryApply(group.patches[i]);
			}
			else
			{
				Hook::RestoreBytes(site);
			}
		}

		group.isApplied = isApplied;
		return true;
	}

	void InstallCullingPatches()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		for (const Iw4::BytePatch& patch : build.surfaceListCapacityPatches)
		{
			if (TryApply(patch))
			{
				++stats.appliedPatches;
			}
		}

		if (TryApply(build.staticModelLightingPatch))
		{
			++stats.appliedPatches;
		}

		groups[0] = { build.cellPlaneCountPatches, 2, CullingLevel::NearbyRooms, false };
		groups[1] = { &build.staticModelCullDistPatch, 1, CullingLevel::NoPropDistance, false };
		groups[2] = { build.wholeMapPatches, 2, CullingLevel::WholeMap, false };
		entityFrustumGroup = { build.entityFrustumPatches, 4, CullingLevel::Engine, false };
		lodFovGroup = { build.lodFovPatches, 2, CullingLevel::Engine, false };

		const bool isEntityCullingOff = TrySetGroupApplied(entityFrustumGroup, true);
		const bool isLodSteady = TrySetGroupApplied(lodFovGroup, true);

		Log() << "remix: scene entities and brush models no longer frustum culled (" << isEntityCullingOff
			  << "), model detail no longer follows the zoom (" << isLodSteady << ")";

		stats.isRadiusHooked = TryHookCall(build.R_AddWorldSurfacesDpvsCall,
			reinterpret_cast<const void*>(&AddWorldSurfacesDpvsDetour), reinterpret_cast<void**>(&originalAddWorldSurfacesDpvs));

		stats.isLodHooked = TryHookCall(build.XModelGetLodForDistCall, reinterpret_cast<const void*>(&GetLodForDistDetour),
			reinterpret_cast<void**>(&originalGetLodForDist));

		TrySetCullingLevel(CullingLevel::Engine);

		Log() << "remix: surface lists enlarged and static model lighting failures kept (" << stats.appliedPatches
			  << " of 4 patches), view culling left to the engine (radius " << appliedRadius << " when raised, walk hooked "
			  << stats.isRadiusHooked << ", static model LOD hooked " << stats.isLodHooked << ")";
	}

	static bool TryApplyLevel(CullingLevel level)
	{
		bool isComplete = true;

		for (PatchGroup& group : groups)
		{
			if (!group.patches)
			{
				return false;
			}

			const bool shouldApply = static_cast<int>(level) >= static_cast<int>(group.level);

			if (!TrySetGroupApplied(group, shouldApply))
			{
				isComplete = false;
			}
		}

		appliedLevel = level;

		Log() << "remix: view culling level " << static_cast<int>(level);

		return isComplete;
	}

	bool TrySetCullingLevel(CullingLevel level)
	{
		stats.level = level;

		if (mapLevel)
		{
			return true;
		}

		return TryApplyLevel(level);
	}

	void SetMapCullingLevel(std::optional<int> level)
	{
		if (level == mapLevel)
		{
			return;
		}

		mapLevel = level;

		if (!groups[0].patches)
		{
			return;
		}

		if (mapLevel)
		{
			Log() << "remix: this map's settings choose the view culling level";
			TryApplyLevel(static_cast<CullingLevel>(*mapLevel));
			return;
		}

		TryApplyLevel(stats.level);
	}

	bool IsCullingLevelFromMap()
	{
		return mapLevel.has_value();
	}

	static void ResolveKeptModels()
	{
		keptModels.clear();
		keptModelsVersion = GetMapSettingsVersion();
		keptModelsWorld = GetMapIdentity();

		const std::vector<std::string>& names = GetMapSettings().keptModels;
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (names.empty() || build.gfxWorld == 0)
		{
			return;
		}

		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));

		if (!world)
		{
			return;
		}

		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);
		const auto modelCount = ReadAt<std::uint32_t>(world + Iw4::worldSModelCount);

		if (!drawInsts)
		{
			return;
		}

		for (std::uint32_t instance = 0; instance < modelCount; ++instance)
		{
			const std::uintptr_t drawInst = drawInsts + instance * Iw4::smodelDrawInstStride;
			const auto model = ReadAt<std::uintptr_t>(drawInst + Iw4::smodelDrawInstModel);

			if (!model || keptModels.count(model) != 0)
			{
				continue;
			}

			const auto* modelName = ReadAt<const char*>(model + Iw4::xModelName);

			if (!modelName)
			{
				continue;
			}

			for (const std::string& name : names)
			{
				if (_stricmp(name.c_str(), modelName) == 0)
				{
					keptModels.insert(model);
					break;
				}
			}
		}

		Log() << "remix: " << keptModels.size() << " of " << names.size()
			  << " models the map settings keep are on this map, they are never culled by distance";
	}

	void UpdateCulling()
	{
		if (keptModelsVersion != GetMapSettingsVersion() || keptModelsWorld != GetMapIdentity())
		{
			ResolveKeptModels();
		}

		const MapSettings& mapSettings = GetMapSettings();
		float radius = tuning.radius;
		float propScale = tuning.propDistanceScale;

		if (mapSettings.cullingRadius)
		{
			radius = *mapSettings.cullingRadius;
		}

		if (mapSettings.propDistanceScale)
		{
			propScale = *mapSettings.propDistanceScale;
		}

		float camera[3] = {};
		stats.isInCullingArea = false;

		if (TryGetCameraOrigin(camera))
		{
			for (const CullingArea& area : mapSettings.cullingAreas)
			{
				bool isInside = true;

				for (int axis = 0; axis < 3; ++axis)
				{
					const float low = std::min(area.boxMin[axis], area.boxMax[axis]);
					const float high = std::max(area.boxMin[axis], area.boxMax[axis]);

					if (camera[axis] < low || camera[axis] > high)
					{
						isInside = false;
					}
				}

				if (isInside)
				{
					radius = area.radius;
					stats.isInCullingArea = true;
					break;
				}
			}
		}

		appliedRadius = std::clamp(radius, 0.0f, 100000.0f);
		appliedPropScale = std::clamp(propScale, 0.1f, 100.0f);
		stats.appliedRadius = appliedRadius;
		stats.appliedPropScale = appliedPropScale;
		stats.keptModels = static_cast<int>(keptModels.size());
	}

	void SetCullingTuning(const CullingTuning& settings)
	{
		tuning.radius = std::clamp(settings.radius, 0.0f, 100000.0f);
		tuning.propDistanceScale = std::clamp(settings.propDistanceScale, 0.1f, 100.0f);
	}

	CullingTuning GetCullingTuning()
	{
		return tuning;
	}

	void SetEntityCullingDisabled(bool isDisabled)
	{
		TrySetGroupApplied(entityFrustumGroup, isDisabled);
	}

	bool IsEntityCullingDisabled()
	{
		return entityFrustumGroup.isApplied;
	}

	void SetSteadyModelDetail(bool isEnabled)
	{
		TrySetGroupApplied(lodFovGroup, isEnabled);
	}

	bool IsSteadyModelDetail()
	{
		return lodFovGroup.isApplied;
	}

	CullingStats GetCullingStats()
	{
		return stats;
	}

	void LogEngineLimits()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.s_warnCount == 0 || build.warnFrameCount == 0)
		{
			return;
		}

		const auto frame = ReadAt<int>(Engine::RebaseAddress(build.warnFrameCount));
		const std::size_t limitCount = sizeof(engineLimits) / sizeof(engineLimits[0]);

		for (std::size_t i = 0; i < limitCount; ++i)
		{
			const auto nextRepeat = ReadAt<int>(Engine::RebaseAddress(build.s_warnCount) + engineLimits[i].id * sizeof(int));

			if (nextRepeat != 0 && nextRepeat >= frame)
			{
				hitEngineLimits |= 1ull << i;
			}
		}

		++engineLimitFrames;

		if (engineLimitFrames < engineLimitLogFrames)
		{
			return;
		}

		engineLimitFrames = 0;

		if (hitEngineLimits == 0)
		{
			return;
		}

		std::string text;

		for (std::size_t i = 0; i < limitCount; ++i)
		{
			if ((hitEngineLimits >> i) & 1ull)
			{
				char entry[64];
				std::snprintf(entry, sizeof(entry), "%d (%s) ", engineLimits[i].id, engineLimits[i].name);
				text += entry;
			}
		}

		hitEngineLimits = 0;

		Log() << "remix: engine limits hit in the last " << engineLimitLogFrames << " frames: " << text;
	}

	std::string DescribeEngineWarnings()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.s_warnCount == 0 || build.warnFrameCount == 0)
		{
			return "not mapped";
		}

		const auto frame = ReadAt<int>(Engine::RebaseAddress(build.warnFrameCount));
		std::string text;

		for (const int warning : watchedWarnings)
		{
			const auto nextRepeat = ReadAt<int>(Engine::RebaseAddress(build.s_warnCount) + warning * sizeof(int));

			if (nextRepeat == 0)
			{
				continue;
			}

			char entry[48];

			if (nextRepeat >= frame)
			{
				std::snprintf(entry, sizeof(entry), "%d (now) ", warning);
			}
			else
			{
				std::snprintf(entry, sizeof(entry), "%d (earlier) ", warning);
			}

			text += entry;
		}

		if (text.empty())
		{
			return "none";
		}

		return text;
	}
}
