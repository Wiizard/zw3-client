#pragma once

namespace Components
{
	class StringTable : public Component
	{
	public:
		StringTable();

	private:
		static std::mutex tablesMutex;
		static std::unordered_map<std::string, Game::StringTable*> tables;
		static Utils::Memory::Allocator allocator;

		static Game::StringTable* LoadObject(const std::string& filename);
	};
}
