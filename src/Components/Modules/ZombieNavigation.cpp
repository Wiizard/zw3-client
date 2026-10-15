#include "STDInclude.hpp"

#include <charconv>
#include <numbers>

#include "ZombieNavigation.hpp"
#include "ZombieNavigationGraph.hpp"
#include "BotAI/Iw4.hpp"
#include "BotAI/Navgen/Internal.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"

namespace Components::ZombieNavigation
{
	namespace
	{
		Graph graph;
		std::vector<Node> authoredNodes;
		std::vector<int> spawnEntries;
		std::vector<int> playableNodes;
		bool walkGraphReady = false;
		std::vector<Barrier> pendingBarriers;
		struct DebrisGroup
		{
			Position origin;
			unsigned short model;
			std::vector<int> blocks;
		};
		std::unordered_map<int, DebrisGroup> debrisGroups;
		int lastBarrierSync = -1000;
		int spawnStart = -1;
		unsigned int spawnRevision = 0;
		std::vector<int> spawnReachable;
		Position anchorOrigin{};
		unsigned int anchorRevision = std::numeric_limits<unsigned int>::max();
		int anchorTime = -1000;
		int anchorNode = -1;

		struct CrewRoute
		{
			std::vector<int> path;
			std::vector<int> visited;
			std::size_t next = 0;
			int goal = -1;
			int avoidGoal = -1;
			int avoidUntil = 0;
			int avoidFrom = -1;
			int avoidTo = -1;
			int until = 0;
			int pauseUntil = 0;
			int retryAt = 0;
			int updated = 0;
			int progressAt = 0;
			int steeredAt = 0;
			unsigned int revision = 0;
			Position progress{};
			float yaw = 0.0f;
			float turn = 0.0f;
		};

		std::array<CrewRoute, Game::MAX_CLIENTS> crewRoutes;
		unsigned int randomState = 0x6D2B79F5u;
		constexpr float roamHumanRadius = 1200.0f;
		constexpr float regroupHumanRadius = 1600.0f;

		int Now()
		{
			return *Game::svs_time;
		}

		unsigned int Random(const unsigned int bound)
		{
			randomState ^= randomState << 13;
			randomState ^= randomState >> 17;
			randomState ^= randomState << 5;
			return randomState % bound;
		}

		Position Vector(const unsigned int argument)
		{
			Position value{};
			Game::Scr_GetVector(argument, value.data());
			return value;
		}

		bool Finite(const Position& value)
		{
			return std::ranges::all_of(value, [](const float number) { return std::isfinite(number); });
		}

		Position Origin(const Game::gentity_s* entity)
		{
			const auto* origin = entity->client ? entity->client->ps.origin : entity->r.currentOrigin;
			return { origin[0], origin[1], origin[2] };
		}

		void Reset()
		{
			graph = Graph{};
			authoredNodes.clear();
			spawnEntries.clear();
			playableNodes.clear();
			walkGraphReady = false;
			pendingBarriers.clear();
			debrisGroups.clear();
			lastBarrierSync = -1000;
			spawnStart = -1;
			spawnReachable.clear();
			anchorRevision = std::numeric_limits<unsigned int>::max();
			anchorTime = -1000;
			anchorNode = -1;
			crewRoutes = {};
		}

