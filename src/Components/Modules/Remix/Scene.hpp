#pragma once

#include <cstdint>
#include <string>

namespace Components::Remix
{
	struct RemixStats
	{
		bool isHookInstalled = false;
		bool hasDevice = false;
		unsigned long long cameraFrames = 0;
		unsigned long long skippedViews = 0;
		unsigned long long injectionFrames = 0;
		int suppressedPasses = 0;
		bool isEffectAlphaHooked = false;
		bool isMotionTrackerHooked = false;
		bool isMarksBlendHooked = false;
		bool isParticleCloudColourHooked = false;
	};

	enum class EffectKind
	{
		CodeMesh,
		Marks,
		Glass,
		ParticleClouds,
		Sparks,
		BulletTracers,
	};

	inline constexpr int effectKindCount = 6;

	bool InstallHooks();

	bool TryGetCameraOrigin(float* origin);

	bool TryProjectToScreen(const float* point, float width, float height, float* screen);

	std::uintptr_t GetMapIdentity();

	std::string GetMapName();

	// The map whose settings and lights the menu's own 3D scene uses (the lobby draws Kino). Set every frame the
	// lobby is drawn in Remix mode; empty once it has not been for a few frames or while a map is loaded.
	void SetMenuSceneMap(const std::string& mapName);

	std::string GetMenuSceneMap();

	void SetGlowBrightness(float brightness);

	void SetEffectCullingDisabled(bool isDisabled);

	void SetWeatherEnabled(bool isEnabled);

	void SetOffscreenWorldEnabled(bool isEnabled);

	void SetMapColourGrade(bool isEnabled);

	bool IsMapColourGrade();

	void SetMapColourGradeStrength(float strength);

	float GetMapColourGradeStrength();

	bool IsEffectCullingDisabled();

	bool IsWeatherEnabled();

	bool IsOffscreenWorldEnabled();

	bool IsRenderFrameLocked();

	void ApplyThreadedRenderer(bool isThreaded);

	float GetGlowBrightness();

	IDirect3DDevice9* GetSceneDevice();

	bool IsEffectDrawn(EffectKind kind);

	bool TrySetEffectDrawn(EffectKind kind, bool isDrawn);

	void OnPresent(IDirect3DDevice9* device);

	// The scene keeps rendering while exposure and asynchronous material replacements settle behind this cover.
	float GetSceneStartupOpacity();

	void OnPostEffectsBegin();

	void MarkUiPhase(IDirect3DDevice9* device);

	void OnDeviceLost();

	RemixStats GetStats();
}
