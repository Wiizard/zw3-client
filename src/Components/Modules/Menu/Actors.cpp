#include "STDInclude.hpp"
#include "Actors.hpp"
#include "../Dedicated.hpp"
#include "../LobbyCombat.hpp"
#include "../Logger.hpp"
#include "../Party.hpp"
#include "../Scheduler.hpp"
#include "../ZoneBuilder.hpp"
#include "Engine.hpp"
#include <numbers>
#include <zlib.h>

namespace Components::Menu
{
	namespace
	{
		using Vec = std::array<float, 3>;
		using namespace Engine;
		constexpr std::size_t survivorCount = 4, zombieCount = 32;
		constexpr float radians = std::numbers::pi_v<float> / 180.0f;
		enum Clip : unsigned int { Idle, Fire, Death, Walk, RifleIdle, RifleFire, ShotgunFire, Run, Melee, Attack, ClipCount };
		constexpr const char* modelNames[] = {"zw3_richtofen_hd", "zw3_dempsey_hd", "zw3_nikolai_hd", "zw3_takeo_hd", "fsm_zombie_barechest"};
		constexpr const char* weaponNames[] = {"wm_t5_1911", "t5_wm_commando", "t5_wm_ak74u", "t5_wm_beretta682"};
		constexpr const char* playerClips[ClipCount] = {
			"pb_stand_ads_pistol", "pt_stand_shoot_pistol", nullptr, "pb_combatwalk_forward_loop_pistol",
			"pb_rifle_idlea_ads", "pt_rifle_fire_ads", "pt_stand_shoot_shotgun", nullptr, "pt_melee_pistol_1", nullptr};
		constexpr const char* zombieClips[ClipCount] = {
			"ai_zombie_walk_v1", nullptr, "fsm_zombie_death", "ai_zombie_walk_v1",
			nullptr, nullptr, nullptr, "ai_zombie_run_v1", nullptr, "zw2_zombie_attack01"};
		std::array<Game::XModel*, 5> models{};
		std::array<Game::XModel*, 4> weapons{};
		Game::XModel* zombieHead = nullptr;
		std::array<std::array<Game::XAnimParts*, ClipCount>, 2> clips{};
		bool prepared = false, ready = false;
		thread_local std::vector<void*>* allocationBlocks = nullptr;

		void* AllocateAnimation(int size)
		{
			assert(allocationBlocks && size > 0);
			auto* block = std::calloc(1, static_cast<std::size_t>(size));
			if (!block) throw std::bad_alloc();
			allocationBlocks->push_back(block);
			return block;
		}

		void FreeAnimation(void* block, int)
		{
			if (!allocationBlocks) return;
			const auto it = std::ranges::find(*allocationBlocks, block);
			if (it == allocationBlocks->end()) return;
			std::free(block);
			allocationBlocks->erase(it);
		}

		struct NativeActor
		{
			alignas(16) std::array<std::byte, sizeof(Game::DObj)> object{};
			Game::cpose_t pose{};
			XAnim_s* anims = nullptr;
			XAnimTree_s* tree = nullptr;
			std::vector<void*> blocks;
			int model = -1, weapon = -1;
			bool created = false;
			unsigned int lastBase = ClipCount, lastAction = ClipCount;
			unsigned int lastActionStart = 0;
			Game::DObj* Object() { return reinterpret_cast<Game::DObj*>(object.data()); }

			void Release()
			{
				if (created) DObjFree(Object());
				created = false;
				allocationBlocks = &blocks;
				if (tree) XAnimFreeTree(tree, FreeAnimation);
				allocationBlocks = nullptr;
				for (auto* block : blocks) std::free(block);
				blocks.clear();
				tree = nullptr;
				anims = nullptr;
				model = weapon = -1;
				lastBase = lastAction = ClipCount;
			}

