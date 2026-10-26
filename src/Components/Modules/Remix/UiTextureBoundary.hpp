#pragma once

#include <cstdint>
#include <unordered_set>

namespace Components::Remix
{
	class UiTextureBoundary
	{
		using Hashes = std::unordered_set<std::uint64_t>;
		bool initialized = false;
		bool suppressed = false;
		Hashes previousSaved;
		Hashes previousActive;
		Hashes previousWithdrawn;

	public:
		void Invalidate() { initialized = false; }

		template <typename Writer>
		void Update(bool suppress, const Hashes& saved, const Hashes& active, const Hashes& withdrawn, Writer write)
		{
			if (initialized && suppressed == suppress && previousSaved == saved && previousActive == active
				&& previousWithdrawn == withdrawn)
			{
				return;
			}

			Hashes known = saved;
			known.insert(active.begin(), active.end());
			known.insert(withdrawn.begin(), withdrawn.end());

			for (const std::uint64_t hash : known)
			{
				const bool desired = active.contains(hash) || (saved.contains(hash) && !withdrawn.contains(hash));
				// Mutate each opinion explicitly: parsing a negative config entry can retain a positive one.
				if (!write(hash, !suppress && desired))
				{
					return;
				}
			}

			previousSaved = saved;
			previousActive = active;
			previousWithdrawn = withdrawn;
			suppressed = suppress;
			initialized = true;
		}
	};
}
