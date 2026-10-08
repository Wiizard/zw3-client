#include "STDInclude.hpp"

namespace Utils
{
	static std::uintptr_t moduleBase = 0;
	static std::uintptr_t idbBase = 0;
	static bool isBound = false;

	constexpr std::size_t blockSize = 0x10000;
	constexpr std::size_t maxBlocks = 8;
	constexpr std::uintptr_t rel32Reach = 0x7FFF0000;
	constexpr std::size_t absoluteJumpSize = 14;

	struct NearBlock
	{
		std::uintptr_t base;
		std::size_t used;
	};

	static NearBlock blocks[maxBlocks];
	static std::size_t blockCount = 0;

	bool Hook::Bind(std::uintptr_t idbImageBase)
	{
		const HMODULE module = GetModuleHandleA(nullptr);

		if (!module)
		{
			return false;
		}

		moduleBase = reinterpret_cast<std::uintptr_t>(module);
		idbBase = idbImageBase;
		isBound = true;

		return true;
	}

	bool Hook::IsBound()
	{
		return isBound;
	}

	std::uintptr_t Hook::Rebase(std::uintptr_t idbAddress)
	{
		if (!isBound)
		{
			return idbAddress;
		}

		return moduleBase + (idbAddress - idbBase);
	}

	std::uintptr_t Hook::Unrebase(std::uintptr_t liveAddress)
	{
		if (!isBound)
		{
			return liveAddress;
		}

		return idbBase + (liveAddress - moduleBase);
	}

	static bool IsWithinRel32(std::uintptr_t from, std::uintptr_t to)
	{
		const std::uintptr_t distance = (to > from) ? (to - from) : (from - to);
		return distance <= rel32Reach;
	}

