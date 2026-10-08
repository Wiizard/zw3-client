#include "STDInclude.hpp"

#include "Huffman.hpp"
#include "Logger.hpp"

namespace Components
{
	bool Huffman::isInstalled = false;
	Utils::Hook Huffman::callSiteHooks[5];
	Utils::Hook Huffman::readStub;
	Utils::Hook Huffman::writeStub;

	constexpr std::uintptr_t CL_ParseServerMessageCall = 0x140100D39;
	constexpr std::uintptr_t SV_ExecuteClientMessageCall = 0x140238111;
	constexpr std::uintptr_t CL_WritePacketCall = 0x1400F79B5;
	constexpr std::uintptr_t CL_Record_fCall = 0x1400FD208;
	constexpr std::uintptr_t SV_SendMessageToClientCall = 0x1402424A2;

	constexpr std::uintptr_t MSG_ReadBitsCompress = 0x140201FB0;
	constexpr std::uintptr_t MSG_WriteBitsCompress = 0x1402029E0;

	constexpr int clientBufferSize = 0x800;
	constexpr int serverBufferSize = 0x20000;

	int Huffman::Decompress_Hook(const unsigned char* from, int fromSize, unsigned char* to, int maxOut)
	{
		return Utils::Huffman::Decompress(from, to, fromSize, maxOut);
	}

	int Huffman::CL_WritePacket_Compress(bool, const unsigned char* from, unsigned char* to, int fromSize)
	{
		return Utils::Huffman::Compress(from, to, fromSize, clientBufferSize);
	}

	int Huffman::CL_Record_f_Compress(bool, const unsigned char* from, unsigned char* to, int fromSize)
	{
		return Utils::Huffman::Compress(from, to, fromSize, serverBufferSize);
	}

	int Huffman::SV_SendMessageToClient_Compress(bool, const unsigned char* from, unsigned char* to, int fromSize)
	{
		return Utils::Huffman::Compress(from, to, fromSize, serverBufferSize);
	}

	int Huffman::MSG_ReadBitsCompress_Stub(const unsigned char*, int, unsigned char*, int)
	{
		Logger::Warning("Cannot use the original MSG_ReadBitsCompress function!\n");
		return 0;
	}

	int Huffman::MSG_WriteBitsCompress_Stub(bool, const unsigned char*, unsigned char*, int)
	{
		Logger::Warning("Cannot use the original MSG_WriteBitsCompress function!\n");
		return 0;
	}

	bool Huffman::IsInstalled()
	{
		return isInstalled;
	}

	Huffman::Huffman()
	{
		int failed = 0;

		failed += !callSiteHooks[0].Initialize(CL_ParseServerMessageCall,
			Decompress_Hook, HOOK_CALL)->Install()->IsInstalled();
		failed += !callSiteHooks[1].Initialize(SV_ExecuteClientMessageCall,
			Decompress_Hook, HOOK_CALL)->Install()->IsInstalled();
		failed += !callSiteHooks[2].Initialize(CL_WritePacketCall,
			CL_WritePacket_Compress, HOOK_CALL)->Install()->IsInstalled();
		failed += !callSiteHooks[3].Initialize(CL_Record_fCall,
			CL_Record_f_Compress, HOOK_CALL)->Install()->IsInstalled();
		failed += !callSiteHooks[4].Initialize(SV_SendMessageToClientCall,
			SV_SendMessageToClient_Compress, HOOK_CALL)->Install()->IsInstalled();

		failed += !readStub.Initialize(MSG_ReadBitsCompress,
			MSG_ReadBitsCompress_Stub, HOOK_JUMP)->Install()->IsInstalled();
		failed += !writeStub.Initialize(MSG_WriteBitsCompress,
			MSG_WriteBitsCompress_Stub, HOOK_JUMP)->Install()->IsInstalled();

		if (failed)
		{
			Logger::Error("huffman failed to seat {} of 7 hooks, compression is left alone", failed);
			return;
		}

		for (auto& hook : callSiteHooks)
		{
			hook.Quick();
		}

		readStub.Quick();
		writeStub.Quick();

		isInstalled = true;
	}
}
