#include "STDInclude.hpp"

#include "IStringTable.hpp"

namespace Assets
{
	void IStringTable::SaveStringTableCellArray(Components::ZoneBuilder::Zone* builder, const Game::StringTableCell* values, int count)
	{
		auto* const buffer = builder->GetBuffer();
		auto* const destValues = buffer->Dest<Game::X86::StringTableCell>();

		for (int i = 0; i < count; ++i)
		{
			const auto record = Game::X86::Convert(values[i]);
			buffer->Save(&record);
		}

		for (int i = 0; i < count; ++i)
		{
			if (values[i].string)
			{
				buffer->SaveString(values[i].string);
				Utils::Stream::ClearPointer(&destValues[i].string);
			}
		}
	}

	void IStringTable::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.stringTable;
		auto* const dest = buffer->Dest<Game::X86::StringTable>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->values)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			SaveStringTableCellArray(builder, asset->values, asset->columnCount * asset->rowCount);
			Utils::Stream::ClearPointer(&dest->values);
		}

		buffer->PopBlock();
	}
}
