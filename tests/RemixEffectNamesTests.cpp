#include "Components/Modules/Remix/EffectNames.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
	try
	{
		using namespace Components::Remix;
		if (ClassifyScriptEffectName("zw3/poison_lake") != ScriptEffectKind::Ground
			|| UsesSoftParticleFade(ClassifyScriptEffectName("zw3/poison_lake")))
			throw std::runtime_error("The poison pool must not fade against its supporting floor");
		if (!UsesSoftParticleFade(ScriptEffectKind::Particle) || UsesSoftParticleFade(ScriptEffectKind::Eye))
			throw std::runtime_error("Ground FX protection changed smoke or zombie-eye rendering");
		if (ClassifyScriptEffectName("zw3/poison_lake_smoke") != ScriptEffectKind::Particle)
			throw std::runtime_error("Only the exact poison pool FX should bypass soft-particle fading");
		for (const std::string_view name : {
			"smc_t6/zm_prison/fx/c85ed9d7224fff72062bb1d6bdb279fc",
			"zw3/redeye", "zw3/blueeye", "zw3/purpleeye", "zw3/greeneye", "zw3/orangeeye",
			"misc/aircraft_light_wingtip_red", "misc/aircraft_light_wingtip_green",
			"zombie/eye_glow_red_single", "zombie/eye_glow_blue_single", "zombie/eye_glow_lightyellow" })
		{
			if (ClassifyScriptEffectName(name) != ScriptEffectKind::Eye)
				throw std::runtime_error("An eye colour lost its separate non-particle path: " + std::string(name));
		}
		for (const std::string_view name : { "maps/mp_maps/fx_mp_ash_falling_large", "zw3/snow", "misc/glow_weapon_fx" })
		{
			if (ClassifyScriptEffectName(name) != ScriptEffectKind::Particle)
				throw std::runtime_error("A protected non-eye effect was lost or received eye size/brightness adjustments");
		}
		for (const std::string_view name : { "", "misc/lamp_flare", "maps/ambient_smoke",
			"smc_t6/zm_prison/fx/unrelated", "maps/mp_maps/fx_mp_ash_falling_large_unrelated" })
		{
			if (ClassifyScriptEffectName(name) != ScriptEffectKind::None)
				throw std::runtime_error("An unrelated effect bypassed its normal filtering");
		}
		EffectSurfaces surfaces;
		const int eyeIndices = 0;
		const int ashIndices = 0;
		const int unrelatedIndices = 0;
		const int groundIndices = 0;
		surfaces.BeginFrame();
		surfaces.Add(&eyeIndices, ScriptEffectKind::Eye);
		surfaces.Add(&ashIndices, ScriptEffectKind::Particle);
		surfaces.Add(&groundIndices, ScriptEffectKind::Ground);
		if (surfaces.Get(&eyeIndices) != ScriptEffectKind::Eye
			|| surfaces.Get(&ashIndices) != ScriptEffectKind::Particle
			|| surfaces.Get(&groundIndices) != ScriptEffectKind::Ground
			|| surfaces.Get(&unrelatedIndices) != ScriptEffectKind::None)
			throw std::runtime_error("Eye, particle and ordinary surfaces must remain distinct even with shared textures");

		surfaces.BeginFrame();
		if (surfaces.Get(&eyeIndices) != ScriptEffectKind::Eye)
			throw std::runtime_error("The render thread lost the previous frame's surfaces");
		surfaces.Add(&eyeIndices, ScriptEffectKind::Particle);
		if (surfaces.Get(&eyeIndices) != ScriptEffectKind::Particle)
			throw std::runtime_error("An old eye classification overrode a reused current-frame address");

		surfaces.BeginFrame();
		if (surfaces.Get(&ashIndices) != ScriptEffectKind::None)
			throw std::runtime_error("An expired surface retained its effect classification");

		std::cout << "Remix effects: poison pool without intersection fading, eyes, ash/snow and frame-buffer reuse passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