			bool Create(int selectedModel, int selectedWeapon, unsigned int entity)
			{
				Logger::Debug("menu actors: creating native {} (entity {}, weapon {})\n", modelNames[selectedModel], entity, selectedWeapon);
				model = selectedModel;
				weapon = selectedWeapon;
				allocationBlocks = &blocks;
				anims = XAnimCreateAnims("menu actors", ClipCount + 1, AllocateAnimation);
				// The fifth parameter is a full uint32 count on the stack in 2.0.13.
				XAnimBlend(anims, 0, "root", 1, ClipCount, 0);
				const auto& parts = clips[selectedModel == 4];
				for (unsigned int i = 0; i < ClipCount; ++i)
					XAnimCreate(anims, i + 1, (parts[i] ? parts[i] : parts[Idle])->name);
				tree = XAnimCreateTree(anims, AllocateAnimation);
				allocationBlocks = nullptr;

				DObjModel_s entries[3]{{models[selectedModel], 0, false, {}}};
				unsigned int count = 1;
				if (selectedModel == 4 && zombieHead) entries[count++] = {zombieHead, 0, true, {}};
				if (selectedWeapon >= 0 && weapons[selectedWeapon])
				{
					const auto tag = Engine::Bind<unsigned int (*)(const char*)>(0x140221910)("tag_weapon_right");
					if (tag) entries[count++] = {weapons[selectedWeapon], static_cast<std::uint16_t>(tag), true, {}};
				}
				object = {};
				DObjCreate(entries, count, tree, reinterpret_cast<char*>(object.data()), entity);
				created = true;
				return true;
			}
		};

		struct State
		{
			Vec origin{};
			float yaw = 0;
			int model = 0, weapon = -1;
			unsigned int base = Idle, action = ClipCount, actionStart = 0;
			bool visible = false, dead = false;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(State, origin, yaw, model, weapon, base, action, actionStart, visible, dead)
		};
		struct Survivor
		{
			State state;
			Vec home{};
			unsigned int nextShot = 0, nextWeapon = 0, meleeImpact = 0;
			int target = -1;
			unsigned int burst = 0;
		};
		struct Zombie
		{
			State state;
			unsigned int respawn = 0, lastAttack = 0, blockedSince = 0;
			int target = -1;
		};
		struct Packet
		{
			unsigned int version = 2, time = 0;
			std::string map;
			std::array<State, survivorCount> survivors{};
			std::array<State, zombieCount> zombies{};
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Packet, version, time, map, survivors, zombies)
		};
		std::array<NativeActor, survivorCount + zombieCount> entities;
		std::array<Survivor, survivorCount> survivors;
		std::array<Zombie, zombieCount> zombies;
		BackdropData::spawn standing{};
		std::string currentMap;
		unsigned int nextSpawn = 0, spawnSequence = 0;
		std::mutex snapshotMutex;
		std::optional<Packet> remote, previousRemote;
		unsigned int receivedAt = 0;
		std::string published;
		std::atomic_int clockOffset = 0;

		unsigned int Now() { return timeGetTime(); }
		bool Due(unsigned int now, unsigned int deadline) { return static_cast<int>(now - deadline) >= 0; }
		float Distance(const Vec& a, const Vec& b) { return std::hypot(a[0] - b[0], a[1] - b[1]); }
		float Turn(float from, float to, float amount)
		{
			return from + std::remainder(to - from, 360.0f) * std::clamp(amount, 0.0f, 1.0f);
		}
		Vec Offset(float forward, float side)
		{
			const float yaw = standing.yaw * radians;
			return {standing.origin[0] + std::cos(yaw) * forward - std::sin(yaw) * side,
				standing.origin[1] + std::sin(yaw) * forward + std::cos(yaw) * side, standing.origin[2]};
		}

		Game::trace_t Trace(const Vec& start, const Vec& end, bool capsule = false)
		{
			const Game::Bounds bounds = capsule ? Game::Bounds{{0, 0, 36}, {14, 14, 34}} : Game::Bounds{};
			Game::trace_t result{};
			// CG_WorldTrace supplies the engine's glass callback to CM_WorldTrace.
			Engine::Bind<void (*)(Game::trace_t*, const float*, const float*, const Game::Bounds*, int)>(0x1400CA9A0)(
				&result, start.data(), end.data(), &bounds, 0x10001);
			return result;
		}
		bool Ground(Vec& point)
		{
			Vec top = point, bottom = point;
			top[2] += 48;
			bottom[2] -= 100;
			const auto hit = Trace(top, bottom);
			if (hit.startsolid || hit.allsolid || hit.fraction >= 1 || hit.normal[2] < 0.65f) return false;
			point[2] = std::lerp(top[2], bottom[2], hit.fraction) + 0.5f;
			return true;
		}
		bool Clear(const Vec& start, const Vec& end, bool capsule = true)
		{
			const auto hit = Trace(start, end, capsule);
			return !hit.startsolid && !hit.allsolid && hit.fraction > 0.99f;
		}
		bool LineOfSight(const Vec& from, const Vec& to)
		{
			Vec a = from, b = to;
			a[2] += 45;
			b[2] += 45;
			return Clear(a, b, false);
		}

