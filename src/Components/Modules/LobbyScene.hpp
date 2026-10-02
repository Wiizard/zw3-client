#pragma once

namespace Components
{
	class LobbyScene : public Component
	{
	public:
		LobbyScene();
		~LobbyScene();

		static bool IsTransitionActive();
		static void StartTransition();
		static void StopTransition();
		static bool IsSceneReady();
		static bool IsStartupLoading();
		static bool IsCinematicActive();
		static void PrepareStartup();
		static void ReleaseResources();
		static bool DeferLaunch(const std::function<void()>& launch);
	};
}
