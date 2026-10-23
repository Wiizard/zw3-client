// Execute the archived x64 engine routines in an isolated test process, not in a game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <thread>
#include <vector>

#undef assert
#define assert(condition) do { if (!(condition)) { std::cerr << "FAILED " << __LINE__ << ": " #condition "\n"; ExitProcess(1); } } while (false)

namespace Utils
{
	struct Hook
	{
		static inline std::uintptr_t image = 0;
		static inline std::size_t next = 0x9800000;
		static std::uintptr_t Rebase(std::uintptr_t address) { return image + address - 0x140000000; }
		static bool MatchesBytes(std::uintptr_t address, const std::uint8_t* expected, std::size_t size)
		{
			return std::memcmp(reinterpret_cast<void*>(Rebase(address)), expected, size) == 0;
		}
		static void* AllocateDataNear(std::uintptr_t, std::size_t size)
		{
			for (int i = 0; i < 1024; ++i)
			{
				// Exercise signed image-relative displacements below the main image.
				const auto address = image - next;
				next += (size + 0xffff) & ~std::size_t(0xffff);
				if (auto* ptr = VirtualAlloc(reinterpret_cast<void*>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) return ptr;
			}
			return nullptr;
		}
		static std::uintptr_t Trampoline(std::uintptr_t, std::uintptr_t target)
		{
			auto* code = static_cast<std::uint8_t*>(AllocateDataNear(0, 32));
			assert(code);
			DWORD previous;
			assert(VirtualProtect(code, 32, PAGE_EXECUTE_READWRITE, &previous));
			code[0] = 0xff; code[1] = 0x25;
			std::memset(code + 2, 0, 4);
			std::memcpy(code + 6, &target, 8);
			return reinterpret_cast<std::uintptr_t>(code);
		}
	};
}

#include "../src/Components/Modules/RendererBudgets.hpp"

using namespace Components;
static std::atomic_uint warnings[60];
static std::atomic_uint workerFallbacks;
static bool expectPatchOverflow = false;
static std::uint8_t* patchEndCanary = nullptr;
static std::vector<std::uint32_t> atlas(256 * 1024 * 4, 0);
static unsigned atlasLocks = 0, atlasUnlocks = 0;

static void Fatal(int, const char* text)
{
	assert(expectPatchOverflow && std::strcmp(text, "modelLightingPatchList ran out of elements.") == 0);
	assert(patchEndCanary && std::all_of(patchEndCanary, patchEndCanary + 64, [](auto b) { return b == 0xa5; }));
	ExitProcess(42);
}

static bool IsAlternateFormat(const char*) { return false; }
static long LockAtlas(void*, unsigned, void* box, void*, unsigned)
{
	++atlasLocks;
	auto* bytes = static_cast<std::uint8_t*>(box);
	*reinterpret_cast<unsigned*>(bytes) = 256 * 4;
	*reinterpret_cast<unsigned*>(bytes + 4) = 256 * 1024 * 4;
	*reinterpret_cast<void**>(bytes + 8) = atlas.data();
	return 0;
}
static long UnlockAtlas(void*, unsigned) { ++atlasUnlocks; return 0; }

static void Warn(int type, ...) { assert(type >= 0 && type < 60); ++warnings[type]; }
static void DirectWorker(int, void*) { ++workerFallbacks; }
static void* Allocate(std::size_t size) { return std::calloc(1, size ? size : 1); }
static void* Copy(void* to, const void* from, std::size_t bytes) { return std::memcpy(to, from, bytes); }

template<class T> static T& At(std::size_t rva)
{
	return *reinterpret_cast<T*>(Utils::Hook::image + rva);
}

static std::vector<std::uint8_t> Read(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	assert(file.good());
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void Redirect(std::size_t rva, const void* target)
{
	auto* code = reinterpret_cast<std::uint8_t*>(Utils::Hook::image + rva);
	code[0] = 0xff; code[1] = 0x25;
	std::memset(code + 2, 0, 4);
	std::memcpy(code + 6, &target, 8);
}

static void RedirectTarget(std::size_t call, const void* target)
{
	assert(At<std::uint8_t>(call) == 0xe8);
	Redirect(call + 5 + At<std::int32_t>(call + 1), target);
}

static RendererBudgets::MaterialStream Stream(std::vector<std::uint16_t>& words)
{
	return {UINT32_MAX, 0, nullptr, 0, 0, words.data(), words.data(), words.data() + words.size()};
}

int main(int argc, char** argv)
{
	SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* error) -> LONG
	{
		std::cerr << "Native exception 0x" << std::hex << error->ExceptionRecord->ExceptionCode
			<< " at image RVA 0x" << error->ContextRecord->Rip - Utils::Hook::image
			<< " rcx=0x" << error->ContextRecord->Rcx << " rdx=0x" << error->ContextRecord->Rdx << '\n';
		return EXCEPTION_EXECUTE_HANDLER;
	});
	assert(argc >= 2 && argc <= 4);
	const std::string mode = argc == 3 ? argv[2] : "";
	const bool overflowChild = mode == "--patch-overflow" || mode == "--patch-inline-overflow";
	auto code = Read(std::string(argv[1]) + "/0-0.text");
	auto constants = Read(std::string(argv[1]) + "/rdata.bin");
	Utils::Hook::image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x9500000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	assert(Utils::Hook::image);
	std::memcpy(reinterpret_cast<void*>(Utils::Hook::image + 0x1000), code.data(), code.size());
	std::memcpy(reinterpret_cast<void*>(Utils::Hook::image + 0x362000), constants.data(), constants.size());
	Redirect(0x3eae0, reinterpret_cast<void*>(Warn));
	RedirectTarget(0x3e50e, reinterpret_cast<void*>(Allocate));
	RedirectTarget(0x4777c, reinterpret_cast<void*>(Copy));
	Redirect(0x467e0, reinterpret_cast<void*>(DirectWorker));
	RedirectTarget(0x3d413, reinterpret_cast<void*>(Fatal));
	if (mode == "--stock-patch-overflow")
	{
		At<void*>(0x8c28500) = reinterpret_cast<void*>(Utils::Hook::image + 0x9145900);
		At<unsigned>(0x9145900 + 0x41938) = 4096;
		patchEndCanary = reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + 0x1096a8 + 4096 * 52);
		std::memset(patchEndCanary, 0xa5, 64);
		expectPatchOverflow = true;
		reinterpret_cast<void(*)()>(Utils::Hook::image + 0x3d3e0)();
		assert(false);
	}
	bool lightingApplied = false;

	// One mismatched instruction must prevent every write in the group.
	for (const auto site : {0x3d466, 0x3d401, 0x3dc12, 0x3d418, 0x3dc35, 0x4fc74})
	{
		At<std::uint8_t>(site) ^= 1;
		std::vector<std::uint8_t> beforeMismatch(code.size());
		std::memcpy(beforeMismatch.data(), reinterpret_cast<void*>(Utils::Hook::image + 0x1000), code.size());
		assert(!RendererBudgets::RaiseStaticLighting());
		assert(std::memcmp(beforeMismatch.data(), reinterpret_cast<void*>(Utils::Hook::image + 0x1000), code.size()) == 0);
		assert(RendererBudgets::lightingPatchCapacity == 4096);
		At<std::uint8_t>(site) ^= 1;
	}
	lightingApplied = RendererBudgets::RaiseStaticLighting();
	assert(lightingApplied && RendererBudgets::RaiseWorkerQueues());
	assert(!RendererBudgets::RaiseStaticLighting());
	assert(!RendererBudgets::RaiseWorkerQueues());

	// Run the actual initializer and allocator, including 16-bit handle ownership.
	const auto initLighting = reinterpret_cast<void(*)()>(Utils::Hook::image + 0x3e3c0);
	const auto lightModel = reinterpret_cast<bool(*)(void*, unsigned)>(Utils::Hook::image + 0x3d450);
	std::vector<std::uint8_t> draws(65535 * 88), world(0x400);
	At<void*>(0x8ccc9b0) = world.data();
	At<void*>(0x8ccc7a0) = world.data();
	*reinterpret_cast<void**>(world.data() + 0x88) = draws.data();
	*reinterpret_cast<void**>(world.data() + 0x328) = draws.data();
	unsigned largestCapacity = 0;
	for (unsigned config = 1; config <= 3; ++config)
	{
		At<unsigned>(0x8ccc948) = config;
		At<std::uint64_t>(0x9450b04) = 0;
		At<unsigned>(0x9450b0c) = 100;
		std::memset(draws.data(), 0, draws.size());
		initLighting();
		const auto capacity = At<unsigned>(0x9450b00);
		assert(capacity >= RenderBudgetPolicy::minimumStaticLighting && capacity < 65535);
		largestCapacity = (std::max)(largestCapacity, capacity);
		for (unsigned i = 0; i < capacity; ++i)
		{
			assert(lightModel(draws.data() + i * 88, i));
			assert(*reinterpret_cast<std::uint16_t*>(draws.data() + i * 88 + 66) == i + 1);
		}
		assert(At<unsigned>(0x9450b04) == capacity);
		assert(!lightModel(draws.data() + capacity * 88, capacity));
		assert(lightModel(draws.data(), 0));
		assert(warnings[13] == 0);
		// Recycle a stale owner through the real eviction path, including invalidation.
		const auto cache = Utils::Hook::image + 0x3d466 + 7 + At<std::int32_t>(0x3d469);
		*reinterpret_cast<std::uint16_t*>(cache) = 101;
		At<unsigned>(0x9450b08) = 1;
		At<unsigned>(0x9450b0c) = 200;
		assert(lightModel(draws.data() + capacity * 88, capacity));
		assert(*reinterpret_cast<std::uint16_t*>(draws.data() + 100 * 88 + 66) == 0);
		assert(*reinterpret_cast<std::uint16_t*>(draws.data() + capacity * 88 + 66) == 101);
		*reinterpret_cast<std::uint16_t*>(cache) = 101;
		At<unsigned>(0x9450b08) = 1;
		At<unsigned>(0x9450b0c) = 201;
		assert(lightModel(draws.data() + 65534 * 88, 65534));
		assert(*reinterpret_cast<std::uint16_t*>(draws.data() + 65534 * 88 + 66) == 101);
		assert((*reinterpret_cast<unsigned*>(cache + RenderBudgetPolicy::modelBits + 2047 * 4) & 2) != 0);
	}

	// Exercise both real patch producers and the real upload loop, including both
	// native frame buffers and their original reset/consumer instructions.
	using namespace RenderBudgetPolicy;
	const auto allocatePatch = reinterpret_cast<std::uint8_t*(*)()>(Utils::Hook::image + 0x3d3e0);
	const auto colorPatch = reinterpret_cast<void(*)(unsigned)>(Utils::Hook::image + 0x3dbb0);
	const auto patchDelta = At<std::int32_t>(0x3d41b);
	auto* patchBase = reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + patchDelta);
	assert(At<std::int32_t>(0x3dc38) == patchDelta && At<std::int32_t>(0x4fc77) == patchDelta);
	assert(RendererBudgets::lightingPatchCapacity == lightingPatchEntries);
	if (overflowChild)
	{
		At<void*>(0x8c28500) = reinterpret_cast<void*>(Utils::Hook::image + 0x9145900);
		At<unsigned>(0x9145900 + 0x41938) = lightingPatchEntries;
		patchEndCanary = patchBase + lightingPatchEntries * lightingPatchStride;
		std::memset(patchEndCanary, 0xa5, 64);
		expectPatchOverflow = true;
		if (mode == "--patch-inline-overflow")
		{
			*reinterpret_cast<std::uint16_t*>(draws.data() + 66) = 1;
			draws[70] = 0x20;
			colorPatch(0);
		}
		else allocatePatch();
		assert(false);
	}

	// A tiny ABI adapter executes the unchanged native counter-reset instruction.
	auto* resetCode = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	assert(resetCode);
	const std::uint8_t resetPrefix[] = {0x56, 0x48, 0x89, 0xca, 0x31, 0xf6}; // push rsi; mov rdx,rcx; xor esi,esi
	std::memcpy(resetCode, resetPrefix, sizeof(resetPrefix));
	std::memcpy(resetCode + 6, code.data() + 0x1bc6f - 0x1000, 6);
	resetCode[12] = 0x5e; resetCode[13] = 0xc3;
	const auto resetPatches = reinterpret_cast<void(*)(void*)>(resetCode);

	RedirectTarget(0x3dec2, reinterpret_cast<void*>(IsAlternateFormat));
	std::array<void*, 22> atlasVtable{};
	atlasVtable[0x98 / 8] = reinterpret_cast<void*>(LockAtlas);
	atlasVtable[0xa0 / 8] = reinterpret_cast<void*>(UnlockAtlas);
	void* atlasVtablePointer = atlasVtable.data();
	void* atlasObject = &atlasVtablePointer;
	At<void*>(0x94409f8) = &atlasObject;
	// Preserve the actual patched LEA and count load at the backend handoff.
	auto* consumeCode = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	assert(consumeCode);
	const std::uint8_t consumerPrefix[] = {0x48, 0x89, 0xca}; // mov rdx,rcx
	std::memcpy(consumeCode, consumerPrefix, 3);
	std::memcpy(consumeCode + 3, reinterpret_cast<void*>(Utils::Hook::image + 0x4fc74), 13);
	consumeCode[16] = 0x48; consumeCode[17] = 0xb8;
	const auto upload = Utils::Hook::image + 0x3de90;
	std::memcpy(consumeCode + 18, &upload, 8);
	consumeCode[26] = 0xff; consumeCode[27] = 0xe0;
	const auto consumePatches = reinterpret_cast<void(*)(void*)>(consumeCode);
	std::vector<std::uint8_t> completedFirstFrame;
	std::memset(draws.data(), 0, draws.size());
	for (unsigned frame = 0; frame < 2; ++frame)
	{
		auto* frontEnd = reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + frame * backEndDataStride);
		auto* list = patchBase + frame * backEndDataStride;
		At<void*>(0x8c28500) = frontEnd;
		std::memset(frontEnd, 0xcc, backEndDataStride);
		std::memset(list + lightingPatchEntries * lightingPatchStride, 0xa5, 64);
		resetPatches(frontEnd);
		const unsigned color = 0xff405060 + frame;
		for (unsigned i = 0; i < lightingPatchEntries; ++i)
		{
			const auto handle = static_cast<std::uint16_t>(i % largestCapacity);
			auto* item = list + i * lightingPatchStride;
			if (i < lightingPatchEntries / 2)
			{
				assert(allocatePatch() == item);
				assert(std::all_of(item, item + lightingPatchStride, [](auto b) { return b == 0; }));
				*reinterpret_cast<std::uint16_t*>(item) = handle;
				*reinterpret_cast<unsigned*>(item + 4) = color;
			}
			else
			{
				auto* draw = draws.data() + i * 88;
				*reinterpret_cast<std::uint16_t*>(draw + 66) = handle + 1;
				draw[70] = 0x20;
				draw[72] = 0x10; draw[73] = 0x20; draw[74] = 0x30; draw[75] = 0xff;
				colorPatch(i);
				assert(*reinterpret_cast<std::uint16_t*>(item) == handle);
				assert(*reinterpret_cast<unsigned*>(item + 4) == 0xff102030);
				assert(std::all_of(item + 8, item + lightingPatchStride, [](auto b) { return b == 0; }));
			}
		}
		assert(*reinterpret_cast<unsigned*>(frontEnd + 0x41938) == lightingPatchEntries);
		assert(std::all_of(list + lightingPatchEntries * lightingPatchStride, list + lightingPatchEntries * lightingPatchStride + 64, [](auto b) { return b == 0xa5; }));
		if (frame == 1) assert(std::equal(completedFirstFrame.begin(), completedFirstFrame.end(), patchBase));
		std::fill(atlas.begin(), atlas.end(), 0);
		consumePatches(frontEnd);
		assert(atlasLocks == frame + 1 && atlasUnlocks == frame + 1);
		for (unsigned handle = 0; handle < largestCapacity; ++handle)
		{
			const unsigned last = handle + ((lightingPatchEntries - 1 - handle) / largestCapacity) * largestCapacity;
			const unsigned expected = last < lightingPatchEntries / 2 ? color : 0xff102030;
			for (unsigned z = 0; z < 4; ++z) for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 4; ++x)
			{
				assert(atlas[z * 256 * 1024 + (handle / 64 * 4 + y) * 256 + handle % 64 * 4 + x] == expected);
			}
		}
		resetPatches(frontEnd);
		assert(*reinterpret_cast<unsigned*>(frontEnd + 0x41938) == 0);
		assert(allocatePatch() == list);
		if (frame == 0) completedFirstFrame.assign(list, list + lightingPatchEntries * lightingPatchStride);
	}

	// Keep the fatal guard at the expanded boundary; isolate that expected exit.
	char executable[MAX_PATH];
	assert(GetModuleFileNameA(nullptr, executable, MAX_PATH));
	for (const auto childMode : {"--stock-patch-overflow", "--patch-overflow", "--patch-inline-overflow"})
	{
		auto childCommand = std::string("\"") + executable + "\" \"" + argv[1] + "\" " + childMode;
		STARTUPINFOA startup{}; startup.cb = sizeof(startup);
		PROCESS_INFORMATION child{};
		assert(CreateProcessA(nullptr, childCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child));
		assert(WaitForSingleObject(child.hProcess, 30000) == WAIT_OBJECT_0);
		DWORD childExit = 0;
		assert(GetExitCodeProcess(child.hProcess, &childExit) && childExit == 42);
		CloseHandle(child.hThread); CloseHandle(child.hProcess);
	}

	// The real queue initializer must derive counts from the enlarged allocations.
	std::array<std::uint8_t, 32> dvar{};
	At<void*>(0x1bd9b30) = dvar.data();
	reinterpret_cast<void(*)()>(Utils::Hook::image + 0x473c0)();
	const auto addWorker = reinterpret_cast<void(*)(int, const void*)>(Utils::Hook::image + 0x47700);
	for (const auto type : {9, 11})
	{
		const auto desc = 0x9470180 + type * 128;
		const auto stride = At<unsigned>(desc + 0x14);
		assert(stride == (type == 9 ? 24u : 32u));
		assert(At<unsigned>(desc + 0x24) == 4096);
		std::array<unsigned, 8> payload{};
		for (unsigned i = 0; i < 4096; ++i)
		{
			payload.fill(i);
			addWorker(type, payload.data());
		}
		auto* data = At<std::uint8_t*>(desc + 0x18);
		for (unsigned i = 0; i < 4096; ++i) assert(*reinterpret_cast<unsigned*>(data + i * stride) == i);
		At<void*>(0x3ced60 + type * 8) = nullptr;
		addWorker(type, payload.data());
		assert(At<unsigned>(desc + 0xc) == 4096);
	}
	assert(warnings[51] == 2 && workerFallbacks == 2);

	// Compare native encoded words before/after redirection, without changing packing.
	const auto append = reinterpret_cast<void(*)(RendererBudgets::MaterialStream*, unsigned, std::uint16_t)>(Utils::Hook::image + 0x5efe0);
	std::vector<std::uint16_t> stockWords(512), grownWords(512);
	auto stock = Stream(stockWords), grown = Stream(grownWords);
	for (unsigned i = 0; i < 160; ++i)
	{
		const unsigned key = (i / 5 * 10331) & 0x3fffffff;
		append(&stock, key, static_cast<std::uint16_t>(i));
		RendererBudgets::AppendMaterial(&grown, key, static_cast<std::uint16_t>(i));
	}
	assert(stock.cursor - stock.begin == grown.cursor - grown.begin);
	assert(std::equal(stock.begin, stock.cursor, grown.begin));
	assert(stock.count == grown.count && stock.key == grown.key);
	assert(RendererBudgets::RaiseMaterialStreams());
	assert(!RendererBudgets::RaiseMaterialStreams());

	// Exercise an actual patched call site; its downstream native encoder is unchanged.
	std::vector<std::uint16_t> hookWords(512);
	auto hooked = Stream(hookWords);
	const auto target = 0x5f1bd + 5 + At<std::int32_t>(0x5f1be);
	auto throughHook = reinterpret_cast<void(*)(RendererBudgets::MaterialStream*, unsigned, std::uint16_t)>(Utils::Hook::image + target);
	for (unsigned i = 0; i < 2600; ++i) throughHook(&hooked, i, static_cast<std::uint16_t>(i));
	assert(hooked.cursor - hooked.begin == 7800 && warnings[15] == 0);
	assert(std::all_of(hookWords.begin(), hookWords.end(), [](auto word) { return word == 0; }));
	const auto reusedBegin = hooked.begin;
	auto reused = Stream(hookWords);
	throughHook(&reused, 100, 200);
	assert(reused.begin == reusedBegin);

	// A bounded allocation failure must preserve the original stream unchanged.
	std::vector<std::uint16_t> boundedWords(512);
	auto bounded = Stream(boundedWords);
	const auto used = RendererBudgets::streamBytes;
	RendererBudgets::streamBytes = RendererBudgets::streamMemoryLimit;
	RendererBudgets::GrowMaterialStream(&bounded);
	assert(bounded.begin == boundedWords.data() && RendererBudgets::streamFallbacks == 1);
	RendererBudgets::streamBytes = used;
	std::array<std::vector<std::uint16_t>, 4> threadWords;
	std::vector<std::thread> threads;
	for (auto& words : threadWords)
	{
		words.resize(512);
		threads.emplace_back([&words, throughHook]
		{
			auto stream = Stream(words);
			for (unsigned i = 0; i < 2600; ++i) throughHook(&stream, i, static_cast<std::uint16_t>(i));
			assert(stream.cursor - stream.begin == 7800);
		});
	}
	for (auto& thread : threads) thread.join();
	assert(warnings[15] == 0);

	// The command arena and its six backend decoders are one indivisible format.
	for (const auto site : {0x5f369, 0x5f4c0, 0x5f663, 0x5f7c1, 0x5f9ad, 0x5fb78,
		0x7672e, 0x76803, 0x76b2f, 0x7b159, 0x7b33f, 0x7b4b2})
	{
		At<std::uint8_t>(site) ^= 1;
		std::vector<std::uint8_t> unchanged(code.size());
		std::memcpy(unchanged.data(), reinterpret_cast<void*>(Utils::Hook::image + 0x1000), code.size());
		assert(!RendererBudgets::RaiseModelCommands());
		assert(RendererBudgets::modelCommandStorage == nullptr);
		assert(std::memcmp(unchanged.data(), reinterpret_cast<void*>(Utils::Hook::image + 0x1000), code.size()) == 0);
		At<std::uint8_t>(site) ^= 1;
	}
	assert(RendererBudgets::RaiseModelCommands());
	assert(!RendererBudgets::RaiseModelCommands());
	std::array<std::uint8_t, 0x198> commandModel{};
	std::array<std::uint8_t, 16 * 88> commandSurfaces{};
	*reinterpret_cast<void**>(commandModel.data() + 0x88) = commandSurfaces.data();
	*reinterpret_cast<std::uint16_t*>(commandModel.data() + 0x64) = 16;
	std::array<std::uint16_t, 128> commandIds{};
	for (unsigned i = 0; i < commandIds.size(); ++i) commandIds[i] = static_cast<std::uint16_t>(i + 60000);
	using Command = unsigned(*)(void*, unsigned, const std::uint16_t*, unsigned, RendererBudgets::ModelCommandStream*, unsigned*, std::uint16_t*);
	const auto stockCommand = reinterpret_cast<Command>(Utils::Hook::image + 0x5ed40);
	const auto hookTarget = Utils::Hook::image + 0x5f369 + 5 + At<std::int32_t>(0x5f36a);
	const auto commandHook = reinterpret_cast<Command>(hookTarget);
	std::vector<std::uint8_t> expectedCommand(512);
	RendererBudgets::ModelCommandStream expectedStream{0, 0, expectedCommand.data(), 0, 512};
	unsigned commandFlags = 0; std::uint16_t commandToken = 0;
	assert(stockCommand(commandModel.data(), 0, commandIds.data(), 128, &expectedStream, &commandFlags, &commandToken) == 128);
	const auto nativeRecordBytes = expectedStream.used;
	for (unsigned frame = 0; frame < 2; ++frame)
	{
		auto* original = reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + frame * backEndDataStride + 0xd2f80);
		RendererBudgets::ModelCommandStream stream{0, 0, original, 0, 0x4000};
		for (unsigned slot = 0; slot < modelCommandSlots; ++slot)
		{
			assert(commandHook(commandModel.data(), 0, commandIds.data(), 128, &stream, &commandFlags, &commandToken) == 128);
			assert(commandToken == slot * 4 && stream.used == (slot + 1) * modelCommandSlotBytes);
			assert(std::equal(expectedCommand.begin(), expectedCommand.begin() + nativeRecordBytes, stream.data + slot * modelCommandSlotBytes));
		}
		const auto overflowBefore = warnings[14].load();
		assert(commandHook(commandModel.data(), 0, commandIds.data(), 128, &stream, &commandFlags, &commandToken) == 0);
		assert(warnings[14] == overflowBefore + 1 && stream.used == stream.capacity);
		// Reinitialization selects the same arena without sharing the other frame.
		auto* arena = stream.data;
		stream = {0, 0, original, 0, 0x4000};
		assert(commandHook(commandModel.data(), 0, commandIds.data(), 1, &stream, &commandFlags, &commandToken) == 1);
		assert(stream.data == arena && commandToken == 0);
	}
	assert(RendererBudgets::modelCommandRejectedStreams == 0);
	// Execute the actual cached/pretessellated producer too; only index copying
	// crosses into the host runtime. Its 16-byte header and copied indices stay native.
	std::array<std::uint8_t, 24> indexState{};
	std::vector<std::uint16_t> destinationIndices(4096);
	std::array<std::uint16_t, 3> sourceIndices{12, 34, 56};
	*reinterpret_cast<unsigned*>(indexState.data() + 4) = 4096;
	*reinterpret_cast<void**>(indexState.data() + 16) = destinationIndices.data();
	At<void*>(0x8cccdd8) = indexState.data();
	At<void*>(0x8cccd98) = sourceIndices.data();
	At<void*>(0x8cc6758) = dvar.data(); dvar[16] = 1;
	RedirectTarget(0x5ecc7, reinterpret_cast<void*>(Copy));
	*reinterpret_cast<std::uint16_t*>(commandModel.data() + 0x64) = 1;
	commandModel[0x60 + 0x32] = 1;
	*reinterpret_cast<std::uint16_t*>(commandSurfaces.data() + 4) = 1;
	std::fill(expectedCommand.begin(), expectedCommand.end(), std::uint8_t{});
	expectedStream.used = 0; commandFlags = 2;
	assert(stockCommand(commandModel.data(), 0, commandIds.data(), 128, &expectedStream, &commandFlags, &commandToken) == 128);
	assert(commandFlags == 3 && expectedStream.used == 16);
	assert(*reinterpret_cast<unsigned*>(indexState.data()) == 384);
	for (unsigned i = 0; i < 384; ++i) assert(destinationIndices[i] == sourceIndices[i % 3]);
	*reinterpret_cast<unsigned*>(indexState.data()) = 0;
	std::fill(destinationIndices.begin(), destinationIndices.end(), std::uint16_t{});
	RendererBudgets::ModelCommandStream cachedStream{0, 0, reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + 0xd2f80 + 0x4000), 0, 0x2000};
	commandFlags = 2;
	assert(commandHook(commandModel.data(), 0, commandIds.data(), 128, &cachedStream, &commandFlags, &commandToken) == 128);
	assert(commandFlags == 3 && cachedStream.used == modelCommandSlotBytes && commandToken == 0);
	assert(std::equal(expectedCommand.begin(), expectedCommand.begin() + 16, cachedStream.data));
	for (unsigned i = 0; i < 384; ++i) assert(destinationIndices[i] == sourceIndices[i % 3]);
	dvar[16] = 0;
	std::map<std::uintptr_t, unsigned> passArenas;
	for (unsigned frame = 0; frame < 2; ++frame) for (unsigned offset = 0; offset < 0x36728; offset += 0x800)
	{
		RendererBudgets::ModelCommandStream pass{0, 0, reinterpret_cast<std::uint8_t*>(Utils::Hook::image + 0x9145900 + frame * backEndDataStride + 0xd2f80 + offset), 0, 0x800};
		commandFlags = 0;
		assert(commandHook(commandModel.data(), 0, commandIds.data(), 1, &pass, &commandFlags, &commandToken) == 1);
		assert(passArenas.try_emplace(reinterpret_cast<std::uintptr_t>(pass.data), frame).second);
	}
	assert(passArenas.size() == 218 && RendererBudgets::modelCommandRejectedStreams == 0);
	// Execute each actual patched backend shift with the native token layout.
	const std::array<std::pair<unsigned, unsigned>, 6> decoderSites = {{{0x7672e,2},{0x76803,12},{0x76b2f,2},{0x7b159,3},{0x7b33f,2},{0x7b4b2,8}}};
	for (const auto [site, reg] : decoderSites)
	{
		std::vector<std::uint8_t> adapter;
		if (reg == 12) adapter = {0x41,0x54,0x41,0x89,0xcc,0x41,0xc1,0xec,0x04};
		else if (reg == 8) adapter = {0x41,0x89,0xc8,0x41,0xc1,0xe8,0x04};
		else if (reg == 3) adapter = {0x53,0x89,0xcb,0xc1,0xeb,0x04};
		else adapter = {0x89,0xca,0xc1,0xea,0x04};
		const auto bytes = reg >= 8 ? 4u : 3u;
		const auto* actual = reinterpret_cast<const std::uint8_t*>(Utils::Hook::image + site);
		adapter.insert(adapter.end(), actual, actual + bytes);
		if (reg == 12) adapter.insert(adapter.end(), {0x44,0x89,0xe0,0x41,0x5c});
		else if (reg == 8) adapter.insert(adapter.end(), {0x44,0x89,0xc0});
		else if (reg == 3) adapter.insert(adapter.end(), {0x89,0xd8,0x5b});
		else adapter.insert(adapter.end(), {0x89,0xd0});
		adapter.push_back(0xc3);
		auto* decoderCode = VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		assert(decoderCode);
		std::memcpy(decoderCode, adapter.data(), adapter.size());
		const auto decode = reinterpret_cast<unsigned(*)(unsigned)>(decoderCode);
		for (unsigned slot = 0; slot < modelCommandSlots; ++slot) for (unsigned surface = 0; surface < 16; ++surface)
		{
			const auto encoded = slot * 16 + surface;
			assert(decode(encoded) == slot * modelCommandSlotBytes);
		}
		VirtualFree(decoderCode, 0, MEM_RELEASE);
	}
	unsigned replaySubmitted = 0, replayExpected = 0, stockReplaySubmitted = 0, replayViews = 0, replayPeakRecords = 0, replayPeakModels = 0;
	if (argc == 4)
	{
		// Replay the real primary submission routine against archived placements.
		// Geometry/material headers are minimal fixtures; no GPU calls execute.
		auto fixture = Read(argv[3]);
		const auto count = *reinterpret_cast<const unsigned*>(fixture.data());
		const auto types = *reinterpret_cast<const unsigned*>(fixture.data() + 4);
		assert(count <= 65535 && types <= 65535 && fixture.size() == 8 + 256 + count * 125 + types * 4);
		std::memcpy(reinterpret_cast<void*>(Utils::Hook::image + 0x8cca400), fixture.data() + 8, 256);
		auto* sourceDraws = fixture.data() + 264;
		auto* sourceVis = sourceDraws + count * 88;
		auto* sourceTypes = sourceVis + count;
		auto* sourceBounds = sourceTypes + types * 4;
		std::vector<std::array<std::uint8_t, 0x198>> modelFixtures(types);
		std::vector<std::array<std::uint8_t, 16 * 88>> surfaceFixtures(types);
		std::array<std::uint8_t, 0x60> material{};
		std::array<void*, 16> materials{}; materials.fill(material.data());
		for (unsigned type = 0; type < types; ++type)
		{
			auto& header = modelFixtures[type];
			*reinterpret_cast<float*>(header.data() + 0x60) = 1000000.f;
			*reinterpret_cast<unsigned short*>(header.data() + 0x64) = static_cast<unsigned short>(*reinterpret_cast<const unsigned*>(sourceTypes + type * 4));
			*reinterpret_cast<void**>(header.data() + 0x88) = surfaceFixtures[type].data();
			*reinterpret_cast<void**>(header.data() + 0x58) = materials.data();
			header[0x141] = 1;
		}
		std::memcpy(draws.data(), sourceDraws, count * 88);
		for (unsigned i = 0; i < count; ++i)
		{
			auto* draw = draws.data() + i * 88;
			const auto type = *reinterpret_cast<std::uint64_t*>(draw + 56);
			assert(type < types);
			*reinterpret_cast<void**>(draw + 56) = modelFixtures[type].data();
			*reinterpret_cast<std::uint16_t*>(draw + 66) = 1;
			std::memset(draw + 76, 0, 8);
		}
		std::vector<std::uint8_t> visibility(sourceVis, sourceVis + count);
		*reinterpret_cast<unsigned*>(world.data()) = count;
		*reinterpret_cast<void**>(world.data() + 0x38) = visibility.data();
		At<void*>(0x8cc6aa0) = dvar.data();
		At<void*>(0x8c28500) = reinterpret_cast<void*>(Utils::Hook::image + 0x9145900);
		At<unsigned>(0x9145900 + 0x41968) = 0;
		At<unsigned>(0x9145900 + 0x41964) = 0;
		auto* arena = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(0x14005FDF1, 0x40000));
		assert(arena);
		const auto fixtureDelta = reinterpret_cast<std::int64_t>(arena) - static_cast<std::int64_t>(Utils::Hook::image + 0x9145900);
		assert(fixtureDelta >= INT32_MIN && fixtureDelta <= INT32_MAX);
		At<std::int32_t>(0x5fdf4) = static_cast<std::int32_t>(fixtureDelta);
		std::array<std::uint8_t, 0x1000> view{};
		const std::array<unsigned, 6> producerSites{0x5f369,0x5f4c0,0x5f663,0x5f7c1,0x5f9ad,0x5fb78};
		std::array<std::array<std::uint8_t, 5>, 6> savedProducerCalls{};
		for (unsigned i = 0; i < producerSites.size(); ++i)
		{
			auto* call = reinterpret_cast<void*>(Utils::Hook::image + producerSites[i]);
			std::memcpy(savedProducerCalls[i].data(), call, 5);
			std::memcpy(call, code.data() + producerSites[i] - 0x1000, 5);
		}
		std::memset(reinterpret_cast<void*>(Utils::Hook::image + 0x9145900 + 0xd2f80), 0, 0x4000);
		warnings[14] = 0;
		reinterpret_cast<void(*)(void*)>(Utils::Hook::image + 0x5fce0)(view.data());
		bool stockHasCrane = false;
		for (unsigned at = 0; at + 12 <= 0x4000;)
		{
			const auto* record = reinterpret_cast<const std::uint8_t*>(Utils::Hook::image + 0x9145900 + 0xd2f80 + at);
			const auto n = *reinterpret_cast<const std::uint16_t*>(record + 8);
			if (!n) break;
			const auto bytes = (n * 2 + 13) & ~3u;
			assert(n <= 128 && at + bytes <= 0x4000);
			for (unsigned j = 0; j < n; ++j) stockHasCrane |= *reinterpret_cast<const std::uint16_t*>(record + 10 + j * 2) == 56202;
			stockReplaySubmitted += n; at += bytes;
		}
		std::cerr << "Stock replay: submitted " << stockReplaySubmitted << ", warning14 " << warnings[14] << ", crane " << stockHasCrane << '\n';
		assert(warnings[14] > 0);
		for (unsigned i = 0; i < producerSites.size(); ++i) std::memcpy(reinterpret_cast<void*>(Utils::Hook::image + producerSites[i]), savedProducerCalls[i].data(), 5);
		visibility.assign(sourceVis, sourceVis + count);
		view.fill(0);
		At<unsigned>(0x9145900 + 0x41968) = 0;
		At<unsigned>(0x9145900 + 0x41964) = 0;
		warnings[14] = 0; warnings[15] = 0;
		std::memset(RendererBudgets::modelCommandStorage, 0, modelCommandPassBytes);
		RendererBudgets::modelCommandPeakRecords = 0;
		reinterpret_cast<void(*)(void*)>(Utils::Hook::image + 0x5fce0)(view.data());
		const auto* records = RendererBudgets::modelCommandStorage;
		for (unsigned slot = 0; slot < RendererBudgets::modelCommandPeakRecords; ++slot)
		{
			const auto* record = records + slot * modelCommandSlotBytes;
			const auto n = *reinterpret_cast<const std::uint16_t*>(record + 8);
			if (!n) break;
			for (unsigned j = 0; j < n; ++j)
			{
				const auto index = *reinterpret_cast<const std::uint16_t*>(record + 10 + j * 2);
				assert(index < count && visibility[index]);
				visibility[index] = 2; ++replaySubmitted;
			}
		}
		replayExpected = static_cast<unsigned>(std::count_if(visibility.begin(), visibility.end(), [](auto value) { return value != 0; }));
		assert(replaySubmitted == replayExpected && replayExpected > 3698 && visibility[56202] == 2);
		assert(stockReplaySubmitted < replaySubmitted);
		assert(warnings[14] == 0 && warnings[15] == 0);
		// Use the archived bounds to test eight complete camera frustums at each
		// reported problem area, then let the actual native distance test decide.
		for (const auto eye : {std::array<float,3>{857,-5332,152}, {2923,-5110,228}, {2627,-3059,-244}, {1704,-4087,329},
			{3502,-5117,287}, {2740,-5340,170}, {1853,-2721,-271}, {1500,-5117,287}, {500,-5340,170}})
		{
		for (unsigned heading = 0; heading < 8; ++heading)
		{
			const auto angle = heading * 3.14159265358979323846 / 4;
			const std::array<double,3> forward{std::cos(angle),std::sin(angle),0};
			const std::array<double,3> right{-forward[1],forward[0],0};
			std::array<std::array<double,3>,5> planes{};
			planes[0] = forward;
			for (unsigned axis = 0; axis < 3; ++axis)
			{
				planes[1][axis] = forward[axis] * 1.333333333 - right[axis];
				planes[2][axis] = forward[axis] * 1.333333333 + right[axis];
				planes[3][axis] = forward[axis] * .75 - (axis == 2 ? 1 : 0);
				planes[4][axis] = forward[axis] * .75 + (axis == 2 ? 1 : 0);
			}
			for (unsigned i = 0; i < count; ++i)
			{
				const auto* bounds = reinterpret_cast<const float*>(sourceBounds + i * 36);
				visibility[i] = 1;
				for (const auto& plane : planes)
				{
					double distance = 0;
					for (unsigned axis = 0; axis < 3; ++axis) distance += plane[axis] * (bounds[axis] - eye[axis]) + std::abs(plane[axis]) * bounds[axis + 3];
					if (distance < 0) { visibility[i] = 0; break; }
				}
			}
			At<float>(0x8cca474) = eye[0]; At<float>(0x8cca478) = eye[1]; At<float>(0x8cca47c) = eye[2];
			At<unsigned>(0x9145900 + 0x41968) = 0; At<unsigned>(0x9145900 + 0x41964) = 0;
			view.fill(0); warnings[14] = 0; warnings[15] = 0;
			std::memset(RendererBudgets::modelCommandStorage, 0, modelCommandPassBytes);
			RendererBudgets::modelCommandPeakRecords = 0;
			reinterpret_cast<void(*)(void*)>(Utils::Hook::image + 0x5fce0)(view.data());
			unsigned submitted = 0;
			for (unsigned slot = 0; slot < RendererBudgets::modelCommandPeakRecords; ++slot)
			{
				const auto* record = RendererBudgets::modelCommandStorage + slot * modelCommandSlotBytes;
				const auto n = *reinterpret_cast<const std::uint16_t*>(record + 8);
				for (unsigned j = 0; j < n; ++j)
				{
					const auto index = *reinterpret_cast<const std::uint16_t*>(record + 10 + j * 2);
					assert(index < count && visibility[index] == 1);
					visibility[index] = 2; ++submitted;
				}
			}
			const auto admitted = std::count_if(visibility.begin(), visibility.end(), [](auto value) { return value != 0; });
			++replayViews;
			replayPeakModels = (std::max)(replayPeakModels, static_cast<unsigned>(admitted));
			replayPeakRecords = (std::max)(replayPeakRecords, RendererBudgets::modelCommandPeakRecords.load());
			assert(submitted == admitted && warnings[14] == 0 && warnings[15] == 0);
		}
		}
	}
	const auto result = std::string("{\"passed\":true,\"nativeLightingConfigs\":3,\"largestLightingCapacity\":") + std::to_string(largestCapacity)
		+ ",\"nativeEvictionTests\":3,\"lightingPatchCapacity\":24576,\"nativeLightingPatchWrites\":49152,\"nativeAtlasUploads\":2,\"stockPatchOverflowReproduced\":true,\"nativePatchOverflowGuards\":2,\"nativeFrameReset\":true,\"workerQueueCommands\":8192,\"queueOverflowFallbacks\":2,\"nativeMaterialPackingCompared\":160,\"expandedMaterialWrites\":13000,\"concurrentStreams\":4,\"nativeCommandRecords\":8192,\"nativeCommandRecordParity\":true,\"nativeCommandDecoderCases\":393216,\"modelReplaySubmitted\":"
		+ std::to_string(replaySubmitted) + ",\"modelReplayExpected\":" + std::to_string(replayExpected) + ",\"stockModelReplaySubmitted\":" + std::to_string(stockReplaySubmitted)
		+ ",\"nativePretessCommandParity\":true,\"nativeCommandPassArenas\":218,\"nativeFrustumReplayViews\":" + std::to_string(replayViews) + ",\"nativeFrustumReplayPeakRecords\":" + std::to_string(replayPeakRecords) + ",\"nativeFrustumReplayPeakModels\":" + std::to_string(replayPeakModels) + ",\"gameStarted\":false}";
	std::cout << result << '\n';
	if (argc >= 3) { std::ofstream report(argv[2]); assert(report.good()); report << result << '\n'; }
}
