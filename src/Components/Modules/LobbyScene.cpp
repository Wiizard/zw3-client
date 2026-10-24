#include "STDInclude.hpp"
#include "LobbyScene.hpp"

namespace Components
{
	bool LobbyScene::IsTransitionActive()
	{
		return false;
	}

	bool LobbyScene::IsSceneReady()
	{
		return false;
	}

	bool LobbyScene::IsStartupLoading()
	{
		return false;
	}

	bool LobbyScene::IsCinematicActive()
	{
		constexpr std::uintptr_t cinematicGlobBink = 0x1493C9BA8;
		if (Utils::Hook::Get<void*>(cinematicGlobBink)) return true;

		const auto state = Game::CL_GetLocalClientConnectionState(0);
		return state == Game::CA_CINEMATIC || state == Game::CA_LOGO;
	}

	void LobbyScene::StartTransition(const std::string&)
	{
	}

	void LobbyScene::StopTransition()
	{
	}

	void LobbyScene::ClearRemoteScene()
	{
	}

	bool LobbyScene::DeferLaunch(const std::function<void()>&, const std::string&)
	{
		// The caller performs its normal launch immediately.
		return false;
	}
}
