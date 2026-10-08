#pragma once

namespace Components
{
	class Huffman : public Component
	{
	public:
		Huffman();

		static bool IsInstalled();

	private:
		static bool isInstalled;
		static Utils::Hook callSiteHooks[5];
		static Utils::Hook readStub;
		static Utils::Hook writeStub;

		static int Decompress_Hook(const unsigned char* from, int fromSize, unsigned char* to, int maxOut);

		static int CL_WritePacket_Compress(bool flag, const unsigned char* from, unsigned char* to, int fromSize);
		static int CL_Record_f_Compress(bool flag, const unsigned char* from, unsigned char* to, int fromSize);
		static int SV_SendMessageToClient_Compress(bool flag, const unsigned char* from, unsigned char* to, int fromSize);

		static int MSG_ReadBitsCompress_Stub(const unsigned char* from, int fromSize, unsigned char* to, int maxOut);
		static int MSG_WriteBitsCompress_Stub(bool flag, const unsigned char* from, unsigned char* to, int fromSize);
	};
}
