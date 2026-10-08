#pragma once

namespace Steam
{
	class Utils
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static unsigned int GetAppID(Interface* self);
		static void SetOverlayNotificationPosition(Interface* self, int position);
	};
}
