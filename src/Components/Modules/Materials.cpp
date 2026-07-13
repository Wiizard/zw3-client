#include <cmath>
#include <array>
#include <algorithm>

#include "Materials.hpp"
#include "AssetHandler.hpp"

#pragma push_macro("min")
#pragma push_macro("max")

#ifndef min
#define min std::min
#endif

#ifndef max
#define max std::max
#endif

#include <gdiplus.h>

#pragma pop_macro("max")
#pragma pop_macro("min")

#pragma comment(lib, "gdiplus.lib")

namespace Components
{
	Utils::Hook Materials::ImageVersionCheckHook;

	std::vector<Game::GfxImage*> Materials::ImageTable;
	std::vector<Game::Material*> Materials::MaterialTable;
	std::unordered_map<std::string, Game::Material*> RuntimeMaterialTable;

	namespace
	{

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


		ULONG_PTR GdiPlusToken = 0;

		bool EnsureGdiPlusStarted()
		{
			if (GdiPlusToken)
			{
				return true;
			}

			Gdiplus::GdiplusStartupInput input;
			return Gdiplus::GdiplusStartup(&GdiPlusToken, &input, nullptr) == Gdiplus::Ok;
		}
		std::uint16_t ColorTo565(const unsigned char* bgra)
		{
			const auto r = bgra[2] >> 3;
			const auto g = bgra[1] >> 2;
			const auto b = bgra[0] >> 3;
			return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
		}

