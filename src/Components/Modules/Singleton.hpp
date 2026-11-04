#pragma once

namespace Components
{
	class Singleton : public Component
	{
	public:
		Singleton();

		static bool IsFirstInstance();
		// 1 for the first instance; extra local instances take the lowest free
		// slot (2, 3, ...) so each keeps its own saved identity between launches.
		static int GetInstanceSlot();

	private:
		static HANDLE mutex;
		static HANDLE slotMutex;
		static bool isFirstInstance;
		static int instanceSlot;
	};
}