		bool MenuVisible(const char* name)
		{
			auto* menu = Game::Menus_FindByName(Game::uiContext, name);
			return menu && Game::Menu_IsVisible(Game::uiContext, menu);
		}
		std::array<int, 4> Roster()
		{
			const bool privateLobby = MenuVisible("menu_xboxlive_privatelobby");
			const bool partyLobby = privateLobby || MenuVisible("zwnet_matchmaking") || MenuVisible("zwnet_party_lobby");
			int count = 1;
			const auto* countDvar = Game::Dvar_FindVar(privateLobby ? "party_currentPlayers" : "zwnet_lobby_member_count");
			if (partyLobby && countDvar && countDvar->type == Game::DVAR_TYPE_INT) count = std::clamp(countDvar->current.integer, 1, 4);
			std::array<int, 4> result{-1, -1, -1, -1};
			constexpr const char* names[] = {"Richtofen", "Dempsey", "Nikolai", "Takeo"};
			std::size_t occupied = 0;
			for (int slot = 0; slot < count; ++slot)
			{
				const auto* owner = Game::Dvar_FindVar(Utils::String::VA("character_%d_player", slot + 1));
				if (privateLobby && owner && owner->type == Game::DVAR_TYPE_STRING && owner->current.string
					&& (!*owner->current.string || !_stricmp(owner->current.string, "None"))) continue;
				int model = slot;
				const auto* character = Game::Dvar_FindVar(Utils::String::VA("character_%d", slot + 1));
				if (character && character->type == Game::DVAR_TYPE_STRING && character->current.string)
					for (int i = 0; i < 4; ++i) if (!_stricmp(character->current.string, names[i])) model = i;
				result[occupied++] = model;
			}
			return result;
		}

		unsigned int Duration(bool zombie, unsigned int clip)
		{
			const auto* parts = clips[zombie][clip];
			return parts && parts->framerate > 0 ? static_cast<unsigned int>(parts->numframes * 1000.0f / parts->framerate) : 1000;
		}
		void Action(State& state, unsigned int clip, unsigned int now)
		{
			state.action = clip;
			state.actionStart = now;
		}
		void Kill(Zombie& zombie, unsigned int now)
		{
			zombie.state.dead = true;
			Action(zombie.state, Death, now);
			zombie.respawn = now + 4500;
			zombie.target = -1;
		}

		void UpdateRoster(unsigned int now)
		{
			const auto roster = Roster();
			for (std::size_t slot = 0; slot < survivorCount; ++slot)
			{
				auto& survivor = survivors[slot];
				if (roster[slot] < 0) { survivor.state.visible = false; continue; }
				const bool changed = !survivor.state.visible || survivor.state.model != roster[slot];
				if (!changed) continue;
				survivor = {};
				survivor.state.model = roster[slot];
				survivor.state.weapon = static_cast<int>(slot % 4);
				survivor.state.yaw = standing.yaw;
				survivor.nextWeapon = now + 20000 + static_cast<unsigned int>(slot) * 3000;
				survivor.nextShot = now + 1500;
				// Find a grounded, reachable place near the map's spawn, without
				// assuming Kino's floor, stairs, or balcony coordinates.
				for (int attempt = 0; attempt < 24; ++attempt)
				{
					const float side = (static_cast<float>(slot) - 1.5f) * 45.0f + static_cast<float>(attempt % 3 - 1) * 12.0f;
					Vec candidate = Offset(-static_cast<float>(attempt / 3) * 20.0f, side);
					if (!Ground(candidate) || !Clear(standing.origin, candidate)) continue;
					bool occupied = false;
					for (std::size_t other = 0; other < slot; ++other)
						if (survivors[other].state.visible && Distance(candidate, survivors[other].home) < 30) occupied = true;
					if (occupied) continue;
					survivor.home = survivor.state.origin = candidate;
					survivor.state.visible = true;
					break;
				}
			}
		}

