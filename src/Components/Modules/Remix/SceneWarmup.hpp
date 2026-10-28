#pragma once

#include <algorithm>
#include <cstdint>

namespace Components::Remix
{
	class SceneWarmup
	{
	public:
		void Reset() { *this = {}; }

		void ObserveScene(const std::uintptr_t identity, const std::uint64_t now)
		{
			if (!identity) return;
			if (identity != world)
			{
				world = identity;
				started = now;
				frames = 0;
				revealing = false;
			}
			++frames;
			if (!revealing && frames >= 8 && now - started >= 1000)
			{
				revealing = true;
				revealStart = now;
			}
		}

		float Opacity(const std::uint64_t now) const
		{
			if (!world) return 0.0f;
			if (!revealing) return 1.0f;
			return 1.0f - std::clamp(static_cast<float>(now - revealStart) / 250.0f, 0.0f, 1.0f);
		}

	private:
		std::uintptr_t world = 0;
		std::uint64_t started = 0;
		std::uint64_t revealStart = 0;
		unsigned frames = 0;
		bool revealing = false;
	};
}
