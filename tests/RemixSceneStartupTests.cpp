#include "Components/Modules/Remix/SceneWarmup.hpp"
#include "Components/Modules/Remix/WaterMaterialPolicy.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
	using namespace Components::Remix;
	try
	{
		const auto require = [](bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		};
		const char* waterTechnique = "wc_l_sm_ua_b0c0n0s0p0_nocast";
		require(IsScrollingWaterMaterial("wc/oilrig_ocean_blend_scrolling2_shallows", waterTechnique,
			"water_ocean_island_shallow_scrolling_col", 1), "Island shallows were not recognized as water");
		require(IsScrollingWaterMaterial("wc/oilrig_ocean_blend_scrolling2_underwater", waterTechnique,
			"water_ocean_island_scrolling_col", 1), "Island ocean was not recognized as water");
		require(IsScrollingWaterMaterial("wc/za_island_river", waterTechnique,
			"water_ocean_island_scrolling_col", 1), "Shared river texture lost its water classification");
		require(IsScrollingWaterMaterial("wc/za_water_clear_scrolling", waterTechnique,
			"za_water_solid_scrolling_col", 1), "Existing scrolling water support was lost");
		for (const auto* name : { "wc/water_foam", "wc/waterfall", "wc/metal_wall", "wc/ocean_foam" })
		{
			require(!IsScrollingWaterMaterial(name, waterTechnique, "ordinary_col", 1),
				"Foam, waterfalls or ordinary geometry were made into a solid water surface");
		}
		require(!IsScrollingWaterMaterial("water", "unlit_blend", "water_ocean", 2), "A water sprite became world water");
		require(!IsScrollingWaterMaterial("wc/water", "wc_l_sm_r0c0", "water_ocean", 1), "Opaque geometry became water");

		SceneWarmup warmup;
		require(warmup.Opacity(0) == 0, "The main menu was covered");
		warmup.ObserveScene(123, 100);
		require(warmup.Opacity(100) == 1, "The first scene was exposed before settling");
		warmup.ObserveScene(123, 5000);
		require(warmup.Opacity(5000) == 1, "A slow first frame bypassed the minimum rendered frame count");
		for (int frame = 0; frame < 6; ++frame) warmup.ObserveScene(123, 5000);
		require(warmup.Opacity(5000) == 1 && warmup.Opacity(5125) == 0.5f && warmup.Opacity(5250) == 0,
			"The scene did not fade in after rendering behind the cover");
		warmup.ObserveScene(456, 6000);
		require(warmup.Opacity(6000) == 1, "A map change reused the previous scene's warmup");
		warmup.Reset();
		require(warmup.Opacity(6000) == 0, "Disconnect or device reset left the menu covered");
		warmup.ObserveScene(456, 7000);
		require(warmup.Opacity(7000) == 1, "Reloading the same map after disconnect skipped warmup");
		std::cout << "Remix water classification and scene warmup tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
