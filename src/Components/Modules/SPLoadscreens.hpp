#pragma once

namespace Components
{
	class SPLoadscreens : public Component
	{
	public:
		SPLoadscreens();
		~SPLoadscreens();

	private:
		static void(*OriginalMapCommand)();
		static void InstallMapCommandHook();
	};
}
