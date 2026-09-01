#pragma once

namespace Components
{
	class SPLoadscreens : public Component
	{
	public:
		SPLoadscreens();
		~SPLoadscreens();
		void preDestroy() override;

		static void SetLoadingMap(const std::string& mapname);
		static void PreloadMapPreview(const std::string& mapname);
		static void OnMenuFreed(Game::menuDef_t* menu);
	};
}
