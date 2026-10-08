#include "STDInclude.hpp"

#include <objidl.h>

#pragma push_macro("min")
#pragma push_macro("max")
#define min std::min
#define max std::max
#include <gdiplus.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#pragma comment(lib, "gdiplus.lib")

#include "Materials.hpp"
#include "AssetHandler.hpp"
#include "FastFiles.hpp"
#include "Logger.hpp"
#include "Renderer.hpp"

extern "C"
{
	void IwiVersionStub();
}

namespace Components
{
	constexpr std::uintptr_t Image_LoadFromFileWithReader_VersionTest = 0x14006A212;
	static const std::uint8_t versionTest[] = { 0x80, 0x7D, 0xE9, 0x69, 0x75, 0xDE, 0x80, 0x7D, 0xEA, 0x08 };

	constexpr std::uintptr_t Material_Process2DTextureCoordsForAtlasing = 0x14001A6C0;
	constexpr std::uintptr_t r_atlasAnimFPS = 0x148C285D8;

	constexpr std::uintptr_t Com_Error = 0x1401F38F0;
	constexpr std::uintptr_t R_DelayLoadImage_Com_ErrorCall = 0x140037AE3;
	constexpr std::uintptr_t Load_Texture_Com_ErrorCall = 0x140037BE9;

	static const std::uintptr_t atlasCalls[] =
	{
		0x1400F4246,
		0x1400F4376,
		0x1400F443C,
		0x1400F451D,
		0x1400F463C,
	};

	constexpr unsigned char textureSemantic2d = 0;
	constexpr unsigned char textureSemanticColorMap = 2;

	constexpr unsigned int writableImageFlags = 0x1000003;

	constexpr std::uintptr_t DB_LoadXAssets_UnloadDirtyCall = 0x14012EFFA;
	constexpr std::uintptr_t Material_DirtyTechniqueSetOverrides = 0x14003AE10;
	constexpr std::uintptr_t Material_OverrideTechniqueSets = 0x14003ABE0;

	static Utils::Hook unloadOverridesHook;

	static Utils::Hook versionHook;
	static Utils::Hook atlasHooks[std::size(atlasCalls)];

	std::vector<Game::GfxImage*> Materials::imageTable;
	std::vector<Game::Material*> Materials::materialTable;

	static std::unordered_map<std::string, Game::Material*> runtimeMaterials;
	static std::recursive_mutex tableMutex;

	static ULONG_PTR gdiPlusToken = 0;

#pragma pack(push, 1)
	struct NewsIwiHeader
	{
		std::uint8_t format;
		std::uint8_t flags;
		std::uint16_t width;
		std::uint16_t height;
		std::uint16_t depth;
	};
#pragma pack(pop)

	constexpr std::size_t iwiHeaderOffset = 8;
	constexpr std::size_t iwiDataOffset = iwiHeaderOffset + sizeof(NewsIwiHeader) + 16;

	constexpr std::uint8_t iwiFormatArgb32 = 0x01;
	constexpr std::uint8_t iwiFormatDxt5 = 0x0D;
	constexpr std::uint8_t iwiFormatDxt5Alt = 0x0E;

	static const char* VoteDvarString(const char* name)
	{
		const Game::dvar_t* const dvar = Game::Dvar_FindVar(name);

		if (!dvar || dvar->type != Game::DVAR_TYPE_STRING || !dvar->current.string)
		{
			return "";
		}

		return dvar->current.string;
	}

	static bool IsPreviewOf(const char* materialName, const char* mapId)
	{
		return *mapId && !_strnicmp(materialName, "preview_", 8) && !_stricmp(materialName + 8, mapId);
	}

