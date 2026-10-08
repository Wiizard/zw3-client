#include "STDInclude.hpp"

#include "IStructuredDataDefSet.hpp"

namespace Assets
{
	void IStructuredDataDefSet::SaveStructuredDataEnumArray(const Game::StructuredDataEnum* enums, int numEnums, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const destEnums = buffer->Dest<Game::X86::StructuredDataEnum>();

		for (int i = 0; i < numEnums; ++i)
		{
			const auto record = Game::X86::Convert(enums[i]);
			buffer->Save(&record);
		}

		for (int i = 0; i < numEnums; ++i)
		{
			auto* const destEnum = &destEnums[i];
			const auto* const enumDef = &enums[i];

			if (!enumDef->entries)
			{
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destEntries = buffer->Dest<Game::X86::StructuredDataEnumEntry>();

			for (int j = 0; j < enumDef->entryCount; ++j)
			{
				const auto record = Game::X86::Convert(enumDef->entries[j]);
				buffer->Save(&record);
			}

			for (int j = 0; j < enumDef->entryCount; ++j)
			{
				if (enumDef->entries[j].string)
				{
					buffer->SaveString(enumDef->entries[j].string);
					Utils::Stream::ClearPointer(&destEntries[j].string);
				}
			}

			Utils::Stream::ClearPointer(&destEnum->entries);
		}
	}

	void IStructuredDataDefSet::SaveStructuredDataStructArray(const Game::StructuredDataStruct* structs, int numStructs, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const destStructs = buffer->Dest<Game::X86::StructuredDataStruct>();

		for (int i = 0; i < numStructs; ++i)
		{
			const auto record = Game::X86::Convert(structs[i]);
			buffer->Save(&record);
		}

		for (int i = 0; i < numStructs; ++i)
		{
			auto* const destStruct = &destStructs[i];
			const auto* const structDef = &structs[i];

			if (!structDef->properties)
			{
				continue;
			}

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destProperties = buffer->Dest<Game::X86::StructuredDataStructProperty>();

			for (int j = 0; j < structDef->propertyCount; ++j)
			{
				const auto record = Game::X86::Convert(structDef->properties[j]);
				buffer->Save(&record);
			}

			for (int j = 0; j < structDef->propertyCount; ++j)
			{
				if (structDef->properties[j].name)
				{
					buffer->SaveString(structDef->properties[j].name);
					Utils::Stream::ClearPointer(&destProperties[j].name);
				}
			}

			Utils::Stream::ClearPointer(&destStruct->properties);
		}
	}

	void IStructuredDataDefSet::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.structuredDataDefSet;
		auto* const dest = buffer->Dest<Game::X86::StructuredDataDefSet>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->defs)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destDataArray = buffer->Dest<Game::X86::StructuredDataDef>();

			for (unsigned int i = 0; i < asset->defCount; ++i)
			{
				const auto defRecord = Game::X86::Convert(asset->defs[i]);
				buffer->Save(&defRecord);
			}

			for (unsigned int i = 0; i < asset->defCount; ++i)
			{
				auto* const destData = &destDataArray[i];
				const auto* const data = &asset->defs[i];

				if (data->enums)
				{
					buffer->Align(Utils::Stream::ALIGN_4);

					SaveStructuredDataEnumArray(data->enums, data->enumCount, builder);
					Utils::Stream::ClearPointer(&destData->enums);
				}

				if (data->structs)
				{
					buffer->Align(Utils::Stream::ALIGN_4);

					SaveStructuredDataStructArray(data->structs, data->structCount, builder);
					Utils::Stream::ClearPointer(&destData->structs);
				}

				if (data->indexedArrays)
				{
					buffer->Align(Utils::Stream::ALIGN_4);

					for (int j = 0; j < data->indexedArrayCount; ++j)
					{
						const auto arrayRecord = Game::X86::Convert(data->indexedArrays[j]);
						buffer->Save(&arrayRecord);
					}

					Utils::Stream::ClearPointer(&destData->indexedArrays);
				}

				if (data->enumedArrays)
				{
					buffer->Align(Utils::Stream::ALIGN_4);

					for (int j = 0; j < data->enumedArrayCount; ++j)
					{
						const auto arrayRecord = Game::X86::Convert(data->enumedArrays[j]);
						buffer->Save(&arrayRecord);
					}

					Utils::Stream::ClearPointer(&destData->enumedArrays);
				}
			}

			Utils::Stream::ClearPointer(&dest->defs);
		}

		buffer->PopBlock();
	}
}
