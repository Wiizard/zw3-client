#include "../src/Components/Modules/Remix/CutoutDecalPolicy.hpp"

#include <cassert>
#include <iostream>

using namespace Components::Remix;

int main()
{
	// Sumpf dump: paper/thatch/rug use opaque-region, sort 9, replace blend, alpha test at half.
	for (const auto name : { "wc/decal_papers_japanese", "wc/mtl_sumpf_decal_thatch_1", "wc/com_ch_me_rug_01" })
	{
		assert(IsCutoutDecalState(0, 9, 0x012, true) && IsCutoutDecalName(name));
		assert(IsCutoutDecalState(1, 9, 0x012, true) && IsCutoutDecalName(name));
	}
	assert(!IsCutoutDecalName("wc/makin_wall_wood_planks02"));
	assert(!IsCutoutDecalName("wc/foliage_leaves"));
	assert(!IsCutoutDecalName("decal_folder/wall"));
	assert(!IsCutoutDecalState(0, 9, 0x012, false));
	assert(!IsCutoutDecalState(2, 9, 0x012, true));
	assert(!IsCutoutDecalState(0, 1, 0x012, true));
	assert(!IsCutoutDecalState(0, 29, 0x012, true));
	assert(!IsCutoutDecalState(0, 9, 0x165, true));

	assert(IsConvertedDecalAlias("sac_bo1_local/mp_zombie_cod5_sumpf_23_2027806700379584198", 0, 9, 0x012, true));
	assert(IsConvertedDecalAlias("generated/_5n_5n_61n", 1, 9, 0x162, false));
	assert(!IsConvertedDecalAlias("wc/foliage_leaves", 0, 9, 0x012, true));
	assert(!IsConvertedDecalAlias("sac_bo1_local/opaque_floor", 0, 1, 0x012, false));
	assert(!IsConvertedDecalAlias("sac_bo1_local/opaque_floor", 0, 9, 0x012, false));
	assert(!IsConvertedDecalAlias("generated/effect", 2, 9, 0x165, false));

	// Aliases alone cannot tag arbitrary cut-out geometry; a genuine opaque user always wins.
	assert(HasNonDecalGeometry(false, true, false));
	assert(!HasNonDecalGeometry(false, true, true));
	assert(HasNonDecalGeometry(true, true, true));
	assert(HasNonDecalGeometry(true, false, true));
	std::cout << "Remix cut-out decal tests passed\n";
}