		void Load(const std::string_view text)
		{
			Reset();
			std::size_t cursor = 0;
			while (cursor < text.size())
			{
				const auto end = text.find(';', cursor);
				const auto record = text.substr(cursor, end == text.npos ? text.size() - cursor : end - cursor);
				std::vector<int> fields;
				std::size_t field = 0;
				while (field < record.size())
				{
					while (field < record.size() && std::isspace(static_cast<unsigned char>(record[field])))
					{
						++field;
					}
					double value = 0.0;
					const auto result = std::from_chars(record.data() + field, record.data() + record.size(), value);
					if (result.ec != std::errc{} || !std::isfinite(value)
						|| value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
					{
						fields.clear();
						break;
					}
					fields.push_back(static_cast<int>(value));
					field = static_cast<std::size_t>(result.ptr - record.data());
					while (field < record.size() && std::isspace(static_cast<unsigned char>(record[field])))
					{
						++field;
					}
					if (field < record.size() && record[field++] != ',')
					{
						fields.clear();
						break;
					}
				}
				if (fields.size() >= 4 && fields[0] >= 0 && fields[0] < Graph::maxNodes)
				{
					const int index = fields[0];
					if (graph.nodes.size() <= static_cast<std::size_t>(index))
					{
						graph.nodes.resize(index + 1);
					}
					Node node;
					node.valid = true;
					node.origin = { static_cast<float>(fields[1]), static_cast<float>(fields[2]), static_cast<float>(fields[3]) };
					for (std::size_t child = 4; child < fields.size(); ++child)
					{
						if (fields[child] >= 0 && fields[child] < Graph::maxNodes && fields[child] != index)
						{
							node.children.push_back(fields[child]);
						}
					}
					graph.nodes[index] = std::move(node);
				}
				if (end == text.npos)
				{
					break;
				}
				cursor = end + 1;
			}
			graph.Invalidate();
			authoredNodes = graph.nodes;
		}

		void AddLink(const int from, const int to)
		{
			if (from < 0 || to < 0 || from >= static_cast<int>(authoredNodes.size()) || to >= static_cast<int>(authoredNodes.size())
				|| !authoredNodes[from].valid || !authoredNodes[to].valid)
			{
				return;
			}
			auto& children = authoredNodes[from].children;
			if (std::ranges::find(children, to) == children.end())
			{
				children.push_back(to);
				graph.Invalidate();
				walkGraphReady = false;
				lastBarrierSync = -1000;
			}
		}

		void BuildWalkGraph()
		{
			if (walkGraphReady || authoredNodes.empty() || !BotAI::SV_Trace) return;
			// Trace the permanent layout once with purchase barriers temporarily
			// excluded. No script or server frame runs while these traces execute.
			struct RestoreContents
			{
				std::vector<std::pair<int*, int>> values;
				~RestoreContents() { for (const auto& [address, value] : values) *address = value; }
			} restore;
			for (const auto& barrier : graph.barriers)
			{
				auto* contents = reinterpret_cast<int*>(reinterpret_cast<char*>(&Game::g_entities[barrier.entity]) + BotAI::gentityContents);
				restore.values.emplace_back(contents, *contents);
				*contents = 0;
			}
			const auto barriers = graph.barriers;
			graph.nodes = authoredNodes;
			graph.SetBarriers({});
			for (int index = 0; index < static_cast<int>(graph.nodes.size()); ++index)
			{
				auto& node = graph.nodes[index];
				if (!node.valid) continue;
				node.children.clear();
				for (const int child : authoredNodes[index].children)
				{
					if (graph.Valid(child) && BotAI::Navgen::CanWalk(node.origin.data(), authoredNodes[child].origin.data()))
					{
						node.children.push_back(child);
					}
				}
			}
			graph.Invalidate();
			int start = -1;
			std::vector<int> roots;
			for (int client = 0; client < *Game::svs_clientCount; ++client)
			{
				const auto* entity = Game::svs_clients[client].gentity;
				if (entity && entity->client && entity->r.isInUse && entity->health > 0
					&& entity->client->sess.sessionState == Game::SESS_STATE_PLAYING && !Game::svs_clients[client].bIsTestClient)
				{
					start = -1;
					const auto origin = Origin(entity);
					std::vector<std::pair<float, int>> candidates;
					for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node)
						if (graph.Valid(node)) candidates.emplace_back(DistanceSquared(origin, graph.nodes[node].origin), node);
					std::ranges::sort(candidates);
					for (const auto& [distance, node] : candidates)
					{
						if (distance > 1000.0f * 1000.0f) break;
						if (BotAI::Navgen::CanWalk(origin.data(), graph.nodes[node].origin.data())) { start = node; break; }
					}
					if (start < 0) start = graph.Nearest(origin);
					if (start >= 0 && std::ranges::find(roots, start) == roots.end()) roots.push_back(start);
				}
			}
			if (roots.empty())
			{
				graph.SetBarriers(barriers);
				return;
			}
			std::vector<int> playable(graph.nodes.size(), -1);
			for (const int root : roots)
			{
				const auto component = graph.Reachable(root);
				for (std::size_t node = 0; node < component.size(); ++node)
					if (component[node] >= 0) playable[node] = component[node];
			}
			playableNodes = playable;
			// Outside-window zombie nodes inherit the nearest playable entrance.
			// Exterior zombie links must not join rooms through unopened debris.
			std::vector<std::vector<int>> incoming(authoredNodes.size());
			for (int node = 0; node < static_cast<int>(authoredNodes.size()); ++node)
			{
				for (const int child : authoredNodes[node].children)
					if (graph.Valid(child)) incoming[child].push_back(node);
			}
			spawnEntries.assign(graph.nodes.size(), -1);
			std::vector<float> distance(graph.nodes.size(), std::numeric_limits<float>::max());
			using Entry = std::pair<float, int>;
			std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
			for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node)
			{
				if (playable[node] < 0) continue;
				spawnEntries[node] = node;
				distance[node] = 0.0f;
				queue.emplace(0.0f, node);
			}
			while (!queue.empty())
			{
				const auto [cost, node] = queue.top();
				queue.pop();
				if (cost > distance[node]) continue;
				for (const int child : incoming[node])
				{
					const float next = cost + std::sqrt(DistanceSquared(graph.nodes[node].origin, graph.nodes[child].origin));
					if (next >= distance[child]) continue;
					distance[child] = next;
					spawnEntries[child] = spawnEntries[node];
					queue.emplace(next, child);
				}
			}
			walkGraphReady = true;
			graph.SetBarriers(barriers);
		}

