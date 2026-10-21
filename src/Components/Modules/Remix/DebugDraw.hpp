#pragma once

namespace Components::Remix
{
	struct DebugDrawSettings
	{
		bool isLights = false;
		bool isMarkers = false;
		bool isTriggers = false;
		bool isCullingAreas = false;
		float lineWidth = 1.0f;
		float lightRange = 3000.0f;
	};

	struct DebugDrawStats
	{
		int lines = 0;
		bool didFail = false;
	};

	void SetDebugDrawSettings(const DebugDrawSettings& settings);

	DebugDrawSettings GetDebugDrawSettings();

	void DrawDebugShapes(IDirect3DDevice9* device);

	void ReleaseDebugDraw();

	DebugDrawStats GetDebugDrawStats();
}
