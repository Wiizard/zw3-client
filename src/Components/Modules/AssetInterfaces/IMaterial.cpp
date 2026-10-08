#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "IMaterial.hpp"
#include "../Command.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	constexpr int IW4X_MAT_VERSION = 1;
	constexpr char TS_WATER_MAP = 0xB;

	constexpr std::uint32_t GFXS0_SRCBLEND_RGB_SHIFT = 0x0;
	constexpr std::uint32_t GFXS0_SRCBLEND_RGB_MASK = 0xF;
	constexpr std::uint32_t GFXS0_DSTBLEND_RGB_SHIFT = 0x4;
	constexpr std::uint32_t GFXS0_DSTBLEND_RGB_MASK = 0xF0;
	constexpr std::uint32_t GFXS0_BLENDOP_RGB_SHIFT = 0x8;
	constexpr std::uint32_t GFXS0_BLENDOP_RGB_MASK = 0x700;
	constexpr std::uint32_t GFXS0_ATEST_DISABLE = 0x800;
	constexpr std::uint32_t GFXS0_ATEST_GT_0 = 0x1000;
	constexpr std::uint32_t GFXS0_ATEST_LT_128 = 0x2000;
	constexpr std::uint32_t GFXS0_ATEST_GE_128 = 0x3000;
	constexpr std::uint32_t GFXS0_ATEST_MASK = 0x3000;
	constexpr std::uint32_t GFXS0_CULL_NONE = 0x4000;
	constexpr std::uint32_t GFXS0_CULL_BACK = 0x8000;
	constexpr std::uint32_t GFXS0_CULL_FRONT = 0xC000;
	constexpr std::uint32_t GFXS0_CULL_MASK = 0xC000;
	constexpr std::uint32_t GFXS0_SRCBLEND_ALPHA_SHIFT = 0x10;
	constexpr std::uint32_t GFXS0_SRCBLEND_ALPHA_MASK = 0xF0000;
	constexpr std::uint32_t GFXS0_DSTBLEND_ALPHA_SHIFT = 0x14;
	constexpr std::uint32_t GFXS0_DSTBLEND_ALPHA_MASK = 0xF00000;
	constexpr std::uint32_t GFXS0_BLENDOP_ALPHA_SHIFT = 0x18;
	constexpr std::uint32_t GFXS0_BLENDOP_ALPHA_MASK = 0x7000000;
	constexpr std::uint32_t GFXS0_COLORWRITE_RGB = 0x8000000;
	constexpr std::uint32_t GFXS0_COLORWRITE_ALPHA = 0x10000000;
	constexpr std::uint32_t GFXS0_GAMMAWRITE = 0x40000000;
	constexpr std::uint32_t GFXS0_POLYMODE_LINE = 0x80000000;

	constexpr std::uint32_t GFXS1_DEPTHWRITE = 0x1;
	constexpr std::uint32_t GFXS1_DEPTHTEST_DISABLE = 0x2;
	constexpr std::uint32_t GFXS1_DEPTHTEST_SHIFT = 0x2;
	constexpr std::uint32_t GFXS1_DEPTHTEST_MASK = 0xC;
	constexpr std::uint32_t GFXS1_POLYGON_OFFSET_SHIFT = 0x4;
	constexpr std::uint32_t GFXS1_POLYGON_OFFSET_MASK = 0x30;
	constexpr std::uint32_t GFXS1_STENCIL_FRONT_ENABLE = 0x40;
	constexpr std::uint32_t GFXS1_STENCIL_BACK_ENABLE = 0x80;
	constexpr std::uint32_t GFXS1_STENCIL_FRONT_PASS_SHIFT = 0x8;
	constexpr std::uint32_t GFXS1_STENCIL_FRONT_FAIL_SHIFT = 0xB;
	constexpr std::uint32_t GFXS1_STENCIL_FRONT_ZFAIL_SHIFT = 0xE;
	constexpr std::uint32_t GFXS1_STENCIL_FRONT_FUNC_SHIFT = 0x11;
	constexpr std::uint32_t GFXS1_STENCIL_BACK_PASS_SHIFT = 0x14;
	constexpr std::uint32_t GFXS1_STENCIL_BACK_FAIL_SHIFT = 0x17;
	constexpr std::uint32_t GFXS1_STENCIL_BACK_ZFAIL_SHIFT = 0x1A;
	constexpr std::uint32_t GFXS1_STENCIL_BACK_FUNC_SHIFT = 0x1D;
	constexpr std::uint32_t GFXS_STENCILOP_MASK = 0x7;

	constexpr std::uint32_t depthTestDisabled = 0xFFFFFFFF;

	struct StateBitsJson
	{
		std::uint32_t srcBlendRgb;
		std::uint32_t dstBlendRgb;
		std::uint32_t blendOpRgb;
		std::uint32_t srcBlendAlpha;
		std::uint32_t dstBlendAlpha;
		std::uint32_t blendOpAlpha;
		std::int64_t depthTest;
		std::uint32_t polygonOffset;
		std::string alphaTest;
		std::string cullFace;
		bool colorWriteRgb;
		bool colorWriteAlpha;
		bool polymodeLine;
		bool gammaWrite;
		bool depthWrite;
		bool stencilFrontEnabled;
		bool stencilBackEnabled;
		std::uint32_t stencilFrontPass;
		std::uint32_t stencilFrontFail;
		std::uint32_t stencilFrontZFail;
		std::uint32_t stencilFrontFunc;
		std::uint32_t stencilBackPass;
		std::uint32_t stencilBackFail;
		std::uint32_t stencilBackZFail;
		std::uint32_t stencilBackFunc;
	};

	template <typename T>
	static bool TryRead(const rapidjson::Value& object, const char* member, T& out)
	{
		if (!object.IsObject())
		{
			return false;
		}

		const auto found = object.FindMember(member);

		if (found == object.MemberEnd())
		{
			return false;
		}

		const auto& value = found->value;

		if constexpr (std::is_same_v<T, bool>)
		{
			if (!value.IsBool())
			{
				return false;
			}

			out = value.GetBool();
		}
		else if constexpr (std::is_same_v<T, std::string>)
		{
			if (!value.IsString())
			{
				return false;
			}

			out.assign(value.GetString(), value.GetStringLength());
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			if (!value.IsNumber())
			{
				return false;
			}

			out = static_cast<T>(value.GetDouble());
		}
		else if (value.IsInt64())
		{
			out = static_cast<T>(value.GetInt64());
		}
		else if (value.IsUint64())
		{
			out = static_cast<T>(value.GetUint64());
		}
		else
		{
			return false;
		}

		return true;
	}

	static bool TryReadFloats(const rapidjson::Value& object, const char* member, float* out, rapidjson::SizeType count)
	{
		const auto found = object.FindMember(member);

		if (found == object.MemberEnd() || !found->value.IsArray() || found->value.Size() < count)
		{
			return false;
		}

		for (rapidjson::SizeType i = 0; i < count; ++i)
		{
			if (!found->value[i].IsNumber())
			{
				return false;
			}

			out[i] = static_cast<float>(found->value[i].GetDouble());
		}

		return true;
	}

	static bool TryReadFlags(const rapidjson::Value& object, const char* member, std::size_t size, unsigned long& flags)
	{
		std::string text;
		return TryRead(object, member, text) && Utils::JSON::TryReadFlags(text, size, flags);
	}

	static std::string EncodeBase64(const void* data, std::size_t size)
	{
		std::string encoded(4 * ((size + 2) / 3) + 1, '\0');
		auto encodedLength = static_cast<unsigned long>(encoded.size());

		if (base64_encode(static_cast<const unsigned char*>(data), static_cast<unsigned long>(size), encoded.data(), &encodedLength) != CRYPT_OK)
		{
			return {};
		}

		encoded.resize(encodedLength);
		return encoded;
	}

	static bool TryDecodeBase64(const std::string& text, std::string& decoded)
	{
		decoded.assign(text.size() / 4 * 3 + 3, '\0');
		auto decodedLength = static_cast<unsigned long>(decoded.size());

		if (base64_decode(text.data(), static_cast<unsigned long>(text.size()), reinterpret_cast<unsigned char*>(decoded.data()), &decodedLength) != CRYPT_OK)
		{
			return false;
		}

		decoded.resize(decodedLength);
		return true;
	}

	static Game::GfxImage* FindImage(const std::string& imageName, const std::string& materialName, Components::ZoneBuilder::Zone* builder)
	{
		auto* const image = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_IMAGE, imageName, builder).image;

		if (!image)
		{
			Components::Logger::Fatal("Could not find image {} for material {}\n", imageName, materialName);
		}

		return image;
	}

	static bool TryReadWater(const rapidjson::Value& json, Game::water_t* water, const std::string& materialName, Components::ZoneBuilder::Zone* builder)
	{
		std::string imageName;

		if (TryRead(json, "image", imageName))
		{
			water->image = FindImage(imageName, materialName, builder);
			water->image->semantic = TS_WATER_MAP;
		}

		std::string h0Text;
		std::string wTermText;

		const bool isValid = TryRead(json, "amplitude", water->amplitude)
			&& TryRead(json, "M", water->M)
			&& TryRead(json, "N", water->N)
			&& TryRead(json, "Lx", water->Lx)
			&& TryRead(json, "Lz", water->Lz)
			&& TryRead(json, "gravity", water->gravity)
			&& TryRead(json, "windvel", water->windvel)
			&& TryReadFloats(json, "winddir", water->winddir, 2)
			&& TryReadFloats(json, "codeConstant", water->codeConstant, 4)
			&& TryRead(json, "H0", h0Text)
			&& TryRead(json, "wTerm", wTermText);

		if (!isValid || water->M < 0 || water->N < 0)
		{
			return false;
		}

		const auto count = static_cast<std::size_t>(water->M) * static_cast<std::size_t>(water->N);
		std::string h0;
		std::string wTerm;

		if (!TryDecodeBase64(h0Text, h0) || h0.size() < count * sizeof(Game::complex_s))
		{
			return false;
		}

		if (!TryDecodeBase64(wTermText, wTerm) || wTerm.size() < count * sizeof(float))
		{
			return false;
		}

		auto* const allocator = builder->GetAllocator();
		water->H0Real = allocator->AllocateArray<float>(count);
		water->H0Imag = allocator->AllocateArray<float>(count);
		water->wTerm = allocator->AllocateArray<float>(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			Game::complex_s value{};
			std::memcpy(&value, h0.data() + i * sizeof(Game::complex_s), sizeof(value));
			water->H0Real[i] = value.real;
			water->H0Imag[i] = value.imag;
		}

		std::memcpy(water->wTerm, wTerm.data(), count * sizeof(float));
		return true;
	}

	static bool TryReadStateBits(const rapidjson::Value& json, Game::GfxStateBits* stateBits)
	{
		StateBitsJson entry{};

		const bool isValid = TryRead(json, "srcBlendRgb", entry.srcBlendRgb)
			&& TryRead(json, "dstBlendRgb", entry.dstBlendRgb)
			&& TryRead(json, "blendOpRgb", entry.blendOpRgb)
			&& TryRead(json, "srcBlendAlpha", entry.srcBlendAlpha)
			&& TryRead(json, "dstBlendAlpha", entry.dstBlendAlpha)
			&& TryRead(json, "blendOpAlpha", entry.blendOpAlpha)
			&& TryRead(json, "depthTest", entry.depthTest)
			&& TryRead(json, "polygonOffset", entry.polygonOffset)
			&& TryRead(json, "alphaTest", entry.alphaTest)
			&& TryRead(json, "cullFace", entry.cullFace)
			&& TryRead(json, "colorWriteRgb", entry.colorWriteRgb)
			&& TryRead(json, "colorWriteAlpha", entry.colorWriteAlpha)
			&& TryRead(json, "polymodeLine", entry.polymodeLine)
			&& TryRead(json, "gammaWrite", entry.gammaWrite)
			&& TryRead(json, "depthWrite", entry.depthWrite)
			&& TryRead(json, "stencilFrontEnabled", entry.stencilFrontEnabled)
			&& TryRead(json, "stencilBackEnabled", entry.stencilBackEnabled)
			&& TryRead(json, "stencilFrontPass", entry.stencilFrontPass)
			&& TryRead(json, "stencilFrontFail", entry.stencilFrontFail)
			&& TryRead(json, "stencilFrontZFail", entry.stencilFrontZFail)
			&& TryRead(json, "stencilFrontFunc", entry.stencilFrontFunc)
			&& TryRead(json, "stencilBackPass", entry.stencilBackPass)
			&& TryRead(json, "stencilBackFail", entry.stencilBackFail)
			&& TryRead(json, "stencilBackZFail", entry.stencilBackZFail)
			&& TryRead(json, "stencilBackFunc", entry.stencilBackFunc);

		if (!isValid)
		{
			return false;
		}

		std::uint32_t loadBits0 = 0;
		std::uint32_t loadBits1 = 0;

		loadBits0 |= entry.srcBlendRgb << GFXS0_SRCBLEND_RGB_SHIFT;
		loadBits0 |= entry.dstBlendRgb << GFXS0_DSTBLEND_RGB_SHIFT;
		loadBits0 |= entry.blendOpRgb << GFXS0_BLENDOP_RGB_SHIFT;
		loadBits0 |= entry.srcBlendAlpha << GFXS0_SRCBLEND_ALPHA_SHIFT;
		loadBits0 |= entry.dstBlendAlpha << GFXS0_DSTBLEND_ALPHA_SHIFT;
		loadBits0 |= entry.blendOpAlpha << GFXS0_BLENDOP_ALPHA_SHIFT;

		if (entry.depthTest == -1 || entry.depthTest == depthTestDisabled)
		{
			loadBits1 |= GFXS1_DEPTHTEST_DISABLE;
		}
		else
		{
			loadBits1 |= static_cast<std::uint32_t>(entry.depthTest) << GFXS1_DEPTHTEST_SHIFT;
		}

		loadBits1 |= entry.polygonOffset << GFXS1_POLYGON_OFFSET_SHIFT;

		if (entry.alphaTest == "disable")
		{
			loadBits0 |= GFXS0_ATEST_DISABLE;
		}
		else if (entry.alphaTest == ">0")
		{
			loadBits0 |= GFXS0_ATEST_GT_0;
		}
		else if (entry.alphaTest == "<128")
		{
			loadBits0 |= GFXS0_ATEST_LT_128;
		}
		else if (entry.alphaTest == ">=128")
		{
			loadBits0 |= GFXS0_ATEST_GE_128;
		}
		else
		{
			return false;
		}

		if (entry.cullFace == "none")
		{
			loadBits0 |= GFXS0_CULL_NONE;
		}
		else if (entry.cullFace == "back")
		{
			loadBits0 |= GFXS0_CULL_BACK;
		}
		else if (entry.cullFace == "front")
		{
			loadBits0 |= GFXS0_CULL_FRONT;
		}
		else
		{
			return false;
		}

		if (entry.gammaWrite)
		{
			loadBits0 |= GFXS0_GAMMAWRITE;
		}

		if (entry.colorWriteAlpha)
		{
			loadBits0 |= GFXS0_COLORWRITE_ALPHA;
		}

		if (entry.colorWriteRgb)
		{
			loadBits0 |= GFXS0_COLORWRITE_RGB;
		}

		if (entry.polymodeLine)
		{
			loadBits0 |= GFXS0_POLYMODE_LINE;
		}

		if (entry.depthWrite)
		{
			loadBits1 |= GFXS1_DEPTHWRITE;
		}

		if (entry.stencilFrontEnabled)
		{
			loadBits1 |= GFXS1_STENCIL_FRONT_ENABLE;
		}

		if (entry.stencilBackEnabled)
		{
			loadBits1 |= GFXS1_STENCIL_BACK_ENABLE;
		}

		loadBits1 |= entry.stencilFrontPass << GFXS1_STENCIL_FRONT_PASS_SHIFT;
		loadBits1 |= entry.stencilFrontFail << GFXS1_STENCIL_FRONT_FAIL_SHIFT;
		loadBits1 |= entry.stencilFrontZFail << GFXS1_STENCIL_FRONT_ZFAIL_SHIFT;
		loadBits1 |= entry.stencilFrontFunc << GFXS1_STENCIL_FRONT_FUNC_SHIFT;
		loadBits1 |= entry.stencilBackPass << GFXS1_STENCIL_BACK_PASS_SHIFT;
		loadBits1 |= entry.stencilBackFail << GFXS1_STENCIL_BACK_FAIL_SHIFT;
		loadBits1 |= entry.stencilBackZFail << GFXS1_STENCIL_BACK_ZFAIL_SHIFT;
		loadBits1 |= entry.stencilBackFunc << GFXS1_STENCIL_BACK_FUNC_SHIFT;

		stateBits->loadBits[0] = loadBits0;
		stateBits->loadBits[1] = loadBits1;
		return true;
	}

	static Game::Material* ReadMaterial(const std::string& name, const std::string& contents, Components::ZoneBuilder::Zone* builder)
	{
		rapidjson::Document materialJson;
		materialJson.Parse<rapidjson::kParseNanAndInfFlag>(contents.data(), contents.size());

		if (materialJson.HasParseError() || !materialJson.IsObject())
		{
			Components::Logger::Fatal("Invalid material json for {} (Is it zonebuilder format?)\n", name);
		}

		int version = 0;

		if (!TryRead(materialJson, "version", version) || version != IW4X_MAT_VERSION)
		{
			Components::Logger::Fatal("Invalid material json version for {}, expected {} and got {}\n", name, IW4X_MAT_VERSION, version);
		}

		auto* const allocator = builder->GetAllocator();
		auto* const asset = allocator->Allocate<Game::Material>();

		std::string materialName;
		unsigned long gameFlags = 0;
		unsigned long surfaceTypeBits = 0;
		unsigned long stateFlags = 0;

		const bool isValid = TryRead(materialJson, "name", materialName)
			&& TryReadFlags(materialJson, "gameFlags", sizeof(char), gameFlags)
			&& TryRead(materialJson, "sortKey", asset->info.sortKey)
			&& TryRead(materialJson, "textureAtlasRowCount", asset->info.textureAtlasRowCount)
			&& TryRead(materialJson, "textureAtlasColumnCount", asset->info.textureAtlasColumnCount)
			&& TryReadFlags(materialJson, "surfaceTypeBits", sizeof(int), surfaceTypeBits)
			&& TryRead(materialJson, "hashIndex", asset->info.hashIndex)
			&& TryRead(materialJson, "cameraRegion", asset->cameraRegion)
			&& TryReadFlags(materialJson, "stateFlags", sizeof(char), stateFlags);

		if (!isValid)
		{
			Components::Logger::Fatal("Invalid material json for {} (broken json)\n", name);
		}

		asset->info.name = allocator->DuplicateString(materialName);
		asset->info.gameFlags = static_cast<unsigned char>(gameFlags);
		asset->info.surfaceTypeBits = static_cast<unsigned int>(surfaceTypeBits);
		asset->stateFlags = static_cast<unsigned char>(stateFlags);

		const auto drawSurface = materialJson.FindMember("gfxDrawSurface");

		if (drawSurface != materialJson.MemberEnd() && drawSurface->value.IsObject())
		{
			const auto& json = drawSurface->value;
			auto& fields = asset->info.drawSurf.fields;
			std::uint64_t values[11]{};

			const bool isDrawSurfaceValid = TryRead(json, "customIndex", values[0])
				&& TryRead(json, "hasGfxEntIndex", values[1])
				&& TryRead(json, "materialSortedIndex", values[2])
				&& TryRead(json, "objectId", values[3])
				&& TryRead(json, "prepass", values[4])
				&& TryRead(json, "primarySortKey", values[5])
				&& TryRead(json, "reflectionProbeIndex", values[6])
				&& TryRead(json, "sceneLightIndex", values[7])
				&& TryRead(json, "surfType", values[8])
				&& TryRead(json, "unused", values[9])
				&& TryRead(json, "useHeroLighting", values[10]);

			if (!isDrawSurfaceValid)
			{
				Components::Logger::Fatal("Invalid material json for {} (broken gfxDrawSurface)\n", name);
			}

			fields.customIndex = values[0];
			fields.hasGfxEntIndex = values[1];
			fields.materialSortedIndex = values[2];
			fields.objectId = values[3];
			fields.prepass = values[4];
			fields.primarySortKey = values[5];
			fields.reflectionProbeIndex = values[6];
			fields.sceneLightIndex = values[7];
			fields.surfType = values[8];
			fields.unused = values[9];
			fields.useHeroLighting = values[10];
		}

		const auto textureTable = materialJson.FindMember("textureTable");

		if (textureTable != materialJson.MemberEnd() && textureTable->value.IsArray())
		{
			const auto& textures = textureTable->value;
			asset->textureCount = static_cast<unsigned char>(textures.Size());
			asset->textureTable = allocator->AllocateArray<Game::MaterialTextureDef>(asset->textureCount);

			for (rapidjson::SizeType i = 0; i < asset->textureCount; ++i)
			{
				const auto& textureJson = textures[i];

				if (!textureJson.IsObject())
				{
					continue;
				}

				auto* const textureDef = &asset->textureTable[i];

				const bool isTextureValid = TryRead(textureJson, "semantic", textureDef->semantic)
					&& TryRead(textureJson, "samplerState", textureDef->samplerState)
					&& TryRead(textureJson, "nameStart", textureDef->nameStart)
					&& TryRead(textureJson, "nameEnd", textureDef->nameEnd)
					&& TryRead(textureJson, "nameHash", textureDef->nameHash);

				if (!isTextureValid)
				{
					Components::Logger::Fatal("Invalid material json for {} (broken texture {})\n", name, i);
				}

				if (textureDef->semantic == TS_WATER_MAP)
				{
					auto* const water = allocator->Allocate<Game::water_t>();
					const auto waterJson = textureJson.FindMember("water");

					if (waterJson != textureJson.MemberEnd() && waterJson->value.IsObject() && !TryReadWater(waterJson->value, water, name, builder))
					{
						Components::Logger::Fatal("Invalid material json for {} (broken water in texture {})\n", name, i);
					}

					textureDef->u.water = water;
					continue;
				}

				std::string imageName;

				if (TryRead(textureJson, "image", imageName))
				{
					textureDef->u.image = FindImage(imageName, name, builder);
					textureDef->u.image->semantic = static_cast<unsigned char>(textureDef->semantic);
				}
			}
		}

		const auto stateBitsEntry = materialJson.FindMember("stateBitsEntry");

		if (stateBitsEntry != materialJson.MemberEnd() && stateBitsEntry->value.IsArray())
		{
			const auto count = std::min(stateBitsEntry->value.Size(), static_cast<rapidjson::SizeType>(std::size(asset->stateBitsEntry)));

			for (rapidjson::SizeType i = 0; i < count; ++i)
			{
				if (stateBitsEntry->value[i].IsInt())
				{
					asset->stateBitsEntry[i] = static_cast<unsigned char>(stateBitsEntry->value[i].GetInt());
				}
			}
		}

		const auto stateBitsTable = materialJson.FindMember("stateBitsTable");

		if (stateBitsTable == materialJson.MemberEnd() || !stateBitsTable->value.IsArray())
		{
			return nullptr;
		}

		asset->stateBitsCount = static_cast<unsigned char>(stateBitsTable->value.Size());
		asset->stateBitsTable = allocator->AllocateArray<Game::GfxStateBits>(asset->stateBitsCount);

		for (rapidjson::SizeType i = 0; i < asset->stateBitsCount; ++i)
		{
			if (!TryReadStateBits(stateBitsTable->value[i], &asset->stateBitsTable[i]))
			{
				Components::Logger::Fatal("Invalid statebits {} in material {}\n", i, name);
			}
		}

		const auto constantTable = materialJson.FindMember("constantTable");

		if (constantTable != materialJson.MemberEnd() && constantTable->value.IsArray())
		{
			asset->constantCount = static_cast<unsigned char>(constantTable->value.Size());
			auto* const table = allocator->AllocateArray<Game::MaterialConstantDef>(asset->constantCount);

			for (rapidjson::SizeType i = 0; i < asset->constantCount; ++i)
			{
				const auto& constant = constantTable->value[i];
				auto* const entry = &table[i];
				std::string constantName;

				if (!TryReadFloats(constant, "literal", entry->literal, 4) || !TryRead(constant, "name", constantName) || !TryRead(constant, "nameHash", entry->nameHash))
				{
					Components::Logger::Fatal("Invalid constant {} in material {}\n", i, name);
				}

				std::memcpy(entry->name, constantName.data(), std::min(constantName.size(), sizeof(entry->name)));
			}

			asset->constantTable = table;
		}

		std::string techsetName;

		if (TryRead(materialJson, "techniqueSet", techsetName))
		{
			asset->techniqueSet = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_TECHNIQUE_SET, techsetName, builder).techniqueSet;

			if (!asset->techniqueSet)
			{
				Components::Logger::Fatal("Could not find technique set {} for material {}", techsetName, name);
			}
		}

		return asset;
	}

	static rapidjson::Value StateBitsToJson(const Game::GfxStateBits& entry, Utils::JSON::Allocator& allocator)
	{
		const auto loadBits0 = entry.loadBits[0];
		const auto loadBits1 = entry.loadBits[1];

		std::uint32_t depthTest = depthTestDisabled;

		if (!(loadBits1 & GFXS1_DEPTHTEST_DISABLE))
		{
			depthTest = (loadBits1 & GFXS1_DEPTHTEST_MASK) >> GFXS1_DEPTHTEST_SHIFT;
		}

		const char* alphaTest = "disable";

		if ((loadBits0 & GFXS0_ATEST_MASK) == GFXS0_ATEST_GE_128)
		{
			alphaTest = ">=128";
		}
		else if ((loadBits0 & GFXS0_ATEST_MASK) == GFXS0_ATEST_GT_0)
		{
			alphaTest = ">0";
		}
		else if ((loadBits0 & GFXS0_ATEST_MASK) == GFXS0_ATEST_LT_128)
		{
			alphaTest = "<128";
		}

		const char* cullFace = "none";

		if ((loadBits0 & GFXS0_CULL_MASK) == GFXS0_CULL_BACK)
		{
			cullFace = "back";
		}
		else if ((loadBits0 & GFXS0_CULL_MASK) == GFXS0_CULL_FRONT)
		{
			cullFace = "front";
		}

		rapidjson::Value json(rapidjson::kObjectType);

		json.AddMember("alphaTest", rapidjson::StringRef(alphaTest), allocator);
		json.AddMember("blendOpAlpha", (loadBits0 & GFXS0_BLENDOP_ALPHA_MASK) >> GFXS0_BLENDOP_ALPHA_SHIFT, allocator);
		json.AddMember("blendOpRgb", (loadBits0 & GFXS0_BLENDOP_RGB_MASK) >> GFXS0_BLENDOP_RGB_SHIFT, allocator);
		json.AddMember("colorWriteAlpha", (loadBits0 & GFXS0_COLORWRITE_ALPHA) != 0, allocator);
		json.AddMember("colorWriteRgb", (loadBits0 & GFXS0_COLORWRITE_RGB) != 0, allocator);
		json.AddMember("cullFace", rapidjson::StringRef(cullFace), allocator);
		json.AddMember("depthTest", depthTest, allocator);
		json.AddMember("depthWrite", (loadBits1 & GFXS1_DEPTHWRITE) != 0, allocator);
		json.AddMember("dstBlendAlpha", (loadBits0 & GFXS0_DSTBLEND_ALPHA_MASK) >> GFXS0_DSTBLEND_ALPHA_SHIFT, allocator);
		json.AddMember("dstBlendRgb", (loadBits0 & GFXS0_DSTBLEND_RGB_MASK) >> GFXS0_DSTBLEND_RGB_SHIFT, allocator);
		json.AddMember("gammaWrite", (loadBits0 & GFXS0_GAMMAWRITE) != 0, allocator);
		json.AddMember("polygonOffset", (loadBits1 & GFXS1_POLYGON_OFFSET_MASK) >> GFXS1_POLYGON_OFFSET_SHIFT, allocator);
		json.AddMember("polymodeLine", (loadBits0 & GFXS0_POLYMODE_LINE) != 0, allocator);
		json.AddMember("srcBlendRgb", (loadBits0 & GFXS0_SRCBLEND_RGB_MASK) >> GFXS0_SRCBLEND_RGB_SHIFT, allocator);
		json.AddMember("srcBlendAlpha", (loadBits0 & GFXS0_SRCBLEND_ALPHA_MASK) >> GFXS0_SRCBLEND_ALPHA_SHIFT, allocator);
		json.AddMember("stencilBackEnabled", (loadBits1 & GFXS1_STENCIL_BACK_ENABLE) != 0, allocator);
		json.AddMember("stencilBackFail", (loadBits1 >> GFXS1_STENCIL_BACK_FAIL_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilBackFunc", (loadBits1 >> GFXS1_STENCIL_BACK_FUNC_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilBackPass", (loadBits1 >> GFXS1_STENCIL_BACK_PASS_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilBackZFail", (loadBits1 >> GFXS1_STENCIL_BACK_ZFAIL_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilFrontEnabled", (loadBits1 & GFXS1_STENCIL_FRONT_ENABLE) != 0, allocator);
		json.AddMember("stencilFrontFail", (loadBits1 >> GFXS1_STENCIL_FRONT_FAIL_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilFrontFunc", (loadBits1 >> GFXS1_STENCIL_FRONT_FUNC_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilFrontPass", (loadBits1 >> GFXS1_STENCIL_FRONT_PASS_SHIFT) & GFXS_STENCILOP_MASK, allocator);
		json.AddMember("stencilFrontZFail", (loadBits1 >> GFXS1_STENCIL_FRONT_ZFAIL_SHIFT) & GFXS_STENCILOP_MASK, allocator);

		return json;
	}

	static rapidjson::Value WaterToJson(const Game::water_t* water, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);

		if (water->image)
		{
			json.AddMember("image", rapidjson::Value(water->image->name, allocator), allocator);
			Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_IMAGE, water->image });
		}

		const auto count = static_cast<std::size_t>(std::max(water->M, 0)) * static_cast<std::size_t>(std::max(water->N, 0));

		if (water->H0Real && water->H0Imag)
		{
			std::vector<Game::complex_s> h0(count);

			for (std::size_t i = 0; i < count; ++i)
			{
				h0[i].real = water->H0Real[i];
				h0[i].imag = water->H0Imag[i];
			}

			json.AddMember("H0", rapidjson::Value(EncodeBase64(h0.data(), count * sizeof(Game::complex_s)).data(), allocator), allocator);
		}

		if (water->wTerm)
		{
			json.AddMember("wTerm", rapidjson::Value(EncodeBase64(water->wTerm, count * sizeof(float)).data(), allocator), allocator);
		}

		json.AddMember("M", water->M, allocator);
		json.AddMember("N", water->N, allocator);
		json.AddMember("Lx", water->Lx, allocator);
		json.AddMember("Lz", water->Lz, allocator);
		json.AddMember("gravity", water->gravity, allocator);
		json.AddMember("windvel", water->windvel, allocator);
		json.AddMember("winddir", Utils::JSON::MakeArray(water->winddir, 2, allocator), allocator);
		json.AddMember("amplitude", water->amplitude, allocator);
		json.AddMember("codeConstant", Utils::JSON::MakeArray(water->codeConstant, 4, allocator), allocator);

		return json;
	}

	void IMaterial::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (!header->data)
		{
			this->LoadFromDisk(header, name, builder);
		}

		if (!header->data)
		{
			this->LoadNative(header, name, builder);
		}
	}

	void IMaterial::LoadFromDisk(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File materialFile(std::format("materials/{}.iw4x.json", name));

		if (!materialFile.Exists())
		{
			return;
		}

		header->material = ReadMaterial(name, materialFile.GetBuffer(), builder);
	}

	void IMaterial::LoadNative(Game::XAssetHeader* header, const std::string& name, [[maybe_unused]] Components::ZoneBuilder::Zone* builder)
	{
		header->material = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).material;
	}

	void IMaterial::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.material;

		if (asset->techniqueSet)
		{
			builder->LoadAsset(Game::ASSET_TYPE_TECHNIQUE_SET, asset->techniqueSet);
		}

		if (!asset->textureTable)
		{
			return;
		}

		for (unsigned char i = 0; i < asset->textureCount; ++i)
		{
			const auto* const textureDef = &asset->textureTable[i];

			if (!textureDef->u.image)
			{
				continue;
			}

			if (textureDef->semantic == TS_WATER_MAP)
			{
				if (textureDef->u.water->image)
				{
					builder->LoadAsset(Game::ASSET_TYPE_IMAGE, textureDef->u.water->image);
				}
			}
			else
			{
				builder->LoadAsset(Game::ASSET_TYPE_IMAGE, textureDef->u.image);
			}
		}
	}

	void IMaterial::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.material;
		auto* const dest = buffer->Dest<Game::X86::Material>();

		auto record = Game::X86::Convert(*asset);
		record.info.drawSurf.packed = 0;
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->info.name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->info.name));
			Utils::Stream::ClearPointer(&dest->info.name);
		}

		if (asset->techniqueSet)
		{
			dest->techniqueSet = builder->SaveSubAsset(Game::ASSET_TYPE_TECHNIQUE_SET, asset->techniqueSet);
		}

		if (asset->textureTable)
		{
			if (builder->HasPointer(asset->textureTable))
			{
				dest->textureTable = builder->GetPointer(asset->textureTable);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(asset->textureTable);

				auto* const destTextureTable = buffer->Dest<Game::X86::MaterialTextureDef>();

				for (unsigned char i = 0; i < asset->textureCount; ++i)
				{
					auto textureRecord = Game::X86::Convert(asset->textureTable[i]);
					textureRecord.u.image = 0;
					buffer->Save(&textureRecord);
				}

				for (unsigned char i = 0; i < asset->textureCount; ++i)
				{
					auto* const destTextureDef = &destTextureTable[i];
					const auto* const textureDef = &asset->textureTable[i];

					if (textureDef->semantic == TS_WATER_MAP)
					{
						const auto* const water = textureDef->u.water;

						if (!water)
						{
							continue;
						}

						buffer->Align(Utils::Stream::ALIGN_4);

						auto* const destWater = buffer->Dest<Game::X86::water_t>();
						const auto waterRecord = Game::X86::Convert(*water);
						buffer->Save(&waterRecord);
						Utils::Stream::ClearPointer(&destTextureDef->u.water);

						const auto count = static_cast<std::size_t>(water->M) * static_cast<std::size_t>(water->N);

						if (water->H0Real && water->H0Imag)
						{
							buffer->Align(Utils::Stream::ALIGN_4);

							for (std::size_t j = 0; j < count; ++j)
							{
								const Game::complex_s value{ water->H0Real[j], water->H0Imag[j] };
								buffer->Save(&value);
							}

							Utils::Stream::ClearPointer(&destWater->H0);
						}

						if (water->wTerm)
						{
							buffer->Align(Utils::Stream::ALIGN_4);
							buffer->SaveArray(water->wTerm, count);
							Utils::Stream::ClearPointer(&destWater->wTerm);
						}

						if (water->image)
						{
							destWater->image = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, water->image);
						}
					}
					else if (textureDef->u.image)
					{
						destTextureDef->u.image = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, textureDef->u.image);
					}
				}

				Utils::Stream::ClearPointer(&dest->textureTable);
			}
		}

		if (asset->constantTable)
		{
			if (builder->HasPointer(asset->constantTable))
			{
				dest->constantTable = builder->GetPointer(asset->constantTable);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_16);
				builder->StorePointer(asset->constantTable);

				for (unsigned char i = 0; i < asset->constantCount; ++i)
				{
					const auto constantRecord = Game::X86::Convert(asset->constantTable[i]);
					buffer->Save(&constantRecord);
				}

				Utils::Stream::ClearPointer(&dest->constantTable);
			}
		}

		if (asset->stateBitsTable)
		{
			if (builder->HasPointer(asset->stateBitsTable))
			{
				dest->stateBitsTable = builder->GetPointer(asset->stateBitsTable);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(asset->stateBitsTable);

				for (unsigned char i = 0; i < asset->stateBitsCount; ++i)
				{
					const auto stateBitsRecord = Game::X86::Convert(asset->stateBitsTable[i]);
					buffer->Save(&stateBitsRecord);
				}

				Utils::Stream::ClearPointer(&dest->stateBitsTable);
			}
		}

		buffer->PopBlock();
	}

	void IMaterial::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.material;

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_MAT_VERSION, allocator);
		output.AddMember("name", rapidjson::Value(asset->info.name, allocator), allocator);
		output.AddMember("gameFlags", rapidjson::Value(std::format("{:08b}", asset->info.gameFlags).data(), allocator), allocator);
		output.AddMember("stateFlags", rapidjson::Value(std::format("{:08b}", asset->stateFlags).data(), allocator), allocator);
		output.AddMember("sortKey", static_cast<int>(asset->info.sortKey), allocator);

		if (asset->techniqueSet)
		{
			output.AddMember("techniqueSet", rapidjson::Value(asset->techniqueSet->name, allocator), allocator);
			Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_TECHNIQUE_SET, asset->techniqueSet });
		}

		output.AddMember("textureAtlasRowCount", static_cast<int>(asset->info.textureAtlasRowCount), allocator);
		output.AddMember("textureAtlasColumnCount", static_cast<int>(asset->info.textureAtlasColumnCount), allocator);
		output.AddMember("surfaceTypeBits", rapidjson::Value(std::format("{:032b}", asset->info.surfaceTypeBits).data(), allocator), allocator);

		rapidjson::Value textureTable(rapidjson::kArrayType);

		for (unsigned char i = 0; asset->textureTable && i < asset->textureCount; ++i)
		{
			const auto* const textureDef = &asset->textureTable[i];
			rapidjson::Value textureJson(rapidjson::kObjectType);

			textureJson.AddMember("nameStart", static_cast<int>(textureDef->nameStart), allocator);
			textureJson.AddMember("nameEnd", static_cast<int>(textureDef->nameEnd), allocator);
			textureJson.AddMember("nameHash", textureDef->nameHash, allocator);
			textureJson.AddMember("samplerState", static_cast<int>(textureDef->samplerState), allocator);
			textureJson.AddMember("semantic", static_cast<int>(textureDef->semantic), allocator);

			if (textureDef->semantic == TS_WATER_MAP)
			{
				if (textureDef->u.water)
				{
					textureJson.AddMember("water", WaterToJson(textureDef->u.water, allocator), allocator);
				}
			}
			else
			{
				if (!textureDef->u.image)
				{
					Components::Logger::Fatal("Null/missing image for material {}", asset->info.name);
				}

				textureJson.AddMember("image", rapidjson::Value(textureDef->u.image->name, allocator), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_IMAGE, textureDef->u.image });
			}

			textureTable.PushBack(textureJson, allocator);
		}

		output.AddMember("textureTable", textureTable, allocator);

		const auto& fields = asset->info.drawSurf.fields;
		rapidjson::Value drawSurface(rapidjson::kObjectType);

		drawSurface.AddMember("objectId", static_cast<std::uint64_t>(fields.objectId), allocator);
		drawSurface.AddMember("reflectionProbeIndex", static_cast<std::uint64_t>(fields.reflectionProbeIndex), allocator);
		drawSurface.AddMember("hasGfxEntIndex", static_cast<std::uint64_t>(fields.hasGfxEntIndex), allocator);
		drawSurface.AddMember("customIndex", static_cast<std::uint64_t>(fields.customIndex), allocator);
		drawSurface.AddMember("materialSortedIndex", static_cast<std::uint64_t>(fields.materialSortedIndex), allocator);
		drawSurface.AddMember("prepass", static_cast<std::uint64_t>(fields.prepass), allocator);
		drawSurface.AddMember("useHeroLighting", static_cast<std::uint64_t>(fields.useHeroLighting), allocator);
		drawSurface.AddMember("sceneLightIndex", static_cast<std::uint64_t>(fields.sceneLightIndex), allocator);
		drawSurface.AddMember("surfType", static_cast<std::uint64_t>(fields.surfType), allocator);
		drawSurface.AddMember("primarySortKey", static_cast<std::uint64_t>(fields.primarySortKey), allocator);
		drawSurface.AddMember("unused", static_cast<std::uint64_t>(fields.unused), allocator);

		output.AddMember("gfxDrawSurface", drawSurface, allocator);
		output.AddMember("hashIndex", 0, allocator);

		rapidjson::Value stateBitsEntry(rapidjson::kArrayType);

		for (const auto entry : asset->stateBitsEntry)
		{
			stateBitsEntry.PushBack(static_cast<int>(static_cast<char>(entry)), allocator);
		}

		output.AddMember("stateBitsEntry", stateBitsEntry, allocator);
		output.AddMember("cameraRegion", static_cast<int>(asset->cameraRegion), allocator);

		if (asset->constantTable)
		{
			rapidjson::Value constantTable(rapidjson::kArrayType);

			for (unsigned char i = 0; i < asset->constantCount; ++i)
			{
				const auto* const constantDef = &asset->constantTable[i];
				rapidjson::Value constantJson(rapidjson::kObjectType);

				const std::string constantName(constantDef->name, strnlen(constantDef->name, sizeof(constantDef->name)));

				constantJson.AddMember("nameHash", constantDef->nameHash, allocator);
				constantJson.AddMember("literal", Utils::JSON::MakeArray(constantDef->literal, 4, allocator), allocator);
				constantJson.AddMember("name", rapidjson::Value(constantName.data(), static_cast<rapidjson::SizeType>(constantName.size()), allocator), allocator);

				constantTable.PushBack(constantJson, allocator);
			}

			output.AddMember("constantTable", constantTable, allocator);
		}

		if (asset->stateBitsTable)
		{
			rapidjson::Value stateBitsTable(rapidjson::kArrayType);

			for (unsigned char i = 0; i < asset->stateBitsCount; ++i)
			{
				stateBitsTable.PushBack(StateBitsToJson(asset->stateBitsTable[i], allocator), allocator);
			}

			output.AddMember("stateBitsTable", stateBitsTable, allocator);
		}

		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(buffer);
		writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);
		output.Accept(writer);

		Utils::IO::WriteFile(std::format("{}/materials/{}.iw4x.json", Components::ZoneBuilder::GetDumpingZonePath(), asset->info.name), buffer.GetString());
	}

	IMaterial::IMaterial()
	{
		Components::Command::Add("hashName", [](const Components::Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			for (int i = 1; i < params->Size(); ++i)
			{
				Components::Logger::Print("{}\t=>\t{}\n", params->Get(i), Game::R_HashString(params->Get(i)));
			}
		});

		Components::Command::Add("dumpMaterial", [](const Components::Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const std::string material = params->Get(1);

			if (!Game::DB_FindXAssetEntry(Game::ASSET_TYPE_MATERIAL, material.data()))
			{
				Components::Logger::Print("Could not find material {}!\n", material);
				return;
			}

			const Game::XAsset asset{ Game::ASSET_TYPE_MATERIAL, Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, material.data()) };
			Components::AssetHandler::DumpAsset(asset);
		});
	}
}
