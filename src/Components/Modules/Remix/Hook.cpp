#include "STDInclude.hpp"

#include "Hook.hpp"
#include "Core.hpp"

namespace Components::Remix::Hook
{
	struct PatchedSite
	{
		std::uintptr_t address = 0;
		unsigned char original[8]{};
		std::size_t size = 5;
	};

	static std::vector<PatchedSite> patchedSites;

	constexpr std::uintptr_t rel32Reach = 0x7FFF0000;

	static bool WriteCode(std::uintptr_t address, const void* data, std::size_t size)
	{
		DWORD previous = 0;

		if (!VirtualProtect(reinterpret_cast<void*>(address), size,
				PAGE_EXECUTE_READWRITE, &previous))
		{
			return false;
		}

		std::memcpy(reinterpret_cast<void*>(address), data, size);

		VirtualProtect(reinterpret_cast<void*>(address), size, previous, &previous);

		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

		return true;
	}

	static bool WriteCallDisplacement(std::uintptr_t site, const unsigned char* bytes)
	{
		const std::uintptr_t displacement = site + 1;

		if (bytes[0] != 0xE8)
		{
			return WriteCode(site, bytes, 5);
		}

		const std::uintptr_t block = displacement & ~static_cast<std::uintptr_t>(15);
		const std::size_t offset = static_cast<std::size_t>(displacement - block);

		if (offset + 4 > 16)
		{
			LogWarn() << "hook: displacement at 0x" << std::hex << displacement << std::dec
					  << " crosses a 16-byte boundary -- written non-atomically";
			return WriteCode(site, bytes, 5);
		}

		DWORD previous = 0;

		if (!VirtualProtect(reinterpret_cast<void*>(block), 16, PAGE_EXECUTE_READWRITE, &previous))
		{
			return false;
		}

		volatile LONG64* const target = reinterpret_cast<volatile LONG64*>(block);

		alignas(16) LONG64 expected[2] = { target[0], target[1] };

		for (;;)
		{
			alignas(16) LONG64 desired[2] = { expected[0], expected[1] };
			std::memcpy(reinterpret_cast<unsigned char*>(desired) + offset, bytes + 1, 4);

			if (InterlockedCompareExchange128(target, desired[1], desired[0], expected))
			{
				break;
			}
		}

		VirtualProtect(reinterpret_cast<void*>(block), 16, previous, &previous);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(block), 16);

