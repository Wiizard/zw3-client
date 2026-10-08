#include "STDInclude.hpp"

#include "Debug.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"

namespace Components
{
	const Game::dvar_t* Debug::debugOverlay = nullptr;
	const Game::dvar_t* Debug::bug_name = nullptr;

	static const char* const pmFlagNames[] =
	{
		"PMF_PRONE",
		"PMF_DUCKED",
		"PMF_MANTLE",
		"PMF_LADDER",
		"PMF_SIGHT_AIMING",
		"PMF_BACKWARDS_RUN",
		"PMF_WALKING",
		"PMF_TIME_HARDLANDING",
		"PMF_TIME_KNOCKBACK",
		"PMF_PRONEMOVE_OVERRIDDEN",
		"PMF_RESPAWNED",
		"PMF_FROZEN",
		"PMF_LADDER_FALL",
		"PMF_JUMPING",
		"PMF_SPRINTING",
		"PMF_SHELLSHOCKED",
		"PMF_MELEE_CHARGE",
		"PMF_NO_SPRINT",
		"PMF_NO_JUMP",
		"PMF_REMOTE_CONTROLLING",
		"PMF_ANIM_SCRIPTED",
		"PMF_UNK1",
		"PMF_DIVING",
	};

	static const char* const poFlagNames[] =
	{
		"POF_INVULNERABLE",
		"POF_REMOTE_EYES",
		"POF_LASER_ALTVIEW",
		"POF_THERMAL_VISION",
		"POF_THERMAL_VISION_OVERLAY_FOF",
		"POF_REMOTE_CAMERA_SOUNDS",
		"POF_ALT_SCENE_REAR_VIEW",
		"POF_ALT_SCENE_TAG_VIEW",
		"POF_SHIELD_ATTACHED_TO_WORLD_MODEL",
		"POF_DONT_LERP_VIEWANGLES",
		"POF_EMP_JAMMED",
		"POF_FOLLOW",
		"POF_PLAYER",
		"POF_SPEC_ALLOW_CYCLE",
		"POF_SPEC_ALLOW_FREELOOK",
		"POF_AC130",
		"POF_COMPASS_PING",
		"POF_ADS_THIRD_PERSON_TOGGLE",
	};

	static const char* const plFlagNames[] =
	{
		"PLF_ANGLES_LOCKED",
		"PLF_USES_OFFSET",
		"PLF_WEAPONVIEW_ONLY",
	};

	static const char* const eFlagNames[] =
	{
		"EF_NONSOLID_BMODEL",
		"EF_TELEPORT_BIT",
		"EF_CROUCHING",
		"EF_PRONE",
		"EF_UNK1",
		"EF_NODRAW",
		"EF_TIMED_OBJECT",
		"EF_VOTED",
		"EF_TALK",
		"EF_FIRING",
		"EF_TURRET_ACTIVE_PRONE",
		"EF_TURRET_ACTIVE_DUCK",
		"EF_LOCK_LIGHT_VIS",
		"EF_AIM_ASSIST",
		"EF_LOOP_RUMBLE",
		"EF_LASER_SIGHT",
		"EF_MANTLE",
		"EF_DEAD",
		"EF_ADS",
		"EF_NEW",
		"EF_VEHICLE_ACTIVE",
		"EF_JAMMING",
		"EF_COMPASS_PING",
		"EF_SOFT",
	};

	static const char buttonGlyphs[] =
	{
		'\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x0E', '\x0F', '\x10',
		'\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\0'
	};

	static const char fontTestTemplate[] = "%s: %s All those moments will be lost in time, like tears in rain.";
	static const int fontTestFonts[] = { 1, 2, 3, 5, 6 };

	constexpr float flagsTitleScale = 0.5f;
	constexpr float flagsScale = 0.201f;
	constexpr float flagsY = 20.0f;
	constexpr float fontTestX = -25.0f;
	constexpr float fontTestScale = 0.4f;

