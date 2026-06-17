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

	private:
		static void(*OriginalMapCommand)();
		static void(*OriginalDisconnectCommand)();
		static void InstallMapCommandHook();
		static void InstallDisconnectCommandHook();
	};
}
