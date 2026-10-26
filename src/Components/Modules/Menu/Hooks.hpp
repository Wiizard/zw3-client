#pragma once

namespace Components::Menu
{
	struct HookRequest
	{
		std::uintptr_t address;
		std::uintptr_t target;
		void* replacement;
		bool isJump = false;
		// Chain a call already owned by this client, preserving its behavior.
		void** original = nullptr;
	};
	bool InstallHooks(std::span<const HookRequest> requests);
	void RemoveHooks();
	bool ObserveZoneLoads();
	bool ObserveStreamStops(void(*replacement)(int), void(**original)(int));
}
