#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace Components::FxSpotlightLifecycle
{
	template <typename T>
	T Read(const void* object, std::size_t offset)
	{
		T value;
		std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
		return value;
	}

	// IW4 owns one spotlight effect and one spotlight element, not an element list.
	// Repeating BO3 lights can spawn again while their preceding element is alive.
	// Retire that element through the engine before it overwrites the only handle.
	template <typename Release>
	bool Prepare(void* system, void* effect, int index, Release&& release)
	{
		const auto* definition = Read<const void*>(effect, 0);
		const auto* name = Read<const char*>(definition, 0);
		if (!name || !std::string_view(name).starts_with("smc_t7/")) return true;
		const auto count = static_cast<std::int64_t>(Read<int>(definition, 20))
			+ Read<int>(definition, 24) + Read<int>(definition, 28);
		if (index < 0 || index >= count) return false;
		const auto* elements = Read<const std::byte*>(definition, 32);
		if (!elements) return false;
		if (Read<std::uint8_t>(elements + index * 288, 176) != 9) return true;

		const auto active = Read<int>(system, 0xA30);
		if (active == 0) return true;
		const auto* effects = Read<const std::byte*>(system, 0x180);
		const auto owner = Read<std::uint16_t>(system, 0xA34);
		// Another effect must not steal the singleton or release its owner's refs.
		if (active != 1 || effects + owner * 4 != effect) return false;
		release(system, Read<std::uint16_t>(system, 0xA36), effect);
		return true;
	}
}
