#include "Components/Modules/Remix/EffectNames.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace Components::Remix
{
	struct TestMaterial
	{
		const char* name;
		const char** technique;
	};

	namespace Iw4 { constexpr auto materialTechniqueSet = offsetof(TestMaterial, technique); }
	template <typename T> T ReadAt(std::uintptr_t address) { return *reinterpret_cast<const T*>(address); }

	// Retain the old cache declaration to make the test fail against the pre-fix implementation too.
	static std::unordered_map<std::uintptr_t, ScriptEffectKind> scriptEffectDefs;
#include "EffectAssetLookups.inc"
}

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using namespace Components::Remix;
		char name[96] = "maps/ambient_smoke";
		const char* definition = name;
		const auto definitionAddress = reinterpret_cast<std::uintptr_t>(&definition);
		Require(GetScriptEffectKind(definitionAddress) == ScriptEffectKind::None, "ordinary FX must use the normal path");
		strcpy_s(name, "smc_t6/zm_prison/fx/c85ed9d7224fff72062bb1d6bdb279fc");
		Require(GetScriptEffectKind(definitionAddress) == ScriptEffectKind::Eye,
			"a zombie-eye FX reusing an old definition address must regain eye protection");
		strcpy_s(name, "maps/mp_maps/fx_mp_ash_falling_large");
		Require(GetScriptEffectKind(definitionAddress) == ScriptEffectKind::Particle, "ash reusing an eye address must stay a particle");
		strcpy_s(name, "zw3/poison_lake");
		Require(GetScriptEffectKind(definitionAddress) == ScriptEffectKind::Ground,
			"a poison pool reusing an ash definition must regain floor-intersection protection");

		const char* technique = "effect_blend";
		TestMaterial material{ name, &technique };
		const auto materialAddress = reinterpret_cast<std::uintptr_t>(&material);
		Require(EyeOffsetDistance(materialAddress) == 0.0f, "ordinary particles have no eye offset");
		technique = "effect_eyeoffset";
		strcpy_s(name, "gfx_eyeglow_red_eye10");
		Require(EyeOffsetDistance(materialAddress) == 10.0f, "reused eye materials must restore their camera offset");
		strcpy_s(name, "gfx_eyeglow_blue_eye3");
		Require(EyeOffsetDistance(materialAddress) == 3.0f, "a different eye material must use its current offset");

		strcpy_s(name, "gfx_lamp_glow_eye10");
		Require(IsEyeOffsetLampFlare(materialAddress), "lamp flares remain hidden");
		strcpy_s(name, "gfx_eyeglow_red_eye10");
		Require(!IsEyeOffsetLampFlare(materialAddress), "an eye reusing a lamp material address must not stay hidden");
		strcpy_s(name, "gfx_lamp_glow_eye10");
		Require(IsEyeOffsetLampFlare(materialAddress), "a reused lamp address must restore lamp filtering");
		Require(GetScriptEffectKind(0) == ScriptEffectKind::None && EyeOffsetDistance(0) == 0.0f,
			"missing definitions and materials must stay safe");
		std::cout << "Effect asset reuse: zombie-eye recognition, material offsets and lamp filtering passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
