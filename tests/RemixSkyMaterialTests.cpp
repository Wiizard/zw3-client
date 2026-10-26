#include "Components/Modules/Remix/SkyMaterialPolicy.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
	using Components::Remix::IsSkyTextureMaterial;
	try
	{
		if (IsSkyTextureMaterial("wc/zm_asylum_sh_wall_n02", "wc_l_sm_r0c0n0s0", 2)
			|| IsSkyTextureMaterial("wc/metal_wall", "wc_l_sm_r0c0", 2)
			|| IsSkyTextureMaterial("wc/plain_panel", "wc_unlit_replace", 2)
			|| IsSkyTextureMaterial("mc/mtl_motion_tracker_screen", "mc_unlit_replace_lin_ua", 2)
			|| IsSkyTextureMaterial("wc/sky_sign", "wc_l_sm_r0c0", 2))
		{
			throw std::runtime_error("Solid geometry or a screen was incorrectly tagged as sky");
		}

		if (!IsSkyTextureMaterial("wc/sky_factory", "wc_sky", 2)
			|| !IsSkyTextureMaterial("wc/sky", "sky", 2)
			|| !IsSkyTextureMaterial("mc/backlot_skydome", "mc_unlit_replace", 2)
			|| !IsSkyTextureMaterial("wc/panorama", "wc_unlit_replace", 2)
			|| !IsSkyTextureMaterial("wc/bg_facades_city", "wc_unlit_replace", 1))
		{
			throw std::runtime_error("An actual sky or converted background lost its sky classification");
		}

		std::cout << "Remix sky: Factory walls retain occlusion; sky shaders and converted skydomes remain supported\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