	static void CropMatchmakingVotePreview(const Game::Material* material, float* s0, float* s1, float* t0, float* t1)
	{
		const bool isWholeImage = *s0 == 0.0f && *s1 == 1.0f && *t0 == 0.0f && *t1 == 1.0f;

		if (!material || !material->info.name || !material->textureTable || !isWholeImage)
		{
			return;
		}

		const Game::dvar_t* const voteActive = Game::Dvar_FindVar("zwnet_vote_active");

		if (!voteActive)
		{
			return;
		}

		const bool isVoteActive = voteActive->current.enabled;
		bool isWinner = false;

		if (!isVoteActive)
		{
			const char* const winnerId = VoteDvarString("zwnet_vote_winner_id");

			if (!*winnerId)
			{
				return;
			}

			const char* const winnerImage = VoteDvarString("zwnet_vote_winner_image");
			isWinner = !_stricmp(material->info.name, winnerImage) || IsPreviewOf(material->info.name, winnerId);

			if (!isWinner)
			{
				return;
			}
		}

		const char* const mapA = VoteDvarString("zwnet_vote_map_a_image");
		const char* const mapB = VoteDvarString("zwnet_vote_map_b_image");
		const char* const mapAId = VoteDvarString("zwnet_vote_map_a_id");
		const char* const mapBId = VoteDvarString("zwnet_vote_map_b_id");
		const bool isMapA = !_stricmp(material->info.name, mapA) || IsPreviewOf(material->info.name, mapAId);
		const bool isMapB = !_stricmp(material->info.name, mapB) || IsPreviewOf(material->info.name, mapBId);

		if (!isWinner && !isMapA && !isMapB)
		{
			return;
		}

		Game::menuDef_t* const menu = Game::Menus_FindByName(Game::uiContext, "zwnet_matchmaking");

		if (!menu || !Game::Menus_MenuIsInStack(Game::uiContext, menu))
		{
			return;
		}

		const Game::GfxImage* image = nullptr;

		for (int i = 0; i < material->textureCount; ++i)
		{
			const unsigned char semantic = material->textureTable[i].semantic;

			if (semantic == textureSemantic2d || semantic == textureSemanticColorMap)
			{
				image = material->textureTable[i].u.image;
				break;
			}
		}

		if (!image || !image->width || !image->height)
		{
			return;
		}

		const char* itemName = "image_map_preview_vote_b";
		const char* fallbackName = "image_map_preview_vote_b_fallback";

		if (isWinner)
		{
			itemName = "image_map_preview_winner";
			fallbackName = "image_map_preview_winner_fallback";
		}
		else if (isMapA)
		{
			itemName = "image_map_preview_vote_a";
			fallbackName = "image_map_preview_vote_a_fallback";
		}

		for (int i = 0; i < menu->itemCount; ++i)
		{
			const Game::itemDef_s* const item = menu->items[i];

			if (!item || !item->window.name)
			{
				continue;
			}

			if (_stricmp(item->window.name, itemName) && _stricmp(item->window.name, fallbackName))
			{
				continue;
			}

			const Game::rectDef_s& rect = item->window.rect;

			if (rect.w <= 0.0f || rect.h <= 0.0f)
			{
				return;
			}

			const float imageAspect = static_cast<float>(image->width) / static_cast<float>(image->height);
			const float cardAspect = rect.w / rect.h;

			if (imageAspect < cardAspect)
			{
				const float span = imageAspect / cardAspect;
				*t0 = (1.0f - span) * 0.5f;
				*t1 = (1.0f + span) * 0.5f;
			}
			else
			{
				const float span = cardAspect / imageAspect;
				*s0 = (1.0f - span) * 0.5f;
				*s1 = (1.0f + span) * 0.5f;
			}

			return;
		}
	}

	static void ProcessAtlasCoords(const Game::Material* material, float* s0, float* s1, float* t0, float* t1)
	{
		const bool isSpinner = material && material->info.name && _stricmp(material->info.name, "searching_for_player") == 0;

		if (!isSpinner)
		{
			reinterpret_cast<void(*)(const Game::Material*, float*, float*, float*, float*)>(
				Utils::Hook::Rebase(Material_Process2DTextureCoordsForAtlasing))(material, s0, s1, t0, t1);
			CropMatchmakingVotePreview(material, s0, s1, t0, t1);
			return;
		}

		constexpr unsigned int columnCount = 8;
		constexpr unsigned int rowCount = 4;
		constexpr unsigned int frameCount = 30;

		const auto* const atlasFps = Utils::Hook::Get<Game::dvar_t*>(r_atlasAnimFPS);
		int frameRate = 15;

		if (atlasFps)
		{
			frameRate = std::max(atlasFps->current.integer, 1);
		}

		const auto frame = static_cast<unsigned int>((static_cast<std::uint64_t>(timeGetTime()) * frameRate / 1000u) % frameCount);
		const auto column = frame % columnCount;
		const auto row = frame / columnCount;

		const float cellWidth = 1.0f / static_cast<float>(columnCount);
		const float cellHeight = 1.0f / static_cast<float>(rowCount);

		*s0 = (static_cast<float>(column) + *s0) * cellWidth;
		*s1 = (static_cast<float>(column) + *s1) * cellWidth;
		*t0 = (static_cast<float>(row) + *t0) * cellHeight;
		*t1 = (static_cast<float>(row) + *t1) * cellHeight;
	}

	static bool EnsureGdiPlusStarted()
	{
		if (gdiPlusToken)
		{
			return true;
		}

		const Gdiplus::GdiplusStartupInput input;
		return Gdiplus::GdiplusStartup(&gdiPlusToken, &input, nullptr) == Gdiplus::Ok;
	}

