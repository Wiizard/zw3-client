#pragma once

namespace Steam
{
	class Friends
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static const char* GetPersonaName(Interface* self);
		static void ActivateGameOverlay(Interface* self, const char* dialog);
		static void ActivateGameOverlayToStore(Interface* self, unsigned int appId, int flag);
	};
}