		void AppendBarrier(const Game::scr_entref_t reference)
		{
			if (reference.classnum || reference.entnum >= Game::MAX_GENTITIES)
			{
				return;
			}
			const auto* entity = &Game::g_entities[reference.entnum];
			const auto* bytes = reinterpret_cast<const char*>(entity);
			const int contents = *reinterpret_cast<const int*>(bytes + BotAI::gentityContents);
			if (!entity->r.isInUse || !entity->r.isLinked || !(contents & BotAI::maskPlayerSolidNoBodies))
			{
				return;
			}
			const auto* bounds = reinterpret_cast<const Game::Bounds*>(bytes + BotAI::gentityAbsMid);
			Barrier barrier{ reference.entnum, {}, {} };
			std::copy_n(entity->r.currentOrigin, 3, barrier.origin.begin());
			std::copy_n(bounds->midPoint, 3, barrier.middle.begin());
			std::copy_n(bounds->halfSize, 3, barrier.half.begin());
			const auto previous = std::ranges::find(graph.barriers, reference.entnum, &Barrier::entity);
			if (previous != graph.barriers.end() && previous->origin == barrier.origin)
			{
				// Decorative yaw changes do not change which doorway was purchased.
				barrier.middle = previous->middle;
				barrier.half = previous->half;
			}
			if (Finite(barrier.middle) && Finite(barrier.half)
				&& std::ranges::all_of(barrier.half, [](const float extent) { return extent >= 0.0f; }))
			{
				pendingBarriers.push_back(barrier);
			}
		}

