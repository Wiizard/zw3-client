#include "MemoryTuning.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"

namespace Components
{
	// Minimum allowed picmip values. See header for rationale.
	//   0 -> native resolution (what the shipped config currently says)
	//   1 -> 1/4 memory per texture
	//   2 -> 1/16 memory per texture (our floor)
	constexpr int kPicmipFloorBump = 2;
	constexpr int kPicmipFloorSpec = 2;

	// -----------------------------------------------------------------
	// Post-config enforcement (one-shot)
	//
	// Runs once, after the engine has exec'd config_mp.cfg. Reads the
	// current dvar values and, if the user's persisted config pushed
	// them below the floor, forces them back up via a direct dvar set.
	// This covers existing users whose config_mp.cfg already contains
	// `seta r_picmip_bump "0"`.
	//
	// Crucially: this runs *before* the main menu renders a single
	// frame, so no texture has been uploaded yet and the forced value
	// applies to everything the renderer does from here on.
	//
	// Previous versions also hooked Dvar_RegisterInt to bump the
	// initial default at registration time (Layer 1). That hook fired
	// hundreds of times during startup and the repeated uninstall /
	// install cycle of the 5-byte JMP patch caused sporadic launch
	// crashes. Since this one-shot callback runs before any textures
	// are uploaded and achieves the same end result, Layer 1 was
	// removed entirely.
	// -----------------------------------------------------------------
	void MemoryTuning::EnforcePicmipFloors()
	{
		// Respect the opt-out.
		if (const auto* opt = Game::Dvar_FindVar("zw3_mem_allowHighPicmip"))
		{
			if (opt->current.enabled) return;
		}

		struct { const char* name; int floor; } picmips[] = {
			{ "r_picmip_bump", kPicmipFloorBump },
			{ "r_picmip_spec", kPicmipFloorSpec },
		};

		for (const auto& p : picmips)
		{
			auto* dvar = Game::Dvar_FindVar(p.name);
			if (dvar == nullptr) continue;

			if (dvar->current.integer < p.floor)
			{
				Logger::Print(Game::CON_CHANNEL_SYSTEM,
					"MemoryTuning: raising {} from {} to {} (address-space safety)\n",
					p.name, dvar->current.integer, p.floor);

				Game::Dvar_SetInt(dvar, p.floor);
			}
		}
	}

	// -----------------------------------------------------------------
	// Component wiring
	// -----------------------------------------------------------------
	MemoryTuning::MemoryTuning()
	{
		// Register the opt-out dvar so users can see it in `dvarlist`.
		Game::Dvar_RegisterBool("zw3_mem_allowHighPicmip", false,
			Game::DVAR_ARCHIVE,
			"Disable the engine-enforced minimum for r_picmip_bump / "
			"r_picmip_spec. Set to 1 only if you have a lot of VRAM and "
			"want full-resolution bump/spec maps. Default: 0.");

		// One-shot enforcement after config_mp.cfg has been exec'd but
		// before gameplay starts. No hooks on hot-path engine functions.
		Scheduler::OnGameInitialized(EnforcePicmipFloors,
			Scheduler::Pipeline::MAIN, 0ms);
	}
}
