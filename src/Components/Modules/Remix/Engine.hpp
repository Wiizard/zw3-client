#pragma once

#include <functional>

#include "Iw4.hpp"

namespace Components::Remix::Engine
{
	using PrePostCallback = void (*)();

	struct DrawSurfLayout
	{
		std::uint64_t groupMask;
		std::uint64_t usesGfxEntity;
		int hasEntityBit;
		unsigned smodelListCountShift;
	};

	struct MaterialPool
	{
		std::uintptr_t pool;
		std::uintptr_t entries;
		std::size_t count;
		std::size_t stride;
	};

	const Iw4::GameBuild& CurrentBuild();

	const DrawSurfLayout& CurrentDrawSurfLayout();

	MaterialPool CurrentMaterialPool();

	bool SetDvar(const char* name, const char* value);

	bool EngineAddressable();

	std::uintptr_t RebaseAddress(std::uintptr_t idbAddress);

	bool InstallPrePostHook(PrePostCallback callback);

	bool IsRendererThreaded();

	void RunOnMainThread(std::function<void()> task);

	void RunMainThreadTasks();

	unsigned MainFrameNumber();
}