		void AddBarrier(const Game::scr_entref_t reference)
		{
			if (reference.classnum || reference.entnum >= Game::MAX_GENTITIES) return;
			const auto* entity = &Game::g_entities[reference.entnum];
			if (const auto group = debrisGroups.find(reference.entnum); group != debrisGroups.end())
			{
				if (entity->r.isInUse && entity->model == group->second.model && Origin(entity) == group->second.origin)
				{
					for (const int block : group->second.blocks)
					{
						Game::scr_entref_t child{};
						child.entnum = static_cast<unsigned short>(block);
						AppendBarrier(child);
					}
					return;
				}
				debrisGroups.erase(group);
			}
			AppendBarrier(reference);
		}

		int Anchor(const Position& origin)
		{
			if (origin == anchorOrigin && graph.revision == anchorRevision && Now() - anchorTime < 200) return anchorNode;
			std::vector<std::pair<float, int>> candidates;
			for (int index = 0; index < static_cast<int>(graph.nodes.size()); ++index)
			{
				if (!graph.Valid(index) || (walkGraphReady && playableNodes[index] < 0))
				{
					continue;
				}
				const auto& point = graph.nodes[index].origin;
				const float height = origin[2] - point[2];
				const float score = DistanceSquared(origin, point) + 3.0f * height * height;
				if (score <= 1000.0f * 1000.0f) candidates.emplace_back(score, index);
			}
			std::ranges::sort(candidates);
			anchorNode = -1;
			for (const auto& [distance, index] : candidates)
			{
				(void)distance;
				const auto& point = graph.nodes[index].origin;
				if (graph.SegmentOpen(origin, point, 0.0f) && BotAI::Navgen::CanWalk(origin.data(), point.data()))
				{
					anchorNode = index;
					break;
				}
			}
			anchorOrigin = origin;
			anchorRevision = graph.revision;
			anchorTime = Now();
			return anchorNode;
		}

		bool CanSpawn(const Position& origin, const Position& victim)
		{
			if (!Finite(origin) || !Finite(victim))
			{
				return false;
			}
			if (graph.barriers.empty())
			{
				return true;
			}
			if (graph.nodes.empty())
			{
				return graph.SegmentOpen(victim, origin);
			}
			const int nearest = graph.Nearest(origin);
			const int destination = nearest >= 0 && nearest < static_cast<int>(spawnEntries.size()) ? spawnEntries[nearest] : nearest;
			const int start = Anchor(victim);
			if (!graph.Valid(start) || !graph.Valid(destination))
			{
				return false;
			}
			if (start != spawnStart || graph.revision != spawnRevision)
			{
				spawnReachable = graph.Reachable(start);
				spawnStart = start;
				spawnRevision = graph.revision;
			}
			return spawnReachable[destination] >= 0;
		}

		float HumanDistance(const Position& point, const Game::gentity_s* self)
		{
			float result = std::numeric_limits<float>::max();
			for (int client = 0; client < *Game::svs_clientCount; ++client)
			{
				const auto* other = Game::svs_clients[client].gentity;
				if (!other || !other->r.isInUse || !other->client || other->health <= 0 || Game::svs_clients[client].bIsTestClient
					|| other->client->sess.sessionState != Game::SESS_STATE_PLAYING
					|| other->client->sess.cs.team != self->client->sess.cs.team) continue;
				result = std::min(result, DistanceSquared(point, Origin(other)));
			}
			return result;
		}

		Game::gentity_s* CrewEntity(const Game::scr_entref_t reference)
		{
			if (reference.classnum || reference.entnum >= Game::MAX_CLIENTS)
			{
				return nullptr;
			}
			auto* entity = &Game::g_entities[reference.entnum];
			return entity->r.isInUse && entity->client && Game::SV_IsTestClient(reference.entnum) ? entity : nullptr;
		}

		void Remember(CrewRoute& route, const int node)
		{
			if (route.visited.empty() || route.visited.back() != node)
			{
				route.visited.push_back(node);
				if (route.visited.size() > 24)
				{
					route.visited.erase(route.visited.begin());
				}
			}
		}

