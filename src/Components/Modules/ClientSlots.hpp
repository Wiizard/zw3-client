#pragma once

namespace Components
{
	class ClientSlots : public Component
	{
	public:
		static constexpr std::size_t BASEGAME_CLIENT_LIMIT = 18;

		static constexpr std::size_t CLIENT_LIMIT = 127;

		static constexpr std::size_t CLIENT_MASK_WORDS = (CLIENT_LIMIT + 31) / 32;

		static constexpr std::size_t ENTITY_LIMIT = 2048;

		ClientSlots();

		static std::size_t SentClientCount();

		static bool IsServerWide();

		static Game::clientInfo_t* CgameClientInfo(std::size_t index);
		static std::size_t CgameClientCount();

		static std::size_t ScriptChildCapacity();

		static std::size_t ScriptChildPools();
		static std::size_t ScriptChildHighest();

		static std::size_t ScriptParentCapacity();
		static std::size_t ScriptParentsUsed();

		static std::size_t ScriptMemoryFreeBlocks();

		static int FirstSlotForNewBot();

	private:
		static bool MoveSlotArrays();
	};
}
