#pragma once

namespace Components
{
	class Sound : public Component
	{
	public:
		Sound();

		static void PrepareLobbyRoundStart(const std::string& wav);
		static void PlayLobbyRoundStart();
	};
}
