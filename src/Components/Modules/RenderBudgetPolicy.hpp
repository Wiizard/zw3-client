#pragma once

#include <cstddef>
#include <cstdint>

namespace Components::RenderBudgetPolicy
{
	// Handles and world model indices remain 16-bit; zero is the invalid handle.
	constexpr std::size_t handleEntries = 65536;
	constexpr std::size_t freeHandles = 0;
	constexpr std::size_t modelBits = handleEntries * sizeof(std::uint16_t);
	constexpr std::size_t modelIds = modelBits + handleEntries / 8;
	constexpr std::size_t frameStamps = modelIds + handleEntries * sizeof(std::uint16_t);
	constexpr std::size_t cacheBytes = frameStamps + handleEntries * sizeof(std::uint32_t);
	constexpr std::uint32_t minimumStaticLighting = 8192;
	// Keep native front/back buffer spacing; each relocated list fits one stride.
	constexpr std::size_t backEndDataStride = 0x13D700;
	constexpr std::size_t lightingPatchStride = 52;
	constexpr std::uint32_t lightingPatchEntries = 24576;
	constexpr std::size_t lightingPatchStorageBytes = 2 * backEndDataStride;
	// Preserve the 16-bit material token: 12 record bits and 4 surface bits.
	// A fixed record stride removes the old 16 KiB variable-byte-offset ceiling.
	constexpr unsigned modelCommandSlotShift = 9;
	constexpr std::size_t modelCommandSlotBytes = 1u << modelCommandSlotShift;
	constexpr unsigned modelCommandSlots = 4096;
	constexpr std::size_t modelCommandPassBytes = modelCommandSlots * modelCommandSlotBytes;
	constexpr unsigned modelCommandPasses = 256;
	constexpr std::size_t modelCommandStorageBytes = modelCommandPasses * modelCommandPassBytes;
	constexpr std::uint32_t workerScale = 16;
	constexpr std::uint32_t worker9Bytes = 0x1800 * workerScale;
	constexpr std::uint32_t worker11Bytes = 0x2000 * workerScale;

	static_assert(modelBits == 0x20000 && modelIds == 0x22000 && frameStamps == 0x42000);
	static_assert(cacheBytes == 0x82000);
	static_assert(lightingPatchEntries * lightingPatchStride <= backEndDataStride);
	static_assert(lightingPatchEntries >= 16384); // Full tested GPU atlas, including dynamic slots.
	static_assert(worker9Bytes / 24 == 4096 && worker11Bytes / 32 == 4096);
	static_assert(worker9Bytes % 24 == 0 && worker11Bytes % 32 == 0);
}
