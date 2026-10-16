
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace Game {
struct Value { bool enabled = false; int integer = 0; };
struct dvar_t { Value current; };
dvar_t opt, bump, spec;
bool hasBump = true, hasSpec = true;
int writes = 0;
constexpr int CON_CHANNEL_SYSTEM = 0;
dvar_t* Dvar_FindVar(const char* name) {
 if (!std::strcmp(name, "zw3_mem_allowHighPicmip")) return &opt;
 if (!std::strcmp(name, "r_picmip_bump")) return hasBump ? &bump : nullptr;
 if (!std::strcmp(name, "r_picmip_spec")) return hasSpec ? &spec : nullptr;
 return nullptr;
}
void Dvar_SetInt(dvar_t* dvar, int value) { dvar->current.integer = value; ++writes; }
}
namespace Components {
struct Logger { template<class... T> static void Print(T...) {} };
struct MemoryTuning { static void EnforcePicmipFloors(); };
constexpr int kPicmipFloorBump = 2;
constexpr int kPicmipFloorSpec = 2;
	void MemoryTuning::EnforcePicmipFloors()
	{
		// Respect the opt-out.
		if (const auto* opt = Game::Dvar_FindVar("zw3_mem_allowHighPicmip"))
		{
			if (opt->current.enabled) return;
		}

		struct { const char* name; int floor; int* rendererValue; } picmips[] = {
			{ "r_picmip_bump", kPicmipFloorBump, reinterpret_cast<int*>(0x6C41D28) },
			{ "r_picmip_spec", kPicmipFloorSpec, reinterpret_cast<int*>(0x6C41D2C) },
		};

		for (const auto& p : picmips)
		{
			auto* dvar = Game::Dvar_FindVar(p.name);
			if (dvar == nullptr) continue;

			const auto value = std::max(*p.rendererValue, p.floor);
			if (*p.rendererValue < p.floor)
			{
				Logger::Print(Game::CON_CHANNEL_SYSTEM,
					"MemoryTuning: raising calculated {} from {} to {} (address-space safety)\n",
					p.name, *p.rendererValue, value);
			}
			*p.rendererValue = value;
			if (dvar->current.integer != value)
				Game::Dvar_SetInt(dvar, value);
		}
	}

}
int* values = reinterpret_cast<int*>(0x6C41D24);
void check(bool ok, const char* name) { if (!ok) { std::printf("FAIL: %s\n", name); std::exit(1); } }
void reset(int b, int s, int db, int ds, bool opt = false) {
 values[0] = 0; values[1] = b; values[2] = s;
 Game::bump.current.integer = db; Game::spec.current.integer = ds;
 Game::opt.current.enabled = opt; Game::hasBump = Game::hasSpec = true; Game::writes = 0;
}
void apply() { Components::MemoryTuning::EnforcePicmipFloors(); }
int main() {
 check(VirtualAlloc(reinterpret_cast<void*>(0x6C41000), 4096, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE) != nullptr, "allocate renderer cache");
 reset(0, 0, 0, 0); apply(); check(values[1]==2 && values[2]==2 && Game::bump.current.integer==2 && Game::spec.current.integer==2, "automatic zero corrected in both caches and dvars");
 reset(1, 1, 1, 1); apply(); check(values[1]==2 && values[2]==2, "automatic one corrected");
 reset(2, 3, 2, 3); apply(); check(values[1]==2 && values[2]==3 && Game::writes==0, "existing lower texture quality preserved");
 reset(0, 0, 2, 2); apply(); check(values[1]==2 && values[2]==2 && Game::writes==0, "stale renderer cache corrected despite already-correct dvars");
 reset(3, 3, 0, 0); apply(); check(Game::bump.current.integer==3 && Game::spec.current.integer==3, "dvars synchronized with calculated values");
 reset(0, 1, 0, 1, true); apply(); check(values[1]==0 && values[2]==1 && Game::writes==0, "opt-out preserved");
 reset(0, 0, 0, 0); Game::hasBump=false; apply(); check(values[1]==0 && values[2]==2, "missing dvar handled independently");
 reset(0, 0, 0, 0); apply(); check(values[0]==0, "diffuse left unchanged");
 values[1]=values[2]=0; Game::bump.current.integer=Game::spec.current.integer=0; apply(); check(values[1]==2 && values[2]==2, "subsequent recalculation corrected");
 std::puts("PASS: 9 cases using EnforcePicmipFloors extracted unchanged from production source.");
}
