#pragma once

namespace Components
{
	class StructuredData : public Component
	{
	public:
		enum PlayerDataType
		{
			FEATURES,
			WEAPONS,
			ATTACHEMENTS,
			CHALLENGES,
			CAMOS,
			PERKS,
			KILLSTREAKS,
			ACCOLADES,
			CARDICONS,
			CARDTITLES,
			CARDNAMEPLATES,
			TEAMS,
			GAMETYPES,

			COUNT
		};

		StructuredData();

	private:
		static Utils::Hook updateVersionHook;
		static Utils::Memory::Allocator memAllocator;
		static const char* enumTranslation[COUNT];

		static bool UpdateVersionOffsets(Game::StructuredDataDefSet* set, Game::StructuredDataBuffer* buffer, Game::StructuredDataDef* oldDef);

		static void PatchPlayerDataEnum(Game::StructuredDataDef* data, PlayerDataType type, const std::vector<std::string>& entries);
		static void PatchAdditionalData(Game::StructuredDataDef* data, const std::unordered_map<std::string, std::string>& patches);
		static void PatchCustomClassLimit(Game::StructuredDataDef* data, int count);
	};
}