		bool PickRoute(Game::gentity_s* entity, CrewRoute& route, const int now)
		{
			const Position origin = Origin(entity);
			const float humanDistance = HumanDistance(origin, entity);
			const bool regrouping = humanDistance < std::numeric_limits<float>::max() && humanDistance > regroupHumanRadius * regroupHumanRadius;
			const int start = Anchor(origin);
			if (!graph.Valid(start))
			{
				return false;
			}
			std::vector<float> distance;
			std::vector<int> previous;
			std::vector<bool> allowed(graph.nodes.size(), true);
			if (!regrouping)
			{
				for (int node = 0; node < static_cast<int>(graph.nodes.size()); ++node)
				{
					const float separation = HumanDistance(graph.nodes[node].origin, entity);
					allowed[node] = separation == std::numeric_limits<float>::max() || separation <= regroupHumanRadius * regroupHumanRadius;
				}
			}
			graph.Routes(start, distance, previous, &allowed,
				now < route.avoidUntil ? route.avoidFrom : -1, now < route.avoidUntil ? route.avoidTo : -1);
			int goal = -1;
			float bestScore = -std::numeric_limits<float>::max();
			for (int index = 0; index < static_cast<int>(graph.nodes.size()); ++index)
			{
				if (index == start || previous[index] < 0 || distance[index] > (regrouping ? 6000.0f : 2400.0f)
					|| (index == route.avoidGoal && now < route.avoidUntil))
				{
					continue;
				}
				const auto& point = graph.nodes[index].origin;
				const float separation = DistanceSquared(origin, point);
				const float nearHuman = HumanDistance(point, entity);
				if (separation < 120.0f * 120.0f || (!regrouping && separation > 1600.0f * 1600.0f)
					|| (nearHuman < std::numeric_limits<float>::max() && nearHuman > roamHumanRadius * roamHumanRadius))
				{
					continue;
				}
				float score = static_cast<float>(Random(300)) + std::min(distance[index], 1000.0f) * 0.25f;
				if (regrouping) score -= std::sqrt(nearHuman);
				if (std::ranges::find(route.visited, index) != route.visited.end())
				{
					score -= 700.0f;
				}
				for (int client = 0; client < *Game::svs_clientCount; ++client)
				{
					const auto* other = Game::svs_clients[client].gentity;
					if (!other || other == entity || !other->r.isInUse || !other->client || other->health <= 0)
					{
						continue;
					}
					if (DistanceSquared(point, Origin(other)) < 300.0f * 300.0f)
					{
						score -= 800.0f;
					}
					const auto& otherRoute = crewRoutes[client];
					if (graph.Valid(otherRoute.goal) && now - otherRoute.updated < 600
						&& DistanceSquared(point, graph.nodes[otherRoute.goal].origin) < 350.0f * 350.0f)
					{
						score -= 1200.0f;
					}
				}
				if (score > bestScore)
				{
					goal = index;
					bestScore = score;
				}
			}
			if (goal < 0)
			{
				return false;
			}
			std::vector<int> path;
			for (int node = goal; node != start; node = previous[node])
			{
				if (!graph.Valid(node) || previous[node] < 0 || path.size() >= graph.nodes.size())
				{
					return false;
				}
				path.push_back(node);
			}
			path.push_back(start);
			std::ranges::reverse(path);
			route.path = std::move(path);
			route.next = 0;
			route.goal = goal;
			route.revision = graph.revision;
			route.until = now + 16000;
			route.progress = origin;
			route.progressAt = now;
			route.retryAt = 0;
			return true;
		}

