#pragma once

#include "RenderBudgetPolicy.hpp"

namespace Components::RendererBudgets
{
	struct Patch
	{
		std::uintptr_t address;
		std::vector<std::uint8_t> expected;
		std::size_t offset;
		std::int64_t value;
		std::size_t width = 4;
	};

	// Validate and unlock every page before changing any instruction. A mismatch
	// leaves the complete budget group stock, including its backing storage.
	inline bool Commit(const std::vector<Patch>& patches)
	{
		SYSTEM_INFO info{};
		GetSystemInfo(&info);
		std::map<std::uintptr_t, DWORD> pages;
		for (const auto& patch : patches)
		{
			if ((patch.width != 1 && patch.width != 4) || patch.offset + patch.width > patch.expected.size()
				|| (patch.width == 4 && (patch.value < INT32_MIN || patch.value > INT32_MAX))
				|| (patch.width == 1 && (patch.value < 0 || patch.value > UINT8_MAX))
				|| !Utils::Hook::MatchesBytes(patch.address, patch.expected.data(), patch.expected.size()))
			{
				return false;
			}
			const auto begin = Utils::Hook::Rebase(patch.address);
			const auto end = begin + patch.expected.size() - 1;
			for (auto page = begin & ~(std::uintptr_t(info.dwPageSize) - 1); page <= end; page += info.dwPageSize)
			{
				pages.try_emplace(page, 0);
			}
		}
		for (auto& [page, protection] : pages)
		{
			if (!VirtualProtect(reinterpret_cast<void*>(page), info.dwPageSize, PAGE_EXECUTE_READWRITE, &protection))
			{
				for (auto& [priorPage, priorProtection] : pages)
				{
					if (!priorProtection) continue;
					DWORD unused;
					VirtualProtect(reinterpret_cast<void*>(priorPage), info.dwPageSize, priorProtection, &unused);
				}
				return false;
			}
		}
		for (const auto& patch : patches)
		{
			const auto value = static_cast<std::int32_t>(patch.value);
			std::memcpy(reinterpret_cast<void*>(Utils::Hook::Rebase(patch.address) + patch.offset), &value, patch.width);
		}
		for (auto& [page, protection] : pages)
		{
			DWORD unused;
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(page), info.dwPageSize);
			VirtualProtect(reinterpret_cast<void*>(page), info.dwPageSize, protection, &unused);
		}
		return true;
	}

	inline Patch Rip(std::uintptr_t address, std::initializer_list<std::uint8_t> bytes, std::uintptr_t target)
	{
		return { address, bytes, 3, static_cast<std::int64_t>(target) - static_cast<std::int64_t>(Utils::Hook::Rebase(address) + bytes.size()) };
	}

	inline std::uint32_t lightingPatchCapacity = 4096;

	inline bool RaiseStaticLighting()
	{
		using namespace RenderBudgetPolicy;
		auto* const storage = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(0x14003D466, cacheBytes));
		if (!storage) return false;
		const auto base = reinterpret_cast<std::uintptr_t>(storage);
		const auto image = Utils::Hook::Rebase(0x140000000);
		auto* const patchStorage = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(0x14003D418, lightingPatchStorageBytes));
		if (!patchStorage)
		{
			VirtualFree(storage, 0, MEM_RELEASE);
			return false;
		}
		// Both native frame buffers retain their original address and spacing.
		// Relocate only the list: frontEnd + displacement selects the matching list
		// for both producers and the backend consumer, without sharing frames.
		const auto patchDisplacement = reinterpret_cast<std::int64_t>(patchStorage)
			- static_cast<std::int64_t>(Utils::Hook::Rebase(0x149145900));
		const std::vector<Patch> patches =
		{
			Rip(0x14003D466, {0x4c,0x8d,0x05,0x93,0x36,0x40,0x09}, base),
			Rip(0x14003D582, {0x4c,0x8d,0x05,0x77,0x35,0x40,0x09}, base),
			Rip(0x140021EDC, {0x48,0x8d,0x0d,0x1d,0x24,0x42,0x09}, base + modelBits),
			Rip(0x14003DD13, {0x48,0x8d,0x35,0xe6,0x65,0x40,0x09}, base + modelBits),
			{0x14001BBD8, {0x2b,0x84,0x8b,0x00,0x9b,0x44,0x09}, 3, static_cast<std::int64_t>(base + frameStamps) - static_cast<std::int64_t>(image)},
			{0x14001BBEA, {0x66,0x89,0x8c,0x43,0x00,0x0b,0x44,0x09}, 4, static_cast<std::int64_t>(base) - static_cast<std::int64_t>(image)},
			{0x14003D4C1, {0x41,0x3b,0x94,0xa8,0x00,0x90,0x00,0x00}, 4, frameStamps},
			{0x14003D4CB, {0x41,0x0f,0xb7,0x84,0x68,0x00,0x58,0x00,0x00}, 5, modelIds},
			{0x14003D5AE, {0x66,0x45,0x89,0xa4,0x40,0x00,0x58,0x00,0x00}, 5, modelIds},
			{0x14003D5D1, {0x09,0x82,0x00,0x38,0x00,0x00}, 2, modelBits},
			{0x14003D5E9, {0x41,0x89,0x84,0x88,0x00,0x90,0x00,0x00}, 4, frameStamps},
			{0x14003E477, {0x81,0xfa,0x00,0x08,0x00,0x00}, 2, minimumStaticLighting},
			{0x14003E497, {0x81,0xfa,0x00,0x08,0x00,0x00}, 2, minimumStaticLighting},
			{0x14003D401, {0x48,0x81,0xff,0x00,0x10,0x00,0x00}, 3, lightingPatchEntries},
			{0x14003DC12, {0x49,0x81,0xfe,0x00,0x10,0x00,0x00}, 3, lightingPatchEntries},
			{0x14003D418, {0x48,0x81,0xc3,0xa8,0x96,0x10,0x00}, 3, patchDisplacement},
			{0x14003DC35, {0x48,0x8d,0x93,0xa8,0x96,0x10,0x00}, 3, patchDisplacement},
			{0x14004FC74, {0x48,0x8d,0x8a,0xa8,0x96,0x10,0x00}, 3, patchDisplacement},
		};
		if (!Commit(patches))
		{
			VirtualFree(storage, 0, MEM_RELEASE);
			VirtualFree(patchStorage, 0, MEM_RELEASE);
			return false;
		}
		lightingPatchCapacity = lightingPatchEntries;
		// Keep native capacity, frame counters, texture sizing and cache invalidation.
		// The native initializer sizes the GPU atlas from the new minimum as before.
		return true;
	}

	inline bool RaiseWorkerQueues()
	{
		using namespace RenderBudgetPolicy;
		auto* const storage = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(0x1400473DE, worker9Bytes + worker11Bytes));
		if (!storage) return false;
		const auto base = reinterpret_cast<std::uintptr_t>(storage);
		const std::vector<Patch> patches =
		{
			Rip(0x1400473DE, {0x48,0x8d,0x05,0x4b,0x98,0x42,0x09}, base),
			Rip(0x140047416, {0x48,0x8d,0x05,0x13,0xc0,0x42,0x09}, base + worker9Bytes),
			{0x1400474F9, {0xc7,0x05,0x1d,0x91,0x42,0x09,0x00,0x18,0x00,0x00}, 6, worker9Bytes},
			{0x140047549, {0xc7,0x05,0xcd,0x91,0x42,0x09,0x00,0x20,0x00,0x00}, 6, worker11Bytes},
		};
		if (!Commit(patches))
		{
			VirtualFree(storage, 0, MEM_RELEASE);
			return false;
		}
		// Native initialization derives maximum command counts from bytes / stride.
		// Command payload layouts and all producer/consumer synchronization stay stock.
		return true;
	}

	struct ModelCommandStream
	{
		std::uint32_t kind;
		std::uint32_t padding;
		std::uint8_t* data;
		std::uint32_t used;
		std::uint32_t capacity;
	};
	static_assert(sizeof(ModelCommandStream) == 24 && offsetof(ModelCommandStream, used) == 16);
	inline std::uint8_t* modelCommandStorage = nullptr;
	inline std::atomic_uint modelCommandPeakRecords = 0;
	inline std::atomic_uint modelCommandRejectedStreams = 0;

	inline unsigned AppendModelCommand(void* model, unsigned lod, const std::uint16_t* models,
		unsigned count, ModelCommandStream* stream, unsigned* flags, std::uint16_t* token)
	{
		using namespace RenderBudgetPolicy;
		const auto original = Utils::Hook::Rebase(0x149145900);
		const auto address = reinterpret_cast<std::uintptr_t>(stream->data);
		if (!stream->used && address >= original && address < original + 2 * backEndDataStride)
		{
			const auto frame = (address - original) / backEndDataStride;
			const auto offset = (address - original) % backEndDataStride;
			if (offset >= 0xD2F80 && offset < 0x1096A8 && (offset - 0xD2F80) % 0x800 == 0)
			{
				const auto pass = frame * 128 + (offset - 0xD2F80) / 0x800;
				stream->data = modelCommandStorage + pass * modelCommandPassBytes;
				stream->capacity = modelCommandPassBytes;
			}
		}
		const auto data = reinterpret_cast<std::uintptr_t>(stream->data);
		const auto pool = reinterpret_cast<std::uintptr_t>(modelCommandStorage);
		// All known native producers use the two original frame arenas. Refuse an
		// unknown layout rather than mixing byte offsets with record-index tokens.
		if (!pool || data < pool || data >= pool + modelCommandStorageBytes
			|| (data - pool) % modelCommandPassBytes || stream->used % modelCommandSlotBytes
			|| count > (modelCommandSlotBytes - 13) / 2)
		{
			++modelCommandRejectedStreams;
			reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(0x14003EAE0))(14);
			return 0;
		}
		const auto slot = static_cast<std::uint32_t>(stream->used / modelCommandSlotBytes);
		// Native construction still handles cached/noncached records, their model
		// IDs, surface pointers and the original overflow warning at full capacity.
		const auto result = reinterpret_cast<unsigned(*)(void*, unsigned, const std::uint16_t*, unsigned,
			ModelCommandStream*, unsigned*, std::uint16_t*)>(Utils::Hook::Rebase(0x14005ED40))
			(model, lod, models, count, stream, flags, token);
		if (result)
		{
			*token = static_cast<std::uint16_t>(slot * 4);
			stream->used = static_cast<std::uint32_t>((slot + 1) * modelCommandSlotBytes);
			auto peak = modelCommandPeakRecords.load();
			while (peak < slot + 1 && !modelCommandPeakRecords.compare_exchange_weak(peak, slot + 1)) {}
		}
		return result;
	}

	inline bool RaiseModelCommands()
	{
		using namespace RenderBudgetPolicy;
		const std::pair<std::uintptr_t, std::array<std::uint8_t, 5>> calls[] =
		{
			{0x14005F369, {0xe8,0xd2,0xf9,0xff,0xff}},
			{0x14005F4C0, {0xe8,0x7b,0xf8,0xff,0xff}},
			{0x14005F663, {0xe8,0xd8,0xf6,0xff,0xff}},
			{0x14005F7C1, {0xe8,0x7a,0xf5,0xff,0xff}},
			{0x14005F9AD, {0xe8,0x8e,0xf3,0xff,0xff}},
			{0x14005FB78, {0xe8,0xc3,0xf1,0xff,0xff}},
		};
		std::vector<Patch> patches =
		{
			{0x14007672E, {0xc1,0xe2,0x02}, 2, modelCommandSlotShift, 1},
			{0x140076803, {0x41,0xc1,0xe4,0x02}, 3, modelCommandSlotShift, 1},
			{0x140076B2F, {0xc1,0xe2,0x02}, 2, modelCommandSlotShift, 1},
			{0x14007B159, {0xc1,0xe3,0x02}, 2, modelCommandSlotShift, 1},
			{0x14007B33F, {0xc1,0xe2,0x02}, 2, modelCommandSlotShift, 1},
			{0x14007B4B2, {0x41,0xc1,0xe0,0x02}, 3, modelCommandSlotShift, 1},
		};
		for (const auto& patch : patches)
		{
			if (!Utils::Hook::MatchesBytes(patch.address, patch.expected.data(), patch.expected.size())) return false;
		}
		for (const auto& [address, bytes] : calls)
		{
			if (!Utils::Hook::MatchesBytes(address, bytes.data(), bytes.size())) return false;
		}
		auto* storage = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, modelCommandStorageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (!storage) return false;
		const auto relay = Utils::Hook::Trampoline(Utils::Hook::Rebase(calls[0].first), reinterpret_cast<std::uintptr_t>(AppendModelCommand));
		if (!relay)
		{
			VirtualFree(storage, 0, MEM_RELEASE);
			return false;
		}
		for (const auto& [address, bytes] : calls)
		{
			patches.push_back({address, {bytes.begin(), bytes.end()}, 1,
				static_cast<std::int64_t>(relay) - static_cast<std::int64_t>(Utils::Hook::Rebase(address) + 5)});
		}
		if (!Commit(patches))
		{
			VirtualFree(storage, 0, MEM_RELEASE);
			return false;
		}
		modelCommandStorage = storage;
		return true;
	}

	struct MaterialStream
	{
		std::uint32_t key;
		std::uint32_t padding;
		std::uint16_t* header;
		std::uint32_t count;
		std::uint32_t padding2;
		std::uint16_t* begin;
		std::uint16_t* cursor;
		std::uint16_t* end;
	};
	static_assert(sizeof(MaterialStream) == 0x30);
	static_assert(offsetof(MaterialStream, begin) == 0x18 && offsetof(MaterialStream, end) == 0x28);

	struct StreamStorage
	{
		std::unique_ptr<std::uint16_t[]> words;
		std::size_t count = 0;
	};

	inline std::mutex streamMutex;
	inline std::map<std::uintptr_t, StreamStorage> streams;
	inline std::size_t streamBytes = 0;
	inline std::atomic_uint streamFallbacks = 0;
	constexpr std::size_t streamMemoryLimit = 128 * 1024 * 1024;
	constexpr std::size_t materialStreamScale = 16;

	inline void GrowMaterialStream(MaterialStream* stream)
	{
		// Native front-end buffers are double-buffered. Key storage by the original
		// stream address, not by a stack descriptor or a render-thread/frame guess.
		// It is reused only when the engine reuses that same original buffer.
		if (stream->cursor != stream->begin || stream->header || stream->count
			|| stream->key != UINT32_MAX || !stream->begin)
		{
			return;
		}
		const auto begin = reinterpret_cast<std::uintptr_t>(stream->begin);
		const auto end = reinterpret_cast<std::uintptr_t>(stream->end);
		if (end <= begin || (end - begin) % sizeof(std::uint16_t) || end - begin > 0x8000)
		{
			++streamFallbacks;
			return;
		}
		const auto count = (end - begin) / sizeof(std::uint16_t) * materialStreamScale;
		std::lock_guard lock(streamMutex);
		auto entry = streams.find(begin);
		const auto oldCount = entry == streams.end() ? 0 : entry->second.count;
		if (count > oldCount)
		{
			const auto extra = (count - oldCount) * sizeof(std::uint16_t);
			if (extra > streamMemoryLimit - streamBytes)
			{
				++streamFallbacks;
				return;
			}
			auto words = std::unique_ptr<std::uint16_t[]>(new (std::nothrow) std::uint16_t[count]);
			if (!words)
			{
				++streamFallbacks;
				return;
			}
			if (entry == streams.end())
			{
				try { entry = streams.try_emplace(begin).first; }
				catch (const std::bad_alloc&)
				{
					++streamFallbacks;
					return;
				}
			}
			entry->second.words = std::move(words);
			entry->second.count = count;
			streamBytes += extra;
		}
		stream->begin = entry->second.words.get();
		stream->cursor = stream->begin;
		stream->end = stream->begin + entry->second.count;
	}

	inline void AppendMaterial(MaterialStream* stream, std::uint32_t key, std::uint16_t model)
	{
		GrowMaterialStream(stream);
		// Preserve native packing, ordering, delayed-material handling and warnings.
		reinterpret_cast<void(*)(MaterialStream*, std::uint32_t, std::uint16_t)>(Utils::Hook::Rebase(0x14005EFE0))(stream, key, model);
	}

	inline bool RaiseMaterialStreams()
	{
		const std::pair<std::uintptr_t, std::array<std::uint8_t, 5>> calls[] =
		{
			{0x14005F1BD, {0xe8,0x1e,0xfe,0xff,0xff}},
			{0x14005F424, {0xe8,0xb7,0xfb,0xff,0xff}},
			{0x14005F6E5, {0xe8,0xf6,0xf8,0xff,0xff}},
			{0x14005FA59, {0xe8,0x82,0xf5,0xff,0xff}},
			{0x14005FACE, {0xe8,0x0d,0xf5,0xff,0xff}},
			{0x1400605E3, {0xe8,0xf8,0xe9,0xff,0xff}},
			{0x140060D51, {0xe8,0x8a,0xe2,0xff,0xff}},
			{0x140061312, {0xe8,0xc9,0xdc,0xff,0xff}},
			{0x140061A61, {0xe8,0x7a,0xd5,0xff,0xff}},
		};
		for (const auto& [address, bytes] : calls)
		{
			if (!Utils::Hook::MatchesBytes(address, bytes.data(), bytes.size())) return false;
		}
		const auto relay = Utils::Hook::Trampoline(Utils::Hook::Rebase(calls[0].first), reinterpret_cast<std::uintptr_t>(AppendMaterial));
		if (!relay) return false;
		std::vector<Patch> patches;
		for (const auto& [address, bytes] : calls)
		{
			patches.push_back({address, {bytes.begin(), bytes.end()}, 1,
				static_cast<std::int64_t>(relay) - static_cast<std::int64_t>(Utils::Hook::Rebase(address) + 5)});
		}
		return Commit(patches);
	}
}
