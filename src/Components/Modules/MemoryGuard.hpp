#pragma once

namespace Components
{
	// MemoryGuard collects diagnostic information when the engine hits
	// Sys_OutOfMemError and performs lightweight housekeeping between
	// map transitions (heap compaction). It never unloads any fastfile
	// and never touches common/global zones (zw3.ff, iw4x_*, localize).
	class MemoryGuard : public Component
	{
	public:
		MemoryGuard();

		// Called from Maps::UnloadMapZones after the engine has finished
		// releasing map-scoped zones. Runs safe housekeeping only.
		static void OnMapUnloaded();

	private:
		static Utils::Hook OutOfMemHook;

		static void OutOfMemHandler_Stub(const char* filename, int line);

		static std::string CollectDiagnostics(const char* filename, int line);
		static std::string FormatLoadedZones();
		static std::string FormatMemoryInfo();
		static void WriteDiagnosticsLog(const std::string& diag);
	};
}
