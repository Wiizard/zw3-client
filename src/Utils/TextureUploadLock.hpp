#pragma once

namespace Utils
{
	// The native image loader assumes LockRect succeeded before copying pixels.
	// Keep an unlockable GPU texture usable by uploading a CPU staging surface.
	class TextureUploadLock
	{
	public:
		TextureUploadLock() = default;
		TextureUploadLock(const TextureUploadLock&) = delete;
		TextureUploadLock& operator=(const TextureUploadLock&) = delete;
		~TextureUploadLock() { Clear(); }
		bool UsesStaging() const { return this->staging != nullptr; }

		HRESULT Lock(IDirect3DTexture9* texture, UINT level, D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags)
		{
			if (!texture || !locked || this->staging) return D3DERR_INVALIDCALL;
			*locked = {};
			const HRESULT direct = texture->LockRect(level, locked, rect, flags);
			if (SUCCEEDED(direct) && locked->pBits && locked->Pitch > 0) return direct;
			if (SUCCEEDED(direct)) texture->UnlockRect(level);
			*locked = {};

			// This fallback is for the native full-mip write, never a readback or subrect.
			if (rect || flags) return FAILED(direct) ? direct : D3DERR_INVALIDCALL;
			D3DSURFACE_DESC desc{};
			HRESULT result = texture->GetLevelDesc(level, &desc);
			if (FAILED(result)) return result;
			result = texture->GetDevice(&this->device);
			if (FAILED(result) || !this->device) { Clear(); return FAILED(result) ? result : E_FAIL; }
			result = this->device->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format,
				D3DPOOL_SYSTEMMEM, &this->staging, nullptr);
			if (FAILED(result) || !this->staging) { Clear(); return FAILED(result) ? result : E_FAIL; }
			result = this->staging->LockRect(0, locked, nullptr, 0);
			this->stagingLocked = SUCCEEDED(result);
			if (FAILED(result) || !locked->pBits || locked->Pitch <= 0)
			{
				*locked = {};
				Clear();
				return FAILED(result) ? result : D3DERR_INVALIDCALL;
			}

			this->target = texture;
			this->target->AddRef();
			this->targetLevel = level;
			return D3D_OK;
		}

		HRESULT Unlock(IDirect3DTexture9* texture, UINT level)
		{
			if (!this->staging) return texture->UnlockRect(level);
			if (texture != this->target || level != this->targetLevel) return D3DERR_INVALIDCALL;
			HRESULT result = this->staging->UnlockRect(0);
			this->stagingLocked = false;
			IDirect3DSurface9* source = nullptr;
			IDirect3DSurface9* destination = nullptr;
			if (SUCCEEDED(result)) result = this->staging->GetSurfaceLevel(0, &source);
			if (SUCCEEDED(result)) result = texture->GetSurfaceLevel(level, &destination);
			if (SUCCEEDED(result)) result = this->device->UpdateSurface(source, nullptr, destination, nullptr);
			if (destination) destination->Release();
			if (source) source->Release();
			Clear();
			return result;
		}

	private:
		IDirect3DDevice9* device = nullptr;
		IDirect3DTexture9* staging = nullptr;
		IDirect3DTexture9* target = nullptr;
		UINT targetLevel = 0;
		bool stagingLocked = false;

		void Clear()
		{
			if (this->stagingLocked) this->staging->UnlockRect(0);
			if (this->staging) this->staging->Release();
			if (this->target) this->target->Release();
			if (this->device) this->device->Release();
			this->staging = nullptr;
			this->target = nullptr;
			this->device = nullptr;
			this->stagingLocked = false;
		}
	};
}
