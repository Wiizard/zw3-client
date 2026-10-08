#pragma once

namespace Components
{
	class SPLoadscreens : public Component
	{
	public:
		SPLoadscreens();

		static void SetLoadingMap(const std::string& name);
		static void PreloadMapPreview(const std::string& name);

		static void PatchConnectMenu();

		static void OnMenusFreed();
	};
}
