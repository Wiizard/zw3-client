#include "STDInclude.hpp"

#include "StringTable.hpp"
#include "AssetHandler.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"

namespace Components
{
	std::mutex StringTable::tablesMutex;
	std::unordered_map<std::string, Game::StringTable*> StringTable::tables;
	Utils::Memory::Allocator StringTable::allocator;

	constexpr std::uintptr_t StringTable_HashString = 0x140280F80;

	constexpr unsigned int ASSET_TYPE_STRINGTABLE = 0x25;

	Game::StringTable* StringTable::LoadObject(const std::string& filename)
	{
		FileSystem::File rawTable(filename, FileSystem::GetCurrentThread());

		if (!rawTable.Exists())
		{
			tables[filename] = nullptr;
			return nullptr;
		}

		const Utils::CSV parsed(rawTable.GetBuffer(), false, false);
		const auto hashString = reinterpret_cast<int(*)(const char*)>(Utils::Hook::Rebase(StringTable_HashString));

		auto* const table = allocator.Allocate<Game::StringTable>();
		table->name = allocator.DuplicateString(filename);
		table->columnCount = static_cast<int>(parsed.GetColumns());
		table->rowCount = static_cast<int>(parsed.GetRows());

		const std::size_t cellCount = static_cast<std::size_t>(table->columnCount) * static_cast<std::size_t>(table->rowCount);

		if (cellCount > 0)
		{
			table->values = allocator.AllocateArray<Game::StringTableCell>(cellCount);
		}

		for (int row = 0; row < table->rowCount; ++row)
		{
			for (int column = 0; column < table->columnCount; ++column)
			{
				const std::string value = parsed.GetElementAt(static_cast<std::size_t>(row), static_cast<std::size_t>(column));
				Game::StringTableCell* const cell = &table->values[row * table->columnCount + column];

				cell->hash = hashString(value.data());
				cell->string = allocator.DuplicateString(value);
			}
		}

		tables[filename] = table;
		return table;
	}

	StringTable::StringTable()
	{
		const bool isAnswering = AssetHandler::OnFind(ASSET_TYPE_STRINGTABLE, [](unsigned int, const std::string& name) -> void*
		{
			const std::string filename = Utils::String::ToLower(name);
			const std::lock_guard lock(tablesMutex);

			const auto cached = tables.find(filename);

			if (cached != tables.end())
			{
				return cached->second;
			}

			return LoadObject(filename);
		});

		if (!isAnswering)
		{
			Logger::Error("stringtable: lookups cannot be answered, IW4x's own tables will not replace the stock ones\n");
		}
	}
}
