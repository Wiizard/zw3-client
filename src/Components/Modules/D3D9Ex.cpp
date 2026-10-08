#include "STDInclude.hpp"

#include "D3D9Ex.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "Logger.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"
#include "Window.hpp"

#include <Utils/StagedTextureUpload.hpp>

namespace Components
{
	constexpr std::uintptr_t Direct3DCreate9Import = 0x1403629D0;

	typedef IDirect3D9*(WINAPI* Direct3DCreate9_t)(UINT sdkVersion);
	typedef HRESULT(WINAPI* Direct3DCreate9Ex_t)(UINT sdkVersion, IDirect3D9Ex** direct3D);

	static Direct3DCreate9_t direct3DCreate9 = nullptr;
	static Direct3DCreate9Ex_t direct3DCreate9Ex = nullptr;

	constexpr std::uintptr_t Image_LoadFromFileWithReader = 0x14006A180;

	constexpr std::uintptr_t Image_LoadFromFileWithReaderCalls[] =
	{
		0x140037AA5,

		0x140037BD0,

		0x140037E5D,

		0x140037F43,
	};

	static Utils::Hook imageLoadHooks[std::size(Image_LoadFromFileWithReaderCalls)];

	static std::atomic_bool isStartupUpload = true;

	static std::atomic_bool isMapUpload = false;
	static std::string loadingMap;
	static bool hasSeenConnection = false;

	static bool ShouldStageUploads()
	{
		if (!D3D9Ex::IsD3D9ExEnabled())
		{
			return false;
		}

		return isStartupUpload.load(std::memory_order_relaxed) || isMapUpload.load(std::memory_order_relaxed) || Renderer::IsDeviceRecoveryActive();
	}

	struct ImageUploadScope;

	static thread_local ImageUploadScope* currentUpload = nullptr;

	struct ImageUploadScope
	{
		Game::GfxImage* image;
		ImageUploadScope* previous;
		Utils::StagedTextureUpload upload;

		explicit ImageUploadScope(Game::GfxImage* target) : image(target), previous(currentUpload)
		{
			currentUpload = this;
		}

		ImageUploadScope(const ImageUploadScope&) = delete;
		ImageUploadScope& operator=(const ImageUploadScope&) = delete;

		~ImageUploadScope()
		{
			currentUpload = this->previous;
		}

		void Finish()
		{
			currentUpload = this->previous;

			if (!this->upload.IsPending())
			{
				return;
			}

			const HRESULT result = this->upload.Finish();

			if (FAILED(result))
			{
				Logger::Error("d3d9ex: could not upload image {} (hresult {:08X})\n", this->image->name, static_cast<unsigned int>(result));
				Game::Com_Error(Game::ERR_FATAL, "Could not upload image '%s' (HRESULT %08X).", this->image->name, static_cast<unsigned int>(result));
			}
		}
	};

	Dvar::Var D3D9Ex::r_useD3D9Ex;

#pragma region D3D9Device

	HRESULT D3D9Ex::D3D9Device::QueryInterface(REFIID riid, void** object)
	{
		*object = nullptr;

		const HRESULT result = device->QueryInterface(riid, object);

		if (result == NOERROR)
		{
			*object = this;
		}

		return result;
	}

	ULONG D3D9Ex::D3D9Device::AddRef()
	{
		return device->AddRef();
	}

	ULONG D3D9Ex::D3D9Device::Release()
	{
		const ULONG count = device->Release();

		if (!count)
		{
			delete this;
		}

		return count;
	}

	HRESULT D3D9Ex::D3D9Device::TestCooperativeLevel()
	{
		return device->TestCooperativeLevel();
	}

	UINT D3D9Ex::D3D9Device::GetAvailableTextureMem()
	{
		return device->GetAvailableTextureMem();
	}

	HRESULT D3D9Ex::D3D9Device::EvictManagedResources()
	{
		return device->EvictManagedResources();
	}

	HRESULT D3D9Ex::D3D9Device::GetDirect3D(IDirect3D9** direct3D)
	{
		return device->GetDirect3D(direct3D);
	}

	HRESULT D3D9Ex::D3D9Device::GetDeviceCaps(D3DCAPS9* caps)
	{
		return device->GetDeviceCaps(caps);
	}

	HRESULT D3D9Ex::D3D9Device::GetDisplayMode(UINT swapChainIndex, D3DDISPLAYMODE* mode)
	{
		return device->GetDisplayMode(swapChainIndex, mode);
	}

