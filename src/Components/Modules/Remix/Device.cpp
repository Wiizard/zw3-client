#include "STDInclude.hpp"

#include "Device.hpp"
#include "Core.hpp"

#include "Components/Modules/Logger.hpp"

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace Components::Remix::Device
{
	// vtable slots of the d3d9 interfaces, in declaration order of d3d9.h
	constexpr std::size_t direct3DCreateDeviceSlot = 16;
	constexpr std::size_t deviceResetSlot = 16;
	constexpr std::size_t deviceSetRenderStateSlot = 57;
	constexpr std::size_t textureReleaseSlot = 2;
	constexpr std::size_t textureLockRectSlot = 19;
	constexpr std::size_t textureUnlockRectSlot = 20;

	using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice);
	using Reset_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentationParameters);
	using SetRenderState_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value);
	using Release_t = ULONG(STDMETHODCALLTYPE*)(IDirect3DTexture9* self);
	using LockRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* lockedRect, const RECT* rect, DWORD flags);
	using UnlockRect_t = HRESULT(STDMETHODCALLTYPE*)(IDirect3DTexture9* self, UINT level);

	static CreateDevice_t originalCreateDevice = nullptr;
	static Reset_t originalReset = nullptr;
	static SetRenderState_t originalSetRenderState = nullptr;
	static Release_t originalTextureRelease = nullptr;
	static LockRect_t originalLockRect = nullptr;
	static UnlockRect_t originalUnlockRect = nullptr;

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

	// the game's blend states are remapped while an effect pass is open; our own draws pass through untouched
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

		HookTextureClass(device);

		if (createdCallback)
		{
			createdCallback(device);
		}
	}

	static HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice9** returnedDevice)
	{
		const HRESULT result = originalCreateDevice(self, adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDevice);

		if (SUCCEEDED(result) && returnedDevice && *returnedDevice)
		{
			AttachDevice(*returnedDevice);
		}

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
