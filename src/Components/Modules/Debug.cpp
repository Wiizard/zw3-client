#include "STDInclude.hpp"

#include "Debug.hpp"
#include "Console.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Remix/Remix.hpp"

namespace Components
{
	const Game::dvar_t* Debug::debugOverlay = nullptr;
	const Game::dvar_t* Debug::bug_name = nullptr;
	const Game::dvar_t* Debug::cg_drawLookAt = nullptr;

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
	constexpr std::uintptr_t cg_drawViewpos = 0x14058B898;
	constexpr std::uintptr_t CG_GetPlayerViewOrigin = 0x1400C3C90;
	constexpr std::uintptr_t CG_RegisterDvars_DrawViewposCall = 0x1400D7803;
	constexpr std::uintptr_t Dvar_RegisterBool_Engine = 0x140285C70;

	constexpr int viewposEye = 1;
	constexpr int viewposFeet = 2;
	constexpr int viewposBoth = 3;

	// registered by the exe, never drawn in MP
	constexpr std::uintptr_t cg_drawFPS = 0x140587508;
	constexpr std::uintptr_t cg_drawFPSLabels = 0x1406BCE60;

	constexpr int drawFpsSimpleRanges = 2;
	constexpr int drawFpsVerbose = 3;
	constexpr int drawFpsVerboseViewpos = 4;

	constexpr int fpsSampleCount = 32;
	constexpr float fpsScale = 0.75f;
	constexpr float fpsOffsetX = 5.0f;
	constexpr float fpsOffsetY = -5.0f;
	constexpr float degreesPerRadian = 57.29577951308232f;

	static int fpsPreviousMs = 0;
	static bool hasFpsPreviousMs = false;
	static int fpsFrameTimes[fpsSampleCount]{};
	static int fpsIndex = 0;
	static bool hasFullFpsWindow = false;

	constexpr std::uintptr_t CG_LocationalTrace = 0x1400CA0C0;
	constexpr std::uintptr_t CM_TraceStaticModel = 0x1401EB790;
	constexpr std::uintptr_t AngleVectors = 0x14027E000;
	constexpr std::uintptr_t Com_SurfaceTypeToName = 0x14028DF60;
	constexpr std::uintptr_t cgs_gameModels = 0x1405876A0;
	constexpr std::uintptr_t cm_numStaticModels = 0x141BD3198;
	constexpr std::uintptr_t cm_staticModelList = 0x141BD31A0;

	constexpr int MASK_SHOT = 0x2806831;
	constexpr int MASK_ALL = -1;

	constexpr int lookAtMaterial = 1;
	constexpr int lookAtObject = 2;
	constexpr int lookAtPoint = 3;
	constexpr int lookAtAll = 4;
	constexpr int lookAtClips = 5;
	constexpr int ENTITYNUM_WORLD = 2046;
	constexpr int ET_GENERAL = 0;
	constexpr int ET_SCRIPTMOVER = 6;
	constexpr int brushSolid = 0xFFFFFF;
	constexpr int scriptMoverModel = 0x140;
	constexpr int surfaceTypeShift = 20;
	constexpr int surfaceTypeMask = 0x1F;
	constexpr float lookAtRange = 8192.0f;

	struct cStaticModel_s
	{
		Game::XModel* xmodel;
		float origin[3];
		float invScaledAxis[3][3];
		Game::Bounds absBounds;
	};

	AssertSize(cStaticModel_s, 0x50);

	static Utils::Hook drawViewposHook;

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

	// bool in the exe, never drawn in MP
	Game::dvar_t* Debug::Dvar_RegisterDrawViewpos(const char* dvarName, [[maybe_unused]] bool value, unsigned int flags, [[maybe_unused]] const char* description)
	{
		Console::AddDvarHint(dvarName, "  1: eye\n  2: feet\n  3: both");
		return Game::Dvar_RegisterInt(dvarName, 0, 0, viewposBoth, flags, "Draw viewpos: 1 eye, 2 feet, 3 both");
	}