	constexpr float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	constexpr int playerstateFlagsPage = 2;
	constexpr int fontTestPage = 5;

	constexpr std::uintptr_t cgArray = 0x1404769A0;

	constexpr std::uintptr_t cgMedia = 0x14046B390;

	constexpr std::uintptr_t fullScreenUiFlag = 0x140C5CEA8;
	constexpr std::uintptr_t clsState = 0x1406CECF8;
	constexpr int connectionActive = 9;
	constexpr std::uintptr_t cg_drawMaterial = 0x1406BCE80;

	constexpr std::uintptr_t player_debugHealthFlags = 0x14008BF8C;
	constexpr std::uintptr_t player_debugHealth = 0x140440E70;

	static const std::uint8_t player_debugHealthFlagsMov[] = { 0x41, 0xB8, 0x8C, 0x00, 0x00, 0x00 };

	static const Utils::Hook::LeaSite Com_Assert_fLea = { 0x1401F582C, Utils::Hook::leaRdx, 0x140080E50 };

	std::string Debug::BuildFlagsString(int flags, std::span<const char* const> names)
	{
		std::string result;

		for (std::size_t i = 0; i < names.size(); ++i)
		{
			char color = '7';

			if (flags & (1 << i))
			{
				color = '2';
			}

			result.append(Utils::String::VA("^%c%s\n", color, names[i]));
		}

		return result;
	}

	void Debug::CG_Debug_DrawPSFlags(int localClientNum)
	{
		constexpr int maxChars = 4096;

		const auto* const ps = reinterpret_cast<const Game::playerState_s*>(Utils::Hook::Rebase(cgArray));
		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);

		auto* const font = Game::UI_GetFontHandle(placement, 6, flagsScale);
		auto* const titleFont = Game::UI_GetFontHandle(placement, 6, flagsTitleScale);

		Game::UI_DrawText(placement, "Client View of Flags", maxChars, titleFont, -60.0f, 0.0f, 1, 1, flagsTitleScale, white, 1);

		const auto pmFlags = BuildFlagsString(ps->pm_flags, pmFlagNames);
		Game::UI_DrawText(placement, pmFlags.data(), maxChars, font, 30.0f, flagsY, 1, 1, flagsScale, white, 3);

		const auto poFlags = BuildFlagsString(ps->otherFlags, poFlagNames);
		Game::UI_DrawText(placement, poFlags.data(), maxChars, font, 350.0f, flagsY, 1, 1, flagsScale, white, 3);

		const auto plFlags = BuildFlagsString(ps->linkFlags, plFlagNames);
		Game::UI_DrawText(placement, plFlags.data(), maxChars, font, 350.0f, 250.0f, 1, 1, flagsScale, white, 3);

