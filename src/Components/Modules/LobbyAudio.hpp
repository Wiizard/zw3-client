#pragma once

namespace Components
{
	class LobbyAudio
	{
	public:
		static void PrepareLobbyRoundStart(const std::string& wav);
		static void PlayLobbyRoundStart();
		static void Poll();
		static void Shutdown();
	};
}