	static std::uint16_t ColorTo565(const unsigned char* bgra)
	{
		const auto r = bgra[2] >> 3;
		const auto g = bgra[1] >> 2;
		const auto b = bgra[0] >> 3;

		return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
	}

	static void ColorFrom565(std::uint16_t color, unsigned char out[3])
	{
		out[0] = static_cast<unsigned char>(((color >> 11) & 31) * 255 / 31);
		out[1] = static_cast<unsigned char>(((color >> 5) & 63) * 255 / 63);
		out[2] = static_cast<unsigned char>((color & 31) * 255 / 31);
	}

	static void WriteLe16(std::string& out, std::uint16_t value)
	{
		out.push_back(static_cast<char>(value & 0xFF));
		out.push_back(static_cast<char>((value >> 8) & 0xFF));
	}

	static void WriteLe32(std::string& out, std::uint32_t value)
	{
		out.push_back(static_cast<char>(value & 0xFF));
		out.push_back(static_cast<char>((value >> 8) & 0xFF));
		out.push_back(static_cast<char>((value >> 16) & 0xFF));
		out.push_back(static_cast<char>((value >> 24) & 0xFF));
	}

	static std::uint16_t ReadLe16(const unsigned char* data)
	{
		return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
	}

	static std::uint32_t ReadLe32(const unsigned char* data)
	{
		return static_cast<std::uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
	}

	static void BuildAlphaPalette(unsigned char first, unsigned char second, unsigned char palette[8])
	{
		palette[0] = first;
		palette[1] = second;

		if (first > second)
		{
			for (int i = 1; i <= 6; ++i)
			{
				palette[i + 1] = static_cast<unsigned char>(((7 - i) * first + i * second) / 7);
			}

			return;
		}

		for (int i = 1; i <= 4; ++i)
		{
			palette[i + 1] = static_cast<unsigned char>(((5 - i) * first + i * second) / 5);
		}

		palette[6] = 0;
		palette[7] = 255;
	}

	static void BuildColorPalette(std::uint16_t color0, std::uint16_t color1, unsigned char palette[4][3])
	{
		ColorFrom565(color0, palette[0]);
		ColorFrom565(color1, palette[1]);

		for (int c = 0; c < 3; ++c)
		{
			palette[2][c] = static_cast<unsigned char>((2 * palette[0][c] + palette[1][c]) / 3);
			palette[3][c] = static_cast<unsigned char>((palette[0][c] + 2 * palette[1][c]) / 3);
		}
	}

	static std::string EncodeNewsDxt5(const std::vector<unsigned char>& pixels, unsigned int width, unsigned int height)
	{
		std::string out;

		const auto blockCountX = (width + 3) / 4;
		const auto blockCountY = (height + 3) / 4;
		out.reserve(blockCountX * blockCountY * 16);

		for (unsigned int by = 0; by < blockCountY; ++by)
		{
			for (unsigned int bx = 0; bx < blockCountX; ++bx)
			{
				unsigned char block[16][4]{};

				for (unsigned int y = 0; y < 4; ++y)
				{
					for (unsigned int x = 0; x < 4; ++x)
					{
						const auto sx = std::min((bx * 4) + x, width - 1);
						const auto sy = std::min((by * 4) + y, height - 1);
						std::memcpy(block[(y * 4) + x], pixels.data() + (((sy * width) + sx) * 4), 4);
					}
				}

				unsigned char minAlpha = 255;
				unsigned char maxAlpha = 0;

				for (const auto& texel : block)
				{
					minAlpha = std::min(minAlpha, texel[3]);
					maxAlpha = std::max(maxAlpha, texel[3]);
				}

				out.push_back(static_cast<char>(maxAlpha));
				out.push_back(static_cast<char>(minAlpha));

				unsigned char alphaPalette[8]{};
				BuildAlphaPalette(maxAlpha, minAlpha, alphaPalette);

				std::uint64_t alphaMask = 0;

				for (unsigned int i = 0; i < 16; ++i)
				{
					unsigned int bestIndex = 0;
					unsigned int bestDistance = 999999;

					for (unsigned int a = 0; a < 8; ++a)
					{
						const auto distance = static_cast<unsigned int>(std::abs(static_cast<int>(block[i][3]) - static_cast<int>(alphaPalette[a])));

						if (distance < bestDistance)
						{
							bestDistance = distance;
							bestIndex = a;
						}
					}

					alphaMask |= static_cast<std::uint64_t>(bestIndex) << (i * 3);
				}

				for (unsigned int i = 0; i < 6; ++i)
				{
					out.push_back(static_cast<char>((alphaMask >> (8 * i)) & 0xFF));
				}

				const unsigned char* minColor = block[0];
				const unsigned char* maxColor = block[0];
				int minLuma = 999999;
				int maxLuma = -1;

				for (const auto& texel : block)
				{
					const int luma = static_cast<int>(texel[2]) * 299 + static_cast<int>(texel[1]) * 587 + static_cast<int>(texel[0]) * 114;

					if (luma < minLuma)
					{
						minLuma = luma;
						minColor = texel;
					}

					if (luma > maxLuma)
					{
						maxLuma = luma;
						maxColor = texel;
					}
				}

				auto color0 = ColorTo565(maxColor);
				auto color1 = ColorTo565(minColor);

				if (color0 <= color1)
				{
					std::swap(color0, color1);
				}

				WriteLe16(out, color0);
				WriteLe16(out, color1);

				unsigned char palette[4][3]{};
				BuildColorPalette(color0, color1, palette);

				std::uint32_t colorMask = 0;

				for (unsigned int i = 0; i < 16; ++i)
				{
					unsigned int bestIndex = 0;
					unsigned int bestDistance = 0xFFFFFFFFu;

					for (unsigned int c = 0; c < 4; ++c)
					{
						const int db = static_cast<int>(block[i][0]) - static_cast<int>(palette[c][2]);
						const int dg = static_cast<int>(block[i][1]) - static_cast<int>(palette[c][1]);
						const int dr = static_cast<int>(block[i][2]) - static_cast<int>(palette[c][0]);
						const auto distance = static_cast<unsigned int>((dr * dr) + (dg * dg) + (db * db));

						if (distance < bestDistance)
						{
							bestDistance = distance;
							bestIndex = c;
						}
					}

					colorMask |= bestIndex << (i * 2);
				}

				WriteLe32(out, colorMask);
			}
		}

		return out;
	}

