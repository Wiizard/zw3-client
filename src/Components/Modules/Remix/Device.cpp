#include "STDInclude.hpp"

#include "Device.hpp"
#include "Core.hpp"
#include "Engine.hpp"
#include "QueryPoll.hpp"
#include "RenderFrameLock.hpp"
#include "TextureTags.hpp"
#include "Api.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <vector>

#include "Components/Modules/Logger.hpp"

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace Components::Remix::Device
{
	constexpr std::size_t direct3DCreateDeviceSlot = 16;
	constexpr std::size_t deviceResetSlot = 16;
	constexpr std::size_t deviceSetRenderStateSlot = 57;
	constexpr std::size_t deviceBeginSceneSlot = 41;
	constexpr std::size_t deviceClearSlot = 43;
	constexpr std::size_t textureReleaseSlot = 2;
	constexpr std::size_t textureLockRectSlot = 19;
	constexpr std::size_t textureUnlockRectSlot = 20;
	constexpr std::size_t queryGetDataSlot = 7;
	constexpr std::size_t deviceSetRenderTargetSlot = 37;
	constexpr std::size_t deviceSetTransformSlot = 44;
	constexpr std::size_t deviceDrawPrimitiveSlot = 81;
	constexpr std::size_t deviceDrawIndexedPrimitiveSlot = 82;
	constexpr std::size_t deviceDrawPrimitiveUpSlot = 83;
	constexpr std::size_t deviceDrawIndexedPrimitiveUpSlot = 84;
	constexpr std::size_t bufferLockSlot = 11;
	constexpr std::size_t bufferUnlockSlot = 12;

	using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice);
	using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentationParameters);
	using SetRenderState_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value);
	using BeginScene_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self);
	using Clear_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, DWORD count, const D3DRECT* rects, DWORD flags,
		D3DCOLOR color, float z, DWORD stencil);
	using Release_t = ULONG(STDMETHODCALLTYPE*)(IDirect3DTexture9* self);
	using LockRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* lockedRect, const RECT* rect, DWORD flags);
	using UnlockRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9* self, UINT level);
	using QueryGetData_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DQuery9* self, void* data, DWORD size, DWORD flags);
	using BufferLock_t = HRESULT(STDMETHODCALLTYPE*)(void* self, UINT offset, UINT size, void** data, DWORD flags);
	using BufferUnlock_t = HRESULT(STDMETHODCALLTYPE*)(void* self);
	using SetRenderTarget_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, DWORD index, IDirect3DSurface9* surface);
	using SetTransform_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DTRANSFORMSTATETYPE state,
		const D3DMATRIX* matrix);
	using DrawPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT start,
		UINT count);
	using DrawIndexedPrimitive_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
		INT baseVertex, UINT minIndex, UINT vertices, UINT startIndex, UINT count);
	using DrawPrimitiveUp_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT count,
		const void* data, UINT stride);
	using DrawIndexedPrimitiveUp_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
		UINT minIndex, UINT vertices, UINT count, const void* indices, D3DFORMAT format, const void* data, UINT stride);

	static CreateDevice_t originalCreateDevice = nullptr;
	static Reset_t originalReset = nullptr;
	static SetRenderState_t originalSetRenderState = nullptr;
	static BeginScene_t originalBeginScene = nullptr;
	static std::vector<DeviceCallback> beginSceneCallbacks;
	static bool isInBeginSceneCallbacks = false;
	static Clear_t originalClear = nullptr;
	static bool isSceneKept = false;
	static Release_t originalTextureRelease = nullptr;
	static LockRect_t originalLockRect = nullptr;
	static UnlockRect_t originalUnlockRect = nullptr;
	static QueryGetData_t originalQueryGetData = nullptr;
	static SetRenderTarget_t originalSetRenderTarget = nullptr;
	static SetTransform_t originalSetTransform = nullptr;
	static DrawPrimitive_t originalDrawPrimitive = nullptr;
	static DrawIndexedPrimitive_t originalDrawIndexedPrimitive = nullptr;
	static DrawPrimitiveUp_t originalDrawPrimitiveUp = nullptr;
	static DrawIndexedPrimitiveUp_t originalDrawIndexedPrimitiveUp = nullptr;

	enum class CaptureState
	{
		Idle,
		Armed,
		Capturing,
	};

	static std::atomic<CaptureState> captureState = CaptureState::Idle;
	static std::mutex captureMutex;
	static std::vector<std::string> capturedEvents;
	static unsigned capturedDraws = 0;
	static unsigned capturedOrthoDraws = 0;
	static bool isProjectionOrtho = false;
	static int captureIndex = 0;
	static BufferLock_t originalVertexLock = nullptr;
	static BufferUnlock_t originalVertexUnlock = nullptr;
	static BufferLock_t originalIndexLock = nullptr;
	static BufferUnlock_t originalIndexUnlock = nullptr;

	static RenderFrameLock renderFrameLock;

	static DeviceCallback createdCallback = nullptr;
	static DeviceCallback resetCallback = nullptr;

	static std::uintptr_t gameImageBegin = 0;
	static std::uintptr_t gameImageEnd = 0;

	static std::mutex hashMutex;
	static std::unordered_map<const void*, std::uint64_t> textureHashes;
	static std::unordered_map<const void*, void*> lockedTopLevels;

	static bool isEffectPass = false;
	static bool isPremultipliedRemap = false;
	static bool isAlphaAsEmissive = false;
	static DWORD requestedSourceBlend = D3DBLEND_ONE;
	static DWORD requestedDestinationBlend = D3DBLEND_ZERO;

	template <typename T>
	static T SwapSlot(void* object, std::size_t slot, void* replacement)
	{
		void** const vtable = *static_cast<void***>(object);
		void* const current = vtable[slot];

		if (current == replacement)
		{
			return nullptr;
		}

		DWORD previous = 0;

		if (!VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &previous))
		{
			return nullptr;
		}

		vtable[slot] = replacement;
		VirtualProtect(&vtable[slot], sizeof(void*), previous, &previous);

		return reinterpret_cast<T>(current);
	}

	static bool IsGameCaller(const void* returnAddress)
	{
		const auto address = reinterpret_cast<std::uintptr_t>(returnAddress);
		return address >= gameImageBegin && address < gameImageEnd;
	}

	static DWORD ForwardedSourceBlend()
	{
		const bool isAdditive = requestedDestinationBlend == D3DBLEND_ONE
			&& (requestedSourceBlend == D3DBLEND_ONE || requestedSourceBlend == D3DBLEND_INVDESTCOLOR);

		if (isEffectPass && isAdditive)
		{
			return D3DBLEND_SRCALPHA;
		}

		const bool isPremultiplied = requestedSourceBlend == D3DBLEND_ONE
			&& requestedDestinationBlend == D3DBLEND_INVSRCALPHA;

		if (isPremultipliedRemap && isPremultiplied)
		{
			return D3DBLEND_SRCALPHA;
		}

		return requestedSourceBlend;
	}

	static DWORD ForwardedDestinationBlend()
	{
		const bool isAlphaBlend = requestedSourceBlend == D3DBLEND_SRCALPHA
			&& requestedDestinationBlend == D3DBLEND_INVSRCALPHA;

		if (isAlphaAsEmissive && isAlphaBlend)
		{
			return D3DBLEND_ONE;
		}

		return requestedDestinationBlend;
	}

	static HRESULT STDMETHODCALLTYPE SetRenderStateHook(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value)
	{
		if (!IsGameCaller(_ReturnAddress()))
		{
			return originalSetRenderState(self, state, value);
		}

		if (state == D3DRS_SRCBLEND)
		{
			requestedSourceBlend = value;
			const HRESULT result = originalSetRenderState(self, state, ForwardedSourceBlend());

			if (isAlphaAsEmissive)
			{
				originalSetRenderState(self, D3DRS_DESTBLEND, ForwardedDestinationBlend());
			}

			return result;
		}

		if (state == D3DRS_DESTBLEND)
		{
			requestedDestinationBlend = value;
			const HRESULT result = originalSetRenderState(self, state, ForwardedDestinationBlend());

			if (isEffectPass || isPremultipliedRemap)
			{
				originalSetRenderState(self, D3DRS_SRCBLEND, ForwardedSourceBlend());
			}

			return result;
		}

		return originalSetRenderState(self, state, value);
	}

	static HRESULT STDMETHODCALLTYPE ResetHook(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentationParameters)
	{
		if (resetCallback)
		{
			resetCallback(self);
		}

		return originalReset(self, presentationParameters);
	}

	static ULONG STDMETHODCALLTYPE TextureReleaseHook(IDirect3DTexture9* self)
	{
		const ULONG references = originalTextureRelease(self);

		if (references == 0)
		{
			const std::lock_guard lock(hashMutex);
			textureHashes.erase(self);
			lockedTopLevels.erase(self);
		}

		return references;
	}

	static HRESULT STDMETHODCALLTYPE LockRectHook(IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* lockedRect, const RECT* rect, DWORD flags)
	{
		const HRESULT result = originalLockRect(self, level, lockedRect, rect, flags);
		const bool isFullTopLevelWrite = level == 0 && !rect && !(flags & D3DLOCK_READONLY);

		if (SUCCEEDED(result) && isFullTopLevelWrite && lockedRect)
		{
			const std::lock_guard lock(hashMutex);
			lockedTopLevels[self] = lockedRect->pBits;
		}

		return result;
	}

	static HRESULT STDMETHODCALLTYPE UnlockRectHook(IDirect3DTexture9* self, UINT level)
	{
		if (level == 0)
		{
			void* bits = nullptr;

			{
				const std::lock_guard lock(hashMutex);
				const auto found = lockedTopLevels.find(self);

				if (found != lockedTopLevels.end())
				{
					bits = found->second;
					lockedTopLevels.erase(found);
				}
			}

			D3DSURFACE_DESC desc{};
			std::uint64_t hash = 0;

			if (bits && SUCCEEDED(self->GetLevelDesc(0, &desc)) && TryHashTopLevel(bits, desc, hash))
			{
				const std::lock_guard lock(hashMutex);
				textureHashes[self] = hash;
			}
		}

		return originalUnlockRect(self, level);
	}

	void EnterRenderFrame()
	{
		renderFrameLock.lock();
	}

	void LeaveRenderFrame()
	{
		renderFrameLock.unlock();
	}

	static HRESULT STDMETHODCALLTYPE QueryGetDataHook(IDirect3DQuery9* self, void* data, DWORD size, DWORD flags)
	{
		return PollQuery(flags, renderFrameLock.OwnsCurrentThread(), [&](const DWORD pollFlags)
		{
			return originalQueryGetData(self, data, size, pollFlags);
		}, []
		{
			return renderFrameLock.TryLockFor(std::chrono::milliseconds(100));
		}, []
		{
			renderFrameLock.unlock();
		});
	}

	template <typename Call>
	static HRESULT GuardedBufferCall(Call call)
	{
		if (!Engine::IsRendererThreaded())
		{
			return call();
		}

		// Lock/Unlock cannot be skipped or issued concurrently after a timeout:
		// native callers still need the returned buffer pointer and lock state.
		const std::lock_guard lock(renderFrameLock);
		return call();
	}

	static HRESULT STDMETHODCALLTYPE VertexLockHook(void* self, UINT offset, UINT size, void** data, DWORD flags)
	{
		return GuardedBufferCall([&] { return originalVertexLock(self, offset, size, data, flags); });
	}

	static HRESULT STDMETHODCALLTYPE VertexUnlockHook(void* self)
	{
		return GuardedBufferCall([&] { return originalVertexUnlock(self); });
	}

	static HRESULT STDMETHODCALLTYPE IndexLockHook(void* self, UINT offset, UINT size, void** data, DWORD flags)
	{
		return GuardedBufferCall([&] { return originalIndexLock(self, offset, size, data, flags); });
	}

	static HRESULT STDMETHODCALLTYPE IndexUnlockHook(void* self)
	{
		return GuardedBufferCall([&] { return originalIndexUnlock(self); });
	}

	static void HookBufferClasses(IDirect3DDevice9* device)
	{
		IDirect3DVertexBuffer9* vertexProbe = nullptr;

		if (SUCCEEDED(device->CreateVertexBuffer(64, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
				&vertexProbe, nullptr)) && vertexProbe)
		{
			if (const auto lock = SwapSlot<BufferLock_t>(vertexProbe, bufferLockSlot, reinterpret_cast<void*>(&VertexLockHook)))
			{
				originalVertexLock = lock;
			}

			if (const auto unlock = SwapSlot<BufferUnlock_t>(vertexProbe, bufferUnlockSlot,
					reinterpret_cast<void*>(&VertexUnlockHook)))
			{
				originalVertexUnlock = unlock;
			}

			vertexProbe->Release();
		}

		IDirect3DIndexBuffer9* indexProbe = nullptr;

		if (SUCCEEDED(device->CreateIndexBuffer(64, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
				&indexProbe, nullptr)) && indexProbe)
		{
			if (const auto lock = SwapSlot<BufferLock_t>(indexProbe, bufferLockSlot, reinterpret_cast<void*>(&IndexLockHook)))
			{
				originalIndexLock = lock;
			}

			if (const auto unlock = SwapSlot<BufferUnlock_t>(indexProbe, bufferUnlockSlot,
					reinterpret_cast<void*>(&IndexUnlockHook)))
			{
				originalIndexUnlock = unlock;
			}

			indexProbe->Release();
		}
	}

	static void HookQueryClass(IDirect3DDevice9* device)
	{
		IDirect3DQuery9* probe = nullptr;

		if (FAILED(device->CreateQuery(D3DQUERYTYPE_EVENT, &probe)) || !probe)
		{
			LogError() << "remix: could not create a probe query, the renderer stays on one thread";
			return;
		}

		const auto getData = SwapSlot<QueryGetData_t>(probe, queryGetDataSlot, reinterpret_cast<void*>(&QueryGetDataHook));

		if (getData)
		{
			originalQueryGetData = getData;
		}

		probe->Release();
	}

	bool IsQueryPollGuarded()
	{
		return originalQueryGetData && originalVertexLock && originalVertexUnlock && originalIndexLock
			&& originalIndexUnlock;
	}

	static void HookTextureClass(IDirect3DDevice9* device)
	{
		IDirect3DTexture9* probe = nullptr;

		if (FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &probe, nullptr)) || !probe)
		{
			LogError() << "remix: could not create a probe texture, texture tags cannot recognise the game's images";
			return;
		}

		const auto release = SwapSlot<Release_t>(probe, textureReleaseSlot, reinterpret_cast<void*>(&TextureReleaseHook));
		const auto lockRect = SwapSlot<LockRect_t>(probe, textureLockRectSlot, reinterpret_cast<void*>(&LockRectHook));
		const auto unlockRect = SwapSlot<UnlockRect_t>(probe, textureUnlockRectSlot, reinterpret_cast<void*>(&UnlockRectHook));

		if (release)
		{
			originalTextureRelease = release;
		}

		if (lockRect)
		{
			originalLockRect = lockRect;
		}

		if (unlockRect)
		{
			originalUnlockRect = unlockRect;
		}

		probe->Release();
	}

	static bool IsCapturing()
	{
		return captureState.load(std::memory_order_relaxed) == CaptureState::Capturing;
	}

	static void PushCapturedEvent(const std::string& text)
	{
		const std::lock_guard lock(captureMutex);
		char prefix[64];
		std::snprintf(prefix, sizeof(prefix), "[%u draws, %u ortho] ", capturedDraws, capturedOrthoDraws);
		capturedEvents.push_back(std::string(prefix) + text);
		capturedDraws = 0;
		capturedOrthoDraws = 0;
	}

	static std::uint64_t CaptureRuntimeTextureHash(IDirect3DBaseTexture9* texture)
	{
		const remixapi_Interface* api = GetReadyApi();
		std::uint64_t hash = 0;
		if (texture && texture->GetType() == D3DRTYPE_TEXTURE && api && api->dxvk_GetTextureHash)
		{
			api->dxvk_GetTextureHash(static_cast<IDirect3DTexture9*>(texture), &hash);
		}
		return hash;
	}

	static bool CaptureRuntimeDrawStatus(IDirect3DDevice9* device, int& injected, int& queries)
	{
		// Diagnostic for the installed Remix Plus build only; verify its device/member layout before reading it.
		static const unsigned char memberInstruction[] = { 0x49, 0x8D, 0x86, 0xF0, 0xE7, 0x02, 0x00 };
		if (!originalDrawIndexedPrimitive || std::memcmp(reinterpret_cast<const unsigned char*>(originalDrawIndexedPrimitive)
			+ 0xFE, memberInstruction, sizeof(memberInstruction)) != 0)
		{
			return false;
		}
		__try
		{
			const auto rtx = reinterpret_cast<std::uintptr_t>(device) + 0x2E7F0;
			if (*reinterpret_cast<IDirect3DDevice9* const*>(rtx + 0x7E05D8) != device) return false;
			injected = *reinterpret_cast<const unsigned char*>(rtx + 0x7E063D);
			queries = *reinterpret_cast<const int*>(rtx + 0x7E0644);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void CountCapturedDraw(IDirect3DDevice9* self, const char* kind, UINT primitives, UINT vertices,
		const void* caller)
	{
		if (!IsCapturing())
		{
			return;
		}

		IDirect3DVertexShader9* vertexShader = nullptr;
		self->GetVertexShader(&vertexShader);
		IDirect3DPixelShader9* pixelShader = nullptr;
		self->GetPixelShader(&pixelShader);
		D3DMATRIX projection{};
		self->GetTransform(D3DTS_PROJECTION, &projection);
		DWORD depthWrite = TRUE;
		self->GetRenderState(D3DRS_ZWRITEENABLE, &depthWrite);
		IDirect3DBaseTexture9* texture0 = nullptr;
		self->GetTexture(0, &texture0);
		std::uint64_t textureHash = 0;
		std::uint64_t runtimeHash = 0;
		if (texture0)
		{
			TryGetTextureHash(texture0, textureHash);
			runtimeHash = CaptureRuntimeTextureHash(texture0);
			texture0->Release();
		}
		DWORD fvf = 0;
		self->GetFVF(&fvf);
		const auto address = reinterpret_cast<std::uintptr_t>(caller);
		int runtimeInjected = -1;
		int runtimeQueries = -1;
		CaptureRuntimeDrawStatus(self, runtimeInjected, runtimeQueries);
		char draw[320];
		std::snprintf(draw, sizeof(draw), "draw %s: %u primitives, %u vertices, %s+0x%llX, texture 0x%016llX GPU 0x%016llX, VS %d PS %d FVF 0x%lX projection44 %.3f zwrite %lu RTinjected %d queries %d",
			kind, primitives, vertices, IsGameCaller(caller) ? "game" : "zw3", static_cast<unsigned long long>(
				address - (IsGameCaller(caller) ? gameImageBegin : reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"zw3.dll")))),
			static_cast<unsigned long long>(textureHash), static_cast<unsigned long long>(runtimeHash),
			vertexShader != nullptr, pixelShader != nullptr, fvf,
			projection._44, depthWrite, runtimeInjected, runtimeQueries);
		PushCapturedEvent(draw);

		// State-block application can restore a projection without calling our SetTransform hook.
		if (!vertexShader && projection._44 == 1.0f && depthWrite == FALSE)
		{
			PushCapturedEvent("possible UI boundary: actual fixed-function orthographic draw");
		}

		IDirect3DVertexDeclaration9* declaration = nullptr;
		self->GetVertexDeclaration(&declaration);

		if (declaration)
		{
			D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH + 1]{};
			UINT count = MAXD3DDECLLENGTH + 1;

			if (SUCCEEDED(declaration->GetDeclaration(elements, &count)))
			{
				for (UINT i = 0; i < count; ++i)
				{
					if (elements[i].Usage == D3DDECLUSAGE_POSITIONT)
					{
						PushCapturedEvent("possible UI boundary: pretransformed POSITIONT draw");
						break;
					}
				}
			}

			declaration->Release();
		}

		for (DWORD stage = 0; stage < 16; ++stage)
		{
			IDirect3DBaseTexture9* texture = nullptr;
			self->GetTexture(stage, &texture);
			std::uint64_t hash = 0;

			if (!texture)
			{
				continue;
			}

			TryGetTextureHash(texture, hash);
			const std::uint64_t gpuHash = CaptureRuntimeTextureHash(texture);
			texture->Release();

			if (gpuHash && gpuHash != hash)
			{
				char text[192];
				std::snprintf(text, sizeof(text), "runtime texture stage %lu: cached 0x%016llX GPU 0x%016llX, tags %s",
					stage, static_cast<unsigned long long>(hash), static_cast<unsigned long long>(gpuHash),
					DescribeTextureTags(gpuHash).c_str());
				PushCapturedEvent(text);
			}

			if (hash && DescribeTextureTags(hash).find("rtx.uiTextures") != std::string::npos)
			{
				DWORD operation = D3DTOP_DISABLE;
				self->GetTextureStageState(stage < 8 ? stage : 7, D3DTSS_COLOROP, &operation);
				char text[192];
				std::snprintf(text, sizeof(text), "possible UI boundary: stage %lu hash 0x%016llX, VS %d PS %d colour op %lu",
					stage, static_cast<unsigned long long>(hash), vertexShader != nullptr, pixelShader != nullptr, operation);
				PushCapturedEvent(text);
			}
		}

		if (pixelShader)
		{
			pixelShader->Release();
		}

		const std::lock_guard lock(captureMutex);
		++capturedDraws;

		if (vertexShader)
		{
			vertexShader->Release();
		}
		else if (isProjectionOrtho)
		{
			++capturedOrthoDraws;
		}
	}

	static HRESULT STDMETHODCALLTYPE SetRenderTargetHook(IDirect3DDevice9* self, DWORD index, IDirect3DSurface9* surface)
	{
		if (IsCapturing())
		{
			D3DSURFACE_DESC description{};

			if (surface)
			{
				surface->GetDesc(&description);
			}

			char text[160];
			std::snprintf(text, sizeof(text), "render target %lu = %p (%ux%u format %d)%s", index,
				static_cast<void*>(surface), description.Width, description.Height, static_cast<int>(description.Format),
				IsGameCaller(_ReturnAddress()) ? " by the game" : " by zw3");
			PushCapturedEvent(text);
		}

		return originalSetRenderTarget(self, index, surface);
	}

	static HRESULT STDMETHODCALLTYPE SetTransformHook(IDirect3DDevice9* self, D3DTRANSFORMSTATETYPE state,
		const D3DMATRIX* matrix)
	{
		if (state == D3DTS_PROJECTION && matrix)
		{
			const bool isOrtho = matrix->_34 == 0.0f && matrix->_44 == 1.0f;

			if (IsCapturing() && isOrtho != isProjectionOrtho)
			{
				char text[200];
				std::snprintf(text, sizeof(text), "projection -> %s (%.3f %.3f %.3f %.3f %.3f)%s",
					isOrtho ? "ORTHOGRAPHIC" : "perspective", matrix->_11, matrix->_22, matrix->_33, matrix->_43,
					matrix->_34, IsGameCaller(_ReturnAddress()) ? " by the game" : " by zw3");
				PushCapturedEvent(text);
			}

			isProjectionOrtho = isOrtho;
		}

		return originalSetTransform(self, state, matrix);
	}

	static HRESULT STDMETHODCALLTYPE DrawPrimitiveHook(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT start, UINT count)
	{
		CountCapturedDraw(self, "primitive", count, 0, _ReturnAddress());
		return originalDrawPrimitive(self, type, start, count);
	}

	static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveHook(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
		INT baseVertex, UINT minIndex, UINT vertices, UINT startIndex, UINT count)
	{
		CountCapturedDraw(self, "indexed", count, vertices, _ReturnAddress());
		return originalDrawIndexedPrimitive(self, type, baseVertex, minIndex, vertices, startIndex, count);
	}

	static HRESULT STDMETHODCALLTYPE DrawPrimitiveUpHook(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT count,
		const void* data, UINT stride)
	{
		CountCapturedDraw(self, "primitive UP", count, 0, _ReturnAddress());
		return originalDrawPrimitiveUp(self, type, count, data, stride);
	}

	static HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUpHook(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
		UINT minIndex, UINT vertices, UINT count, const void* indices, D3DFORMAT format, const void* data, UINT stride)
	{
		CountCapturedDraw(self, "indexed UP", count, vertices, _ReturnAddress());
		return originalDrawIndexedPrimitiveUp(self, type, minIndex, vertices, count, indices, format, data, stride);
	}

	static void HookCaptureSlots(IDirect3DDevice9* device)
	{
		if (const auto hooked = SwapSlot<SetRenderTarget_t>(device, deviceSetRenderTargetSlot,
				reinterpret_cast<void*>(&SetRenderTargetHook)))
		{
			originalSetRenderTarget = hooked;
		}

		if (const auto hooked = SwapSlot<SetTransform_t>(device, deviceSetTransformSlot,
				reinterpret_cast<void*>(&SetTransformHook)))
		{
			originalSetTransform = hooked;
		}

		if (const auto hooked = SwapSlot<DrawPrimitive_t>(device, deviceDrawPrimitiveSlot,
				reinterpret_cast<void*>(&DrawPrimitiveHook)))
		{
			originalDrawPrimitive = hooked;
		}

		if (const auto hooked = SwapSlot<DrawIndexedPrimitive_t>(device, deviceDrawIndexedPrimitiveSlot,
				reinterpret_cast<void*>(&DrawIndexedPrimitiveHook)))
		{
			originalDrawIndexedPrimitive = hooked;
		}

		if (const auto hooked = SwapSlot<DrawPrimitiveUp_t>(device, deviceDrawPrimitiveUpSlot,
				reinterpret_cast<void*>(&DrawPrimitiveUpHook)))
		{
			originalDrawPrimitiveUp = hooked;
		}

		if (const auto hooked = SwapSlot<DrawIndexedPrimitiveUp_t>(device, deviceDrawIndexedPrimitiveUpSlot,
				reinterpret_cast<void*>(&DrawIndexedPrimitiveUpHook)))
		{
			originalDrawIndexedPrimitiveUp = hooked;
		}
	}

	void ArmFrameCapture()
	{
		captureState.store(CaptureState::Armed);
	}

	void AdvanceFrameCapture()
	{
		const CaptureState state = captureState.load();

		if (state == CaptureState::Armed)
		{
			const std::lock_guard lock(captureMutex);
			capturedEvents.clear();
			capturedDraws = 0;
			capturedOrthoDraws = 0;
			captureState.store(CaptureState::Capturing);
			return;
		}

		if (state != CaptureState::Capturing)
		{
			return;
		}

		PushCapturedEvent("present (end of the captured frame)");
		captureState.store(CaptureState::Idle);

		++captureIndex;
		const std::wstring fileName = L"zw3_remix_events_" + std::to_wstring(captureIndex) + L".txt";
		std::ofstream file(Core::ModuleDirectory() + fileName, std::ios::trunc);
		const std::lock_guard lock(captureMutex);

		for (const std::string& line : capturedEvents)
		{
			file << line << "\n";
		}

		Log() << "remix: wrote one frame's events to zw3_remix_events_" << captureIndex << ".txt";
	}

	static HRESULT STDMETHODCALLTYPE ClearHook(IDirect3DDevice9* self, DWORD count, const D3DRECT* rects, DWORD flags,
		D3DCOLOR color, float z, DWORD stencil)
	{
		if (isSceneKept)
		{
			flags &= ~static_cast<DWORD>(D3DCLEAR_TARGET);

			if (!flags)
			{
				return D3D_OK;
			}
		}

		return originalClear(self, count, rects, flags, color, z, stencil);
	}

	void KeepSceneUntilFrameEnd()
	{
		isSceneKept = originalClear != nullptr;
	}

	static HRESULT STDMETHODCALLTYPE BeginSceneHook(IDirect3DDevice9* self)
	{
		const HRESULT result = originalBeginScene(self);

		if (SUCCEEDED(result) && !isInBeginSceneCallbacks)
		{
			isSceneKept = false;
			isInBeginSceneCallbacks = true;

			for (const DeviceCallback callback : beginSceneCallbacks)
			{
				callback(self);
			}

			isInBeginSceneCallbacks = false;
		}

		return result;
	}

	void OnBeginScene(DeviceCallback callback)
	{
		beginSceneCallbacks.push_back(callback);
	}

	HRESULT BeginOwnScene(IDirect3DDevice9* device)
	{
		const bool wasInCallbacks = isInBeginSceneCallbacks;
		isInBeginSceneCallbacks = true;
		const HRESULT result = device->BeginScene();
		isInBeginSceneCallbacks = wasInCallbacks;
		return result;
	}

	static void AttachDevice(IDirect3DDevice9* device)
	{
		const auto reset = SwapSlot<Reset_t>(device, deviceResetSlot, reinterpret_cast<void*>(&ResetHook));
		const auto setRenderState = SwapSlot<SetRenderState_t>(device, deviceSetRenderStateSlot, reinterpret_cast<void*>(&SetRenderStateHook));

		if (reset)
		{
			originalReset = reset;
		}

		if (setRenderState)
		{
			originalSetRenderState = setRenderState;
		}

		if (const auto beginScene = SwapSlot<BeginScene_t>(device, deviceBeginSceneSlot, reinterpret_cast<void*>(&BeginSceneHook)))
		{
			originalBeginScene = beginScene;
		}

		if (const auto clear = SwapSlot<Clear_t>(device, deviceClearSlot, reinterpret_cast<void*>(&ClearHook)))
		{
			originalClear = clear;
		}

		HookTextureClass(device);
		HookQueryClass(device);
		HookBufferClasses(device);
		HookCaptureSlots(device);

		if (createdCallback)
		{
			createdCallback(device);
		}
	}

	static HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice)
	{
		Log() << "startup: creating Direct3D device, behavior flags " << behaviorFlags;
		const HRESULT result = originalCreateDevice(self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDevice);

		if (SUCCEEDED(result) && returnedDevice && *returnedDevice)
		{
			AttachDevice(*returnedDevice);
		}
		Log() << "startup: Direct3D device initialization returned " << std::hex << result;

		return result;
	}

	IDirect3D9* WrapDirect3D(IDirect3D9* direct3D, DeviceCallback onCreated, DeviceCallback onReset)
	{
		if (!direct3D)
		{
			return nullptr;
		}

		MODULEINFO info{};

		if (GetModuleInformation(GetCurrentProcess(), GetModuleHandleW(nullptr), &info, sizeof(info)))
		{
			gameImageBegin = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
			gameImageEnd = gameImageBegin + info.SizeOfImage;
		}

		createdCallback = onCreated;
		resetCallback = onReset;

		const auto createDevice = SwapSlot<CreateDevice_t>(direct3D, direct3DCreateDeviceSlot, reinterpret_cast<void*>(&CreateDeviceHook));

		if (createDevice)
		{
			originalCreateDevice = createDevice;
		}

		return direct3D;
	}

	void NoteFrameEvent(const std::string& text)
	{
		if (IsCapturing())
		{
			PushCapturedEvent(text);
			return;
		}

		Logger::Debug("remix: {}\n", text);
	}

	struct FormatLayout
	{
		UINT blockSize;
		UINT bytesPerBlock;
	};

	static bool TryGetFormatLayout(D3DFORMAT format, FormatLayout& layout)
	{
		switch (static_cast<DWORD>(format))
		{
		case D3DFMT_DXT1:
		case MAKEFOURCC('A', 'T', 'I', '1'):
			layout = { 4, 8 };
			return true;
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
		case MAKEFOURCC('A', 'T', 'I', '2'):
			layout = { 4, 16 };
			return true;
		case D3DFMT_A8:
		case D3DFMT_L8:
		case D3DFMT_A4L4:
			layout = { 1, 1 };
			return true;
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_X4R4G4B4:
		case D3DFMT_A8L8:
		case D3DFMT_V8U8:
		case D3DFMT_L16:
		case D3DFMT_R16F:
			layout = { 1, 2 };
			return true;
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
		case D3DFMT_G16R16:
		case D3DFMT_A2R10G10B10:
		case D3DFMT_A2B10G10R10:
		case D3DFMT_R32F:
		case D3DFMT_G16R16F:
		case D3DFMT_V16U16:
		case D3DFMT_Q8W8V8U8:
			layout = { 1, 4 };
			return true;
		case D3DFMT_A16B16G16R16:
		case D3DFMT_A16B16G16R16F:
		case D3DFMT_G32R32F:
			layout = { 1, 8 };
			return true;
		case D3DFMT_A32B32G32R32F:
			layout = { 1, 16 };
			return true;
		default:
			return false;
		}
	}

	bool TryHashTopLevel(const void* bits, const D3DSURFACE_DESC& desc, std::uint64_t& hash)
	{
		FormatLayout layout{};

		if (!bits || !TryGetFormatLayout(desc.Format, layout))
		{
			return false;
		}

		const UINT blocksWide = (desc.Width + layout.blockSize - 1) / layout.blockSize;
		const UINT blocksHigh = (desc.Height + layout.blockSize - 1) / layout.blockSize;
		const UINT rowBytes = (layout.bytesPerBlock * blocksWide + 3u) & ~3u;
		const std::size_t size = static_cast<std::size_t>(rowBytes) * blocksHigh;

		hash = XXH3_64bits(bits, size);
		return true;
	}

	bool TryGetTextureHash(const void* texture, std::uint64_t& hash)
	{
		const std::lock_guard lock(hashMutex);
		const auto found = textureHashes.find(texture);

		if (found == textureHashes.end())
		{
			return false;
		}

		hash = found->second;
		return hash != 0;
	}

	void BeginEffectBlendRemap(IDirect3DDevice9* device)
	{
		isEffectPass = true;
		originalSetRenderState(device, D3DRS_SRCBLEND, ForwardedSourceBlend());
	}

	void EndEffectBlendRemap(IDirect3DDevice9* device)
	{
		isEffectPass = false;
		originalSetRenderState(device, D3DRS_SRCBLEND, requestedSourceBlend);
	}

	void BeginOverlayBlendRemap(IDirect3DDevice9* device)
	{
		isEffectPass = true;
		isAlphaAsEmissive = true;
		originalSetRenderState(device, D3DRS_SRCBLEND, ForwardedSourceBlend());
		originalSetRenderState(device, D3DRS_DESTBLEND, ForwardedDestinationBlend());
	}

	void EndOverlayBlendRemap(IDirect3DDevice9* device)
	{
		isEffectPass = false;
		isAlphaAsEmissive = false;
		originalSetRenderState(device, D3DRS_SRCBLEND, requestedSourceBlend);
		originalSetRenderState(device, D3DRS_DESTBLEND, requestedDestinationBlend);
	}

	void BeginPremultipliedRemap(IDirect3DDevice9* device)
	{
		isPremultipliedRemap = true;
		originalSetRenderState(device, D3DRS_SRCBLEND, ForwardedSourceBlend());
	}

	void EndPremultipliedRemap(IDirect3DDevice9* device)
	{
		isPremultipliedRemap = false;
		originalSetRenderState(device, D3DRS_SRCBLEND, requestedSourceBlend);
	}
}
