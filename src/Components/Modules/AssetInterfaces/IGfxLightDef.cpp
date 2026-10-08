#include "STDInclude.hpp"

#include "IGfxLightDef.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Assets
{
	constexpr auto lightMagic = "IW4xLit";
	constexpr char lightVersion = '0';

	static bool TryReadCString(const std::string& data, std::size_t& at, std::string& out)
	{
		const auto end = data.find('\0', at);

		if (end == std::string::npos)
		{
			return false;
		}

		out.assign(data, at, end - at);
		at = end + 1;
		return true;
	}

	void IGfxLightDef::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File lightFile(std::format("lights/{}.iw4xLight", name));

		if (!lightFile.Exists())
		{
			return;
		}

		const auto& contents = lightFile.GetBuffer();
		constexpr auto headerSize = 8 + sizeof(Game::X86::GfxLightDef);

		if (contents.size() < headerSize || std::memcmp(contents.data(), lightMagic, 7) != 0)
		{
			Components::Logger::Fatal("Reading light '{}' failed, header is invalid!", name);
		}

		if (contents[7] != lightVersion)
		{
			Components::Logger::Fatal("Reading light '{}' failed, expected version is {}, but it was {}!", name, lightVersion, contents[7]);
		}

		Game::X86::GfxLightDef record{};
		std::memcpy(&record, contents.data() + 8, sizeof(record));

		auto* const allocator = builder->GetAllocator();
		auto* const asset = allocator->Allocate<Game::GfxLightDef>();
		*asset = Game::X86::Convert(record);

		std::size_t at = headerSize;

		if (record.name)
		{
			std::string lightName;

			if (!TryReadCString(contents, at, lightName))
			{
				Components::Logger::Fatal("Reading light '{}' failed, its name is cut short!", name);
			}

			asset->name = allocator->DuplicateString(lightName);
		}

		if (record.attenuation.image)
		{
			std::string imageName;

			if (!TryReadCString(contents, at, imageName))
			{
				Components::Logger::Fatal("Reading light '{}' failed, its image name is cut short!", name);
			}

			asset->attenuation.image = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_IMAGE, imageName, builder).image;
		}

		header->lightDef = asset;
	}

	void IGfxLightDef::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.lightDef;

		if (asset->attenuation.image)
		{
			builder->LoadAsset(Game::ASSET_TYPE_IMAGE, asset->attenuation.image);
		}
	}

	void IGfxLightDef::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.lightDef;
		auto* const dest = buffer->Dest<Game::X86::GfxLightDef>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->attenuation.image)
		{
			dest->attenuation.image = builder->SaveSubAsset(Game::ASSET_TYPE_IMAGE, asset->attenuation.image);
		}

		buffer->PopBlock();
	}

	void IGfxLightDef::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.lightDef;

		if (!asset->name)
		{
			Components::Logger::Fatal("asset->name was unexpectedly null! could not find the asset for it");
		}

		auto record = Game::X86::Convert(*asset);
		Utils::Stream::ClearPointer(&record.name);

		std::string output(lightMagic);
		output.push_back(lightVersion);

		if (asset->attenuation.image)
		{
			Utils::Stream::ClearPointer(&record.attenuation.image);
		}

		output.append(reinterpret_cast<const char*>(&record), sizeof(record));
		output.append(asset->name);
		output.push_back('\0');

		if (asset->attenuation.image)
		{
			output.append(asset->attenuation.image->name);
			output.push_back('\0');

			Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_IMAGE, asset->attenuation.image });
		}

		Utils::IO::WriteFile(std::format("{}/lights/{}.iw4xLight", Components::ZoneBuilder::GetDumpingZonePath(), asset->name), output);
	}
}