		void Simulate(unsigned int now, float dt)
		{
			UpdateRoster(now);
			const int count = static_cast<int>(std::ranges::count_if(survivors, [](const Survivor& s) { return s.state.visible; }));
			if (!count) return;
			const auto population = LobbyCombat::Population(count);
			for (auto& zombie : zombies)
			{
				if (zombie.state.dead && Due(now, zombie.respawn)) zombie.state.visible = false;
			}
			const auto active = std::ranges::count_if(zombies, [](const Zombie& z) { return z.state.visible; });
			if (active < population && Due(now, nextSpawn))
			{
				nextSpawn = now + LobbyCombat::SpawnInterval(count);
				for (auto& zombie : zombies)
				{
					if (zombie.state.visible) continue;
					for (unsigned int attempt = 0; attempt < 32; ++attempt)
					{
						const float angle = static_cast<float>((spawnSequence * 137 + attempt * 47) % 360) * radians;
						const float distance = 180.0f + static_cast<float>(attempt % 4) * 35.0f;
						Vec candidate = Offset(std::cos(angle) * distance, std::sin(angle) * distance);
						if (!Ground(candidate)) continue;
						bool reachable = false;
						for (const auto& survivor : survivors)
							if (survivor.state.visible && Clear(candidate, survivor.home)) reachable = true;
						if (!reachable) continue;
						zombie = {};
						zombie.state.model = 4;
						zombie.state.weapon = -1;
						zombie.state.origin = candidate;
						zombie.state.visible = true;
						zombie.state.base = (++spawnSequence % 3 == 0) ? Run : Walk;
						break;
					}
					break;
				}
			}

			for (auto& zombie : zombies)
			{
				auto& state = zombie.state;
				if (!state.visible || state.dead) continue;
				float distance = std::numeric_limits<float>::max();
				zombie.target = -1;
				for (std::size_t i = 0; i < survivorCount; ++i)
				{
					if (!survivors[i].state.visible) continue;
					const float next = Distance(state.origin, survivors[i].state.origin);
					if (next < distance) { distance = next; zombie.target = static_cast<int>(i); }
				}
				if (zombie.target < 0) continue;
				const auto& goal = survivors[zombie.target].state.origin;
				const float heading = std::atan2(goal[1] - state.origin[1], goal[0] - state.origin[0]) / radians;
				state.yaw = Turn(state.yaw, heading, dt * 6);
				if (state.action < ClipCount && now - state.actionStart < Duration(true, state.action)) continue;
				state.action = ClipCount;
				if (distance < LobbyCombat::attackDistance && LineOfSight(state.origin, goal))
				{
					Action(state, Attack, now);
					zombie.lastAttack = now;
					continue;
				}
				const float speed = state.base == Run ? 65.0f : 30.0f;
				bool moved = false;
				for (float deflection : {0.0f, 40.0f, -40.0f, 80.0f, -80.0f})
				{
					const float angle = (heading + deflection) * radians;
					Vec next = state.origin;
					next[0] += std::cos(angle) * speed * dt;
					next[1] += std::sin(angle) * speed * dt;
					if (!Ground(next) || std::abs(next[2] - state.origin[2]) > 18 || !Clear(state.origin, next)) continue;
					bool crowded = false;
					for (const auto& other : zombies)
						if (&other != &zombie && other.state.visible && !other.state.dead && Distance(other.state.origin, next) < 24) crowded = true;
					if (crowded) continue;
					state.origin = next;
					moved = true;
					break;
				}
				if (moved) zombie.blockedSince = 0;
				else if (!zombie.blockedSince) zombie.blockedSince = now;
				else if (now - zombie.blockedSince > 8000) state.visible = false;
			}

			for (auto& survivor : survivors)
			{
				auto& state = survivor.state;
				if (!state.visible) continue;
				if (Due(now, survivor.nextWeapon))
				{
					state.weapon = (state.weapon + 1) % 4;
					survivor.nextWeapon = now + 25000;
				}
				state.base = state.weapon == 0 ? Idle : RifleIdle;
				float best = 500;
				int target = -1;
				for (std::size_t i = 0; i < zombieCount; ++i)
				{
					const auto& z = zombies[i].state;
					if (!z.visible || z.dead) continue;
					const float distance = Distance(state.origin, z.origin);
					if (distance < best && LineOfSight(state.origin, z.origin)) { best = distance; target = static_cast<int>(i); }
				}
				survivor.target = target;
				if (target >= 0)
				{
					const auto& goal = zombies[target].state.origin;
					state.yaw = Turn(state.yaw, std::atan2(goal[1] - state.origin[1], goal[0] - state.origin[0]) / radians, dt * 5);
				}
				else state.yaw = Turn(state.yaw, standing.yaw, dt * 2);
				if (survivor.meleeImpact && Due(now, survivor.meleeImpact))
				{
					for (auto& zombie : zombies)
						if (zombie.state.visible && !zombie.state.dead && LobbyCombat::InMeleeSweep(
							zombie.state.origin[0] - state.origin[0], zombie.state.origin[1] - state.origin[1], state.yaw * radians)) Kill(zombie, now);
					survivor.meleeImpact = 0;
				}
				if (state.action < ClipCount && now - state.actionStart >= Duration(false, state.action)) state.action = ClipCount;
				if (target < 0 || !Due(now, survivor.nextShot) || state.action == Melee) continue;
				if (best < LobbyCombat::meleeDistance && zombies[target].state.action == Attack && now - zombies[target].lastAttack > 450)
				{
					Action(state, Melee, now);
					survivor.meleeImpact = now + 280;
					survivor.nextShot = now + 1300;
					survivor.burst = 0;
				}
				else if (best > LobbyCombat::gunSafetyDistance)
				{
					Action(state, state.weapon == 0 ? Fire : state.weapon == 3 ? ShotgunFire : RifleFire, now);
					if (!survivor.burst) survivor.burst = state.weapon == 3 ? 1 : 3;
					if (--survivor.burst == 0)
					{
						Kill(zombies[target], now);
						survivor.nextShot = now + LobbyCombat::FireCooldown(count);
					}
					else survivor.nextShot = now + (state.weapon == 0 ? 160 : 115);
				}
			}
		}

