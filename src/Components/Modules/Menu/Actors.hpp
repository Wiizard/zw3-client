#pragma once

#include "Stage.hpp"

namespace Components::Menu
{
	// Native animated entities submitted in the backdrop's world scene.
	class Actors : public Component
	{
	public:
		Actors();
		static bool IsReady();
		// The fraction of the segment that is clear of the map's world
		// geometry (1 when nothing is hit, 0 when it starts in solid).
		static float Clearance(const std::array<float, 3>& from, const std::array<float, 3>& to);
		static void Prepare();
		static void Reset();
		static void DrawScene(const BackdropData::spawn& anchor, std::string_view map, float deltaSeconds);
		static std::string GetSceneSnapshot();
		static void ReceiveSceneSnapshot(const std::string& packet, bool immediate = false);
		static void PollLocalScene();
		static void ClearRemoteScene();
		static void ReceiveSceneClock(unsigned int clientSent, unsigned int hostTime);
	};
}
