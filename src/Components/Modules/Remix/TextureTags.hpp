#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

namespace Components::Remix
{
	enum class TextureTag
	{
		Sky,
		WorldSpaceUi,
		Beam,
		Decal,
		Particle,
		Water,
		Ui,
		Ignore,
	};

	inline constexpr int textureTagCount = 8;

	struct TextureTagStats
	{
		unsigned long long passes = 0;
		int materials = 0;
		int texturesWithoutHash = 0;
		int tagged[textureTagCount] = {};
		int withdrawn = 0;
		unsigned long long skippedPasses = 0;
		int restored = 0;
		int selfLit = 0;
	};

	void UpdateTextureTags(IDirect3DDevice9* device);

	void SanitizeUserConf();

	void ExcludeFromOpacityMicromaps(IDirect3DDevice9* device, std::uint64_t hash);

	void WithdrawGeneratedTag(IDirect3DDevice9* device, std::uint64_t hash, TextureTag tag);

	void ResetTextureTags();

	void UpdateUiTextureBoundary(IDirect3DDevice9* device);

	void InvalidateUiTextureBoundary();

	TextureTagStats GetTextureTagStats();

	bool IsDecalTexture(std::uint64_t hash);

	// Blended world layers with no opaque surface under them (FixedFunction checks the map's triangles). They stay
	// plain blended geometry: as decals Remix would draw them on top of nothing and show the sky through them.
	void SetFloatingLayers(const std::unordered_set<std::uint64_t>& hashes);

	bool IsFloatingLayer(std::uint64_t hash);

	// The Remix tags zw3 has on a texture this session ("-" for none), for frame dumps.
	std::string DescribeTextureTags(std::uint64_t hash);

	bool IsSharedLayerTexture(std::uint64_t hash);

	void SetSharedDecalTags(bool isEnabled);

	bool IsSharedDecalTags();

	void TagGeneratedTexture(IDirect3DDevice9* device, std::uint64_t hash, TextureTag tag);

	const char* TextureTagOption(TextureTag tag);

	bool IsLampLightHash(std::uint64_t hash);

	unsigned GetLampLightVersion();
}
