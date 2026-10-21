#include "STDInclude.hpp"

#include "TextureTags.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Api.hpp"
#include "Look.hpp"
#include "MapSettings.hpp"
#include "PbrMaterials.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	static constexpr std::uint8_t noStateBits = 0xFF;
	static constexpr int replaceBlend = 0x012;
	static constexpr int additiveBlend = 0x122;
	static constexpr int screenBlend = 0x12A;
	static constexpr int multiplyBlend = 0x131;
	static constexpr int premultipliedBlend = 0x162;
	static constexpr int alphaBlend = 0x165;
	static constexpr int lastLayerSortKey = 18;
	static constexpr unsigned passInterval = 300;
	static constexpr unsigned firstPassDelay = 2;
	static constexpr unsigned earlyPassInterval = 30;
	static constexpr unsigned earlyPassFrames = 900;
	static constexpr const wchar_t* tagFileName = L"zw3_remix_textures.conf";
	static constexpr const wchar_t* tagReportFileName = L"zw3_remix_textures_report.txt";
	static constexpr int namesPerTexture = 4;

	static const char* const tagOptions[textureTagCount] = {
		"rtx.skyBoxTextures",
		"rtx.worldSpaceUiTextures",
		"rtx.beamTextures",
		"rtx.decalTextures",
		"rtx.particleTextures",
		"rtx.animatedWaterTextures",
		"rtx.uiTextures",
		"rtx.ignoreTextures",
	};

	struct MaterialFacts
	{
		char name[128];
		char techniqueSet[64];
		char imageName[64];
		std::uint8_t sortKey;
		std::uint8_t cameraRegion;
		int blend;
		int polygonOffset;
		bool isAlphaTested;
		bool isAlphaTestedAtHalf;
		bool hasTexture;
		bool hasWaterTexture;
		void* waterTexture;
		void* colourTexture;
		std::uint64_t textureHash;
		bool isForcedCutout;
	};

	struct Classification
	{
		int tag;
		bool isGeometry;
		bool isGlass = false;
		bool isSelfLit = false;
		bool isWater = false;
		bool isBlendedLayer = false;
	};

	struct TextureUse
	{
		std::uint32_t tagMask = 0;
		bool hasGeometryUser = false;
		bool hasNonUiUser = false;
		std::string materialNames;
		int materialCount = 0;
		bool hasGlassUser = false;
		bool isCodeImage = false;
		std::string imageName;
		std::string selfLitMaterial;
		void* selfLitTexture = nullptr;
		void* fixtureTexture = nullptr;
		bool isLampLight = false;
		bool hasWaterUser = false;
		bool hasBlendedLayerUser = false;
		bool hasLeafUser = false;
		bool hasNonLeafUser = false;
		void* leafTexture = nullptr;
		bool hasUnnamedLeafUser = false;
		bool hasSkinUser = false;
		bool hasNonSkinUser = false;
		void* waterTexture = nullptr;
	};

	struct WithdrawnTag
	{
		int option;
		std::uint64_t hash;
		std::string imageName;
	};

	static std::unordered_set<std::uint64_t> pushed[textureTagCount];
	static std::unordered_set<std::uint64_t> saved[textureTagCount];
	static std::unordered_set<std::uint64_t> selfLitSeen;
	static std::unordered_set<std::string> reportedMaterials;
	static constexpr std::size_t maxReportedMaterials = 300;
	static std::unordered_set<std::uint64_t> withdrawn[textureTagCount];
	static std::vector<WithdrawnTag> withdrawnLog;
	static std::unordered_set<std::uint64_t> sharedLayerTextures;
	static bool hasReadSaved = false;
	static bool isSharedDecalTagged = true;
	static TextureTagStats stats;
	static std::uintptr_t settledWorld = 0;
	static std::uintptr_t taggedWorld = 0;
	static unsigned settledFrames = 0;

	static void CopyText(char* destination, std::size_t capacity, const char* source)
	{
		std::size_t length = 0;

		if (source)
		{
			while (length + 1 < capacity && source[length] != '\0')
			{
				destination[length] = source[length];
				++length;
			}
		}

		destination[length] = '\0';
	}

	static void ReadMainStateBits(std::uintptr_t material, MaterialFacts& facts)
	{
		facts.blend = -1;
		facts.polygonOffset = 0;

		const std::uintptr_t entries = material + Iw4::materialStateBitsEntry;
		std::uint8_t entry = ReadAt<std::uint8_t>(entries + Iw4::techniqueLit);

		if (entry == noStateBits)
		{
			entry = ReadAt<std::uint8_t>(entries + Iw4::techniqueEmissive);
		}

		if (entry == noStateBits)
		{
			entry = ReadAt<std::uint8_t>(entries + Iw4::techniqueUnlit);
		}

		const auto stateBitsCount = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsCount);
		const auto stateBits = ReadAt<std::uintptr_t>(material + Iw4::materialStateBitsTable);

		if (entry == noStateBits || entry >= stateBitsCount || !stateBits)
		{
			return;
		}

		const std::uintptr_t loadBits = stateBits + entry * Iw4::stateBitsStride;

		const auto firstBits = ReadAt<std::uint32_t>(loadBits);

		facts.blend = static_cast<int>(firstBits & 0x7FFu);
		facts.polygonOffset = static_cast<int>((ReadAt<std::uint32_t>(loadBits + sizeof(std::uint32_t)) >> 4) & 3u);
		facts.isAlphaTested = (firstBits & 0x800u) == 0 && (firstBits & 0x3000u) != 0;
		facts.isAlphaTestedAtHalf = (firstBits & 0x800u) == 0 && (firstBits & 0x3000u) == 0x3000u;
	}

	static bool IsCutoutMaterial(std::string_view name)
	{
		for (const std::string& listed : GetMapSettings().cutoutMaterials)
		{
			if (name == listed)
			{
				return true;
			}
		}

		return name.find("curtain") != std::string_view::npos;
	}

	// converted maps draw some cloth opaque although its colour map has holes; those texels decode to transparent black,
	// so the lit pass is switched to the engine's alpha test at half (GFXS0_ATEST_GE_128) and Remix cuts them out
	static bool TryForceCutout(std::uintptr_t material, const MaterialFacts& facts)
	{
		if (facts.blend != replaceBlend || facts.isAlphaTested || !IsCutoutMaterial(facts.name))
		{
			return false;
		}

		const std::uint8_t entry = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsEntry + Iw4::techniqueLit);
		const auto stateBitsCount = ReadAt<std::uint8_t>(material + Iw4::materialStateBitsCount);
		const auto stateBits = ReadAt<std::uintptr_t>(material + Iw4::materialStateBitsTable);

		if (entry == noStateBits || entry >= stateBitsCount || !stateBits)
		{
			return false;
		}

		auto* const loadBits = reinterpret_cast<std::uint32_t*>(stateBits + entry * Iw4::stateBitsStride);
		*loadBits = (*loadBits & ~0x3800u) | 0x3000u;
		return true;
	}

	static void ReadColorTexture(std::uintptr_t material, MaterialFacts& facts)
	{
		const auto textureCount = ReadAt<std::uint8_t>(material + Iw4::materialTextureCount);
		const auto textures = ReadAt<std::uintptr_t>(material + Iw4::materialTextureTable);

		if (!textures || textureCount == 0)
		{
			return;
		}

		std::uintptr_t chosen = 0;

		for (std::uint8_t i = 0; i < textureCount; ++i)
		{
			const std::uintptr_t definition = textures + i * Iw4::textureDefStride;

			if (ReadAt<std::uint8_t>(definition + Iw4::textureDefSemantic) == Iw4::textureSemanticWater)
			{
				facts.hasWaterTexture = true;

				const auto water = ReadAt<std::uintptr_t>(definition + Iw4::textureDefImage);

				if (water)
				{
					const auto waterImage = ReadAt<std::uintptr_t>(water + Iw4::waterImage);

					if (waterImage)
					{
						facts.waterTexture = ReadAt<void*>(waterImage + Iw4::gfxImageTexture);
					}
				}

				continue;
			}

			if (!chosen)
			{
				chosen = definition;
			}

			if (ReadAt<std::uint32_t>(definition + Iw4::textureDefNameHash) == Iw4::colorMapNameHash)
			{
				chosen = definition;
				break;
			}
		}

		if (!chosen)
		{
			return;
		}

		const auto image = ReadAt<std::uintptr_t>(chosen + Iw4::textureDefImage);

		if (!image)
		{
			return;
		}

		CopyText(facts.imageName, sizeof(facts.imageName), ReadAt<const char*>(image + Iw4::gfxImageName));

		if (ReadAt<std::uint8_t>(image + Iw4::gfxImageMapType) != Iw4::gfxImageMapType2D)
		{
			return;
		}

		facts.hasTexture = true;
		facts.colourTexture = ReadAt<void*>(image + Iw4::gfxImageTexture);
		Device::TryGetTextureHash(ReadAt<const void*>(image + Iw4::gfxImageTexture), facts.textureHash);
	}

	static std::uint64_t lastPassFingerprint = 0;

	static std::uint64_t MaterialPoolFingerprint(const Engine::MaterialPool& materialPool)
	{
		std::uint64_t fingerprint = 1469598103934665603ull;

		for (std::size_t i = 0; i < materialPool.count; ++i)
		{
			fingerprint = (fingerprint ^ ReadAt<std::uint64_t>(materialPool.entries + i * materialPool.stride)) * 1099511628211ull;
		}

		fingerprint ^= static_cast<std::uint64_t>(GetLookSettings().isFixtureLights) << 1;
		fingerprint ^= static_cast<std::uint64_t>(isSharedDecalTagged) << 2;

		return fingerprint;
	}

	static bool TryReadMaterial(std::uintptr_t material, std::uintptr_t poolStart, std::uintptr_t poolEnd,
		MaterialFacts& facts)
	{
		__try
		{
			const auto first = ReadAt<std::uintptr_t>(material);

			if (first == 0 || (first >= poolStart && first < poolEnd))
			{
				return false;
			}

			CopyText(facts.name, sizeof(facts.name), reinterpret_cast<const char*>(first));
			facts.sortKey = ReadAt<std::uint8_t>(material + Iw4::materialSortKey);
			facts.cameraRegion = ReadAt<std::uint8_t>(material + Iw4::materialCameraRegion);

			const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);
			const char* techniqueSetName = nullptr;

			if (techniqueSet)
			{
				techniqueSetName = ReadAt<const char*>(techniqueSet);
			}

			CopyText(facts.techniqueSet, sizeof(facts.techniqueSet), techniqueSetName);
			ReadMainStateBits(material, facts);

			if (TryForceCutout(material, facts))
			{
				ReadMainStateBits(material, facts);
				facts.isForcedCutout = true;
			}

			ReadColorTexture(material, facts);

			// maps converted from black ops leave their alpha blended world decals in the opaque region; models with
			// blended materials there (zw3's zombies) are left alone so their bodies are never taken for decals
			const bool isAlphaBlended = facts.blend == alphaBlend || facts.blend == premultipliedBlend;
			const bool isWorldMaterial = std::string_view(facts.name).substr(0, 3) != "mc/";

			if (facts.cameraRegion == 0 && isAlphaBlended && isWorldMaterial)
			{
				facts.cameraRegion = 1;
			}

			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static bool Contains(std::string_view text, std::string_view part)
	{
		return text.find(part) != std::string_view::npos;
	}

	static bool ContainsAny(std::string_view text, std::initializer_list<std::string_view> parts)
	{
		for (const std::string_view part : parts)
		{
			if (Contains(text, part))
			{
				return true;
			}
		}

		return false;
	}

	static bool StartsWithAny(std::string_view text, std::initializer_list<std::string_view> prefixes)
	{
		for (const std::string_view prefix : prefixes)
		{
			if (text.substr(0, prefix.size()) == prefix)
			{
				return true;
			}
		}

		return false;
	}

	static bool EndsWith(std::string_view text, std::string_view suffix)
	{
		return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
	}

	static std::string_view StripComma(const char* text)
	{
		std::string_view view(text);

		if (!view.empty() && view.front() == ',')
		{
			view.remove_prefix(1);
		}

		return view;
	}

	// some converted maps (prototype, castle night) name every technique set "<name>.3x", which would defeat the exact
	// and suffix rules below
	static std::string_view TechniqueSetName(const char* text)
	{
		std::string_view view = StripComma(text);

		if (EndsWith(view, ".3x"))
		{
			view.remove_suffix(3);
		}

		return view;
	}

	static bool HasToken(std::string_view text, std::initializer_list<std::string_view> tokens)
	{
		std::size_t start = 0;

		while (start <= text.size())
		{
			std::size_t end = text.find('_', start);

			if (end == std::string_view::npos)
			{
				end = text.size();
			}

			const std::string_view token = text.substr(start, end - start);

			for (const std::string_view wanted : tokens)
			{
				if (token == wanted)
				{
					return true;
				}
			}

			start = end + 1;
		}

		return false;
	}

	static bool HasObjectiveSuffix(std::string_view name)
	{
		std::size_t at = name.find("_obj");

		while (at != std::string_view::npos)
		{
			const std::size_t next = at + 4;

			if (next == name.size() || name[next] == '_')
			{
				return true;
			}

			at = name.find("_obj", at + 1);
		}

		return false;
	}

	static std::string_view GlassBaseName(std::string_view name)
	{
		std::string_view base = name;
		const std::size_t slash = base.rfind('/');

		if (slash != std::string_view::npos)
		{
			base = base.substr(slash + 1);
		}

		if (base.substr(0, 4) == "mtl_")
		{
			base = base.substr(4);
		}

		return base;
	}

	static bool IsGlassName(std::string_view name)
	{
		const std::string_view base = GlassBaseName(name);
		const bool hasGlassToken = ContainsAny(base, { "glass", "glss", "window", "windshield", "windscreen", "_win_", "lens",
			"plexi", "plastic_clear", "transparent", "fishtank" }) || EndsWith(base, "_shield");
		const bool isNotGlass = ContainsAny(base, { "fiberglass", "sunglasses", "glassblock", "brickglass", "mirror",
			"windowshade", "windowframe", "window_frame", "windowfence", "window_shutter", "windowshutter", "curtain", "blinds",
			"newspaper", "facade", "trim_window", "castlewindow", "glass_bars" }) || EndsWith(base, "riot_shield");

		return hasGlassToken && !isNotGlass;
	}

	static bool IsVegetationName(std::string_view name)
	{
		const bool hasToken = ContainsAny(name, { "tree", "leaf", "leaves", "bush", "grass", "ivy", "palm", "fern", "foliage",
			"branch", "canopy", "plant", "vine", "shrub", "hedge", "reed", "weed", "crop", "oak", "birch", "flower", "moss",
			"cactus", "bamboo", "frond", "cattail", "lily", "cruciform", "needles", "topiary", "poppy", "dandelion", "treeline" })
			|| (Contains(name, "pine") && !ContainsAny(name, { "pinewood", "pineapple" }))
			|| (Contains(name, "brush") && !ContainsAny(name, { "brushed", "brushmetal" }));
		const bool isNotVegetation = ContainsAny(name, { "mtl_canopy0", "canopy_metal", "marketstall_canopy", "flower_pot",
			"street", "godray", "shadow_caster", "haybale", "haycart", "split_log", "_log_", "crosswalk", "facade", "castleivy",
			"lilypad", "tree_surround" });

		return hasToken && !isNotVegetation;
	}

	static bool IsLeafMaterial(const MaterialFacts& facts)
	{
		const std::string_view name = StripComma(facts.name);
		const std::string_view techniqueSet = TechniqueSetName(facts.techniqueSet);
		const bool isOpaqueCutout = (facts.cameraRegion == 0 || facts.cameraRegion == 1) && facts.blend == replaceBlend
			&& facts.isAlphaTestedAtHalf;
		const bool isLeafKind = Contains(techniqueSet, "_ambient_") || IsVegetationName(name);

		return isOpaqueCutout && isLeafKind && !ContainsAny(name, { "bark", "trunk", "stump", "facade" });
	}

	static bool IsSkinColourMap(std::string_view imageName)
	{
		static constexpr std::string_view faces[] = {
			"airborne_head_d_clr", "airborne_head_e_clr", "ally_sniper_army_head_col", "ally_ghillie_tf141_arctic_head_col",
			"seal_tf141_arctic_sniper_head_col", "ally_sniper_tf141_dsrt_head_col", "ally_sniper_tf141_frst_head_col",
			"seal_udt_sniper_head_a_col", "arab_head_skin_col", "arab_head_skin_b_col", "arctic_head_b_col", "militia_head_a_col",
			"militia_head_b_col", "seal_socom_head_b_col", "seal_soccom_head_c_col", "seal_soccom_head_d_col",
			"navy_seal_head_b_col", "navy_seal_head_d_col", "us_army_head_b_col", "us_army_head_c_col", "us_army_head_d_col",
		};

		for (const std::string_view face : faces)
		{
			if (imageName == face)
			{
				return true;
			}
		}

		return false;
	}

	static float SelfLitIntensity(std::string_view name)
	{
		if (ContainsAny(name, { "light_stick", "lightstick", "glowstick", "glow_stick" }))
		{
			return 24.0f;
		}

		return 2.0f;
	}

	// the lit half of a converted map's lamp models is a self-lit unlit material, usually named *_light*_on or *_lamp*
	static bool IsLampName(std::string_view name)
	{
		const bool isNamedLight = ContainsAny(name, { "light", "lamp", "lantern", "bulb", "sconce", "chandelier",
			"candle", "torch" });
		const bool isEffect = ContainsAny(name, { "gfx_", "lightning", "_eye", "flare", "godray", "beam" });

		return isNamedLight && !isEffect;
	}

	static std::unordered_set<std::uint64_t> lampLightHashes;
	static unsigned lampLightVersion = 0;

	bool IsLampLightHash(std::uint64_t hash)
	{
		return lampLightHashes.count(hash) != 0;
	}

	unsigned GetLampLightVersion()
	{
		return lampLightVersion;
	}

	static bool IsFixtureMaterial(const MaterialFacts& facts)
	{
		const std::string_view name = StripComma(facts.name);
		const bool isLamp = ContainsAny(name, { "light_fixture", "lightfixture", "fluorescent", "flourescent", "lamp",
			"bulb", "lantern", "chandelier", "streetlight", "street_light", "exit_sign", "neon", "lightbox",
			"light_box", "emergency_light", "spotlight", "monitor", "screen", "television", "_tv" });

		return facts.cameraRegion == 0 && facts.blend == replaceBlend && isLamp;
	}

	static void ReportDiagnosticMaterial(const MaterialFacts& facts)
	{
		const std::string_view techniqueSet = TechniqueSetName(facts.techniqueSet);
		const std::string_view name = StripComma(facts.name);
		const bool isOutdoor = Contains(techniqueSet, "outdoor");
		const bool isWeaponOptic = Contains(name, "mtl_weapon") && ContainsAny(name, { "lens", "scope", "glass", "reticle" });
		const bool isWet = ContainsAny(name, { "puddle", "wet", "water", "rain", "mud" });
		const bool isMetalLayer = facts.cameraRegion == 1 && (facts.blend == premultipliedBlend || facts.blend == alphaBlend)
			&& ContainsAny(name, { "pipe", "rust", "metal" });

		if (!isOutdoor && !isWeaponOptic && !isWet && !isMetalLayer)
		{
			return;
		}

		if (reportedMaterials.size() >= maxReportedMaterials || !reportedMaterials.insert(std::string(name)).second)
		{
			return;
		}

		const std::string_view imageName = StripComma(facts.imageName);
		char text[320];
		std::snprintf(text, sizeof(text), "remix: material %.*s technique set %.*s region %d blend 0x%X sort %d image %.*s"
			" water %d", static_cast<int>(name.size()), name.data(), static_cast<int>(techniqueSet.size()),
			techniqueSet.data(), facts.cameraRegion, static_cast<unsigned>(facts.blend), facts.sortKey,
			static_cast<int>(imageName.size()), imageName.data(), static_cast<int>(facts.hasWaterTexture));

		Log() << text;
	}

	static Classification Classify(const MaterialFacts& facts)
	{
		const std::string_view techniqueSet = TechniqueSetName(facts.techniqueSet);
		const std::string_view name = StripComma(facts.name);
		std::string_view baseName = name;
		const std::size_t slash = name.rfind('/');

		if (slash != std::string_view::npos)
		{
			baseName = name.substr(slash + 1);
		}

		const int region = facts.cameraRegion;
		const int sortKey = facts.sortKey;
		const int blend = facts.blend;
		const bool isAdditiveOrScreen = blend == additiveBlend || blend == screenBlend;

		if (Contains(techniqueSet, "shadowcaster") || Contains(name, "shadow_caster") || EndsWith(name, "/shadowcaster"))
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (EndsWith(name, "_dummy") && blend == premultipliedBlend)
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (Contains(techniqueSet, "custom_growing_ice_cracks"))
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (techniqueSet == "2d" || techniqueSet == "grain_overlay" || techniqueSet == "splatter"
			|| techniqueSet == "splatter_alt")
		{
			return { static_cast<int>(TextureTag::Ui), false };
		}

		if (region == 4)
		{
			return { -1, false };
		}

		if (techniqueSet.substr(0, 10) == "distortion" || sortKey == 43)
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		const bool isSkyTechniqueSet = techniqueSet == "sky" || EndsWith(techniqueSet, "_sky");

		if (isSkyTechniqueSet || (sortKey == 2 && name.substr(0, 3) == "wc/"))
		{
			return { static_cast<int>(TextureTag::Sky), false };
		}

		if (name.substr(0, 13) == "wc/bg_facades" && Contains(techniqueSet, "unlit"))
		{
			return { static_cast<int>(TextureTag::Sky), false };
		}

		if (facts.hasWaterTexture)
		{
			return { static_cast<int>(TextureTag::Water), false, false, false, true };
		}

		if (region == 1 && Contains(techniqueSet, "_ua") && Contains(name, "water") && !Contains(name, "foam"))
		{
			return { static_cast<int>(TextureTag::Water), false, false, false, true };
		}

		if (region == 2 && StartsWithAny(baseName, { "headicon", "objpoint", "waypoint", "compass_objpoint", "killicon",
			"rank_", "cardicon", "faction_" }))
		{
			return { static_cast<int>(TextureTag::WorldSpaceUi), false };
		}

		if (techniqueSet == "unlit_blend_lin" || techniqueSet == "unlit_add_lin")
		{
			return { static_cast<int>(TextureTag::WorldSpaceUi), false };
		}

		if (techniqueSet == "mc_reflexsight" || techniqueSet == "mc_effect_replace" || (region == 2 && sortKey == 53))
		{
			return { static_cast<int>(TextureTag::WorldSpaceUi), false };
		}

		if (Contains(techniqueSet, "objective") || (region == 1 && sortKey == 29 && HasObjectiveSuffix(name)))
		{
			return { static_cast<int>(TextureTag::WorldSpaceUi), false };
		}

		const bool isViewmodelScreen = techniqueSet.substr(0, 9) == "mc_unlit_" && blend != replaceBlend && blend != -1
			&& ContainsAny(name, { "weapon", "uav" });

		if (isViewmodelScreen)
		{
			return { static_cast<int>(TextureTag::WorldSpaceUi), false };
		}

		if (techniqueSet == "post_light")
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		const bool isGlassPane = IsGlassName(name) && region == 1 && (blend == premultipliedBlend || blend == alphaBlend)
			&& facts.polygonOffset == 0 && sortKey >= lastLayerSortKey && !Contains(techniqueSet, "unlit")
			&& !Contains(techniqueSet, "_ua") && !Contains(name, "water") && name.substr(0, 14) != "mc/mtl_weapon_";

		if (isGlassPane)
		{
			return { -1, false, true };
		}

		const bool isWeaponLens = region == 1 && blend == premultipliedBlend && name.substr(0, 14) == "mc/mtl_weapon_"
			&& Contains(baseName, "lens");
		const bool isReflexLens = ContainsAny(name, { "reflex", "eotech" })
			|| ContainsAny(StripComma(facts.imageName), { "reflex", "eotech" });

		if (isWeaponLens && isReflexLens)
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (isWeaponLens)
		{
			return { -1, false, true };
		}

		if (region == 2 && (techniqueSet == "laser" || HasToken(baseName, { "tracer", "laser" })))
		{
			return { static_cast<int>(TextureTag::Particle), false };
		}

		// muzzle flashes, impacts and explosions often use glow/flare names and falloff techniques too, and ignoring
		// them is what made weapon effects vanish
		const bool isWeaponEffect = ContainsAny(name, { "muzzle", "muz_", "flash", "impact", "spark", "explosion",
			"explo", "blood", "bullet", "shell", "tracer", "fire", "flame", "smoke", "debris", "dust", "wind_" });
		const bool isGlowCard = ContainsAny(name, { "glow", "flare", "lensflare", "lenseflare", "corona", "halo",
			"sun_flare", "spotlight_beam", "floodlight", "lightglow", "light_godray", "godray_dim", "long_ray" })
			&& !isWeaponEffect;

		if (region == 2 && isGlowCard)
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (region == 2 && Contains(techniqueSet, "falloff") && isAdditiveOrScreen && !isWeaponEffect
			&& !Contains(name, "wind"))
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (region == 1 && isAdditiveOrScreen && Contains(name, "caustic"))
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		if (region == 1 && isAdditiveOrScreen
			&& (Contains(techniqueSet, "falloff") || ContainsAny(name, { "godray", "atmos_" })))
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		const bool isBackdrop = ContainsAny(name, { "bg_", "background", "distant", "vista", "horizon", "skybox",
			"mountain", "cloud" });
		const bool isVegetation = ContainsAny(name, { "tree", "pine", "foliage", "bush", "leaf", "leaves", "grass", "plant",
			"palm", "canopy", "cruciform", "fern", "ivy", "vine", "shrub", "branch" });
		const bool isSelfLit = (region == 0 || region == 2) && blend == replaceBlend && Contains(techniqueSet, "unlit")
			&& !isBackdrop && !isVegetation;

		if (isSelfLit)
		{
			return { -1, false, false, true };
		}

		if (region == 2)
		{
			if (blend == replaceBlend || techniqueSet.substr(0, 9) == "mc_effect")
			{
				return { -1, true };
			}

			return { static_cast<int>(TextureTag::Particle), false };
		}

		if (region == 1 && (sortKey == 13 || StartsWithAny(name, { "wc/gfx_", "mc/gfx_" })))
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		if (region == 1 && (facts.polygonOffset != 0 || blend == multiplyBlend))
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		if (region == 1 && Contains(techniqueSet, "_ua") && Contains(name, "foam") && isAdditiveOrScreen)
		{
			return { static_cast<int>(TextureTag::Ignore), false };
		}

		if (region == 1 && Contains(techniqueSet, "_ua") && Contains(name, "foam"))
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		const bool isModelLayerDecal = ContainsAny(baseName, { "_logo_", "decal" });
		const bool isDecalLayerName = EndsWith(baseName, "_dec") || Contains(baseName, "_dec_") || Contains(baseName, "decal");
		const bool isBlendedWorldLayer = (blend == premultipliedBlend || blend == alphaBlend) && facts.polygonOffset == 0
			&& !Contains(techniqueSet, "unlit") && !Contains(name, "glass");
		const bool isBlendedLayer = isBlendedWorldLayer
			&& (sortKey <= lastLayerSortKey || isModelLayerDecal || isDecalLayerName);

		if (region == 1 && isBlendedLayer)
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		if (region == 1 && isBlendedWorldLayer)
		{
			return { -1, false, false, false, false, true };
		}

		const bool isCutoutDecalName = Contains(baseName, "decal") || EndsWith(baseName, "_dec")
			|| Contains(baseName, "_dec_") || ContainsAny(baseName, { "paper", "poster", "streetline", "crosswalk", "rug" });
		const bool isCutoutDecal = blend == replaceBlend && facts.isAlphaTested && sortKey >= 6 && sortKey <= 16
			&& isCutoutDecalName;

		if (region == 1 && isCutoutDecal)
		{
			return { static_cast<int>(TextureTag::Decal), false };
		}

		return { -1, true };
	}

	static int ResolveTag(const TextureUse& use)
	{
		if (use.isCodeImage)
		{
			return -1;
		}

		const std::uint32_t decalBit = 1u << static_cast<int>(TextureTag::Decal);

		if (use.hasGeometryUser)
		{
			return -1;
		}

		if (use.tagMask == 0)
		{
			return -1;
		}

		const std::uint32_t uiBit = 1u << static_cast<int>(TextureTag::Ui);
		const std::uint32_t ignoreBit = 1u << static_cast<int>(TextureTag::Ignore);
		std::uint32_t mask = use.tagMask;

		// a texture is screen-space UI only when every material using it is 2D: Remix rasterises UI textures on top of
		// the path-traced frame, so an effect or rain sprite sharing a HUD texture would draw through walls
		if ((mask & ~(uiBit | ignoreBit)) != 0 || use.hasNonUiUser)
		{
			mask &= ~uiBit;
		}

		if ((mask & ~ignoreBit) != 0)
		{
			mask &= ~ignoreBit;
		}

		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			if (mask & (1u << tag))
			{
				return tag;
			}
		}

		return -1;
	}

	static std::string_view TrimSpaces(std::string_view text)
	{
		const std::size_t first = text.find_first_not_of(" \t\r\n");

		if (first == std::string_view::npos)
		{
			return {};
		}

		const std::size_t last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	static void ReadTagLine(std::string_view line, std::unordered_set<std::uint64_t>* sets)
	{
		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			const std::string_view option = tagOptions[tag];

			if (line.substr(0, option.size()) != option)
			{
				continue;
			}

			const std::size_t equals = line.find('=', option.size());

			if (equals == std::string_view::npos || !TrimSpaces(line.substr(option.size(), equals - option.size())).empty())
			{
				continue;
			}

			std::size_t start = equals + 1;

			while (start < line.size())
			{
				std::size_t comma = line.find(',', start);

				if (comma == std::string_view::npos)
				{
					comma = line.size();
				}

				const std::string entry(TrimSpaces(line.substr(start, comma - start)));

				if (!entry.empty() && entry[0] != '-')
				{
					sets[tag].insert(std::strtoull(entry.c_str(), nullptr, 16));
				}

				start = comma + 1;
			}

			return;
		}
	}

	static void ReadTagFile(const wchar_t* fileName, std::unordered_set<std::uint64_t>* sets)
	{
		std::ifstream file(Core::ModuleDirectory() + fileName);

		if (!file)
		{
			return;
		}

		std::string line;

		while (std::getline(file, line))
		{
			ReadTagLine(line, sets);
		}
	}

	static void RestoreTags(const remixapi_Interface* api)
	{
		hasReadSaved = true;

		ReadTagFile(L"user.conf", saved);

		std::unordered_set<std::uint64_t> cached[textureTagCount];
		ReadTagFile(tagFileName, cached);

		int restored = 0;

		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			std::string list;

			for (const std::uint64_t hash : cached[tag])
			{
				saved[tag].insert(hash);

				if (!pushed[tag].insert(hash).second)
				{
					continue;
				}

				char text[24];
				std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(hash));

				if (!list.empty())
				{
					list += ", ";
				}

				list += text;
				++restored;
			}

			if (!list.empty())
			{
				api->SetConfigVariable(tagOptions[tag], list.c_str());
				Device::NoteFrameEvent(std::string("texture tags restored ") + tagOptions[tag]);
			}

			stats.tagged[tag] = static_cast<int>(pushed[tag].size());
		}

		stats.restored = restored;

		if (restored > 0)
		{
			Log() << "remix: restored " << restored << " texture tags from earlier sessions, Remix has them before the"
					 " first frame of the match";
		}
	}

	static bool TryParseHash(std::string_view text, std::uint64_t& hash)
	{
		const std::string trimmed(TrimSpaces(text));

		if (trimmed.size() < 3 || trimmed[0] != '0' || (trimmed[1] != 'x' && trimmed[1] != 'X'))
		{
			return false;
		}

		hash = std::strtoull(trimmed.c_str(), nullptr, 16);
		return hash != 0;
	}

	static int TagOfOption(std::string_view option)
	{
		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			if (option == tagOptions[tag])
			{
				return tag;
			}
		}

		return -1;
	}

	static void ReadWithdrawnTags(std::unordered_set<std::uint64_t>* sets)
	{
		std::ifstream file(Core::ModuleDirectory() + tagFileName);
		std::string line;
		const std::string_view prefix = "# withdrawn from ";

		while (std::getline(file, line))
		{
			const std::string_view text(line);

			if (text.substr(0, prefix.size()) != prefix)
			{
				continue;
			}

			const std::size_t colon = text.find(':', prefix.size());

			if (colon == std::string_view::npos)
			{
				continue;
			}

			const int tag = TagOfOption(text.substr(prefix.size(), colon - prefix.size()));
			const std::string_view rest = TrimSpaces(text.substr(colon + 1));
			std::uint64_t hash = 0;

			if (tag >= 0 && TryParseHash(rest.substr(0, rest.find(' ')), hash))
			{
				sets[tag].insert(hash);
			}
		}
	}

	static bool IsInAnySet(const std::unordered_set<std::uint64_t>* sets, std::uint64_t hash)
	{
		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			if (sets[tag].count(hash) != 0)
			{
				return true;
			}
		}

		return false;
	}

	void SanitizeUserConf()
	{
		std::unordered_set<std::uint64_t> withdrawnSets[textureTagCount];
		std::unordered_set<std::uint64_t> cachedSets[textureTagCount];
		std::unordered_set<std::uint64_t> untaggedByRules;
		ReadWithdrawnTags(withdrawnSets);
		ReadTagFile(tagFileName, cachedSets);
		CollectUntaggedPbrHashes(untaggedByRules);

		const std::wstring path = Core::ModuleDirectory() + L"user.conf";
		std::ifstream input(path);

		if (!input)
		{
			return;
		}

		std::vector<std::string> lines;
		std::string line;
		int dropped = 0;

		while (std::getline(input, line))
		{
			const std::size_t equals = line.find('=');
			int tag = -1;

			if (equals != std::string::npos)
			{
				tag = TagOfOption(TrimSpaces(std::string_view(line).substr(0, equals)));
			}

			if (tag < 0)
			{
				lines.push_back(line);
				continue;
			}

			std::string kept;
			std::size_t start = equals + 1;

			while (start <= line.size())
			{
				std::size_t comma = line.find(',', start);

				if (comma == std::string::npos)
				{
					comma = line.size();
				}

				const std::string_view entry = TrimSpaces(std::string_view(line).substr(start, comma - start));
				start = comma + 1;

				if (entry.empty())
				{
					continue;
				}

				const bool isNegative = entry[0] == '-';
				std::uint64_t hash = 0;
				std::string_view hashText = entry;

				if (isNegative)
				{
					hashText = entry.substr(1);
				}

				const bool hasHash = TryParseHash(hashText, hash);
				const bool isWithdrawn = hasHash && withdrawnSets[tag].count(hash) != 0;
				const bool contradictsCache = hasHash && isNegative && cachedSets[tag].count(hash) != 0;
				const bool contradictsRules = hasHash && !isNegative && cachedSets[tag].count(hash) == 0
					&& (untaggedByRules.count(hash) != 0 || IsInAnySet(cachedSets, hash));

				if (isWithdrawn || contradictsCache || contradictsRules)
				{
					++dropped;
					continue;
				}

				if (!kept.empty())
				{
					kept += ", ";
				}

				kept += std::string(entry);
			}

			lines.push_back(std::string(tagOptions[tag]) + " = " + kept);
		}

		input.close();

		if (dropped == 0)
		{
			return;
		}

		if (!CopyFileW(path.c_str(), (path + L".zw3.bak").c_str(), FALSE))
		{
			return;
		}

		std::string output;

		for (const std::string& kept : lines)
		{
			output += kept + "\n";
		}

		if (!TryWriteFileAtomically(path, output))
		{
			return;
		}

		Log() << "remix: removed " << dropped << " stale texture tag opinions from user.conf (tags our rules withdrew,"
				 " removals that contradict our saved tags, and tags on textures our rules now treat differently, such as"
				 " glowing surfaces and glass), backup in user.conf.zw3.bak";
	}

	static void WriteTagFile()
	{
		std::ostringstream file;

		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			file << tagOptions[tag] << " = ";

			bool isFirst = true;

			for (const std::uint64_t hash : pushed[tag])
			{
				char text[24];
				std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(hash));

				if (!isFirst)
				{
					file << ", ";
				}

				file << text;
				isFirst = false;
			}

			file << "\n";
		}

		for (const WithdrawnTag& entry : withdrawnLog)
		{
			char text[24];
			std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(entry.hash));

			file << "# withdrawn from " << tagOptions[entry.option] << ": " << text << " " << entry.imageName << "\n";
		}

		TryWriteFileAtomically(Core::ModuleDirectory() + tagFileName, file.str());
	}

	// which images and materials each tag came from, so a texture drawn in the wrong place can be traced to its rule
	static void WriteTagReport(const std::unordered_map<std::uint64_t, TextureUse>& uses)
	{
		std::ostringstream file;
		file << "# tag\thash\timage\tusers\tmaterials (technique set, camera region)\n";

		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			for (const auto& entry : uses)
			{
				if (ResolveTag(entry.second) != tag)
				{
					continue;
				}

				char text[24];
				std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(entry.first));
				file << tagOptions[tag] << "\t" << text << "\t" << entry.second.imageName << "\t"
					 << entry.second.materialCount << "\t" << entry.second.materialNames << "\n";
			}
		}

		TryWriteFileAtomically(Core::ModuleDirectory() + tagReportFileName, file.str());
	}

	void UpdateTextureTags(IDirect3DDevice9* device)
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.materialPool == 0)
		{
			return;
		}

		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		if (!hasReadSaved)
		{
			RestoreTags(api);
		}

		const std::uintptr_t mapIdentity = GetMapIdentity();

		if (!mapIdentity || mapIdentity != settledWorld)
		{
			settledWorld = mapIdentity;
			settledFrames = 0;
			return;
		}

		++settledFrames;

		if (taggedWorld != mapIdentity)
		{
			if (settledFrames < firstPassDelay)
			{
				return;
			}
		}
		else
		{
			unsigned interval = passInterval;

			if (settledFrames < earlyPassFrames)
			{
				interval = earlyPassInterval;
			}

			if ((settledFrames % interval) != 0)
			{
				return;
			}
		}

		const bool isRepeatPass = taggedWorld == mapIdentity;
		taggedWorld = mapIdentity;

		const Engine::MaterialPool materialPool = Engine::CurrentMaterialPool();
		const std::uintptr_t pool = materialPool.pool;
		const std::uintptr_t entries = materialPool.entries;
		const std::uintptr_t poolEnd = entries + materialPool.count * materialPool.stride;

		// zw3 raises the material pool to 16384 entries, so a full pass is four times iw4dlss5's cost; a repeat pass
		// over the same materials, with every texture already hashed and the same switches, finds nothing new
		const std::uint64_t fingerprint = MaterialPoolFingerprint(materialPool);

		if (isRepeatPass && fingerprint == lastPassFingerprint && stats.texturesWithoutHash == 0)
		{
			++stats.skippedPasses;
			return;
		}

		lastPassFingerprint = fingerprint;

		std::unordered_map<std::uint64_t, TextureUse> uses;
		int materials = 0;
		int texturesWithoutHash = 0;

		for (std::size_t i = 0; i < materialPool.count; ++i)
		{
			MaterialFacts facts{};

			if (!TryReadMaterial(entries + i * materialPool.stride, pool, poolEnd, facts))
			{
				continue;
			}

			++materials;

			if (!facts.hasTexture)
			{
				continue;
			}

			if (facts.textureHash == 0)
			{
				++texturesWithoutHash;
				continue;
			}

			ReportDiagnosticMaterial(facts);

			if (facts.isForcedCutout)
			{
				Log() << "remix: " << facts.name << " is alpha tested at half so the holes in its cloth are cut out";
			}

			const Classification classification = Classify(facts);
			TextureUse& use = uses[facts.textureHash];

			if (StripComma(facts.imageName).substr(0, 1) == "$")
			{
				use.isCodeImage = true;
			}

			if (use.imageName.empty())
			{
				use.imageName = StripComma(facts.imageName);
			}

			if (classification.isGeometry)
			{
				use.hasGeometryUser = true;
			}

			if (classification.isGlass)
			{
				use.hasGlassUser = true;
			}

			if (!classification.isSelfLit && !use.fixtureTexture && IsFixtureMaterial(facts))
			{
				use.fixtureTexture = facts.colourTexture;
			}

			if (classification.isSelfLit && IsLampName(StripComma(facts.name)))
			{
				use.isLampLight = true;
			}

			if (classification.isSelfLit && use.selfLitMaterial.empty())
			{
				use.selfLitMaterial = StripComma(facts.name);
				use.selfLitTexture = facts.colourTexture;
			}

			if (classification.isBlendedLayer)
			{
				use.hasBlendedLayerUser = true;
			}

			if (IsLeafMaterial(facts))
			{
				use.hasLeafUser = true;

				if (!use.leafTexture)
				{
					use.leafTexture = facts.colourTexture;
				}

				if (!IsVegetationName(StripComma(facts.name)))
				{
					use.hasUnnamedLeafUser = true;
				}
			}
			else
			{
				use.hasNonLeafUser = true;
			}

			if (IsSkinColourMap(StripComma(facts.imageName)))
			{
				use.hasSkinUser = true;
			}
			else
			{
				use.hasNonSkinUser = true;
			}

			if (classification.isWater)
			{
				use.hasWaterUser = true;

				if (!use.waterTexture)
				{
					use.waterTexture = facts.waterTexture;
				}
			}

			if (classification.tag >= 0)
			{
				use.tagMask |= 1u << classification.tag;
			}

			if (classification.tag != static_cast<int>(TextureTag::Ui))
			{
				use.hasNonUiUser = true;
			}

			if (use.materialCount < namesPerTexture)
			{
				use.materialNames += (use.materialCount ? ", " : "") + std::string(StripComma(facts.name)) + " ("
					+ std::string(TechniqueSetName(facts.techniqueSet)) + " r" + std::to_string(facts.cameraRegion) + ")";
			}

			++use.materialCount;
		}

		std::string additions[textureTagCount];
		std::string removals[textureTagCount];
		int addedTotal = 0;
		int removedTotal = 0;

		const std::uint32_t decalBit = 1u << static_cast<int>(TextureTag::Decal);

		for (const auto& entry : uses)
		{
			const int tag = ResolveTag(entry.second);
			const bool isSharedLayer = entry.second.hasGeometryUser && !entry.second.isCodeImage
				&& ((entry.second.tagMask & decalBit) || entry.second.hasBlendedLayerUser);

			if (isSharedLayer)
			{
				sharedLayerTextures.insert(entry.first);
			}

			for (int option = 0; option < textureTagCount; ++option)
			{
				if (option == tag || saved[option].count(entry.first) == 0 || !withdrawn[option].insert(entry.first).second)
				{
					continue;
				}

				char removal[24];
				std::snprintf(removal, sizeof(removal), "-0x%016llX", static_cast<unsigned long long>(entry.first));

				if (!removals[option].empty())
				{
					removals[option] += ", ";
				}

				removals[option] += removal;
				pushed[option].erase(entry.first);
				withdrawnLog.push_back({ option, entry.first, entry.second.imageName });
				++removedTotal;
			}

			if (tag < 0 && entry.second.hasGlassUser && !entry.second.hasGeometryUser && !entry.second.isCodeImage)
			{
				RegisterPbrGlass(entry.first);
			}

			if (tag < 0 && entry.second.hasLeafUser && !entry.second.hasNonLeafUser && !entry.second.isCodeImage)
			{
				RegisterPbrSurfaceKind(entry.first, surfaceKindFoliage);

				if (!entry.second.hasUnnamedLeafUser)
				{
					RegisterPbrFoliageMask(entry.first, Device::Unwrap(static_cast<IDirect3DTexture9*>(entry.second.leafTexture)));
				}
			}

			if (tag < 0 && entry.second.hasSkinUser && !entry.second.hasNonSkinUser && !entry.second.isCodeImage)
			{
				RegisterPbrSurfaceKind(entry.first, surfaceKindSkin);
			}

			if (tag == static_cast<int>(TextureTag::Water) && entry.second.hasWaterUser && !entry.second.isCodeImage)
			{
				RegisterPbrWater(entry.first, static_cast<IDirect3DTexture9*>(entry.second.waterTexture));
			}

			if (tag < 0 && entry.second.isLampLight && !entry.second.isCodeImage && lampLightHashes.insert(entry.first).second)
			{
				++lampLightVersion;
			}

			if (tag < 0 && entry.second.fixtureTexture && entry.second.selfLitMaterial.empty() && !entry.second.isCodeImage
				&& GetLookSettings().isFixtureLights)
			{
				RegisterPbrFixture(entry.first, Device::Unwrap(static_cast<IDirect3DTexture9*>(entry.second.fixtureTexture)));
			}

			const bool isSelfLitTexture = tag < 0 && !entry.second.selfLitMaterial.empty() && !entry.second.hasGeometryUser
				&& !entry.second.isCodeImage;

			if (isSelfLitTexture && selfLitSeen.insert(entry.first).second)
			{
				++stats.selfLit;
				Log() << "remix: " << entry.second.selfLitMaterial << " (" << entry.second.imageName << ") is self-lit, it"
						 " glows in Remix from its own texture";
				RegisterPbrEmissive(entry.first, Device::Unwrap(static_cast<IDirect3DTexture9*>(entry.second.selfLitTexture)),
					SelfLitIntensity(entry.second.selfLitMaterial));
			}

			if (tag < 0 || !pushed[tag].insert(entry.first).second)
			{
				continue;
			}

			char text[24];
			std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(entry.first));

			if (!additions[tag].empty())
			{
				additions[tag] += ", ";
			}

			additions[tag] += text;
			++addedTotal;
		}

		for (int tag = 0; tag < textureTagCount; ++tag)
		{
			if (!removals[tag].empty())
			{
				api->SetConfigVariable(tagOptions[tag], removals[tag].c_str());
				Device::NoteFrameEvent(std::string("texture tags removed ") + tagOptions[tag]);
			}

			if (!additions[tag].empty())
			{
				api->SetConfigVariable(tagOptions[tag], additions[tag].c_str());
				Device::NoteFrameEvent(std::string("texture tags added ") + tagOptions[tag]);
			}

			stats.tagged[tag] = static_cast<int>(pushed[tag].size());
		}

		++stats.passes;
		stats.materials = materials;
		stats.texturesWithoutHash = texturesWithoutHash;
		stats.withdrawn = static_cast<int>(withdrawnLog.size());

		if (removedTotal > 0)
		{
			Log() << "remix: withdrew " << removedTotal << " tags saved by earlier sessions or rules that the current"
					 " rules disagree with, each one is listed at the end of zw3_remix_textures.conf";
		}

		if (!isRepeatPass || addedTotal > 0 || removedTotal > 0)
		{
			WriteTagReport(uses);
		}

		if (addedTotal > 0 || removedTotal > 0)
		{
			WriteTagFile();

			Log() << "remix: tagged " << addedTotal << " more textures from " << materials << " materials (sky "
				  << stats.tagged[0] << ", world UI " << stats.tagged[1] << ", beam " << stats.tagged[2] << ", decal "
				  << stats.tagged[3] << ", particle " << stats.tagged[4] << ", water " << stats.tagged[5] << ", UI "
				  << stats.tagged[6] << ", ignore " << stats.tagged[7] << "), lists in zw3_remix_textures.conf";
		}
	}

	void ResetTextureTags()
	{
		settledWorld = 0;
		taggedWorld = 0;
		settledFrames = 0;
	}

	TextureTagStats GetTextureTagStats()
	{
		return stats;
	}

	void SetSharedDecalTags(bool isEnabled)
	{
		isSharedDecalTagged = isEnabled;
	}

	bool IsSharedDecalTags()
	{
		return isSharedDecalTagged;
	}

	bool IsSharedLayerTexture(std::uint64_t hash)
	{
		return isSharedDecalTagged && sharedLayerTextures.count(hash) != 0;
	}

	bool IsDecalTexture(std::uint64_t hash)
	{
		return pushed[static_cast<int>(TextureTag::Decal)].count(hash) != 0;
	}

	void ExcludeFromOpacityMicromaps(IDirect3DDevice9* device, std::uint64_t hash)
	{
		static std::unordered_set<std::uint64_t> excluded;
		const remixapi_Interface* api = TryGetApi(device);

		if (!api || !excluded.insert(hash).second)
		{
			return;
		}

		char text[24];
		std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(hash));
		api->SetConfigVariable("rtx.opacityMicromapIgnoreTextures", text);
	}

	void WithdrawGeneratedTag(IDirect3DDevice9* device, std::uint64_t hash, TextureTag tag)
	{
		const remixapi_Interface* api = TryGetApi(device);
		const int option = static_cast<int>(tag);

		if (!api || !withdrawn[option].insert(hash).second)
		{
			return;
		}

		char text[24];
		std::snprintf(text, sizeof(text), "-0x%016llX", static_cast<unsigned long long>(hash));
		api->SetConfigVariable(tagOptions[option], text);

		pushed[option].erase(hash);
		withdrawnLog.push_back({ option, hash, "generated" });
		stats.tagged[option] = static_cast<int>(pushed[option].size());
		stats.withdrawn = static_cast<int>(withdrawnLog.size());

		WriteTagFile();
	}

	const char* TextureTagOption(TextureTag tag)
	{
		return tagOptions[static_cast<int>(tag)];
	}

	void TagGeneratedTexture(IDirect3DDevice9* device, std::uint64_t hash, TextureTag tag)
	{
		const remixapi_Interface* api = TryGetApi(device);
		const int option = static_cast<int>(tag);

		if (!api || !pushed[option].insert(hash).second)
		{
			return;
		}

		char text[24];
		std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(hash));
		api->SetConfigVariable(tagOptions[option], text);

		stats.tagged[option] = static_cast<int>(pushed[option].size());
	}
}
