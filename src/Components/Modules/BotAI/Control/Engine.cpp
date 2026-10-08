#include "Components/Modules/BotAI/Control/State.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Control/Loadout.hpp"
#include "Components/Modules/BotAI/Control/Personality.hpp"
#include "Components/Modules/BotAI/Control/Objectives.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include "Components/Modules/BotAI/Control/Identity.hpp"
#include "Components/Modules/BotAI/Control/Lobby.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include "Components/Modules/ClientSlots.hpp"
#include "Components/Modules/Events.hpp"
#include <string>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <share.h>

namespace Components::BotAI
{
	static constexpr int pingRefreshMin  = 10;
	static constexpr int pingRefreshSpan = 20;
	static constexpr int pingJitterMin   = -4;
	static constexpr int pingJitterMax   = 6;
	static constexpr int pingSpikePercent    = 5;
	static constexpr int pingSpikeMin    = 10;
	static constexpr int pingSpikeMax    = 40;
	static constexpr int pingWanderPercent   = 10;
	static constexpr int pingOutlierPercent  = 10;
	static constexpr int pingOutlierMin      = 30;
	static constexpr int pingOutlierMax      = 120;
	static constexpr int classChangePercent  = 8;
	static constexpr int classChangeLosingPercent = 25;
	static constexpr int classChangeLosingDeaths = 3;
	static constexpr int leaveMinMs          = 240000;
	static constexpr int leaveMaxMs          = 720000;


	using Sys_Milliseconds_t = int (__cdecl*)();

	static void* countDvar  = nullptr;
	static void* fillDvar   = nullptr;

	static void* generateDvar = nullptr;
	static void* autogenDvar = nullptr;

	static int lastGenerateValue = 0;

	static constexpr int staleRegenFrames = 40;
	static int staleRegenCountdown = 0;
	static constexpr int annotateDelayFrames = 60;
	static int annotateCountdown = 0;
	static const char* autoGenReason = "stale file, regenerated on load";


	static int nextOrdinal = 0;

	static const char* pendingName = nullptr;

	static void* debugDvar  = nullptr;


	bool debugOn = false;

	int debugTick = 0;
	int sightLineCalls = 0;
	int pathFinds = 0;

	static constexpr int perfLogFrames = 100;
	static float perfThinkMsSum = 0.0f;
	static float perfThinkMsMax = 0.0f;
	static int perfFrames = 0;
	static int perfTraces = 0;
	static int perfMovingFrames = 0;
	static int perfSprintFrames = 0;
	static int perfPaths = 0;


	bool pathBuiltThisFrame = false;

	static char activeMap[64] = {};



	BotState bots[maxClients] = {};

	Tuning tuning = {};


	static int framesSinceSpawn = 0;
	static int matchStartTick = 0;
	static constexpr int openingHoldFrames = 600;
	static constexpr float openingClimbCostScale = 5.0f;
	static constexpr int tracedBots = 2;
	static bool wasOpeningHeld = false;
	static constexpr int settleFrames = 300;
	static int joinGapFrames = spawnFrameGap;
	static bool settingsTried = false;


	static int burstFrames = 0;

	static constexpr int scriptPlayerChildren = 2200;
	static constexpr int scriptAddMargin = 1500;
	static constexpr int scriptDropRoom = 1000;
	static constexpr int scriptPeakDecay = 1;
	static constexpr int scriptJoiningMs = 15000;
	static constexpr int scriptDropGapMs = 2000;
	static int scriptPeak = 0;
	static int scriptDropAllowedTime = 0;
	static int scriptBotCap = -1;
	static int joiningBots = 0;

	static constexpr int scriptPlayerParents = 750;
	static constexpr int scriptParentMargin = 2500;
	static constexpr int scriptParentDropRoom = 1500;
	static constexpr int scriptParentPeakDecay = 20;
	static constexpr int scriptParentSampleFrames = 20;
	static int scriptParentPeak = 0;
	static int scriptParentsUsed = 0;
	static int scriptParentSampleFrame = 0;
	static int scriptParentBaseline = -1;
	static int scriptBaselinePlayers = 0;
	static int scriptPlayers = 0;
	static int settledPlayerParents = 0;
	static int settledPlayers = 0;
	static constexpr int firstWaveSize = 8;
	static constexpr int parentMarginDivisor = 5;

	static constexpr int scriptPlayerMemoryBlocks = 8;
	static constexpr int scriptMemoryBlockMargin = 300;
	static constexpr int scriptMemoryDropBlocks = 150;
	static int scriptMemoryBlocks = 0;

	static constexpr int burstPlayerParents = 650;
	static constexpr int burstPlayerMemoryBlocks = 6;
	static constexpr int burstMemoryBlockMargin = 300;
	static int burstPlan = -1;
	static int burstJoined = 0;


	static unsigned int rngState = 0;


	struct TuningDvar
	{
		const char* name;
		int Tuning::* field;
		int defaultValue;
		int minValue;
		int maxValue;
		const char* help;
		void* dvar;
	};

	static TuningDvar tuningDvars[] = {
		{ "bots_skill", &Tuning::skill, 5, 0, 10,
		  "how well every bot aims, reacts and reads the map; a change takes at each bot's next death\n"
		  "  0 = mixed lobby, each bot rolls its own level 1 to 7 on a bell curve around 4\n"
		  "  1 = clueless: 1 s to notice you, 0.6 s to swing on, 4 units of aim error, no recoil control, hears little, forgets you in 0.75 s\n"
		  "  2 = bad: 0.8 s reaction, wide sprays, still misses most of a fight\n"
		  "  3 = casual: 0.5 s reaction, starts to control recoil and hold angles\n"
		  "  4 = average: 0.4 s reaction, pre-aims corners, tracks a moving target\n"
		  "  5 = good: 0.3 s reaction, tight aim, punishes a peek\n"
		  "  6 = very good: 0.15 s reaction, almost no aim error, hears you coming\n"
		  "  7 = ruthless: 0.05 s reaction, 0.1 s swing, no aim error, full recoil control, remembers you for 7.5 s\n"
		  "  8 = adaptive: each bot plays up to three levels harder against a player whose K/D is over 1.0 and easier under it\n"
		  "  9 = chaos: every aim, reaction and habit number rolled per bot, so the lobby is uneven\n"
		  " 10 = adaptive hard: like 8 but aims to keep every player at a K/D of 0.75" },
		{ "bots_freeze", &Tuning::freeze, 0, 0, 1,
		  "0 off; 1 takes the bots off the stock AI and holds them still" },
		{ "bots_pingmin", &Tuning::pingMin, 20, 0, 999,
		  "lowest ping (ms) a bot shows on the scoreboard; each bot rolls its own between min and max at join" },
		{ "bots_pingmax", &Tuning::pingMax, 180, 0, 999,
		  "highest ping (ms) a bot shows on the scoreboard; each bot rolls its own between min and max at join" },
		{ "bots_class", &Tuning::botClass, 0, 0, ArchetypeCount,
		  "which play style every bot gets; a change takes at each bot's next death\n"
		  "  0 = mixed lobby, each bot draws its own style by weight: rusher 22, slayer 28, anchor 14, flanker 12, sniper 16, support 8, tuber 6, knifer 6, shotgunner 6\n"
		  "  1 = rusher: SMG, sprints at the fight, chases what it loses\n"
		  "  2 = slayer: assault rifle, steady all-rounder\n"
		  "  3 = anchor: LMG or rifle, holds ground and lanes, crouches a lot\n"
		  "  4 = flanker: silenced gun, goes round the long way, hits from behind\n"
		  "  5 = sniper: bolt rifle, every sniper quickscopes and drops the scope after the shot\n"
		  "  6 = support: LMG or rifle, sticks with teammates, plays the objective\n"
		  "  7 = noob tuber: grenade launcher first, one man army, danger close\n"
		  "  8 = knifer: melee only, never fires, sprints and lunges\n"
		  "  9 = shotgunner: akimbo shotguns, point blank rusher" },
		{ "bots_sniperlobby", &Tuning::sniperLobby, 0, 0, 1,
		  "0 off; 1 makes every bot a quickscoping sniper (Intervention or Barrett, akimbo pistols, the knife) that never holds the scope; takes at each bot's next death" },
		{ "bots_type", &Tuning::type, 1, 0, 1,
		  "0 roam: walk the waypoints and fight what turns up; 1 hunt: each bot picks an enemy (real players first) and heads for them" },
		{ "bots_roam", &Tuning::roam, 10, 0, 100,
		  "hunt mode only: percent of lives a bot spends roaming the map instead of hunting; 0 always hunts, 100 always roams" },
		{ "bots_max_follow", &Tuning::maxFollow, 2, 0, maxClients,
		  "most bots that hunt the same enemy, and teammates that roam to the same node; 0 no cap" },
		{ "bots_hunt_replan", &Tuning::huntReplan, 4000, 500, 20000,
		  "ms between a hunting bot re-checking where its enemy is and re-routing; lower is more relentless" },
		{ "bots_push", &Tuning::push, 60, 0, 100,
		  "percent chance a bot pushes to where it last saw a target it lost; 0 never, 100 always; scaled by each bot's aggression" },
		{ "bots_glance", &Tuning::glance, 30, 0, 100,
		  "percent chance, every few seconds, that a walking bot glances to one side; 0 off" },
		{ "bots_hear", &Tuning::hear, 400, 0, 2000,
		  "units within which a bot hears a moving enemy it cannot see and turns to it; gunfire carries 3x this, a silencer a quarter of that; 0 off" },
		{ "bots_ads_walk", &Tuning::adsWalk, 300, 0, 3000,
		  "units from a known enemy inside which a careful bot walks slow with the sight up (the reckless and quickscopers never do); 0 off" },
		{ "bots_burst", &Tuning::burst, 1, 0, 1,
		  "0 holds the trigger on full auto; 1 fires in bursts past 300 units, longer bursts the nearer" },
		{ "bots_prefire", &Tuning::prefire, 60, 0, 100,
		  "percent of the sight coming up at which a skill 4+ bot starts shooting; 100 waits for it fully; snipers always wait" },
		{ "bots_dropshot", &Tuning::dropshot, 100, 0, 300,
		  "percent scale on the per-skill drop shot and jump shot chances; 0 off, 100 the table, 300 triple" },
		{ "bots_swap", &Tuning::swap, 1, 0, 1,
		  "0 reloads where it stands; 1 switches to the pistol when the primary runs dry inside 400 units, back after the fight" },
		{ "bots_cover", &Tuning::cover, 1, 0, 1,
		  "0 off; 1 runs for a node the enemy cannot see to reload when dry, or to regen when low" },
		{ "bots_revenge", &Tuning::revenge, 25, 0, 100,
		  "percent of lives that go straight after the last killer; 0 never, 100 every life" },
		{ "bots_help", &Tuning::help, 800, 0, 3000,
		  "units within which idle teammates go after whoever just hit a bot; 0 off" },
		{ "bots_forceTarget", &Tuning::forceTarget, -1, -1, maxClients - 1,
		  "client number every bot that counts them as an enemy goes after; -1 off" },
		{ "bots_trace", &Tuning::trace, 1, 0, 1,
		  "1 writes every frame of the first two bots to a _trace.log beside the debug log while bots_debug is on; 0 off" },
		{ "bots_smooth", &Tuning::smooth, 1, 0, 1,
		  "0 walks node to node; 1 cuts corners when the walk to a node further along is clear" },
		{ "bots_tactical", &Tuning::tactical, 1, 0, 1,
		  "0 off; 1 throws the tactical grenade at a lost target's spot before pushing it" },
		{ "bots_kick", &Tuning::kick, 100, 0, 400,
		  "percent of the weapon file's view kick a shot puts on a bot's view: 100 is the client's own recoil, more reads stronger in a killcam, 0 off" },
		{ "bots_yy", &Tuning::yy, 35, 0, 100,
		  "percent chance, every second or two on the move, that a quickscoper flicks to its sidearm and back (and once after every shot); 0 off" },
		{ "bots_ignore_humans", &Tuning::ignoreHumans, 0, 0, 1,
		  "0 normal; 1 makes bots blind to real players, to watch them fight each other (BO1's sv_botsIgnoreHumans)" },
		{ "bots_fightmove", &Tuning::fightMove, 1, 0, 1,
		  "0 stops and strafes; 1 keeps walking the route while shooting a target beyond footwork range (BO1's strafe-on-path)" },
		{ "bots_spread", &Tuning::spread, 100, 0, 300,
		  "percent scale on how much a bot avoids ground its teammates just walked, so a squad spreads over the lanes; 0 off, 300 triple" },
		{ "bots_churn", &Tuning::churn, 10, 0, 100,
		  "percent of bots that leave the match on their own four to twelve minutes in, the fill bringing someone new; 0 nobody leaves" },
		{ "bots_reserve", &Tuning::reserve, 0, 0, 4,
		  "party seats kept open for people joining mid-match; 0 fills every seat and a joining party replaces bots to get in" },
		{ "bots_settings", &Tuning::settings, 0, 0, 2,
		  "0 idle; 1 loads main\\zw3\\bots\\botsettings.txt into the bots dvars; 2 writes the live values to it; goes back to 0 on its own" },
	};