	static bool ReserveBlockNear(std::uintptr_t anchor, NearBlock& out)
	{
		const std::uintptr_t low = (anchor > rel32Reach) ? (anchor - rel32Reach) : blockSize;
		const std::uintptr_t high = anchor + rel32Reach;

		for (std::uintptr_t step = blockSize; step <= rel32Reach; step += blockSize)
		{
			const std::uintptr_t candidates[2] = { anchor - step, anchor + step };

			for (const std::uintptr_t candidate : candidates)
			{
				if (candidate < low || candidate > high)
				{
					continue;
				}

				void* reserved = VirtualAlloc(reinterpret_cast<void*>(candidate & ~(blockSize - 1)),
					blockSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

				if (reserved)
				{
					out.base = reinterpret_cast<std::uintptr_t>(reserved);
					out.used = 0;
					return true;
				}
			}
		}

		return false;
	}

	static void* AllocateNear(std::uintptr_t anchor, std::size_t size, std::size_t alignment)
	{
		if (size == 0 || size > blockSize)
		{
			return nullptr;
		}

		for (std::size_t index = 0; index < blockCount; ++index)
		{
			NearBlock& block = blocks[index];
			const std::size_t offset = (block.used + alignment - 1) & ~(alignment - 1);

			if (offset + size > blockSize)
			{
				continue;
			}

			if (!IsWithinRel32(anchor, block.base + offset))
			{
				continue;
			}

			block.used = offset + size;
			return reinterpret_cast<void*>(block.base + offset);
		}

		if (blockCount >= maxBlocks)
		{
			return nullptr;
		}

		NearBlock fresh{};

		if (!ReserveBlockNear(anchor, fresh))
		{
			return nullptr;
		}

		fresh.used = size;
		blocks[blockCount++] = fresh;

		return reinterpret_cast<void*>(fresh.base);
	}

	static void WriteAbsoluteJump(std::uint8_t* where, std::uintptr_t destination)
	{
		where[0] = 0xFF;
		where[1] = 0x25;
		where[2] = 0x00;
		where[3] = 0x00;
		where[4] = 0x00;
		where[5] = 0x00;
		std::memcpy(&where[6], &destination, sizeof(destination));
	}

	std::uintptr_t Hook::Trampoline(std::uintptr_t anchor, std::uintptr_t target)
	{
		void* slot = AllocateNear(anchor, absoluteJumpSize, 16);

		if (!slot)
		{
			return 0;
		}

		WriteAbsoluteJump(static_cast<std::uint8_t*>(slot), target);
		FlushInstructionCache(GetCurrentProcess(), slot, absoluteJumpSize);

		return reinterpret_cast<std::uintptr_t>(slot);
	}

	void* Hook::AllocateDataNear(std::uintptr_t anchor, std::size_t size)
	{
		const std::uintptr_t liveAnchor = Rebase(anchor);
		const std::size_t reservation = (size + blockSize - 1) & ~(blockSize - 1);

		if (size == 0 || reservation >= rel32Reach)
		{
			return nullptr;
		}

		for (std::uintptr_t step = blockSize; step + reservation <= rel32Reach; step += blockSize)
		{
			void* reserved = nullptr;

			if (liveAnchor > step + reservation + blockSize)
			{
				const std::uintptr_t below = (liveAnchor - step - reservation) & ~(blockSize - 1);
				reserved = VirtualAlloc(reinterpret_cast<void*>(below), reservation, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			}

			if (!reserved)
			{
				const std::uintptr_t above = (liveAnchor + step) & ~(blockSize - 1);
				reserved = VirtualAlloc(reinterpret_cast<void*>(above), reservation, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			}

			if (reserved)
			{
				return reserved;
			}
		}

		return nullptr;
	}

	void Hook::Signature::Process()
	{
		if (this->signatures.empty())
		{
			return;
		}

		char* const begin = static_cast<char*>(this->start);
		const std::size_t count = this->signatures.size();
		Container* const containers = this->signatures.data();

		for (std::size_t i = 0; i < this->length; ++i)
		{
			char* const address = begin + i;

			for (std::size_t k = 0; k < count; ++k)
			{
				Container* const container = &containers[k];
				const std::size_t maskLength = std::strlen(container->mask);

				std::size_t j;

				for (j = 0; j < maskLength; ++j)
				{
					if (container->mask[j] != '?' && container->signature[j] != address[j])
					{
						break;
					}
				}

				if (j == maskLength)
				{
					container->callback(address);
				}
			}
		}
	}

	void Hook::Signature::Add(const Container& container)
	{
		this->signatures.push_back(container);
	}

	Hook::~Hook()
	{
		if (this->initialized)
		{
			this->Uninstall();
		}
	}

	Hook* Hook::Initialize(std::uintptr_t site, void(*replacement)(), bool asJump)
	{
		return this->Initialize(site, reinterpret_cast<void*>(replacement), asJump);
	}

	Hook* Hook::Initialize(std::uintptr_t site, void* replacement, bool asJump)
	{
		return this->Initialize(reinterpret_cast<void*>(Rebase(site)), replacement, asJump);
	}

	Hook* Hook::Initialize(void* site, void* replacement, bool asJump)
	{
		if (this->initialized)
		{
			return this;
		}

		this->initialized = true;
		this->useJump = asJump;
		this->place = site;
		this->stub = replacement;

		const auto* code = static_cast<const char*>(site);
		this->original = const_cast<char*>(code) + 5 + *reinterpret_cast<const std::int32_t*>(code + 1);

		return this;
	}

	Hook* Hook::Install(bool unprotect, bool keepUnprotected)
	{
		std::lock_guard<std::mutex> _(this->stateMutex);

		if (!this->initialized || this->installed)
		{
			return this;
		}

		const auto site = reinterpret_cast<std::uintptr_t>(this->place);
		const std::uintptr_t next = site + sizeof(this->buffer);
		auto target = reinterpret_cast<std::uintptr_t>(this->stub);

		if (!IsWithinRel32(next, target))
		{
			target = Trampoline(next, target);

			if (!target)
			{
				return this;
			}
		}

		if (unprotect && !VirtualProtect(this->place, sizeof(this->buffer),
			PAGE_EXECUTE_READWRITE, &this->protection))
		{
			return this;
		}

		this->installed = true;

		std::memcpy(this->buffer, this->place, sizeof(this->buffer));

		auto* const code = static_cast<std::uint8_t*>(this->place);
		const auto relative = static_cast<std::int32_t>(target - next);

		code[0] = this->useJump ? 0xE9 : 0xE8;
		std::memcpy(code + 1, &relative, sizeof(relative));

		if (unprotect && !keepUnprotected)
		{
			VirtualProtect(this->place, sizeof(this->buffer), this->protection, &this->protection);
		}

		FlushInstructionCache(GetCurrentProcess(), this->place, sizeof(this->buffer));

		return this;
	}

	Hook* Hook::Uninstall(bool unprotect)
	{
		std::lock_guard<std::mutex> _(this->stateMutex);

		if (!this->initialized || !this->installed)
		{
			return this;
		}

		this->installed = false;

		if (unprotect && !VirtualProtect(this->place, sizeof(this->buffer),
			PAGE_EXECUTE_READWRITE, &this->protection))
		{
			return this;
		}

		std::memcpy(this->place, this->buffer, sizeof(this->buffer));

		if (unprotect)
		{
			VirtualProtect(this->place, sizeof(this->buffer), this->protection, &this->protection);
		}

		FlushInstructionCache(GetCurrentProcess(), this->place, sizeof(this->buffer));

		return this;
	}

	void Hook::Quick()
	{
		if (this->installed)
		{
			this->installed = false;
		}
	}

	void* Hook::GetAddress()
	{
		return this->place;
	}

	void* Hook::GetOriginal()
	{
		return this->original;
	}

	bool Hook::IsInstalled()
	{
		return this->installed;
	}

	void Hook::Nop(void* place, std::size_t length)
	{
		DWORD oldProtect;

		if (!VirtualProtect(place, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			return;
		}

		std::memset(place, 0x90, length);

		VirtualProtect(place, length, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), place, length);
	}

	void Hook::Nop(std::uintptr_t place, std::size_t length)
	{
		Nop(reinterpret_cast<void*>(Rebase(place)), length);
	}

	bool Hook::MatchesBytes(std::uintptr_t place, const std::uint8_t* expected, std::size_t length)
	{
		const auto* const live = reinterpret_cast<const std::uint8_t*>(Rebase(place));

		return std::memcmp(live, expected, length) == 0;
	}

	bool Hook::BranchesTo(std::uintptr_t site, std::uintptr_t target, bool asJump)
	{
		const std::uintptr_t live = Rebase(site);
		std::uint8_t opcode = 0xE8;

		if (asJump)
		{
			opcode = 0xE9;
		}

		if (*reinterpret_cast<const std::uint8_t*>(live) != opcode)
		{
			return false;
		}

		const auto relative = *reinterpret_cast<const std::int32_t*>(live + 1);
		return live + 5 + relative == Rebase(target);
	}

	constexpr std::size_t leaLength = 7;

	constexpr std::size_t nearStringsSize = 0x400;

	static char* nearStrings = nullptr;
	static std::size_t nearStringsUsed = 0;

	static std::int64_t LeaDistance(const Hook::LeaSite& lea, const void* target)
	{
		const auto from = static_cast<std::int64_t>(Hook::Rebase(lea.address) + leaLength);

		return static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) - from;
	}

	bool Hook::IsLeaIntact(const LeaSite& lea)
	{
		if (!MatchesBytes(lea.address, lea.opcode.data(), lea.opcode.size()))
		{
			return false;
		}

		const auto displacement = Get<std::int32_t>(lea.address + lea.opcode.size());

		return static_cast<std::uintptr_t>(lea.address + leaLength + displacement) == lea.target;
	}

	bool Hook::CanLeaReach(const LeaSite& lea, const void* target)
	{
		const std::int64_t distance = LeaDistance(lea, target);

		return distance >= std::numeric_limits<std::int32_t>::min() && distance <= std::numeric_limits<std::int32_t>::max();
	}

	void Hook::PointLeaAt(const LeaSite& lea, const void* target)
	{
		Set<std::int32_t>(lea.address + lea.opcode.size(), static_cast<std::int32_t>(LeaDistance(lea, target)));
	}

	bool Hook::TryPointLeasAt(std::span<const LeaSite> leas, const char* text)
	{
		if (!text)
		{
			return false;
		}

		for (const auto& lea : leas)
		{
			if (!IsLeaIntact(lea) || !CanLeaReach(lea, text))
			{
				return false;
			}
		}

		for (const auto& lea : leas)
		{
			PointLeaAt(lea, text);
		}

		return true;
	}

	bool Hook::TryPointLeaAt(const LeaSite& lea, const char* text)
	{
		return TryPointLeasAt(std::span(&lea, 1), text);
	}

	const char* Hook::PlaceNearImage(const char* text)
	{
		if (!nearStrings)
		{
			nearStrings = static_cast<char*>(AllocateDataNear(idbBase, nearStringsSize));

			if (!nearStrings)
			{
				return nullptr;
			}
		}

		const std::size_t length = std::strlen(text) + 1;

		if (nearStringsUsed + length > nearStringsSize)
		{
			return nullptr;
		}

		char* const placed = nearStrings + nearStringsUsed;
		std::memcpy(placed, text, length);
		nearStringsUsed += length;

		return placed;
	}

	void Hook::SetString(void* place, const char* string, std::size_t length)
	{
		DWORD oldProtect;

		if (!VirtualProtect(place, length + 1, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			return;
		}

		std::strncpy(static_cast<char*>(place), string, length);

		VirtualProtect(place, length + 1, oldProtect, &oldProtect);
	}

	void Hook::SetString(std::uintptr_t place, const char* string, std::size_t length)
	{
		SetString(reinterpret_cast<void*>(Rebase(place)), string, length);
	}

	void Hook::SetString(void* place, const char* string)
	{
		SetString(place, string, std::strlen(static_cast<char*>(place)));
	}

	void Hook::SetString(std::uintptr_t place, const char* string)
	{
		SetString(reinterpret_cast<void*>(Rebase(place)), string);
	}

	void Hook::RedirectJump(void* place, void* stub)
	{
		const auto site = reinterpret_cast<std::uintptr_t>(place);
		const std::uintptr_t next = site + 6;
		auto target = reinterpret_cast<std::uintptr_t>(stub);

		if (!IsWithinRel32(next, target))
		{
			target = Trampoline(next, target);

			if (!target)
			{
				return;
			}
		}

		Set<std::int32_t>(static_cast<char*>(place) + 2, static_cast<std::int32_t>(target - next));
	}

	void Hook::RedirectJump(std::uintptr_t place, void* stub)
	{
		RedirectJump(reinterpret_cast<void*>(Rebase(place)), stub);
	}
}
