#include "Components/Modules/FxSpotlightLifecycle.hpp"
#include <array>
#include <cassert>
#include <iostream>

namespace
{
	template <typename T, std::size_t N>
	void Put(std::array<std::byte, N>& data, std::size_t offset, T value)
	{
		std::memcpy(data.data() + offset, &value, sizeof(value));
	}

	struct Fixture
	{
		std::array<std::byte, 0xA50> system{};
		std::array<std::byte, 144> effect{};
		std::array<std::byte, 40> definition{};
		std::array<std::byte, 288> element{};
		int releases = 0;
		int refs = 2;
		bool slotAllocated = false;

		Fixture()
		{
			Put(effect, 0, definition.data());
			Put(definition, 0, "smc_t7/mp_zombie_castle/fx/regression");
			Put(definition, 20, 1);
			Put(definition, 32, element.data());
			Put(element, 176, std::uint8_t{9});
			Put(system, 0x180, effect.data());
			Put(system, 0xA36, std::uint16_t{0x54C});
		}

		bool Prepare(int index = 0)
		{
			return Components::FxSpotlightLifecycle::Prepare(system.data(), effect.data(), index,
				[this](void* s, unsigned int handle, void* owner)
				{
					assert(s == system.data() && owner == effect.data() && handle == 0x54C);
					assert(slotAllocated && refs == 3);
					slotAllocated = false;
					--refs;
					++releases;
					Put(system, 0xA30, 0);
				});
		}

		void Spawn()
		{
			assert(Prepare());
			assert(!slotAllocated && refs == 2);
			slotAllocated = true;
			++refs;
			Put(system, 0xA30, 1);
		}
	};
}

int main()
{
	Fixture looping;
	// Reproduce repeated births before expiry, including a large frame catch-up.
	for (int i = 0; i < 10000; ++i) looping.Spawn();
	assert(looping.releases == 9999 && looping.refs == 3 && looping.slotAllocated);
	assert(!looping.Prepare(-1) && !looping.Prepare(1));
	assert(looping.releases == 9999);

	Fixture foreign;
	foreign.Spawn();
	Put(foreign.system, 0xA34, std::uint16_t{36});
	assert(!foreign.Prepare() && foreign.releases == 0 && foreign.refs == 3);

	// The captured crash has count=2 and a handle already on the free list.
	// It must not be passed to the native release routine a second time.
	Fixture corrupt;
	Put(corrupt.system, 0xA30, 2);
	assert(!corrupt.Prepare() && corrupt.releases == 0);

	Fixture pointLight;
	pointLight.Spawn();
	Put(pointLight.element, 176, std::uint8_t{8});
	assert(pointLight.Prepare() && pointLight.releases == 0);

	Fixture stock;
	stock.Spawn();
	Put(stock.definition, 0, "stock/spotlight");
	assert(stock.Prepare() && stock.releases == 0);
	std::cout << "BO3 spotlight lifecycle regression tests passed\n";
}