	static bool DecodeNewsDxt5(const unsigned char* data, std::size_t size, unsigned int width, unsigned int height, std::vector<unsigned char>& pixels)
	{
		const auto blockCountX = (width + 3) / 4;
		const auto blockCountY = (height + 3) / 4;
		const auto expectedSize = static_cast<std::size_t>(blockCountX) * static_cast<std::size_t>(blockCountY) * 16u;

		if (size < expectedSize)
		{
			return false;
		}

		pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);

		const auto* source = data;

		for (unsigned int by = 0; by < blockCountY; ++by)
		{
			for (unsigned int bx = 0; bx < blockCountX; ++bx)
			{
				unsigned char alphaPalette[8]{};
				BuildAlphaPalette(source[0], source[1], alphaPalette);

				std::uint64_t alphaMask = 0;

				for (unsigned int i = 0; i < 6; ++i)
				{
					alphaMask |= static_cast<std::uint64_t>(source[2 + i]) << (8 * i);
				}

				unsigned char palette[4][3]{};
				BuildColorPalette(ReadLe16(source + 8), ReadLe16(source + 10), palette);

				const auto colorMask = ReadLe32(source + 12);

				for (unsigned int y = 0; y < 4; ++y)
				{
					for (unsigned int x = 0; x < 4; ++x)
					{
						const auto px = (bx * 4) + x;
						const auto py = (by * 4) + y;

						if (px >= width || py >= height)
						{
							continue;
						}

						const auto i = (y * 4) + x;
						const auto colorIndex = (colorMask >> (i * 2)) & 0x3;
						const auto alphaIndex = (alphaMask >> (i * 3)) & 0x7;
						auto* const target = pixels.data() + (((py * width) + px) * 4);

						target[0] = palette[colorIndex][2];
						target[1] = palette[colorIndex][1];
						target[2] = palette[colorIndex][0];
						target[3] = alphaPalette[alphaIndex];
					}
				}

				source += 16;
			}
		}

