#include "STDInclude.hpp"

#include "IGfxImage.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	constexpr auto imageMagic = "IW4xImg";
	constexpr std::uint8_t IW4X_IMG_VERSION = 1;
	constexpr std::uint8_t IMG_FLAG_MAPTYPE_2D = 0;
	constexpr std::size_t loadDefHeaderSize = 16;

	struct LegacyLoadDef
	{
		char levelCount;
		char flags;
		std::int16_t dimensions[3];
		std::int32_t format;
		std::int32_t resourceSize;
	};

	template <typename T>
	static T ReadRaw(const std::string& data, std::size_t at)
	{
		T value{};
		std::memcpy(&value, data.data() + at, sizeof(T));
		return value;
	}

	template <typename T>
	static void AppendRaw(std::string& output, const T& value)
	{
		output.append(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	static Game::GfxImage* ReadSpecialImage(const std::string& name, const std::string& contents, Utils::Memory::Allocator* allocator)
	{
		constexpr std::size_t fieldsAt = 8;
		constexpr std::size_t loadDefAt = fieldsAt + 3 + sizeof(std::int32_t);

		if (contents.size() < loadDefAt || std::memcmp(contents.data(), imageMagic, 7) != 0)
		{
			Components::Logger::Fatal("Reading image '{}' failed, header is invalid!", name);
		}

		const auto version = static_cast<std::uint8_t>(contents[7]);
		const bool isLegacyZeroVersion = version == '0';

		if (!isLegacyZeroVersion && version > IW4X_IMG_VERSION)
		{
			Components::Logger::Fatal("Reading image '{}' failed, image version is too new! expected {} and got {}!", name, IW4X_IMG_VERSION, version);
		}

		auto* const image = allocator->Allocate<Game::GfxImage>();
		image->name = allocator->DuplicateString(name);
		image->mapType = static_cast<unsigned char>(contents[fieldsAt]);
		image->semantic = static_cast<unsigned char>(contents[fieldsAt + 1]);
		image->category = static_cast<unsigned char>(contents[fieldsAt + 2]);

		const auto dataLength = ReadRaw<std::int32_t>(contents, fieldsAt + 3);
		image->cardMemory.platform[0] = dataLength;
		image->cardMemory.platform[1] = dataLength;

		const auto dataSize = static_cast<std::size_t>(std::max(dataLength, 0));
		auto* const loadDef = static_cast<Game::GfxImageLoadDef*>(allocator->Allocate(loadDefHeaderSize + dataSize));
		std::size_t dataAt = 0;

		if (isLegacyZeroVersion)
		{
			dataAt = loadDefAt + loadDefHeaderSize;

			if (contents.size() < dataAt + dataSize)
			{
				Components::Logger::Fatal("Reading image '{}' failed, the file is cut short!", name);
			}

			const auto legacy = ReadRaw<LegacyLoadDef>(contents, loadDefAt);
			loadDef->levelCount = legacy.levelCount;
			loadDef->flags = legacy.flags;
			loadDef->format = legacy.format;
			loadDef->resourceSize = legacy.resourceSize;

			image->width = static_cast<unsigned short>(legacy.dimensions[0]);
			image->height = static_cast<unsigned short>(legacy.dimensions[1]);
			image->depth = static_cast<unsigned short>(legacy.dimensions[2]);
		}
		else
		{
			dataAt = loadDefAt + 1 + sizeof(std::int32_t) + 3 * sizeof(std::uint16_t) + sizeof(std::int32_t);

			if (contents.size() < dataAt + dataSize)
			{
				Components::Logger::Fatal("Reading image '{}' failed, the file is cut short!", name);
			}

			loadDef->levelCount = contents[loadDefAt];
			loadDef->flags = ReadRaw<std::int32_t>(contents, loadDefAt + 1);
			image->width = ReadRaw<std::uint16_t>(contents, loadDefAt + 5);
			image->height = ReadRaw<std::uint16_t>(contents, loadDefAt + 7);
			image->depth = ReadRaw<std::uint16_t>(contents, loadDefAt + 9);
			loadDef->resourceSize = dataLength;
			loadDef->format = ReadRaw<std::int32_t>(contents, loadDefAt + 11);
		}

		std::memset(loadDef->pad, 0, sizeof(loadDef->pad));
		std::memcpy(loadDef->data, contents.data() + dataAt, dataSize);

		if (loadDef->resourceSize != dataLength)
		{
			Components::Logger::Fatal("Resource size doesn't match the data length ({})!\n", name);
		}

		image->texture.loadDef = loadDef;
		image->delayLoadPixels = true;

		return image;
	}

	static Game::GfxImage* ReadIwiImage(const std::string& name, Utils::Memory::Allocator* allocator)
	{
		const auto path = std::format("images/{}.iwi", name);
		Components::FileSystem::File iwiFile(path);

		if (!iwiFile.Exists() || iwiFile.GetBuffer().size() < sizeof(Game::GfxImageFileHeader))
		{
			Components::Logger::Fatal("Loading image '{}' failed!", path);
		}

		const auto iwiHeader = ReadRaw<Game::GfxImageFileHeader>(iwiFile.GetBuffer(), 0);

		if (std::memcmp(iwiHeader.tag, "IWi", 3) != 0 && iwiHeader.version == 8)
		{
			Components::Logger::Fatal("Image is not a valid IWi!");
		}

		auto* const image = allocator->Allocate<Game::GfxImage>();
		image->name = allocator->DuplicateString(name);
		image->category = Game::IMG_CATEGORY_LOAD_FROM_FILE;
		image->mapType = IMG_FLAG_MAPTYPE_2D;
		image->cardMemory.platform[0] = iwiHeader.fileSizeForPicmip[0] - 32;
		image->cardMemory.platform[1] = iwiHeader.fileSizeForPicmip[0] - 32;

		auto* const loadDef = allocator->Allocate<Game::GfxImageLoadDef>();
		loadDef->flags = static_cast<int>(iwiHeader.flags);
		loadDef->levelCount = 0;

		image->width = static_cast<unsigned short>(iwiHeader.dimensions[0]);
		image->height = static_cast<unsigned short>(iwiHeader.dimensions[1]);
		image->depth = static_cast<unsigned short>(iwiHeader.dimensions[2]);

		switch (iwiHeader.format)
		{
		case Game::IMG_FORMAT_BITMAP_RGBA:
			loadDef->format = 21;
			break;

		case Game::IMG_FORMAT_BITMAP_RGB:
			loadDef->format = 20;
			break;

		case Game::IMG_FORMAT_DXT1:
			loadDef->format = 0x31545844;
			break;

		case Game::IMG_FORMAT_DXT3:
			loadDef->format = 0x33545844;
			break;

		case Game::IMG_FORMAT_DXT5:
			loadDef->format = 0x35545844;
			break;

		default:
			break;
		}

		image->texture.loadDef = loadDef;

		return image;
	}

	void IGfxImage::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		auto fileName = name;

		if (!fileName.empty() && fileName[0] == '*')
		{
			fileName.erase(fileName.begin());
		}

		Components::FileSystem::File specialFile(std::format("images/{}.iw4xImage", fileName));

		if (specialFile.Exists())
		{
			header->image = ReadSpecialImage(name, specialFile.GetBuffer(), builder->GetAllocator());
			return;
		}

		if (name.empty() || name[0] == '*')
		{
			return;
		}

		header->image = ReadIwiImage(name, builder->GetAllocator());
	}

	void IGfxImage::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.image;
		auto* const dest = buffer->Dest<Game::X86::GfxImage>();

		auto record = Game::X86::Convert(*asset);
		record.texture.loadDef = 0;
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_TEMP);

		if (asset->texture.loadDef)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			const auto* const loadDef = asset->texture.loadDef;
			buffer->Save(loadDef, loadDefHeaderSize, 1);

			builder->IncrementExternalSize(static_cast<unsigned int>(loadDef->resourceSize));

			if (loadDef->resourceSize > 0)
			{
				buffer->Save(loadDef->data, static_cast<std::size_t>(loadDef->resourceSize));
			}

			Utils::Stream::ClearPointer(&dest->texture.loadDef);
		}

		buffer->PopBlock();
		buffer->PopBlock();
	}

	void IGfxImage::Dump(Game::XAssetHeader header)
	{
		const auto* const image = header.image;
		std::string name = image->name;

		const bool isSpecial = image->category != Game::IMG_CATEGORY_LOAD_FROM_FILE && image->texture.loadDef;

		if (isSpecial)
		{
			if (name[0] == '*')
			{
				name.erase(name.begin());
			}

			const auto* const loadDef = image->texture.loadDef;

			std::string output(imageMagic);
			AppendRaw(output, IW4X_IMG_VERSION);
			AppendRaw(output, image->mapType);
			AppendRaw(output, image->semantic);
			AppendRaw(output, image->category);
			AppendRaw(output, loadDef->resourceSize);
			AppendRaw(output, loadDef->levelCount);
			AppendRaw(output, loadDef->flags);
			AppendRaw(output, image->width);
			AppendRaw(output, image->height);
			AppendRaw(output, image->depth);
			AppendRaw(output, loadDef->format);
			output.append(reinterpret_cast<const char*>(loadDef->data), static_cast<std::size_t>(std::max(loadDef->resourceSize, 0)));

			Utils::IO::WriteFile(std::format("{}/images/{}.iw4xImage", Components::ZoneBuilder::GetDumpingZonePath(), name), output);
			return;
		}

		const auto dumpPath = std::format("{}/images/{}.iwi", Components::ZoneBuilder::GetDumpingZonePath(), name);
		auto contents = Components::FileSystem::File(dumpPath).GetBuffer();

		if (contents.empty())
		{
			contents = Components::FileSystem::File(std::format("images/{}.iwi", name)).GetBuffer();
		}

		if (contents.empty())
		{
			if (name.starts_with("watersetup"))
			{
				return;
			}

			Components::Logger::Print("Image {} not found, mapping to normalmap!\n", name);
			contents = Components::FileSystem::File("images/$identitynormalmap.iwi").GetBuffer();
		}

		if (contents.empty())
		{
			Components::Logger::Fatal("Unable to map to normalmap, this should not happen!\n");
		}

		Utils::IO::WriteFile(dumpPath, contents);
	}
}
