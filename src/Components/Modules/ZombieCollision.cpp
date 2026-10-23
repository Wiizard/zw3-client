#include "STDInclude.hpp"

#include "ZombieCollision.hpp"
#include "ClipMap.hpp"
#include "CollmapCollision.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"
#include "Logger.hpp"

namespace Components
{
	using namespace ClipMap;

	constexpr std::uintptr_t SV_ClipMoveToEntity_BrushModelTraceCall = 0x140233D4B;
	constexpr std::uintptr_t CM_TransformedBrushModelTrace = 0x1401EF6F0;
	constexpr std::uintptr_t CM_TransformedCapsuleTrace = 0x1401EFF60;
	constexpr std::uintptr_t G_TraceCapsule = 0x14019F560;

	constexpr std::uintptr_t pmoveHandlers = 0x14041F640;
	constexpr std::uintptr_t serverPmoveTraces[] = { pmoveHandlers + 0x10, pmoveHandlers + 0x20 };
	constexpr std::uintptr_t level_time = 0x1418673E8;
	constexpr std::uintptr_t level_maxclients = 0x1418673E0;

	constexpr int contentsPlayerClip = 0x10000;
	constexpr int nonSolidBrushModel = 1;
	constexpr char modelTypeModel = 0;
	constexpr char modelTypeBrush = 4;

	constexpr float defaultRadius = 15.0f;
	constexpr float defaultHeight = 72.0f;

	constexpr std::size_t historySize = 32;
	constexpr float noAngles[3]{};

	constexpr float insideEpsilon = 0.25f;

	constexpr float pushPerUnit = 10.0f;
	constexpr float maxPushSpeed = 190.0f;
	constexpr float minPushHalfSize = 4.0f;

	constexpr int brushSolid = 0xFFFFFF;

	struct Sample
	{
		int time;
		float origin[3];
	};

	struct Zombie
	{
		bool isActive;
		Game::Bounds box;
		std::array<Sample, historySize> history;
		std::size_t count;
		std::size_t next;
	};

	static std::array<Zombie, Game::MAX_GENTITIES> zombies;
	static std::vector<int> activeZombies;

	static thread_local int rewindTime = 0;

	static const Game::dvar_t* sv_zombieRewind = nullptr;

	static const Game::dvar_t* sv_zombieCollision = nullptr;

	static bool IsEnabled()
	{
		return !sv_zombieCollision || sv_zombieCollision->current.enabled;
	}
	static Utils::Hook brushModelTraceHook;

	static const Zombie* FindZombie(const Game::gentity_s* ent)
	{
		const auto& zombie = zombies[ent->s.number];

		if (!IsEnabled() || !zombie.isActive || !ent->r.isInUse || ent->r.modelType != modelTypeModel || !(ent->r.contents & contentsPlayerClip))
		{
			return nullptr;
		}

		return &zombie;
	}

	static void OriginAt(const Zombie& zombie, const Game::gentity_s* ent, const int time, float* origin)
	{
		std::memcpy(origin, ent->r.currentOrigin, sizeof(float[3]));

		const Sample* after = nullptr;

		for (std::size_t i = 0; i < zombie.count; ++i)
		{
			const auto& sample = zombie.history[(zombie.next + historySize - 1 - i) % historySize];

			if (sample.time <= time)
			{
				if (!after)
				{
					return;
				}

				const float fraction = static_cast<float>(time - sample.time) / static_cast<float>(after->time - sample.time);

				for (int j = 0; j < 3; ++j)
				{
					origin[j] = sample.origin[j] + (after->origin[j] - sample.origin[j]) * fraction;
				}

				return;
			}

			after = &sample;
		}

		if (after)
		{
			std::memcpy(origin, after->origin, sizeof(float[3]));
		}
	}

	static bool IsInside(const float* start, const Game::Bounds* bounds, const float* origin, const Game::Bounds* box)
	{
		for (int i = 0; i < 3; ++i)
		{
			const float distance = std::fabs((start[i] + bounds->midPoint[i]) - (origin[i] + box->midPoint[i]));

			if (distance >= bounds->halfSize[i] + box->halfSize[i] - insideEpsilon)
			{
				return false;
			}
		}

		return true;
	}

	static void OutwardOf(const float* point, const float* origin, const Game::Bounds* box, float* out, float* distance)
	{
		out[0] = point[0] - (origin[0] + box->midPoint[0]);
		out[1] = point[1] - (origin[1] + box->midPoint[1]);
		*distance = std::sqrt(out[0] * out[0] + out[1] * out[1]);

		if (*distance < 0.001f)
		{
			out[0] = 1.0f;
			out[1] = 0.0f;
			return;
		}

		out[0] /= *distance;
		out[1] /= *distance;
	}