		void ColorFrom565(std::uint16_t c, unsigned char out[3])
		{
			out[0] = static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31);
			out[1] = static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63);
			out[2] = static_cast<unsigned char>((c & 31) * 255 / 31);
		}

		void WriteLe16(std::string& out, std::uint16_t value)
		{
			out.push_back(static_cast<char>(value & 0xFF));
			out.push_back(static_cast<char>((value >> 8) & 0xFF));
		}

		void WriteLe32(std::string& out, std::uint32_t value)
		{
			out.push_back(static_cast<char>(value & 0xFF));
			out.push_back(static_cast<char>((value >> 8) & 0xFF));
			out.push_back(static_cast<char>((value >> 16) & 0xFF));
			out.push_back(static_cast<char>((value >> 24) & 0xFF));
		}

		std::uint16_t ReadLe16(const unsigned char* data)
		{
			return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
		}

		std::uint32_t ReadLe32(const unsigned char* data)
		{
			return static_cast<std::uint32_t>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
		}

		std::string EncodeNewsDxt5(const std::vector<unsigned char>& pixels, unsigned int width, unsigned int height)
		{
			std::string out;

			const auto blockCountX = (width + 3) / 4;
			const auto blockCountY = (height + 3) / 4;
			out.reserve(blockCountX * blockCountY * 16);

			for (auto by = 0u; by < blockCountY; ++by)
			{
				for (auto bx = 0u; bx < blockCountX; ++bx)
				{
					unsigned char block[16][4]{};

					for (auto y = 0u; y < 4; ++y)
					{
						for (auto x = 0u; x < 4; ++x)
						{
							const auto sx = std::min((bx * 4) + x, width - 1);
							const auto sy = std::min((by * 4) + y, height - 1);
							std::memcpy(block[(y * 4) + x], pixels.data() + (((sy * width) + sx) * 4), 4);
						}
					}

					unsigned char minA = 255;
					unsigned char maxA = 0;

					for (const auto& px : block)
					{
						minA = std::min(minA, px[3]);
						maxA = std::max(maxA, px[3]);
					}

					out.push_back(static_cast<char>(maxA));
					out.push_back(static_cast<char>(minA));

					unsigned char alphaPalette[8]{};
					alphaPalette[0] = maxA;
					alphaPalette[1] = minA;

					if (maxA > minA)
					{
						for (auto i = 1; i <= 6; ++i)
						{
							alphaPalette[i + 1] = static_cast<unsigned char>(((7 - i) * maxA + i * minA) / 7);
						}
					}
					else
					{
						for (auto i = 1; i <= 4; ++i)
						{
							alphaPalette[i + 1] = static_cast<unsigned char>(((5 - i) * maxA + i * minA) / 5);
						}

						alphaPalette[6] = 0;
						alphaPalette[7] = 255;
					}

					std::uint64_t alphaMask = 0;

					for (auto i = 0u; i < 16; ++i)
					{
						auto bestIndex = 0u;
						auto bestDistance = 999999u;

						for (auto a = 0u; a < 8; ++a)
						{
							const auto distance = static_cast<unsigned int>(std::abs(static_cast<int>(block[i][3]) - static_cast<int>(alphaPalette[a])));

							if (distance < bestDistance)
							{
								bestDistance = distance;
								bestIndex = a;
							}
						}

						alphaMask |= (static_cast<std::uint64_t>(bestIndex) << (i * 3));
					}

					for (auto i = 0u; i < 6; ++i)
					{
						out.push_back(static_cast<char>((alphaMask >> (8 * i)) & 0xFF));
					}

					const unsigned char* minColor = block[0];
					const unsigned char* maxColor = block[0];
					auto minLuma = 999999;
					auto maxLuma = -1;

					for (const auto& px : block)
					{
						const auto luma = static_cast<int>(px[2]) * 299 + static_cast<int>(px[1]) * 587 + static_cast<int>(px[0]) * 114;

						if (luma < minLuma)
						{
							minLuma = luma;
							minColor = px;
						}

						if (luma > maxLuma)
						{
							maxLuma = luma;
							maxColor = px;
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
					ColorFrom565(color0, palette[0]);
					ColorFrom565(color1, palette[1]);

					for (auto c = 0; c < 3; ++c)
					{
						palette[2][c] = static_cast<unsigned char>((2 * palette[0][c] + palette[1][c]) / 3);
						palette[3][c] = static_cast<unsigned char>((palette[0][c] + 2 * palette[1][c]) / 3);
					}

					std::uint32_t colorMask = 0;

					for (auto i = 0u; i < 16; ++i)
					{
						auto bestIndex = 0u;
						auto bestDistance = 0xFFFFFFFFu;

						for (auto c = 0u; c < 4; ++c)
						{
							const auto db = static_cast<int>(block[i][0]) - static_cast<int>(palette[c][2]);
							const auto dg = static_cast<int>(block[i][1]) - static_cast<int>(palette[c][1]);
							const auto dr = static_cast<int>(block[i][2]) - static_cast<int>(palette[c][0]);
							const auto distance = static_cast<unsigned int>((dr * dr) + (dg * dg) + (db * db));

							if (distance < bestDistance)
							{
								bestDistance = distance;
								bestIndex = c;
							}
						}

						colorMask |= (bestIndex << (i * 2));
					}

					WriteLe32(out, colorMask);
				}
			}

			return out;
		}

		bool DecodeNewsDxt5(const unsigned char* data, std::size_t size, unsigned int width, unsigned int height, std::vector<unsigned char>& pixels)
		{
			const auto blockCountX = (width + 3) / 4;
			const auto blockCountY = (height + 3) / 4;
			const auto expectedSize = static_cast<std::size_t>(blockCountX) * static_cast<std::size_t>(blockCountY) * 16u;

			if (size < expectedSize)
			{
				return false;
			}

			pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);

			auto* src = data;

			for (auto by = 0u; by < blockCountY; ++by)
			{
				for (auto bx = 0u; bx < blockCountX; ++bx)
				{
					const auto a0 = src[0];
					const auto a1 = src[1];

					unsigned char alphaPalette[8]{};
					alphaPalette[0] = a0;
					alphaPalette[1] = a1;

					if (a0 > a1)
					{
						for (auto i = 1; i <= 6; ++i)
						{
							alphaPalette[i + 1] = static_cast<unsigned char>(((7 - i) * a0 + i * a1) / 7);
						}
					}
					else
					{
						for (auto i = 1; i <= 4; ++i)
						{
							alphaPalette[i + 1] = static_cast<unsigned char>(((5 - i) * a0 + i * a1) / 5);
						}

						alphaPalette[6] = 0;
						alphaPalette[7] = 255;
					}

					std::uint64_t alphaMask = 0;
					for (auto i = 0u; i < 6; ++i)
					{
						alphaMask |= static_cast<std::uint64_t>(src[2 + i]) << (8 * i);
					}

					const auto color0 = ReadLe16(src + 8);
					const auto color1 = ReadLe16(src + 10);
					const auto colorMask = ReadLe32(src + 12);

					unsigned char palette[4][3]{};
					ColorFrom565(color0, palette[0]);
					ColorFrom565(color1, palette[1]);

					for (auto c = 0; c < 3; ++c)
					{
						palette[2][c] = static_cast<unsigned char>((2 * palette[0][c] + palette[1][c]) / 3);
						palette[3][c] = static_cast<unsigned char>((palette[0][c] + 2 * palette[1][c]) / 3);
					}

					for (auto y = 0u; y < 4; ++y)
					{
						for (auto x = 0u; x < 4; ++x)
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
							auto* dst = pixels.data() + (((py * width) + px) * 4);

							dst[0] = palette[colorIndex][2];
							dst[1] = palette[colorIndex][1];
							dst[2] = palette[colorIndex][0];
							dst[3] = alphaPalette[alphaIndex];
						}
					}

					src += 16;
				}
			}

			return true;
		}


	}

	Game::Material* Materials::Create(const std::string& name, Game::GfxImage* image)
	{
		if (name.empty() || !image)
		{
			return nullptr;
		}

		if (auto* existing = Materials::GetRuntimeMaterial(name))
		{
			return existing;
		}

		RuntimeMaterialTable.erase(name);

		auto* baseMaterial = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, "white").material;

		if (!baseMaterial)
		{
			baseMaterial = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, "ui_cursor").material;
		}

		if (!baseMaterial)
		{
			baseMaterial = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, "default").material;
		}

		if (!baseMaterial || !baseMaterial->textureTable || !baseMaterial->textureCount)
		{
			return nullptr;
		}

		auto* material = Utils::Memory::GetAllocator()->allocate<Game::Material>();
		std::memcpy(material, baseMaterial, sizeof(Game::Material));

		material->info.name = Utils::Memory::GetAllocator()->duplicateString(name);
		material->info.sortKey = baseMaterial->info.sortKey;
		material->info.textureAtlasColumnCount = 1;
		material->info.textureAtlasRowCount = 1;

		material->textureCount = 1;
		material->textureTable = Utils::Memory::GetAllocator()->allocate<Game::MaterialTextureDef>();
		std::memcpy(material->textureTable, baseMaterial->textureTable, sizeof(Game::MaterialTextureDef));

		material->textureTable->nameHash = Game::R_HashString("colorMap");
		material->textureTable->nameStart = 'c';
		material->textureTable->nameEnd = 'p';
		material->textureTable->u.image = image;

		Materials::MaterialTable.push_back(material);
		RuntimeMaterialTable[name] = material;
		AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_MATERIAL, { material });
		AssetHandler::ExposeTemporaryAssets(true);

		return material;
	}


	Game::Material* Materials::GetRuntimeMaterial(const std::string& materialName)
	{
		const auto entry = RuntimeMaterialTable.find(materialName);

		if (entry == RuntimeMaterialTable.end())
		{
			return nullptr;
		}

		if (!Materials::IsValid(entry->second))
		{
			RuntimeMaterialTable.erase(entry);
			return nullptr;
		}

		return entry->second;
	}

	void Materials::Delete(Game::Material* material, bool deleteImage)
	{
		if (!material) return;

		if (deleteImage)
		{
			for (char i = 0; i < material->textureCount; ++i)
			{
				Materials::DeleteImage(material->textureTable[i].u.image);
			}
		}

		Utils::Memory::GetAllocator()->free(material->textureTable);
		Utils::Memory::GetAllocator()->free(material->info.name);
		Utils::Memory::GetAllocator()->free(material);

		for (auto entry = RuntimeMaterialTable.begin(); entry != RuntimeMaterialTable.end();)
		{
			if (entry->second == material)
			{
				entry = RuntimeMaterialTable.erase(entry);
			}
			else
			{
				++entry;
			}
		}

		auto mat = std::find(Materials::MaterialTable.begin(), Materials::MaterialTable.end(), material);
		if (mat != Materials::MaterialTable.end())
		{
			Materials::MaterialTable.erase(mat);
		}
	}

	Game::GfxImage* Materials::CreateImage(const std::string& name, unsigned int width, unsigned int height, unsigned int depth, unsigned int flags, _D3DFORMAT format)
	{
		Game::GfxImage* image = Utils::Memory::GetAllocator()->allocate<Game::GfxImage>();
		image->name = Utils::Memory::GetAllocator()->duplicateString(name);

		Game::Image_Setup(image, width, height, depth, flags, format);

		Materials::ImageTable.push_back(image);
		AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_IMAGE, { image });
		AssetHandler::ExposeTemporaryAssets(true);

		return image;
	}

	void Materials::DeleteImage(Game::GfxImage* image)
	{
		if (!image) return;

		Game::Image_Release(image);

		Utils::Memory::GetAllocator()->free(image->name);
		Utils::Memory::GetAllocator()->free(image);

		auto img = std::find(Materials::ImageTable.begin(), Materials::ImageTable.end(), image);
		if (img != Materials::ImageTable.end())
		{
			Materials::ImageTable.erase(img);
		}
	}

	void Materials::DeleteAll()
	{
		std::vector<Game::Material*> materials;
		Utils::Merge(&materials, Materials::MaterialTable);
		Materials::MaterialTable.clear();

		for (auto& material : materials)
		{
			Materials::Delete(material);
		}

		std::vector<Game::GfxImage*> images;
		Utils::Merge(&images, Materials::ImageTable);
		Materials::ImageTable.clear();

		for (auto& image : images)
		{
			Materials::DeleteImage(image);
		}
	}

	bool Materials::IsValid(Game::Material* material)
	{
		if (!material || !material->textureCount || !material->textureTable) return false;

		for (char i = 0; i < material->textureCount; ++i)
		{
			if (!material->textureTable[i].u.image || !material->textureTable[i].u.image->texture.map)
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

		if (imageData.empty() || imageData.size() > 2 * 1024 * 1024)
		{
			return false;
		}

		if (!EnsureGdiPlusStarted())
		{
			return false;
		}

		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, imageData.size());
		if (!memory)
		{
			return false;
		}

		void* memoryData = GlobalLock(memory);
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

		Gdiplus::Bitmap bitmap(stream, FALSE);
		if (bitmap.GetLastStatus() != Gdiplus::Ok)
		{
			stream->Release();
			return false;
		}

		width = bitmap.GetWidth();
		height = bitmap.GetHeight();

		if (!width || !height || width > 1024 || height > 1024)
		{
			stream->Release();
			pixels.clear();
			width = 0;
			height = 0;
			return false;
		}

		Gdiplus::Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
		Gdiplus::BitmapData bitmapData{};

		if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok)
		{
			stream->Release();
			pixels.clear();
			width = 0;
			height = 0;
			return false;
		}

		const auto stride = width * 4;
		pixels.resize(stride * height);

		const auto* srcBase = static_cast<const unsigned char*>(bitmapData.Scan0);
		for (unsigned int y = 0; y < height; ++y)
		{
			const auto* src = srcBase + (y * bitmapData.Stride);
			auto* dst = pixels.data() + (y * stride);
			std::memcpy(dst, src, stride);
		}

		bitmap.UnlockBits(&bitmapData);
		stream->Release();

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

		auto dxtData = EncodeNewsDxt5(pixels, width, height);

		if (dxtData.empty())
		{
			return {};
		}

		NewsIwiHeader header{};
		header.format = 0x0D;
		header.flags = 0x03;
		header.width = static_cast<std::uint16_t>(width);
		header.height = static_cast<std::uint16_t>(height);
		header.depth = 1;

		const auto fileSize = static_cast<std::uint32_t>(4 + 4 + sizeof(header) + 16 + dxtData.size());

		std::string iwi;
		iwi.reserve(fileSize);

		iwi.push_back('I');
		iwi.push_back('W');
		iwi.push_back('i');
		iwi.push_back(0x08);

		for (auto i = 0; i < 4; ++i)
		{
			iwi.push_back(0);
		}

		iwi.append(reinterpret_cast<const char*>(&header), sizeof(header));

		for (auto i = 0; i < 4; ++i)
		{
			iwi.append(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));
		}

		iwi.append(dxtData);

		return iwi;
	}

	Game::GfxImage* Materials::CreateNewsImageFromIwiBytes(const std::string& imageName, const std::string& iwiData)
	{
		if (imageName.empty() || iwiData.size() < 4 + 4 + sizeof(NewsIwiHeader) + 16)
		{
			return nullptr;
		}

		if (iwiData[0] != 'I' || iwiData[1] != 'W' || iwiData[2] != 'i' || static_cast<unsigned char>(iwiData[3]) != 0x08)
		{
			return nullptr;
		}

		const auto headerOffset = 4 + 4;
		const auto dataOffset = headerOffset + sizeof(NewsIwiHeader) + 16;

		NewsIwiHeader header{};
		std::memcpy(&header, iwiData.data() + headerOffset, sizeof(header));

		if (header.width == 0 || header.height == 0 || header.depth != 1)
		{
			return nullptr;
		}

		std::vector<unsigned char> pixels;

		if (header.format == 0x01)
		{
			const auto expectedSize = static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height) * 4u;
			if (iwiData.size() < dataOffset + expectedSize)
			{
				return nullptr;
			}

			pixels.assign(reinterpret_cast<const unsigned char*>(iwiData.data() + dataOffset), reinterpret_cast<const unsigned char*>(iwiData.data() + dataOffset + expectedSize));
		}
		else if (header.format == 0x0D || header.format == 0x0E)
		{
			if (!DecodeNewsDxt5(reinterpret_cast<const unsigned char*>(iwiData.data() + dataOffset), iwiData.size() - dataOffset, header.width, header.height, pixels))
			{
				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}

		auto* image = Materials::CreateImage(imageName, header.width, header.height, 1, 0x1000003, D3DFMT_A8R8G8B8);
		if (!image || !image->texture.map)
		{
			if (image)
			{
				Materials::DeleteImage(image);
			}

			return nullptr;
		}

		D3DLOCKED_RECT lockedRect{};
		if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0)))
		{
			Materials::DeleteImage(image);
			return nullptr;
		}

		const auto srcStride = static_cast<std::uint32_t>(header.width) * 4u;
		auto* dst = static_cast<unsigned char*>(lockedRect.pBits);

		for (auto y = 0u; y < header.height; ++y)
		{
			std::memcpy(dst + (y * lockedRect.Pitch), pixels.data() + (y * srcStride), srcStride);
		}

		image->texture.map->UnlockRect(0);

		return image;
	}

	Game::Material* Materials::CreateNewsMaterialFromIwiBytes(const std::string& materialName, const std::string& iwiData)
	{
		if (materialName.empty() || iwiData.empty())
		{
			return nullptr;
		}

		if (auto* existing = Materials::GetRuntimeMaterial(materialName))
		{
			return existing;
		}

		for (auto* material : Materials::MaterialTable)
		{
			if (material && material->info.name && materialName == material->info.name && Materials::IsValid(material))
			{
				return material;
			}
		}

		auto* image = Materials::CreateNewsImageFromIwiBytes(materialName + "_image", iwiData);
		if (!image)
		{
			return nullptr;
		}

		return Materials::Create(materialName, image);
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

		auto* image = Materials::CreateImage(imageName, width, height, 1, 0x1000003, D3DFMT_A8R8G8B8);
		if (!image || !image->texture.map)
		{
			if (image)
			{
				Materials::DeleteImage(image);
			}

			return nullptr;
		}

		D3DLOCKED_RECT lockedRect{};
		if (FAILED(image->texture.map->LockRect(0, &lockedRect, nullptr, 0)))
		{
			Materials::DeleteImage(image);
			return nullptr;
		}

		const auto srcStride = width * 4;
		auto* dst = static_cast<unsigned char*>(lockedRect.pBits);

		for (unsigned int y = 0; y < height; ++y)
		{
			std::memcpy(dst + (y * lockedRect.Pitch), pixels.data() + (y * srcStride), srcStride);
		}

		image->texture.map->UnlockRect(0);

		return image;
	}

	Game::Material* Materials::CreateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData)
	{
		if (materialName.empty() || imageData.empty())
		{
			return nullptr;
		}

		if (auto* existing = Materials::GetRuntimeMaterial(materialName))
		{
			return existing;
		}

		for (auto* material : Materials::MaterialTable)
		{
			if (material && material->info.name && materialName == material->info.name && Materials::IsValid(material))
			{
				return material;
			}
		}

		auto* image = Materials::CreateNewsImageFromImageBytes(materialName + "_image", imageData);
		if (!image)
		{
			return nullptr;
		}

		return Materials::Create(materialName, image);
	}

	Game::Material* Materials::UpdateNewsMaterialFromImageBytes(const std::string& materialName, const std::string& imageData)
	{
		if (materialName.empty() || imageData.empty())
		{
			return nullptr;
		}

		auto* image = Materials::CreateNewsImageFromImageBytes(materialName + "_image_" + Utils::String::VA("%i", Game::Sys_Milliseconds()), imageData);
		if (!image || !image->texture.map)
		{
			return nullptr;
		}

		auto* material = Materials::GetRuntimeMaterial(materialName);
		if (!material)
		{
			material = Materials::Create(materialName, image);
			return material;
		}

		if (!material->textureTable)
		{
			return nullptr;
		}

		material->textureTable[0].u.image = image;
		AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_MATERIAL, { material });
		AssetHandler::ExposeTemporaryAssets(true);

		return material;
	}


	__declspec(naked) void Materials::ImageVersionCheck()
	{
		__asm
		{
			cmp eax, 9
			je returnSafely

			jmp Materials::ImageVersionCheckHook.original

			returnSafely :
			mov al, 1
				add esp, 18h
				retn
		}
	}

	int Materials::WriteDeathMessageIcon(char* string, int offset, Game::Material* material)
	{
		if (!material)
		{
			material = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_MATERIAL, "default").material;
		}

		int length = strlen(material->info.name);
		string[offset++] = static_cast<char>(length);

		strncpy_s(string + offset, 1024 - offset, material->info.name, length);

		return offset + length;
	}

	__declspec(naked) void Materials::DeathMessageStub()
	{
		__asm
		{
			push eax
			pushad

			push edx
			push eax
			push ecx

			call Materials::WriteDeathMessageIcon
			add esp, 0Ch

			mov[esp + 20h], eax
			popad
			pop eax

			add esp, 8h
			retn
		}
	}

	int Materials::FormatImagePath(char* buffer, size_t size, int, int, const char* image)
	{
#if 0
		if (Utils::String::StartsWith(image, "preview_"))
		{
			std::string newImage = image;
			Utils::String::Replace(newImage, "preview_", "loadscreen_");

			if (FileSystem::FileReader(fmt::sprintf("images/%s.iwi", newImage.data())).exists())
			{
				image = Utils::String::VA("%s", newImage.data());
			}
		}
#endif

		return _snprintf_s(buffer, size, size, "images/%s.iwi", image);
	}

	int Materials::MaterialComparePrint(Game::Material* m1, Game::Material* m2)
	{
		return Utils::Hook::Call<int(Game::Material*, Game::Material*)>(0x5235B0)(m1, m2);
	}

