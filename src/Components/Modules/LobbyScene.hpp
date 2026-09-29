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
		static void PrepareStartup();
		static bool DeferLaunch(const std::function<void()>& launch);
	};
}
