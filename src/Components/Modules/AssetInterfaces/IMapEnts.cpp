#include "STDInclude.hpp"

#include "IMapEnts.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

namespace Assets
{
	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("mapents/{}.ents", baseName);
	}

	static void FindMatchedAssets(const std::string& entityString, const std::regex& catcher, Game::XAssetType type, Components::ZoneBuilder::Zone* builder)
	{
		std::smatch match;
		auto searchStart = entityString.cbegin();

		while (std::regex_search(searchStart, entityString.cend(), match, catcher))
		{
			Components::AssetHandler::FindAssetForZone(type, match[1].str(), builder);
			searchStart = match.suffix().first;
		}
	}

	void IMapEnts::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return;
		}

		const auto& contents = file.GetBuffer();
		auto* const allocator = builder->GetAllocator();

		auto* const entities = allocator->Allocate<Game::MapEnts>();

		entities->stageCount = 1;
		entities->stages = allocator->Allocate<Game::Stage>();
		entities->stages[0].name = "stage 0";
		entities->stages[0].triggerIndex = 0x400;
		entities->stages[0].sunPrimaryLightIndex = 0x1;

		entities->name = allocator->DuplicateString(name);
		entities->entityString = allocator->DuplicateString(contents);
		entities->numEntityChars = static_cast<int>(contents.size()) + 1;

		static const std::regex modelCatcher("model\"? \"([^\\*\\?].*)\"");
		static const std::regex weaponCatcher("weaponinfo\"? \"([^*?].*)\"");

		FindMatchedAssets(contents, modelCatcher, Game::ASSET_TYPE_XMODEL, builder);
		FindMatchedAssets(contents, weaponCatcher, Game::ASSET_TYPE_WEAPON, builder);

		header->mapEnts = entities;
	}

	void IMapEnts::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		Utils::Entities memEnts(header.mapEnts->entityString, static_cast<std::size_t>(header.mapEnts->numEntityChars));

		for (const auto& model : memEnts.GetModels())
		{
			builder->LoadAssetByName(Game::ASSET_TYPE_XMODEL, model, false);
		}

		for (const auto& weapon : memEnts.GetWeapons())
		{
			builder->LoadAssetByName(Game::ASSET_TYPE_WEAPON, weapon, false);
		}
	}

	void IMapEnts::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::MapEnts, 44);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.mapEnts;
		auto* const dest = buffer->Dest<Game::X86::MapEnts>();

		const auto entities = Game::X86::Convert(*asset);
		buffer->Save(&entities);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->entityString)
		{
			buffer->Save(asset->entityString, static_cast<std::size_t>(asset->numEntityChars));
			Utils::Stream::ClearPointer(&dest->entityString);
		}

		AssertSize(Game::X86::MapTriggers, 24);

		if (asset->trigger.models)
		{
			AssertSize(Game::X86::TriggerModel, 8);
			static_assert(sizeof(Game::TriggerModel) == sizeof(Game::X86::TriggerModel));

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->trigger.models, asset->trigger.count);
			Utils::Stream::ClearPointer(&dest->trigger.models);
		}

		if (asset->trigger.hulls)
		{
			AssertSize(Game::X86::TriggerHull, 32);
			static_assert(sizeof(Game::TriggerHull) == sizeof(Game::X86::TriggerHull));

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->trigger.hulls, asset->trigger.hullCount);
			Utils::Stream::ClearPointer(&dest->trigger.hulls);
		}

		if (asset->trigger.slabs)
		{
			AssertSize(Game::X86::TriggerSlab, 20);
			static_assert(sizeof(Game::TriggerSlab) == sizeof(Game::X86::TriggerSlab));

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->trigger.slabs, asset->trigger.slabCount);
			Utils::Stream::ClearPointer(&dest->trigger.slabs);
		}

		if (asset->stages)
		{
			AssertSize(Game::X86::Stage, 20);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destStages = buffer->Dest<Game::X86::Stage>();
			const auto stageCount = static_cast<unsigned char>(asset->stageCount);

			for (unsigned char i = 0; i < stageCount; ++i)
			{
				const auto stage = Game::X86::Convert(asset->stages[i]);
				buffer->Save(&stage);
			}

			for (unsigned char i = 0; i < stageCount; ++i)
			{
				if (asset->stages[i].name)
				{
					buffer->SaveString(asset->stages[i].name);
					Utils::Stream::ClearPointer(&destStages[i].name);
				}
			}

			Utils::Stream::ClearPointer(&dest->stages);
		}

		buffer->PopBlock();
	}

	void IMapEnts::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.mapEnts;

		if (!asset->name || !asset->entityString)
		{
			return;
		}

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(asset->name));

		if (!Utils::IO::WriteFile(path, asset->entityString))
		{
			Components::Logger::Error("Dumping map ents '{}' failed, could not write {}\n", asset->name, path);
		}
	}
}
