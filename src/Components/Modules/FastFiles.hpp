#pragma once

namespace Components
{
	class FastFiles : public Component
	{
	public:
		FastFiles();

		static void AddZonePath(const std::string& path);
		static std::string Current();
		static bool Ready();
		static bool Exists(const std::string& file);
		static bool IsZombieZoneName(std::string_view zoneName);
		static bool ShouldProtectZone(std::string_view zoneName);
		static void ProtectZoneBuffer(std::string& buffer);

		static void LoadLocalizeZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);

		static float GetFullLoadedFraction();

		static unsigned char ZoneKey[1191];

		static symmetric_CTR CurrentCTR;

	private:
		union Key
		{
			struct
			{
				unsigned char key[24];
				unsigned char iv[16];
			};

			unsigned char data[1];
		};

		static unsigned int CurrentZone;
		static unsigned int MaxZones;

		static bool IsIW4xZone;
		static bool IsZW3Zone;
		static bool StreamRead;

		static char LastByteRead;
		static symmetric_CTR ZW3CTR;
		static bool ZW3CTRInitialized;

		static Dvar::Var g_loadingInitialZones;

		static Key CurrentKey;
		static std::vector<std::string> ZonePaths;
		static const char* GetZoneLocation(const char* file);
		static void LoadInitialZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);
		static void LoadDLCUIZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);
		static void LoadGfxZones(Game::XZoneInfo *zoneInfo, unsigned int zoneCount, int sync);

		static void ReadHeaderStub(unsigned int* header, int size);
		static void ReadVersionStub(unsigned int* version, int size);

		static void ReadXFileHeader(void* buffer, int size);
		static bool IsPrivateZW3Module();
		static void InitZW3Crypto(const unsigned char* nonce);
		static void ResetZW3Crypto();

		static void AuthLoadInitCrypto();
		static int AuthLoadInflateCompare(unsigned char* buffer, int length, unsigned char* ivValue);
		static void AuthLoadInflateDecryptBase();
		static void AuthLoadInflateDecryptBaseFunc(unsigned char* buffer);

		static void LoadZonesStub(Game::XZoneInfo *zoneInfo, unsigned int zoneCount);

		static void ReadXFile(void* buffer, int size);
		static void ReadXFileStub(char* buffer, int size);

#ifdef DEBUG
		static void LogStreamRead(int len);
#endif

		static void Load_XSurfaceArray(int atStreamStart, int count);

		static void DB_BuildOSPath_FromSource_Default(const char* zoneName, Game::FF_DIR source, unsigned int size, char* filename);
		static void DB_BuildOSPath_FromSource_Custom(const char* zoneName, Game::FF_DIR source, unsigned int size, char* filename);
		static Game::Sys_File Sys_CreateFile_Stub(const char* dir, const char* filename);
		static bool DB_FileExists_Hk(const char* zoneName, Game::FF_DIR source);
	};
}