	void Debug::CG_DrawViewpos(int localClientNum, int mode, std::vector<std::string>& rows)
	{
		const auto* const ps = reinterpret_cast<const Game::playerState_s*>(Utils::Hook::Rebase(cgArray));

		if (mode == viewposEye || mode == viewposBoth)
		{
			float eye[3]{};
			Utils::Hook::Call<void(int, const Game::playerState_s*, float*)>(CG_GetPlayerViewOrigin)(localClientNum, ps, eye);
			rows.emplace_back(Utils::String::VA("(%.0f, %.0f, %.0f) : %.0f, %.0f (eye)", eye[0], eye[1], eye[2], ps->viewangles[0], ps->viewangles[1]));
		}

		if (mode == viewposFeet || mode == viewposBoth)
		{
			rows.emplace_back(Utils::String::VA("(%.0f, %.0f, %.0f) (feet)", ps->origin[0], ps->origin[1], ps->origin[2]));
		}
	}

	static bool RayHitsBounds(const float* start, const float* delta, const Game::Bounds& bounds)
	{
		float enter = 0.0f;
		float exit = 1.0f;

		for (int axis = 0; axis < 3; ++axis)
		{
			const float min = bounds.midPoint[axis] - bounds.halfSize[axis];
			const float max = bounds.midPoint[axis] + bounds.halfSize[axis];

			if (std::fabs(delta[axis]) < 0.0001f)
			{
				if (start[axis] < min || start[axis] > max)
				{
					return false;
				}

				continue;
			}

			float entry = (min - start[axis]) / delta[axis];
			float leave = (max - start[axis]) / delta[axis];

			if (entry > leave)
			{
				std::swap(entry, leave);
			}

			enter = std::max(enter, entry);
			exit = std::min(exit, leave);

			if (enter > exit)
			{
				return false;
			}
		}

		return true;
	}

	// static model hits come back as world, so retrace them to get the one hit
	static const char* StaticModelAt(const float* start, const float* end, float fraction, int mask)
	{
		const auto count = Utils::Hook::Get<unsigned int>(cm_numStaticModels);
		const auto* const models = Utils::Hook::Get<const cStaticModel_s*>(cm_staticModelList);
		const float delta[3] = { end[0] - start[0], end[1] - start[1], end[2] - start[2] };

		float best = 1.0f;
		const char* name = nullptr;

		for (unsigned int i = 0; i < count; ++i)
		{
			const auto& model = models[i];

			if (!model.xmodel || !RayHitsBounds(start, delta, model.absBounds))
			{
				continue;
			}

			Game::trace_t trace{};
			trace.fraction = 1.0f;
			Utils::Hook::Call<void(const cStaticModel_s*, Game::trace_t*, const float*, const float*, int)>(CM_TraceStaticModel)(&model, &trace, start, end, mask);

			if (trace.fraction < best)
			{
				best = trace.fraction;
				name = model.xmodel->name;
			}
		}

		return name && std::fabs(best - fraction) < 0.0001f ? name : nullptr;
	}

	static const char* const eTypeNames[] =
	{
		"general", "player", "player corpse", "item", "missile", "invisible", "script mover", "sound blend",
		"fx", "loop fx", "primary light", "turret", "helicopter", "plane", "vehicle", "vehicle collmap",
		"vehicle corpse", "vehicle spawner",
	};

	static void AddEntityRows(int localClientNum, int entNum, std::vector<std::string>& rows)
	{
		const auto* const cent = Game::CG_GetEntity(localClientNum, entNum);
		const int eType = cent->nextState.eType;

		if (eType >= 0 && eType < static_cast<int>(std::size(eTypeNames)))
		{
			rows.emplace_back(Utils::String::VA("entity %d: %s", entNum, eTypeNames[eType]));
		}
		else
		{
			rows.emplace_back(Utils::String::VA("entity %d: eType %d", entNum, eType));
		}

		const auto* const sv_running = *Game::com_sv_running;

		if (sv_running && sv_running->current.enabled)
		{
			const auto* const ent = &Game::g_entities[entNum];

			if (ent->r.isInUse)
			{
				rows.emplace_back(Utils::String::VA("classname: %s", Game::SL_ConvertToString(ent->classname)));

				if (ent->targetname)
				{
					rows.emplace_back(Utils::String::VA("targetname: %s", Game::SL_ConvertToString(ent->targetname)));
				}

				if (ent->target)
				{
					rows.emplace_back(Utils::String::VA("target: %s", Game::SL_ConvertToString(ent->target)));
				}

				if (ent->health)
				{
					rows.emplace_back(Utils::String::VA("health: %d", ent->health));
				}
			}
		}

		if (cent->nextState.solid == brushSolid)
		{
			rows.emplace_back(Utils::String::VA("brush: *%d", cent->nextState.index));
		}

		if (eType != ET_GENERAL && eType != ET_SCRIPTMOVER)
		{
			return;
		}

		const int modelIndex = eType == ET_SCRIPTMOVER
			? *reinterpret_cast<const int*>(reinterpret_cast<const char*>(cent) + scriptMoverModel)
			: cent->nextState.index;

		const auto* const xmodel = reinterpret_cast<Game::XModel* const*>(Utils::Hook::Rebase(cgs_gameModels))[modelIndex];

		if (modelIndex > 0 && xmodel)
		{
			rows.emplace_back(Utils::String::VA("model: %s", xmodel->name));
		}
	}

