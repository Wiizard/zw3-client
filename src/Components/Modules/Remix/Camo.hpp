#pragma once

#include <cstdint>

namespace Components::Remix
{
	struct CamoScale
	{
		float u;
		float v;
	};

	IDirect3DTexture9* TryBuildCamoTexture(IDirect3DDevice9* device, IDirect3DTexture9* colorMap,
		IDirect3DTexture9* camoMap, CamoScale scale, std::uint64_t& hash);

	IDirect3DTexture9* TryBuildReticleTexture(IDirect3DDevice9* device, IDirect3DTexture9* colorMap,
		IDirect3DTexture9* detailMap, std::uint64_t& hash);

	IDirect3DTexture9* TryBuildSolidTexture(IDirect3DDevice9* device, std::uint32_t colour, std::uint64_t& hash);
}