		int UpdateRoute(Game::gentity_s* entity)
		{
			auto& route = crewRoutes[entity->s.number];
			const int now = Now();
			const Position origin = Origin(entity);
			const bool interrupted = route.updated && now - route.updated > 600;
			route.updated = now;
			if (route.goal >= 0)
			{
				const bool reached = graph.Valid(route.goal) && DistanceSquared(origin, graph.nodes[route.goal].origin) < 72.0f * 72.0f;
				if (DistanceSquared(origin, route.progress) > 24.0f * 24.0f || now < route.pauseUntil)
				{
					route.progress = origin;
					route.progressAt = now;
				}
				const bool stuck = !interrupted && now - route.progressAt >= 2200;
				const float goalHumanDistance = graph.Valid(route.goal) ? HumanDistance(graph.nodes[route.goal].origin, entity) : 0.0f;
				const bool outsideLeash = goalHumanDistance < std::numeric_limits<float>::max() && goalHumanDistance > regroupHumanRadius * regroupHumanRadius;
				if (reached || stuck || interrupted || outsideLeash || route.revision != graph.revision || now >= route.until)
				{
					Remember(route, route.goal);
					if (stuck)
					{
						route.avoidGoal = route.goal;
						route.avoidUntil = now + 10000;
						if (route.next > 0 && route.next < route.path.size())
						{
							route.avoidFrom = route.path[route.next - 1];
							route.avoidTo = route.path[route.next];
						}
					}
					if (reached)
					{
						route.pauseUntil = now + 300 + static_cast<int>(Random(700));
					}
					if (interrupted)
					{
						route.pauseUntil = 0;
					}
					route.goal = -1;
					route.path.clear();
				}
			}
			if (route.goal < 0)
			{
				if (now < route.retryAt || !PickRoute(entity, route, now))
				{
					if (now >= route.retryAt)
					{
						route.retryAt = now + 1000;
					}
					return -1;
				}
			}
			while (route.next + 1 < route.path.size()
				&& DistanceSquared(origin, graph.nodes[route.path[route.next]].origin) < 56.0f * 56.0f)
			{
				Remember(route, route.path[route.next]);
				++route.next;
			}
			return route.goal;
		}

