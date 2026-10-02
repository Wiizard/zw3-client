#pragma once

namespace Components
{
	class Sound : public Component
	{
	public:
		Sound();
		void preDestroy() override;
		static void PrepareLobbyRoundStart(const std::string& wav);
		static void PlayLobbyRoundStart();

	private:
		static int  Init();
		static void Loop();
		static void UpdateFrontendVolume(int milliseconds);
	};
}
