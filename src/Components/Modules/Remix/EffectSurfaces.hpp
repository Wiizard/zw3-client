#pragma once

#include <array>
#include <vector>

namespace Components::Remix
{
	enum class ScriptEffectKind
	{
		None,
		Particle,
		Eye,
	};

	// Access is synchronized by the caller along with the effect-definition cache.
	class EffectSurfaces
	{
	public:
		void BeginFrame()
		{
			current = 1 - current;
			frames[current].clear();
		}

		void Add(const void* indices, ScriptEffectKind kind)
		{
			frames[current].push_back({ indices, kind });
		}

		ScriptEffectKind Get(const void* indices) const
		{
			// Prefer this frame when the engine reuses index-buffer addresses.
			for (const auto frame : { current, 1 - current })
			{
				for (auto entry = frames[frame].rbegin(); entry != frames[frame].rend(); ++entry)
				{
					if (entry->indices == indices)
					{
						return entry->kind;
					}
				}
			}

			return ScriptEffectKind::None;
		}

	private:
		struct Entry
		{
			const void* indices;
			ScriptEffectKind kind;
		};
		std::array<std::vector<Entry>, 2> frames;
		unsigned current = 0;
	};
}
