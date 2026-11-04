#pragma once

#include "FxSpotlightLifecycle.hpp"

namespace Components::FxSpotlight
{
	inline constexpr std::uintptr_t spawnAddress = 0x14015A3C0;
	inline constexpr std::uintptr_t releaseAddress = 0x140157850;
	inline constexpr std::uintptr_t sites[] = {
		0x14015BB8A, 0x14015DE6B, 0x14015E18B,
		0x14015E4DA, 0x14015E676, 0x14015E78A
	};
	inline Utils::Hook hooks[std::size(sites)];
	using Spawn = void(*)(void*, void*, int, const void*, int, int);
	using Release = void(*)(void*, unsigned int, void*);

	inline void SpawnHook(void* system, void* effect, int index, const void* frame, int time, int sequence)
	{
		const auto release = reinterpret_cast<Release>(Utils::Hook::Rebase(releaseAddress));
		if (FxSpotlightLifecycle::Prepare(system, effect, index, release))
		{
			reinterpret_cast<Spawn>(Utils::Hook::Rebase(spawnAddress))(system, effect, index, frame, time, sequence);
		}
	}

	inline bool Install()
	{
		const std::uint8_t releasePrefix[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10 };
		if (!Utils::Hook::MatchesBytes(releaseAddress, releasePrefix, sizeof(releasePrefix))) return false;
		for (const auto site : sites)
		{
			if (!Utils::Hook::BranchesTo(site, spawnAddress, false)) return false;
		}
		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			if (!hooks[i].Initialize(sites[i], reinterpret_cast<void*>(SpawnHook), HOOK_CALL)->Install()->IsInstalled())
			{
				for (auto& hook : hooks) hook.Uninstall();
				return false;
			}
		}
		return true;
	}
}
