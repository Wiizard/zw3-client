#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include "Components/Modules/Scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Components::BotAI::Navgen
{
	using Cmd_AddCommand_t = void (__cdecl*)(const char* name, void (__cdecl*)(), void* storage);

	static constexpr int commandCount = 11;

	struct CommandSlot
	{
		void* next;
		const char* name;
		void* func;
	};

	static CommandSlot commandStorage[commandCount] = {};

	enum PendingEdit
	{
		PendingNone = 0,
		PendingDelNode,
		PendingKillLink,
		PendingAddLink,
		PendingAddNode,
		PendingProbe,
		PendingGlass,
		PendingSetNode,
		PendingMoveNode,
		PendingVerify,
	};

	static int pendingEdit = PendingNone;
	static int pendingA = 0;
	static int pendingB = 0;
	static float pendingPoint[3] = { 0.0f, 0.0f, 0.0f };
	static int pendingProbeMode = 0;
	static int pendingKind = Waypoints::LinkWalk;
	static bool pendingForce = false;
	static unsigned char pendingType = Waypoints::NodeStand;

	static bool ParseLinkKind(const char* text, int* outKind)
	{
		for (int kind = 0; kind < 5; ++kind)
		{
			if (std::strcmp(text, Waypoints::KindName(kind)) == 0)
			{
				*outKind = kind;
				return true;
			}
		}
		return false;
	}

	static unsigned int EntryBitsFor(int kind)
	{
		if (kind == Waypoints::LinkMantle)
		{
			return Waypoints::linkMantle;
		}
		if (kind == Waypoints::LinkDrop)
		{
			return Waypoints::linkDrop;
		}
		if (kind == Waypoints::LinkLadder)
		{
			return Waypoints::linkLadder;
		}
		if (kind == Waypoints::LinkJump)
		{
			return Waypoints::linkJump;
		}
		return 0;
	}

	static const char* Verdict(bool passed)
	{
		if (passed)
		{
			return "ok";
		}
		return "FAILED";
	}

	static void DescribeLink(int from, int to, char* out, int outSize)
	{
		if (!Waypoints::HasChild(from, to))
		{
			_snprintf_s(out, outSize, _TRUNCATE, "none");
			return;
		}
		const unsigned int flags = Waypoints::FlagsOf(from, to);
		const char* crouch = "";
		if (flags & Waypoints::linkCrouch)
		{
			crouch = " crouch";
		}
		const char* glass = "";
		if (flags & Waypoints::linkGlass)
		{
			glass = " glass";
		}
		_snprintf_s(out, outSize, _TRUNCATE, "%s%s%s", Waypoints::KindName(Waypoints::KindOf(from, to)), crouch, glass);
	}

	static int HostClient()
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int i = 0; i < numClients; ++i)
		{
			const char* client = reinterpret_cast<char*>(svs_clients) + svClientStride * i;
			if (*reinterpret_cast<const int*>(client + svClientState) != 0
				&& *reinterpret_cast<const int*>(client + svClientIsTest) == 0)
			{
				return i;
			}
		}
		return 0;
	}

	static const float* HostOrigin()
	{
		const char* playerState =
			SV_GetPlayerstateForClientNum(HostClient());
		return reinterpret_cast<const float*>(playerState + psOrigin);
	}


	static int EditArgCount()
	{
		const int* args = reinterpret_cast<const int*>(cmd_args_ptr);
		return args[args[0] + cmdArgcIndex];
	}

	static const char* const* EditArgs()
	{
		const int* args = reinterpret_cast<const int*>(cmd_args_ptr);
		const auto* argvTable =
			reinterpret_cast<const char* const* const*>(cmd_args_ptr + cmdArgvOffset);
		return argvTable[args[0]];
	}

	static void DelNode_f()
	{
		if (EditArgCount() < 2)
		{
			Report("usage: bots_delnode <node>\n");
			return;
		}
		pendingA = std::atoi(EditArgs()[1]);
		pendingEdit = PendingDelNode;
	}

	static void KillLink_f()
	{
		if (EditArgCount() < 3)
		{
			Report("usage: bots_killlink <a> <b>\n");
			return;
		}
		pendingA = std::atoi(EditArgs()[1]);
		pendingB = std::atoi(EditArgs()[2]);
		pendingEdit = PendingKillLink;
	}

	static void AddLink_f()
	{
		const int argc = EditArgCount();
		const char* const* args = EditArgs();
		if (argc < 3)
		{
			Report("usage: bots_addlink <a> <b> [walk|mantle|drop|ladder|jump] [force]\n");
			return;
		}
		pendingA = std::atoi(args[1]);
		pendingB = std::atoi(args[2]);
		pendingKind = Waypoints::LinkWalk;
		pendingForce = false;
		for (int i = 3; i < argc; ++i)
		{
			if (std::strcmp(args[i], "force") == 0)
			{
				pendingForce = true;
			}
			else if (!ParseLinkKind(args[i], &pendingKind))
			{
				Report("navgen: unknown link kind %s (walk, mantle, drop, ladder or jump)\n", args[i]);
				return;
			}
		}
		pendingEdit = PendingAddLink;
	}

	static void AddNode_f()
	{
		pendingEdit = PendingAddNode;
	}

	static void Probe_f()
	{
		const int argc = EditArgCount();
		const char* const* args = EditArgs();
		pendingProbeMode = 0;
		pendingA = static_cast<int>(gridStep);
		if (argc >= 5)
		{
			pendingProbeMode = 2;
			pendingA = std::atoi(args[1]);
			pendingPoint[0] = static_cast<float>(std::atof(args[2]));
			pendingPoint[1] = static_cast<float>(std::atof(args[3]));
			pendingPoint[2] = static_cast<float>(std::atof(args[4]));
			if (argc >= 6 && std::strcmp(args[5], "march") == 0)
			{
				pendingProbeMode = 3;
			}
		}
		else if (argc == 3)
		{
			pendingProbeMode = 1;
			pendingA = std::atoi(args[1]);
			pendingB = std::atoi(args[2]);
		}
		else if (argc == 2)
		{
			pendingA = std::atoi(args[1]);
			if (pendingA < 8)
			{
				pendingA = 8;
			}
		}
		pendingEdit = PendingProbe;
	}

	static void Glass_f()
	{
		pendingEdit = PendingGlass;
	}

	static void SetNode_f()
	{
		if (EditArgCount() < 3)
		{
			Report("usage: bots_setnode <node> stand|crouch\n");
			return;
		}
		const char* stance = EditArgs()[2];
		if (std::strcmp(stance, "stand") == 0)
		{
			pendingType = Waypoints::NodeStand;
		}
		else if (std::strcmp(stance, "crouch") == 0)
		{
			pendingType = Waypoints::NodeCrouch;
		}
		else
		{
			Report("navgen: unknown stance %s (stand or crouch)\n", stance);
			return;
		}
		pendingA = std::atoi(EditArgs()[1]);
		pendingEdit = PendingSetNode;
	}

	static void MoveNode_f()
	{
		if (EditArgCount() < 2)
		{
			Report("usage: bots_movenode <node> (moves it to your feet)\n");
			return;
		}
		pendingA = std::atoi(EditArgs()[1]);
		pendingEdit = PendingMoveNode;
	}

	static void Verify_f()
	{
		pendingEdit = PendingVerify;
	}


	static constexpr int queueMaxMaps = 32;
	static constexpr int queueSettleFrames = 40;
	static char queueMaps[queueMaxMaps][32];
	static int queueCount = 0;
	static int queueIndex = 0;
	static int queueSettle = 0;
	static bool isQueueActive = false;

	static const char* const stockMaps[27] = {
		"mp_afghan", "mp_derail", "mp_estate", "mp_favela", "mp_highrise", "mp_invasion",
		"mp_checkpoint", "mp_quarry", "mp_rundown", "mp_rust", "mp_boneyard", "mp_subbase",
		"mp_terminal", "mp_underpass", "mp_brecourt", "mp_nightshift", "mp_abandon", "mp_bloc",
		"mp_compact", "mp_complex", "mp_crash", "mp_fuel2", "mp_overgrown", "mp_storm",
		"mp_strike", "mp_trailerpark", "mp_vacant",
	};

	static void StartGenQueue()
	{
		queueIndex = 0;
		queueSettle = 0;
		isQueueActive = queueCount > 0;
		if (!isQueueActive)
		{
			return;
		}
		Report("navgen: gen queue: %d map(s), starting with %s; bots held at 0 until it is done\n",
			   queueCount, queueMaps[0]);
		char command[64];
		_snprintf_s(command, sizeof(command), _TRUNCATE, "map %s\n", queueMaps[0]);
		Cbuf_AddText(0, command);
	}

	static void GenQueue_f()
	{
		const int argc = EditArgCount();
		const char* const* args = EditArgs();
		if (argc < 2)
		{
			Report("usage: bots_genqueue <map> [map...] | stop\n");
			return;
		}
		if (std::strcmp(args[1], "stop") == 0)
		{
			isQueueActive = false;
			Report("navgen: gen queue stopped at %d of %d\n", queueIndex, queueCount);
			return;
		}
		queueCount = 0;
		for (int i = 1; i < argc && queueCount < queueMaxMaps; ++i)
		{
			std::strncpy(queueMaps[queueCount], args[i], sizeof(queueMaps[queueCount]) - 1);
			queueMaps[queueCount][sizeof(queueMaps[queueCount]) - 1] = 0;
			++queueCount;
		}
		StartGenQueue();
	}

	static void GenAll_f()
	{
		queueCount = 0;
		for (const char* map : stockMaps)
		{
			std::strncpy(queueMaps[queueCount], map, sizeof(queueMaps[queueCount]) - 1);
			queueMaps[queueCount][sizeof(queueMaps[queueCount]) - 1] = 0;
			++queueCount;
		}
		StartGenQueue();
	}

	bool IsGenQueueActive()
	{
		return isQueueActive;
	}

	void RunGenQueue(const char* mapName)
	{
		if (!isQueueActive || !mapName || !*mapName)
		{
			return;
		}
		if (std::strcmp(mapName, queueMaps[queueIndex]) != 0)
		{
			queueSettle = 0;
			return;
		}
		if (++queueSettle < queueSettleFrames)
		{
			return;
		}
		queueSettle = -1000000;

		Generate(mapName, "gen queue");
		++queueIndex;
		if (queueIndex >= queueCount)
		{
			isQueueActive = false;
			Report("navgen: gen queue done: %d map(s); players/navgen_summary.log has one line each\n", queueCount);
			return;
		}
		Report("navgen: gen queue: %d of %d done, loading %s\n", queueIndex, queueCount, queueMaps[queueIndex]);
		char command[64];
		_snprintf_s(command, sizeof(command), _TRUNCATE, "map %s\n", queueMaps[queueIndex]);
		Cbuf_AddText(0, command);
	}


	static const char* GlassStateName(int damage)
	{
		if (damage == glassDamageDeleted)
		{
			return "deleted";
		}
		if (damage >= glassDamageDestroy)
		{
			return "destroyed";
		}
		return damage > 0 ? "weakened" : "intact";
	}


	static void ListGlass()
	{
		const float* origin = HostOrigin();

		const int count = GlassPieceCount();
		Report("navgen: %d glass piece(s) in the map\n", count);
		for (int i = 0; i < count; ++i)
		{
			float centre[3] = { 0.0f, 0.0f, 0.0f };
			GlassPieceOrigin(i, centre);
			const float dx = centre[0] - origin[0];
			const float dy = centre[1] - origin[1];
			Report("navgen:   glass %d at %.0f %.0f %.0f %s, %.0f from you\n", i,
				   centre[0], centre[1], centre[2], GlassStateName(GlassPieceDamage(i)),
				   std::sqrt(dx * dx + dy * dy));
		}
	}

	void RegisterCommands()
	{
		const auto add = Cmd_AddCommandInternal;
		add("bots_delnode", DelNode_f, &commandStorage[0]);
		add("bots_killlink", KillLink_f, &commandStorage[1]);
		add("bots_addlink", AddLink_f, &commandStorage[2]);
		add("bots_addnode", AddNode_f, &commandStorage[3]);
		add("bots_probe", Probe_f, &commandStorage[4]);
		add("bots_glass", Glass_f, &commandStorage[5]);
		add("bots_setnode", SetNode_f, &commandStorage[6]);
		add("bots_movenode", MoveNode_f, &commandStorage[7]);
		add("bots_verify", Verify_f, &commandStorage[8]);
		add("bots_genqueue", GenQueue_f, &commandStorage[9]);
		add("bots_genall", GenAll_f, &commandStorage[10]);
	}


	static constexpr float landingSlackXy = 40.0f;
	static constexpr float landingSlackZ = 36.0f;

	static bool PassesKindTest(int kind, const float* from, const float* to, unsigned int* outFlags)
	{
		*outFlags = 0;
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float run = std::sqrt(dx * dx + dy * dy);
		float dirX = 1.0f;
		float dirY = 0.0f;
		if (run > 1.0f)
		{
			dirX = dx / run;
			dirY = dy / run;
		}

		if (kind == Waypoints::LinkWalk)
		{
			if (CanWalk(from, to))
			{
				return true;
			}
			if (CanWalkCrouched(from, to))
			{
				*outFlags = Waypoints::linkCrouch;
				return true;
			}
			return false;
		}

		if (kind == Waypoints::LinkMantle)
		{
			const MantleGeometry geometry = ProbeMantle(from, dirX, dirY);
			if (!geometry.isValid)
			{
				return false;
			}
			const float missXy = Distance2(geometry.landing, to);
			const float missZ = geometry.landing[2] - to[2];
			const bool lands = missXy <= landingSlackXy && std::fabs(missZ) <= landingSlackZ;
			if (probeVerbose)
			{
				const char* closeness = "too far";
				if (lands)
				{
					closeness = "close enough";
				}
				Report("navgen:     mantle: landing is %.0f xy / %+.0f z from the target, %s\n", missXy, missZ, closeness);
			}
			if (lands)
			{
				*outFlags = Waypoints::linkMantle;
			}
			return lands;
		}

		if (kind == Waypoints::LinkDrop)
		{
			if (!CanDrop(from, to))
			{
				return false;
			}
			*outFlags = Waypoints::linkDrop;
			return true;
		}

		if (kind == Waypoints::LinkJump)
		{
			if (!CanHop(from, to))
			{
				return false;
			}
			*outFlags = Waypoints::linkJump;
			return true;
		}

		if (kind == Waypoints::LinkLadder)
		{
			LadderHit ladder = {};
			if (!FindLadder(from, dirX, dirY, &ladder))
			{
				return false;
			}
			const float missXy = Distance2(ladder.top, to);
			const float missZ = ladder.top[2] - to[2];
			const bool lands = missXy <= landingSlackXy && std::fabs(missZ) <= landingSlackZ;
			if (probeVerbose)
			{
				const char* closeness = "too far";
				if (lands)
				{
					closeness = "close enough";
				}
				Report("navgen:     ladder: top is %.0f xy / %+.0f z from the target, %s\n", missXy, missZ, closeness);
			}
			if (lands)
			{
				*outFlags = Waypoints::linkLadder;
			}
			return lands;
		}
		return false;
	}

	static bool ProbeTest(const char* label, int kind, const float* from, const float* to)
	{
		Report("navgen:   %s:\n", label);
		unsigned int flags = 0;
		const bool passed = PassesKindTest(kind, from, to, &flags);
		const char* attribute = "";
		if (flags & Waypoints::linkCrouch)
		{
			attribute = " (crouched)";
		}
		Report("navgen:   %s: %s%s\n", label, Verdict(passed), attribute);
		return passed;
	}

	static void ProbePair(const float* from, const float* to, int nodeA, int nodeB)
	{
		char labelA[24] = "you";
		char labelB[24] = "the point";
		if (nodeA >= 0)
		{
			_snprintf_s(labelA, sizeof(labelA), _TRUNCATE, "node %d", nodeA);
		}
		if (nodeB >= 0)
		{
			_snprintf_s(labelB, sizeof(labelB), _TRUNCATE, "node %d", nodeB);
		}

		probeVerbose = true;
		Report("navgen: probe %s (%.0f %.0f %.0f) -> %s (%.0f %.0f %.0f): %.0f xy, %+.0f z\n",
			   labelA, from[0], from[1], from[2], labelB, to[0], to[1], to[2],
			   Distance2(from, to), to[2] - from[2]);
		if (nodeA >= 0 && nodeB >= 0)
		{
			char there[32];
			char back[32];
			DescribeLink(nodeA, nodeB, there, sizeof(there));
			DescribeLink(nodeB, nodeA, back, sizeof(back));
			Report("navgen:   live graph: %d -> %d %s, %d -> %d %s\n", nodeA, nodeB, there, nodeB, nodeA, back);
		}

		float rayZ = 0.0f;
		float rayNormalZ = 0.0f;
		if (GroundRay(to[0], to[1], to[2] + snapReachUp, snapReachUp + 80.0f, &rayZ, &rayNormalZ))
		{
			Report("navgen:   ground ray at the target: floor %.1f (%+.1f from its z), normal %.2f\n",
				   rayZ, rayZ - to[2], rayNormalZ);
		}
		else
		{
			Report("navgen:   ground ray at the target: no floor from %.0f above to 80 below\n", snapReachUp);
		}

		const GroundHit ground = SnapToGround(to[0], to[1], to[2], 80.0f);
		const char* crouch = "";
		if (ground.needsCrouch)
		{
			crouch = " (crouch)";
		}
		Report("navgen:   snap at the target: %s%s, floor %.1f (%+.1f), normal %.2f\n",
			   Verdict(ground.isValid), crouch, ground.z, ground.z - to[2], ground.normalZ);
		Report("navgen:   footing: %s (strict: %s)\n", Verdict(HasPathFooting(to[0], to[1], to[2])),
			   Verdict(HasSolidFooting(to[0], to[1], to[2])));
		Report("navgen:   shoulder room:\n");
		Report("navgen:   shoulder room: %s\n", Verdict(HasShoulderRoom(to[0], to[1], to[2], ground.needsCrouch)));

		ProbeTest("walk there", Waypoints::LinkWalk, from, to);
		ProbeTest("walk back", Waypoints::LinkWalk, to, from);
		ProbeTest("drop there", Waypoints::LinkDrop, from, to);
		ProbeTest("drop back", Waypoints::LinkDrop, to, from);
		ProbeTest("mantle there", Waypoints::LinkMantle, from, to);
		ProbeTest("mantle back", Waypoints::LinkMantle, to, from);
		ProbeTest("hop there", Waypoints::LinkJump, from, to);
		ProbeTest("hop back", Waypoints::LinkJump, to, from);

		Report("navgen:   slide there:\n");
		Report("navgen:   slide there: %s\n", Verdict(CanSlideDown(from, to)));
		Report("navgen:   slide back:\n");
		Report("navgen:   slide back: %s\n", Verdict(CanSlideDown(to, from)));

		ProbeTest("ladder there", Waypoints::LinkLadder, from, to);
		ProbeTest("ladder back", Waypoints::LinkLadder, to, from);
		probeVerbose = false;
	}

	static bool IsLiveNode(int node)
	{
		return Waypoints::IsLoaded() && node >= 0 && node < Waypoints::Count();
	}

	static void RunProbe()
	{
		if (pendingProbeMode == 0)
		{
			const char* playerState =
				SV_GetPlayerstateForClientNum(HostClient());
			const float* origin = reinterpret_cast<const float*>(playerState + psOrigin);
			const float yaw = reinterpret_cast<const float*>(playerState + psViewAngles)[1];
			const float rad = yaw / 57.295776f;
			const float distance = static_cast<float>(pendingA);

			const float from[3] = { origin[0], origin[1], origin[2] };
			const float cx = from[0] + std::cos(rad) * distance;
			const float cy = from[1] + std::sin(rad) * distance;
			Report("navgen: probe %.0f ahead at yaw %.0f\n", distance, yaw);

			GroundHit ground = SnapToGround(cx, cy, from[2], 80.0f);
			if (!ground.isValid)
			{
				ground = SnapToGround(cx, cy, from[2] - 100.0f, 220.0f);
				if (!ground.isValid)
				{
					Report("navgen:   snap FAILED at %.0f %.0f: no floor from 80 below you to 320 below\n", cx, cy);
					return;
				}
				Report("navgen:   deep snap (drops): floor %.1f (%+.1f)\n", ground.z, ground.z - from[2]);
			}

			const float to[3] = { cx, cy, ground.z };
			ProbePair(from, to, -1, -1);
			return;
		}

		if (!IsLiveNode(pendingA))
		{
			Report("navgen: probe: no node %d in the active graph\n", pendingA);
			return;
		}
		const float* from = Waypoints::Origin(pendingA);
		if (pendingProbeMode == 1)
		{
			if (!IsLiveNode(pendingB))
			{
				Report("navgen: probe: no node %d in the active graph\n", pendingB);
				return;
			}
			ProbePair(from, Waypoints::Origin(pendingB), pendingA, pendingB);
			return;
		}
		if (pendingProbeMode == 3)
		{
			char line[320] = "";
			Report("navgen: march from node %d (%.0f %.0f %.0f) toward %.0f %.0f %.0f, a lattice step at a time:\n",
				   pendingA, from[0], from[1], from[2], pendingPoint[0], pendingPoint[1], pendingPoint[2]);
			MarchFromNode(from, pendingA, pendingPoint[0], pendingPoint[1], pendingPoint[2], line, sizeof(line));
			Report("navgen:   march: %s\n", line);
			return;
		}
		ProbePair(from, pendingPoint, pendingA, -1);
	}


	static void ApplyAddLink(const char* mapName)
	{
		const int a = pendingA;
		const int b = pendingB;
		const int kind = pendingKind;
		if (!IsLiveNode(a) || !IsLiveNode(b) || a == b)
		{
			Report("navgen: addlink: bad node id (%d, %d)\n", a, b);
			return;
		}

		const float* from = Waypoints::Origin(a);
		const float* to = Waypoints::Origin(b);
		const char* forced = "";
		if (pendingForce)
		{
			forced = " (forced)";
		}
		Report("navgen: addlink %d -> %d as %s%s\n", a, b, Waypoints::KindName(kind), forced);

		probeVerbose = true;
		unsigned int flagsAToB = Waypoints::linkIndexMask;
		unsigned int flagsBToA = Waypoints::linkIndexMask;
		unsigned int earned = 0;
		const bool passesThere = PassesKindTest(kind, from, to, &earned);
		Report("navgen:   %s there: %s\n", Waypoints::KindName(kind), Verdict(passesThere));
		if (passesThere)
		{
			flagsAToB = earned;
		}
		else if (pendingForce)
		{
			flagsAToB = EntryBitsFor(kind);
		}

		if (kind == Waypoints::LinkWalk)
		{
			unsigned int earnedBack = 0;
			const bool passesBack = PassesKindTest(kind, to, from, &earnedBack);
			Report("navgen:   walk back: %s\n", Verdict(passesBack));
			if (passesBack)
			{
				flagsBToA = earnedBack;
			}
			else if (pendingForce)
			{
				flagsBToA = 0;
			}
		}
		else if (kind == Waypoints::LinkLadder)
		{
			flagsBToA = flagsAToB;
		}
		probeVerbose = false;

		if (flagsAToB == Waypoints::linkIndexMask && flagsBToA == Waypoints::linkIndexMask)
		{
			Report("navgen: addlink refused: the %s test failed; add force to write it anyway\n", Waypoints::KindName(kind));
			return;
		}
		if (!Waypoints::EditAddLinkKind(a, b, flagsAToB, flagsBToA))
		{
			Report("navgen: addlink failed (a node is at the link cap of %d)\n", Waypoints::maxFileLinks);
			return;
		}

		SaveActiveGraph(mapName);
		const char* ways = "one way";
		if (flagsAToB != Waypoints::linkIndexMask && flagsBToA != Waypoints::linkIndexMask)
		{
			ways = "both ways";
		}
		Report("navgen: link %d -> %d written %s and saved\n", a, b, ways);
	}


	static constexpr int verifyConsoleCap = 40;

	static void VerifyGraph()
	{
		if (!Waypoints::IsLoaded())
		{
			Report("navgen: verify: no graph loaded\n");
			return;
		}

		const int tracesBefore = traceCount;
		int checked[5] = {};
		int failed[5] = {};
		int failures = 0;
		const int count = Waypoints::Count();
		for (int i = 0; i < count; ++i)
		{
			for (int c = 0; c < Waypoints::ChildCount(i); ++c)
			{
				const int child = Waypoints::ChildAt(i, c);
				const int kind = Waypoints::KindOf(i, child);
				const float* from = Waypoints::Origin(i);
				const float* to = Waypoints::Origin(child);
				++checked[kind];

				unsigned int flags = 0;
				if (PassesKindTest(kind, from, to, &flags))
				{
					continue;
				}
				++failed[kind];
				ReportCapped(failures, verifyConsoleCap,
							 "navgen:   %s %d -> %d FAILED: %.0f %.0f %.0f -> %.0f %.0f %.0f\n",
							 Waypoints::KindName(kind), i, child, from[0], from[1], from[2], to[0], to[1], to[2]);
				++failures;
			}
		}

		Report("navgen: verify: %d link(s) checked, %d failed: walk %d of %d, mantle %d of %d, drop %d of %d, "
			   "ladder %d of %d, jump %d of %d; %d traces\n",
			   checked[0] + checked[1] + checked[2] + checked[3] + checked[4], failures,
			   failed[0], checked[0], failed[1], checked[1], failed[2], checked[2],
			   failed[3], checked[3], failed[4], checked[4], traceCount - tracesBefore);
		const char* stale = "";
		if (IsLoadedStale())
		{
			stale = " (stale)";
		}
		Report("navgen: file gen rev %d, generator %d%s\n", LoadedRevision(), generatorRevision, stale);
	}


	void ApplyQueuedEdits(const char* mapName)
	{
		if (!pendingEdit || !mapName || !*mapName)
		{
			return;
		}
		const int editType = pendingEdit;
		pendingEdit = PendingNone;

		if (editType == PendingProbe || editType == PendingGlass
			|| editType == PendingVerify || editType == PendingAddLink)
		{
			ReportBegin(mapName, true);
			SuppressGlass(editType != PendingGlass);
			if (editType == PendingProbe)
			{
				RunProbe();
			}
			else if (editType == PendingGlass)
			{
				ListGlass();
			}
			else if (editType == PendingVerify)
			{
				VerifyGraph();
			}
			else
			{
				ApplyAddLink(mapName);
			}
			SuppressGlass(false);
			ReportEnd();
			return;
		}

		bool applied = false;
		switch (editType)
		{
		case PendingDelNode:
			applied = Waypoints::EditDeleteNode(pendingA);
			break;
		case PendingKillLink:
			applied = Waypoints::EditRemoveLink(pendingA, pendingB);
			break;
		case PendingAddNode:
		{
			const int added = Waypoints::EditAddNode(HostOrigin());
			applied = added >= 0;
			if (applied)
			{
				Report("navgen: added node %d at your feet\n", added);
			}
			break;
		}
		case PendingSetNode:
			applied = Waypoints::EditSetType(pendingA, pendingType);
			break;
		case PendingMoveNode:
			applied = Waypoints::EditMoveNode(pendingA, HostOrigin());
			if (applied)
			{
				Report("navgen: moved node %d to your feet\n", pendingA);
			}
			break;
		default:
			break;
		}

		if (applied)
		{
			SaveActiveGraph(mapName);
			Report("navgen: edit applied and saved\n");
		}
		else
		{
			Report("navgen: edit failed (bad id, or no graph loaded)\n");
		}
	}

	static constexpr std::uintptr_t CG_WorldPosToScreenPosReal = 0x1400A8040;
	static constexpr std::uintptr_t ScrPlace_GetActivePlacement = 0x1400F2B90;
	static constexpr std::uintptr_t cg_refdefViewOrigin = 0x1404E1158;
	static constexpr std::uintptr_t R_AddCmdDrawQuadPic = 0x14001CCE0;
	static constexpr std::uintptr_t R_AddCmdDrawStretchPic = 0x14001BE90;
	static constexpr std::uintptr_t s_renderCmdBufferSize = 0x1491458B0;
	static constexpr std::uintptr_t s_cmdList = 0x1491458B8;

	static constexpr std::int64_t renderCmdTail = 0x2000;
	static constexpr std::int64_t overlayCmdReserve = 0x2000;
	static constexpr std::int64_t picCmdSize = 0x38;
	static constexpr std::size_t textCmdBase = 96;

	using CG_WorldPosToScreenPosReal_t = bool(*)(int localClientNum, const void* placement, const float* worldPos, float* outScreenPos);
	using ScrPlace_GetActivePlacement_t = const void*(*)(int localClientNum);
	using R_AddCmdDrawQuadPic_t = void(*)(const float (*verts)[2], const float* color, Game::Material* material);
	using R_AddCmdDrawStretchPic_t = void(*)(float x, float y, float w, float h, float s0, float t0, float s1, float t1, const float* color, Game::Material* material);

	static void* showNodesDvar = nullptr;
	static void* showNodesRangeDvar = nullptr;

	static constexpr float labelRangeSq = 700.0f * 700.0f;
	static constexpr float nodeSizePx = 10.0f;
	static constexpr float markSizePx = 14.0f;
	static constexpr float linkThicknessPx = 1.5f;
	static constexpr float routeThicknessPx = 3.0f;
	static constexpr int overlayMarkCap = 32768;
	static constexpr int overlayLineCap = 65536;
	static constexpr int overlayLabelCap = 4096;
	static constexpr int overlayLabelChars = 24;

	struct DrawColor
	{
		float r;
		float g;
		float b;
		float a;
	};

	static constexpr float drawLift = 12.0f;
	static constexpr DrawColor colorStand = { 0.3f, 1.0f, 0.3f, 1.0f };
	static constexpr DrawColor colorCrouch = { 1.0f, 1.0f, 0.2f, 1.0f };
	static constexpr DrawColor colorOther = { 0.4f, 0.7f, 1.0f, 1.0f };
	static constexpr DrawColor colorTwoWay = { 1.0f, 1.0f, 1.0f, 0.7f };
	static constexpr DrawColor colorOneWay = { 1.0f, 0.55f, 0.1f, 1.0f };
	static constexpr DrawColor colorMantle = { 1.0f, 0.25f, 0.25f, 1.0f };
	static constexpr DrawColor colorDrop = { 0.35f, 0.55f, 1.0f, 1.0f };
	static constexpr DrawColor colorLadder = { 0.2f, 1.0f, 1.0f, 1.0f };
	static constexpr DrawColor colorJump = { 1.0f, 0.85f, 0.2f, 1.0f };
	static constexpr DrawColor colorLabel = { 1.0f, 1.0f, 1.0f, 1.0f };
	static constexpr DrawColor colorGlass = { 0.9f, 0.2f, 0.9f, 0.9f };
	static constexpr DrawColor colorBroken = { 0.5f, 0.5f, 0.5f, 0.6f };
	static constexpr DrawColor colorSeed = { 1.0f, 0.9f, 0.1f, 1.0f };
	static constexpr DrawColor colorRoute = { 1.0f, 0.5f, 0.0f, 1.0f };
	static constexpr DrawColor colorProtected = { 1.0f, 0.4f, 1.0f, 1.0f };
	static constexpr DrawColor colorBlockerSurface = { 1.0f, 0.2f, 0.8f, 0.9f };
	static constexpr DrawColor colorBlockerFoliage = { 0.2f, 0.9f, 0.3f, 0.8f };
	static constexpr DrawColor colorBlockerSheet = { 1.0f, 0.6f, 0.1f, 0.9f };
	static constexpr DrawColor colorWallSolid = { 1.0f, 0.25f, 0.2f, 0.9f };
	static constexpr DrawColor colorWallThin = { 1.0f, 0.85f, 0.2f, 0.9f };
	static constexpr DrawColor colorWallConceal = { 0.3f, 0.9f, 1.0f, 0.9f };
	static constexpr DrawColor colorAreaHere = { 1.0f, 1.0f, 1.0f, 1.0f };
	static constexpr DrawColor colorAreaSeen = { 0.3f, 1.0f, 0.3f, 1.0f };
	static constexpr DrawColor colorAreaHidden = { 0.45f, 0.45f, 0.45f, 0.8f };
	static constexpr float radiansPerDegree = 0.017453293f;
	static constexpr int maxWallDrawEdges = 4096;
	static constexpr float wallSliceRangeUnits = 1500.0f;
	static constexpr float wallFloorLift = 2.0f;
	static constexpr float wallHeadRise = 72.0f;
	static constexpr float wallDrawUnits = 96.0f;
	static constexpr float wallDrawRise = 30.0f;
	static constexpr float wallDrawRangeSq = 1200.0f * 1200.0f;
	static constexpr float wallSolidShare = 0.25f;
	static constexpr float wallConcealShare = 0.9f;

	static constexpr DrawColor colorByStage[RemoveStageCount] = {
		{ 0.3f, 1.0f, 0.3f, 1.0f },
		{ 0.6f, 0.6f, 0.6f, 0.9f },
		{ 1.0f, 1.0f, 0.3f, 1.0f },
		{ 1.0f, 0.6f, 0.2f, 1.0f },
		{ 1.0f, 0.3f, 0.3f, 1.0f },
		{ 1.0f, 0.3f, 1.0f, 1.0f },
		{ 0.3f, 1.0f, 1.0f, 1.0f },
		{ 0.6f, 0.2f, 0.8f, 1.0f },
	};

	static constexpr DrawColor colorByHoleKind[4] = {
		{ 1.0f, 0.2f, 0.2f, 1.0f },
		{ 0.4f, 0.6f, 1.0f, 1.0f },
		{ 0.6f, 0.6f, 0.6f, 1.0f },
		{ 0.7f, 0.3f, 0.9f, 1.0f },
	};

	struct OverlayMark
	{
		float x;
		float y;
		float size;
		DrawColor color;
	};

	struct OverlayLine
	{
		float fromX;
		float fromY;
		float toX;
		float toY;
		float thickness;
		DrawColor color;
	};

	struct OverlayLabel
	{
		float x;
		float y;
		DrawColor color;
		char text[overlayLabelChars];
	};

	struct OverlayFrame
	{
		int markCount;
		int lineCount;
		int labelCount;
		OverlayMark marks[overlayMarkCap];
		OverlayLine lines[overlayLineCap];
		OverlayLabel labels[overlayLabelCap];
	};

	static OverlayFrame overlayFrame;

	static int projectionStamp = 0;
	static int projectedStamp[maxRawNodes];
	static bool isProjected[maxRawNodes];
	static float projectedScreen[maxRawNodes][2];

	struct Overlay
	{
		const void* placement;
		CG_WorldPosToScreenPosReal_t worldToScreen;
		const float* camera;
		float rangeSq;
		OverlayFrame* frame;
	};

	static float DistanceSqTo(const Overlay& overlay, const float* world)
	{
		const float dx = world[0] - overlay.camera[0];
		const float dy = world[1] - overlay.camera[1];
		const float dz = world[2] - overlay.camera[2];
		return dx * dx + dy * dy + dz * dz;
	}

	static bool Project(const Overlay& overlay, const float* world, float out[2])
	{
		const float lifted[3] = { world[0], world[1], world[2] + drawLift };
		return overlay.worldToScreen(0, overlay.placement, lifted, out);
	}

	static bool ProjectNode(const Overlay& overlay, int index, const float* world, float out[2])
	{
		if (index < 0 || index >= maxRawNodes)
		{
			return Project(overlay, world, out);
		}

		if (projectedStamp[index] != projectionStamp)
		{
			projectedStamp[index] = projectionStamp;
			isProjected[index] = Project(overlay, world, projectedScreen[index]);
		}

		out[0] = projectedScreen[index][0];
		out[1] = projectedScreen[index][1];
		return isProjected[index];
	}

	static void AddLabel(Overlay& overlay, float x, float y, const DrawColor& color, const char* text)
	{
		OverlayFrame& frame = *overlay.frame;

		if (frame.labelCount >= overlayLabelCap)
		{
			return;
		}

		OverlayLabel& label = frame.labels[frame.labelCount++];
		label.x = x;
		label.y = y;
		label.color = color;
		_snprintf_s(label.text, sizeof(label.text), _TRUNCATE, "%s", text);
	}

	static void AddMark(Overlay& overlay, const float* screen, float sizePx, const DrawColor& color, const char* label, float distanceSq)
	{
		OverlayFrame& frame = *overlay.frame;

		if (frame.markCount >= overlayMarkCap)
		{
			return;
		}

		OverlayMark& mark = frame.marks[frame.markCount++];
		mark.x = screen[0];
		mark.y = screen[1];
		mark.size = sizePx;
		mark.color = color;

		if (label && distanceSq < labelRangeSq)
		{
			AddLabel(overlay, screen[0] + sizePx * 0.5f + 2.0f, screen[1] - 7.0f, colorLabel, label);
		}
	}

	static void AddLine(Overlay& overlay, const float* fromScreen, const float* toScreen, const DrawColor& color, float thickness)
	{
		OverlayFrame& frame = *overlay.frame;

		if (frame.lineCount >= overlayLineCap)
		{
			return;
		}

		OverlayLine& line = frame.lines[frame.lineCount++];
		line.fromX = fromScreen[0];
		line.fromY = fromScreen[1];
		line.toX = toScreen[0];
		line.toY = toScreen[1];
		line.thickness = thickness;
		line.color = color;
	}

	static void DrawMark(Overlay& overlay, const float* world, float sizePx, const DrawColor& color, const char* label, float distanceSq)
	{
		float screen[2];

		if (!Project(overlay, world, screen))
		{
			return;
		}

		AddMark(overlay, screen, sizePx, color, label, distanceSq);
	}

	static void DrawLink(Overlay& overlay, const float* from, const float* to, const DrawColor& color, float thickness)
	{
		float fromScreen[2];
		float toScreen[2];

		if (!Project(overlay, from, fromScreen) || !Project(overlay, to, toScreen))
		{
			return;
		}

		AddLine(overlay, fromScreen, toScreen, color, thickness);
	}

	static bool IsOverlayFull(const Overlay& overlay)
	{
		return overlay.frame->markCount >= overlayMarkCap && overlay.frame->lineCount >= overlayLineCap;
	}

	static const DrawColor& LinkColor(Waypoints::LinkKind kind, bool isTwoWay)
	{
		if (kind == Waypoints::LinkMantle)
		{
			return colorMantle;
		}

		if (kind == Waypoints::LinkDrop)
		{
			return colorDrop;
		}

		if (kind == Waypoints::LinkLadder)
		{
			return colorLadder;
		}

		if (kind == Waypoints::LinkJump)
		{
			return colorJump;
		}

		if (isTwoWay)
		{
			return colorTwoWay;
		}

		return colorOneWay;
	}

	struct DrawCandidate
	{
		float distanceSq;
		int node;
	};

	static bool NearerFirst(const DrawCandidate& a, const DrawCandidate& b)
	{
		return a.distanceSq < b.distanceSq;
	}

	static void DrawFinalGraph(Overlay& overlay)
	{
		static DrawCandidate candidates[maxFinalNodes];
		int candidateCount = 0;
		const int count = Waypoints::Count();

		for (int i = 0; i < count && candidateCount < maxFinalNodes; ++i)
		{
			if (Waypoints::ChildCount(i) == 0)
			{
				continue;
			}

			const float distanceSq = DistanceSqTo(overlay, Waypoints::Origin(i));

			if (distanceSq > overlay.rangeSq)
			{
				continue;
			}

			candidates[candidateCount].distanceSq = distanceSq;
			candidates[candidateCount].node = i;
			++candidateCount;
		}

		std::sort(candidates, candidates + candidateCount, NearerFirst);

		for (int slot = 0; slot < candidateCount && !IsOverlayFull(overlay); ++slot)
		{
			const int i = candidates[slot].node;
			const float* origin = Waypoints::Origin(i);
			float screen[2];

			if (!ProjectNode(overlay, i, origin, screen))
			{
				continue;
			}

			const unsigned char type = Waypoints::TypeOf(i);
			const DrawColor* color = &colorOther;

			if (type == Waypoints::NodeStand)
			{
				color = &colorStand;
			}
			else if (type == Waypoints::NodeCrouch)
			{
				color = &colorCrouch;
			}

			char label[16];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "%d", i);
			AddMark(overlay, screen, nodeSizePx, *color, label, candidates[slot].distanceSq);

			for (int c = 0; c < Waypoints::ChildCount(i); ++c)
			{
				const int child = Waypoints::ChildAt(i, c);
				const Waypoints::LinkKind kind = Waypoints::KindOf(i, child);
				const bool isTwoWay = kind == Waypoints::LinkWalk && Waypoints::HasChild(child, i)
					&& Waypoints::KindOf(child, i) == Waypoints::LinkWalk;

				if (isTwoWay && child < i)
				{
					continue;
				}

				float childScreen[2];

				if (!ProjectNode(overlay, child, Waypoints::Origin(child), childScreen))
				{
					continue;
				}

				AddLine(overlay, screen, childScreen, LinkColor(kind, isTwoWay), linkThicknessPx);
			}
		}
	}

	static void DrawRawGraph(Overlay& overlay)
	{
		static DrawCandidate candidates[maxRawNodes];
		int candidateCount = 0;

		for (int i = 0; i < rawCount && candidateCount < maxRawNodes; ++i)
		{
			const float distanceSq = DistanceSqTo(overlay, rawNodes[i].origin);

			if (distanceSq > overlay.rangeSq)
			{
				continue;
			}

			candidates[candidateCount].distanceSq = distanceSq;
			candidates[candidateCount].node = i;
			++candidateCount;
		}

		std::sort(candidates, candidates + candidateCount, NearerFirst);

		for (int slot = 0; slot < candidateCount && !IsOverlayFull(overlay); ++slot)
		{
			const int i = candidates[slot].node;
			const RawNode& node = rawNodes[i];
			float screen[2];

			if (!ProjectNode(overlay, i, node.origin, screen))
			{
				continue;
			}

			const DrawColor* color = &colorStand;

			if (node.removed)
			{
				unsigned char stage = node.removedBy;

				if (stage >= RemoveStageCount)
				{
					stage = RemovedNever;
				}

				color = &colorByStage[stage];
			}
			else if (IsProtectedRaw(i))
			{
				color = &colorProtected;
			}
			else if (node.type == Waypoints::NodeCrouch)
			{
				color = &colorCrouch;
			}

			char label[16];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "r%d", i);
			AddMark(overlay, screen, nodeSizePx, *color, label, candidates[slot].distanceSq);

			for (int c = 0; c < node.linkCount; ++c)
			{
				const int target = node.links[c];
				const Waypoints::LinkKind kind = static_cast<Waypoints::LinkKind>(node.linkKind[c]);
				const bool isTwoWay = kind == Waypoints::LinkWalk && HasLink(target, i)
					&& LinkKindOf(target, i) == Waypoints::LinkWalk;

				if (isTwoWay && target < i)
				{
					continue;
				}

				float targetScreen[2];

				if (!ProjectNode(overlay, target, rawNodes[target].origin, targetScreen))
				{
					continue;
				}

				AddLine(overlay, screen, targetScreen, LinkColor(kind, isTwoWay), linkThicknessPx);
			}
		}
	}

	static void DrawCoverage(Overlay& overlay)
	{
		for (int i = 0; i < HoleCount(); ++i)
		{
			float centre[3];
			int cells = 0;
			int kind = 0;
			HoleAt(i, centre, &cells, &kind);

			const float distanceSq = DistanceSqTo(overlay, centre);

			if (distanceSq > overlay.rangeSq * 4.0f)
			{
				continue;
			}

			char label[24];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "h%d %dc", i, cells);
			DrawMark(overlay, centre, markSizePx, colorByHoleKind[kind], label, 0.0f);
		}

		for (int s = 0; s < seedCount; ++s)
		{
			const float distanceSq = DistanceSqTo(overlay, seedPositions[s]);

			if (distanceSq > overlay.rangeSq)
			{
				continue;
			}

			DrawMark(overlay, seedPositions[s], markSizePx * 0.6f, colorSeed, "s", distanceSq);
		}
	}

	static void DrawBotPath(Overlay& overlay)
	{
		const short* path = nullptr;
		int length = 0;
		int index = 0;
		const int client = NearestBotPath(overlay.camera, &path, &length, &index);

		if (client < 0 || !path)
		{
			return;
		}

		const int count = Waypoints::Count();

		for (int k = index; k < length; ++k)
		{
			const int node = path[k];

			if (node < 0 || node >= count)
			{
				break;
			}

			char label[16];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "p%d", k - index);
			DrawMark(overlay, Waypoints::Origin(node), markSizePx * 0.7f, colorRoute, label, 0.0f);

			if (k + 1 < length && path[k + 1] >= 0 && path[k + 1] < count)
			{
				DrawLink(overlay, Waypoints::Origin(node), Waypoints::Origin(path[k + 1]), colorRoute, routeThicknessPx);
			}
		}
	}

	static void DrawGlass(Overlay& overlay)
	{
		const int glassCount = GlassPieceCount();

		for (int i = 0; i < glassCount; ++i)
		{
			float centre[3] = { 0.0f, 0.0f, 0.0f };
			GlassPieceOrigin(i, centre);

			const float distanceSq = DistanceSqTo(overlay, centre);

			if (distanceSq > overlay.rangeSq)
			{
				continue;
			}

			const int damage = GlassPieceDamage(i);
			const bool isGone = damage == glassDamageDeleted || damage >= glassDamageDestroy;
			const DrawColor* color = &colorGlass;

			if (isGone)
			{
				color = &colorBroken;
			}

			char label[16];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "g%d", i);

			const float raw[3] = { centre[0], centre[1], centre[2] - drawLift };
			DrawMark(overlay, raw, 8.0f, *color, label, distanceSq);
		}
	}

	static void DrawSightBlockers(Overlay& overlay)
	{
		for (int i = 0; i < SightTriangleCount(); ++i)
		{
			float corners[3][3];

			if (!SightTriangleAt(i, corners) || DistanceSqTo(overlay, corners[0]) > overlay.rangeSq)
			{
				continue;
			}

			for (int edge = 0; edge < 3; ++edge)
			{
				const float* from = corners[edge];
				const float* to = corners[(edge + 1) % 3];
				const float fromLow[3] = { from[0], from[1], from[2] - drawLift };
				const float toLow[3] = { to[0], to[1], to[2] - drawLift };
				DrawLink(overlay, fromLow, toLow, colorBlockerSurface, linkThicknessPx);
			}
		}

		static constexpr int boxEdges[12][2] = {
			{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
			{ 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
		};

		for (int i = 0; i < SightBoxCount(); ++i)
		{
			float corners[8][3];
			bool isSoft = false;

			if (!SightBoxAt(i, corners, &isSoft) || DistanceSqTo(overlay, corners[0]) > overlay.rangeSq)
			{
				continue;
			}

			const DrawColor* color = &colorBlockerSheet;

			if (isSoft)
			{
				color = &colorBlockerFoliage;
			}

			for (const auto& edge : boxEdges)
			{
				const float fromLow[3] = { corners[edge[0]][0], corners[edge[0]][1], corners[edge[0]][2] - drawLift };
				const float toLow[3] = { corners[edge[1]][0], corners[edge[1]][1], corners[edge[1]][2] - drawLift };
				DrawLink(overlay, fromLow, toLow, *color, linkThicknessPx);
			}
		}
	}

	static void DrawRealWalls(Overlay& overlay)
	{
		if (!HasWallGeometry())
		{
			return;
		}

		static WallEdge edges[maxWallDrawEdges];
		float radius = std::sqrt(overlay.rangeSq);

		if (radius > wallSliceRangeUnits)
		{
			radius = wallSliceRangeUnits;
		}

		const float feet[3] = { overlay.camera[0], overlay.camera[1], overlay.camera[2] - Waypoints::standEyeRise };
		float groundZ = feet[2];
		const int groundNode = Waypoints::Nearest(feet);

		if (groundNode >= 0)
		{
			groundZ = Waypoints::Origin(groundNode)[2];
		}

		const float eyeZ = groundZ + Waypoints::standEyeRise;
		const float floorZ = groundZ + wallFloorLift - drawLift;
		const float headZ = groundZ + wallHeadRise - drawLift;
		const int count = SliceWallsNear(overlay.camera, radius, eyeZ, edges, maxWallDrawEdges);

		for (int i = 0; i < count && !IsOverlayFull(overlay); ++i)
		{
			const float floorFrom[3] = { edges[i].from[0], edges[i].from[1], floorZ };
			const float floorTo[3] = { edges[i].to[0], edges[i].to[1], floorZ };
			const float headFrom[3] = { edges[i].from[0], edges[i].from[1], headZ };
			const float headTo[3] = { edges[i].to[0], edges[i].to[1], headZ };
			DrawLink(overlay, floorFrom, floorTo, colorWallSolid, linkThicknessPx);
			DrawLink(overlay, headFrom, headTo, colorWallSolid, linkThicknessPx);
			DrawLink(overlay, floorFrom, headFrom, colorWallSolid, linkThicknessPx);
		}
	}

	static void DrawWallRays(Overlay& overlay)
	{
		if (!Waypoints::HasWalls())
		{
			return;
		}

		static DrawCandidate candidates[maxFinalNodes];
		int candidateCount = 0;
		const int count = Waypoints::Count();
		float rangeSq = overlay.rangeSq;

		if (rangeSq > wallDrawRangeSq)
		{
			rangeSq = wallDrawRangeSq;
		}

		for (int i = 0; i < count && candidateCount < maxFinalNodes; ++i)
		{
			if (Waypoints::ChildCount(i) == 0 || !Waypoints::WallsOf(i))
			{
				continue;
			}

			const float distanceSq = DistanceSqTo(overlay, Waypoints::Origin(i));

			if (distanceSq > rangeSq)
			{
				continue;
			}

			candidates[candidateCount].distanceSq = distanceSq;
			candidates[candidateCount].node = i;
			++candidateCount;
		}

		std::sort(candidates, candidates + candidateCount, NearerFirst);

		for (int slot = 0; slot < candidateCount && !IsOverlayFull(overlay); ++slot)
		{
			const int node = candidates[slot].node;
			const float* origin = Waypoints::Origin(node);
			const Waypoints::NodeWalls* walls = Waypoints::WallsOf(node);
			const float centre[3] = { origin[0], origin[1], origin[2] + wallDrawRise - drawLift };
			DrawMark(overlay, centre, 5.0f, colorLabel, nullptr, candidates[slot].distanceSq);

			for (int direction = 0; direction < Waypoints::wallDirections; ++direction)
			{
				if (walls->standReach[direction] == Waypoints::wallReachOpen)
				{
					continue;
				}

				const float reachUnits = static_cast<float>(walls->standReach[direction]) * Waypoints::wallReachUnit;

				if (reachUnits > wallDrawUnits)
				{
					continue;
				}

				const float share = static_cast<float>(walls->standShare[direction]) / static_cast<float>(Waypoints::wallShareFull - 1);
				const DrawColor* color = &colorWallThin;

				if (share <= wallSolidShare)
				{
					color = &colorWallSolid;
				}
				else if (share >= wallConcealShare)
				{
					color = &colorWallConceal;
				}

				const float yaw = static_cast<float>(direction) * Waypoints::wallDirectionDeg * radiansPerDegree;
				const float end[3] = { centre[0] + std::cos(yaw) * reachUnits, centre[1] + std::sin(yaw) * reachUnits, centre[2] };
				DrawLink(overlay, centre, end, *color, linkThicknessPx);
			}
		}
	}

	static void DrawAreas(Overlay& overlay)
	{
		const int areaCount = Waypoints::AreaCount();

		if (areaCount == 0)
		{
			return;
		}

		const int here = Waypoints::AreaOf(Waypoints::Nearest(overlay.camera));

		for (int area = 0; area < areaCount && !IsOverlayFull(overlay); ++area)
		{
			const Waypoints::AreaInfo* info = Waypoints::AreaAt(area);

			if (!info)
			{
				continue;
			}

			const float* origin = Waypoints::Origin(info->node);
			const float distanceSq = DistanceSqTo(overlay, origin);

			if (distanceSq > overlay.rangeSq * 4.0f)
			{
				continue;
			}

			const DrawColor* color = &colorAreaHidden;

			if (area == here)
			{
				color = &colorAreaHere;
			}
			else if (here >= 0 && Waypoints::AreasSee(here, area))
			{
				color = &colorAreaSeen;
			}

			char label[16];
			_snprintf_s(label, sizeof(label), _TRUNCATE, "a%d", area);
			DrawMark(overlay, origin, markSizePx, *color, label, 0.0f);
		}
	}

	static std::int64_t FreeRenderCmdBytes()
	{
		const auto bufferSize = static_cast<std::int64_t>(*reinterpret_cast<const int*>(Utils::Hook::Rebase(s_renderCmdBufferSize)));
		const auto* const cmdList = *reinterpret_cast<const std::int64_t* const*>(Utils::Hook::Rebase(s_cmdList));
		return bufferSize - renderCmdTail + cmdList[2] - cmdList[1];
	}

	static void RenderOverlay(const OverlayFrame& frame)
	{
		auto* const white = Game::Material_RegisterHandle("white", 7);
		auto* const font = Game::R_RegisterFont("fonts/smallFont", 0);
		const float textHeight = static_cast<float>(Game::R_TextHeight(font));
		const auto drawQuad = reinterpret_cast<R_AddCmdDrawQuadPic_t>(Utils::Hook::Rebase(R_AddCmdDrawQuadPic));
		const auto drawStretchPic = reinterpret_cast<R_AddCmdDrawStretchPic_t>(Utils::Hook::Rebase(R_AddCmdDrawStretchPic));

		const std::int64_t usable = FreeRenderCmdBytes() - overlayCmdReserve;

		if (usable <= 0)
		{
			return;
		}

		const std::int64_t lineBudget = usable * 45 / 100 / picCmdSize;
		const std::int64_t markBudget = usable * 30 / 100 / picCmdSize;
		std::int64_t labelBytes = usable * 25 / 100;

		for (int i = 0; i < frame.lineCount && i < lineBudget; ++i)
		{
			const OverlayLine& line = frame.lines[i];
			const float dx = line.toX - line.fromX;
			const float dy = line.toY - line.fromY;
			const float length = std::sqrt(dx * dx + dy * dy);

			if (length < 0.001f)
			{
				continue;
			}

			const float offsetX = -dy / length * line.thickness * 0.5f;
			const float offsetY = dx / length * line.thickness * 0.5f;
			const float corners[4][2] = {
				{ line.fromX - offsetX, line.fromY - offsetY },
				{ line.toX - offsetX, line.toY - offsetY },
				{ line.toX + offsetX, line.toY + offsetY },
				{ line.fromX + offsetX, line.fromY + offsetY },
			};
			drawQuad(corners, &line.color.r, white);
		}

		for (int i = 0; i < frame.markCount && i < markBudget; ++i)
		{
			const OverlayMark& mark = frame.marks[i];
			const float half = mark.size * 0.5f;
			drawStretchPic(mark.x - half, mark.y - half, mark.size, mark.size, 0.0f, 0.0f, 1.0f, 1.0f, &mark.color.r, white);
		}

		for (int i = 0; i < frame.labelCount; ++i)
		{
			const OverlayLabel& label = frame.labels[i];
			const auto cost = static_cast<std::int64_t>((std::strlen(label.text) + textCmdBase) & ~std::size_t{ 3 });

			if (cost > labelBytes)
			{
				break;
			}

			labelBytes -= cost;
			Game::R_AddCmdDrawText(label.text, overlayLabelChars, font, label.x, label.y + textHeight, 1.0f, 1.0f, 0.0f, &label.color.r, 0);
		}
	}

	static void DrawOverlay()
	{
		if (!showNodesDvar || !Game::CL_IsCgameInitialized(0))
		{
			return;
		}

		const int mode = ReadDvar(showNodesDvar);

		if (mode == 0 || !Waypoints::IsLoaded())
		{
			return;
		}

		float range = 6000.0f;

		if (showNodesRangeDvar)
		{
			range = static_cast<float>(ReadDvar(showNodesRangeDvar));
		}

		overlayFrame.markCount = 0;
		overlayFrame.lineCount = 0;
		overlayFrame.labelCount = 0;
		++projectionStamp;

		Overlay overlay = {};
		overlay.placement = reinterpret_cast<ScrPlace_GetActivePlacement_t>(Utils::Hook::Rebase(ScrPlace_GetActivePlacement))(0);
		overlay.worldToScreen = reinterpret_cast<CG_WorldPosToScreenPosReal_t>(Utils::Hook::Rebase(CG_WorldPosToScreenPosReal));
		overlay.camera = reinterpret_cast<const float*>(Utils::Hook::Rebase(cg_refdefViewOrigin));
		overlay.rangeSq = range * range;
		overlay.frame = &overlayFrame;

		DrawGlass(overlay);

		if (mode == 5)
		{
			DrawSightBlockers(overlay);
		}
		else if (mode == 6)
		{
			DrawRealWalls(overlay);
		}
		else if (mode == 7)
		{
			DrawAreas(overlay);
		}
		else if (mode == 8)
		{
			DrawWallRays(overlay);
		}
		else if (mode == 2)
		{
			DrawRawGraph(overlay);
		}
		else
		{
			DrawFinalGraph(overlay);
		}

		if (mode == 3)
		{
			DrawCoverage(overlay);
		}
		else if (mode == 4)
		{
			DrawBotPath(overlay);
		}

		RenderOverlay(overlayFrame);
	}

	void RegisterOverlay()
	{
		showNodesDvar = Dvar_RegisterInt(
			"bots_shownodes", 0, 0, 8, 0,
			"0 off; 1 the graph; 2 the raw graph coloured by remove stage; 3 coverage (holes, seeds); 4 the nearest bot's route; "
			"5 sight blockers with no collision (magenta drawn surfaces, green foliage, orange sheets); "
			"6 the real walls around you, the solid brushes cut at your eye height; "
			"7 areas (white yours, green in sight of yours, grey hidden); "
			"8 the wall rays of each node (red stops bullets, yellow lets some through, cyan hides but stops nothing)");
		showNodesRangeDvar = Dvar_RegisterInt(
			"bots_shownodes_range", 6000, 250, 50000, 0,
			"how far from the camera bots_shownodes draws, in units");

		Scheduler::Loop(DrawOverlay, Scheduler::Pipeline::RENDERER);
	}
}