	void Debug::CG_DrawLookAt(int localClientNum, int mode, std::vector<std::string>& rows)
	{
		const bool showMaterial = mode == lookAtMaterial || mode >= lookAtAll;
		const bool showObject = mode == lookAtObject || mode >= lookAtAll;
		const bool showPoint = mode == lookAtPoint || mode >= lookAtAll;
		const int mask = mode == lookAtClips ? MASK_ALL : MASK_SHOT;

		const auto* const ps = reinterpret_cast<const Game::playerState_s*>(Utils::Hook::Rebase(cgArray));

		float start[3]{};
		Utils::Hook::Call<void(int, const Game::playerState_s*, float*)>(CG_GetPlayerViewOrigin)(localClientNum, ps, start);

		float forward[3]{};
		Utils::Hook::Call<void(const float*, float*, float*, float*)>(AngleVectors)(ps->viewangles, forward, nullptr, nullptr);

		const float end[3] =
		{
			start[0] + forward[0] * lookAtRange,
			start[1] + forward[1] * lookAtRange,
			start[2] + forward[2] * lookAtRange,
		};

		Game::trace_t trace{};
		trace.fraction = 1.0f;
		Utils::Hook::Call<void(Game::trace_t*, const float*, const float*, int, int)>(CG_LocationalTrace)(&trace, start, end, ps->clientNum, mask);

		if (trace.fraction >= 1.0f)
		{
			rows.emplace_back("nothing");
			return;
		}

		if (showMaterial)
		{
			rows.emplace_back(Utils::String::VA("material: %s", trace.material ? trace.material : "none"));

			const int surfaceType = (trace.surfaceFlags >> surfaceTypeShift) & surfaceTypeMask;
			rows.emplace_back(Utils::String::VA("surface: %s, contents: 0x%X",
				Utils::Hook::Call<const char*(int)>(Com_SurfaceTypeToName)(surfaceType), trace.contents));
		}

		if (showObject)
		{
			switch (trace.hitType)
			{
			case Game::TRACE_HITTYPE_ENTITY:
				if (trace.hitId != ENTITYNUM_WORLD)
				{
					AddEntityRows(localClientNum, trace.hitId, rows);
				}
				else if (const char* const name = StaticModelAt(start, end, trace.fraction, mask))
				{
					rows.emplace_back(Utils::String::VA("static model: %s", name));
				}
				else
				{
					rows.emplace_back("world");
				}
				break;
			case Game::TRACE_HITTYPE_DYNENT_MODEL:
			case Game::TRACE_HITTYPE_DYNENT_BRUSH:
				rows.emplace_back(Utils::String::VA("dynent %d", trace.hitId));
				break;
			case Game::TRACE_HITTYPE_GLASS:
				rows.emplace_back(Utils::String::VA("glass %d", trace.hitId));
				break;
			default:
				break;
			}
		}

		if (!showPoint)
		{
			return;
		}

		rows.emplace_back(Utils::String::VA("normal: (%.2f, %.2f, %.2f)", trace.normal[0], trace.normal[1], trace.normal[2]));
		rows.emplace_back(Utils::String::VA("(%.0f, %.0f, %.0f) %.0f units",
			start[0] + (end[0] - start[0]) * trace.fraction,
			start[1] + (end[1] - start[1]) * trace.fraction,
			start[2] + (end[2] - start[2]) * trace.fraction,
			lookAtRange * trace.fraction));
	}

	static void ResetFpsWindow()
	{
		hasFpsPreviousMs = false;
		hasFullFpsWindow = false;
		fpsIndex = 0;
	}

	static void SampleFrameTime()
	{
		const int now = Game::Sys_Milliseconds();

		if (!hasFpsPreviousMs)
		{
			fpsPreviousMs = now;
			hasFpsPreviousMs = true;
			return;
		}

		fpsFrameTimes[fpsIndex] = now - fpsPreviousMs;
		fpsPreviousMs = now;

		fpsIndex = (fpsIndex + 1) % fpsSampleCount;

		if (fpsIndex == 0)
		{
			hasFullFpsWindow = true;
		}
	}

