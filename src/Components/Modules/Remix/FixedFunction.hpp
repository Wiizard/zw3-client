#pragma once

#include <cstdint>
#include <vector>

namespace Components::Remix
{
	struct FixtureSpot
	{
		float position[3];
		float normal[3];
	};

	struct WorldStats
	{
		bool isHookInstalled = false;
		unsigned vertexCount = 0;
		unsigned indexCount = 0;
		unsigned lastViewSurfaces = 0;
		unsigned long long builds = 0;
		unsigned long long fixedFunctionLists = 0;
		unsigned long long passedThroughLists = 0;
		unsigned long long skyLists = 0;
		bool isBrushModelHookInstalled = false;
		unsigned long long brushModelLists = 0;
		unsigned long long blendLayerLists = 0;
		bool isStaticModelHookInstalled = false;
		unsigned long long staticModelLists = 0;
		unsigned staticModelSurfaces = 0;
		bool isEntityModelHookInstalled = false;
		unsigned long long entityModelLists = 0;
		bool isSkinnedModelHookInstalled = false;
		unsigned long long skinnedModelLists = 0;
		unsigned long long cachedSkinnedSurfaces = 0;
		unsigned long long scrollingLists = 0;
		unsigned camoTextures = 0;
		unsigned long long cutoutDecalLists = 0;
		unsigned long long dynamicLightLists = 0;
		unsigned materialBatches = 0;
		unsigned long long batchDraws = 0;
		unsigned long long shaderTintLists = 0;
		unsigned staticModelBatches = 0;
		unsigned bakedStaticModels = 0;
		unsigned long long staticBatchDraws = 0;
		bool isPropListFilterHooked = false;
		unsigned completedWorldBatches = 0;
		unsigned completedPropBatches = 0;
		unsigned completedBigProps = 0;
		unsigned layerTwins = 0;
		unsigned keptBakedProps = 0;
		unsigned hiddenBakedProps = 0;
	};

	bool InstallWorldHooks();

	void BeginSceneView(IDirect3DDevice9* device);

	void DrawCulledBatches(IDirect3DDevice9* device);

	void RerollAlienColours();

	std::vector<FixtureSpot> CollectFixtureSpots();

	unsigned GetSurfaceSpotsVersion();

	IDirect3DTexture9* GetMaterialColorMap(std::uintptr_t material);

	void EndSceneView();

	void ReleaseWorldBuffers();

	void RequestSceneDump();

	void WriteMapMaterialReport();

	IDirect3DTexture9* TryGetIgnoredColorMapTwin(IDirect3DDevice9* device, std::uintptr_t material);

	IDirect3DTexture9* TryGetParticleColorMapTwin(IDirect3DDevice9* device, std::uintptr_t material);

	bool IsScriptEffectSurface(const void* indices);

	void RecordDumpEffect(IDirect3DDevice9* device, std::uintptr_t material, const char* action);

	void SetPlayerBodyShadow(bool isEnabled);

	bool IsPlayerBodyShadow();

	WorldStats GetWorldStats();
}