		const auto eFlags = BuildFlagsString(ps->eFlags, eFlagNames);
		Game::UI_DrawText(placement, eFlags.data(), maxChars, font, 525.0f, flagsY, 1, 1, flagsScale, white, 3);
	}

	void Debug::CG_DrawDebugPlayerHealth(int localClientNum)
	{
		constexpr float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		constexpr float green[] = { 0.0f, 1.0f, 0.0f, 1.0f };

		const auto* const ps = reinterpret_cast<const Game::playerState_s*>(Utils::Hook::Rebase(cgArray));

		float healthFraction = 0.0f;

		if (ps->stats[0] && ps->stats[2])
		{
			const float health = static_cast<float>(ps->stats[0]) / static_cast<float>(ps->stats[2]);
			healthFraction = std::clamp(health, 0.0f, 1.0f);
		}

		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);
		auto* const whiteMaterial = Utils::Hook::Get<Game::Material*>(cgMedia);

		Game::CL_DrawStretchPic(placement, 10.0f, 10.0f, 100.0f, 10.0f, 1, 1, 0.0f, 0.0f, 1.0f, 1.0f, black, whiteMaterial);
		Game::CL_DrawStretchPic(placement, 10.0f, 10.0f, 100.0f * healthFraction, 10.0f, 1, 1, 0.0f, 0.0f, healthFraction, 1.0f, green, whiteMaterial);
	}

	void Debug::CG_Debug_DrawFontTest(int localClientNum)
	{
		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);

		float y = 10.0f;

		for (const int fontEnum : fontTestFonts)
		{
			auto* const font = Game::UI_GetFontHandle(placement, fontEnum, fontTestScale);

			char line[0x200]{};
			sprintf_s(line, fontTestTemplate, font->fontName, buttonGlyphs);
			Game::UI_FilterStringForButtonAnimation(line, sizeof(line));

			Game::UI_DrawText(placement, line, std::numeric_limits<int>::max(), font, fontTestX, y, 1, 1, fontTestScale, white, 3);
			y += 25.0f;
		}
	}

	void Debug::CG_DrawDebugOverlays_Hk()
	{
		constexpr int localClientNum = 0;

		if (!Utils::Hook::Get<int>(fullScreenUiFlag) || Utils::Hook::Get<int>(clsState) != connectionActive)
		{
			return;
		}

		const auto* const drawMaterial = Utils::Hook::Get<const Game::dvar_t*>(cg_drawMaterial);

		if (!drawMaterial || drawMaterial->current.integer)
		{
			return;
		}

		if (!debugOverlay)
		{
			return;
		}

		if (debugOverlay->current.integer == playerstateFlagsPage)
		{
			CG_Debug_DrawPSFlags(localClientNum);
		}
		else if (debugOverlay->current.integer == fontTestPage)
		{
			CG_Debug_DrawFontTest(localClientNum);
		}

		const auto* const debugHealth = Utils::Hook::Get<const Game::dvar_t*>(player_debugHealth);

		if (debugHealth && debugHealth->current.enabled)
		{
			CG_DrawDebugPlayerHealth(localClientNum);
		}
	}

	void Debug::Com_Assert_f()
	{
		assert(false && "a");
	}

	void Debug::CL_InitDebugDvars()
	{
		static const char* debugOverlayNames[] =
		{
			"Off",
			"ViewmodelInfo",
			"Playerstate Flags",
			"Entity Counts",
			"Controllers",
			"FontTest",
			nullptr,
		};

		debugOverlay = Game::Dvar_RegisterEnum("debugOverlay", debugOverlayNames, 0, Game::DVAR_NONE, "Toggles the display of various debug info.");
		bug_name = Game::Dvar_RegisterString("bug_name", "bug0", Game::DVAR_NONE, "Name appended to the copied console log");
	}

	Debug::Debug()
	{
		const bool isExpected = Utils::Hook::IsLeaIntact(Com_Assert_fLea)
			&& Utils::Hook::MatchesBytes(player_debugHealthFlags, player_debugHealthFlagsMov, sizeof(player_debugHealthFlagsMov));

		if (!isExpected)
		{
			Logger::Error("debug: Com_Init or BG_RegisterDvars does not read as expected, no debug overlays\n");
			return;
		}

		const auto assertHandler = Utils::Hook::Trampoline(Utils::Hook::Rebase(Com_Assert_fLea.address), reinterpret_cast<std::uintptr_t>(Com_Assert_f));

		if (!assertHandler || !Utils::Hook::CanLeaReach(Com_Assert_fLea, reinterpret_cast<void*>(assertHandler)))
		{
			Logger::Error("debug: no room for assert's handler beside the image, no debug overlays\n");
			return;
		}

		Utils::Hook::PointLeaAt(Com_Assert_fLea, reinterpret_cast<void*>(assertHandler));
		Utils::Hook::Set<std::uint32_t>(player_debugHealthFlags + 2, Game::DVAR_NONE);

		Events::OnDvarInit(CL_InitDebugDvars);
		Scheduler::Loop(CG_DrawDebugOverlays_Hk, Scheduler::Pipeline::RENDERER);
	}
}
