#pragma once

namespace Components
{
	// Compatibility queries for callers of the retired Kino lobby scene.
	class LobbyScene
	{
	public:
		static bool IsTransitionActive();
		static void StartTransition(const std::string& mapName = {});
		static void StopTransition();
		static bool IsSceneReady();
		static bool IsStartupLoading();
		static bool IsCinematicActive();
		static void ClearRemoteScene();
		static bool DeferLaunch(const std::function<void()>& launch, const std::string& mapName = {});
	};
}
