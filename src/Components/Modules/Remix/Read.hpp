#pragma once

#include <cstdint>
#include <cstring>

namespace Components::Remix
{
	template <typename T>
	T ReadAt(std::uintptr_t address)
	{
		T result;
		std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(result));
		return result;
	}
}
