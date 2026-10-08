#pragma once

namespace Components
{
	class Debug : public Component
	{
	public:
		Debug();

	private:
		static const Game::dvar_t* debugOverlay;
		static const Game::dvar_t* bug_name;

		static std::string BuildFlagsString(int flags, std::span<const char* const> names);

		static void CG_Debug_DrawPSFlags(int localClientNum);
		static void CG_DrawDebugPlayerHealth(int localClientNum);
		static void CG_Debug_DrawFontTest(int localClientNum);

		static void CG_DrawDebugOverlays_Hk();

		static void Com_Assert_f();

		static void CL_InitDebugDvars();
	};
}
