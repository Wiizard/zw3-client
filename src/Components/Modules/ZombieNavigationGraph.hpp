#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <string_view>
#include <vector>

namespace Components::ZombieNavigation
{
	using Position = std::array<float, 3>;

	struct Barrier
	{
		int entity;
		Position middle;
		Position half;
		Position origin{};
		bool operator==(const Barrier&) const = default;
	};

	struct Node
	{
		Position origin{};
		std::vector<int> children;
		bool valid = false;
	};

	inline float DistanceSquared(const Position& a, const Position& b)
	{
		float result = 0.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			const float delta = a[axis] - b[axis];
			result += delta * delta;
		}
		return result;
	}

	class Graph
	{
	public:
		static constexpr int maxNodes = 16384;
		std::vector<Node> nodes;
		std::vector<Barrier> barriers;
		unsigned int revision = 0;

		bool Valid(const int index) const
		{
			return index >= 0 && index < static_cast<int>(nodes.size()) && nodes[index].valid;
		}

		// Only purchase barriers split the existing graph. Map-authored stair,
		// window and climb links remain available to their original users.
		bool SegmentOpen(const Position& from, const Position& to, const float padding = 8.0f) const
		{
			for (const auto& barrier : barriers)
			{
				float enter = 0.0f;
				float leave = 1.0f;
				bool intersects = true;
				for (int axis = 0; axis < 3; ++axis)
				{
					const float lift = axis == 2 ? 30.0f : 0.0f;
					const float start = from[axis] + lift;
					const float delta = to[axis] - from[axis];
					const float extent = barrier.half[axis] + (axis == 2 ? 0.0f : padding);
					const float low = barrier.middle[axis] - extent;
					const float high = barrier.middle[axis] + extent;
					if (std::abs(delta) < 0.001f)
					{
						if (start < low || start > high)
						{
							intersects = false;
							break;
						}
						continue;
					}
					float a = (low - start) / delta;
					float b = (high - start) / delta;
					if (a > b)
					{
						std::swap(a, b);
					}
					enter = std::max(enter, a);
					leave = std::min(leave, b);
					if (enter > leave)
					{
						intersects = false;
						break;
					}
				}
				if (intersects)
				{
					return false;
				}
			}
			return true;
		}

		int Nearest(const Position& origin) const
		{
			int best = -1;
			float bestDistance = std::numeric_limits<float>::max();
			for (int index = 0; index < static_cast<int>(nodes.size()); ++index)
			{
				if (!nodes[index].valid)
				{
					continue;
				}
				const auto& point = nodes[index].origin;
				const float height = point[2] - origin[2];
				const float distance = DistanceSquared(point, origin) + 3.0f * height * height;
				if (distance < bestDistance)
				{
					best = index;
					bestDistance = distance;
				}
			}
			return best;
		}

		void SetBarriers(std::vector<Barrier> value)
		{
			if (barriers != value)
			{
				barriers = std::move(value);
				Invalidate();
			}
		}

		void Invalidate()
		{
			++revision;
			openLinks.clear();
		}

		bool LinkOpen(const int from, const int to)
		{
			if (!Valid(from) || !Valid(to))
			{
				return false;
			}
			if (openLinks.size() != nodes.size())
			{
				openLinks.resize(nodes.size());
			}
			auto& cached = openLinks[from];
			if (cached.empty())
			{
				cached.reserve(nodes[from].children.size());
				for (const int child : nodes[from].children)
				{
					cached.emplace_back(child, Valid(child) && SegmentOpen(nodes[from].origin, nodes[child].origin));
				}
			}
			const auto found = std::ranges::find(cached, to, &std::pair<int, bool>::first);
			return found != cached.end() && found->second;
		}

		std::vector<int> Reachable(const int start)
		{
			std::vector<int> result(nodes.size(), -1);
			if (!Valid(start))
			{
				return result;
			}
			std::vector<int> queue{ start };
			result[start] = start;
			for (std::size_t head = 0; head < queue.size(); ++head)
			{
				const int node = queue[head];
				for (const int child : nodes[node].children)
				{
					if (Valid(child) && result[child] < 0 && LinkOpen(node, child))
					{
						result[child] = node;
						queue.push_back(child);
					}
				}
			}
			return result;
		}

		void Routes(const int start, std::vector<float>& distance, std::vector<int>& previous,
			const std::vector<bool>* allowed = nullptr, const int avoidFrom = -1, const int avoidTo = -1)
		{
			distance.assign(nodes.size(), std::numeric_limits<float>::max());
			previous.assign(nodes.size(), -1);
			if (!Valid(start))
			{
				return;
			}
			using Entry = std::pair<float, int>;
			std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
			distance[start] = 0.0f;
			previous[start] = start;
			queue.emplace(0.0f, start);
			while (!queue.empty())
			{
				const auto [cost, node] = queue.top();
				queue.pop();
				if (cost > distance[node])
				{
					continue;
				}
				for (const int child : nodes[node].children)
				{
					if (!LinkOpen(node, child) || (allowed && !(*allowed)[child])
						|| (node == avoidFrom && child == avoidTo) || (node == avoidTo && child == avoidFrom))
					{
						continue;
					}
					const float next = cost + std::sqrt(DistanceSquared(nodes[node].origin, nodes[child].origin));
					if (next < distance[child])
					{
						distance[child] = next;
						previous[child] = node;
						queue.emplace(next, child);
					}
				}
			}
		}

	private:
		std::vector<std::vector<std::pair<int, bool>>> openLinks;
	};
}