#ifdef DEBUG
	void Materials::DumpImageCfg(int, const char*, const char* material)
	{
		Materials::DumpImageCfgPath(0, nullptr, Utils::String::VA("images/%s.iwi", material));
	}

	void Materials::DumpImageCfgPath(int, const char*, const char* material)
	{
		FILE* fp = nullptr;
		if (!fopen_s(&fp, "dump.cfg", "a") && fp)
		{
			fprintf(fp, "dumpraw %s\n", material);
			fclose(fp);
		}
	}

#endif

	Materials::Materials()
	{
		EnsureGdiPlusStarted();

		Materials::ImageVersionCheckHook.initialize(0x53A456, Materials::ImageVersionCheck, HOOK_CALL)->install();

		Utils::Hook(0x5A30D9, Materials::DeathMessageStub, HOOK_JUMP).install()->quick();

		Utils::Hook(0x53AC19, Materials::FormatImagePath, HOOK_CALL).install()->quick();

		Utils::Hook::Set<void*>(0x523894, Materials::MaterialComparePrint);

		AssetHandler::ExposeTemporaryAssets(true);

		AssetHandler::OnFind(Game::ASSET_TYPE_MATERIAL, [](Game::XAssetType, const std::string& filename)
			{
				if (auto* material = Materials::GetRuntimeMaterial(filename))
				{
					return Game::XAssetHeader{ material };
				}

				return Game::XAssetHeader{ nullptr };
			});

		AssetHandler::OnFind(Game::ASSET_TYPE_IMAGE, [](Game::XAssetType, const std::string& filename)
			{
				for (auto* image : Materials::ImageTable)
				{
					if (image && image->name && !_stricmp(image->name, filename.data()))
					{
						return Game::XAssetHeader{ image };
					}
				}

				return Game::XAssetHeader{ nullptr };
			});

#ifdef DEBUG
		if (Flags::HasFlag("dump"))
		{
			Utils::Hook(0x51F5AC, Materials::DumpImageCfg, HOOK_CALL).install()->quick();
			Utils::Hook(0x51F4C4, Materials::DumpImageCfg, HOOK_CALL).install()->quick();
			Utils::Hook(0x53AC62, Materials::DumpImageCfgPath, HOOK_CALL).install()->quick();
		}
		else
		{
			Utils::Hook::Nop(0x51F5AC, 5);
			Utils::Hook::Nop(0x51F4C4, 5);
		}
#else
		Utils::Hook::Nop(0x51F5AC, 5);
		Utils::Hook::Nop(0x51F4C4, 5);
#endif

		Renderer::OnDeviceRecoveryBegin([]()
			{
				Dvar::Var("zw3_ui_news_image").set("");
				Dvar::Var("zw3_ui_news_has_image").set(false);

				for (auto& image : Materials::ImageTable)
				{
					Game::Image_Release(image);
					image->texture.map = nullptr;
				}

				RuntimeMaterialTable.clear();
			});
	}

	Materials::~Materials()
	{
		Materials::DeleteAll();

		Materials::ImageVersionCheckHook.uninstall();

		if (GdiPlusToken)
		{
			Gdiplus::GdiplusShutdown(GdiPlusToken);
			GdiPlusToken = 0;
		}
	}
}
