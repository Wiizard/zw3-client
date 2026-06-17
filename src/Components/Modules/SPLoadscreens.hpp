#pragma once

namespace Components
{
	class SPLoadscreens : public Component
	{
	public:
		SPLoadscreens();
		~SPLoadscreens();

		static void SetLoadingMap(const std::string& mapname);
		static void PreloadMapPreview(const std::string& mapname);

	private:
		static void(*OriginalMapCommand)();
		static void InstallMapCommandHook();
	};
}
