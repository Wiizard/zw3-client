#pragma once

#include "RendererDepthBiasPolicy.hpp"
#include <bit>

namespace Components::RendererDepthBias
{
	inline Utils::Hook hooks[2];
	constexpr std::uintptr_t nativePolygonOffset = 0x140069670;
	constexpr std::uintptr_t worldAddress = 0x148CCC7A0;

	inline bool IsConvertedWorld()
	{
		const auto* world = Utils::Hook::Get<Game::GfxWorld*>(worldAddress);
		if (!world || !world->surfaceCount || !world->dpvs.surfaces) return false;
		const auto* material = world->dpvs.surfaces[0].material;
		return material && material->techniqueSet
			&& RendererDepthBiasPolicy::IsConvertedWorldTechnique(material->techniqueSet->name);
	}

	inline void SetPolygonOffset(IDirect3DDevice9* device, std::uint32_t stateBits)
	{
		if (!RendererDepthBiasPolicy::UsesPreciseDecalBias(IsConvertedWorld(), stateBits))
		{
			reinterpret_cast<void(*)(IDirect3DDevice9*, std::uint32_t)>(Utils::Hook::Rebase(nativePolygonOffset))(device, stateBits);
			return;
		}
		// Match both native entry paths, including forced state restoration. Do not
		// change dvars, cached state bits, shadow offsets, or other games' materials.
		const float bias = RendererDepthBiasPolicy::DecalBias(stateBits);
		device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
		device->SetRenderState(D3DRS_DEPTHBIAS, std::bit_cast<DWORD>(bias));
	}

	inline bool Install()
	{
		constexpr std::uintptr_t sites[] = {0x140068D71, 0x14006994C};
		constexpr std::uint8_t nativeBytes[] = {0x40,0x53,0x48,0x83,0xec,0x20,0x83,0xe2,0x30};
		if (!Utils::Hook::MatchesBytes(nativePolygonOffset, nativeBytes, sizeof(nativeBytes))) return false;
		for (const auto site : sites)
			if (!Utils::Hook::BranchesTo(site, nativePolygonOffset, false)) return false;
		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			if (!hooks[i].Initialize(sites[i], SetPolygonOffset, HOOK_CALL)->Install()->IsInstalled())
			{
				for (auto& hook : hooks) hook.Uninstall();
				return false;
			}
		}
		return true;
	}
}
