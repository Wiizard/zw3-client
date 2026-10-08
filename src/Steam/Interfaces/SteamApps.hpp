#pragma once

namespace Steam
{
	class Apps
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static bool BIsDlcInstalled(Interface* self, unsigned int appId);
	};
}