	static char* ClientSlot(int clientNum)
	{
		return reinterpret_cast<char*>(svs_clients) + svClientStride * clientNum;
	}

	static bool IsSlotClaimedByMember(int clientNum)
	{
		if (!Session_GetXuidEvenIfInactive || clientNum >= sessionUserCount)
		{
			return false;
		}

		return Session_GetXuidEvenIfInactive(reinterpret_cast<void*>(lobbySession), clientNum) != 0;
	}

	static unsigned short RemotePortOf(int clientNum)
	{
		return *reinterpret_cast<const unsigned short*>(ClientSlot(clientNum) + svClientRemotePort);
	}

	bool IsFillBot(int clientNum)
	{
		if (clientNum < 0 || clientNum >= maxClients || !bots[clientNum].isFillBot)
		{
			return false;
		}

		const char* client = ClientSlot(clientNum);
		return *reinterpret_cast<const int*>(client + svClientState) != 0
			&& *reinterpret_cast<const int*>(client + svClientIsTest) != 0
			&& RemotePortOf(clientNum) == bots[clientNum].remotePort;
	}

	static int NextTestClientSlot()
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);

		for (int i = static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT); i < numClients; ++i)
		{
			if (!*reinterpret_cast<const int*>(ClientSlot(i) + svClientState))
			{
				return i;
			}
		}

		int start = 0;
		if (void* dvar = *reinterpret_cast<void**>(sv_privateClients))
		{
			start = *reinterpret_cast<int*>(static_cast<char*>(dvar) + dvarCurrent);
		}
		if (start < 0)
		{
			start = 0;
		}

		for (int i = start; i < numClients; ++i)
		{
			if (!*reinterpret_cast<const int*>(ClientSlot(i) + svClientState))
			{
				return i;
			}
		}

		return -1;
	}

	static int ScriptChildrenUsed()
	{
		return static_cast<int>(ClientSlots::ScriptChildHighest());
	}

	static int ScriptChildCapacity()
	{
		return static_cast<int>(ClientSlots::ScriptChildCapacity());
	}

	static bool HasParentRoom(int joining)
	{
		const int parentCapacity = static_cast<int>(ClientSlots::ScriptParentCapacity());

		if (parentCapacity == 0)
		{
			return true;
		}

		if (scriptParentBaseline < 0)
		{
			return false;
		}

		const int used = std::max(scriptParentsUsed, scriptParentPeak);
		const int margin = std::max(scriptParentMargin, parentCapacity / parentMarginDivisor);

		if (settledPlayerParents == 0)
		{
			const int joined = scriptPlayers - scriptBaselinePlayers;
			return joined < firstWaveSize && used + scriptPlayerParents * joining + margin <= parentCapacity;
		}

		const int wave = std::max(firstWaveSize, settledPlayers - scriptBaselinePlayers);
		const int next = std::max(scriptPlayerParents, 2 * settledPlayerParents);
		return joining <= wave && used + next * joining + margin <= parentCapacity;
	}

	static bool HasScriptRoom(int joining)
	{
		const int playerChildren = scriptPlayerChildren / static_cast<int>(ClientSlots::ScriptChildPools());
		const bool hasChildRoom = scriptPeak + playerChildren * joining + scriptAddMargin <= ScriptChildCapacity();
		const bool hasMemoryRoom = scriptMemoryBlocks - scriptPlayerMemoryBlocks * joining >= scriptMemoryBlockMargin;
		return hasChildRoom && hasMemoryRoom && HasParentRoom(joining);
	}

	bool HasScriptRoomForBot()
	{
		return HasScriptRoom(joiningBots + 1);
	}

	static int PlanBurst()
	{
		const int playerChildren = scriptPlayerChildren / static_cast<int>(ClientSlots::ScriptChildPools());
		const int childPlan = (ScriptChildCapacity() - ScriptChildrenUsed() - scriptAddMargin) / playerChildren;
		const int memoryPlan = (static_cast<int>(ClientSlots::ScriptMemoryFreeBlocks()) - burstMemoryBlockMargin) / burstPlayerMemoryBlocks;
		int plan = std::min(childPlan, memoryPlan);

		const int parentCapacity = static_cast<int>(ClientSlots::ScriptParentCapacity());

		if (parentCapacity > 0)
		{
			const int parentPlan = (parentCapacity - static_cast<int>(ClientSlots::ScriptParentsUsed()) - scriptParentMargin) / burstPlayerParents;
			plan = std::min(plan, parentPlan);
		}

		BotLog("burst plan: %d players, children %d of %d, memory blocks %d, parents %d of %d", std::max(plan, 0),
			ScriptChildrenUsed(), ScriptChildCapacity(), static_cast<int>(ClientSlots::ScriptMemoryFreeBlocks()),
			static_cast<int>(ClientSlots::ScriptParentsUsed()), parentCapacity);

		return std::max(plan, 0);
	}

	int ReadDvar(void* dvar)
	{
		if (!dvar)
		{
			return 0;
		}
		return *reinterpret_cast<int*>(reinterpret_cast<char*>(dvar) + dvarCurrent);
	}


	const char* TuningHelp(const char* name)
	{
		for (const TuningDvar& entry : tuningDvars)
		{
			if (_stricmp(entry.name, name) == 0)
			{
				return entry.help;
			}
		}
		return nullptr;
	}


	static void RegisterTuning()
	{
		for (TuningDvar& entry : tuningDvars)
		{
			entry.dvar = Dvar_RegisterInt(
				entry.name, entry.defaultValue, entry.minValue, entry.maxValue, 0, entry.help);
		}
	}


	static void RefreshTuning()
	{
		for (const TuningDvar& entry : tuningDvars)
		{
			tuning.*entry.field = ReadDvar(entry.dvar);
		}
	}


	bool BotsPath(const char* file, char* out, std::size_t outSize)
	{
		if (!*Game::fs_basepath)
		{
			return false;
		}

		const std::string folder = std::format("{}\\main\\zw3\\bots\\", (*Game::fs_basepath)->current.string);
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		std::snprintf(out, outSize, "%s%s", folder.c_str(), file);
		return true;
	}

	static FILE* botLogFile = nullptr;
	static FILE* traceLogFile = nullptr;
	static SYSTEMTIME logOpenTime = {};
	static bool hasLogOpenTime = false;

	static FILE* OpenStucklog(const char* suffix)
	{
		char folder[MAX_PATH * 2];
		if (!BotsPath("Stucklog", folder, sizeof(folder)))
		{
			return nullptr;
		}
		CreateDirectoryA(folder, nullptr);

		const char* mapLabel = "nomap";
		if (*activeMap)
		{
			mapLabel = activeMap;
		}

		if (!hasLogOpenTime)
		{
			GetLocalTime(&logOpenTime);
			hasLogOpenTime = true;
		}

		char path[MAX_PATH * 2];
		std::snprintf(path, sizeof(path), "%s\\%04d-%02d-%02d_%02d-%02d-%02d_%s%s.log", folder,
			logOpenTime.wYear, logOpenTime.wMonth, logOpenTime.wDay, logOpenTime.wHour, logOpenTime.wMinute,
			logOpenTime.wSecond, mapLabel, suffix);
		return _fsopen(path, "a", _SH_DENYNO);
	}

	void AppendBotLog(const char* line)
	{
		if (!botLogFile)
		{
			botLogFile = OpenStucklog("");
			if (!botLogFile)
			{
				return;
			}
		}
		std::fprintf(botLogFile, "%s\n", line);
		std::fflush(botLogFile);
	}

	static void AppendTraceLog(const char* line)
	{
		if (!traceLogFile)
		{
			traceLogFile = OpenStucklog("_trace");
			if (!traceLogFile)
			{
				return;
			}
		}
		std::fprintf(traceLogFile, "%s\n", line);
	}


	void BotLog(const char* format, ...)
	{
		if (!debugOn)
		{
			return;
		}

		char body[512];
		va_list args;
		va_start(args, format);
		_vsnprintf_s(body, sizeof(body), _TRUNCATE, format, args);
		va_end(args);

		char line[544];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "%d %s", ServerTimeMs(), body);
		AppendBotLog(line);
	}


	static void DropBot(int clientNum)
	{
		BotLog("drop client %d", clientNum);
		char* client = ClientSlot(clientNum);
		SV_DropClientFn(client, "EXE_DISCONNECTED", 0);
		*reinterpret_cast<int*>(client + svClientState) = 0;
		bots[clientNum].spawnStage = 0;

		if (bots[clientNum].isFillBot)
		{
			ClearPlayerData(clientNum);
		}
	}


	void RescueBot(int clientNum)
	{
		if (!bots[clientNum].isFillBot)
		{
			return;
		}

		DropBot(clientNum);
	}


	static int AngleToShort(float degrees)
	{
		return static_cast<int>(degrees * (65536.0f / 360.0f)) & 0xFFFF;
	}


	static bool IsRealGun(int index)
	{
		const char* weaponDef = WeaponDefOf(index);
		if (!weaponDef || *reinterpret_cast<const int*>(weaponDef + weapDefClass) == weapClassGrenade)
		{
			return false;
		}

		const char* name = WeaponNameOf(index);
		if (name
			&& (std::strncmp(name, "killstreak_", 11) == 0
				|| std::strcmp(name, "airdrop_marker_mp") == 0
				|| std::strstr(name, "throwingknife") != nullptr
				|| std::strstr(name, "claymore") != nullptr
				|| std::strstr(name, "c4_mp") != nullptr
				|| std::strstr(name, "grenade") != nullptr
				|| std::strstr(name, "semtex") != nullptr))
		{
			return false;
		}
		return true;
	}


	static bool IsPistolClass(int index)
	{
		const char* weaponDef = WeaponDefOf(index);
		return weaponDef && *reinterpret_cast<const int*>(weaponDef + weapDefClass) == weapClassPistol;
	}


	static bool IsRealPrimary(int index)
	{
		return IsRealGun(index) && !IsPistolClass(index);
	}


	static bool IsRealPistol(int index)
	{
		return IsRealGun(index) && IsPistolClass(index);
	}


	unsigned short FirstHeldRealWeapon(const char* playerState)
	{
		const int primary = FindHeldWeapon(playerState, IsRealPrimary);
		if (primary)
		{
			return static_cast<unsigned short>(primary);
		}
		return static_cast<unsigned short>(FindHeldWeapon(playerState, IsRealPistol));
	}


	static bool IsHeldWeapon(const char* playerState, unsigned short index)
	{
		if (index == 0)
		{
			return false;
		}
		return FindHeldWeapon(playerState, [index](int held)
		{
			return held == static_cast<int>(index);
		}) != 0;
	}

	static unsigned short AnyHeldWeapon(const char* playerState)
	{
		return static_cast<unsigned short>(FindHeldWeapon(playerState, [](int)
		{
			return true;
		}));
	}


	unsigned short FindSecondGun(const char* playerState, unsigned short first)
	{
		const int found = FindHeldWeapon(playerState, [first](int index)
		{
			return index != first && IsRealGun(index);
		});
		return static_cast<unsigned short>(found);
	}


	static constexpr int skipCamTapFrameCount = 2;
	static constexpr int skipCamTapGapMs  = 450;

	static constexpr int adsMinOnMs = 450;
	static constexpr int adsMinOffMs = 350;
	static constexpr int adsFlipWindowMs = 1500;
	static constexpr int adsFlipLogGapMs = 3000;
	static constexpr int crouchMinMs = 700;
	static constexpr int proneMinMs = 1200;
	static constexpr signed char sprintStopForward = 100;
	static constexpr float adsReleaseTurnDeg = 6.0f;
	static constexpr float traceOpenReach = 2000.0f;


	static float WrapDeg(float delta)
	{
		float wrapped = std::fmod(delta + 180.0f, 360.0f);
		if (wrapped < 0.0f)
		{
			wrapped += 360.0f;
		}
		return wrapped - 180.0f;
	}

	static void ButtonLetters(int buttons, char* out)
	{
		static const int bits[] = { cmdButtonAttack, cmdButtonAds, cmdButtonSprint, cmdButtonCrouch, cmdButtonProne,
									cmdButtonUp, cmdButtonMelee, cmdButtonReload, cmdButtonFrag, cmdButtonSpecial,
									cmdButtonBreath, cmdButtonUse };
		static const char letters[] = "ADSCPJMRFTBU";
		int count = 0;
		for (int k = 0; k < 12; ++k)
		{
			if ((buttons & bits[k]) != 0)
			{
				out[count] = letters[k];
				++count;
			}
		}
		if (count == 0)
		{
			out[count] = '-';
			++count;
		}
		out[count] = 0;
	}

	static void TraceFrame(int clientNum, const BotInput& input, int sentButtons, int forward, const char* playerState,
						   unsigned short held, unsigned short asked)
	{
		BotState& bot = bots[clientNum];
		char wanted[16];
		char sent[16];
		ButtonLetters(input.buttons, wanted);
		ButtonLetters(sentButtons, sent);

		char turns[200] = "";
		int shown = bot.turnCount;
		if (shown > turnNoteSlots)
		{
			shown = turnNoteSlots;
		}
		for (int k = 0; k < shown; ++k)
		{
			const std::size_t used = std::strlen(turns);
			_snprintf_s(turns + used, sizeof(turns) - used, _TRUNCATE, " %s:%.0f", bot.turnTags[k], bot.turnYaws[k]);
		}

		const float* origin = reinterpret_cast<const float*>(playerState + psOrigin);
		const float* velocity = reinterpret_cast<const float*>(playerState + psVelocity);
		const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]);
		const float yawStep = WrapDeg(input.angles[1] - bot.traceLastYaw);
		const float pitchStep = WrapDeg(input.angles[0] - bot.traceLastPitch);
		bot.traceLastYaw = input.angles[1];
		bot.traceLastPitch = input.angles[0];

		const char* aiLabel = "-";
		if (bot.aiMode)
		{
			aiLabel = bot.aiMode;
		}
		const char* navLabel = "-";
		if (bot.navMode)
		{
			navLabel = bot.navMode;
		}
		const char* taskSource = "-";
		if (bot.task.source)
		{
			taskSource = bot.task.source;
		}
		const char* heldName = "none";
		if (held)
		{
			heldName = BG_GetWeaponName(held);
		}
		const char* askedName = "none";
		if (asked)
		{
			askedName = BG_GetWeaponName(asked);
		}

		float targetDistance = 0.0f;
		if (bot.targetClient >= 0)
		{
			const float* targetOrigin = reinterpret_cast<const float*>(PlayerStateOf(bot.targetClient) + psOrigin);
			const float dx = targetOrigin[0] - origin[0];
			const float dy = targetOrigin[1] - origin[1];
			const float dz = targetOrigin[2] - origin[2];
			targetDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		const float eye[3] = { origin[0], origin[1], origin[2] + *reinterpret_cast<const float*>(playerState + psViewHeight) };
		const float viewYawRad = input.angles[1] / radToDeg;
		const float viewPitchRad = input.angles[0] / radToDeg;
		const float viewEnd[3] = {
			eye[0] + std::cos(viewPitchRad) * std::cos(viewYawRad) * traceOpenReach,
			eye[1] + std::cos(viewPitchRad) * std::sin(viewYawRad) * traceOpenReach,
			eye[2] - std::sin(viewPitchRad) * traceOpenReach,
		};
		const float noBounds[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		const int noIgnore[4] = { -1, -1, 0, 0 };
		unsigned char viewTrace[128] = {};
		SV_Trace(viewTrace, eye, viewEnd, noBounds, noIgnore, maskSight, 0, nullptr, 1);
		const float viewOpen = *reinterpret_cast<const float*>(viewTrace + traceFraction) * traceOpenReach;

		char line[768];
		_snprintf_s(line, sizeof(line), _TRUNCATE,
			"%d c%d %s/%s task %d:%s pos %.0f %.0f %.0f spd %.0f view %.1f %.1f step %.1f %.1f kick %.2f %.2f"
			" turns %d%s want %s sent %s fwd %d right %d held %s asked %s ads %.2f tgt %d dist %.0f trace %d notrace %d"
			" open %.0f",
			ServerTimeMs(), clientNum, aiLabel, navLabel, bot.task.kind, taskSource, origin[0], origin[1], origin[2],
			speed, input.angles[1], input.angles[0], yawStep, pitchStep, bot.kickAngle[1], bot.kickAngle[0],
			bot.turnCount, turns, wanted, sent, forward, static_cast<int>(input.right), heldName, askedName,
			*reinterpret_cast<const float*>(playerState + psAdsAmount), bot.targetClient, targetDistance, bot.traceTime,
			bot.noTraceTime, viewOpen);
		AppendTraceLog(line);
	}


	static int LimitToggle(int buttons, int bit, int sentButtons, int& changeTime, int now, int minOnMs, int minOffMs)
	{
		const bool wantsOn = (buttons & bit) != 0;
		const bool wasOn = (sentButtons & bit) != 0;
		if (wantsOn == wasOn)
		{
			return buttons;
		}

		int minMs = minOffMs;
		if (wasOn)
		{
			minMs = minOnMs;
		}
		if (now - changeTime < minMs)
		{
			if (wasOn)
			{
				return buttons | bit;
			}
			return buttons & ~bit;
		}
		changeTime = now;
		return buttons;
	}


	static void BuildUserCmd(int clientNum, const BotInput& input, char* cmd)
	{
		for (int i = 0; i < cmdSize; ++i)
		{
			cmd[i] = 0;
		}

		const char* playerState = PlayerStateOf(clientNum);
		const float* deltaAngles = reinterpret_cast<const float*>(playerState + psDeltaAngles);

		const int now = ServerTimeMs();
		*reinterpret_cast<int*>(cmd + cmdServerTime) = now + input.serverTimeBias;

		BotState& limited = bots[clientNum];
		int buttons = input.buttons;
		const unsigned short heldNow = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		const char* heldName = nullptr;
		if (heldNow)
		{
			heldName = WeaponNameOf(heldNow);
		}
		const bool isHoldingStreakRemote = limited.ksPhase == 0 && heldName && std::strncmp(heldName, "killstreak_", 11) == 0;
		const bool isHoldingBag = heldName && std::strncmp(heldName, "onemanarmy", 10) == 0;
		if (isHoldingStreakRemote)
		{
			buttons &= ~(cmdButtonAttack | cmdButtonAds);
		}
		if ((input.buttons & cmdButtonAttack) != 0 && heldName && std::strstr(heldName, "_akimbo") != nullptr)
		{
			buttons |= cmdButtonThrow;
		}
		int adsHoldMs = adsMinOnMs;
		const float turnStepDeg = std::fabs(WrapDeg(input.angles[1] - limited.lastCmdYaw));
		limited.lastCmdYaw = input.angles[1];
		if (turnStepDeg > adsReleaseTurnDeg)
		{
			adsHoldMs = 0;
		}
		buttons = LimitToggle(buttons, cmdButtonAds, limited.sentButtons, limited.adsChangeTime, now, adsHoldMs, adsMinOffMs);
		if (((input.buttons | buttons) & (cmdButtonAds | cmdButtonAttack | cmdButtonCrouch | cmdButtonProne)) != 0)
		{
			buttons &= ~cmdButtonSprint;
		}
		int crouchHoldMs = crouchMinMs;
		int proneHoldMs = proneMinMs;
		if ((buttons & cmdButtonUp) != 0)
		{
			crouchHoldMs = 0;
			proneHoldMs = 0;
		}
		buttons = LimitToggle(buttons, cmdButtonCrouch, limited.sentButtons, limited.crouchChangeTime, now, crouchHoldMs, crouchMinMs);
		buttons = LimitToggle(buttons, cmdButtonProne, limited.sentButtons, limited.proneChangeTime, now, proneHoldMs, proneMinMs);
		if (((buttons ^ limited.sentButtons) & cmdButtonAds) != 0)
		{
			const int oldest = limited.adsFlipTimes[limited.adsFlipNext];
			limited.adsFlipTimes[limited.adsFlipNext] = now;
			limited.adsFlipNext = (limited.adsFlipNext + 1) % 3;
			if (oldest != 0 && now - oldest < adsFlipWindowMs && now - limited.adsFlipLogTime > adsFlipLogGapMs)
			{
				limited.adsFlipLogTime = now;
				const char* aiLabel = "-";
				if (limited.aiMode)
				{
					aiLabel = limited.aiMode;
				}
				const char* navLabel = "-";
				if (limited.navMode)
				{
					navLabel = limited.navMode;
				}
				BotLog("adsflip client %d three flips in %d ms, mode %s/%s target %d", clientNum, now - oldest,
					aiLabel, navLabel, limited.targetClient);
			}
		}
		limited.sentButtons = buttons;
		*reinterpret_cast<int*>(cmd + cmdButtons) = buttons;

		for (int i = 0; i < 3; ++i)
		{
			reinterpret_cast<int*>(cmd + cmdAngles)[i] = AngleToShort(input.angles[i] - deltaAngles[i]);
		}

		BotState& bot = bots[clientNum];
		const unsigned short held = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		if (held && bot.ksPhase == 0 && !bot.swapWeapon && bot.yyFrames == 0 && !isHoldingStreakRemote && !isHoldingBag)
		{
			bot.lastHeldWeapon = held;
		}

		unsigned short weapon = input.weapon ? input.weapon : held;
		if (isHoldingStreakRemote && weapon == held)
		{
			weapon = FirstHeldRealWeapon(playerState);
		}
		if (!IsHeldWeapon(playerState, weapon))
		{
			weapon = held;
		}
		if (!IsHeldWeapon(playerState, weapon))
		{
			weapon = bot.lastHeldWeapon;
		}
		if (!IsHeldWeapon(playerState, weapon))
		{
			weapon = FirstHeldRealWeapon(playerState);
		}
		if (!IsHeldWeapon(playerState, weapon))
		{
			weapon = AnyHeldWeapon(playerState);
		}
		if (weapon != held && weapon != bot.lastAskedWeapon)
		{
			BotLog("hand client %d asks for %s, held %s", clientNum, BG_GetWeaponName(weapon), held ? BG_GetWeaponName(held) : "none");
		}
		bot.lastAskedWeapon = weapon;

		unsigned short altWeapon = input.altWeapon;
		const char* weaponDef = weapon ? WeaponDefOf(weapon) : nullptr;
		if (weaponDef && *reinterpret_cast<const int*>(weaponDef + weapDefInventoryType) == weapInventoryAltMode)
		{
			const int base = FindHeldWeapon(playerState, [weapon](int held)
			{
				const char* completeDef = BG_GetWeaponCompleteDef(held);
				return completeDef
					&& *reinterpret_cast<const int*>(completeDef + weaponCompleteAltWeaponIndex) == static_cast<int>(weapon);
			});
			if (base)
			{
				altWeapon = static_cast<unsigned short>(base);
			}
		}
		if (input.forward != 0 || input.right != 0)
		{
			++perfMovingFrames;
			if ((*reinterpret_cast<const int*>(playerState + psPmFlags) & pmFlagSprint) != 0)
			{
				++perfSprintFrames;
			}
		}

		*reinterpret_cast<unsigned short*>(cmd + cmdWeapon)        = weapon;
		*reinterpret_cast<unsigned short*>(cmd + cmdAltWeapon)     = altWeapon;
		*reinterpret_cast<unsigned short*>(cmd + cmdOffhandWeapon) = input.offhandWeapon;

		signed char forward = input.forward;
		const int pmFlags = *reinterpret_cast<const int*>(playerState + psPmFlags);
		if ((pmFlags & pmFlagSprint) != 0 && (buttons & cmdButtonSprint) == 0 && forward > sprintStopForward)
		{
			forward = sprintStopForward;
		}
		cmd[cmdForwardMove] = forward;
		cmd[cmdRightMove]   = input.right;

		*reinterpret_cast<float*>(cmd + cmdMeleeYaw) = input.meleeYaw;
		cmd[cmdMeleeDist] = static_cast<char>(input.meleeDist);

		for (int i = 0; i < 3; ++i)
		{
			cmd[cmdSelectedLoc + i] = input.selectedLocation[i];
		}
		for (int i = 0; i < 2; ++i)
		{
			cmd[cmdRemoteAngles + i] = input.remoteAngles[i];
		}

		if (limited.isTraced)
		{
			TraceFrame(clientNum, input, buttons, forward, playerState, held, weapon);
		}
	}


	bool ThinkBot(Game::client_s* clientState)
	{
		char* client = reinterpret_cast<char*>(clientState);
		const int clientNum =
			static_cast<int>((client - reinterpret_cast<char*>(svs_clients)) / svClientStride);
		if (clientNum < 0 || clientNum >= maxClients)
		{
			return false;
		}

		BotState& bot = bots[clientNum];
		const BotInput& input = bot.input;
		if (!*reinterpret_cast<void**>(client + svClientGentity))
		{
			return false;
		}

		char cmd[cmdSize];
		if (!input.isActive)
		{
			if (bot.deadSince == 0)
			{
				return false;
			}
			const int now = ServerTimeMs();
			if (bot.skipCamTapFrames == 0 && now >= bot.skipCamNextTime)
			{
				bot.skipCamTapFrames = skipCamTapFrameCount;
				bot.skipCamNextTime = now + skipCamTapGapMs;
				if (bot.skipCamTaps == 0)
				{
					BotLog("skipcam client %d taps use %d ms after dying", clientNum, now - bot.deadSince);
				}
				++bot.skipCamTaps;
			}
			if (bot.skipCamTapFrames == 0)
			{
				return false;
			}
			--bot.skipCamTapFrames;

			BotInput tap = input;
			tap.buttons = cmdButtonUse;
			tap.forward = 0;
			tap.right = 0;
			tap.weapon = 0;
			BuildUserCmd(clientNum, tap, cmd);
			*reinterpret_cast<int*>(client + svClientDeltaMessage) =
				*reinterpret_cast<int*>(client + svClientOutgoingSeq) - 1;
			SV_ClientThink(client, cmd);
			return true;
		}

		BuildUserCmd(clientNum, input, cmd);

		*reinterpret_cast<int*>(client + svClientDeltaMessage) =
			*reinterpret_cast<int*>(client + svClientOutgoingSeq) - 1;
		SV_ClientThink(client, cmd);
		return true;
	}


	static void ApplyIdentity(int clientNum, char* client)
	{
		char* entity = *reinterpret_cast<char**>(client + svClientGentity);
		if (!entity)
		{
			return;
		}

		char* gclient = *reinterpret_cast<char**>(entity + gentityClient);
		if (!gclient)
		{
			return;
		}

		const bool isAlive = *reinterpret_cast<int*>(entity + gentityHealth) > 0
			&& *reinterpret_cast<int*>(gclient + gclientSessionState) == 0;

		if (isAlive)
		{
			bots[clientNum].hasSpawnedOnce = true;
		}

		const int sessionState = *reinterpret_cast<int*>(gclient + gclientSessionState);
		if (bots[clientNum].wasAlive && !isAlive && sessionState <= 1)
		{
			const int kills = *reinterpret_cast<const int*>(gclient + gclientKills);
			if (kills < bots[clientNum].killsAtLastDeath)
			{
				bots[clientNum].killsAtLastDeath = 0;
			}
			if (kills > bots[clientNum].killsAtLastDeath)
			{
				bots[clientNum].deathsWithoutKill = 0;
			}
			else
			{
				++bots[clientNum].deathsWithoutKill;
			}
			bots[clientNum].killsAtLastDeath = kills;

			int changePercent = classChangePercent;
			if (bots[clientNum].deathsWithoutKill >= classChangeLosingDeaths)
			{
				changePercent = classChangeLosingPercent;
			}
			if (PersonalitySettingChanged(clientNum))
			{
				RollPersonality(clientNum);
			}
			else if (RollPercent(changePercent))
			{
				RerollLoadout(clientNum);
				bots[clientNum].deathsWithoutKill = 0;
			}
			TryWriteLoadout(clientNum, bots[clientNum].loadout);
		}
		bots[clientNum].wasAlive = isAlive;

		const int team = *reinterpret_cast<int*>(gclient + gclientTeam);
		const bool lostTeam = bots[clientNum].hasSpawnedOnce && !isAlive && IsTeamBased() && team != 1 && team != 2;
		if (bots[clientNum].spawnStage || (bots[clientNum].hasSpawnedOnce && !lostTeam))
		{
			bots[clientNum].unassignedFrames = 0;
		}
		else if (++bots[clientNum].unassignedFrames > (lostTeam ? lostTeamRetryFrames : unassignedRetryFrames))
		{
			if (lostTeam)
			{
				BotLog("reteam client %d sits on team %d in session state %d, menus again", clientNum, team, sessionState);
			}
			bots[clientNum].unassignedFrames = 0;
			bots[clientNum].spawnStage = stageTeam;
			bots[clientNum].stageCountdown = menuStepFrames;
		}

		const Identity identity = bots[clientNum].identity.name
			? bots[clientNum].identity
			: IdentityFor(bots[clientNum].ordinal);
		*reinterpret_cast<int*>(gclient + gclientRank) = identity.rank;
		*reinterpret_cast<int*>(gclient + gclientPrestige) = identity.prestige;
		*reinterpret_cast<int*>(gclient + gclientCardIcon) = identity.cardIcon;
		*reinterpret_cast<int*>(gclient + gclientCardTitle) = identity.cardTitle;
		*reinterpret_cast<int*>(gclient + gclientCardNameplate) = identity.cardNameplate;
	}


	unsigned int NextRand()
	{
		if (!rngState)
		{
			rngState = static_cast<unsigned int>(Sys_Milliseconds()) | 1u;
		}

		unsigned int x = rngState;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		rngState = x;
		return x;
	}


	float Flrand(float low, float high)
	{
		return low + (static_cast<float>(NextRand() % 10001u) / 10000.0f) * (high - low);
	}


	int IrandMs(int low, int high)
	{
		if (high <= low)
		{
			return low;
		}
		return low + static_cast<int>(NextRand() % static_cast<unsigned int>(high - low + 1));
	}


	int JitterMs(int ms)
	{
		return IrandMs(ms * 7 / 10, ms * 13 / 10);
	}


	bool RollPercent(int percent)
	{
		return static_cast<int>(NextRand() % 100u) < percent;
	}


	static void UpdatePing(int clientNum, char* client)
	{
		int low = tuning.pingMin;
		int high = tuning.pingMax;
		if (high < low)
		{
			const int swap = low;
			low = high;
			high = swap;
		}

		BotState& bot = bots[clientNum];
		if (bot.pingBase <= 0)
		{
			bot.pingBase = IrandMs(low, high);
			if (RollPercent(pingOutlierPercent))
			{
				bot.pingBase = high + IrandMs(pingOutlierMin, pingOutlierMax);
			}
			bot.ping = bot.pingBase;
			bot.pingNextTick = debugTick;
		}

		if (debugTick >= bot.pingNextTick)
		{
			bot.pingNextTick = debugTick + pingRefreshMin
				+ static_cast<int>(NextRand() % static_cast<unsigned int>(pingRefreshSpan));

			if (RollPercent(pingWanderPercent))
			{
				bot.pingBase += (NextRand() & 1u) ? 1 : -1;
			}
			if (bot.pingBase < low)
			{
				bot.pingBase = low;
			}
			if (bot.pingBase > high + pingOutlierMax)
			{
				bot.pingBase = high + pingOutlierMax;
			}

			int shown = bot.pingBase + IrandMs(pingJitterMin, pingJitterMax);
			if (RollPercent(pingSpikePercent))
			{
				shown += IrandMs(pingSpikeMin, pingSpikeMax);
			}

			if (shown < low)
			{
				shown = low;
			}
			if (shown > high + pingOutlierMax + pingSpikeMax)
			{
				shown = high + pingOutlierMax + pingSpikeMax;
			}
			bot.ping = shown;
		}

		*reinterpret_cast<int*>(client + svClientPing) = bot.ping;
	}


	int ChooseSkill()
	{
		const int configured = tuning.skill;
		if (configured >= 1 && configured <= skillCount)
		{
			return configured - 1;
		}

		const float u1 = (static_cast<float>(NextRand() % 10000u) + 1.0f) / 10001.0f;
		const float u2 = static_cast<float>(NextRand() % 10000u) / 10000.0f;
		const float normal = std::sqrt(-2.0f * std::log(u1)) * std::cos(6.2831853f * u2);
		int skill = static_cast<int>(3.5f + 1.75f * normal + 0.5f);
		if (skill < 1)
		{
			skill = 1;
		}
		if (skill > skillCount)
		{
			skill = skillCount;
		}
		return skill - 1;
	}


	bool IsSkillFixed()
	{
		const int configured = tuning.skill;
		return configured >= 1 && configured <= skillCount;
	}


	int SkillSetting()
	{
		return tuning.skill;
	}


	void RollSkill(int clientNum)
	{
		BotState& bot = bots[clientNum];
		bot.skillSettingSeen = SkillSetting();

		if (SkillSetting() == skillRandom)
		{
			bot.skillIndex = static_cast<int>(NextRand() % static_cast<unsigned int>(skillCount));
			BotSkill& row = bot.skillRow;
			row.aimTime = Flrand(0.1f, 0.6f);
			row.reactionTime = IrandMs(100, 1000);
			row.noTraceLookTime = IrandMs(500, 4000);
			row.noTraceAdsTime = IrandMs(500, 2500);
			row.rememberTime = IrandMs(750, 7500);
			row.fov = Flrand(0.866f, 0.940f);
			row.distStart = Flrand(1000.0f, 10000.0f);
			row.distMax = row.distStart * 2.0f;
			row.aimOffsetTime = Flrand(0.0f, 1.5f);
			row.aimOffsetAmount = Flrand(0.0f, 4.0f);
			row.semiTime = Flrand(0.1f, 0.9f);
			row.strafeChance = IrandMs(0, 65);
			row.shootAfterMs = IrandMs(0, 1000);
		}
		else
		{
			bot.skillIndex = ChooseSkill();
			bot.skillRow = skills[bot.skillIndex];
		}
		bot.baseSkillIndex = bot.skillIndex;

		bot.reactionScale = Flrand(skillJitterMin, skillJitterMax);
		bot.aimTimeScale = Flrand(skillJitterMin, skillJitterMax);
		bot.aimErrorScale = Flrand(skillJitterMin, skillJitterMax);
		bot.skillRow.reactionTime = static_cast<int>(static_cast<float>(bot.skillRow.reactionTime) * bot.reactionScale);
		bot.skillRow.aimTime *= bot.aimTimeScale;

		bot.dropshotPercent = dropshotPercentBySkill[bot.skillIndex];
		bot.jumpshotPercent = jumpshotPercentBySkill[bot.skillIndex];
		BotLog("skill client %d rolled %d under bots_skill %d", clientNum, bot.skillIndex + 1, tuning.skill);
	}


	static int humanKills[maxClients] = {};
	static int humanDeaths[maxClients] = {};

	static void ReadHumanScore(int clientNum)
	{
		const char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
		const char* gclient = *reinterpret_cast<char* const*>(entity + gentityClient);
		if (!gclient)
		{
			humanKills[clientNum] = 0;
			humanDeaths[clientNum] = 0;
			return;
		}
		humanKills[clientNum] = *reinterpret_cast<const int*>(gclient + gclientKills);
		humanDeaths[clientNum] = *reinterpret_cast<const int*>(gclient + gclientDeaths);
	}


	bool IsSkillAdaptive()
	{
		return tuning.skill == skillAdaptive || tuning.skill == skillAdaptiveHard;
	}


	static float HumanKd(int clientNum)
	{
		return (static_cast<float>(humanKills[clientNum]) + 1.0f) / (static_cast<float>(humanDeaths[clientNum]) + 1.0f);
	}


	static int AdaptiveSkillIndex(int baseIndex, int humanClient)
	{
		const float target = tuning.skill == skillAdaptiveHard ? adaptiveHardKdTarget : adaptiveKdTarget;
		float pressure = (HumanKd(humanClient) - target) / target;
		if (pressure < -1.0f)
		{
			pressure = -1.0f;
		}
		if (pressure > 1.0f)
		{
			pressure = 1.0f;
		}
		int index = baseIndex + static_cast<int>(std::lround(pressure * static_cast<float>(adaptiveShiftMax)));
		if (index < 0)
		{
			index = 0;
		}
		if (index >= skillCount)
		{
			index = skillCount - 1;
		}
		return index;
	}


	void ApplyFightSkill(int clientNum, int targetClient)
	{
		BotState& bot = bots[clientNum];
		int index = bot.baseSkillIndex;
		const bool againstHuman = targetClient >= 0 && targetClient < maxClients
			&& *reinterpret_cast<const int*>(ClientSlot(targetClient) + svClientIsTest) == 0;
		if (IsSkillAdaptive() && againstHuman)
		{
			index = AdaptiveSkillIndex(bot.baseSkillIndex, targetClient);
		}
		if (index == bot.skillIndex || tuning.skill == skillRandom)
		{
			return;
		}
		bot.skillIndex = index;
		bot.skillRow = skills[index];
		bot.skillRow.reactionTime = static_cast<int>(static_cast<float>(bot.skillRow.reactionTime) * bot.reactionScale);
		bot.skillRow.aimTime *= bot.aimTimeScale;
		bot.dropshotPercent = dropshotPercentBySkill[index];
		bot.jumpshotPercent = jumpshotPercentBySkill[index];
		if (againstHuman)
		{
			BotLog("skill client %d plays %d against client %d (base %d, their kd %.2f)", clientNum, index + 1,
				targetClient, bot.baseSkillIndex + 1, HumanKd(targetClient));
		}
		else
		{
			BotLog("skill client %d back to its own %d", clientNum, index + 1);
		}
	}


	static const char* const extraSettingNames[] = { "bots", "sv_bots_fill", "bots_debug", "bots_autogen" };

	static bool SaveSettingsFile()
	{
		char path[MAX_PATH * 2];
		if (!BotsPath("botsettings.txt", path, sizeof(path)))
		{
			return false;
		}
		FILE* file = nullptr;
		if (fopen_s(&file, path, "w") != 0 || !file)
		{
			Print("bots: could not write %s\n", path);
			return false;
		}
		std::fprintf(file, "// bot settings: read once at startup, and again with bots_settings 1;\n");
		std::fprintf(file, "// bots_settings 2 rewrites this file from the live values. one dvar a line: name value\n\n");
		for (const char* name : extraSettingNames)
		{
			int value = 0;
			if (TryReadDvarInt(name, value))
			{
				std::fprintf(file, "%s %d\n", name, value);
			}
		}
		std::fprintf(file, "\n");
		for (const TuningDvar& entry : tuningDvars)
		{
			if (std::strcmp(entry.name, "bots_settings") == 0)
			{
				continue;
			}
			const char* helpLine = entry.help;
			while (helpLine && *helpLine)
			{
				const char* lineEnd = std::strchr(helpLine, '\n');
				const int lineLength = lineEnd ? static_cast<int>(lineEnd - helpLine) : static_cast<int>(std::strlen(helpLine));
				std::fprintf(file, "// %.*s\n", lineLength, helpLine);
				helpLine = lineEnd ? lineEnd + 1 : nullptr;
			}
			std::fprintf(file, "// (default %d, %d to %d)\n%s %d\n\n", entry.defaultValue, entry.minValue,
				entry.maxValue, entry.name, ReadDvar(entry.dvar));
		}
		std::fclose(file);
		Print("bots: wrote %s\n", path);
		BotLog("settings written to %s", path);
		return true;
	}


	static bool LoadSettingsFile(bool writeWhenMissing)
	{
		char path[MAX_PATH * 2];
		if (!BotsPath("botsettings.txt", path, sizeof(path)))
		{
			return false;
		}
		FILE* file = nullptr;
		if (fopen_s(&file, path, "r") != 0 || !file)
		{
			if (writeWhenMissing)
			{
				SaveSettingsFile();
			}
			return false;
		}
		char line[512];
		int applied = 0;
		int skipped = 0;
		while (std::fgets(line, sizeof(line), file))
		{
			char name[64] = {};
			int value = 0;
			if (line[0] == '/' || line[0] == '#' || sscanf_s(line, "%63s %d", name, static_cast<unsigned int>(sizeof(name)), &value) != 2)
			{
				continue;
			}
			if (std::strcmp(name, "bots_settings") == 0)
			{
				continue;
			}
			const bool isOurs = std::strncmp(name, "bots", 4) == 0 || std::strcmp(name, "sv_bots_fill") == 0;
			if (!isOurs || !TrySetDvarInt(name, value))
			{
				++skipped;
				continue;
			}
			++applied;
		}
		std::fclose(file);
		Print("bots: %d settings loaded from %s, %d lines skipped\n", applied, path, skipped);
		BotLog("settings loaded %d from %s, %d skipped", applied, path, skipped);
		return true;
	}


	void SendMenuResponse(void* entity, const char* response, const char* menu)
	{
		const auto addString = Scr_AddString;
		addString(response);
		addString(menu);
		Scr_Notify(
			entity, *reinterpret_cast<unsigned short*>(scr_const_menuresponse), 2);
	}


	static int BalancedTeam()
	{
		static int lastPick = 2;
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		int onTeam[3] = { 0, 0, 0 };
		for (int k = 0; k < numClients; ++k)
		{
			const char* client = ClientSlot(k);
			if (*reinterpret_cast<const int*>(client + svClientState) == 0)
			{
				continue;
			}
			const char* entity = *reinterpret_cast<char* const*>(client + svClientGentity);
			const char* gclient = entity ? *reinterpret_cast<char* const*>(entity + gentityClient) : nullptr;
			if (!gclient)
			{
				continue;
			}
			const int team = *reinterpret_cast<const int*>(gclient + gclientTeam);
			if (team == 1 || team == 2)
			{
				++onTeam[team];
			}
		}

		if (onTeam[1] < onTeam[2])
		{
			lastPick = 1;
		}
		else if (onTeam[2] < onTeam[1])
		{
			lastPick = 2;
		}
		else
		{
			lastPick = lastPick == 1 ? 2 : 1;
		}
		return lastPick;
	}


	static void AdvanceMenuStages()
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			if (!bots[i].spawnStage)
			{
				continue;
			}

			char* client = ClientSlot(i);
			void* entity = *reinterpret_cast<void**>(client + svClientGentity);

			if (!IsFillBot(i) || !entity)
			{
				bots[i].spawnStage = 0;
				continue;
			}

			--bots[i].stageCountdown;
			if (bots[i].stageCountdown > 0)
			{
				continue;
			}

			if (bots[i].spawnStage == stageTeam)
			{
				if (CurrentGametype() == GametypeUnknown && bots[i].stageCountdown > -gametypeWaitFrames)
				{
					--bots[i].stageCountdown;
					continue;
				}

				char* gclient = *reinterpret_cast<char**>(reinterpret_cast<char*>(entity) + gentityClient);
				if (gclient && IsTeamBased())
				{
					const int team = BalancedTeam();
					*reinterpret_cast<int*>(gclient + gclientTeam) = team;
					BotLog("team client %d assigned %d before the menu", i, team);
				}
				else if (gclient)
				{
					BotLog("team client %d left free: %s has no teams", i, GametypeName());
				}
				SendMenuResponse(entity, "autoassign", "team_marinesopfor");
				bots[i].spawnStage = stageClass;
				bots[i].stageCountdown = menuStepFrames;
				continue;
			}

			if (TryWriteLoadout(i, bots[i].loadout))
			{
				SendMenuResponse(entity, "custom1", "changeclass");
			}
			else
			{
				SendMenuResponse(entity, "class0", "changeclass");
			}
			bots[i].spawnStage = 0;
		}
	}


	static bool IsAnyHumanFrozen(int numClients)
	{
		for (int i = 0; i < numClients && i < maxClients; ++i)
		{
			const char* client = ClientSlot(i);
			if (*reinterpret_cast<const int*>(client + svClientState) == 0 || *reinterpret_cast<const int*>(client + svClientIsTest))
			{
				continue;
			}
			const char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * i;
			const char* gclient = *reinterpret_cast<char* const*>(entity + gentityClient);
			if (!gclient || *reinterpret_cast<const int*>(gclient + gclientSessionState) != 0)
			{
				continue;
			}
			if ((*reinterpret_cast<const int*>(gclient + gclientFlags) & gclientFlagFrozen) != 0)
			{
				return true;
			}
		}
		return false;
	}


	static void ForgetRoutes()
	{
		for (int i = 0; i < maxClients; ++i)
		{
			DropPath(bots[i]);
		}
		ClearRouteHeat();
	}


	static void WatchZw3Bot(int clientNum, int now)
	{
		BotState& bot = bots[clientNum];
		const unsigned short remotePort = RemotePortOf(clientNum);
		if (!bot.isZw3Bot || bot.remotePort != remotePort)
		{
			bot = BotState{};
			bot.isZw3Bot = true;
			bot.remotePort = remotePort;
			bot.joinedAtTime = now;
			BotLog("zw3 bot client %d joined, left to zw3", clientNum);
		}

		if (bot.hasSpawnedOnce)
		{
			return;
		}

		const char* entity = *reinterpret_cast<char* const*>(ClientSlot(clientNum) + svClientGentity);
		if (!entity)
		{
			return;
		}

		const char* gclient = *reinterpret_cast<char* const*>(entity + gentityClient);
		if (!gclient)
		{
			return;
		}

		if (*reinterpret_cast<const int*>(entity + gentityHealth) > 0
			&& *reinterpret_cast<const int*>(gclient + gclientSessionState) == 0)
		{
			bot.hasSpawnedOnce = true;
		}
	}


	static void Reconcile()
	{
		if (*reinterpret_cast<int*>(svState) != 2)
		{
			framesSinceSpawn = 0;
			scriptPeak = 0;
			scriptDropAllowedTime = 0;
			scriptBotCap = -1;
			scriptParentPeak = 0;
			scriptParentsUsed = 0;
			scriptParentSampleFrame = 0;
			scriptParentBaseline = -1;
			scriptBaselinePlayers = 0;
			scriptPlayers = 0;
			settledPlayerParents = 0;
			settledPlayers = 0;
			scriptMemoryBlocks = 0;
			burstPlan = -1;
			burstJoined = 0;
			return;
		}

		scriptPeak = std::max(ScriptChildrenUsed(), scriptPeak - scriptPeakDecay);
		scriptMemoryBlocks = static_cast<int>(ClientSlots::ScriptMemoryFreeBlocks());

		++scriptParentSampleFrame;

		const bool isJoining = burstFrames > 0 || joiningBots > 0;

		if (scriptParentSampleFrame >= scriptParentSampleFrames || isJoining)
		{
			scriptParentSampleFrame = 0;
			scriptParentsUsed = static_cast<int>(ClientSlots::ScriptParentsUsed());
			scriptParentPeak = std::max(scriptParentsUsed, scriptParentPeak - scriptParentPeakDecay);
		}

		if ((debugTick % 200) == 0)
		{
			BotLog("script VM: parents %d of %d, fullest child pool %d of %d, memory blocks %d, %d bot(s) joining",
				scriptParentsUsed, static_cast<int>(ClientSlots::ScriptParentCapacity()), ScriptChildrenUsed(), ScriptChildCapacity(),
				scriptMemoryBlocks, joiningBots);
		}

		RefreshTuning();
		if (!settingsTried)
		{
			settingsTried = true;
			LoadSettingsFile(true);
			RefreshTuning();
		}
		if (tuning.settings == 1)
		{
			LoadSettingsFile(true);
			TrySetDvarInt("bots_settings", 0);
			RefreshTuning();
		}
		else if (tuning.settings == 2)
		{
			SaveSettingsFile();
			TrySetDvarInt("bots_settings", 0);
			RefreshTuning();
		}
		AdvanceMenuStages();

		const bool freeze = tuning.freeze != 0;
		debugOn = ReadDvar(debugDvar) != 0;
		++debugTick;
		pathBuiltThisFrame = false;

		void* mapDvar = *reinterpret_cast<void**>(sv_mapname);
		const char* mapName = mapDvar
			? *reinterpret_cast<const char**>(reinterpret_cast<char*>(mapDvar) + dvarCurrent)
			: nullptr;
		if (mapName && std::strcmp(mapName, activeMap) != 0)
		{
			_snprintf_s(activeMap, sizeof(activeMap), _TRUNCATE, "%s", mapName);
			if (botLogFile)
			{
				std::fclose(botLogFile);
				botLogFile = nullptr;
			}
			if (traceLogFile)
			{
				std::fclose(traceLogFile);
				traceLogFile = nullptr;
			}
			hasLogOpenTime = false;

			const bool generated = Navgen::TryLoadFile(mapName);
			const bool loaded = generated || Waypoints::Load(mapName);
			annotateCountdown = 0;
			if (generated && !Navgen::TryLoadAnnotations(mapName))
			{
				annotateCountdown = annotateDelayFrames;
			}
			staleRegenCountdown = 0;
			if (generated && Navgen::IsLoadedStale())
			{
				staleRegenCountdown = staleRegenFrames;
				autoGenReason = "stale file, regenerated on load";
			}
			else if (!generated && ReadDvar(autogenDvar))
			{
				staleRegenCountdown = staleRegenFrames;
				autoGenReason = "bots_autogen: no file for this map";
			}
			burstFrames = burstFrameCount;
			nextOrdinal = 0;
			matchStartTick = debugTick;
			ForgetRoutes();
			ForgetObjectives();
			ResetOpening();
			ForgetTeamKnowledge();
			ForgetGunfire();
			LoadMapMemory(activeMap);
			for (int i = 0; i < maxClients; ++i)
			{
				bots[i].ping = 0;
				bots[i].ordinal = 0;
				bots[i].hasSpawnedOnce = false;
			}
			Print("bots: %s waypoints for %s\n",
				loaded ? (generated ? "generated" : "baked") : "no", activeMap);

			BotLog("map %s (%s waypoints, %d nodes) build %s %s", activeMap,
				loaded ? (generated ? "generated" : "baked") : "no", Waypoints::Count(),
				__DATE__, __TIME__);
			if (staleRegenCountdown > 0)
			{
				BotLog("map %s queues a waypoint build: %s", activeMap, autoGenReason);
			}
		}

		Navgen::ApplyQueuedEdits(activeMap);
		Navgen::RunGenQueue(activeMap);

		const int generateValue = ReadDvar(generateDvar);
		if (generateValue && !lastGenerateValue && *activeMap)
		{
			Navgen::Generate(activeMap, "bots_generatenodes");
			ForgetRoutes();
			ForgetObjectives();
			staleRegenCountdown = 0;
			annotateCountdown = 0;
			BotLog("map %s generated waypoints (bots_generatenodes), %d nodes now in use", activeMap, Waypoints::Count());
		}
		lastGenerateValue = generateValue;

		if (staleRegenCountdown > 0 && (generateValue || ReadDvar(autogenDvar)) && *activeMap
			&& !Navgen::IsGenQueueActive())
		{
			--staleRegenCountdown;
			if (staleRegenCountdown == 0)
			{
				Navgen::Generate(activeMap, autoGenReason);
				ForgetRoutes();
				ForgetObjectives();
				annotateCountdown = 0;
				BotLog("map %s generated waypoints (%s), %d nodes now in use", activeMap, autoGenReason, Waypoints::Count());
			}
		}

		Navgen::RefreshSightBlockers(activeMap);
		Navgen::RefreshWalls(activeMap);

		if (annotateCountdown > 0 && *activeMap && !Navgen::IsGenQueueActive())
		{
			--annotateCountdown;
			if (annotateCountdown == 0)
			{
				Navgen::AnnotateGraph(activeMap);
				BotLog("map %s annotated its waypoints: %d areas", activeMap, Waypoints::AreaCount());
			}
		}

		RefreshObjectives();
		TrackMapMemory();

		int target = ReadDvar(countDvar);
		const bool isGenerating = Navgen::IsGenQueueActive();
		if (isGenerating)
		{
			target = 0;
		}
		const int numClients = *reinterpret_cast<int*>(svs_numClients);

		const auto thinkStart = std::chrono::steady_clock::now();
		int botCount = 0;
		int zw3BotCount = 0;
		int joiningCount = 0;
		int humanCount = 0;
		int reservedCount = 0;
		int lastBot = -1;
		int squatter = -1;
		int leaver = -1;
		int botSlots[maxClients] = {};
		const int now = ServerTimeMs();

		bool isOpeningHeld = false;
		if (debugTick - matchStartTick < openingHoldFrames)
		{
			isOpeningHeld = IsAnyHumanFrozen(numClients);
		}
		if (isOpeningHeld != wasOpeningHeld)
		{
			wasOpeningHeld = isOpeningHeld;
			if (isOpeningHeld)
			{
				BotLog("opening: bots held, a player is still frozen");
			}
			else
			{
				BotLog("opening: bots released, no player is frozen");
			}
		}
		const bool driveFrozen = freeze || isOpeningHeld;
		int tracedCount = 0;
		if (IsOpening(now))
		{
			Waypoints::SetClimbCostScale(openingClimbCostScale);
		}
		else
		{
			Waypoints::SetClimbCostScale(1.0f);
		}

		for (int i = 0; i < numClients; ++i)
		{
			const char* client = ClientSlot(i);
			if (*reinterpret_cast<const int*>(client + svClientState) == 0)
			{
				if (IsSlotClaimedByMember(i))
				{
					++reservedCount;
				}

				bots[i] = BotState{};
				continue;
			}
			if (*reinterpret_cast<const int*>(client + svClientIsTest))
			{
				const bool isFillBot = IsFillBot(i);
				if (isFillBot)
				{
					botSlots[botCount] = i;
					++botCount;
					lastBot = i;
				}
				else
				{
					WatchZw3Bot(i, now);
					++zw3BotCount;
				}

				if (!bots[i].hasSpawnedOnce || now - bots[i].joinedAtTime < scriptJoiningMs)
				{
					++joiningCount;
				}

				if (!isFillBot)
				{
					continue;
				}

				if (leaver < 0 && bots[i].leaveAtTime != 0 && now >= bots[i].leaveAtTime)
				{
					leaver = i;
				}

				if (squatter < 0 && IsSlotClaimedByMember(i))
				{
					squatter = i;
				}

				bots[i].isTraced = debugOn && tuning.trace != 0 && tracedCount < tracedBots;
				if (bots[i].isTraced)
				{
					++tracedCount;
				}
				UpdatePing(i, ClientSlot(i));
				ApplyIdentity(i, ClientSlot(i));
				DriveBot(i, driveFrozen);
			}
			else
			{
				++humanCount;
				ReadHumanScore(i);
			}
		}
		joiningBots = joiningCount;
		scriptPlayers = botCount + zw3BotCount + humanCount;

		if (scriptParentBaseline < 0 && scriptParentsUsed > 0)
		{
			scriptParentBaseline = scriptParentsUsed;
			scriptBaselinePlayers = scriptPlayers;
			BotLog("script VM: %d parents at the map's start with %d player(s)", scriptParentBaseline, scriptBaselinePlayers);
		}

		if (joiningCount == 0 && scriptParentBaseline >= 0 && scriptPlayers > scriptBaselinePlayers)
		{
			const int settled = (scriptParentsUsed - scriptParentBaseline) / (scriptPlayers - scriptBaselinePlayers);

			if (scriptPlayers != settledPlayers)
			{
				BotLog("script VM: %d parents a player, settled with %d player(s)", std::max(settled, 1), scriptPlayers);
			}

			settledPlayerParents = std::max(settled, 1);
			settledPlayers = scriptPlayers;
		}

		const int nextSlot = NextTestClientSlot();
		const bool hasFreeSlot = nextSlot >= 0 && !IsSlotClaimedByMember(nextSlot);

		const int fill = isGenerating ? 0 : ReadDvar(fillDvar);
		const int peopleCount = humanCount + reservedCount + zw3BotCount;
		if (fill > 0 && fill - peopleCount > target)
		{
			target = fill - peopleCount;
		}
		const int seatCap = numClients - tuning.reserve - peopleCount;
		if (target > seatCap)
		{
			if ((debugTick % 200) == 0 && seatCap >= 0)
			{
				BotLog("fill capped at %d bots: %d people and %d seat(s) kept open (bots_reserve)", seatCap, peopleCount, tuning.reserve);
			}
			target = seatCap;
		}
		if (target < 0)
		{
			target = 0;
		}

		{
			const float thinkMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - thinkStart).count();
			perfThinkMsSum += thinkMs;
			if (thinkMs > perfThinkMsMax)
			{
				perfThinkMsMax = thinkMs;
			}
			perfTraces += sightLineCalls;
			perfPaths += pathFinds;
			sightLineCalls = 0;
			pathFinds = 0;
			++perfFrames;
			if (perfFrames >= perfLogFrames)
			{
				BotLog("perf %d bots think avg %.2f ms max %.2f ms a frame, %.1f traces and %.2f routes a frame, sprinting %d%% of the moving frames", botCount,
					perfThinkMsSum / static_cast<float>(perfFrames), perfThinkMsMax,
					static_cast<float>(perfTraces) / static_cast<float>(perfFrames),
					static_cast<float>(perfPaths) / static_cast<float>(perfFrames),
					perfMovingFrames > 0 ? perfSprintFrames * 100 / perfMovingFrames : 0);
				perfThinkMsSum = 0.0f;
				perfThinkMsMax = 0.0f;
				perfFrames = 0;
				perfTraces = 0;
				perfPaths = 0;
				perfMovingFrames = 0;
				perfSprintFrames = 0;
			}
		}

		if (squatter >= 0)
		{
			BotLog("evict client %d, slot claimed by lobby member xuid %llx", squatter,
				Session_GetXuidEvenIfInactive ? Session_GetXuidEvenIfInactive(reinterpret_cast<void*>(lobbySession), squatter) : 0ULL);
			DropBot(squatter);
			return;
		}

		const int scriptRoom = ScriptChildCapacity() - ScriptChildrenUsed();
		const int parentCapacity = static_cast<int>(ClientSlots::ScriptParentCapacity());
		const int parentRoom = parentCapacity - scriptParentsUsed;
		const bool isParentShort = parentCapacity > 0 && parentRoom < scriptParentDropRoom;
		const bool isMemoryShort = scriptMemoryBlocks < scriptMemoryDropBlocks;
		if ((scriptRoom < scriptDropRoom || isParentShort || isMemoryShort) && now >= scriptDropAllowedTime && botCount > 0)
		{
			scriptDropAllowedTime = now + scriptDropGapMs;

			const int dropped = botSlots[NextRand() % static_cast<unsigned int>(botCount)];
			scriptBotCap = botCount - 1;

			BotLog("script VM: %d child and %d parent variables and %d memory blocks left, dropping client %d, the fill holds at %d",
				scriptRoom, parentRoom, scriptMemoryBlocks, dropped, scriptBotCap);
			DropBot(dropped);
			return;
		}

		if (botCount > target)
		{
			if (lastBot >= 0)
			{
				DropBot(botSlots[NextRand() % static_cast<unsigned int>(botCount)]);
			}
			return;
		}

		if (leaver >= 0 && !isGenerating)
		{
			BotLog("leave client %d goes on its own", leaver);
			bots[leaver].leaveAtTime = 0;
			DropBot(leaver);
			return;
		}

		if (botCount >= target)
		{
			return;
		}

		if (scriptBotCap >= 0 && botCount >= scriptBotCap)
		{
			return;
		}

		if (reservedCount > 0 && debugTick - matchStartTick < settleFrames)
		{
			if ((debugTick % 100) == 0)
			{
				BotLog("fill waits: %d lobby member(s) still connecting, %d s into the map", reservedCount,
					(debugTick - matchStartTick) / 20);
			}
			return;
		}

		if (!hasFreeSlot && nextSlot >= 0 && (debugTick % 100) == 0)
		{
			BotLog("fill waits: slot %d is claimed by a lobby member who has not connected yet", nextSlot);
		}

		if (burstFrames > 0)
		{
			--burstFrames;
		}

		++framesSinceSpawn;
		if (framesSinceSpawn < (burstFrames > 0 ? 1 : joinGapFrames))
		{
			return;
		}
		framesSinceSpawn = 0;
		joinGapFrames = IrandMs(spawnFrameGap / 2, spawnFrameGap * 2);

		if (!hasFreeSlot)
		{
			return;
		}

		if (burstFrames > 0 && burstPlan < 0)
		{
			burstPlan = PlanBurst();
			burstJoined = 0;
		}

		const bool isMemoryLeft = scriptMemoryBlocks >= burstMemoryBlockMargin;
		const bool isParentLeft = HasParentRoom(joiningCount + 1);
		const bool isBursting = burstFrames > 0 && burstJoined < burstPlan && isMemoryLeft && isParentLeft;

		if (!isBursting && !HasScriptRoom(joiningCount + 1))
		{
			if ((debugTick % 200) == 0)
			{
				BotLog("fill waits: the script VM has no room for another player, peak %d of %d child and %d of %d parent variables, %d bot(s) joining",
					scriptPeak, ScriptChildCapacity(), scriptParentPeak, static_cast<int>(ClientSlots::ScriptParentCapacity()), joiningCount);
			}
			return;
		}

		const int ordinal = nextOrdinal++;

		const char* taken[maxClients] = {};
		int takenCount = 0;
		for (int i = 0; i < numClients && takenCount < maxClients; ++i)
		{
			const char* client = ClientSlot(i);
			if (*reinterpret_cast<const int*>(client + svClientState) != 0
				&& *reinterpret_cast<const int*>(client + svClientIsTest)
				&& bots[i].identity.name)
			{
				taken[takenCount++] = bots[i].identity.name;
			}
		}
		const Identity identity = TakeLobbyIdentity(taken, takenCount);
		pendingName = identity.name;
		void* entity = SV_AddTestClient();
		pendingName = nullptr;
		if (!entity)
		{
			return;
		}

		if (isBursting)
		{
			++burstJoined;
		}

		const int clientNum =
			(reinterpret_cast<char*>(entity) - reinterpret_cast<char*>(g_entities)) / gentityStride;
		if (clientNum >= 0 && clientNum < numClients)
		{
			bots[clientNum] = BotState{};
			bots[clientNum].isFillBot = true;
			bots[clientNum].remotePort = RemotePortOf(clientNum);
			bots[clientNum].joinedAtTime = ServerTimeMs();
			bots[clientNum].spawnStage = stageTeam;
			bots[clientNum].stageCountdown = menuStepFrames;
			bots[clientNum].ordinal = ordinal;
			bots[clientNum].identity = identity;

			if (clientNum != nextSlot)
			{
				BotLog("fill wanted slot %d for the new bot, the engine seated it in %d", nextSlot, clientNum);
			}
			RollSkill(clientNum);
			RollPersonality(clientNum);
			if (RollPercent(tuning.churn))
			{
				bots[clientNum].leaveAtTime = ServerTimeMs() + IrandMs(leaveMinMs, leaveMaxMs);
			}

			BotLog("join client %d ordinal %d name %s skill %d %s%s", clientNum, ordinal,
				identity.name, bots[clientNum].skillIndex + 1,
				ArchetypeName(bots[clientNum].personality.archetype),
				bots[clientNum].personality.isQuickscoper ? " quickscoper" : "");
		}
	}


	static void G_RunFrame_Hk(int levelTime)
	{
		Reconcile();

		reinterpret_cast<G_RunFrame_t>(Utils::Hook::Rebase(G_RunFrame))(levelTime);
	}


	int DesiredBotCount()
	{
		return ReadDvar(countDvar);
	}


	int DesiredFillCount()
	{
		return ReadDvar(fillDvar);
	}


	int PlannedBotCount(int peopleCount, int slotCount)
	{
		int planned = ReadDvar(countDvar);
		const int fill = ReadDvar(fillDvar);
		if (fill > 0 && fill - peopleCount > planned)
		{
			planned = fill - peopleCount;
		}
		const int seatCap = slotCount - tuning.reserve - peopleCount;
		if (planned > seatCap)
		{
			planned = seatCap;
		}
		if (planned < 0)
		{
			planned = 0;
		}
		return planned;
	}


	const char* PendingBotName()
	{
		return pendingName;
	}


	static void RegisterDvars()
	{
		countDvar = Dvar_RegisterInt(
			"bots", 0, 0, maxClients - 1, 0,
			"test clients to keep in the match while hosting; 0 off, N keeps N bots joined, topped up as they leave");
		fillDvar = Dvar_RegisterInt(
			"sv_bots_fill", 0, 0, maxClients, 0,
			"players to keep in the match in total, bots making up the difference; 0 off, and `bots` wins where it asks for more");
		debugDvar = Dvar_RegisterInt(
			"bots_debug", 0, 0, 1, 0,
			"0 off; 1 prints a breadcrumb through the bot AI pass, to find where a hang is");
		generateDvar = Dvar_RegisterInt(
			"bots_generatenodes", 0, 0, 1, 0,
			"0 idle; 1 builds and saves a waypoint graph for the running map, then set it back to 0 to re-arm");
		autogenDvar = Dvar_RegisterInt(
			"bots_autogen", 0, 0, 1, 0,
			"0 off; 1 generates the waypoint graph on map load when there is no file or a stale one (archived, survives restarts)");
		RegisterTuning();
		RefreshTuning();

		char settingsPath[MAX_PATH * 2];
		if (BotsPath("botsettings.txt", settingsPath, sizeof(settingsPath)))
		{
			settingsTried = true;
			LoadSettingsFile(true);
			RefreshTuning();
		}

		Navgen::RegisterCommands();
		Navgen::RegisterOverlay();
	}


	static void LoadSettingsAtStartup()
	{
		char settingsPath[MAX_PATH * 2];
		if (settingsTried || !BotsPath("botsettings.txt", settingsPath, sizeof(settingsPath)))
		{
			return;
		}
		settingsTried = true;
		LoadSettingsFile(true);
		RefreshTuning();
	}


	bool InstallControl()
	{
		if (!Utils::Hook::BranchesTo(G_RunFrameJump, G_RunFrame, HOOK_JUMP)
			|| !Utils::Hook::BranchesTo(G_RunFrameCall, G_RunFrame, HOOK_CALL))
		{
			return false;
		}

		if (!InstallUnlockHook())
		{
			return false;
		}

		static Utils::Hook runFrameJump;
		static Utils::Hook runFrameCall;
		const bool isSeated = runFrameJump.Initialize(G_RunFrameJump, G_RunFrame_Hk, HOOK_JUMP)->Install()->IsInstalled()
			&& runFrameCall.Initialize(G_RunFrameCall, G_RunFrame_Hk, HOOK_CALL)->Install()->IsInstalled();
		if (!isSeated)
		{
			runFrameJump.Uninstall();
			runFrameCall.Uninstall();
			UninstallUnlockHook();
			return false;
		}

		runFrameJump.Quick();
		runFrameCall.Quick();

		Events::OnDvarInit(RegisterDvars);
		Events::OnSVInit(LoadSettingsAtStartup);
		Events::OnSVInit(LoadBotNames);
		return true;
	}
}