	// UI text scales are against a 48 pixel font, so undo that to scale the font's own size
	static float UiScaleOf(const Game::Font_s* font)
	{
		return fpsScale * static_cast<float>(font->pixelHeight) / 48.0f;
	}

	// value ends at x, label starts there
	static void DrawFpsRow(const float* placement, Game::Font_s* font, const char* value, const char* label, float x, float y, const float* color)
	{
		constexpr int maxChars = std::numeric_limits<int>::max();
		constexpr float labelColor[] = { 1.0f, 1.0f, 1.0f, 0.75f };

		const float scale = UiScaleOf(font);
		const auto width = static_cast<float>(Game::UI_TextWidth(value, maxChars, font, scale));
		Game::UI_DrawText(placement, value, maxChars, font, x - width, y, 3, 1, scale, color, 0);

		if (label)
		{
			Game::UI_DrawText(placement, label, maxChars, font, x, y, 3, 1, scale, labelColor, 0);
		}
	}

	// returns the baseline of the last row drawn, 0 when nothing is
	float Debug::CG_DrawFPS(int localClientNum)
	{
		SampleFrameTime();

		const auto* const drawFps = Utils::Hook::Get<const Game::dvar_t*>(cg_drawFPS);

		if (!drawFps || drawFps->current.integer <= 0 || !hasFullFpsWindow)
		{
			return 0.0f;
		}

		auto* const font = Game::R_RegisterFont("fonts/bigDevFont", 0);

		if (!font)
		{
			return 0.0f;
		}

		int minTime = std::numeric_limits<int>::max();
		int maxTime = 0;
		int total = 0;

		for (const int frameTime : fpsFrameTimes)
		{
			total += frameTime;
			minTime = std::min(minTime, frameTime);
			maxTime = std::max(maxTime, frameTime);
		}

		const float average = static_cast<float>(total) / static_cast<float>(fpsSampleCount);

		float variance = 0.0f;

		for (const int frameTime : fpsFrameTimes)
		{
			variance += std::fabs(static_cast<float>(frameTime) - average);
		}

		variance /= static_cast<float>(fpsSampleCount);

		total = std::max(total, 1);
		minTime = std::max(minTime, 1);
		const int slowestTime = std::max(maxTime, 1);

		const int fpsMin = static_cast<int>(std::lrint(1000.0f / static_cast<float>(slowestTime)));
		const int fpsMax = static_cast<int>(std::lrint(1000.0f / static_cast<float>(minTime)));
		const int fps = static_cast<int>(std::lrint(1000.0f * fpsSampleCount / static_cast<float>(total)));
		const float msPerFrame = static_cast<float>(total) / static_cast<float>(fpsSampleCount);

		// red under 55, white from 65
		const float frac = std::clamp(static_cast<float>(fps - 55) / 10.0f, 0.0f, 1.0f);
		const float fpsColor[] = { 1.0f, frac, frac, 1.0f };

		const int level = drawFps->current.integer;
		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);

		const auto* const drawLabels = Utils::Hook::Get<const Game::dvar_t*>(cg_drawFPSLabels);
		const bool shouldDrawLabels = drawLabels && drawLabels->current.enabled;

		const float lineHeight = static_cast<float>(Game::R_TextHeight(font)) * fpsScale;
		const float lineStep = static_cast<float>(static_cast<int>(lineHeight)) * fpsScale;

		float x = fpsOffsetX;

		if (shouldDrawLabels)
		{
			x -= static_cast<float>(Game::UI_TextWidth(" cg ms/frame", std::numeric_limits<int>::max(), font, UiScaleOf(font)));
		}

		float y = fpsOffsetY + lineHeight;

		const char* fpsText = Utils::String::VA("%i", fps);

		if (level >= drawFpsSimpleRanges)
		{
			fpsText = Utils::String::VA("(%i-%i, %i) %i", fpsMin, fpsMax, static_cast<int>(std::lrint(variance)), fps);
		}

		// with RTX Remix the frames on screen can outnumber the game's, DLSS frame generation adds its own
		if (Remix::Remix::IsActive() && Remix::Remix::IsEnabled())
		{
			if (const int presentedFps = Remix::Remix::GetPresentedFps(); presentedFps >= 0)
			{
				fpsText = Utils::String::VA("%s | %i RTX", fpsText, presentedFps);
			}
		}

