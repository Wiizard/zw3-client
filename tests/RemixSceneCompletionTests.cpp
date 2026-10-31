#include <array>
#include <iostream>
#include <stdexcept>

using DWORD = unsigned long;
constexpr DWORD D3DCOLORWRITEENABLE_RED = 1;
constexpr DWORD D3DCOLORWRITEENABLE_GREEN = 2;
constexpr DWORD D3DCOLORWRITEENABLE_BLUE = 4;
constexpr int D3DSBT_ALL = 1, D3DTS_WORLD = 1;
constexpr int D3DRS_LIGHTING = 0, D3DRS_ALPHABLENDENABLE = 1, D3DRS_COLORWRITEENABLE = 2;
constexpr int D3DTSS_TEXTURETRANSFORMFLAGS = 1, D3DTSS_COLOROP = 2, D3DTSS_ALPHAOP = 3;
constexpr DWORD FALSE = 0, D3DTTFF_DISABLE = 0, D3DTOP_DISABLE = 0;
constexpr DWORD worldVertexFormat = 0x152;
constexpr int identity = 1;
constexpr bool FAILED(int result) { return result < 0; }

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

struct IDirect3DDevice9;
struct StateBlock
{
	IDirect3DDevice9* device = nullptr;
	std::array<DWORD, 3> saved{};
	void Capture();
	void Apply();
};
struct IDirect3DDevice9
{
	std::array<DWORD, 3> states{1, 1, 0};
	StateBlock block{this, {}};
	bool failsStateBlock = false;
	int CreateStateBlock(int, StateBlock** out)
	{
		if (failsStateBlock) return -1;
		*out = &block;
		return 0;
	}
	void SetVertexShader(void*) {}
	void SetPixelShader(void*) {}
	void SetFVF(DWORD) {}
	void SetTransform(int, const int*) {}
	void SetRenderState(int state, DWORD value) { states[state] = value; }
	void SetTextureStageState(int, int, DWORD) {}
};
void StateBlock::Capture() { saved = device->states; }
void StateBlock::Apply() { device->states = saved; }

static unsigned sceneNumber = 1, culledFillScene = 0, drawn = 0;
static bool buffersReady = true;
static StateBlock* completeSceneState = nullptr;
static int materialBatches, batchVertexBuffer, batchIndexBuffer;
static int staticModelBatches, staticBatchVertexBuffer, staticBatchIndexBuffer;
static struct { unsigned completedWorldBatches, completedPropBatches, completedBigProps; } stats;
static bool EnsureWorldBuffers(IDirect3DDevice9*) { return buffersReady; }
static void EnsureStaticModelBatches(IDirect3DDevice9*) {}
static void CheckRaytracedDraw(IDirect3DDevice9* device)
{
	// Mirrors Remix's external draw acceptance rule, rather than the DLL's state-setting implementation.
	Require((device->states[D3DRS_COLORWRITEENABLE] & 7) == 7,
		"Remix would ignore completed walls: RGB writes are disabled");
	Require(device->states[D3DRS_ALPHABLENDENABLE] == FALSE, "Opaque completion inherited glass blending");
	++drawn;
}
static unsigned DrawKnownBatches(IDirect3DDevice9* device, int, int, int, const char*)
{
	CheckRaytracedDraw(device);
	return 1;
}
static unsigned DrawCulledBigProps(IDirect3DDevice9* device)
{
	CheckRaytracedDraw(device);
	return 1;
}

#include "SceneCompletion.inc"

int main()
{
	try
	{
		IDirect3DDevice9 device;
		const auto original = device.states;
		DrawCulledBatches(&device);
		Require(drawn == 3, "World, static props and large props did not complete");
		Require(device.states == original, "Completion leaked colour or blend state into post-processing");
		DrawCulledBatches(&device);
		Require(drawn == 3, "The same scene was submitted twice");
		++sceneNumber;
		buffersReady = false;
		DrawCulledBatches(&device);
		DrawCulledBatches(nullptr);
		Require(drawn == 3 && device.states == original, "An unavailable scene changed rendering state");
		buffersReady = true;
		completeSceneState = nullptr;
		device.failsStateBlock = true;
		DrawCulledBatches(&device);
		Require(drawn == 3 && device.states == original, "A state-block failure changed rendering state");
		++sceneNumber;
		device.failsStateBlock = false;
		DrawCulledBatches(&device);
		Require(drawn == 6 && device.states == original, "Completion did not recover on the next scene");
		std::cout << "Remix scene completion: depth-only input, RGB capture, state restoration and retry passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
