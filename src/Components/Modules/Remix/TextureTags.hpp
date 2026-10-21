#pragma once

#include <cstdint>

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

	TextureTagStats GetTextureTagStats();

	bool IsDecalTexture(std::uint64_t hash);

	bool IsSharedLayerTexture(std::uint64_t hash);

	void SetSharedDecalTags(bool isEnabled);

	bool IsSharedDecalTags();

	void TagGeneratedTexture(IDirect3DDevice9* device, std::uint64_t hash, TextureTag tag);

	const char* TextureTagOption(TextureTag tag);

	bool IsLampLightHash(std::uint64_t hash);

	unsigned GetLampLightVersion();
}
