#pragma once

namespace Components
{
	class Stats : public Component
	{
	public:
		Stats();

		static bool IsInstalled();

	private:
		static bool isInstalled;

		static void SendStats();
		static void AddScriptFunctions();

		static const std::int64_t* GetStatsID();
		static void SprintfLiveStorageFilename(char* target, std::size_t size);
		static void SprintfLiveStorageFilenameWithFsGame(char* target, std::size_t size, const char* modName);
		static void MoveOldStatsToNewFolder();

		static void Steam_FileRead_Core_Checksum(const void* data, unsigned int size, unsigned int key, void* checksum);
		static void Steam_FileWrite_Core_Checksum(const void* data, unsigned int size, unsigned int key, void* checksum);
	};
}
