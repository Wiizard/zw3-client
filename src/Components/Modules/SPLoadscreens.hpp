#pragma once

namespace Components
{
	class SPLoadscreens : public Component
	{
	public:
		SPLoadscreens();
		~SPLoadscreens();

		static void SetLoadingMap(const std::string& mapname);

	private:
		static std::string LoadingMap;
		static void(*OriginalMapCommand)();

		static void MapCommandStub();
		static void InstallMapCommandHook();
	};
}