		bool Valid(const State& state)
		{
			return state.model >= 0 && state.model < 5 && state.weapon >= -1 && state.weapon < 4
				&& state.base < ClipCount && state.action <= ClipCount && std::isfinite(state.yaw)
				&& std::ranges::all_of(state.origin, [](float n) { return std::isfinite(n) && std::abs(n) < 10000000; });
		}
		void Publish(unsigned int now)
		{
			static unsigned int last = 0;
			if (now - last < 100 || !Party::IsHostingParty()) return;
			last = now;
			Packet packet;
			packet.map = currentMap;
			packet.time = now;
			for (std::size_t i = 0; i < survivorCount; ++i) packet.survivors[i] = survivors[i].state;
			for (std::size_t i = 0; i < zombieCount; ++i) packet.zombies[i] = zombies[i].state;
			const auto bytes = nlohmann::json::to_msgpack(nlohmann::json(packet));
			std::string compressed(compressBound(static_cast<uLong>(bytes.size())), '\0');
			uLongf length = static_cast<uLongf>(compressed.size());
			if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &length, bytes.data(), static_cast<uLong>(bytes.size()), Z_BEST_SPEED) != Z_OK || length > 8192) return;
			compressed.resize(length);
			std::lock_guard lock(snapshotMutex);
			published = std::move(compressed);
		}

		void Draw(NativeActor& actor, const State& state, unsigned int entity, unsigned int now, float dt)
		{
			const auto& parts = clips[state.model == 4];
			const unsigned int base = parts[state.base] ? state.base : Idle;
			const unsigned int action = state.action < ClipCount && parts[state.action] ? state.action : ClipCount;
			if (base != actor.lastBase || action != actor.lastAction || (action < ClipCount && state.actionStart != actor.lastActionStart))
			{
				const auto clear = Engine::Bind<void (*)(XAnimTree_s*, unsigned int, float)>(0x1402B1350);
				clear(actor.tree, 0, 0.08f);
				const bool fullBodyAction = state.model == 4 && action < ClipCount;
				if (!fullBodyAction) XAnimSetCompleteGoalWeight(actor.Object(), base + 1, 1, 0.08f, 0, 0, 0, 0);
				if (action < ClipCount) XAnimSetCompleteGoalWeight(actor.Object(), action + 1, 1, 0.06f, 0, 0, 0, 1);
				actor.lastBase = base;
				actor.lastAction = action;
				actor.lastActionStart = state.actionStart;
			}
			const auto setTime = Engine::Bind<void (*)(XAnimTree_s*, unsigned int, float)>(0x1402B43B0);
			const auto duration = std::max(1u, Duration(state.model == 4, base));
			setTime(actor.tree, base + 1, static_cast<float>((now + entity * 173) % duration) / static_cast<float>(duration));
			if (action < ClipCount)
				setTime(actor.tree, action + 1, std::clamp(static_cast<float>(now - state.actionStart) / static_cast<float>(std::max(1u, Duration(state.model == 4, action))), 0.0f, 0.999f));
			actor.pose = {};
			std::ranges::copy(state.origin, actor.pose.origin);
			actor.pose.angles[1] = state.yaw;
			DObjUpdateClientInfo(actor.Object(), dt, false);
			Vec lightOrigin = state.origin;
			lightOrigin[2] += 35;
			R_AddDObjToScene(actor.Object(), &actor.pose, entity, 0, lightOrigin.data(), 0);
		}
	}

	bool Actors::IsReady() { return ready; }

	void Actors::Prepare()
	{
		if (prepared || !Game::Sys_IsDatabaseReady2()) return;
		const auto loaded = [](Game::XAssetType type, const char* name) -> void*
		{
			auto* entry = Game::DB_FindXAssetEntry(type, name);
			return entry && !Game::DB_IsXAssetDefault(type, name) ? entry->asset.header.data : nullptr;
		};
		for (std::size_t i = 0; i < models.size(); ++i) models[i] = static_cast<Game::XModel*>(loaded(Game::ASSET_TYPE_XMODEL, modelNames[i]));
		for (std::size_t i = 0; i < weapons.size(); ++i) weapons[i] = static_cast<Game::XModel*>(loaded(Game::ASSET_TYPE_XMODEL, weaponNames[i]));
		zombieHead = static_cast<Game::XModel*>(loaded(Game::ASSET_TYPE_XMODEL, "c_t5_ger_zombie_head1"));
		for (std::size_t kind = 0; kind < clips.size(); ++kind)
			for (std::size_t i = 0; i < ClipCount; ++i)
			{
				const auto* name = kind ? zombieClips[i] : playerClips[i];
				clips[kind][i] = name ? static_cast<Game::XAnimParts*>(loaded(Game::ASSET_TYPE_XANIMPARTS, name)) : nullptr;
			}
		prepared = std::ranges::all_of(models, [](const auto* model) { return model && model->numLods; }) && clips[0][Idle] && clips[1][Idle];
		Logger::Print("menu actors: native Primis/zombie models {} (using zw3.ff and zw3_common.ff)\n", prepared ? "ready" : "unavailable");
	}

	void Actors::Reset()
	{
		if (std::ranges::any_of(entities, [](const NativeActor& actor) { return actor.created; })) R_SyncRenderThread();
		for (auto& actor : entities) actor.Release();
		survivors = {};
		zombies = {};
		currentMap.clear();
		prepared = ready = false;
		nextSpawn = spawnSequence = 0;
		std::lock_guard lock(snapshotMutex);
		published.clear();
	}

	void Actors::DrawScene(const BackdropData::spawn& anchor, std::string_view map, float dt)
	{
		if (map != currentMap)
		{
			Reset();
			standing = anchor;
			Ground(standing.origin);
			currentMap = map;
			nextSpawn = Now() + 1200;
		}
		Prepare();
		if (!prepared) return;
		std::array<State, survivorCount + zombieCount> states{};
		unsigned int now = Now();
		bool received = false;
		if (Party::IsLobbySceneClient())
		{
			std::lock_guard lock(snapshotMutex);
			if (remote && remote->map == currentMap && now - receivedAt < 2000)
			{
				received = true;
				const float blend = std::clamp(static_cast<float>(now - receivedAt) / 100.0f, 0.0f, 1.0f);
				now += static_cast<unsigned int>(clockOffset.load());
				for (std::size_t i = 0; i < states.size(); ++i)
				{
					states[i] = i < survivorCount ? remote->survivors[i] : remote->zombies[i - survivorCount];
					if (!previousRemote || previousRemote->map != remote->map) continue;
					const auto& before = i < survivorCount ? previousRemote->survivors[i] : previousRemote->zombies[i - survivorCount];
					if (!before.visible || before.dead || states[i].dead || before.model != states[i].model || Distance(before.origin, states[i].origin) > 64) continue;
					for (std::size_t axis = 0; axis < 3; ++axis) states[i].origin[axis] = std::lerp(before.origin[axis], states[i].origin[axis], blend);
					states[i].yaw = Turn(before.yaw, states[i].yaw, blend);
				}
			}
		}
		else ClearRemoteScene();
		if (!received)
		{
			Simulate(now, std::clamp(dt, 0.0f, 0.1f));
			Publish(now);
			for (std::size_t i = 0; i < survivorCount; ++i) states[i] = survivors[i].state;
			for (std::size_t i = 0; i < zombieCount; ++i) states[i + survivorCount] = zombies[i].state;
		}
		bool changing = false;
		for (std::size_t i = 0; i < states.size(); ++i)
			if (entities[i].created && (!states[i].visible || entities[i].model != states[i].model || entities[i].weapon != states[i].weapon)) changing = true;
		if (changing) R_SyncRenderThread();
		for (std::size_t i = 0; i < states.size(); ++i)
		{
			auto& actor = entities[i];
			const auto& state = states[i];
			if (actor.created && (!state.visible || actor.model != state.model || actor.weapon != state.weapon)) actor.Release();
			if (state.visible && !actor.created) actor.Create(state.model, state.weapon, static_cast<unsigned int>(i) + 32);
		}
		ready = false;
		for (std::size_t i = 0; i < states.size(); ++i)
			if (states[i].visible && entities[i].created)
			{
				Draw(entities[i], states[i], static_cast<unsigned int>(i) + 32, now, dt);
				ready = true;
			}
	}

	std::string Actors::GetSceneSnapshot()
	{
		std::lock_guard lock(snapshotMutex);
		return published;
	}
	void Actors::ReceiveSceneSnapshot(const std::string& bytes, bool immediate)
	{
		if (bytes.empty() || bytes.size() > 8192) return;
		std::array<Bytef, 65536> unpacked{};
		uLongf length = static_cast<uLongf>(unpacked.size());
		if (uncompress(unpacked.data(), &length, reinterpret_cast<const Bytef*>(bytes.data()), static_cast<uLong>(bytes.size())) != Z_OK) return;
		try
		{
			auto packet = nlohmann::json::from_msgpack(unpacked.data(), unpacked.data() + length).get<Packet>();
			if (packet.version != 2 || !BackdropData::find_stage(packet.map)
				|| !std::ranges::all_of(packet.survivors, Valid) || !std::ranges::all_of(packet.zombies, Valid)) return;
			std::lock_guard lock(snapshotMutex);
			if (remote && remote->map == packet.map && !Due(packet.time, remote->time)) return;
			previousRemote = remote;
			remote = std::move(packet);
			receivedAt = Now();
			if (immediate) clockOffset = static_cast<int>(remote->time - receivedAt);
		}
		catch (const nlohmann::json::exception&) {}
	}
	void Actors::PollLocalScene() { /* Party transports native scene snapshots. */ }
	void Actors::ClearRemoteScene()
	{
		std::lock_guard lock(snapshotMutex);
		remote.reset();
		previousRemote.reset();
	}
	void Actors::ReceiveSceneClock(unsigned int sent, unsigned int hostTime)
	{
		const auto now = Now();
		const auto roundTrip = now - sent;
		if (roundTrip < 1000) clockOffset = static_cast<int>(hostTime + roundTrip / 2 - now);
	}
	Actors::Actors()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;
		Scheduler::OnShutdown([] { Reset(); });
	}
}
