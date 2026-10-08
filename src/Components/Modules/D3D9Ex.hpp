#pragma once

#include "Dvar.hpp"

namespace Components
{
	class D3D9Ex : public Component
	{
	public:
		D3D9Ex();

		static void LoadTexture(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image);

		static bool IsD3D9ExEnabled();

		static void BeginMapLoading(const std::string& map);

	private:
		class D3D9Device : public IDirect3DDevice9
		{
		public:
			D3D9Device(IDirect3DDevice9* original) : device(original) {}
			virtual ~D3D9Device() = default;

			HRESULT WINAPI QueryInterface(REFIID riid, void** object) override;
			ULONG WINAPI AddRef() override;
			ULONG WINAPI Release() override;
			HRESULT WINAPI TestCooperativeLevel() override;
			UINT WINAPI GetAvailableTextureMem() override;
			HRESULT WINAPI EvictManagedResources() override;
			HRESULT WINAPI GetDirect3D(IDirect3D9** direct3D) override;
			HRESULT WINAPI GetDeviceCaps(D3DCAPS9* caps) override;
			HRESULT WINAPI GetDisplayMode(UINT swapChainIndex, D3DDISPLAYMODE* mode) override;
			HRESULT WINAPI GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* parameters) override;
			HRESULT WINAPI SetCursorProperties(UINT xHotSpot, UINT yHotSpot, IDirect3DSurface9* cursorBitmap) override;
			void WINAPI SetCursorPosition(int x, int y, DWORD flags) override;
			BOOL WINAPI ShowCursor(BOOL shouldShow) override;
			HRESULT WINAPI CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DSwapChain9** swapChain) override;
			HRESULT WINAPI GetSwapChain(UINT swapChainIndex, IDirect3DSwapChain9** swapChain) override;
			UINT WINAPI GetNumberOfSwapChains() override;
			HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS* presentationParameters) override;
			HRESULT WINAPI Present(const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion) override;
			HRESULT WINAPI GetBackBuffer(UINT swapChainIndex, UINT backBufferIndex, D3DBACKBUFFER_TYPE type, IDirect3DSurface9** backBuffer) override;
			HRESULT WINAPI GetRasterStatus(UINT swapChainIndex, D3DRASTER_STATUS* rasterStatus) override;
			HRESULT WINAPI SetDialogBoxMode(BOOL shouldEnableDialogs) override;
			void WINAPI SetGammaRamp(UINT swapChainIndex, DWORD flags, const D3DGAMMARAMP* ramp) override;
			void WINAPI GetGammaRamp(UINT swapChainIndex, D3DGAMMARAMP* ramp) override;
			HRESULT WINAPI CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateVolumeTexture(UINT width, UINT height, UINT depth, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DVolumeTexture9** volumeTexture, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateCubeTexture(UINT edgeLength, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture9** cubeTexture, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9** vertexBuffer, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateRenderTarget(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL isLockable, IDirect3DSurface9** surface, HANDLE* sharedHandle) override;
			HRESULT WINAPI CreateDepthStencilSurface(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL shouldDiscard, IDirect3DSurface9** surface, HANDLE* sharedHandle) override;
			HRESULT WINAPI UpdateSurface(IDirect3DSurface9* sourceSurface, const RECT* sourceRect, IDirect3DSurface9* destinationSurface, const POINT* destPoint) override;
			HRESULT WINAPI UpdateTexture(IDirect3DBaseTexture9* sourceTexture, IDirect3DBaseTexture9* destinationTexture) override;
			HRESULT WINAPI GetRenderTargetData(IDirect3DSurface9* renderTarget, IDirect3DSurface9* destSurface) override;
			HRESULT WINAPI GetFrontBufferData(UINT swapChainIndex, IDirect3DSurface9* destSurface) override;
			HRESULT WINAPI StretchRect(IDirect3DSurface9* sourceSurface, const RECT* sourceRect, IDirect3DSurface9* destSurface, const RECT* destRect, D3DTEXTUREFILTERTYPE filter) override;
			HRESULT WINAPI ColorFill(IDirect3DSurface9* surface, const RECT* rect, D3DCOLOR color) override;
			HRESULT WINAPI CreateOffscreenPlainSurface(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, IDirect3DSurface9** surface, HANDLE* sharedHandle) override;
			HRESULT WINAPI SetRenderTarget(DWORD renderTargetIndex, IDirect3DSurface9* renderTarget) override;
			HRESULT WINAPI GetRenderTarget(DWORD renderTargetIndex, IDirect3DSurface9** renderTarget) override;
			HRESULT WINAPI SetDepthStencilSurface(IDirect3DSurface9* newZStencil) override;
			HRESULT WINAPI GetDepthStencilSurface(IDirect3DSurface9** zStencilSurface) override;
			HRESULT WINAPI BeginScene() override;
			HRESULT WINAPI EndScene() override;
			HRESULT WINAPI Clear(DWORD count, const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) override;
			HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override;
			HRESULT WINAPI GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX* matrix) override;
			HRESULT WINAPI MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) override;
			HRESULT WINAPI SetViewport(const D3DVIEWPORT9* viewport) override;
			HRESULT WINAPI GetViewport(D3DVIEWPORT9* viewport) override;
			HRESULT WINAPI SetMaterial(const D3DMATERIAL9* material) override;
			HRESULT WINAPI GetMaterial(D3DMATERIAL9* material) override;
			HRESULT WINAPI SetLight(DWORD index, const D3DLIGHT9* light) override;
			HRESULT WINAPI GetLight(DWORD index, D3DLIGHT9* light) override;
			HRESULT WINAPI LightEnable(DWORD index, BOOL isEnabled) override;
			HRESULT WINAPI GetLightEnable(DWORD index, BOOL* isEnabled) override;
			HRESULT WINAPI SetClipPlane(DWORD index, const float* plane) override;
			HRESULT WINAPI GetClipPlane(DWORD index, float* plane) override;
			HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override;
			HRESULT WINAPI GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) override;
			HRESULT WINAPI CreateStateBlock(D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9** stateBlock) override;
			HRESULT WINAPI BeginStateBlock() override;
			HRESULT WINAPI EndStateBlock(IDirect3DStateBlock9** stateBlock) override;
			HRESULT WINAPI SetClipStatus(const D3DCLIPSTATUS9* clipStatus) override;
			HRESULT WINAPI GetClipStatus(D3DCLIPSTATUS9* clipStatus) override;
			HRESULT WINAPI GetTexture(DWORD stage, IDirect3DBaseTexture9** texture) override;
			HRESULT WINAPI SetTexture(DWORD stage, IDirect3DBaseTexture9* texture) override;
			HRESULT WINAPI GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* value) override;
			HRESULT WINAPI SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override;
			HRESULT WINAPI GetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD* value) override;
			HRESULT WINAPI SetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value) override;
			HRESULT WINAPI ValidateDevice(DWORD* numPasses) override;
			HRESULT WINAPI SetPaletteEntries(UINT paletteNumber, const PALETTEENTRY* entries) override;
			HRESULT WINAPI GetPaletteEntries(UINT paletteNumber, PALETTEENTRY* entries) override;
			HRESULT WINAPI SetCurrentTexturePalette(UINT paletteNumber) override;
			HRESULT WINAPI GetCurrentTexturePalette(UINT* paletteNumber) override;
			HRESULT WINAPI SetScissorRect(const RECT* rect) override;
			HRESULT WINAPI GetScissorRect(RECT* rect) override;
			HRESULT WINAPI SetSoftwareVertexProcessing(BOOL isSoftware) override;
			BOOL WINAPI GetSoftwareVertexProcessing() override;
			HRESULT WINAPI SetNPatchMode(float segments) override;
			float WINAPI GetNPatchMode() override;
			HRESULT WINAPI DrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount) override;
			HRESULT WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, INT baseVertexIndex, UINT minVertexIndex, UINT numVertices, UINT startIndex, UINT primCount) override;
			HRESULT WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT primitiveCount, const void* vertexStreamZeroData, UINT vertexStreamZeroStride) override;
			HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minVertexIndex, UINT numVertices, UINT primitiveCount, const void* indexData, D3DFORMAT indexDataFormat, const void* vertexStreamZeroData, UINT vertexStreamZeroStride) override;
			HRESULT WINAPI ProcessVertices(UINT srcStartIndex, UINT destIndex, UINT vertexCount, IDirect3DVertexBuffer9* destBuffer, IDirect3DVertexDeclaration9* vertexDecl, DWORD flags) override;
			HRESULT WINAPI CreateVertexDeclaration(const D3DVERTEXELEMENT9* vertexElements, IDirect3DVertexDeclaration9** decl) override;
			HRESULT WINAPI SetVertexDeclaration(IDirect3DVertexDeclaration9* decl) override;
			HRESULT WINAPI GetVertexDeclaration(IDirect3DVertexDeclaration9** decl) override;
			HRESULT WINAPI SetFVF(DWORD fvf) override;
			HRESULT WINAPI GetFVF(DWORD* fvf) override;
			HRESULT WINAPI CreateVertexShader(const DWORD* function, IDirect3DVertexShader9** shader) override;
			HRESULT WINAPI SetVertexShader(IDirect3DVertexShader9* shader) override;
			HRESULT WINAPI GetVertexShader(IDirect3DVertexShader9** shader) override;
			HRESULT WINAPI SetVertexShaderConstantF(UINT startRegister, const float* constantData, UINT vector4fCount) override;
			HRESULT WINAPI GetVertexShaderConstantF(UINT startRegister, float* constantData, UINT vector4fCount) override;
			HRESULT WINAPI SetVertexShaderConstantI(UINT startRegister, const int* constantData, UINT vector4iCount) override;
			HRESULT WINAPI GetVertexShaderConstantI(UINT startRegister, int* constantData, UINT vector4iCount) override;
			HRESULT WINAPI SetVertexShaderConstantB(UINT startRegister, const BOOL* constantData, UINT boolCount) override;
			HRESULT WINAPI GetVertexShaderConstantB(UINT startRegister, BOOL* constantData, UINT boolCount) override;
			HRESULT WINAPI SetStreamSource(UINT streamNumber, IDirect3DVertexBuffer9* streamData, UINT offsetInBytes, UINT stride) override;
			HRESULT WINAPI GetStreamSource(UINT streamNumber, IDirect3DVertexBuffer9** streamData, UINT* offsetInBytes, UINT* stride) override;
			HRESULT WINAPI SetStreamSourceFreq(UINT streamNumber, UINT divider) override;
			HRESULT WINAPI GetStreamSourceFreq(UINT streamNumber, UINT* divider) override;
			HRESULT WINAPI SetIndices(IDirect3DIndexBuffer9* indexData) override;
			HRESULT WINAPI GetIndices(IDirect3DIndexBuffer9** indexData) override;
			HRESULT WINAPI CreatePixelShader(const DWORD* function, IDirect3DPixelShader9** shader) override;
			HRESULT WINAPI SetPixelShader(IDirect3DPixelShader9* shader) override;
			HRESULT WINAPI GetPixelShader(IDirect3DPixelShader9** shader) override;
			HRESULT WINAPI SetPixelShaderConstantF(UINT startRegister, const float* constantData, UINT vector4fCount) override;
			HRESULT WINAPI GetPixelShaderConstantF(UINT startRegister, float* constantData, UINT vector4fCount) override;
			HRESULT WINAPI SetPixelShaderConstantI(UINT startRegister, const int* constantData, UINT vector4iCount) override;
			HRESULT WINAPI GetPixelShaderConstantI(UINT startRegister, int* constantData, UINT vector4iCount) override;
			HRESULT WINAPI SetPixelShaderConstantB(UINT startRegister, const BOOL* constantData, UINT boolCount) override;
			HRESULT WINAPI GetPixelShaderConstantB(UINT startRegister, BOOL* constantData, UINT boolCount) override;
			HRESULT WINAPI DrawRectPatch(UINT handle, const float* numSegs, const D3DRECTPATCH_INFO* rectPatchInfo) override;
			HRESULT WINAPI DrawTriPatch(UINT handle, const float* numSegs, const D3DTRIPATCH_INFO* triPatchInfo) override;
			HRESULT WINAPI DeletePatch(UINT handle) override;
			HRESULT WINAPI CreateQuery(D3DQUERYTYPE type, IDirect3DQuery9** query) override;

		private:
			IDirect3DDevice9* device;
		};

		class D3D9 : public IDirect3D9
		{
		public:
			D3D9(IDirect3D9Ex* original) : direct3D(original) {}
			virtual ~D3D9() = default;

			HRESULT WINAPI QueryInterface(REFIID riid, void** object) override;
			ULONG WINAPI AddRef() override;
			ULONG WINAPI Release() override;
			HRESULT WINAPI RegisterSoftwareDevice(void* initializeFunction) override;
			UINT WINAPI GetAdapterCount() override;
			HRESULT WINAPI GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* identifier) override;
			UINT WINAPI GetAdapterModeCount(UINT adapter, D3DFORMAT format) override;
			HRESULT WINAPI EnumAdapterModes(UINT adapter, D3DFORMAT format, UINT modeIndex, D3DDISPLAYMODE* mode) override;
			HRESULT WINAPI GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* mode) override;
			HRESULT WINAPI CheckDeviceType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT displayFormat, D3DFORMAT backBufferFormat, BOOL isWindowed) override;
			HRESULT WINAPI CheckDeviceFormat(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE resourceType, D3DFORMAT checkFormat) override;
			HRESULT WINAPI CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT surfaceFormat, BOOL isWindowed, D3DMULTISAMPLE_TYPE multiSampleType, DWORD* qualityLevels) override;
			HRESULT WINAPI CheckDepthStencilMatch(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, D3DFORMAT renderTargetFormat, D3DFORMAT depthStencilFormat) override;
			HRESULT WINAPI CheckDeviceFormatConversion(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT sourceFormat, D3DFORMAT targetFormat) override;
			HRESULT WINAPI GetDeviceCaps(UINT adapter, D3DDEVTYPE deviceType, D3DCAPS9* caps) override;
			HMONITOR WINAPI GetAdapterMonitor(UINT adapter) override;
			HRESULT WINAPI CreateDevice(UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice) override;

		private:
			IDirect3D9* direct3D;
		};

		static Dvar::Var r_useD3D9Ex;

		static bool TryRedirectCreate();
		static IDirect3D9* WINAPI Direct3DCreate9Stub(UINT sdkVersion);

		static bool LoadImageWithReader(Game::GfxImage* image, Game::ImageFileReader_t reader);
		static void HookImageLoads();
	};
}
