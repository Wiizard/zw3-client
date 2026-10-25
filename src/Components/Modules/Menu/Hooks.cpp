#include "STDInclude.hpp"
#include "../Logger.hpp"
#include "Backdrop.hpp"
#include "Hooks.hpp"

namespace Components::Menu
{
	static std::vector<std::unique_ptr<Utils::Hook>> hooks;
	static Game::DB_LoadXAssets_t originalLoad;

	static bool IsClientTarget(std::uintptr_t address)
	{
		// Utils::Hook uses a nearby FF25 thunk when the DLL is out of rel32 range.
		MEMORY_BASIC_INFORMATION memory{};
		if (!VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory))
			|| memory.State != MEM_COMMIT || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
		const auto* code = reinterpret_cast<const std::uint8_t*>(address);
		const std::uint8_t jump[] = {0xFF, 0x25, 0, 0, 0, 0};
		if (address + 14 <= reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize
			&& std::memcmp(code, jump, sizeof(jump)) == 0) std::memcpy(&address, code + 6, sizeof(address));
		HMODULE owner = nullptr, self = nullptr;
		constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		return GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(address), &owner)
			&& GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&IsClientTarget), &self) && owner == self;
	}

	void RemoveHooks()
	{
		for (auto i = hooks.rbegin(); i != hooks.rend(); ++i) (*i)->Uninstall();
		hooks.clear();
	}

	bool InstallHooks(std::span<const HookRequest> requests)
	{
		std::vector<std::uintptr_t> targets;
		for (const auto& request : requests)
		{
			const auto site = Utils::Hook::Rebase(request.address);
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(site);
			const auto target = site + 5 + *reinterpret_cast<const std::int32_t*>(bytes + 1);
			if (bytes[0] != (request.isJump ? 0xE9 : 0xE8)
				|| (target != Utils::Hook::Rebase(request.target) && (!request.original || !IsClientTarget(target))))
			{
				Logger::Error("menu backdrop: hook at 0x{:X} does not match iw4mp 2.0.13\n", request.address);
				return false;
			}
			targets.push_back(target);
		}
		for (const auto& request : requests)
		{
			auto hook = std::make_unique<Utils::Hook>();
			if (!hook->Initialize(request.address, request.replacement, request.isJump)->Install()->IsInstalled())
			{
				RemoveHooks();
				return false;
			}
			hooks.push_back(std::move(hook));
		}
		for (std::size_t i = 0; i < requests.size(); ++i)
			if (requests[i].original) *requests[i].original = reinterpret_cast<void*>(targets[i]);
		return true;
	}

	static void LoadAssets(Game::XZoneInfo* zones, unsigned int count, int sync)
	{
		Backdrop::BeforeZoneLoad(zones, count);
		originalLoad(zones, count, sync);
	}

	bool ObserveZoneLoads()
	{
		// Four complete, position-independent instructions, verified in IDA.
		// This observes engine callers as well as Game::DB_LoadXAssets.
		constexpr std::uintptr_t entry = 0x14012EC40;
		constexpr std::uint8_t expected[] = {0x48,0x8B,0xC4,0x44,0x89,0x40,0x18,0x89,0x50,0x10,0x48,0x89,0x48,0x08};
		if (!Utils::Hook::MatchesBytes(entry, expected, sizeof(expected))) return false;
		auto* trampoline = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(entry, 28));
		if (!trampoline) return false;
		std::memcpy(trampoline, expected, sizeof(expected));
		const std::uint8_t jump[] = {0xFF,0x25,0,0,0,0};
		std::memcpy(trampoline + 14, jump, sizeof(jump));
		const auto next = Utils::Hook::Rebase(entry) + sizeof(expected);
		std::memcpy(trampoline + 20, &next, sizeof(next));
		DWORD protection;
		if (!VirtualProtect(trampoline, 28, PAGE_EXECUTE_READ, &protection))
		{
			VirtualFree(trampoline, 0, MEM_RELEASE);
			return false;
		}
		std::uint8_t replacement[14];
		std::memcpy(replacement, jump, sizeof(jump));
		const auto target = reinterpret_cast<std::uintptr_t>(&LoadAssets);
		std::memcpy(replacement + 6, &target, sizeof(target));
		auto* code = reinterpret_cast<void*>(Utils::Hook::Rebase(entry));
		if (!VirtualProtect(code, sizeof(replacement), PAGE_EXECUTE_READWRITE, &protection))
		{
			VirtualFree(trampoline, 0, MEM_RELEASE);
			return false;
		}
		originalLoad = reinterpret_cast<Game::DB_LoadXAssets_t>(trampoline);
		std::memcpy(code, replacement, sizeof(replacement));
		VirtualProtect(code, sizeof(replacement), protection, &protection);
		FlushInstructionCache(GetCurrentProcess(), trampoline, 28);
		FlushInstructionCache(GetCurrentProcess(), code, sizeof(replacement));
		return true;
	}
}