		float AngleDelta(float angle)
		{
			return std::remainder(angle, 360.0f);
		}
	}

	void RegisterScriptFunctions()
	{
		using GSC::Script;
		Script::AddFunction("ZW3NavLoad", [] { Load(Game::Scr_GetString(0)); });
		Script::AddFunction("ZW3NavLink", []
		{
			const int from = Game::Scr_GetInt(0);
			const int to = Game::Scr_GetInt(1);
			AddLink(from, to);
			AddLink(to, from);
		});
		Script::AddFunction("ZW3NavChild", [] { AddLink(Game::Scr_GetInt(0), Game::Scr_GetInt(1)); });
		Script::AddFunction("ZW3NavNode", []
		{
			const int index = Game::Scr_GetInt(0);
			const auto origin = Vector(1);
			if (index < 0 || index >= Graph::maxNodes || !Finite(origin)) return;
			if (authoredNodes.size() <= static_cast<std::size_t>(index)) authoredNodes.resize(index + 1);
			authoredNodes[index].origin = origin;
			authoredNodes[index].valid = true;
			walkGraphReady = false;
			lastBarrierSync = -1000;
			graph.Invalidate();
		});
		Script::AddFunction("ZW3NavNeedsBarrierSync", [] { Game::Scr_AddBool(Now() - lastBarrierSync >= 200); });
		Script::AddFunction("ZW3NavBeginBarriers", [] { pendingBarriers.clear(); });
		Script::AddMethod("ZW3NavBarrier", AddBarrier);
		Script::AddMethod("ZW3NavDebrisBlock", [](const Game::scr_entref_t reference)
		{
			if (reference.classnum || reference.entnum >= Game::MAX_GENTITIES) return;
			const int block = Game::Scr_GetInt(0);
			if (block < 0 || block >= Game::MAX_GENTITIES) return;
			const auto* entity = &Game::g_entities[reference.entnum];
			auto& group = debrisGroups[reference.entnum];
			if (block == reference.entnum) group = { Origin(entity), entity->model, {} };
			group.blocks.push_back(block);
		});
		Script::AddFunction("ZW3NavEndBarriers", []
		{
			std::ranges::sort(pendingBarriers, {}, &Barrier::entity);
			const auto duplicate = std::ranges::unique(pendingBarriers, {}, &Barrier::entity);
			pendingBarriers.erase(duplicate.begin(), duplicate.end());
			if (walkGraphReady && std::ranges::any_of(pendingBarriers, [](const Barrier& barrier)
				{ return std::ranges::find(graph.barriers, barrier.entity, &Barrier::entity) == graph.barriers.end(); }))
			{
				walkGraphReady = false;
			}
			graph.SetBarriers(std::move(pendingBarriers));
			BuildWalkGraph();
			pendingBarriers.clear();
			lastBarrierSync = Now();
		});
		Script::AddFunction("ZW3NavCanSpawn", [] { Game::Scr_AddBool(CanSpawn(Vector(0), Vector(1))); });
		Script::AddFunction("ZW3NavSpawnAllowed", []
		{
			const Position origin = Vector(0);
			for (int client = 0; client < *Game::svs_clientCount; ++client)
			{
				const auto* entity = Game::svs_clients[client].gentity;
				if (entity && entity->r.isInUse && entity->client && entity->health > 0
					&& entity->client->sess.sessionState == Game::SESS_STATE_PLAYING && CanSpawn(origin, Origin(entity)))
				{
					Game::Scr_AddBool(true);
					return;
				}
			}
			Game::Scr_AddBool(false);
		});
		Script::AddMethod("ZW3CrewRoamGoal", [](const Game::scr_entref_t reference)
		{
			auto* entity = CrewEntity(reference);
			Game::Scr_AddInt(entity ? UpdateRoute(entity) : -1);
		});
		Script::AddMethod("ZW3CrewCanReach", [](const Game::scr_entref_t reference)
		{
			const auto* entity = CrewEntity(reference);
			const Position destination = Vector(0);
			const float separation = entity ? HumanDistance(destination, entity) : 0.0f;
			Game::Scr_AddBool(entity && (separation == std::numeric_limits<float>::max() || separation <= regroupHumanRadius * regroupHumanRadius)
				&& CanSpawn(destination, Origin(entity)));
		});
		Script::AddMethod("ZW3CrewRoamYaw", [](const Game::scr_entref_t reference)
		{
			const auto* entity = CrewEntity(reference);
			if (!entity)
			{
				Game::Scr_AddFloat(0.0f);
				return;
			}
			auto& route = crewRoutes[reference.entnum];
			float yaw = entity->client->ps.viewangles[1];
			if (route.next < route.path.size() && graph.Valid(route.path[route.next]))
			{
				const auto& target = graph.nodes[route.path[route.next]].origin;
				const auto origin = Origin(entity);
				const float desired = std::atan2(target[1] - origin[1], target[0] - origin[0]) * (180.0f / std::numbers::pi_v<float>);
				const float seconds = std::clamp((Now() - route.steeredAt) * 0.001f, 0.05f, 0.25f);
				yaw += std::clamp(AngleDelta(desired - yaw), -240.0f * seconds, 240.0f * seconds);
				route.turn = std::abs(AngleDelta(desired - yaw));
			}
			route.yaw = yaw;
			route.steeredAt = Now();
			Game::Scr_AddFloat(yaw);
		});
		Script::AddMethod("ZW3CrewRoamSpeed", [](const Game::scr_entref_t reference)
		{
			const auto* entity = CrewEntity(reference);
			int speed = 0;
			if (entity)
			{
				const auto& route = crewRoutes[reference.entnum];
				if (route.goal >= 0 && Now() >= route.pauseUntil && route.turn < 75.0f)
				{
					speed = route.turn > 25.0f ? 45 : 90;
				}
			}
			Game::Scr_AddInt(speed);
		});
		Events::OnVMShutdown(Reset);
	}
}
