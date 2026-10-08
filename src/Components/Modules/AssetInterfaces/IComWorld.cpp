#include "STDInclude.hpp"

#include "IComWorld.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#define IW4X_COMMAP_VERSION 0

namespace Assets
{
	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("comworld/{}.iw4xComWorld", baseName);
	}

	static Game::ComWorld* TryReadComWorld(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();
		Utils::Stream::Reader reader(allocator, file.GetBuffer());

		try
		{
			const auto magic = reader.Read<std::uint64_t>();

			if (std::memcmp(&magic, "IW4xComW", 8))
			{
				Components::Logger::Error("Reading comworld '{}' failed, header is invalid!\n", name);
				return nullptr;
			}

			const auto version = reader.Read<std::int32_t>();

			if (version > IW4X_COMMAP_VERSION)
			{
				Components::Logger::Error("Reading comworld '{}' failed, expected version is {}, but it was {}!\n", name, IW4X_COMMAP_VERSION, version);
				return nullptr;
			}

			const auto* const stored = reader.ReadObject<Game::X86::ComWorld>();

			auto* const asset = allocator->Allocate<Game::ComWorld>();
			*asset = Game::X86::Convert(*stored);

			if (stored->name)
			{
				asset->name = reader.ReadCString();
			}

			if (stored->primaryLights)
			{
				const auto* const storedLights = reader.ReadArray<Game::X86::ComPrimaryLight>(asset->primaryLightCount);
				asset->primaryLights = allocator->AllocateArray<Game::ComPrimaryLight>(asset->primaryLightCount);

				for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
				{
					auto* const light = &asset->primaryLights[i];
					*light = Game::X86::Convert(storedLights[i]);

					if (!storedLights[i].defName)
					{
						continue;
					}

					light->defName = reader.ReadCString();

					if (!Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_LIGHT_DEF, light->defName, builder).data)
					{
						Components::Logger::Error("Reading comworld '{}' failed, light def '{}' could not be found\n", name, light->defName);
						return nullptr;
					}
				}
			}

			return asset;
		}
		catch (const std::exception& ex)
		{
			Components::Logger::Error("Reading comworld '{}' failed: {}\n", name, ex.what());
			return nullptr;
		}
	}

	void IComWorld::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->comWorld = TryReadComWorld(name, builder);
	}

	void IComWorld::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.comWorld;

		if (!asset->primaryLights)
		{
			return;
		}

		for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
		{
			if (asset->primaryLights[i].defName)
			{
				builder->LoadAssetByName(Game::ASSET_TYPE_LIGHT_DEF, asset->primaryLights[i].defName, false);
			}
		}
	}

	void IComWorld::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::ComWorld, 16);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.comWorld;
		auto* const dest = buffer->Dest<Game::X86::ComWorld>();

		const auto world = Game::X86::Convert(*asset);
		buffer->Save(&world);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->primaryLights)
		{
			AssertSize(Game::X86::ComPrimaryLight, 68);
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destLights = buffer->Dest<Game::X86::ComPrimaryLight>();

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				const auto light = Game::X86::Convert(asset->primaryLights[i]);
				buffer->Save(&light);
			}

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				if (asset->primaryLights[i].defName)
				{
					buffer->SaveString(asset->primaryLights[i].defName);
					Utils::Stream::ClearPointer(&destLights[i].defName);
				}
			}

			Utils::Stream::ClearPointer(&dest->primaryLights);
		}

		buffer->PopBlock();
	}

	void IComWorld::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.comWorld;

		if (!asset->name)
		{
			return;
		}

		Utils::Stream buffer;
		buffer.Save("IW4xComW", 8);
		buffer.SaveObject(IW4X_COMMAP_VERSION);

		auto world = Game::X86::Convert(*asset);
		Utils::Stream::ClearPointer(&world.name);

		if (asset->primaryLights)
		{
			Utils::Stream::ClearPointer(&world.primaryLights);
		}

		buffer.Save(&world);
		buffer.SaveString(asset->name);

		if (asset->primaryLights)
		{
			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				auto light = Game::X86::Convert(asset->primaryLights[i]);

				if (asset->primaryLights[i].defName)
				{
					Utils::Stream::ClearPointer(&light.defName);
				}

				buffer.Save(&light);
			}

			for (unsigned int i = 0; i < asset->primaryLightCount; ++i)
			{
				if (asset->primaryLights[i].defName)
				{
					buffer.SaveString(asset->primaryLights[i].defName);
				}
			}
		}

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(asset->name));

		if (!Utils::IO::WriteFile(path, buffer.ToBuffer()))
		{
			Components::Logger::Error("Dumping comworld '{}' failed, could not write {}\n", asset->name, path);
		}
	}
}
