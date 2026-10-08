#pragma once

#include "Dvar.hpp"

namespace Components
{
	class FastFiles : public Component
	{
	public:
		FastFiles();

		static void AddZonePath(const std::string& path);
		static bool Exists(const std::string& file);

		static std::string Current();
		static bool Ready();

		static float GetFullLoadedFraction();

		static void MarkMainMenuReady();
		static bool MainMenuReady();

		static bool HasZW3CommonZone();
		static bool IsZombieZoneName(std::string_view zoneName);
		static bool ShouldProtectZone(std::string_view zoneName);
		static void ProtectZoneBuffer(std::string& buffer);

		static void PrefetchZone(const std::string& zoneName);
		static void PrefetchPath(const std::filesystem::path& path);

		static void LoadDLCUIZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);
		static void LoadGfxZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);
		static void LoadLocalizeZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);

	private:
		static std::vector<std::string> zonePaths;

		static Dvar::Var g_loadingInitialZones;

		static Utils::Hook loadInitialZonesHook;
		static Utils::Hook loadDLCUIZonesHook;
		static Utils::Hook loadGfxZonesHook;
		static Utils::Hook zoneDirHooks[3];

		static const char* GetZoneLocation(const char* file);
		static const char* Sys_GetMapZoneDir_Stub(const char* zoneName);
		static void LoadInitialZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);
	};
}
