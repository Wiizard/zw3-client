#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

namespace Components::Remix
{
	struct PbrStats
	{
		int indexedMaterials = 0;
		int candidateMaterials = 0;
		int convertedMaterials = 0;
		int failedMaterials = 0;
		int publishes = 0;
		int aliases = 0;
		int glassMaterials = 0;
		int waterMaterials = 0;
		int emissiveMaterials = 0;
		int skinMaterials = 0;
		int foliageMaterials = 0;
		bool isWorkerRunning = false;
	};

	void EnsurePbrMod();

	bool TryWriteFileAtomically(const std::wstring& target, const std::string& contents);

	void UpdatePbrMaterials();

	PbrStats GetPbrStats();

	void RegisterPbrAlias(std::uint64_t aliasHash, std::uint64_t baseHash);

	void RegisterPbrGlass(std::uint64_t hash);

	void RegisterPbrWater(std::uint64_t hash, IDirect3DTexture9* heightTexture);

	void RegisterPbrEmissive(std::uint64_t hash, IDirect3DTexture9* colourTexture, float intensity);

	inline constexpr int surfaceKindSkin = 1;
	inline constexpr int surfaceKindFoliage = 2;

	void RegisterPbrSurfaceKind(std::uint64_t hash, int kind);

	void RegisterPbrFloor(std::uint64_t hash);

	void RegisterPbrFixture(std::uint64_t hash, IDirect3DTexture9* colourTexture);

	bool IsFixtureHash(std::uint64_t hash);

	unsigned GetFixtureVersion();

	void SetFixtureGlow(bool isEnabled, float brightness);

	void SetPuddlesActive(bool isActive);

	void RegisterPbrFoliageMask(std::uint64_t hash, IDirect3DTexture9* colourTexture);

	void SetNeonFoliage(bool isEnabled, float intensity);

	bool IsNeonFoliage();

	float GetNeonIntensity();

	struct NeonPulse
	{
		bool isEnabled = false;
		float speed = 0.5f;
		float depth = 0.7f;
	};

	void SetNeonPulse(const NeonPulse& pulse);

	NeonPulse GetNeonPulse();

	bool TryGetNeonPulseFactor(std::uint64_t hash, float& factor);

	struct AlienGlow
	{
		bool isEnabled = false;
		int style = 1;
		float speed = 0.3f;
		float depth = 0.8f;
		float scale = 1.0f;
		int colourMode = 0;
		float tint[3] = { 0.2f, 1.0f, 0.6f };
	};

	void SetAlienGlow(const AlienGlow& glow);

	AlienGlow GetAlienGlow();

	bool IsNeonFoliageHash(std::uint64_t hash);

	void CollectUntaggedPbrHashes(std::unordered_set<std::uint64_t>& hashes);
}
