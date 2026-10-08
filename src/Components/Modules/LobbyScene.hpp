#pragma once

namespace Components
{
	class LobbyScene : public Component
	{
	public:
		LobbyScene();

		static bool IsTransitionActive();
		static void StartTransition();
		static void StopTransition();
		static bool IsSceneReady();
		static bool IsStartupLoading();
		static bool IsCinematicActive();
		static void PrepareStartup();
		static std::string GetSceneSnapshot();
		static void ReceiveSceneSnapshot(const std::string& packet, bool immediate = false);
		static void PollLocalScene();
		static void ClearRemoteScene();
		static void ReceiveSceneClock(unsigned int clientSent, unsigned int hostTime);
		static bool DeferLaunch(const std::function<void()>& launch);
	};
}
