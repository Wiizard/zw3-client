#pragma once

#include <optional>
#include <string>

namespace Components::Remix
{
	enum class CullingLevel
	{
		Engine,
		NearbyRooms,
		NoPropDistance,
		WholeMap,
	};

	inline constexpr int cullingLevelCount = 4;

	struct CullingTuning
	{
		float radius = 1000.0f;
		float propDistanceScale = 1.0f;
	};

	struct CullingStats
	{
		int appliedPatches = 0;
		bool isRadiusHooked = false;
		bool isLodHooked = false;
		CullingLevel level = CullingLevel::Engine;
		float appliedRadius = 1000.0f;
		float appliedPropScale = 1.0f;
		int keptModels = 0;
		bool isInCullingArea = false;
	};

	void InstallCullingPatches();

	void LogEngineLimits();

	bool TrySetCullingLevel(CullingLevel level);

	int GetCameraCell();

	int CellForPoint(const float* point);

	void SetMapCullingLevel(std::optional<int> level);

	void SetCullingTuning(const CullingTuning& tuning);

	CullingTuning GetCullingTuning();

	void UpdateCulling();

	bool IsCullingLevelFromMap();

	void SetEntityCullingDisabled(bool isDisabled);

	bool IsEntityCullingDisabled();

	void SetSteadyModelDetail(bool isEnabled);

	bool IsSteadyModelDetail();

	CullingStats GetCullingStats();

	std::string DescribeEngineWarnings();
}
