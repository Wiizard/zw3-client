#include "STDInclude.hpp"

#pragma warning(push, 0)
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#pragma warning(pop)

#include "IFont_s.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	struct FontPack
	{
		const std::uint8_t* data;
		float pixelHeight;
		int yOffset;
		std::uint8_t* pixels;
		int width;
		int height;
	};

	static int PackFonts(const FontPack& pack, const std::vector<std::uint16_t>& charset, Game::Glyph* glyphs)
	{
		stbtt_fontinfo font{};
		font.userdata = nullptr;

		if (!stbtt_InitFont(&font, pack.data, 0))
		{
			return -1;
		}

		std::memset(pack.pixels, 0, static_cast<std::size_t>(pack.width) * static_cast<std::size_t>(pack.height));

		int x = 1;
		int y = 1;
		int bottomY = 1;

		const float scale = stbtt_ScaleForPixelHeight(&font, pack.pixelHeight);

		int i = 0;

		for (const auto character : charset)
		{
			int advance = 0;
			int leftSideBearing = 0;
			int x0 = 0;
			int y0 = 0;
			int x1 = 0;
			int y1 = 0;

			const int glyphIndex = stbtt_FindGlyphIndex(&font, character);

			stbtt_GetGlyphHMetrics(&font, glyphIndex, &advance, &leftSideBearing);
			stbtt_GetGlyphBitmapBox(&font, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);

			const int glyphWidth = x1 - x0;
			const int glyphHeight = y1 - y0;

			if (x + glyphWidth + 1 >= pack.width)
			{
				y = bottomY;
				x = 1;
			}

			if (y + glyphHeight + 1 >= pack.height)
			{
				return -i;
			}

			stbtt_MakeGlyphBitmap(&font, pack.pixels + x + y * pack.width, glyphWidth, glyphHeight, pack.width, scale, scale, glyphIndex);

			auto& glyph = glyphs[i++];

			glyph.letter = character;
			glyph.s0 = static_cast<float>(x) / static_cast<float>(pack.width);
			glyph.s1 = static_cast<float>(x + glyphWidth) / static_cast<float>(pack.width);
			glyph.t0 = static_cast<float>(y) / static_cast<float>(pack.height);
			glyph.t1 = static_cast<float>(y + glyphHeight) / static_cast<float>(pack.height);
			glyph.pixelWidth = static_cast<char>(glyphWidth);
			glyph.pixelHeight = static_cast<char>(glyphHeight);
			glyph.x0 = static_cast<char>(x0);
			glyph.y0 = static_cast<char>(y0 + pack.yOffset);
			glyph.dx = static_cast<char>(std::roundf(scale * static_cast<float>(advance)));

			x = x + glyphWidth + 1;

			if (y + glyphHeight + 1 > bottomY)
			{
				bottomY = y + glyphHeight + 1;
			}
		}

		return bottomY;
	}

	static bool TryGetInt(const nlohmann::json& object, const char* member, int& out)
	{
		const auto found = object.find(member);

		if (found == object.end() || !found->is_number_integer())
		{
			return false;
		}

		out = found->get<int>();
		return true;
	}

	static void* FindTemplateAsset(Game::XAssetType type, const char* name)
	{
		void* const asset = Components::AssetHandler::FindLoadedAsset(type, name).data;

		if (!asset)
		{
			Components::Logger::Fatal("Font templates are missing: no {} '{}' is loaded\n", Game::DB_GetXAssetTypeName(type), name);
		}

		return asset;
	}

	void IFont_s::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.font;

		if (asset->material)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->material);
		}

		if (asset->glowMaterial)
		{
			builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, asset->glowMaterial);
		}
	}

	void IFont_s::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File fontDefFile(std::format("{}.json", name));
		Components::FileSystem::File fontFile(std::format("{}.ttf", name));

		if (!fontDefFile.Exists() || !fontFile.Exists())
		{
			return;
		}

		const auto fontDef = nlohmann::json::parse(fontDefFile.GetBuffer(), nullptr, false);

		if (fontDef.is_discarded() || !fontDef.is_object())
		{
			Components::Logger::Fatal("JSON Parse Error. Font {} is invalid\n", name);
		}

		int width = 0;
		int height = 0;
		int size = 0;
		int yOffset = 0;

		if (!TryGetInt(fontDef, "textureWidth", width) || !TryGetInt(fontDef, "textureHeight", height)
			|| !TryGetInt(fontDef, "size", size) || !TryGetInt(fontDef, "yOffset", yOffset))
		{
			Components::Logger::Fatal("Font {} needs textureWidth, textureHeight, size and yOffset\n", name);
		}

		if (width <= 0 || height <= 0)
		{
			Components::Logger::Fatal("Font {} has an empty texture\n", name);
		}

		auto* const allocator = builder->GetAllocator();
		auto* const pixels = allocator->AllocateArray<std::uint8_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

		const auto* const texName = allocator->DuplicateString(std::format("if_{}", name.substr(std::min<std::size_t>(6, name.size()))));
		const auto* const fontName = allocator->DuplicateString(name);
		const auto* const glowMaterialName = allocator->DuplicateString(std::format("{}_glow", name));

		auto* const image = allocator->Allocate<Game::GfxImage>();
		std::memcpy(image, FindTemplateAsset(Game::ASSET_TYPE_IMAGE, "gamefonts_pc"), sizeof(Game::GfxImage));

		image->name = texName;

		auto* const material = allocator->Allocate<Game::Material>();
		std::memcpy(material, FindTemplateAsset(Game::ASSET_TYPE_MATERIAL, "fonts/gamefonts_pc"), sizeof(Game::Material));

		auto* const textureTable = allocator->Allocate<Game::MaterialTextureDef>();
		std::memcpy(textureTable, material->textureTable, sizeof(Game::MaterialTextureDef));

		material->textureTable = textureTable;
		material->textureTable->u.image = image;
		material->info.name = fontName;

		auto* const glowMaterial = allocator->Allocate<Game::Material>();
		std::memcpy(glowMaterial, FindTemplateAsset(Game::ASSET_TYPE_MATERIAL, "fonts/gamefonts_pc_glow"), sizeof(Game::Material));

		glowMaterial->textureTable = material->textureTable;
		glowMaterial->info.name = glowMaterialName;

		std::vector<std::uint16_t> charset;
		const auto charsetJson = fontDef.find("charset");

		if (charsetJson != fontDef.end() && charsetJson->is_array())
		{
			for (const auto& character : *charsetJson)
			{
				if (!character.is_number_integer())
				{
					Components::Logger::Fatal("Font {} has a charset entry that is not a number\n", name);
				}

				charset.push_back(static_cast<std::uint16_t>(character.get<int>()));
			}

			std::ranges::sort(charset);

			for (std::uint16_t i = 32; i < 128; ++i)
			{
				if (std::ranges::find(charset, i) == charset.end())
				{
					Components::Logger::Fatal("Font {} missing codepoint {}", name, i);
				}
			}
		}
		else
		{
			for (std::uint16_t i = 32; i < 128; ++i)
			{
				charset.push_back(i);
			}
		}

		auto* const font = allocator->Allocate<Game::Font_s>();
		auto* const glyphs = allocator->AllocateArray<Game::Glyph>(charset.size());

		font->fontName = fontName;
		font->pixelHeight = size;
		font->material = material;
		font->glowMaterial = glowMaterial;
		font->glyphCount = static_cast<int>(charset.size());
		font->glyphs = glyphs;

		const FontPack pack{ reinterpret_cast<const std::uint8_t*>(fontFile.GetBuffer().data()), static_cast<float>(size), yOffset, pixels, width, height };
		const int result = PackFonts(pack, charset, glyphs);

		if (result == -1)
		{
			Components::Logger::Fatal("Truetype font {} is broken", name);
		}
		else if (result < 0)
		{
			Components::Logger::Fatal("Texture size of font {} is not enough", name);
		}
		else if (height - result > size)
		{
			Components::Logger::Warning("Texture of font {} have too much left over space: {}\n", name, height - result);
		}

		header->font = font;

		Game::XAssetHeader temporaryHeader{};

		temporaryHeader.image = image;
		Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_IMAGE, temporaryHeader);

		temporaryHeader.material = material;
		Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_MATERIAL, temporaryHeader);

		temporaryHeader.material = glowMaterial;
		Components::AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_MATERIAL, temporaryHeader);

		Utils::IO::CreateDir("userraw\\images");

		const int fileSize = width * height * 4;
		const int iwiHeaderSize = static_cast<int>(sizeof(Game::GfxImageFileHeader));

		const Game::GfxImageFileHeader iwiHeader =
		{
			{ 'I', 'W', 'i' },
			8,
			2,
			Game::IMG_FORMAT_BITMAP_RGBA,
			0,
			{ static_cast<short>(width), static_cast<short>(height), 1 },
			{ fileSize + iwiHeaderSize, fileSize, fileSize, fileSize },
		};

		std::string outIwi;
		outIwi.resize(static_cast<std::size_t>(fileSize) + sizeof(Game::GfxImageFileHeader));

		std::memcpy(outIwi.data(), &iwiHeader, sizeof(Game::GfxImageFileHeader));

		auto* const rgbaPixels = outIwi.data() + sizeof(Game::GfxImageFileHeader);

		for (int i = 0; i < fileSize; i += 4)
		{
			rgbaPixels[i + 0] = static_cast<char>(255);
			rgbaPixels[i + 1] = static_cast<char>(255);
			rgbaPixels[i + 2] = static_cast<char>(255);
			rgbaPixels[i + 3] = static_cast<char>(pixels[i / 4]);
		}

		Utils::IO::WriteFile(std::format("userraw\\images\\{}.iwi", texName), outIwi);
	}

	void IFont_s::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.font;
		auto* const dest = buffer->Dest<Game::X86::Font_s>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->fontName)
		{
			buffer->SaveString(asset->fontName);
			Utils::Stream::ClearPointer(&dest->fontName);
		}

		if (asset->material)
		{
			dest->material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->material);
		}

		if (asset->glowMaterial)
		{
			dest->glowMaterial = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, asset->glowMaterial);
		}

		if (asset->glyphs)
		{
			const auto* const glyphs = static_cast<const Game::Glyph*>(asset->glyphs);
			buffer->Align(Utils::Stream::ALIGN_4);

			for (int i = 0; i < asset->glyphCount; ++i)
			{
				const auto glyphRecord = Game::X86::Convert(glyphs[i]);
				buffer->Save(&glyphRecord);
			}

			Utils::Stream::ClearPointer(&dest->glyphs);
		}

		buffer->PopBlock();
	}
}
