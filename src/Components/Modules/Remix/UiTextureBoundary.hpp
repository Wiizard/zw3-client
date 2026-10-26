#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
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
			std::string opinions;

			for (const std::uint64_t hash : known)
			{
				const bool desired = active.contains(hash) || (saved.contains(hash) && !withdrawn.contains(hash));
				char text[32];
				// Negative opinions also mask tags inherited from a lower-priority runtime config layer.
				std::snprintf(text, sizeof(text), "%s0x%016llX", suppress || !desired ? "-" : "",
					static_cast<unsigned long long>(hash));
				if (!opinions.empty()) opinions += ", ";
				opinions += text;
			}

			if (!opinions.empty() && !write(opinions))
			{
				return;
			}

			previousSaved = saved;
			previousActive = active;
			previousWithdrawn = withdrawn;
			suppressed = suppress;
			initialized = true;
		}
	};
}