	static void ClipInside(Game::trace_t* results, const float* start, const float* end, const float* origin, const Game::Bounds* box, const int contents, const int number)
	{
		float out[2]{};
		float distance = 0.0f;
		OutwardOf(start, origin, box, out, &distance);

		if ((end[0] - start[0]) * out[0] + (end[1] - start[1]) * out[1] >= 0.0f || results->fraction == 0.0f)
		{
			return;
		}

		results->fraction = 0.0f;
		results->normal[0] = out[0];
		results->normal[1] = out[1];
		results->normal[2] = 0.0f;
		results->contents = contents;
		results->walkable = false;

		if (number >= 0)
		{
			results->hitType = Game::TRACE_HITTYPE_ENTITY;
			results->hitId = static_cast<unsigned short>(number);
		}
	}

	static void PushOut(Game::playerState_s* ps, const Game::Bounds& bounds, const float* origin, const Game::Bounds* box)
	{
		if (!IsInside(ps->origin, &bounds, origin, box))
		{
			return;
		}

		float out[2]{};
		float distance = 0.0f;
		OutwardOf(ps->origin, origin, box, out, &distance);

		const float depth = bounds.halfSize[0] + box->halfSize[0] - distance;
		const float speed = std::min(depth * pushPerUnit, maxPushSpeed);
		const float along = ps->velocity[0] * out[0] + ps->velocity[1] * out[1];

		if (depth > 0.0f && along < speed)
		{
			ps->velocity[0] += out[0] * (speed - along);
			ps->velocity[1] += out[1] * (speed - along);
		}
	}

	static Game::Bounds DecodeSolid(const int solid)
	{
		const float halfWidth = static_cast<float>(solid & 0xFF) - 1.0f;
		const float bottom = 1.0f - (static_cast<float>((solid >> 8) & 0xFF) - 1.0f);
		const float top = static_cast<float>((solid >> 16) & 0xFF) - 33.0f;

		Game::Bounds box{};
		box.midPoint[2] = (bottom + top) * 0.5f;
		box.halfSize[0] = halfWidth;
		box.halfSize[1] = halfWidth;
		box.halfSize[2] = (top - bottom) * 0.5f;
		return box;
	}

	static void Deactivate(const int number)
	{
		zombies[number].isActive = false;
		std::erase(activeZombies, number);
	}

	void ZombieCollision::PostLink(Game::gentity_s* ent)
	{
		auto& zombie = zombies[ent->s.number];

		if (!zombie.isActive)
		{
			return;
		}

		if (!FindZombie(ent))
		{
			Deactivate(ent->s.number);
			return;
		}

		const int time = Utils::Hook::Get<int>(level_time);
		auto* sample = zombie.count ? &zombie.history[(zombie.next + historySize - 1) % historySize] : nullptr;

		if (!sample || sample->time != time)
		{
			sample = &zombie.history[zombie.next];
			zombie.next = (zombie.next + 1) % historySize;
			zombie.count = std::min(zombie.count + 1, historySize);
		}

		sample->time = time;
		std::memcpy(sample->origin, ent->r.currentOrigin, sizeof(sample->origin));

		ent->s.solid = EncodeSolid(zombie.box);
	}

	static void G_TraceCapsule_Pmove(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const int passEntityNum, const int contentmask)
	{
		reinterpret_cast<void(*)(Game::trace_t*, const float*, const float*, const Game::Bounds*, int, int)>(Utils::Hook::Rebase(G_TraceCapsule))
			(results, start, end, bounds, passEntityNum, contentmask);

		for (const int number : activeZombies)
		{
			const auto* const ent = &Game::g_entities[number];
			const auto* const zombie = FindZombie(ent);

			if (!zombie || number == passEntityNum || ent->r.ownerNum - 1 == passEntityNum || !(ent->r.contents & contentmask))
			{
				continue;
			}

			float origin[3]{};
			OriginAt(*zombie, ent, rewindTime ? rewindTime : std::numeric_limits<int>::max(), origin);

			if (IsInside(start, bounds, origin, &zombie->box))
			{
				ClipInside(results, start, end, origin, &zombie->box, ent->r.contents, number);
				continue;
			}

			const float oldFraction = results->fraction;

			reinterpret_cast<void(*)(Game::trace_t*, const float*, const float*, const Game::Bounds*, const Game::Bounds*, int, const float*, const float*)>(Utils::Hook::Rebase(CM_TransformedCapsuleTrace))
				(results, start, end, bounds, &zombie->box, ent->r.contents, origin, noAngles);

			if (results->fraction < oldFraction)
			{
				results->hitType = Game::TRACE_HITTYPE_ENTITY;
				results->hitId = static_cast<unsigned short>(number);
			}
		}
	}