		return true;
	}

	static Game::GfxImage* CreateFilledImage(const std::string& name, const std::vector<unsigned char>& pixels, unsigned int width, unsigned int height)
	{
		auto* const image = Materials::CreateImage(name, width, height, 1, writableImageFlags, D3DFMT_A8R8G8B8);

		if (!image->texture.map)
		{
			Materials::DeleteImage(image);
			return nullptr;
		}

		D3DLOCKED_RECT lockedRect{};

		if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0)))
		{
			Materials::DeleteImage(image);
			return nullptr;
		}

		const auto sourceStride = width * 4;
		auto* const target = static_cast<unsigned char*>(lockedRect.pBits);

		for (unsigned int y = 0; y < height; ++y)
		{
			std::memcpy(target + (y * lockedRect.Pitch), pixels.data() + (y * sourceStride), sourceStride);
		}

		image->texture.map->UnlockRect(0);

		return image;
	}

	Game::Material* Materials::Create(const std::string& name, Game::GfxImage* image)
	{
		if (name.empty() || !image)
		{
			return nullptr;
		}

		std::lock_guard lock(tableMutex);

		if (auto* const existing = GetRuntimeMaterial(name))
		{
			return existing;
		}

		const Game::Material* base = nullptr;

		for (const char* baseName : { "white", "ui_cursor", "default" })
		{
			base = static_cast<Game::Material*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, baseName));

			if (base)
			{
				break;
			}
		}

		if (!base || !base->textureTable || !base->textureCount)
		{
			return nullptr;
		}

		auto* const allocator = Utils::Memory::GetAllocator();

		auto* const material = allocator->Allocate<Game::Material>();
		auto* const texture = allocator->Allocate<Game::MaterialTextureDef>();

		*material = *base;
		*texture = base->textureTable[0];

		material->info.name = allocator->DuplicateString(name);
		material->info.textureAtlasColumnCount = 1;
		material->info.textureAtlasRowCount = 1;
		ConfigureAnimatedAtlas(material);

		material->textureCount = 1;
		material->textureTable = texture;

		texture->nameHash = Game::R_HashString("colorMap");
		texture->nameStart = 'c';
		texture->nameEnd = 'p';
		texture->u.image = image;

		materialTable.push_back(material);
		runtimeMaterials[name] = material;

		return material;
	}

	void Materials::ConfigureAnimatedAtlas(Game::Material* material)
	{
		if (!material || !material->info.name)
		{
			return;
		}

		if (_stricmp(material->info.name, "searching_for_player") == 0)
		{
			material->info.textureAtlasRowCount = 4;
			material->info.textureAtlasColumnCount = 8;
		}
	}

	Game::Material* Materials::GetRuntimeMaterial(const std::string& materialName)
	{
		std::lock_guard lock(tableMutex);

		const auto entry = runtimeMaterials.find(materialName);

		if (entry == runtimeMaterials.end())
		{
			return nullptr;
		}

		if (!IsValid(entry->second))
		{
			runtimeMaterials.erase(entry);
			return nullptr;
		}

		return entry->second;
	}

	void Materials::Delete(Game::Material* material, bool deleteImage)
	{
		if (!material)
		{
			return;
		}

		std::lock_guard lock(tableMutex);

		if (deleteImage)
		{
			for (int i = 0; i < material->textureCount; ++i)
			{
				DeleteImage(material->textureTable[i].u.image);
			}
		}

		std::erase_if(runtimeMaterials, [material](const auto& entry)
		{
			return entry.second == material;
		});

		std::erase(materialTable, material);

		auto* const allocator = Utils::Memory::GetAllocator();
		allocator->Free(material->textureTable);
		allocator->Free(material->info.name);
		allocator->Free(material);
	}

	Game::GfxImage* Materials::CreateImage(const std::string& name, unsigned int width, unsigned int height, unsigned int depth, unsigned int flags, D3DFORMAT format)
	{
		auto* const allocator = Utils::Memory::GetAllocator();

		auto* const image = allocator->Allocate<Game::GfxImage>();
		image->name = allocator->DuplicateString(name);

		Game::Image_Setup(image, static_cast<int>(width), static_cast<int>(height), static_cast<int>(depth), flags, format);

		std::lock_guard lock(tableMutex);
		imageTable.push_back(image);

		return image;
	}

	Game::GfxImage* Materials::LoadPreviewImage(const std::string& name)
	{
		auto* const allocator = Utils::Memory::GetAllocator();
		auto* const image = allocator->Allocate<Game::GfxImage>();

		const Game::XAssetEntry* entry = nullptr;

		if (FastFiles::Ready())
		{
			entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_IMAGE, name.data());
		}

		const auto* const loaded = entry ? static_cast<const Game::GfxImage*>(entry->asset.header.data) : nullptr;

		if (loaded && !loaded->delayLoadPixels && loaded->texture.basemap)
		{
			*image = *loaded;
			image->texture.basemap->AddRef();
			image->cardMemory.platform[0] = 0;
			image->cardMemory.platform[1] = 0;
			image->name = allocator->DuplicateString(name);
		}
		else
		{
			image->name = allocator->DuplicateString(name);
			image->semantic = textureSemanticColorMap;
			image->category = Game::IMG_CATEGORY_LOAD_FROM_FILE;
			image->noPicmip = true;

			if (!Game::Image_LoadFromFileWithReader(image, Game::FS_FOpenFileReadCurrentThread))
			{
				if (image->texture.basemap)
				{
					Game::Image_Release(image);
				}

				allocator->Free(image->name);
				allocator->Free(image);
				return nullptr;
			}
		}

		std::lock_guard lock(tableMutex);
		imageTable.push_back(image);

		return image;
	}

	void Materials::DeleteImage(Game::GfxImage* image)
	{
		if (!image)
		{
			return;
		}

		Game::Image_Release(image);

		std::lock_guard lock(tableMutex);
		std::erase(imageTable, image);

		auto* const allocator = Utils::Memory::GetAllocator();
		allocator->Free(image->name);
		allocator->Free(image);
	}

	void Materials::DeleteAll()
	{
		std::lock_guard lock(tableMutex);

		const std::vector<Game::Material*> materials = materialTable;

		for (auto* material : materials)
		{
			Delete(material);
		}

		const std::vector<Game::GfxImage*> images = imageTable;

		for (auto* image : images)
		{
			DeleteImage(image);
		}
	}

	bool Materials::IsValid(Game::Material* material)
	{
		if (!material || !material->textureCount || !material->textureTable)
		{
			return false;
		}

		for (int i = 0; i < material->textureCount; ++i)
		{
			const Game::GfxImage* image = material->textureTable[i].u.image;

			if (!image || !image->texture.map)
			{
				return false;
			}
		}

		return true;
	}

	bool Materials::DecodeImageBytesToBGRA(const std::string& imageData, std::vector<unsigned char>& pixels, unsigned int& width, unsigned int& height)
	{
		pixels.clear();
		width = 0;
		height = 0;

		if (imageData.empty() || imageData.size() > 2 * 1024 * 1024 || !EnsureGdiPlusStarted())
		{
			return false;
		}

		const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, imageData.size());

		if (!memory)
		{
			return false;
		}

		void* const memoryData = GlobalLock(memory);

		if (!memoryData)
		{
			GlobalFree(memory);
			return false;
		}

		std::memcpy(memoryData, imageData.data(), imageData.size());
		GlobalUnlock(memory);

		IStream* stream = nullptr;

		if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)) || !stream)
		{
			GlobalFree(memory);
			return false;
		}

		const std::unique_ptr<IStream, void(*)(IStream*)> ownedStream(stream, [](IStream* owned)
		{
			owned->Release();
		});

		Gdiplus::Bitmap bitmap(stream, FALSE);

		if (bitmap.GetLastStatus() != Gdiplus::Ok)
		{
			return false;
		}

		const auto bitmapWidth = bitmap.GetWidth();
		const auto bitmapHeight = bitmap.GetHeight();

		if (!bitmapWidth || !bitmapHeight || bitmapWidth > 1024 || bitmapHeight > 1024)
		{
			return false;
		}

		Gdiplus::Rect rect(0, 0, static_cast<INT>(bitmapWidth), static_cast<INT>(bitmapHeight));
		Gdiplus::BitmapData bitmapData{};

		if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok)
		{
			return false;
		}

		const auto stride = bitmapWidth * 4;
		pixels.resize(static_cast<std::size_t>(stride) * bitmapHeight);

		const auto* const sourceBase = static_cast<const unsigned char*>(bitmapData.Scan0);

		for (unsigned int y = 0; y < bitmapHeight; ++y)
		{
			std::memcpy(pixels.data() + (y * stride), sourceBase + (static_cast<std::ptrdiff_t>(y) * bitmapData.Stride), stride);
		}

		bitmap.UnlockBits(&bitmapData);

		width = bitmapWidth;
		height = bitmapHeight;
		return true;
	}

	std::string Materials::ConvertNewsImageBytesToIwi(const std::string& imageData)
	{
		std::vector<unsigned char> pixels;
		unsigned int width = 0;
		unsigned int height = 0;

		if (!DecodeImageBytesToBGRA(imageData, pixels, width, height))
		{
			return {};
		}

		const auto dxtData = EncodeNewsDxt5(pixels, width, height);

		if (dxtData.empty())
		{
			return {};
		}

		NewsIwiHeader header{};
		header.format = iwiFormatDxt5;
		header.flags = 0x03;
		header.width = static_cast<std::uint16_t>(width);
		header.height = static_cast<std::uint16_t>(height);
		header.depth = 1;

		const auto fileSize = static_cast<std::uint32_t>(iwiDataOffset + dxtData.size());

		std::string iwi = "IWi";
		iwi.reserve(fileSize);
		iwi.push_back(0x08);
		iwi.append(4, '\0');
		iwi.append(reinterpret_cast<const char*>(&header), sizeof(header));

		for (int i = 0; i < 4; ++i)
		{
			WriteLe32(iwi, fileSize);
		}

		iwi.append(dxtData);

		return iwi;
	}

	Game::GfxImage* Materials::CreateNewsImageFromIwiBytes(const std::string& imageName, const std::string& iwiData)
	{
		if (imageName.empty() || iwiData.size() < iwiDataOffset)
		{
			return nullptr;
		}

		if (!iwiData.starts_with("IWi") || static_cast<unsigned char>(iwiData[3]) != 0x08)
		{
			return nullptr;
		}

		NewsIwiHeader header{};
		std::memcpy(&header, iwiData.data() + iwiHeaderOffset, sizeof(header));

		if (header.width == 0 || header.height == 0 || header.depth != 1)
		{
			return nullptr;
		}

		const auto* const data = reinterpret_cast<const unsigned char*>(iwiData.data() + iwiDataOffset);
		const auto dataSize = iwiData.size() - iwiDataOffset;
		std::vector<unsigned char> pixels;

		if (header.format == iwiFormatArgb32)
		{
			const auto expectedSize = static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height) * 4u;

			if (dataSize < expectedSize)
			{
				return nullptr;
			}

			pixels.assign(data, data + expectedSize);
		}
		else if (header.format == iwiFormatDxt5 || header.format == iwiFormatDxt5Alt)
		{
			if (!DecodeNewsDxt5(data, dataSize, header.width, header.height, pixels))
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}

		return CreateFilledImage(imageName, pixels, header.width, header.height);
	}

	static Game::Material* FindValidMaterial(const std::vector<Game::Material*>& materials, const std::string& materialName)
	{
		for (auto* const material : materials)
		{
			if (material && material->info.name && materialName == material->info.name && Materials::IsValid(material))
			{
				return material;
			}
		}

		return nullptr;
	}

	Game::Material* Materials::CreateNewsMaterialFromIwiBytes(const std::string& materialName, const std::string& iwiData)
	{
		if (materialName.empty() || iwiData.empty())
		{
			return nullptr;
		}

		if (auto* const existing = GetRuntimeMaterial(materialName))
		{
			return existing;
		}

		{
			std::lock_guard lock(tableMutex);

			if (auto* const existing = FindValidMaterial(materialTable, materialName))
			{
				return existing;
			}
		}

		auto* const image = CreateNewsImageFromIwiBytes(materialName + "_image", iwiData);

		if (!image)
		{
			return nullptr;
		}

		return Create(materialName, image);
	}

	Game::GfxImage* Materials::CreateNewsImageFromImageBytes(const std::string& imageName, const std::string& imageData)
	{
		if (imageName.empty() || imageData.empty())
		{
			return nullptr;
		}

		std::vector<unsigned char> pixels;
		unsigned int width = 0;
		unsigned int height = 0;

		if (!DecodeImageBytesToBGRA(imageData, pixels, width, height))
		{
			return nullptr;
		}

		return CreateFilledImage(imageName, pixels, width, height);
	}

	Game::Material* Materials::CreateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData)
	{
		if (materialName.empty() || imageData.empty())
		{
			return nullptr;
		}

		if (auto* const existing = GetRuntimeMaterial(materialName))
		{
			return existing;
		}

		{
			std::lock_guard lock(tableMutex);

			if (auto* const existing = FindValidMaterial(materialTable, materialName))
			{
				return existing;
			}
		}

		auto* const image = CreateNewsImageFromImageBytes(materialName + "_image", imageData);

		if (!image)
		{
			return nullptr;
		}

		return Create(materialName, image);
	}

	Game::Material* Materials::UpdateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData)
	{
		if (materialName.empty() || imageData.empty())
		{
			return nullptr;
		}

		auto* const image = CreateNewsImageFromImageBytes(std::format("{}_image_{}", materialName, Game::Sys_Milliseconds()), imageData);

		if (!image)
		{
			return nullptr;
		}

		auto* const material = GetRuntimeMaterial(materialName);

		if (!material)
		{
			return Create(materialName, image);
		}

		material->textureTable[0].u.image = image;

		return material;
	}

	Materials::~Materials()
	{
		DeleteAll();

		if (gdiPlusToken)
		{
			Gdiplus::GdiplusShutdown(gdiPlusToken);
			gdiPlusToken = 0;
		}
	}

	struct ImageBufferSite
	{
		std::uintptr_t address;
		std::uint8_t bytes[6];
		std::size_t length;
	};

	static const ImageBufferSite imageBufferSites[] =
	{
		{ 0x140069AD5, { 0x3D, 0x00, 0x00, 0xC0, 0x00 }, 5 },
		{ 0x140069B17, { 0xB9, 0x00, 0x00, 0xC0, 0x00 }, 5 },
		{ 0x140069C74, { 0x81, 0xF9, 0x00, 0x00, 0xC0, 0x00 }, 6 },
		{ 0x140069CB6, { 0xB9, 0x00, 0x00, 0xC0, 0x00 }, 5 },
	};

	constexpr std::uint32_t imageBufferSize = 0x4000000;

	static void RaiseImageBuffer()
	{
		for (const auto& site : imageBufferSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, site.bytes, site.length))
			{
				Logger::Error("materials: 0x{:X} does not read as expected, the image load buffer stays 12 MiB\n", site.address);
				return;
			}
		}

		for (const auto& site : imageBufferSites)
		{
			Utils::Hook::Set<std::uint32_t>(site.address + site.length - sizeof(std::uint32_t), imageBufferSize);
		}
	}

	static void IgnoreMissingImages()
	{
		const bool areCallsIntact = Utils::Hook::BranchesTo(R_DelayLoadImage_Com_ErrorCall, Com_Error, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Load_Texture_Com_ErrorCall, Com_Error, HOOK_CALL);

		if (!areCallsIntact)
		{
			Logger::Error("materials: the image load errors do not read as expected, a missing image still drops to the menu\n");
			return;
		}

		Utils::Hook::Nop(R_DelayLoadImage_Com_ErrorCall, 5);
		Utils::Hook::Nop(Load_Texture_Com_ErrorCall, 5);
	}

	static void DB_LoadXAssets_UnloadOverrides_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Material_DirtyTechniqueSetOverrides))();
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Material_OverrideTechniqueSets))();
	}

	Materials::Materials()
	{
		EnsureGdiPlusStarted();
		RaiseImageBuffer();
		IgnoreMissingImages();

		AssetHandler::OnFind(Game::ASSET_TYPE_MATERIAL, [](unsigned int, const std::string& name) -> void*
		{
			return GetRuntimeMaterial(name);
		});

		AssetHandler::OnLoad(Game::ASSET_TYPE_MATERIAL, [](unsigned int, void* asset, const std::string&, bool*)
		{
			ConfigureAnimatedAtlas(static_cast<Game::Material*>(asset));
		});

		Renderer::OnDeviceRecoveryBegin([]
		{
			Game::Dvar_SetFromStringByName("zw3_ui_news_image", "");
			Game::Dvar_SetFromStringByName("zw3_ui_news_has_image", "0");

			std::lock_guard lock(tableMutex);

			for (auto* const image : imageTable)
			{
				Game::Image_Release(image);
				image->texture.map = nullptr;
			}

			runtimeMaterials.clear();
		});

		const bool areAtlasCallsIntact = std::ranges::all_of(atlasCalls, [](std::uintptr_t site)
		{
			return Utils::Hook::BranchesTo(site, Material_Process2DTextureCoordsForAtlasing, HOOK_CALL);
		});

		if (!areAtlasCallsIntact)
		{
			Logger::Error("materials: a call to Material_Process2DTextureCoordsForAtlasing does not read as expected, the spinner plays 32 frames and the map vote previews are stretched, not cropped\n");
		}
		else
		{
			bool isSeated = true;

			for (std::size_t i = 0; i < std::size(atlasCalls); ++i)
			{
				isSeated = atlasHooks[i].Initialize(atlasCalls[i], reinterpret_cast<void*>(ProcessAtlasCoords), HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			if (!isSeated)
			{
				for (auto& hook : atlasHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("materials: could not seat the atlas hooks, the spinner plays 32 frames and the map vote previews are stretched, not cropped\n");
			}
			else
			{
				for (auto& hook : atlasHooks)
				{
					hook.Quick();
				}
			}
		}

		if (!Utils::Hook::BranchesTo(DB_LoadXAssets_UnloadDirtyCall, Material_DirtyTechniqueSetOverrides, HOOK_CALL))
		{
			Logger::Error("materials: DB_LoadXAssets' unload no longer calls Material_DirtyTechniqueSetOverrides, a render thread sort after an unload can still read a freed remapped techset\n");
		}
		else if (!unloadOverridesHook.Initialize(DB_LoadXAssets_UnloadDirtyCall, reinterpret_cast<void*>(DB_LoadXAssets_UnloadOverrides_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("materials: could not seat the unload override hook, a render thread sort after an unload can still read a freed remapped techset\n");
		}

		if (!Utils::Hook::MatchesBytes(Image_LoadFromFileWithReader_VersionTest, versionTest, sizeof(versionTest)))
		{
			Logger::Error("materials: Image_LoadFromFileWithReader does not read as expected, version 9 images stay refused\n");
			return;
		}

		if (!versionHook.Initialize(Image_LoadFromFileWithReader_VersionTest, IwiVersionStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("materials: could not seat the image version hook, version 9 images stay refused\n");
			return;
		}

		Utils::Hook::Nop(Image_LoadFromFileWithReader_VersionTest + 5, sizeof(versionTest) - 5);
	}
}
