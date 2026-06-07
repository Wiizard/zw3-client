#include "Huffman.hpp"

namespace Components
{
	namespace
	{
		int DecompressClientMessage(const unsigned char* from, unsigned char* to, int fromSize)
		{
			return Utils::Huffman::Decompress(from, to, fromSize, 0x800);
		}

		int DecompressServerMessage(const unsigned char* from, unsigned char* to, int fromSize)
		{
			return Utils::Huffman::Decompress(from, to, fromSize, 0x20000);
		}

		int CompressClientPacket(bool, const unsigned char* from, unsigned char* to, int fromSize)
		{
			return Utils::Huffman::Compress(from, to, fromSize, 0x800);
		}

		int CompressLargeMessage(bool, const unsigned char* from, unsigned char* to, int fromSize)
		{
			return Utils::Huffman::Compress(from, to, fromSize, 0x20000);
		}

		int BlockOriginalReadBitsCompress(const unsigned char*, unsigned char*, int)
		{
			Logger::Warning(Game::CON_CHANNEL_DONT_FILTER, "Cannot use the original MSG_ReadBitsCompress function!\n");
			return 0;
		}

		int BlockOriginalWriteBitsCompress(bool, const unsigned char*, unsigned char*, int)
		{
			Logger::Warning(Game::CON_CHANNEL_DONT_FILTER, "Cannot use the original MSG_WriteBitsCompress function!\n");
			return 0;
		}
	}

	Huffman::Huffman()
	{
		Utils::Hook(0x414D92, DecompressClientMessage, HOOK_CALL).install()->quick(); // SV_ExecuteClientMessage
		Utils::Hook(0x4A9F56, DecompressServerMessage, HOOK_CALL).install()->quick(); // CL_ParseServerMessage
		Utils::Hook(0x411C16, CompressClientPacket, HOOK_CALL).install()->quick(); // CL_WritePacket
		Utils::Hook(0x5A85A1, CompressLargeMessage, HOOK_CALL).install()->quick(); // CL_Record_f
		Utils::Hook(0x48FEDD, CompressLargeMessage, HOOK_CALL).install()->quick(); // SV_SendMessageToClient

		// Disable original (de)compression functions

		Utils::Hook(Game::MSG_ReadBitsCompress, BlockOriginalReadBitsCompress, HOOK_JUMP).install()->quick();

		Utils::Hook(Game::MSG_WriteBitsCompress, BlockOriginalWriteBitsCompress, HOOK_JUMP).install()->quick();

		isInitialized = true;
	}
}
