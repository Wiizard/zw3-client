#pragma once

namespace Components::BotAI::Navgen
{
	bool TryLoadFile(const char* mapName);
	bool IsLoadedStale();

	void Generate(const char* mapName, const char* reason);

	void RegisterCommands();
	void ApplyQueuedEdits(const char* mapName);

	bool IsGenQueueActive();
	void RunGenQueue(const char* mapName);

	int GlassHitId(const float* start, const float* end, const float* bounds);

	int GlassPieceCount();
	bool GlassPieceOrigin(int piece, float out[3]);
	int GlassPieceDamage(int piece);

	bool MantleFaceAhead(const float* start, const float* end, float* outNormalZ);

	void RefreshSightBlockers(const char* mapName);
	bool IsRenderBlocked(const float* from, const float* to);
	float PenetrationShare(const float* from, const float* to, int penetrateType, float depthScale);
	bool TryLoadAnnotations(const char* mapName);
	void AnnotateGraph(const char* mapName);

	struct WallEdge
	{
		float from[2];
		float to[2];
	};

	void RefreshWalls(const char* mapName);
	bool HasWallGeometry();
	int SliceWallsNear(const float* around, float radius, float z, WallEdge* out, int maxEdges);

	void RegisterOverlay();
}
