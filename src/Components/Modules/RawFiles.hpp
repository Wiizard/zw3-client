#pragma once

namespace Components
{
	class RawFiles : public Component
	{
	public:
		RawFiles();

		static char* ReadRawFile(const char* filename, char* buf, int size);

		static const char* LastScriptRead();
		static char* GetMenuBuffer(const char* filename);

		static const char* Com_LoadInfoString_Hk(const char* fileName, const char* fileDesc, const char* ident, char* loadBuffer);

	private:
		static char* Com_LoadInfoString_LoadObj(const char* fileName, const char* fileDesc, const char* ident, char* loadBuffer);
		static char* Scr_ReadFile_Stub(const char* filename, const char* extFilename);
	};
}