	bool ZombieCollision::ClipToEntity(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const Game::Bounds* box, const int contents, const float* origin, const float* angles)
	{
		if (!IsEnabled())
		{
			return false;
		}

		Game::Bounds networked{};

		if (const auto* const ent = ServerEntityAt(origin))
		{
			if (ent->s.eType == Game::ET_PLAYER)
			{
				return false;
			}

			if (FindZombie(ent))
			{
				return true;
			}

			if (!ent->s.solid)
			{
				return true;
			}

			networked = DecodeSolid(ent->s.solid);
			box = &networked;
		}
		else if (const auto* const cent = ClientEntityAt(origin))
		{
			if (cent->nextState.eType == Game::ET_PLAYER)
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		if (box->halfSize[0] < 1.0f || box->halfSize[2] <= 0.0f)
		{
			return true;
		}

		if (IsInside(start, bounds, origin, box))
		{
			ClipInside(results, start, end, origin, box, contents, -1);
			return true;
		}

		reinterpret_cast<void(*)(Game::trace_t*, const float*, const float*, const Game::Bounds*, const Game::Bounds*, int, const float*, const float*)>(Utils::Hook::Rebase(CM_TransformedCapsuleTrace))
			(results, start, end, bounds, box, contents, origin, angles);

		return true;
	}

	static void PushOutOfClientBoxes(Game::pmove_s* pm)
	{
		const auto* const entities = reinterpret_cast<const Game::centity_s*>(Utils::Hook::Rebase(cg_entities));

		for (std::size_t i = 0; i < Game::MAX_GENTITIES; ++i)
		{
			const auto& cent = entities[i];
			const auto& es = cent.nextState;

			if (!cent.nextValid || !es.solid || es.solid == brushSolid || (es.eType != Game::ET_GENERAL && es.eType != Game::ET_SCRIPTMOVER))
			{
				continue;
			}

			const auto box = DecodeSolid(es.solid);

			if (box.halfSize[0] >= minPushHalfSize)
			{
				PushOut(pm->ps, pm->bounds, cent.pose.origin, &box);
			}
		}
	}

	void ZombieCollision::BeginPmove(Game::pmove_s* pm)
	{
		rewindTime = 0;

		if (!IsEnabled())
		{
			return;
		}

		const auto* const clients = Game::level->clients;
		const auto offset = reinterpret_cast<std::uintptr_t>(pm->ps) - reinterpret_cast<std::uintptr_t>(clients);
		const auto clientNum = offset / sizeof(Game::gclient_s);

		if (!clients || offset % sizeof(Game::gclient_s) || clientNum >= static_cast<std::uintptr_t>(Utils::Hook::Get<int>(level_maxclients)))
		{
			PushOutOfClientBoxes(pm);
			return;
		}

		const int maxRewind = sv_zombieRewind ? sv_zombieRewind->current.integer : 0;

		if (maxRewind > 0 && !Game::SV_IsTestClient(static_cast<int>(clientNum)))
		{
			const int now = Utils::Hook::Get<int>(level_time);
			rewindTime = std::clamp(pm->cmd.serverTime, now - maxRewind, now);
		}

		for (const int number : activeZombies)
		{
			const auto* const ent = &Game::g_entities[number];
			const auto* const zombie = FindZombie(ent);

			if (!zombie || zombie->box.halfSize[0] < minPushHalfSize || ent->r.ownerNum - 1 == static_cast<int>(clientNum) || !(ent->r.contents & pm->tracemask))
			{
				continue;
			}

			float origin[3]{};
			OriginAt(*zombie, ent, rewindTime ? rewindTime : std::numeric_limits<int>::max(), origin);
			PushOut(pm->ps, pm->bounds, origin, &zombie->box);
		}
	}

	void ZombieCollision::EndPmove()
	{
		rewindTime = 0;
	}

	void ZombieCollision::SV_BrushModelTrace_Hk(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const unsigned int model, const int contents, const float* origin, const float* angles)
	{
		if ((contents & contentsPlayerClip) && IsEnabled())
		{
			if (const auto* const ent = ServerEntityAt(origin); ent && (ent->s.lerp.eFlags & nonSolidBrushModel))
			{
				return;
			}
		}

		reinterpret_cast<decltype(&SV_BrushModelTrace_Hk)>(Utils::Hook::Rebase(CM_TransformedBrushModelTrace))(results, start, end, bounds, model, contents, origin, angles);
	}

	void ZombieCollision::AddScriptMethods()
	{
		GSC::Script::AddMethod("SetZombieCollision", [](const Game::scr_entref_t entref)
		{
			const auto numParam = Game::Scr_GetNumParam();

			if (numParam != 1 && numParam != 3)
			{
				GSC::Script::Scr_Error("usage: <entity> SetZombieCollision( <enabled> [, <radius>, <height>] )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);

			if (ent->s.eType == Game::ET_PLAYER || ent->r.modelType != modelTypeModel)
			{
				GSC::Script::Scr_Error("SetZombieCollision: entity must be a script model\n");
				return;
			}

			if (!CollmapCollision::HasLinkHook())
			{
				GSC::Script::Scr_Error("SetZombieCollision: SV_LinkEntity is not hooked, see the console\n");
				return;
			}

			auto& zombie = zombies[ent->s.number];

			if (!Game::Scr_GetInt(0))
			{
				Deactivate(ent->s.number);
				ent->r.contents &= ~contentsPlayerClip;
				Game::SV_LinkEntity(ent);
				return;
			}

			const float radius = numParam == 3 ? Game::Scr_GetFloat(1) : defaultRadius;
			const float height = numParam == 3 ? Game::Scr_GetFloat(2) : defaultHeight;

			if (radius <= 0.0f || height <= 0.0f)
			{
				GSC::Script::Scr_ParamError(1, "SetZombieCollision: radius and height must be above 0");
				return;
			}

			Deactivate(ent->s.number);
			zombie = {};
			zombie.isActive = true;
			activeZombies.push_back(ent->s.number);
			zombie.box.midPoint[2] = height * 0.5f;
			zombie.box.halfSize[0] = radius;
			zombie.box.halfSize[1] = radius;
			zombie.box.halfSize[2] = height * 0.5f;

			ent->r.contents |= contentsPlayerClip;
			Game::SV_LinkEntity(ent);
		});

		GSC::Script::AddMethod("SetMovementSolid", [](const Game::scr_entref_t entref)
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				GSC::Script::Scr_Error("usage: <entity> SetMovementSolid( <solid> )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);

			if (ent->r.modelType != modelTypeBrush)
			{
				return;
			}

			if (Game::Scr_GetInt(0))
			{
				ent->s.lerp.eFlags &= ~nonSolidBrushModel;
			}
			else
			{
				ent->s.lerp.eFlags |= nonSolidBrushModel;
			}
		});
	}

	ZombieCollision::ZombieCollision()
	{
		if (!Utils::Hook::BranchesTo(SV_ClipMoveToEntity_BrushModelTraceCall, CM_TransformedBrushModelTrace, HOOK_CALL)
			|| !brushModelTraceHook.Initialize(SV_ClipMoveToEntity_BrushModelTraceCall, reinterpret_cast<void*>(SV_BrushModelTrace_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("zombiecollision: SV_ClipMoveToEntity does not read as expected, SetMovementSolid does nothing on the server\n");
		}
		else
		{
			brushModelTraceHook.Quick();
		}

		bool isExpected = true;

		for (const auto trace : serverPmoveTraces)
		{
			isExpected = isExpected && Utils::Hook::Get<std::uintptr_t>(trace) == Utils::Hook::Rebase(G_TraceCapsule);
		}

		if (isExpected)
		{
			for (const auto trace : serverPmoveTraces)
			{
				Utils::Hook::Set<void*>(trace, reinterpret_cast<void*>(G_TraceCapsule_Pmove));
			}
		}
		else
		{
			Logger::Error("zombiecollision: the server pmove handlers do not read as expected, zombies do not block movement on the server\n");
		}

		Events::OnDvarInit([]
		{
			sv_zombieRewind = Game::Dvar_RegisterInt("sv_zombieRewind", 200, 0, 500, Game::DVAR_NONE, "How many ms back a player's movement sees zombies, to match what they saw");
			sv_zombieCollision = Game::Dvar_RegisterBool("sv_zombieCollision", true, Game::DVAR_NONE,
				"Zombies block players as an upright box; 0 puts every entity trace back to stock, for comparing (a zombie that moves while it is 0 keeps the old collision until it respawns)");
		});

		AddScriptMethods();
	}
}
