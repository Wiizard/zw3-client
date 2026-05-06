#pragma once

namespace Components
{
	// MemoryTuning applies engine-wide memory-pressure defaults that are
	// sensible for a heavy mod running in a 32-bit address space.
	//
	// Specifically: it floors r_picmip_bump and r_picmip_spec at a sane
	// minimum so bump & specular maps never ship at full resolution. This
	// saves significant VRAM / committed VA because these two channels
	// account for roughly half of the total texture footprint of MW2 assets.
	//
	// Diffuse (r_picmip), manual (r_picmip_manual) and water picmips are
	// left untouched — those are visually impactful.
	//
	// Enforcement: a single one-shot Scheduler::OnGameInitialized callback
	// that runs after config_mp.cfg has been exec'd. It reads the current
	// dvar values and forces them back up to the floor if the user's
	// persisted config pushed them below. Fires exactly once per launch,
	// before any textures are uploaded.
	//
	// Override: if the dvar `zw3_mem_allowHighPicmip` is 1, the callback
	// becomes a no-op for the session.
	class MemoryTuning : public Component
	{
	public:
		MemoryTuning();

	private:
		static void EnforcePicmipFloors();
	};
}
