#include "STDInclude.hpp"

#include "PbrMaterials.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Dxt.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	struct RawCandidate
	{
		const void* colourTexture;
		const void* normalTexture;
		const void* specularTexture;
		float environmentMinimum;
		std::uint32_t surfaceTypeBits;
		char name[96];
	};

	struct ExportCandidate
	{
		std::uint64_t colourHash;
		std::uint64_t normalHash;
		std::uint64_t specularHash;
		IDirect3DTexture9* normalMap;
		IDirect3DTexture9* specularMap;
		float environmentMinimum;
		bool isMetal;
		std::string name;
	};

	struct TextureCopy
	{
		UINT width = 0;
		UINT height = 0;
		UINT pitch = 0;
		D3DFORMAT format = D3DFMT_UNKNOWN;
		std::vector<unsigned char> bits;
	};

	struct ExportJob
	{
		std::uint64_t colourHash = 0;
		std::string name;
		std::string normalFile;
		std::string roughnessFile;
		std::string metallicFile;
		std::string heightFile;
		std::string puddleFile;
		bool shouldWriteNormal = false;
		bool shouldWriteHeight = false;
		bool shouldWritePuddle = false;
		bool shouldWriteSpecular = false;
		float environmentMinimum = 0.0f;
		bool isMetal = false;
		TextureCopy normal;
		TextureCopy specular;
	};

	struct PublishedMaterial
	{
		std::string name;
		std::string normalFile;
		std::string roughnessFile;
		std::string metallicFile;
		std::string heightFile;
		std::string puddleFile;
	};

	static constexpr unsigned settleFrames = 120;
	static constexpr std::size_t maxQueuedJobs = 8;
	static constexpr int copiesPerFrame = 2;
	static constexpr float defaultEnvironmentMinimum = 0.07f;
	static constexpr std::uint32_t metalSurfaceBit = 0x1000;
	static constexpr DWORD dxgiFormatBc4 = 80;
	static constexpr DWORD dxgiFormatBc5 = 83;
	static constexpr const wchar_t* modFolder = L"rtx-remix\\mods\\zw3_pbr\\";

	static std::mutex exportMutex;
	static std::deque<ExportJob> jobs;
	static std::unordered_map<std::uint64_t, PublishedMaterial> published;
	static std::unordered_set<std::string> knownFiles;
	static bool isWorkerRunning = false;
	static bool isScanComplete = false;
	static bool hasUnpublishedChanges = false;
	static bool isSettingPublishPending = false;
	static bool isMapLoadPublishDue = false;
	static PbrStats stats;
	static std::unordered_map<std::uint64_t, std::uint64_t> aliases;
	static bool hasUnpublishedAliases = false;
	static std::unordered_set<std::uint64_t> glassHashes;
	static bool hasUnpublishedGlass = false;
	static std::unordered_map<std::uint64_t, std::string> waterNormals;
	static bool hasUnpublishedWater = false;

	struct EmissiveMaterial
	{
		std::string file;
		float intensity = 0.0f;
	};

	static std::unordered_map<std::uint64_t, EmissiveMaterial> emissiveMaterials;
	static bool hasUnpublishedEmissive = false;
	static std::unordered_set<std::uint64_t> neonMasks;
	static std::unordered_set<std::uint64_t> failedMasks;
	static std::unordered_set<std::string> failedHeights;
	static bool isNeonFoliage = false;
	static float neonIntensity = 3.0f;
	static NeonPulse neonPulse;
	static AlienGlow alienGlow;
	static std::unordered_map<std::uint64_t, int> surfaceKinds;
	static bool hasUnpublishedKinds = false;
	static constexpr UINT maxEmissiveSize = 512;
	static constexpr DWORD dxgiFormatB8G8R8A8 = 87;
	static constexpr float waterSlope = 0.15f;
	static constexpr UINT proceduralWaterSize = 128;
	static constexpr int proceduralWaveCount = 40;
	static constexpr const char* legacyWaterPrefix = "water_";
	static ULONGLONG lastPublishTick = 0;
	static constexpr ULONGLONG aliasPublishIntervalMs = 60000;
	static bool didLastPublishFail = false;
	static ULONGLONG lastSettingChangeTick = 0;
	static constexpr ULONGLONG settingSettleMs = 1000;
	static constexpr const char* unsetField = "-";
	static constexpr const char* modFormatMarker = "int zw3_format = 4";
	static constexpr UINT maxHeightSize = 512;
	static constexpr UINT coarsestHeightSize = 8;
	static constexpr int coarseHeightIterations = 200;
	static constexpr int fineHeightIterations = 30;
	static constexpr float displaceInDepth = 0.05f;
	static constexpr float dryRoughnessConstant = 0.49f;
	static constexpr float puddleRoughness = 0.0025f;
	static constexpr float puddleLevel = 0.35f;
	static constexpr float puddleEdge = 0.08f;
	static constexpr float wetRoughnessFactor = 0.5f;
	static std::unordered_set<std::uint64_t> floorHashes;
	static std::unordered_set<std::string> failedPuddles;
	static bool isPuddlesActive = false;
	static constexpr int maskFull = 0;
	static constexpr int maskLeaf = 1;
	static constexpr int maskBright = 2;
	static constexpr float brightMaskLow = 0.55f;
	static constexpr float brightMaskHigh = 0.85f;
	static constexpr float fixtureGlowIntensity = 4.0f;
	static std::unordered_map<std::uint64_t, std::string> fixtureMasks;
	static unsigned fixtureVersion = 0;
	static bool isFixtureGlow = false;
	static float fixtureGlowBrightness = 1.0f;

	static std::vector<ExportCandidate> candidates;
	static std::size_t nextCandidate = 0;
	static std::uintptr_t settledWorld = 0;
	static std::uintptr_t scannedWorld = 0;
	static unsigned settledFrameCount = 0;
	static bool hasLoadedIndex = false;

	static std::wstring ModDirectory()
	{
		return Core::ModuleDirectory() + modFolder;
	}

	static std::wstring Widen(const std::string& text)
	{
		return std::wstring(text.begin(), text.end());
	}

	static std::string NarrowAscii(const std::wstring& text)
	{
		std::string result;
		result.reserve(text.size());

		for (const wchar_t character : text)
		{
			result.push_back(static_cast<char>(character));
		}

		return result;
	}

	static bool TryReplaceFile(const std::wstring& temporary, const std::wstring& target)
	{
		return MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
	}

	bool TryWriteFileAtomically(const std::wstring& target, const std::string& contents)
	{
		const std::wstring temporary = target + L".tmp";

		{
			std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

			if (!file)
			{
				return false;
			}

			file.write(contents.data(), static_cast<std::streamsize>(contents.size()));

			if (!file)
			{
				return false;
			}
		}

		if (!TryReplaceFile(temporary, target))
		{
			DeleteFileW(temporary.c_str());
			return false;
		}

		return true;
	}

	static std::string NeonFileName(std::uint64_t hash)
	{
		char fileName[48];
		std::snprintf(fileName, sizeof(fileName), "neon3_%016llX.dds", static_cast<unsigned long long>(hash));
		return fileName;
	}

	static std::string NeonGreyFileName(std::uint64_t hash)
	{
		char fileName[48];
		std::snprintf(fileName, sizeof(fileName), "neongrey3_%016llX.dds", static_cast<unsigned long long>(hash));
		return fileName;
	}

	static bool IsNeonGreyscale()
	{
		return alienGlow.isEnabled && alienGlow.colourMode != 0;
	}

	static std::string MaterialPrim(std::uint64_t colourHash, const PublishedMaterial& material,
		const EmissiveMaterial* emissive, int surfaceKind, float foliageGlow, bool isNeonGrey)
	{
		char name[24];
		std::snprintf(name, sizeof(name), "mat_%016llX", static_cast<unsigned long long>(colourHash));

		std::ostringstream text;
		text << "        def Material \"" << name << "\"\n"
			 << "        {\n"
			 << "            token outputs:mdl:surface.connect = </RootNode/Looks/" << name << "/Shader.outputs:out>\n"
			 << "\n"
			 << "            def Shader \"Shader\"\n"
			 << "            {\n"
			 << "                uniform token info:implementationSource = \"sourceAsset\"\n"
			 << "                uniform asset info:mdl:sourceAsset = @AperturePBR_Opaque.mdl@\n"
			 << "                uniform token info:mdl:sourceAsset:subIdentifier = \"AperturePBR_Opaque\"\n"
			 << "                bool inputs:use_legacy_alpha_state = 1\n";

		if (!material.normalFile.empty())
		{
			text << "                asset inputs:normalmap_texture = @./textures/" << material.normalFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n";
		}

		if (!material.puddleFile.empty())
		{
			text << "                asset inputs:reflectionroughness_texture = @./textures/" << material.puddleFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n";
		}
		else if (!material.roughnessFile.empty())
		{
			text << "                asset inputs:reflectionroughness_texture = @./textures/" << material.roughnessFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n";
		}
		else
		{
			text << "                float inputs:reflection_roughness_constant = 0.7\n";
		}

		if (!material.heightFile.empty())
		{
			text << "                asset inputs:height_texture = @./textures/" << material.heightFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n"
				 << "                float inputs:displace_in = " << displaceInDepth << "\n";
		}

		if (!material.metallicFile.empty())
		{
			text << "                asset inputs:metallic_texture = @./textures/" << material.metallicFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n";
		}
		else
		{
			text << "                float inputs:metallic_constant = 0\n";
		}

		if (emissive && !emissive->file.empty())
		{
			text << "                bool inputs:enable_emission = 1\n"
				 << "                asset inputs:emissive_mask_texture = @./textures/" << emissive->file << "@\n"
				 << "                float inputs:emissive_intensity = " << emissive->intensity << "\n";
		}
		else if (surfaceKind == surfaceKindFoliage && foliageGlow > 0.0f)
		{
			std::string maskFile = NeonFileName(colourHash);

			if (isNeonGrey)
			{
				maskFile = NeonGreyFileName(colourHash);
			}

			text << "                bool inputs:enable_emission = 1\n"
				 << "                asset inputs:emissive_mask_texture = @./textures/" << maskFile << "@\n"
				 << "                float inputs:emissive_intensity = " << foliageGlow << "\n";
		}

		if (surfaceKind == surfaceKindSkin)
		{
			text << "                bool inputs:subsurface_diffusion_profile = 1\n"
				 << "                color3f inputs:subsurface_transmittance_color = (0.75, 0.45, 0.35)\n"
				 << "                color3f inputs:subsurface_radius = (1, 0.37, 0.18)\n"
				 << "                float inputs:subsurface_radius_scale = 0.15\n"
				 << "                float inputs:subsurface_max_sample_radius = 1\n"
				 << "                float inputs:subsurface_volumetric_anisotropy = 0\n";
		}

		if (surfaceKind == surfaceKindFoliage)
		{
			text << "                float inputs:subsurface_measurement_distance = 1\n"
				 << "                color3f inputs:subsurface_transmittance_color = (0.5, 0.45, 0.25)\n"
				 << "                color3f inputs:subsurface_single_scattering_albedo = (0.45, 0.75, 0.15)\n"
				 << "                float inputs:subsurface_volumetric_anisotropy = 0.5\n";
		}

		text << "                token outputs:out\n"
			 << "            }\n"
			 << "        }\n";

		return text.str();
	}

	static std::string GlassPrim(std::uint64_t hash)
	{
		char name[24];
		std::snprintf(name, sizeof(name), "mat_%016llX", static_cast<unsigned long long>(hash));

		std::ostringstream text;
		text << "        def Material \"" << name << "\"\n"
			 << "        {\n"
			 << "            token outputs:mdl:surface.connect = </RootNode/Looks/" << name << "/Shader.outputs:out>\n"
			 << "\n"
			 << "            def Shader \"Shader\"\n"
			 << "            {\n"
			 << "                uniform token info:implementationSource = \"sourceAsset\"\n"
			 << "                uniform asset info:mdl:sourceAsset = @AperturePBR_Translucent.mdl@\n"
			 << "                uniform token info:mdl:sourceAsset:subIdentifier = \"AperturePBR_Translucent\"\n"
			 << "                float inputs:ior_constant = 1.9\n"
			 << "                bool inputs:thin_walled = 1\n"
			 << "                float inputs:thin_wall_thickness = 1\n"
			 << "                color3f inputs:transmittance_color = (0.9, 0.95, 0.93)\n"
			 << "                bool inputs:use_diffuse_layer = 0\n"
			 << "                token outputs:out\n"
			 << "            }\n"
			 << "        }\n";

		return text.str();
	}

	static std::string WaterPrim(std::uint64_t hash, const std::string& normalFile)
	{
		char name[24];
		std::snprintf(name, sizeof(name), "mat_%016llX", static_cast<unsigned long long>(hash));

		std::ostringstream text;
		text << "        def Material \"" << name << "\"\n"
			 << "        {\n"
			 << "            token outputs:mdl:surface.connect = </RootNode/Looks/" << name << "/Shader.outputs:out>\n"
			 << "\n"
			 << "            def Shader \"Shader\"\n"
			 << "            {\n"
			 << "                uniform token info:implementationSource = \"sourceAsset\"\n"
			 << "                uniform asset info:mdl:sourceAsset = @AperturePBR_Translucent.mdl@\n"
			 << "                uniform token info:mdl:sourceAsset:subIdentifier = \"AperturePBR_Translucent\"\n"
			 << "                float inputs:ior_constant = 1.33\n"
			 << "                bool inputs:thin_walled = 0\n"
			 << "                color3f inputs:transmittance_color = (0.35, 0.55, 0.6)\n"
			 << "                float inputs:transmittance_measurement_distance = 40\n"
			 << "                bool inputs:use_diffuse_layer = 0\n";

		if (!normalFile.empty())
		{
			text << "                asset inputs:normalmap_texture = @./textures/" << normalFile << "@ (\n"
				 << "                    colorSpace = \"raw\"\n"
				 << "                )\n";
		}

		text << "                token outputs:out\n"
			 << "            }\n"
			 << "        }\n";

		return text.str();
	}

	static std::string ModText(const std::vector<std::pair<std::uint64_t, PublishedMaterial>>& materials,
		const std::vector<std::uint64_t>& glass, const std::vector<std::pair<std::uint64_t, std::string>>& water,
		const std::unordered_map<std::uint64_t, EmissiveMaterial>& emissive, const std::unordered_map<std::uint64_t, int>& kinds,
		const std::unordered_set<std::uint64_t>& neon, float neonGlow, bool isNeonGrey)
	{
		std::ostringstream text;
		text << "#usda 1.0\n"
			 << "(\n"
			 << "    customLayerData = {\n"
			 << "        string remix_replacement_status = \"zw3 pbr export\"\n"
			 << "        " << modFormatMarker << "\n"
			 << "    }\n"
			 << "    metersPerUnit = 0.0254\n"
			 << "    upAxis = \"Z\"\n"
			 << ")\n"
			 << "\n"
			 << "def Xform \"RootNode\"\n"
			 << "{\n"
			 << "    def Scope \"Looks\"\n"
			 << "    {\n";

		bool isFirst = true;

		for (const auto& entry : materials)
		{
			if (std::find(glass.begin(), glass.end(), entry.first) != glass.end())
			{
				continue;
			}

			const bool isWater = std::any_of(water.begin(), water.end(), [&entry](const auto& waterEntry)
			{
				return waterEntry.first == entry.first;
			});

			if (isWater)
			{
				continue;
			}

			if (!isFirst)
			{
				text << "\n";
			}

			const auto glow = emissive.find(entry.first);
			const EmissiveMaterial* entryEmissive = nullptr;

			if (glow != emissive.end())
			{
				entryEmissive = &glow->second;
			}

			int entryKind = 0;
			const auto kind = kinds.find(entry.first);

			if (kind != kinds.end())
			{
				entryKind = kind->second;
			}

			float entryGlow = 0.0f;

			if (neon.count(entry.first) != 0)
			{
				entryGlow = neonGlow;
			}

			text << MaterialPrim(entry.first, entry.second, entryEmissive, entryKind, entryGlow, isNeonGrey);
			isFirst = false;
		}

		std::vector<std::uint64_t> extras;

		for (const auto& glow : emissive)
		{
			extras.push_back(glow.first);
		}

		for (const auto& kind : kinds)
		{
			if (emissive.count(kind.first) == 0)
			{
				extras.push_back(kind.first);
			}
		}

		std::sort(extras.begin(), extras.end());

		for (const std::uint64_t hash : extras)
		{
			const bool isPublished = std::any_of(materials.begin(), materials.end(), [hash](const auto& entry)
			{
				return entry.first == hash;
			});
			const bool isSpecial = std::find(glass.begin(), glass.end(), hash) != glass.end()
				|| std::any_of(water.begin(), water.end(), [hash](const auto& waterEntry)
				{
					return waterEntry.first == hash;
				});

			if (isPublished || isSpecial)
			{
				continue;
			}

			const auto glow = emissive.find(hash);
			const EmissiveMaterial* extraEmissive = nullptr;

			if (glow != emissive.end())
			{
				extraEmissive = &glow->second;
			}

			int extraKind = 0;
			const auto kind = kinds.find(hash);

			if (kind != kinds.end())
			{
				extraKind = kind->second;
			}

			if (!isFirst)
			{
				text << "\n";
			}

			float extraGlow = 0.0f;

			if (neon.count(hash) != 0)
			{
				extraGlow = neonGlow;
			}

			text << MaterialPrim(hash, PublishedMaterial{}, extraEmissive, extraKind, extraGlow, isNeonGrey);
			isFirst = false;
		}

		for (const std::uint64_t hash : glass)
		{
			const bool isWater = std::any_of(water.begin(), water.end(), [hash](const auto& waterEntry)
			{
				return waterEntry.first == hash;
			});

			if (isWater)
			{
				continue;
			}

			if (!isFirst)
			{
				text << "\n";
			}

			text << GlassPrim(hash);
			isFirst = false;
		}

		for (const auto& waterEntry : water)
		{
			if (!isFirst)
			{
				text << "\n";
			}

			text << WaterPrim(waterEntry.first, waterEntry.second);
			isFirst = false;
		}

		text << "    }\n"
			 << "}\n";

		return text.str();
	}

	void EnsurePbrMod()
	{
		const std::wstring root = Core::ModuleDirectory();
		CreateDirectoryW((root + L"rtx-remix").c_str(), nullptr);
		CreateDirectoryW((root + L"rtx-remix\\mods").c_str(), nullptr);
		CreateDirectoryW(ModDirectory().c_str(), nullptr);
		CreateDirectoryW((ModDirectory() + L"textures").c_str(), nullptr);

		const std::wstring modFile = ModDirectory() + L"mod.usda";

		if (GetFileAttributesW(modFile.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			return;
		}

		if (TryWriteFileAtomically(modFile, ModText({}, {}, {}, {}, {}, {}, 0.0f, false)))
		{
			Log() << "remix: created an empty PBR material mod so Remix loads it at start-up";
		}
	}

	static void LoadIndex()
	{
		hasLoadedIndex = true;

		std::ifstream modFile(ModDirectory() + L"mod.usda");
		const std::string modText((std::istreambuf_iterator<char>(modFile)), std::istreambuf_iterator<char>());

		if (modText.find(modFormatMarker) == std::string::npos)
		{
			hasUnpublishedGlass = true;
		}

		WIN32_FIND_DATAW found{};
		const HANDLE search = FindFirstFileW((ModDirectory() + L"textures\\*.dds").c_str(), &found);

		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				knownFiles.insert(NarrowAscii(found.cFileName));
			}
			while (FindNextFileW(search, &found));

			FindClose(search);
		}

		std::ifstream file(ModDirectory() + L"zw3_pbr_index.txt");
		std::string line;

		while (std::getline(file, line))
		{
			std::istringstream fields(line);
			std::string hash;
			PublishedMaterial material;

			if (!(fields >> hash >> material.normalFile >> material.roughnessFile >> material.metallicFile))
			{
				continue;
			}

			std::getline(fields, material.name);

			for (std::string* field : { &material.normalFile, &material.roughnessFile, &material.metallicFile })
			{
				if (*field == unsetField)
				{
					field->clear();
				}
			}

			published[std::strtoull(hash.c_str(), nullptr, 16)] = material;
		}

		std::ifstream aliasFile(ModDirectory() + L"zw3_pbr_aliases.txt");
		std::string aliasHash;
		std::string baseHash;

		while (aliasFile >> aliasHash >> baseHash)
		{
			aliases[std::strtoull(aliasHash.c_str(), nullptr, 16)] = std::strtoull(baseHash.c_str(), nullptr, 16);
		}

		stats.aliases = static_cast<int>(aliases.size());

		std::ifstream glassFile(ModDirectory() + L"zw3_pbr_glass.txt");
		std::string glassHash;

		while (glassFile >> glassHash)
		{
			glassHashes.insert(std::strtoull(glassHash.c_str(), nullptr, 16));
		}

		stats.glassMaterials = static_cast<int>(glassHashes.size());

		std::ifstream waterFile(ModDirectory() + L"zw3_pbr_water.txt");
		std::string waterHash;
		std::string waterNormal;

		while (waterFile >> waterHash >> waterNormal)
		{
			if (waterNormal == unsetField)
			{
				waterNormal.clear();
			}

			if (waterNormal.rfind(legacyWaterPrefix, 0) == 0)
			{
				hasUnpublishedWater = true;
				continue;
			}

			waterNormals[std::strtoull(waterHash.c_str(), nullptr, 16)] = waterNormal;
		}

		for (const auto& water : waterNormals)
		{
			if (glassHashes.erase(water.first) != 0)
			{
				hasUnpublishedGlass = true;
			}
		}

		stats.glassMaterials = static_cast<int>(glassHashes.size());
		stats.waterMaterials = static_cast<int>(waterNormals.size());

		std::ifstream emissiveFile(ModDirectory() + L"zw3_pbr_emissive.txt");
		std::string emissiveHash;
		EmissiveMaterial emissiveEntry;

		while (emissiveFile >> emissiveHash >> emissiveEntry.file >> emissiveEntry.intensity)
		{
			emissiveMaterials[std::strtoull(emissiveHash.c_str(), nullptr, 16)] = emissiveEntry;
		}

		stats.emissiveMaterials = static_cast<int>(emissiveMaterials.size());

		std::ifstream kindFile(ModDirectory() + L"zw3_pbr_kinds.txt");
		std::string kindHash;
		int kindValue = 0;

		while (kindFile >> kindHash >> kindValue)
		{
			surfaceKinds[std::strtoull(kindHash.c_str(), nullptr, 16)] = kindValue;
		}

		for (const auto& kind : surfaceKinds)
		{
			if (kind.second == surfaceKindSkin)
			{
				++stats.skinMaterials;
			}

			if (kind.second == surfaceKindFoliage)
			{
				++stats.foliageMaterials;
			}
		}

		stats.indexedMaterials = static_cast<int>(published.size());

		if (modText.find("/textures/neon") != std::string::npos)
		{
			std::lock_guard<std::mutex> lock(exportMutex);
			hasUnpublishedKinds = true;
		}
	}

	static void CopyName(char* destination, std::size_t capacity, const char* source)
	{
		std::size_t length = 0;

		while (source && length + 1 < capacity && source[length] != '\0')
		{
			destination[length] = source[length];
			++length;
		}

		destination[length] = '\0';
	}

	static const void* FindTextureObject(std::uintptr_t material, std::uint32_t nameHash, std::uint8_t semantic)
	{
		const auto textureCount = ReadAt<std::uint8_t>(material + Iw4::materialTextureCount);
		const auto textures = ReadAt<std::uintptr_t>(material + Iw4::materialTextureTable);

		for (std::uint8_t i = 0; textures && i < textureCount; ++i)
		{
			const std::uintptr_t definition = textures + i * Iw4::textureDefStride;

			if (ReadAt<std::uint32_t>(definition + Iw4::textureDefNameHash) != nameHash
				|| ReadAt<std::uint8_t>(definition + Iw4::textureDefSemantic) != semantic)
			{
				continue;
			}

			const auto image = ReadAt<std::uintptr_t>(definition + Iw4::textureDefImage);

			if (!image || ReadAt<std::uint8_t>(image + Iw4::gfxImageMapType) != Iw4::gfxImageMapType2D)
			{
				return nullptr;
			}

			const char* imageName = ReadAt<const char*>(image + Iw4::gfxImageName);

			if (imageName && imageName[0] == '$')
			{
				return nullptr;
			}

			return ReadAt<const void*>(image + Iw4::gfxImageTexture);
		}

		return nullptr;
	}

	static float ReadEnvironmentMinimum(std::uintptr_t material)
	{
		const auto constantCount = ReadAt<std::uint8_t>(material + Iw4::materialConstantCount);
		const auto constants = ReadAt<std::uintptr_t>(material + Iw4::materialConstantTable);

		for (std::uint8_t i = 0; constants && i < constantCount; ++i)
		{
			const std::uintptr_t constant = constants + i * Iw4::constantDefStride;

			if (ReadAt<std::uint32_t>(constant + Iw4::constantDefNameHash) == Iw4::envMapParmsNameHash)
			{
				return ReadAt<float>(constant + Iw4::constantDefLiteral);
			}
		}

		return defaultEnvironmentMinimum;
	}

	static bool TryReadCandidate(std::uintptr_t material, std::uintptr_t poolStart, std::uintptr_t poolEnd,
		RawCandidate& candidate)
	{
		__try
		{
			const auto first = ReadAt<std::uintptr_t>(material);

			if (first == 0 || (first >= poolStart && first < poolEnd))
			{
				return false;
			}

			CopyName(candidate.name, sizeof(candidate.name), reinterpret_cast<const char*>(first));
			candidate.colourTexture = FindTextureObject(material, Iw4::colorMapNameHash,
				Iw4::textureSemanticColour);
			candidate.normalTexture = FindTextureObject(material, Iw4::normalMapNameHash,
				Iw4::textureSemanticNormal);
			candidate.specularTexture = FindTextureObject(material, Iw4::specularMapNameHash,
				Iw4::textureSemanticSpecular);
			candidate.environmentMinimum = ReadEnvironmentMinimum(material);
			candidate.surfaceTypeBits = ReadAt<std::uint32_t>(material + Iw4::materialSurfaceTypeBits);

			return candidate.colourTexture && (candidate.normalTexture || candidate.specularTexture);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static std::string HashFileName(std::uint64_t hash, const char* suffix);
	static std::string HeightFileName(const std::string& normalFile);

	static bool IsMissingDerivedFiles(std::uint64_t colourHash, const PublishedMaterial& material)
	{
		if (material.normalFile.empty())
		{
			return false;
		}

		const std::string heightFile = HeightFileName(material.normalFile);
		const bool isHeightMissing = knownFiles.count(heightFile) == 0 && failedHeights.count(heightFile) == 0;
		const std::string puddleFile = HashFileName(colourHash, ".w.dds");
		const bool isPuddleMissing = floorHashes.count(colourHash) != 0 && knownFiles.count(puddleFile) == 0
			&& failedPuddles.count(puddleFile) == 0;

		return isHeightMissing || isPuddleMissing;
	}

	static void CollectCandidates()
	{
		candidates.clear();
		nextCandidate = 0;

		const Engine::MaterialPool materialPool = Engine::CurrentMaterialPool();
		const std::uintptr_t pool = materialPool.pool;
		const std::uintptr_t entries = materialPool.entries;
		const std::uintptr_t poolEnd = entries + materialPool.count * materialPool.stride;

		std::unordered_set<std::uint64_t> seen;
		std::lock_guard<std::mutex> lock(exportMutex);

		for (std::size_t i = 0; i < materialPool.count; ++i)
		{
			RawCandidate raw{};

			if (!TryReadCandidate(entries + i * materialPool.stride, pool, poolEnd, raw))
			{
				continue;
			}

			ExportCandidate candidate;

			candidate.normalHash = 0;
			candidate.specularHash = 0;
			candidate.normalMap = nullptr;
			candidate.specularMap = nullptr;

			if (!Device::TryGetTextureHash(raw.colourTexture, candidate.colourHash))
			{
				continue;
			}

			if (raw.normalTexture && !Device::TryGetTextureHash(raw.normalTexture, candidate.normalHash))
			{
				continue;
			}

			if (raw.specularTexture && !Device::TryGetTextureHash(raw.specularTexture, candidate.specularHash))
			{
				continue;
			}

			const auto existing = published.find(candidate.colourHash);
			const bool isComplete = existing != published.end()
				&& !IsMissingDerivedFiles(candidate.colourHash, existing->second);

			if (isComplete || !seen.insert(candidate.colourHash).second)
			{
				continue;
			}

			if (raw.normalTexture)
			{
				candidate.normalMap = Device::Unwrap(static_cast<IDirect3DTexture9*>(const_cast<void*>(raw.normalTexture)));
			}

			if (raw.specularTexture)
			{
				candidate.specularMap = Device::Unwrap(
					static_cast<IDirect3DTexture9*>(const_cast<void*>(raw.specularTexture)));
			}
			candidate.environmentMinimum = raw.environmentMinimum;
			candidate.isMetal = (raw.surfaceTypeBits & metalSurfaceBit) != 0;
			candidate.name = raw.name;
			candidates.push_back(candidate);
		}

		isScanComplete = candidates.empty();
		stats.candidateMaterials = static_cast<int>(candidates.size());

		Log() << "remix: " << candidates.size() << " materials of this map have normal and specular maps not exported"
				 " yet, converting them for the PBR mod";
	}

	static bool TryCopyTopLevel(IDirect3DTexture9* texture, TextureCopy& copy)
	{
		D3DSURFACE_DESC description{};

		if (!texture || FAILED(texture->GetLevelDesc(0, &description)) || !IsDecodableFormat(description.Format))
		{
			return false;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		UINT rows = description.Height;

		if (description.Format == D3DFMT_DXT1 || description.Format == D3DFMT_DXT3 || description.Format == D3DFMT_DXT5)
		{
			rows = std::max(1u, (description.Height + 3) / 4);
		}

		copy.width = description.Width;
		copy.height = description.Height;
		copy.pitch = static_cast<UINT>(locked.Pitch);
		copy.format = description.Format;
		copy.bits.assign(static_cast<const unsigned char*>(locked.pBits),
			static_cast<const unsigned char*>(locked.pBits) + static_cast<std::size_t>(copy.pitch) * rows);

		texture->UnlockRect(0);

		return true;
	}

	static std::string HashFileName(std::uint64_t hash, const char* suffix)
	{
		char text[48];
		std::snprintf(text, sizeof(text), "%016llX%s", static_cast<unsigned long long>(hash), suffix);
		return text;
	}

	static std::string SpecularFileName(std::uint64_t hash, float environmentMinimum, bool isMetal, const char* suffix)
	{
		std::uint32_t parameterBits = 0;
		std::memcpy(&parameterBits, &environmentMinimum, sizeof(parameterBits));

		char kind = 'd';

		if (isMetal)
		{
			kind = 'm';
		}

		char text[64];
		std::snprintf(text, sizeof(text), "%016llX_%08X%c%s", static_cast<unsigned long long>(hash), parameterBits, kind,
			suffix);
		return text;
	}

	static void HalveLevel(const std::vector<float>& source, UINT width, UINT height, int channels,
		std::vector<float>& destination)
	{
		const UINT halfWidth = std::max(1u, width / 2);
		const UINT halfHeight = std::max(1u, height / 2);
		destination.assign(static_cast<std::size_t>(halfWidth) * halfHeight * channels, 0.0f);

		for (UINT y = 0; y < halfHeight; ++y)
		{
			const UINT top = std::min(y * 2, height - 1);
			const UINT bottom = std::min(y * 2 + 1, height - 1);

			for (UINT x = 0; x < halfWidth; ++x)
			{
				const UINT left = std::min(x * 2, width - 1);
				const UINT right = std::min(x * 2 + 1, width - 1);

				for (int channel = 0; channel < channels; ++channel)
				{
					const float sum = source[(top * width + left) * channels + channel]
						+ source[(top * width + right) * channels + channel]
						+ source[(bottom * width + left) * channels + channel]
						+ source[(bottom * width + right) * channels + channel];

					destination[(y * halfWidth + x) * channels + channel] = sum * 0.25f;
				}
			}
		}
	}

	static void EncodeBc4Block(const int* values, unsigned char* block)
	{
		int highest = values[0];
		int lowest = values[0];

		for (int i = 1; i < 16; ++i)
		{
			highest = std::max(highest, values[i]);
			lowest = std::min(lowest, values[i]);
		}

		float palette[8];
		palette[0] = static_cast<float>(highest);
		palette[1] = static_cast<float>(lowest);

		for (int i = 1; i < 7; ++i)
		{
			palette[1 + i] = ((7 - i) * highest + i * lowest) / 7.0f;
		}

		std::uint64_t indices = 0;

		for (int i = 0; i < 16 && highest != lowest; ++i)
		{
			int best = 0;
			float bestDistance = std::fabs(values[i] - palette[0]);

			for (int entry = 1; entry < 8; ++entry)
			{
				const float distance = std::fabs(values[i] - palette[entry]);

				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = entry;
				}
			}

			indices |= static_cast<std::uint64_t>(best) << (3 * i);
		}

		block[0] = static_cast<unsigned char>(highest);
		block[1] = static_cast<unsigned char>(lowest);

		for (int byte = 0; byte < 6; ++byte)
		{
			block[2 + byte] = static_cast<unsigned char>((indices >> (8 * byte)) & 0xFF);
		}
	}

	static int ToByte(float value)
	{
		return static_cast<int>(std::clamp(std::round(value * 255.0f), 0.0f, 255.0f));
	}

	static void EncodeBcLevel(const std::vector<float>& level, UINT width, UINT height, int channels,
		std::string& output)
	{
		const UINT blocksWide = std::max(1u, (width + 3) / 4);
		const UINT blocksHigh = std::max(1u, (height + 3) / 4);

		int values[16];
		unsigned char block[8];

		for (UINT blockY = 0; blockY < blocksHigh; ++blockY)
		{
			for (UINT blockX = 0; blockX < blocksWide; ++blockX)
			{
				for (int channel = 0; channel < channels; ++channel)
				{
					for (UINT y = 0; y < 4; ++y)
					{
						const UINT sourceY = std::min(blockY * 4 + y, height - 1);

						for (UINT x = 0; x < 4; ++x)
						{
							const UINT sourceX = std::min(blockX * 4 + x, width - 1);
							values[y * 4 + x] = ToByte(level[(sourceY * width + sourceX) * channels + channel]);
						}
					}

					EncodeBc4Block(values, block);
					output.append(reinterpret_cast<const char*>(block), sizeof(block));
				}
			}
		}
	}

	static void AppendUint32(std::string& output, std::uint32_t value)
	{
		output.append(reinterpret_cast<const char*>(&value), sizeof(value));
	}

	static std::string DdsHeader(UINT width, UINT height, UINT mipCount, DWORD dxgiFormat, UINT blockBytes)
	{
		std::string header = "DDS ";
		const UINT linearSize = std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * blockBytes;

		AppendUint32(header, 124);
		AppendUint32(header, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000);
		AppendUint32(header, height);
		AppendUint32(header, width);
		AppendUint32(header, linearSize);
		AppendUint32(header, 0);
		AppendUint32(header, mipCount);
		header.append(44, '\0');

		AppendUint32(header, 32);
		AppendUint32(header, 0x4);
		header.append("DX10", 4);
		header.append(20, '\0');

		AppendUint32(header, 0x1000 | 0x400000 | 0x8);
		header.append(16, '\0');

		AppendUint32(header, dxgiFormat);
		AppendUint32(header, 3);
		AppendUint32(header, 0);
		AppendUint32(header, 1);
		AppendUint32(header, 0);

		return header;
	}

	static UINT MipCount(UINT width, UINT height)
	{
		UINT count = 1;

		while (width > 1 || height > 1)
		{
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
			++count;
		}

		return count;
	}

	static void EncodeNormalLevel(const std::vector<float>& normals, UINT width, UINT height, std::string& output)
	{
		std::vector<float> encoded(static_cast<std::size_t>(width) * height * 2);

		for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
		{
			float x = normals[i * 3];
			float y = normals[i * 3 + 1];
			float z = normals[i * 3 + 2];
			const float length = std::sqrt(x * x + y * y + z * z);

			if (length > 0.0f)
			{
				x /= length;
				y /= length;
				z /= length;
			}

			const float sum = std::fabs(x) + std::fabs(y) + std::max(z, 0.0f);
			const float px = x / sum;
			const float py = y / sum;

			encoded[i * 2] = (px + py) * 0.5f + 0.5f;
			encoded[i * 2 + 1] = (px - py) * 0.5f + 0.5f;
		}

		EncodeBcLevel(encoded, width, height, 2, output);
	}

	static bool TryWriteNormalFile(const TextureCopy& copy, const std::string& fileName)
	{
		DecodedImage image;

		if (!TryDecodeImage(copy.bits.data(), copy.pitch, copy.format, copy.width, copy.height, image))
		{
			return false;
		}

		std::vector<float> level(image.texels.size() * 3);

		for (std::size_t i = 0; i < image.texels.size(); ++i)
		{
			const float x = Channel(image.texels[i], 24) / 255.0f * 4.08f - 2.08f;
			const float y = Channel(image.texels[i], 8) / 255.0f * 4.0645161f - 2.0645161f;
			const float length = std::sqrt(x * x + y * y + 1.0f);

			level[i * 3] = x / length;
			level[i * 3 + 1] = y / length;
			level[i * 3 + 2] = 1.0f / length;
		}

		UINT width = image.width;
		UINT height = image.height;
		const UINT mipCount = MipCount(width, height);
		std::string contents = DdsHeader(width, height, mipCount, dxgiFormatBc5, 16);
		std::vector<float> next;

		for (UINT mip = 0; mip < mipCount; ++mip)
		{
			EncodeNormalLevel(level, width, height, contents);
			HalveLevel(level, width, height, 3, next);
			level.swap(next);
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}

		return TryWriteFileAtomically(ModDirectory() + L"textures\\" + Widen(fileName), contents);
	}

	static bool TryWriteSingleChannelFile(std::vector<float> level, UINT width, UINT height, bool isSquared,
		const std::string& fileName)
	{
		const UINT mipCount = MipCount(width, height);
		std::string contents = DdsHeader(width, height, mipCount, dxgiFormatBc4, 8);
		std::vector<float> next;
		std::vector<float> stored;

		for (UINT mip = 0; mip < mipCount; ++mip)
		{
			stored = level;

			if (isSquared)
			{
				for (float& value : stored)
				{
					value = std::sqrt(value);
				}
			}

			EncodeBcLevel(stored, width, height, 1, contents);
			HalveLevel(level, width, height, 1, next);
			level.swap(next);
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}

		return TryWriteFileAtomically(ModDirectory() + L"textures\\" + Widen(fileName), contents);
	}

	static std::string HeightFileName(const std::string& normalFile)
	{
		return normalFile.substr(0, normalFile.size() - std::strlen(".n.dds")) + ".h.dds";
	}

	static void RelaxHeights(const std::vector<float>& slopes, UINT width, UINT height, int iterations,
		std::vector<float>& heights)
	{
		const std::size_t count = static_cast<std::size_t>(width) * height;
		std::vector<float> divergence(count);
		double sum = 0.0;

		for (UINT y = 0; y < height; ++y)
		{
			const UINT up = (y + height - 1) % height;

			for (UINT x = 0; x < width; ++x)
			{
				const UINT left = (x + width - 1) % width;
				const std::size_t here = static_cast<std::size_t>(y) * width + x;
				const std::size_t beside = static_cast<std::size_t>(y) * width + left;
				const std::size_t above = static_cast<std::size_t>(up) * width + x;

				divergence[here] = slopes[here * 2] - slopes[beside * 2] + slopes[here * 2 + 1] - slopes[above * 2 + 1];
				sum += divergence[here];
			}
		}

		const float mean = static_cast<float>(sum / static_cast<double>(count));

		for (float& value : divergence)
		{
			value -= mean;
		}

		for (int iteration = 0; iteration < iterations; ++iteration)
		{
			for (UINT y = 0; y < height; ++y)
			{
				const std::size_t row = static_cast<std::size_t>(y) * width;
				const std::size_t rowAbove = static_cast<std::size_t>((y + height - 1) % height) * width;
				const std::size_t rowBelow = static_cast<std::size_t>((y + 1) % height) * width;

				for (UINT x = 0; x < width; ++x)
				{
					const UINT left = (x + width - 1) % width;
					const UINT right = (x + 1) % width;
					const float neighbours = heights[row + left] + heights[row + right] + heights[rowAbove + x]
						+ heights[rowBelow + x];

					heights[row + x] = (neighbours - divergence[row + x]) * 0.25f;
				}
			}
		}
	}

	static bool TryComputeHeights(const TextureCopy& copy, std::vector<float>& heights, UINT& width, UINT& height)
	{
		DecodedImage image;

		if (!TryDecodeImage(copy.bits.data(), copy.pitch, copy.format, copy.width, copy.height, image))
		{
			return false;
		}

		width = image.width;
		height = image.height;
		std::vector<float> slopes(image.texels.size() * 2);

		for (std::size_t i = 0; i < image.texels.size(); ++i)
		{
			slopes[i * 2] = -(Channel(image.texels[i], 24) / 255.0f * 4.08f - 2.08f);
			slopes[i * 2 + 1] = -(Channel(image.texels[i], 8) / 255.0f * 4.0645161f - 2.0645161f);
		}

		std::vector<float> next;

		while (width > maxHeightSize || height > maxHeightSize)
		{
			HalveLevel(slopes, width, height, 2, next);
			slopes.swap(next);
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);

			for (float& slope : slopes)
			{
				slope *= 2.0f;
			}
		}

		struct SlopeLevel
		{
			std::vector<float> slopes;
			UINT width;
			UINT height;
		};

		std::vector<SlopeLevel> levels;
		levels.push_back({ slopes, width, height });

		while (levels.back().width > coarsestHeightSize && levels.back().height > coarsestHeightSize)
		{
			const SlopeLevel& finer = levels.back();
			SlopeLevel coarser;
			HalveLevel(finer.slopes, finer.width, finer.height, 2, coarser.slopes);
			coarser.width = std::max(1u, finer.width / 2);
			coarser.height = std::max(1u, finer.height / 2);

			for (float& slope : coarser.slopes)
			{
				slope *= 2.0f;
			}

			levels.push_back(std::move(coarser));
		}

		heights.assign(static_cast<std::size_t>(levels.back().width) * levels.back().height, 0.0f);
		RelaxHeights(levels.back().slopes, levels.back().width, levels.back().height, coarseHeightIterations, heights);

		for (std::size_t level = levels.size() - 1; level > 0; --level)
		{
			const SlopeLevel& coarse = levels[level];
			const SlopeLevel& fine = levels[level - 1];
			std::vector<float> finer(static_cast<std::size_t>(fine.width) * fine.height);

			for (UINT y = 0; y < fine.height; ++y)
			{
				const UINT coarseY = std::min(y / 2, coarse.height - 1);

				for (UINT x = 0; x < fine.width; ++x)
				{
					const UINT coarseX = std::min(x / 2, coarse.width - 1);
					finer[static_cast<std::size_t>(y) * fine.width + x] = heights[static_cast<std::size_t>(coarseY)
						* coarse.width + coarseX];
				}
			}

			heights.swap(finer);
			RelaxHeights(fine.slopes, fine.width, fine.height, fineHeightIterations, heights);
		}

		const auto range = std::minmax_element(heights.begin(), heights.end());
		const float lowest = *range.first;
		const float span = *range.second - lowest;

		for (float& value : heights)
		{
			if (span > 0.0001f)
			{
				value = (value - lowest) / span;
			}
			else
			{
				value = 1.0f;
			}
		}

		return true;
	}

	static bool TryWriteHeightFile(const TextureCopy& copy, const std::string& fileName)
	{
		std::vector<float> heights;
		UINT width = 0;
		UINT height = 0;

		return TryComputeHeights(copy, heights, width, height)
			&& TryWriteSingleChannelFile(heights, width, height, false, fileName);
	}

	static bool TryComputeRoughness(const ExportJob& job, std::vector<float>& roughness, std::vector<float>& metallic,
		UINT& width, UINT& height)
	{
		DecodedImage image;

		if (!TryDecodeImage(job.specular.bits.data(), job.specular.pitch, job.specular.format, job.specular.width,
				job.specular.height, image))
		{
			return false;
		}

		width = image.width;
		height = image.height;
		roughness.assign(image.texels.size(), 0.0f);
		metallic.assign(image.texels.size(), 0.0f);

		for (std::size_t i = 0; i < image.texels.size(); ++i)
		{
			const float red = Channel(image.texels[i], 16) / 255.0f;
			const float green = Channel(image.texels[i], 8) / 255.0f;
			const float blue = Channel(image.texels[i], 0) / 255.0f;
			const float gloss = Channel(image.texels[i], 24) / 255.0f;

			const float reflectance = (0.2126f * red * red + 0.7152f * green * green + 0.0722f * blue * blue)
				* job.environmentMinimum;
			const float power = std::exp2(9.3775177f * gloss) + 7.0f;
			const float visibility = std::clamp(reflectance / 0.04f, 0.0f, 1.0f);
			float perceptual = std::pow(power, -0.25f);
			perceptual += (1.0f - perceptual) * (1.0f - visibility) * (1.0f - visibility);

			roughness[i] = perceptual * perceptual;
			metallic[i] = std::clamp((reflectance - 0.04f) / (0.30f - 0.04f), 0.0f, 1.0f);
		}

		return true;
	}

	static bool TryWriteSpecularFiles(const ExportJob& job)
	{
		std::vector<float> roughness;
		std::vector<float> metallic;
		UINT width = 0;
		UINT height = 0;

		if (!TryComputeRoughness(job, roughness, metallic, width, height))
		{
			return false;
		}

		if (!TryWriteSingleChannelFile(roughness, width, height, true, job.roughnessFile))
		{
			return false;
		}

		if (job.isMetal && !TryWriteSingleChannelFile(metallic, width, height, false, job.metallicFile))
		{
			return false;
		}

		return true;
	}

	static bool TryWritePuddleFile(const ExportJob& job)
	{
		std::vector<float> heights;
		UINT width = 0;
		UINT height = 0;

		if (!TryComputeHeights(job.normal, heights, width, height))
		{
			return false;
		}

		std::vector<float> roughness;
		std::vector<float> metallic;
		UINT roughnessWidth = 0;
		UINT roughnessHeight = 0;
		const bool hasRoughness = !job.specular.bits.empty()
			&& TryComputeRoughness(job, roughness, metallic, roughnessWidth, roughnessHeight);

		std::vector<float> wet(heights.size());

		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				const std::size_t here = static_cast<std::size_t>(y) * width + x;
				float dry = dryRoughnessConstant;

				if (hasRoughness)
				{
					const UINT sourceX = std::min(x * roughnessWidth / width, roughnessWidth - 1);
					const UINT sourceY = std::min(y * roughnessHeight / height, roughnessHeight - 1);
					dry = roughness[static_cast<std::size_t>(sourceY) * roughnessWidth + sourceX];
				}

				const float water = std::clamp((puddleLevel - heights[here]) / puddleEdge + 0.5f, 0.0f, 1.0f);
				const float damp = dry * wetRoughnessFactor;
				wet[here] = damp + (puddleRoughness - damp) * water;
			}
		}

		return TryWriteSingleChannelFile(wet, width, height, true, job.puddleFile);
	}

	static bool FileExists(const std::string& fileName)
	{
		return GetFileAttributesW((ModDirectory() + L"textures\\" + Widen(fileName)).c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	static bool HasFiles(const PublishedMaterial& material)
	{
		for (const std::string* file : { &material.normalFile, &material.roughnessFile, &material.metallicFile })
		{
			if (!file->empty() && !FileExists(*file))
			{
				return false;
			}
		}

		return true;
	}

	static std::string FieldOrUnset(const std::string& field)
	{
		if (field.empty())
		{
			return unsetField;
		}

		return field;
	}

	static void PublishMod()
	{
		std::vector<std::pair<std::uint64_t, PublishedMaterial>> materials;
		std::vector<std::pair<std::uint64_t, std::uint64_t>> aliasList;
		std::vector<std::uint64_t> glass;
		std::vector<std::pair<std::uint64_t, std::string>> water;
		std::unordered_map<std::uint64_t, EmissiveMaterial> emissive;
		std::unordered_map<std::uint64_t, EmissiveMaterial> fixtures;
		std::unordered_map<std::uint64_t, int> kinds;
		std::unordered_set<std::uint64_t> neon;
		float neonGlow = 0.0f;
		bool isNeonGrey = false;

		{
			std::lock_guard<std::mutex> lock(exportMutex);
			materials.assign(published.begin(), published.end());

			for (auto& entry : materials)
			{
				const bool hasHeight = !entry.second.normalFile.empty()
					&& knownFiles.count(HeightFileName(entry.second.normalFile)) != 0;
				const std::size_t nameStart = entry.second.name.find_first_not_of(' ');
				const bool isWorldMaterial = nameStart != std::string::npos
					&& entry.second.name.compare(nameStart, 3, "wc/") == 0;

				if (hasHeight && isWorldMaterial)
				{
					entry.second.heightFile = HeightFileName(entry.second.normalFile);
				}

				const std::string puddleFile = HashFileName(entry.first, ".w.dds");

				if (isPuddlesActive && floorHashes.count(entry.first) != 0 && knownFiles.count(puddleFile) != 0)
				{
					entry.second.puddleFile = puddleFile;
				}
			}
			neon = neonMasks;
			isNeonGrey = IsNeonGreyscale();

			if (isNeonFoliage)
			{
				neonGlow = neonIntensity;
			}

			aliasList.assign(aliases.begin(), aliases.end());
			glass.assign(glassHashes.begin(), glassHashes.end());
			water.assign(waterNormals.begin(), waterNormals.end());
			emissive = emissiveMaterials;

			if (isFixtureGlow)
			{
				for (const auto& fixture : fixtureMasks)
				{
					fixtures[fixture.first] = EmissiveMaterial{ fixture.second, fixtureGlowIntensity * fixtureGlowBrightness };
				}
			}

			kinds = surfaceKinds;
			hasUnpublishedKinds = false;
			hasUnpublishedAliases = false;
			hasUnpublishedGlass = false;
			hasUnpublishedWater = false;
			hasUnpublishedEmissive = false;
		}

		std::ostringstream kindText;

		for (const auto& kind : kinds)
		{
			char line[24];
			std::snprintf(line, sizeof(line), "%016llX ", static_cast<unsigned long long>(kind.first));
			kindText << line << kind.second << "\n";
		}

		std::ostringstream emissiveText;

		for (const auto& glow : emissive)
		{
			char line[24];
			std::snprintf(line, sizeof(line), "%016llX ", static_cast<unsigned long long>(glow.first));
			emissiveText << line << glow.second.file << " " << glow.second.intensity << "\n";
		}

		std::sort(glass.begin(), glass.end());
		std::sort(water.begin(), water.end());

		std::ostringstream waterText;

		for (const auto& waterEntry : water)
		{
			char line[24];
			std::snprintf(line, sizeof(line), "%016llX ", static_cast<unsigned long long>(waterEntry.first));
			waterText << line << FieldOrUnset(waterEntry.second) << "\n";
		}

		std::ostringstream glassText;

		for (const std::uint64_t hash : glass)
		{
			char line[24];
			std::snprintf(line, sizeof(line), "%016llX\n", static_cast<unsigned long long>(hash));
			glassText << line;
		}

		std::sort(materials.begin(), materials.end(), [](const auto& left, const auto& right)
		{
			return left.first < right.first;
		});

		std::vector<std::pair<std::uint64_t, PublishedMaterial>> present;
		std::ostringstream index;

		std::unordered_map<std::uint64_t, PublishedMaterial> presentByHash;

		for (const auto& entry : materials)
		{
			const PublishedMaterial& material = entry.second;

			if (!HasFiles(material))
			{
				continue;
			}

			present.push_back(entry);
			presentByHash[entry.first] = material;

			char hash[24];
			std::snprintf(hash, sizeof(hash), "%016llX", static_cast<unsigned long long>(entry.first));

			index << hash << " " << FieldOrUnset(material.normalFile) << " " << FieldOrUnset(material.roughnessFile) << " "
				  << FieldOrUnset(material.metallicFile) << " " << material.name << "\n";
		}

		std::ostringstream aliasText;
		int publishedAliases = 0;

		for (const auto& alias : aliasList)
		{
			char line[48];
			std::snprintf(line, sizeof(line), "%016llX %016llX\n", static_cast<unsigned long long>(alias.first),
				static_cast<unsigned long long>(alias.second));
			aliasText << line;

			const auto base = presentByHash.find(alias.second);

			if (base == presentByHash.end() || presentByHash.count(alias.first) != 0)
			{
				continue;
			}

			present.emplace_back(alias.first, base->second);
			++publishedAliases;
		}

		const bool didWriteIndex = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_index.txt", index.str());
		const bool didWriteAliases = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_aliases.txt", aliasText.str());
		const bool didWriteGlass = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_glass.txt", glassText.str());
		const bool didWriteWater = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_water.txt", waterText.str());
		const bool didWriteEmissive = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_emissive.txt", emissiveText.str());
		const bool didWriteKinds = TryWriteFileAtomically(ModDirectory() + L"zw3_pbr_kinds.txt", kindText.str());
		for (const auto& fixture : fixtures)
		{
			emissive.emplace(fixture.first, fixture.second);
		}

		const bool didWriteMod = TryWriteFileAtomically(ModDirectory() + L"mod.usda",
			ModText(present, glass, water, emissive, kinds, neon, neonGlow, isNeonGrey));

		const bool didWriteAll = didWriteIndex && didWriteAliases && didWriteGlass && didWriteWater && didWriteEmissive
			&& didWriteKinds && didWriteMod;

		std::lock_guard<std::mutex> lock(exportMutex);
		didLastPublishFail = !didWriteAll;

		if (!didWriteAll)
		{
			hasUnpublishedKinds = true;
			hasUnpublishedAliases = true;
			hasUnpublishedGlass = true;
			hasUnpublishedWater = true;
			hasUnpublishedEmissive = true;
		}

		++stats.publishes;
		stats.indexedMaterials = static_cast<int>(present.size()) - publishedAliases;
		lastPublishTick = GetTickCount64();

		Device::NoteFrameEvent("pbr mod published");
		Log() << "remix: PBR material mod published with " << present.size() - publishedAliases << " materials and "
			  << publishedAliases << " camo aliases, " << glass.size() << " glass lenses and " << water.size()
			  << " water materials, " << emissive.size() << " glowing materials (index " << didWriteIndex << ", aliases "
			  << didWriteAliases << ", glass " << didWriteGlass << ", water " << didWriteWater << ", glow "
			  << didWriteEmissive << ", skin and foliage " << kinds.size() << " " << didWriteKinds << ", mod " << didWriteMod
			  << "), Remix reloads it within a second";
	}

	static void RunExportWorker()
	{
		for (;;)
		{
			ExportJob job;
			bool shouldPublish = false;

			{
				std::lock_guard<std::mutex> lock(exportMutex);

				if (!jobs.empty())
				{
					job = std::move(jobs.front());
					jobs.pop_front();
				}
				else if (isScanComplete && hasUnpublishedChanges && (isMapLoadPublishDue || isSettingPublishPending))
				{
					hasUnpublishedChanges = false;
					isSettingPublishPending = false;
					shouldPublish = true;
				}
				else
				{
					isWorkerRunning = false;
					stats.isWorkerRunning = false;
					return;
				}
			}

			if (shouldPublish)
			{
				PublishMod();
				continue;
			}

			bool didConvert = true;

			if (job.shouldWriteNormal)
			{
				didConvert = TryWriteNormalFile(job.normal, job.normalFile);
			}

			if (didConvert && job.shouldWriteSpecular)
			{
				didConvert = TryWriteSpecularFiles(job);
			}

			bool didWriteHeight = false;

			if (didConvert && job.shouldWriteHeight)
			{
				didWriteHeight = TryWriteHeightFile(job.normal, job.heightFile);
			}

			bool didWritePuddle = false;

			if (didConvert && job.shouldWritePuddle)
			{
				didWritePuddle = TryWritePuddleFile(job);
			}

			std::lock_guard<std::mutex> lock(exportMutex);

			if (didWritePuddle)
			{
				knownFiles.insert(job.puddleFile);
			}

			if (job.shouldWritePuddle && !didWritePuddle)
			{
				failedPuddles.insert(job.puddleFile);
			}

			if (didWriteHeight)
			{
				knownFiles.insert(job.heightFile);
			}

			if (job.shouldWriteHeight && !didWriteHeight)
			{
				failedHeights.insert(job.heightFile);
			}

			if (!didConvert)
			{
				++stats.failedMaterials;
				continue;
			}

			PublishedMaterial material;
			material.name = job.name;
			material.normalFile = job.normalFile;
			material.roughnessFile = job.roughnessFile;
			material.metallicFile = job.metallicFile;
			published[job.colourHash] = material;
			hasUnpublishedChanges = true;
			++stats.convertedMaterials;
		}
	}

	static void StartExportWorker()
	{
		if (isWorkerRunning)
		{
			return;
		}

		isWorkerRunning = true;
		stats.isWorkerRunning = true;
		std::thread(RunExportWorker).detach();
	}

	static void QueueCandidates()
	{
		int copies = 0;

		while (nextCandidate < candidates.size() && copies < copiesPerFrame)
		{
			{
				std::lock_guard<std::mutex> lock(exportMutex);

				if (jobs.size() >= maxQueuedJobs)
				{
					return;
				}
			}

			const ExportCandidate& candidate = candidates[nextCandidate];
			++nextCandidate;

			ExportJob job;
			job.colourHash = candidate.colourHash;
			job.name = candidate.name;
			job.environmentMinimum = candidate.environmentMinimum;
			job.isMetal = candidate.isMetal;
			if (candidate.normalMap)
			{
				job.normalFile = HashFileName(candidate.normalHash, ".n.dds");
				job.heightFile = HeightFileName(job.normalFile);
			}

			if (candidate.normalMap)
			{
				std::lock_guard<std::mutex> lock(exportMutex);

				if (floorHashes.count(candidate.colourHash) != 0)
				{
					job.puddleFile = HashFileName(candidate.colourHash, ".w.dds");
				}
			}

			if (candidate.specularMap)
			{
				job.roughnessFile = SpecularFileName(candidate.specularHash, candidate.environmentMinimum, candidate.isMetal,
					".r.dds");

				if (candidate.isMetal)
				{
					job.metallicFile = SpecularFileName(candidate.specularHash, candidate.environmentMinimum, true, ".m.dds");
				}
			}

			{
				std::lock_guard<std::mutex> lock(exportMutex);
				job.shouldWriteNormal = !job.normalFile.empty() && knownFiles.count(job.normalFile) == 0;
				job.shouldWriteHeight = !job.heightFile.empty() && knownFiles.count(job.heightFile) == 0
					&& failedHeights.count(job.heightFile) == 0;
				job.shouldWritePuddle = !job.puddleFile.empty() && knownFiles.count(job.puddleFile) == 0
					&& failedPuddles.count(job.puddleFile) == 0;
				job.shouldWriteSpecular = !job.roughnessFile.empty() && knownFiles.count(job.roughnessFile) == 0;
			}

			const bool shouldCopyNormal = job.shouldWriteNormal || job.shouldWriteHeight || job.shouldWritePuddle;
			const bool shouldCopySpecular = job.shouldWriteSpecular || (job.shouldWritePuddle && candidate.specularMap);
			const bool didCopy = (!shouldCopyNormal || TryCopyTopLevel(candidate.normalMap, job.normal))
				&& (!shouldCopySpecular || TryCopyTopLevel(candidate.specularMap, job.specular));

			std::lock_guard<std::mutex> lock(exportMutex);

			if (!didCopy)
			{
				++stats.failedMaterials;
				continue;
			}

			for (const std::string* file : { &job.normalFile, &job.roughnessFile, &job.metallicFile })
			{
				if (!file->empty())
				{
					knownFiles.insert(*file);
				}
			}

			jobs.push_back(std::move(job));
			++copies;
			StartExportWorker();
		}

		if (nextCandidate >= candidates.size())
		{
			std::lock_guard<std::mutex> lock(exportMutex);

			if (!isScanComplete)
			{
				isScanComplete = true;

				if (hasUnpublishedChanges || !jobs.empty())
				{
					StartExportWorker();
				}
			}
		}
	}

	void UpdatePbrMaterials()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.materialPool == 0 || build.gfxWorld == 0)
		{
			return;
		}

		if (!hasLoadedIndex)
		{
			LoadIndex();
		}

		const std::uintptr_t world = GetMapIdentity();

		if (world != settledWorld)
		{
			settledWorld = world;
			scannedWorld = 0;
			settledFrameCount = 0;
			candidates.clear();
			nextCandidate = 0;

			std::lock_guard<std::mutex> lock(exportMutex);
			isMapLoadPublishDue = true;

			const bool hasPending = hasUnpublishedChanges || hasUnpublishedAliases || hasUnpublishedGlass
				|| hasUnpublishedWater || hasUnpublishedEmissive || hasUnpublishedKinds;

			if (hasPending && isScanComplete)
			{
				hasUnpublishedChanges = true;
				StartExportWorker();
			}

			return;
		}

		if (!world)
		{
			return;
		}

		if (settledFrameCount < settleFrames)
		{
			++settledFrameCount;

			if (settledFrameCount == settleFrames)
			{
				std::lock_guard<std::mutex> lock(exportMutex);
				isMapLoadPublishDue = false;
			}

			return;
		}

		if (scannedWorld != world)
		{
			scannedWorld = world;

			{
				std::lock_guard<std::mutex> lock(exportMutex);
				isScanComplete = false;
			}

			CollectCandidates();
		}

		QueueCandidates();

		std::lock_guard<std::mutex> lock(exportMutex);
		const bool isIdle = isScanComplete && !isWorkerRunning && jobs.empty();
		const bool isAliasPublishDue = hasUnpublishedAliases && isIdle
			&& GetTickCount64() - lastPublishTick >= aliasPublishIntervalMs;
		const bool isRetryWaiting = (didLastPublishFail && GetTickCount64() - lastPublishTick < aliasPublishIntervalMs)
			|| GetTickCount64() - lastSettingChangeTick < settingSettleMs;
		const bool isGlassPublishDue = (hasUnpublishedGlass || hasUnpublishedWater || hasUnpublishedEmissive
			|| hasUnpublishedKinds) && isIdle && !isRetryWaiting;

		if ((isAliasPublishDue || isGlassPublishDue) && (isMapLoadPublishDue || isSettingPublishPending))
		{
			hasUnpublishedChanges = true;
			StartExportWorker();
		}
	}

	PbrStats GetPbrStats()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return stats;
	}

	void CollectUntaggedPbrHashes(std::unordered_set<std::uint64_t>& hashes)
	{
		std::ifstream glassFile(ModDirectory() + L"zw3_pbr_glass.txt");
		std::string hash;

		while (glassFile >> hash)
		{
			hashes.insert(std::strtoull(hash.c_str(), nullptr, 16));
		}

		std::ifstream emissiveFile(ModDirectory() + L"zw3_pbr_emissive.txt");
		std::string file;
		float intensity = 0.0f;

		while (emissiveFile >> hash >> file >> intensity)
		{
			hashes.insert(std::strtoull(hash.c_str(), nullptr, 16));
		}
	}

	void RegisterPbrGlass(std::uint64_t hash)
	{
		std::lock_guard<std::mutex> lock(exportMutex);

		if (waterNormals.count(hash) != 0 || !glassHashes.insert(hash).second)
		{
			return;
		}

		hasUnpublishedGlass = true;
		stats.glassMaterials = static_cast<int>(glassHashes.size());
	}

	void RegisterPbrAlias(std::uint64_t aliasHash, std::uint64_t baseHash)
	{
		std::lock_guard<std::mutex> lock(exportMutex);

		const auto found = aliases.find(aliasHash);

		if (found != aliases.end() && found->second == baseHash)
		{
			return;
		}

		aliases[aliasHash] = baseHash;
		hasUnpublishedAliases = true;
		stats.aliases = static_cast<int>(aliases.size());
	}

	static bool TryReadWaterHeights(IDirect3DTexture9* texture, std::vector<float>& heights, UINT& width, UINT& height)
	{
		D3DSURFACE_DESC desc{};

		if (!texture || FAILED(texture->GetLevelDesc(0, &desc)) || desc.Format != D3DFMT_L8)
		{
			return false;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		width = desc.Width;
		height = desc.Height;
		heights.resize(static_cast<std::size_t>(width) * height);

		const auto* bits = static_cast<const unsigned char*>(locked.pBits);

		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				heights[static_cast<std::size_t>(y) * width + x] = bits[static_cast<std::size_t>(y) * locked.Pitch + x] / 255.0f;
			}
		}

		texture->UnlockRect(0);
		return true;
	}

	static void MakeProceduralWaterHeights(std::vector<float>& heights, UINT size)
	{
		float waves[proceduralWaveCount][4];
		std::uint32_t seed = 0x9E3779B9u;

		for (int i = 0; i < proceduralWaveCount; ++i)
		{
			seed = seed * 1664525u + 1013904223u;
			const float angle = static_cast<float>(seed >> 8) / 16777216.0f * 6.2831853f;
			seed = seed * 1664525u + 1013904223u;
			const float radius = 2.0f + static_cast<float>(seed >> 8) / 16777216.0f * 14.0f;
			seed = seed * 1664525u + 1013904223u;
			const float phase = static_cast<float>(seed >> 8) / 16777216.0f * 6.2831853f;

			waves[i][0] = std::round(radius * std::cos(angle));
			waves[i][1] = std::round(radius * std::sin(angle));
			waves[i][2] = std::pow(std::max(1.0f, std::sqrt(waves[i][0] * waves[i][0] + waves[i][1] * waves[i][1])), -1.6f);
			waves[i][3] = phase;
		}

		heights.assign(static_cast<std::size_t>(size) * size, 0.0f);

		for (UINT y = 0; y < size; ++y)
		{
			for (UINT x = 0; x < size; ++x)
			{
				float sum = 0.0f;

				for (const auto& wave : waves)
				{
					const float phase = 6.2831853f * (wave[0] * x + wave[1] * y) / static_cast<float>(size) + wave[3];
					sum += wave[2] * std::sin(phase);
				}

				heights[static_cast<std::size_t>(y) * size + x] = sum;
			}
		}
	}

	static bool TryWriteWaterNormals(const std::vector<float>& heights, UINT width, UINT height, const std::string& fileName)
	{
		std::vector<float> gradients(static_cast<std::size_t>(width) * height * 2);
		double sumSquares = 0.0;

		for (UINT y = 0; y < height; ++y)
		{
			for (UINT x = 0; x < width; ++x)
			{
				const float right = heights[static_cast<std::size_t>(y) * width + (x + 1) % width];
				const float left = heights[static_cast<std::size_t>(y) * width + (x + width - 1) % width];
				const float down = heights[static_cast<std::size_t>((y + 1) % height) * width + x];
				const float up = heights[static_cast<std::size_t>((y + height - 1) % height) * width + x];
				const std::size_t index = static_cast<std::size_t>(y) * width + x;

				gradients[index * 2] = 0.5f * (right - left);
				gradients[index * 2 + 1] = 0.5f * (down - up);
				sumSquares += gradients[index * 2] * gradients[index * 2] + gradients[index * 2 + 1] * gradients[index * 2 + 1];
			}
		}

		const double rms = std::sqrt(sumSquares / (static_cast<double>(width) * height));

		if (rms < 1e-6)
		{
			return false;
		}

		const float scale = waterSlope / static_cast<float>(rms);
		std::vector<float> level(static_cast<std::size_t>(width) * height * 3);

		for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i)
		{
			const float x = -gradients[i * 2] * scale;
			const float y = -gradients[i * 2 + 1] * scale;
			const float length = std::sqrt(x * x + y * y + 1.0f);

			level[i * 3] = x / length;
			level[i * 3 + 1] = y / length;
			level[i * 3 + 2] = 1.0f / length;
		}

		UINT mipWidth = width;
		UINT mipHeight = height;
		const UINT mipCount = MipCount(width, height);
		std::string contents = DdsHeader(width, height, mipCount, dxgiFormatBc5, 16);
		std::vector<float> next;

		for (UINT mip = 0; mip < mipCount; ++mip)
		{
			EncodeNormalLevel(level, mipWidth, mipHeight, contents);
			HalveLevel(level, mipWidth, mipHeight, 3, next);
			level.swap(next);
			mipWidth = std::max(1u, mipWidth / 2);
			mipHeight = std::max(1u, mipHeight / 2);
		}

		return TryWriteFileAtomically(ModDirectory() + L"textures\\" + Widen(fileName), contents);
	}

	static std::string UncompressedDdsHeader(UINT width, UINT height, UINT mipCount)
	{
		std::string header = "DDS ";

		AppendUint32(header, 124);
		AppendUint32(header, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000);
		AppendUint32(header, height);
		AppendUint32(header, width);
		AppendUint32(header, width * 4);
		AppendUint32(header, 0);
		AppendUint32(header, mipCount);
		header.append(44, '\0');

		AppendUint32(header, 32);
		AppendUint32(header, 0x4);
		header.append("DX10", 4);
		header.append(20, '\0');

		AppendUint32(header, 0x1000 | 0x400000 | 0x8);
		header.append(16, '\0');

		AppendUint32(header, dxgiFormatB8G8R8A8);
		AppendUint32(header, 3);
		AppendUint32(header, 0);
		AppendUint32(header, 1);
		AppendUint32(header, 0);

		return header;
	}

	static float LeafWeight(float red, float green, float blue)
	{
		const float brightest = std::max({ red, green, blue });
		const float darkest = std::min({ red, green, blue });

		if (brightest <= 0.0f)
		{
			return 0.0f;
		}

		const float saturation = (brightest - darkest) / brightest;
		float hue = 0.0f;

		if (brightest == red)
		{
			hue = 60.0f * std::fmod((green - blue) / std::max(brightest - darkest, 1e-5f), 6.0f);
		}
		else if (brightest == green)
		{
			hue = 60.0f * ((blue - red) / std::max(brightest - darkest, 1e-5f) + 2.0f);
		}
		else
		{
			hue = 60.0f * ((red - green) / std::max(brightest - darkest, 1e-5f) + 4.0f);
		}

		if (hue < 0.0f)
		{
			hue += 360.0f;
		}

		const auto ramp = [](float value, float low, float high)
		{
			return std::clamp((value - low) / (high - low), 0.0f, 1.0f);
		};

		float leafy = 0.0f;

		if (hue >= 50.0f && hue <= 170.0f)
		{
			leafy = ramp(saturation, 0.12f, 0.3f);
		}

		float vivid = ramp(saturation, 0.45f, 0.65f);

		if (hue < 50.0f || hue > 330.0f)
		{
			vivid = ramp(saturation, 0.6f, 0.8f);
		}

		return std::max(leafy, vivid);
	}

	static bool TryWriteEmissiveMask(IDirect3DTexture9* texture, const std::string& fileName, bool isGreyscale,
		int maskKind)
	{
		D3DSURFACE_DESC desc{};

		if (!texture || FAILED(texture->GetLevelDesc(0, &desc)) || !IsDecodableFormat(desc.Format))
		{
			return false;
		}

		UINT level = 0;
		const UINT levels = texture->GetLevelCount();

		while (level + 1 < levels && (desc.Width > maxEmissiveSize || desc.Height > maxEmissiveSize))
		{
			++level;

			if (FAILED(texture->GetLevelDesc(level, &desc)))
			{
				return false;
			}
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(level, &locked, nullptr, D3DLOCK_READONLY)))
		{
			return false;
		}

		DecodedImage image;
		const bool didDecode = TryDecodeImage(static_cast<const unsigned char*>(locked.pBits), static_cast<UINT>(locked.Pitch),
			desc.Format, desc.Width, desc.Height, image);
		texture->UnlockRect(level);

		if (!didDecode)
		{
			return false;
		}

		UINT width = image.width;
		UINT height = image.height;
		std::vector<float> levelData(static_cast<std::size_t>(width) * height * 4);

		for (std::size_t i = 0; i < image.texels.size(); ++i)
		{
			levelData[i * 4] = Channel(image.texels[i], 0) / 255.0f;
			levelData[i * 4 + 1] = Channel(image.texels[i], 8) / 255.0f;
			levelData[i * 4 + 2] = Channel(image.texels[i], 16) / 255.0f;
			levelData[i * 4 + 3] = 1.0f;

			if (maskKind == maskBright)
			{
				const float brightest = std::max({ levelData[i * 4], levelData[i * 4 + 1], levelData[i * 4 + 2] });
				const float weight = std::clamp((brightest - brightMaskLow) / (brightMaskHigh - brightMaskLow), 0.0f, 1.0f);
				levelData[i * 4] *= weight;
				levelData[i * 4 + 1] *= weight;
				levelData[i * 4 + 2] *= weight;
			}

			if (maskKind == maskLeaf)
			{
				const float weight = LeafWeight(levelData[i * 4 + 2], levelData[i * 4 + 1], levelData[i * 4]);
				levelData[i * 4] *= weight;
				levelData[i * 4 + 1] *= weight;
				levelData[i * 4 + 2] *= weight;
			}

			if (isGreyscale)
			{
				const float brightest = std::max({ levelData[i * 4], levelData[i * 4 + 1], levelData[i * 4 + 2] });
				levelData[i * 4] = brightest;
				levelData[i * 4 + 1] = brightest;
				levelData[i * 4 + 2] = brightest;
			}
		}

		const UINT mipCount = MipCount(width, height);
		std::string contents = UncompressedDdsHeader(width, height, mipCount);
		std::vector<float> next;

		for (UINT mip = 0; mip < mipCount; ++mip)
		{
			for (const float value : levelData)
			{
				contents.push_back(static_cast<char>(static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f)));
			}

			HalveLevel(levelData, width, height, 4, next);
			levelData.swap(next);
			width = std::max(1u, width / 2);
			height = std::max(1u, height / 2);
		}

		return TryWriteFileAtomically(ModDirectory() + L"textures\\" + Widen(fileName), contents);
	}

	void RegisterPbrFixture(std::uint64_t hash, IDirect3DTexture9* colourTexture)
	{
		{
			std::lock_guard<std::mutex> lock(exportMutex);

			if (fixtureMasks.count(hash) != 0 || failedMasks.count(hash) != 0)
			{
				return;
			}
		}

		char fileName[48];
		std::snprintf(fileName, sizeof(fileName), "fixture_%016llX.dds", static_cast<unsigned long long>(hash));

		if (!FileExists(fileName) && !TryWriteEmissiveMask(colourTexture, fileName, false, maskBright))
		{
			std::lock_guard<std::mutex> lock(exportMutex);
			failedMasks.insert(hash);
			return;
		}

		std::lock_guard<std::mutex> lock(exportMutex);
		fixtureMasks[hash] = fileName;
		++fixtureVersion;

		if (isFixtureGlow)
		{
			hasUnpublishedEmissive = true;
		}
	}

	bool IsFixtureHash(std::uint64_t hash)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return fixtureMasks.count(hash) != 0;
	}

	unsigned GetFixtureVersion()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return fixtureVersion;
	}

	void SetFixtureGlow(bool isEnabled, float brightness)
	{
		std::lock_guard<std::mutex> lock(exportMutex);

		if (isFixtureGlow == isEnabled && fixtureGlowBrightness == brightness)
		{
			return;
		}

		isFixtureGlow = isEnabled;
		fixtureGlowBrightness = brightness;
		hasUnpublishedEmissive = true;
		isSettingPublishPending = true;
		lastSettingChangeTick = GetTickCount64();
	}

	void RegisterPbrFloor(std::uint64_t hash)
	{
		std::lock_guard<std::mutex> lock(exportMutex);

		if (floorHashes.insert(hash).second && isPuddlesActive)
		{
			hasUnpublishedKinds = true;
		}
	}

	void SetPuddlesActive(bool isActive)
	{
		std::lock_guard<std::mutex> lock(exportMutex);

		if (isPuddlesActive == isActive)
		{
			return;
		}

		isPuddlesActive = isActive;
		hasUnpublishedKinds = true;
		isSettingPublishPending = true;
		lastSettingChangeTick = GetTickCount64();
	}

	void RegisterPbrSurfaceKind(std::uint64_t hash, int kind)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		const auto found = surfaceKinds.find(hash);

		if (found != surfaceKinds.end() && found->second == kind)
		{
			return;
		}

		if (found != surfaceKinds.end() && found->second == surfaceKindSkin)
		{
			--stats.skinMaterials;
		}

		if (found != surfaceKinds.end() && found->second == surfaceKindFoliage)
		{
			--stats.foliageMaterials;
		}

		surfaceKinds[hash] = kind;
		hasUnpublishedKinds = true;

		if (kind == surfaceKindSkin)
		{
			++stats.skinMaterials;
		}

		if (kind == surfaceKindFoliage)
		{
			++stats.foliageMaterials;
		}
	}

	void RegisterPbrFoliageMask(std::uint64_t hash, IDirect3DTexture9* colourTexture)
	{
		{
			std::lock_guard<std::mutex> lock(exportMutex);

			if (!isNeonFoliage || neonMasks.count(hash) != 0 || failedMasks.count(hash) != 0)
			{
				return;
			}
		}

		const std::string fileName = NeonFileName(hash);
		const std::string greyFileName = NeonGreyFileName(hash);

		const bool hasColourMask = FileExists(fileName) || TryWriteEmissiveMask(colourTexture, fileName, false, maskLeaf);
		const bool hasGreyMask = hasColourMask
			&& (FileExists(greyFileName) || TryWriteEmissiveMask(colourTexture, greyFileName, true, maskLeaf));

		if (!hasGreyMask)
		{
			std::lock_guard<std::mutex> lock(exportMutex);
			failedMasks.insert(hash);
			return;
		}

		std::lock_guard<std::mutex> lock(exportMutex);
		neonMasks.insert(hash);

		if (isNeonFoliage)
		{
			hasUnpublishedKinds = true;
		}
	}

	void SetNeonFoliage(bool isEnabled, float intensity)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		const float clamped = std::clamp(intensity, 0.0f, 50.0f);

		if (isNeonFoliage == isEnabled && neonIntensity == clamped)
		{
			return;
		}

		isNeonFoliage = isEnabled;
		neonIntensity = clamped;
		hasUnpublishedKinds = true;
		isSettingPublishPending = true;
	}

	bool IsNeonFoliage()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return isNeonFoliage;
	}

	float GetNeonIntensity()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return neonIntensity;
	}

	void SetNeonPulse(const NeonPulse& pulse)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		neonPulse.isEnabled = pulse.isEnabled;
		neonPulse.speed = std::clamp(pulse.speed, 0.01f, 5.0f);
		neonPulse.depth = std::clamp(pulse.depth, 0.0f, 1.0f);
	}

	NeonPulse GetNeonPulse()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return neonPulse;
	}

	bool TryGetNeonPulseFactor(std::uint64_t hash, float& factor)
	{
		NeonPulse pulse;

		{
			std::lock_guard<std::mutex> lock(exportMutex);

			if (!isNeonFoliage || !neonPulse.isEnabled || neonMasks.count(hash) == 0)
			{
				return false;
			}

			pulse = neonPulse;
		}

		const float phase = static_cast<float>(hash & 0xFFFF) / 65535.0f;
		const float speedSpread = 0.7f + 0.6f * static_cast<float>((hash >> 16) & 0xFFFF) / 65535.0f;
		const double seconds = static_cast<double>(GetTickCount64()) / 1000.0;
		const double cycles = seconds * pulse.speed * speedSpread + phase;
		const float wave = 0.5f + 0.5f * static_cast<float>(std::sin(cycles * 6.283185307179586));

		factor = 1.0f - pulse.depth * (1.0f - wave);
		return true;
	}

	void SetAlienGlow(const AlienGlow& glow)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		const bool wasGreyscale = IsNeonGreyscale();

		alienGlow = glow;
		alienGlow.style = std::clamp(glow.style, 0, 1);
		alienGlow.speed = std::clamp(glow.speed, 0.01f, 5.0f);
		alienGlow.depth = std::clamp(glow.depth, 0.0f, 1.0f);
		alienGlow.scale = std::clamp(glow.scale, 0.1f, 10.0f);
		alienGlow.colourMode = std::clamp(glow.colourMode, 0, 3);

		for (int channel = 0; channel < 3; ++channel)
		{
			alienGlow.tint[channel] = std::clamp(glow.tint[channel], 0.0f, 1.0f);
		}

		if (IsNeonGreyscale() != wasGreyscale)
		{
			hasUnpublishedKinds = true;
			isSettingPublishPending = true;
		}
	}

	AlienGlow GetAlienGlow()
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return alienGlow;
	}

	bool IsNeonFoliageHash(std::uint64_t hash)
	{
		std::lock_guard<std::mutex> lock(exportMutex);
		return isNeonFoliage && neonMasks.count(hash) != 0;
	}

	void RegisterPbrEmissive(std::uint64_t hash, IDirect3DTexture9* colourTexture, float intensity)
	{
		{
			std::lock_guard<std::mutex> lock(exportMutex);
			const auto found = emissiveMaterials.find(hash);

			if (found != emissiveMaterials.end() && found->second.intensity == intensity)
			{
				return;
			}

			if (failedMasks.count(hash) != 0)
			{
				return;
			}
		}

		char fileName[48];
		std::snprintf(fileName, sizeof(fileName), "glow_%016llX.dds", static_cast<unsigned long long>(hash));

		if (!FileExists(fileName) && !TryWriteEmissiveMask(colourTexture, fileName, false, maskFull))
		{
			std::lock_guard<std::mutex> lock(exportMutex);
			failedMasks.insert(hash);
			return;
		}

		Log() << "remix: " << fileName << " exported, the self-lit surface glows at intensity " << intensity
			  << " through the PBR mod (scaled live by Glow brightness)";

		std::lock_guard<std::mutex> lock(exportMutex);
		emissiveMaterials[hash] = { fileName, intensity };
		hasUnpublishedEmissive = true;
		stats.emissiveMaterials = static_cast<int>(emissiveMaterials.size());
	}

	void RegisterPbrWater(std::uint64_t hash, IDirect3DTexture9* heightTexture)
	{
		std::string exportedNormal;

		{
			std::lock_guard<std::mutex> lock(exportMutex);

			if (waterNormals.count(hash) != 0)
			{
				return;
			}

			const auto found = published.find(hash);

			if (found != published.end())
			{
				exportedNormal = found->second.normalFile;
			}
		}

		if (!heightTexture && !exportedNormal.empty())
		{
			Log() << "remix: scrolling water layer " << exportedNormal << " becomes Remix water with its own normal map";

			std::lock_guard<std::mutex> lock(exportMutex);
			waterNormals[hash] = exportedNormal;
			glassHashes.erase(hash);
			hasUnpublishedWater = true;
			stats.glassMaterials = static_cast<int>(glassHashes.size());
			stats.waterMaterials = static_cast<int>(waterNormals.size());
			return;
		}

		std::vector<float> heights;
		UINT width = 0;
		UINT height = 0;
		bool isGameWater = TryReadWaterHeights(heightTexture, heights, width, height);

		if (heightTexture && !isGameWater)
		{
			D3DSURFACE_DESC desc{};
			heightTexture->GetLevelDesc(0, &desc);
			Log() << "remix: the game's wave map could not be read (format " << desc.Format << ", pool " << desc.Pool
				  << ", " << desc.Width << "x" << desc.Height << "), water uses procedural waves";
		}

		char fileName[48];
		std::snprintf(fileName, sizeof(fileName), "waves_%016llX_n.dds", static_cast<unsigned long long>(hash));

		bool didWrite = isGameWater && TryWriteWaterNormals(heights, width, height, fileName);

		if (!didWrite)
		{
			isGameWater = false;
			width = proceduralWaterSize;
			height = proceduralWaterSize;
			MakeProceduralWaterHeights(heights, width);
			didWrite = TryWriteWaterNormals(heights, width, height, fileName);
		}

		std::string normalFile;

		if (didWrite)
		{
			normalFile = fileName;
		}

		Log() << "remix: water surfaces (colour map " << fileName << ") become Remix water, normals from the game's wave"
				 " map " << isGameWater << ", normal map written " << didWrite;

		std::lock_guard<std::mutex> lock(exportMutex);
		waterNormals[hash] = normalFile;
		glassHashes.erase(hash);
		hasUnpublishedWater = true;
		stats.glassMaterials = static_cast<int>(glassHashes.size());
		stats.waterMaterials = static_cast<int>(waterNormals.size());
	}
}