	HRESULT D3D9Ex::D3D9Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* parameters)
	{
		return device->GetCreationParameters(parameters);
	}

	HRESULT D3D9Ex::D3D9Device::SetCursorProperties(UINT xHotSpot, UINT yHotSpot, IDirect3DSurface9* cursorBitmap)
	{
		return device->SetCursorProperties(xHotSpot, yHotSpot, cursorBitmap);
	}

	void D3D9Ex::D3D9Device::SetCursorPosition(int x, int y, DWORD flags)
	{
		device->SetCursorPosition(x, y, flags);
	}

	BOOL D3D9Ex::D3D9Device::ShowCursor(BOOL shouldShow)
	{
		return device->ShowCursor(shouldShow);
	}

	HRESULT D3D9Ex::D3D9Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DSwapChain9** swapChain)
	{
		return device->CreateAdditionalSwapChain(presentationParameters, swapChain);
	}

	HRESULT D3D9Ex::D3D9Device::GetSwapChain(UINT swapChainIndex, IDirect3DSwapChain9** swapChain)
	{
		return device->GetSwapChain(swapChainIndex, swapChain);
	}

	UINT D3D9Ex::D3D9Device::GetNumberOfSwapChains()
	{
		return device->GetNumberOfSwapChains();
	}

	HRESULT D3D9Ex::D3D9Device::Reset(D3DPRESENT_PARAMETERS* presentationParameters)
	{
		return device->Reset(presentationParameters);
	}

	HRESULT D3D9Ex::D3D9Device::Present(const RECT* sourceRect, const RECT* destRect, HWND destWindowOverride, const RGNDATA* dirtyRegion)
	{
		return device->Present(sourceRect, destRect, destWindowOverride, dirtyRegion);
	}

	HRESULT D3D9Ex::D3D9Device::GetBackBuffer(UINT swapChainIndex, UINT backBufferIndex, D3DBACKBUFFER_TYPE type, IDirect3DSurface9** backBuffer)
	{
		return device->GetBackBuffer(swapChainIndex, backBufferIndex, type, backBuffer);
	}

	HRESULT D3D9Ex::D3D9Device::GetRasterStatus(UINT swapChainIndex, D3DRASTER_STATUS* rasterStatus)
	{
		return device->GetRasterStatus(swapChainIndex, rasterStatus);
	}

	HRESULT D3D9Ex::D3D9Device::SetDialogBoxMode(BOOL shouldEnableDialogs)
	{
		return device->SetDialogBoxMode(shouldEnableDialogs);
	}

	void D3D9Ex::D3D9Device::SetGammaRamp(UINT swapChainIndex, DWORD flags, const D3DGAMMARAMP* ramp)
	{
		device->SetGammaRamp(swapChainIndex, flags, ramp);
	}

	void D3D9Ex::D3D9Device::GetGammaRamp(UINT swapChainIndex, D3DGAMMARAMP* ramp)
	{
		device->GetGammaRamp(swapChainIndex, ramp);
	}

	HRESULT D3D9Ex::D3D9Device::CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture, HANDLE* sharedHandle)
	{
		auto* const scope = currentUpload;
		const bool isImageTexture = pool == D3DPOOL_MANAGED && usage == 0 && !sharedHandle && scope && texture == &scope->image->texture.map;

		if (isImageTexture && ShouldStageUploads())
		{
			const Utils::StagedTextureUpload::Shape shape{ width, height, levels, format };

			if (scope->upload.TryBegin(device, shape, texture))
			{
				return D3D_OK;
			}
		}

		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
			usage |= D3DUSAGE_DYNAMIC;
		}

		return device->CreateTexture(width, height, levels, usage, format, pool, texture, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateVolumeTexture(UINT width, UINT height, UINT depth, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DVolumeTexture9** volumeTexture, HANDLE* sharedHandle)
	{
		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
			usage |= D3DUSAGE_DYNAMIC;
		}

		return device->CreateVolumeTexture(width, height, depth, levels, usage, format, pool, volumeTexture, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateCubeTexture(UINT edgeLength, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture9** cubeTexture, HANDLE* sharedHandle)
	{
		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
			usage |= D3DUSAGE_DYNAMIC;
		}

		return device->CreateCubeTexture(edgeLength, levels, usage, format, pool, cubeTexture, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9** vertexBuffer, HANDLE* sharedHandle)
	{
		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
			usage |= D3DUSAGE_DYNAMIC;
		}

		return device->CreateVertexBuffer(length, usage, fvf, pool, vertexBuffer, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DIndexBuffer9** indexBuffer, HANDLE* sharedHandle)
	{
		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
			usage |= D3DUSAGE_DYNAMIC;
		}

		return device->CreateIndexBuffer(length, usage, format, pool, indexBuffer, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateRenderTarget(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL isLockable, IDirect3DSurface9** surface, HANDLE* sharedHandle)
	{
		return device->CreateRenderTarget(width, height, format, multiSample, multisampleQuality, isLockable, surface, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateDepthStencilSurface(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL shouldDiscard, IDirect3DSurface9** surface, HANDLE* sharedHandle)
	{
		return device->CreateDepthStencilSurface(width, height, format, multiSample, multisampleQuality, shouldDiscard, surface, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::UpdateSurface(IDirect3DSurface9* sourceSurface, const RECT* sourceRect, IDirect3DSurface9* destinationSurface, const POINT* destPoint)
	{
		return device->UpdateSurface(sourceSurface, sourceRect, destinationSurface, destPoint);
	}

	HRESULT D3D9Ex::D3D9Device::UpdateTexture(IDirect3DBaseTexture9* sourceTexture, IDirect3DBaseTexture9* destinationTexture)
	{
		return device->UpdateTexture(sourceTexture, destinationTexture);
	}

	HRESULT D3D9Ex::D3D9Device::GetRenderTargetData(IDirect3DSurface9* renderTarget, IDirect3DSurface9* destSurface)
	{
		return device->GetRenderTargetData(renderTarget, destSurface);
	}

	HRESULT D3D9Ex::D3D9Device::GetFrontBufferData(UINT swapChainIndex, IDirect3DSurface9* destSurface)
	{
		return device->GetFrontBufferData(swapChainIndex, destSurface);
	}

	HRESULT D3D9Ex::D3D9Device::StretchRect(IDirect3DSurface9* sourceSurface, const RECT* sourceRect, IDirect3DSurface9* destSurface, const RECT* destRect, D3DTEXTUREFILTERTYPE filter)
	{
		return device->StretchRect(sourceSurface, sourceRect, destSurface, destRect, filter);
	}

	HRESULT D3D9Ex::D3D9Device::ColorFill(IDirect3DSurface9* surface, const RECT* rect, D3DCOLOR color)
	{
		return device->ColorFill(surface, rect, color);
	}

	HRESULT D3D9Ex::D3D9Device::CreateOffscreenPlainSurface(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, IDirect3DSurface9** surface, HANDLE* sharedHandle)
	{
		if (pool == D3DPOOL_MANAGED)
		{
			pool = D3DPOOL_DEFAULT;
		}

		return device->CreateOffscreenPlainSurface(width, height, format, pool, surface, sharedHandle);
	}

	HRESULT D3D9Ex::D3D9Device::SetRenderTarget(DWORD renderTargetIndex, IDirect3DSurface9* renderTarget)
	{
		return device->SetRenderTarget(renderTargetIndex, renderTarget);
	}

	HRESULT D3D9Ex::D3D9Device::GetRenderTarget(DWORD renderTargetIndex, IDirect3DSurface9** renderTarget)
	{
		return device->GetRenderTarget(renderTargetIndex, renderTarget);
	}

	HRESULT D3D9Ex::D3D9Device::SetDepthStencilSurface(IDirect3DSurface9* newZStencil)
	{
		return device->SetDepthStencilSurface(newZStencil);
	}

	HRESULT D3D9Ex::D3D9Device::GetDepthStencilSurface(IDirect3DSurface9** zStencilSurface)
	{
		return device->GetDepthStencilSurface(zStencilSurface);
	}

	HRESULT D3D9Ex::D3D9Device::BeginScene()
	{
		return device->BeginScene();
	}

	HRESULT D3D9Ex::D3D9Device::EndScene()
	{
		return device->EndScene();
	}

	HRESULT D3D9Ex::D3D9Device::Clear(DWORD count, const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil)
	{
		return device->Clear(count, rects, flags, color, z, stencil);
	}

	HRESULT D3D9Ex::D3D9Device::SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix)
	{
		return device->SetTransform(state, matrix);
	}

	HRESULT D3D9Ex::D3D9Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX* matrix)
	{
		return device->GetTransform(state, matrix);
	}

	HRESULT D3D9Ex::D3D9Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix)
	{
		return device->MultiplyTransform(state, matrix);
	}

	HRESULT D3D9Ex::D3D9Device::SetViewport(const D3DVIEWPORT9* viewport)
	{
		return device->SetViewport(viewport);
	}

	HRESULT D3D9Ex::D3D9Device::GetViewport(D3DVIEWPORT9* viewport)
	{
		return device->GetViewport(viewport);
	}

	HRESULT D3D9Ex::D3D9Device::SetMaterial(const D3DMATERIAL9* material)
	{
		return device->SetMaterial(material);
	}

	HRESULT D3D9Ex::D3D9Device::GetMaterial(D3DMATERIAL9* material)
	{
		return device->GetMaterial(material);
	}

	HRESULT D3D9Ex::D3D9Device::SetLight(DWORD index, const D3DLIGHT9* light)
	{
		return device->SetLight(index, light);
	}

	HRESULT D3D9Ex::D3D9Device::GetLight(DWORD index, D3DLIGHT9* light)
	{
		return device->GetLight(index, light);
	}

	HRESULT D3D9Ex::D3D9Device::LightEnable(DWORD index, BOOL isEnabled)
	{
		return device->LightEnable(index, isEnabled);
	}

	HRESULT D3D9Ex::D3D9Device::GetLightEnable(DWORD index, BOOL* isEnabled)
	{
		return device->GetLightEnable(index, isEnabled);
	}

	HRESULT D3D9Ex::D3D9Device::SetClipPlane(DWORD index, const float* plane)
	{
		return device->SetClipPlane(index, plane);
	}

	HRESULT D3D9Ex::D3D9Device::GetClipPlane(DWORD index, float* plane)
	{
		return device->GetClipPlane(index, plane);
	}

	HRESULT D3D9Ex::D3D9Device::SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
	{
		return device->SetRenderState(state, value);
	}

	HRESULT D3D9Ex::D3D9Device::GetRenderState(D3DRENDERSTATETYPE state, DWORD* value)
	{
		return device->GetRenderState(state, value);
	}

	HRESULT D3D9Ex::D3D9Device::CreateStateBlock(D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9** stateBlock)
	{
		return device->CreateStateBlock(type, stateBlock);
	}

	HRESULT D3D9Ex::D3D9Device::BeginStateBlock()
	{
		return device->BeginStateBlock();
	}

	HRESULT D3D9Ex::D3D9Device::EndStateBlock(IDirect3DStateBlock9** stateBlock)
	{
		return device->EndStateBlock(stateBlock);
	}

	HRESULT D3D9Ex::D3D9Device::SetClipStatus(const D3DCLIPSTATUS9* clipStatus)
	{
		return device->SetClipStatus(clipStatus);
	}

	HRESULT D3D9Ex::D3D9Device::GetClipStatus(D3DCLIPSTATUS9* clipStatus)
	{
		return device->GetClipStatus(clipStatus);
	}

	HRESULT D3D9Ex::D3D9Device::GetTexture(DWORD stage, IDirect3DBaseTexture9** texture)
	{
		return device->GetTexture(stage, texture);
	}

	HRESULT D3D9Ex::D3D9Device::SetTexture(DWORD stage, IDirect3DBaseTexture9* texture)
	{
		return device->SetTexture(stage, texture);
	}

	HRESULT D3D9Ex::D3D9Device::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* value)
	{
		return device->GetTextureStageState(stage, type, value);
	}

	HRESULT D3D9Ex::D3D9Device::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
	{
		return device->SetTextureStageState(stage, type, value);
	}

	HRESULT D3D9Ex::D3D9Device::GetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD* value)
	{
		return device->GetSamplerState(sampler, type, value);
	}

	HRESULT D3D9Ex::D3D9Device::SetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value)
	{
		return device->SetSamplerState(sampler, type, value);
	}

	HRESULT D3D9Ex::D3D9Device::ValidateDevice(DWORD* numPasses)
	{
		return device->ValidateDevice(numPasses);
	}

	HRESULT D3D9Ex::D3D9Device::SetPaletteEntries(UINT paletteNumber, const PALETTEENTRY* entries)
	{
		return device->SetPaletteEntries(paletteNumber, entries);
	}

	HRESULT D3D9Ex::D3D9Device::GetPaletteEntries(UINT paletteNumber, PALETTEENTRY* entries)
	{
		return device->GetPaletteEntries(paletteNumber, entries);
	}

	HRESULT D3D9Ex::D3D9Device::SetCurrentTexturePalette(UINT paletteNumber)
	{
		return device->SetCurrentTexturePalette(paletteNumber);
	}

	HRESULT D3D9Ex::D3D9Device::GetCurrentTexturePalette(UINT* paletteNumber)
	{
		return device->GetCurrentTexturePalette(paletteNumber);
	}

	HRESULT D3D9Ex::D3D9Device::SetScissorRect(const RECT* rect)
	{
		return device->SetScissorRect(rect);
	}

	HRESULT D3D9Ex::D3D9Device::GetScissorRect(RECT* rect)
	{
		return device->GetScissorRect(rect);
	}

	HRESULT D3D9Ex::D3D9Device::SetSoftwareVertexProcessing(BOOL isSoftware)
	{
		return device->SetSoftwareVertexProcessing(isSoftware);
	}

	BOOL D3D9Ex::D3D9Device::GetSoftwareVertexProcessing()
	{
		return device->GetSoftwareVertexProcessing();
	}

	HRESULT D3D9Ex::D3D9Device::SetNPatchMode(float segments)
	{
		return device->SetNPatchMode(segments);
	}

	float D3D9Ex::D3D9Device::GetNPatchMode()
	{
		return device->GetNPatchMode();
	}

	HRESULT D3D9Ex::D3D9Device::DrawPrimitive(D3DPRIMITIVETYPE primitiveType, UINT startVertex, UINT primitiveCount)
	{
		return device->DrawPrimitive(primitiveType, startVertex, primitiveCount);
	}

	HRESULT D3D9Ex::D3D9Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType, INT baseVertexIndex, UINT minVertexIndex, UINT numVertices, UINT startIndex, UINT primCount)
	{
		return device->DrawIndexedPrimitive(primitiveType, baseVertexIndex, minVertexIndex, numVertices, startIndex, primCount);
	}

	HRESULT D3D9Ex::D3D9Device::DrawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT primitiveCount, const void* vertexStreamZeroData, UINT vertexStreamZeroStride)
	{
		return device->DrawPrimitiveUP(primitiveType, primitiveCount, vertexStreamZeroData, vertexStreamZeroStride);
	}

	HRESULT D3D9Ex::D3D9Device::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitiveType, UINT minVertexIndex, UINT numVertices, UINT primitiveCount, const void* indexData, D3DFORMAT indexDataFormat, const void* vertexStreamZeroData, UINT vertexStreamZeroStride)
	{
		return device->DrawIndexedPrimitiveUP(primitiveType, minVertexIndex, numVertices, primitiveCount, indexData, indexDataFormat, vertexStreamZeroData, vertexStreamZeroStride);
	}

	HRESULT D3D9Ex::D3D9Device::ProcessVertices(UINT srcStartIndex, UINT destIndex, UINT vertexCount, IDirect3DVertexBuffer9* destBuffer, IDirect3DVertexDeclaration9* vertexDecl, DWORD flags)
	{
		return device->ProcessVertices(srcStartIndex, destIndex, vertexCount, destBuffer, vertexDecl, flags);
	}

	HRESULT D3D9Ex::D3D9Device::CreateVertexDeclaration(const D3DVERTEXELEMENT9* vertexElements, IDirect3DVertexDeclaration9** decl)
	{
		return device->CreateVertexDeclaration(vertexElements, decl);
	}

	HRESULT D3D9Ex::D3D9Device::SetVertexDeclaration(IDirect3DVertexDeclaration9* decl)
	{
		return device->SetVertexDeclaration(decl);
	}

	HRESULT D3D9Ex::D3D9Device::GetVertexDeclaration(IDirect3DVertexDeclaration9** decl)
	{
		return device->GetVertexDeclaration(decl);
	}

	HRESULT D3D9Ex::D3D9Device::SetFVF(DWORD fvf)
	{
		return device->SetFVF(fvf);
	}

	HRESULT D3D9Ex::D3D9Device::GetFVF(DWORD* fvf)
	{
		return device->GetFVF(fvf);
	}

	HRESULT D3D9Ex::D3D9Device::CreateVertexShader(const DWORD* function, IDirect3DVertexShader9** shader)
	{
		return device->CreateVertexShader(function, shader);
	}

	HRESULT D3D9Ex::D3D9Device::SetVertexShader(IDirect3DVertexShader9* shader)
	{
		return device->SetVertexShader(shader);
	}

	HRESULT D3D9Ex::D3D9Device::GetVertexShader(IDirect3DVertexShader9** shader)
	{
		return device->GetVertexShader(shader);
	}

	HRESULT D3D9Ex::D3D9Device::SetVertexShaderConstantF(UINT startRegister, const float* constantData, UINT vector4fCount)
	{
		return device->SetVertexShaderConstantF(startRegister, constantData, vector4fCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetVertexShaderConstantF(UINT startRegister, float* constantData, UINT vector4fCount)
	{
		return device->GetVertexShaderConstantF(startRegister, constantData, vector4fCount);
	}

	HRESULT D3D9Ex::D3D9Device::SetVertexShaderConstantI(UINT startRegister, const int* constantData, UINT vector4iCount)
	{
		return device->SetVertexShaderConstantI(startRegister, constantData, vector4iCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetVertexShaderConstantI(UINT startRegister, int* constantData, UINT vector4iCount)
	{
		return device->GetVertexShaderConstantI(startRegister, constantData, vector4iCount);
	}

	HRESULT D3D9Ex::D3D9Device::SetVertexShaderConstantB(UINT startRegister, const BOOL* constantData, UINT boolCount)
	{
		return device->SetVertexShaderConstantB(startRegister, constantData, boolCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetVertexShaderConstantB(UINT startRegister, BOOL* constantData, UINT boolCount)
	{
		return device->GetVertexShaderConstantB(startRegister, constantData, boolCount);
	}

	HRESULT D3D9Ex::D3D9Device::SetStreamSource(UINT streamNumber, IDirect3DVertexBuffer9* streamData, UINT offsetInBytes, UINT stride)
	{
		return device->SetStreamSource(streamNumber, streamData, offsetInBytes, stride);
	}

	HRESULT D3D9Ex::D3D9Device::GetStreamSource(UINT streamNumber, IDirect3DVertexBuffer9** streamData, UINT* offsetInBytes, UINT* stride)
	{
		return device->GetStreamSource(streamNumber, streamData, offsetInBytes, stride);
	}

	HRESULT D3D9Ex::D3D9Device::SetStreamSourceFreq(UINT streamNumber, UINT divider)
	{
		return device->SetStreamSourceFreq(streamNumber, divider);
	}

	HRESULT D3D9Ex::D3D9Device::GetStreamSourceFreq(UINT streamNumber, UINT* divider)
	{
		return device->GetStreamSourceFreq(streamNumber, divider);
	}

	HRESULT D3D9Ex::D3D9Device::SetIndices(IDirect3DIndexBuffer9* indexData)
	{
		return device->SetIndices(indexData);
	}

	HRESULT D3D9Ex::D3D9Device::GetIndices(IDirect3DIndexBuffer9** indexData)
	{
		return device->GetIndices(indexData);
	}

	HRESULT D3D9Ex::D3D9Device::CreatePixelShader(const DWORD* function, IDirect3DPixelShader9** shader)
	{
		return device->CreatePixelShader(function, shader);
	}

	HRESULT D3D9Ex::D3D9Device::SetPixelShader(IDirect3DPixelShader9* shader)
	{
		return device->SetPixelShader(shader);
	}

	HRESULT D3D9Ex::D3D9Device::GetPixelShader(IDirect3DPixelShader9** shader)
	{
		return device->GetPixelShader(shader);
	}

	HRESULT D3D9Ex::D3D9Device::SetPixelShaderConstantF(UINT startRegister, const float* constantData, UINT vector4fCount)
	{
		if (IsBadReadPtr(constantData, vector4fCount * 16))
		{
			Logger::Debug("d3d9ex: invalid pixel shader constants at register {}\n", startRegister);
			return D3DERR_INVALIDCALL;
		}

		return device->SetPixelShaderConstantF(startRegister, constantData, vector4fCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetPixelShaderConstantF(UINT startRegister, float* constantData, UINT vector4fCount)
	{
		return device->GetPixelShaderConstantF(startRegister, constantData, vector4fCount);
	}

	HRESULT D3D9Ex::D3D9Device::SetPixelShaderConstantI(UINT startRegister, const int* constantData, UINT vector4iCount)
	{
		return device->SetPixelShaderConstantI(startRegister, constantData, vector4iCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetPixelShaderConstantI(UINT startRegister, int* constantData, UINT vector4iCount)
	{
		return device->GetPixelShaderConstantI(startRegister, constantData, vector4iCount);
	}

	HRESULT D3D9Ex::D3D9Device::SetPixelShaderConstantB(UINT startRegister, const BOOL* constantData, UINT boolCount)
	{
		return device->SetPixelShaderConstantB(startRegister, constantData, boolCount);
	}

	HRESULT D3D9Ex::D3D9Device::GetPixelShaderConstantB(UINT startRegister, BOOL* constantData, UINT boolCount)
	{
		return device->GetPixelShaderConstantB(startRegister, constantData, boolCount);
	}

	HRESULT D3D9Ex::D3D9Device::DrawRectPatch(UINT handle, const float* numSegs, const D3DRECTPATCH_INFO* rectPatchInfo)
	{
		return device->DrawRectPatch(handle, numSegs, rectPatchInfo);
	}

	HRESULT D3D9Ex::D3D9Device::DrawTriPatch(UINT handle, const float* numSegs, const D3DTRIPATCH_INFO* triPatchInfo)
	{
		return device->DrawTriPatch(handle, numSegs, triPatchInfo);
	}

	HRESULT D3D9Ex::D3D9Device::DeletePatch(UINT handle)
	{
		return device->DeletePatch(handle);
	}

	HRESULT D3D9Ex::D3D9Device::CreateQuery(D3DQUERYTYPE type, IDirect3DQuery9** query)
	{
		return device->CreateQuery(type, query);
	}

#pragma endregion

#pragma region D3D9

	HRESULT D3D9Ex::D3D9::QueryInterface(REFIID riid, void** object)
	{
		*object = nullptr;

		const HRESULT result = direct3D->QueryInterface(riid, object);

		if (result == NOERROR)
		{
			*object = this;
		}

		return result;
	}

	ULONG D3D9Ex::D3D9::AddRef()
	{
		return direct3D->AddRef();
	}

	ULONG D3D9Ex::D3D9::Release()
	{
		const ULONG count = direct3D->Release();

		if (!count)
		{
			delete this;
		}

		return count;
	}

	HRESULT D3D9Ex::D3D9::RegisterSoftwareDevice(void* initializeFunction)
	{
		return direct3D->RegisterSoftwareDevice(initializeFunction);
	}

	UINT D3D9Ex::D3D9::GetAdapterCount()
	{
		return direct3D->GetAdapterCount();
	}

	HRESULT D3D9Ex::D3D9::GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* identifier)
	{
		return direct3D->GetAdapterIdentifier(adapter, flags, identifier);
	}

	UINT D3D9Ex::D3D9::GetAdapterModeCount(UINT adapter, D3DFORMAT format)
	{
		return direct3D->GetAdapterModeCount(adapter, format);
	}

	HRESULT D3D9Ex::D3D9::EnumAdapterModes(UINT adapter, D3DFORMAT format, UINT modeIndex, D3DDISPLAYMODE* mode)
	{
		return direct3D->EnumAdapterModes(adapter, format, modeIndex, mode);
	}

	HRESULT D3D9Ex::D3D9::GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* mode)
	{
		return direct3D->GetAdapterDisplayMode(adapter, mode);
	}

	HRESULT D3D9Ex::D3D9::CheckDeviceType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT displayFormat, D3DFORMAT backBufferFormat, BOOL isWindowed)
	{
		return direct3D->CheckDeviceType(adapter, deviceType, displayFormat, backBufferFormat, isWindowed);
	}

	HRESULT D3D9Ex::D3D9::CheckDeviceFormat(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE resourceType, D3DFORMAT checkFormat)
	{
		return direct3D->CheckDeviceFormat(adapter, deviceType, adapterFormat, usage, resourceType, checkFormat);
	}

	HRESULT D3D9Ex::D3D9::CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT surfaceFormat, BOOL isWindowed, D3DMULTISAMPLE_TYPE multiSampleType, DWORD* qualityLevels)
	{
		return direct3D->CheckDeviceMultiSampleType(adapter, deviceType, surfaceFormat, isWindowed, multiSampleType, qualityLevels);
	}

	HRESULT D3D9Ex::D3D9::CheckDepthStencilMatch(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, D3DFORMAT renderTargetFormat, D3DFORMAT depthStencilFormat)
	{
		return direct3D->CheckDepthStencilMatch(adapter, deviceType, adapterFormat, renderTargetFormat, depthStencilFormat);
	}

	HRESULT D3D9Ex::D3D9::CheckDeviceFormatConversion(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT sourceFormat, D3DFORMAT targetFormat)
	{
		return direct3D->CheckDeviceFormatConversion(adapter, deviceType, sourceFormat, targetFormat);
	}

	HRESULT D3D9Ex::D3D9::GetDeviceCaps(UINT adapter, D3DDEVTYPE deviceType, D3DCAPS9* caps)
	{
		return direct3D->GetDeviceCaps(adapter, deviceType, caps);
	}

	HMONITOR D3D9Ex::D3D9::GetAdapterMonitor(UINT adapter)
	{
		return direct3D->GetAdapterMonitor(adapter);
	}

	HRESULT D3D9Ex::D3D9::CreateDevice(UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice)
	{
		const HRESULT result = direct3D->CreateDevice(adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDevice);

		if (SUCCEEDED(result))
		{
			*returnedDevice = new D3D9Device(*returnedDevice);
		}

		return result;
	}

#pragma endregion

	bool D3D9Ex::TryRedirectCreate()
	{
		auto* const importSlot = reinterpret_cast<void**>(Utils::Hook::Rebase(Direct3DCreate9Import));
		void* const resolved = *importSlot;

		HMODULE module = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

		if (!resolved || !GetModuleHandleExA(flags, static_cast<LPCSTR>(resolved), &module))
		{
			return false;
		}

		if (reinterpret_cast<void*>(GetProcAddress(module, "Direct3DCreate9")) != resolved)
		{
			return false;
		}

		direct3DCreate9 = reinterpret_cast<Direct3DCreate9_t>(resolved);
		direct3DCreate9Ex = reinterpret_cast<Direct3DCreate9Ex_t>(GetProcAddress(module, "Direct3DCreate9Ex"));

		Utils::Hook::Set<void*>(Direct3DCreate9Import, reinterpret_cast<void*>(Direct3DCreate9Stub));

		return *importSlot == reinterpret_cast<void*>(Direct3DCreate9Stub);
	}

	IDirect3D9* WINAPI D3D9Ex::Direct3DCreate9Stub(UINT sdkVersion)
	{
		if (IsD3D9ExEnabled())
		{
			IDirect3D9Ex* direct3D = nullptr;

			if (direct3DCreate9Ex && SUCCEEDED(direct3DCreate9Ex(sdkVersion, &direct3D)))
			{
				return new D3D9(direct3D);
			}

			Logger::Error("d3d9ex: no D3D9Ex for sdk version {}, the device is plain d3d9\n", sdkVersion);
		}

		return direct3DCreate9(sdkVersion);
	}

	bool D3D9Ex::IsD3D9ExEnabled()
	{
		return r_useD3D9Ex.IsValid() && r_useD3D9Ex.Get<bool>();
	}

	void D3D9Ex::BeginMapLoading(const std::string& map)
	{
		if (Dedicated::IsEnabled() || map.empty())
		{
			return;
		}

		if (isMapUpload.load(std::memory_order_relaxed) && loadingMap == map)
		{
			return;
		}

		loadingMap = map;
		hasSeenConnection = false;
		isMapUpload.store(true, std::memory_order_relaxed);
	}

	void D3D9Ex::LoadTexture(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image)
	{
		Window::PumpLoadingEvents();

		if (!ShouldStageUploads())
		{
			Game::Load_Texture(loadDef, image);
			return;
		}

		ImageUploadScope scope(image);
		Game::Load_Texture(loadDef, image);
		scope.Finish();
	}

	bool D3D9Ex::LoadImageWithReader(Game::GfxImage* image, Game::ImageFileReader_t reader)
	{
		Window::PumpLoadingEvents();

		if (!ShouldStageUploads())
		{
			return Game::Image_LoadFromFileWithReader(image, reader);
		}

		ImageUploadScope scope(image);
		const bool isLoaded = Game::Image_LoadFromFileWithReader(image, reader);
		scope.Finish();

		return isLoaded;
	}

	void D3D9Ex::HookImageLoads()
	{
		for (const std::uintptr_t site : Image_LoadFromFileWithReaderCalls)
		{
			if (!Utils::Hook::BranchesTo(site, Image_LoadFromFileWithReader, HOOK_CALL))
			{
				Logger::Error("d3d9ex: 0x{:X} no longer calls Image_LoadFromFileWithReader, no image load is staged\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(Image_LoadFromFileWithReaderCalls); ++i)
		{
			isSeated = imageLoadHooks[i].Initialize(Image_LoadFromFileWithReaderCalls[i], reinterpret_cast<void*>(LoadImageWithReader), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : imageLoadHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("d3d9ex: could not seat every image load hook, no image load is staged\n");
			return;
		}

		for (auto& hook : imageLoadHooks)
		{
			hook.Quick();
		}
	}

	D3D9Ex::D3D9Ex()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			r_useD3D9Ex = Dvar::Register("r_useD3D9Ex", false, Game::DVAR_ARCHIVE, "Use extended d3d9 interface!");

			if (!TryRedirectCreate())
			{
				Logger::Error("d3d9ex: the Direct3DCreate9 import does not read as expected, r_useD3D9Ex does nothing\n");
			}
		});

		HookImageLoads();

		Scheduler::Loop([]
		{
			if (!isMapUpload.load(std::memory_order_relaxed))
			{
				return;
			}

			const Game::connstate_t state = Game::CL_GetLocalClientConnectionState(0);

			if (state >= Game::CA_CONNECTING && state < Game::CA_ACTIVE)
			{
				hasSeenConnection = true;
			}

			const bool isLoadOver = state == Game::CA_ACTIVE || (state == Game::CA_DISCONNECTED && FastFiles::Ready());

			if (hasSeenConnection && isLoadOver)
			{
				isMapUpload.store(false, std::memory_order_relaxed);
				loadingMap.clear();
			}
		}, Scheduler::Pipeline::MAIN);

		Scheduler::OnGameInitialized([]
		{
			isStartupUpload.store(false, std::memory_order_relaxed);
		}, Scheduler::Pipeline::MAIN);
	}
}
