#include <STDInclude.hpp>

#include "Components/Modules/GSC/Script.hpp"
#include "Components/Modules/GSC/ScriptExtension.hpp"

#include "Debugger.hpp"

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <json.hpp>
#include <string>
#include <windows.h>

#include <cctype>
#include <cerrno>
#include <limits>

#include <unordered_map>

#include <thread>
#include <atomic>
#include <cmath>

#include <sstream>

namespace Components::Debugger
{
	Dvar::Var mpanim_debug;

	using GScr_PrecacheMpAnim_t = int(__cdecl*)();
	using G_FindConfigstringIndex_t = int(__cdecl*)(const char* name, int start, int max, int create, const char* err);

	static GScr_PrecacheMpAnim_t GScr_PrecacheMpAnim = reinterpret_cast<GScr_PrecacheMpAnim_t>(0x5F8380);
	static G_FindConfigstringIndex_t G_FindConfigstringIndex = reinterpret_cast<G_FindConfigstringIndex_t>(0x4AF0C0);

	using BG_GetSpreadForWeapon_t = float* (__cdecl*)(int ps, float* minSpread, float* maxSpread, float* outSpread);

	inline BG_GetSpreadForWeapon_t BG_GetSpreadForWeapon = reinterpret_cast<BG_GetSpreadForWeapon_t>(0x4770F0);

	static std::vector<std::string> cachedMpAnims;
	static bool mpAnimCommitComplete = false;

	void ExpelMpAnims()
	{
		if (!Game::level || !Game::level->initializing)
			return;

		constexpr int CS_START = 4042;
		constexpr int CS_MAX = 63;

		int used = 0;

		for (const auto& anim : cachedMpAnims)
		{
			if (used >= CS_MAX)
			{
				Game::Com_Printf(15,
					"[mpanim] WARNING: hit hard limit (%d), skipping '%s'\n",
					CS_MAX, anim.c_str());
				break;
			}

			G_FindConfigstringIndex(
				anim.c_str(),
				CS_START,
				CS_MAX,
				1,
				"mp anim"
			);

			++used;
		}

		Game::Com_Printf(15,
			"[mpanim] committed %d mp anims\n", used);

		cachedMpAnims.clear();
	}

	int __cdecl GScr_PrecacheMpAnim_Hook()
	{
		if (!Game::level->initializing)
		{
			Game::Scr_Error("PrecacheMpAnim() must be called before any wait statements in the gametype or level script\n");
			return 0;
		}

		const char* anim = Game::Scr_GetString(0);
		if (!anim || !*anim)
			return 0;

		for (const auto& a : cachedMpAnims)
		{
			if (_stricmp(a.c_str(), anim) == 0)
				return 1;
		}

		cachedMpAnims.emplace_back(anim);

		if (Game::Dvar_FindVar("mpanim_debug")->current.enabled)
		{
			Game::Com_Printf(0, "[mpanim] queued: %s (total=%zu)\n", anim, cachedMpAnims.size());
		}

		return 1;
	}

	void Add_Custom_GSC_Functions()
	{
	}

	DebugSetup::DebugSetup()
	{
		mpanim_debug = Game::Dvar_RegisterBool("mpanim_debug", true, Game::DVAR_NONE, "Enable debug output for PrecacheMpAnim override");

		// Raise Limits
		Utils::Hook(0x5F8380, GScr_PrecacheMpAnim_Hook, HOOK_JUMP).install()->quick();
		// Clear Old Cached Anims
		ExpelMpAnims();

		// Custom GSC Functions
		Add_Custom_GSC_Functions();
	}
}