		DrawFpsRow(placement, font, fpsText, shouldDrawLabels ? " FPS" : nullptr, x, y, fpsColor);

		if (level < drawFpsVerbose)
		{
			return y;
		}

		y += lineStep;
		DrawFpsRow(placement, font, Utils::String::VA("(%i-%i) %1.2f", minTime, maxTime, msPerFrame),
			shouldDrawLabels ? " cg ms/frame" : nullptr, x, y, white);

		if (level < drawFpsVerboseViewpos)
		{
			return y;
		}

		const auto& view = reinterpret_cast<const Game::cg_s*>(Utils::Hook::Rebase(cgArray))->refdef.view;
		const float* const forward = view.axis[0];
		const float flatLength = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		const float pitch = std::atan2(-forward[2], flatLength) * degreesPerRadian;
		const float yaw = std::atan2(forward[1], forward[0]) * degreesPerRadian;

		y += lineStep;
		DrawFpsRow(placement, font, Utils::String::VA("(%.0f, %.0f, %.0f) %.0f %.0f", view.org[0], view.org[1], view.org[2], pitch, yaw),
			shouldDrawLabels ? " viewpos" : nullptr, x, y, white);

		return y;
	}

	void Debug::DrawCornerRows(int localClientNum, const std::vector<std::string>& rows, float top)
	{
		constexpr float scale = 0.25f;
		constexpr float margin = 10.0f;
		constexpr float rowHeight = 12.0f;

		const auto* const placement = Game::ScrPlace_GetActivePlacement(localClientNum);
		auto* const font = Game::UI_GetFontHandle(placement, 6, scale);

		float y = std::max(20.0f, top + rowHeight);

		for (const auto& row : rows)
		{
			const auto width = static_cast<float>(Game::UI_TextWidth(row.data(), std::numeric_limits<int>::max(), font, scale));
			Game::UI_DrawText(placement, row.data(), std::numeric_limits<int>::max(), font, -width - margin, y, 3, 1, scale, white, 3);
			y += rowHeight;
		}
	}

	void Debug::CG_DrawDebugOverlays_Hk()
	{
		constexpr int localClientNum = 0;

		if (!Utils::Hook::Get<int>(fullScreenUiFlag) || Utils::Hook::Get<int>(clsState) != connectionActive)
		{
			ResetFpsWindow();
			return;
		}

		const float fpsBottom = CG_DrawFPS(localClientNum);

		std::vector<std::string> rows;
		const auto* const drawViewpos = Utils::Hook::Get<const Game::dvar_t*>(cg_drawViewpos);

		if (drawViewpos && drawViewpos->current.integer)
		{
			CG_DrawViewpos(localClientNum, drawViewpos->current.integer, rows);
		}

		if (cg_drawLookAt && cg_drawLookAt->current.integer)
		{
			CG_DrawLookAt(localClientNum, cg_drawLookAt->current.integer, rows);
		}

		DrawCornerRows(localClientNum, rows, fpsBottom);

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
		cg_drawLookAt = Game::Dvar_RegisterInt("cg_drawLookAt", 0, 0, lookAtClips, Game::DVAR_NONE, "Draw what the crosshair is on");
		Console::AddDvarHint("cg_drawLookAt", "  1: material\n  2: object\n  3: hit point\n  4: all\n  5: all, clips too");
		Console::AddDvarHint("cg_drawFPS", "  1: simple\n  2: simple ranges\n  3: verbose\n  4: verbose + viewpos");
		Console::AddDvarHint("cg_drawFPSLabels", "  draw labels beside cg_drawFPS");
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

		if (!Utils::Hook::BranchesTo(CG_RegisterDvars_DrawViewposCall, Dvar_RegisterBool_Engine, false)
			|| !drawViewposHook.Initialize(CG_RegisterDvars_DrawViewposCall, reinterpret_cast<void*>(Dvar_RegisterDrawViewpos), HOOK_CALL)->Install()->IsInstalled())
		{
			drawViewposHook.Uninstall();
			Logger::Error("debug: CG_RegisterDvars does not read as expected, cg_drawViewpos stays a bool\n");
		}
		Utils::Hook::Set<std::uint32_t>(player_debugHealthFlags + 2, Game::DVAR_NONE);

		Events::OnDvarInit(CL_InitDebugDvars);
		Scheduler::Loop(CG_DrawDebugOverlays_Hk, Scheduler::Pipeline::RENDERER);
	}
}