		return true;
	}

	static bool ReadSiteGuarded(std::uintptr_t site, unsigned char* out, std::size_t size)
	{
		__try
		{
			std::memcpy(out, reinterpret_cast<const void*>(site), size);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static std::string HexBytes(const unsigned char* bytes, std::size_t size)
	{
		static const char* digits = "0123456789ABCDEF";

		std::string out;

		for (std::size_t i = 0; i < size; ++i)
		{
			if (i)
			{
				out.push_back(' ');
			}

			out.push_back(digits[bytes[i] >> 4]);
			out.push_back(digits[bytes[i] & 0x0F]);
		}

		return out;
	}

	// zw3 components seat their own call hooks at load, so a site that already lands in zw3 is chained, not refused
	static bool IsClientCode(std::uintptr_t address)
	{
		HMODULE owner = nullptr;
		HMODULE self = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

		unsigned char jump[14]{};
		const unsigned char absoluteJump[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };

		if (address && ReadSiteGuarded(address, jump, sizeof(jump)) && std::memcmp(jump, absoluteJump, sizeof(absoluteJump)) == 0)
		{
			std::memcpy(&address, jump + 6, sizeof(address));
		}

		if (!address || !GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(address), &owner))
		{
			return false;
		}

		if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&IsClientCode), &self))
		{
			return false;
		}

		return owner == self;
	}

	void* AllocateNear(std::uintptr_t anchor, std::size_t size)
	{
		SYSTEM_INFO info{};
		GetSystemInfo(&info);

		const std::uintptr_t granularity = info.dwAllocationGranularity;

		std::uintptr_t lowest = 0;

		if (anchor > rel32Reach)
		{
			lowest = (anchor - rel32Reach);
		}
		const std::uintptr_t highest = anchor + rel32Reach;

		for (std::uintptr_t offset = granularity; offset < rel32Reach; offset += granularity)
		{
			const std::uintptr_t candidates[2] = { anchor + offset, anchor - offset };

			for (const std::uintptr_t candidate : candidates)
			{
				if (candidate < lowest || candidate > highest)
				{
					continue;
				}

				void* memory = VirtualAlloc(reinterpret_cast<void*>(candidate & ~(granularity - 1)),
					size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

				if (memory)
				{
					return memory;
				}
			}
		}

		return nullptr;
	}

	std::uintptr_t MakeThunk(std::uintptr_t anchor, const void* target)
	{
		void* memory = AllocateNear(anchor, 16);

		if (!memory)
		{
			return 0;
		}

		unsigned char thunk[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };

		const auto address = reinterpret_cast<std::uint64_t>(target);
		std::memcpy(thunk + 6, &address, sizeof(address));
		std::memcpy(memory, thunk, sizeof(thunk));

		FlushInstructionCache(GetCurrentProcess(), memory, sizeof(thunk));

		return reinterpret_cast<std::uintptr_t>(memory);
	}

	std::uintptr_t ResolveCallTarget(std::uintptr_t site)
	{
		unsigned char bytes[5]{};

		if (!site || !ReadSiteGuarded(site, bytes, sizeof(bytes)) || (bytes[0] != 0xE8 && bytes[0] != 0xE9))
		{
			return 0;
		}

		std::int32_t relative = 0;
		std::memcpy(&relative, bytes + 1, sizeof(relative));

		return site + 5 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(relative));
	}

	bool PatchCall(std::uintptr_t site, const unsigned char* expected, std::size_t expectedSize,
		const void* replacement, void** outOriginal)
	{
		if (!site || !expected || expectedSize != 5 || !replacement)
		{
			return false;
		}

		unsigned char actual[5]{};

		if (!ReadSiteGuarded(site, actual, sizeof(actual)))
		{
			LogError() << "hook: cannot read the patch site at 0x" << std::hex << site << std::dec;
			return false;
		}

		const bool isExpected = std::memcmp(actual, expected, expectedSize) == 0;

		if (!isExpected && !(actual[0] == expected[0] && IsClientCode(ResolveCallTarget(site))))
		{
			LogError() << "hook: site 0x" << std::hex << site << std::dec
					   << " reads " << HexBytes(actual, sizeof(actual))
					   << ", expected " << HexBytes(expected, expectedSize)
					   << " -- refusing to patch";
			return false;
		}

		if (!isExpected)
		{
			Log() << "hook: site 0x" << std::hex << site << std::dec << " already calls into zw3, chaining onto that hook";
		}

		if (actual[0] != 0xE8 && actual[0] != 0xE9)
		{
			LogError() << "hook: site 0x" << std::hex << site << std::dec << " is not a call or a jump";
			return false;
		}

		std::int32_t relative = 0;
		std::memcpy(&relative, actual + 1, sizeof(relative));

		const std::uintptr_t original = site + 5 + static_cast<std::uintptr_t>(
			static_cast<std::intptr_t>(relative));

		if (outOriginal)
		{
			*outOriginal = reinterpret_cast<void*>(original);
		}

		std::uintptr_t destination = reinterpret_cast<std::uintptr_t>(replacement);

		const std::intptr_t direct = static_cast<std::intptr_t>(destination) -
			static_cast<std::intptr_t>(site + 5);

		if (direct > INT32_MAX || direct < INT32_MIN)
		{
			destination = MakeThunk(site, replacement);

			if (!destination)
			{
				LogError() << "hook: no thunk slot within reach of 0x"
						   << std::hex << site << std::dec << ", cannot patch";
				return false;
			}
		}

		const std::intptr_t finalRelative = static_cast<std::intptr_t>(destination) -
			static_cast<std::intptr_t>(site + 5);

		if (finalRelative > INT32_MAX || finalRelative < INT32_MIN)
		{
			LogError() << "hook: thunk is still out of range, cannot patch";
			return false;
		}

		unsigned char patch[5] = { actual[0] };
		const auto rel32 = static_cast<std::int32_t>(finalRelative);
		std::memcpy(patch + 1, &rel32, sizeof(rel32));

		PatchedSite record;
		record.address = site;
		std::memcpy(record.original, actual, sizeof(actual));

		if (!WriteCallDisplacement(site, patch))
		{
			LogError() << "hook: could not make 0x" << std::hex << site << std::dec << " writable";
			return false;
		}

		patchedSites.push_back(record);

		const char* kind = "call";

		if (actual[0] == 0xE9)
		{
			kind = "jump";
		}

		Log() << "hook: patched the " << kind << " at 0x" << std::hex << site
			  << " (was 0x" << original << ")" << std::dec;

		return true;
	}

	bool PatchLiveCall(std::uintptr_t site, const void* replacement)
	{
		unsigned char current[5]{};

		if (!ReadSiteGuarded(site, current, sizeof(current)))
		{
			return false;
		}

		return PatchCall(site, current, sizeof(current), replacement, nullptr);
	}

	bool PatchBytes(std::uintptr_t site, const unsigned char* expected, const unsigned char* replacement,
		std::size_t size)
	{
		if (!site || !expected || !replacement || size == 0 || size > sizeof(PatchedSite::original))
		{
			return false;
		}

		unsigned char actual[sizeof(PatchedSite::original)]{};

		if (!ReadSiteGuarded(site, actual, size))
		{
			LogError() << "hook: cannot read the patch site at 0x" << std::hex << site << std::dec;
			return false;
		}

		if (std::memcmp(actual, expected, size) != 0)
		{
			LogError() << "hook: site 0x" << std::hex << site << std::dec
					   << " reads " << HexBytes(actual, size)
					   << ", expected " << HexBytes(expected, size)
					   << " -- refusing to patch";
			return false;
		}

		PatchedSite record;
		record.address = site;
		record.size = size;
		std::memcpy(record.original, actual, size);

		if (!WriteCode(site, replacement, size))
		{
			LogError() << "hook: could not make 0x" << std::hex << site << std::dec << " writable";
			return false;
		}

		patchedSites.push_back(record);

		Log() << "hook: patched " << size << " bytes at 0x" << std::hex << site << std::dec
			  << " (" << HexBytes(actual, size) << " -> " << HexBytes(replacement, size) << ")";

		return true;
	}

	bool RestoreBytes(std::uintptr_t site)
	{
		for (auto it = patchedSites.begin(); it != patchedSites.end(); ++it)
		{
			if (it->address != site)
			{
				continue;
			}

			if (!WriteCode(site, it->original, it->size))
			{
				LogError() << "hook: could not make 0x" << std::hex << site << std::dec << " writable to restore it";
				return false;
			}

			Log() << "hook: restored " << it->size << " bytes at 0x" << std::hex << site << std::dec;

			patchedSites.erase(it);
			return true;
		}

		return false;
	}

	void RestoreAll()
	{
		for (const PatchedSite& site : patchedSites)
		{
			if (site.size == 5)
			{
				WriteCallDisplacement(site.address, site.original);
			}
			else
			{
				WriteCode(site.address, site.original, site.size);
			}
		}

		if (!patchedSites.empty())
		{
			Log() << "hook: restored " << patchedSites.size() << " patched site(s)";
			patchedSites.clear();
		}
	}
}
