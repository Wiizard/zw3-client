#pragma once

#include <d3d9.h>
#include <algorithm>
#include <cstring>

namespace Utils
{
	// One image at a time: fill its mip chain in CPU memory, then upload it once.
	// The final texture has the same dynamic/default-pool properties as before.
	class StagedTextureUpload
	{
	public:
		StagedTextureUpload() = default;
		StagedTextureUpload(const StagedTextureUpload&) = delete;
		StagedTextureUpload& operator=(const StagedTextureUpload&) = delete;
		~StagedTextureUpload()
		{
			if (source_) source_->Release();
			if (destination_) destination_->Release();
		}

		bool Begin(IDirect3DDevice9* device, UINT width, UINT height, UINT levels,
			D3DFORMAT format, IDirect3DTexture9** output)
		{
			if (source_ || !device || !output) return false;
			IDirect3DTexture9* destination = nullptr;
			if (FAILED(device->CreateTexture(width, height, levels, D3DUSAGE_DYNAMIC,
				format, D3DPOOL_DEFAULT, &destination, nullptr))) return false;

			IDirect3DTexture9* source = nullptr;
			if (FAILED(device->CreateTexture(width, height, levels, 0,
				format, D3DPOOL_SYSTEMMEM, &source, nullptr)))
			{
				destination->Release();
				return false;
			}

			device_ = device;
			source_ = source;
			destination_ = destination;
			output_ = output;
			source_->AddRef(); // Separate guard reference from the engine's ownership.
			*output = source;
			return true;
		}

		bool Pending() const { return destination_ != nullptr; }
		bool UsedFallback() const { return usedFallback_; }

		HRESULT Finish()
		{
			if (!Pending() || *output_ != source_) return S_FALSE;
			// Explicitly dirty the entire chain, regardless of mip upload order.
			auto result = source_->AddDirtyRect(nullptr);
			if (SUCCEEDED(result)) result = device_->UpdateTexture(source_, destination_);
			if (FAILED(result))
			{
				usedFallback_ = true;
				result = CopyMipChain(source_, destination_);
			}

			// Never publish a system-memory texture for rendering, even on failure.
			*output_ = SUCCEEDED(result) ? destination_ : nullptr;
			if (SUCCEEDED(result)) destination_ = nullptr; // Transfer to the engine.
			source_->Release(); // Drop the engine's old ownership; retain our guard.
			return result;
		}

		// Compatibility fallback for a driver which rejects UpdateTexture.
		static HRESULT CopyMipChain(IDirect3DTexture9* source, IDirect3DTexture9* destination)
		{
			if (source->GetLevelCount() != destination->GetLevelCount()) return D3DERR_INVALIDCALL;
			for (UINT level = 0; level < source->GetLevelCount(); ++level)
			{
				D3DSURFACE_DESC from{}, to{};
				auto result = source->GetLevelDesc(level, &from);
				if (FAILED(result)) return result;
				result = destination->GetLevelDesc(level, &to);
				if (FAILED(result)) return result;
				if (from.Width != to.Width || from.Height != to.Height || from.Format != to.Format)
					return D3DERR_INVALIDCALL;

				D3DLOCKED_RECT read{}, write{};
				result = source->LockRect(level, &read, nullptr, D3DLOCK_READONLY);
				if (FAILED(result)) return result;
				result = destination->LockRect(level, &write, nullptr, 0);
				if (FAILED(result))
				{
					source->UnlockRect(level);
					return result;
				}

				const bool compressed = from.Format >= D3DFMT_DXT1 && from.Format <= D3DFMT_DXT5;
				const auto rows = compressed ? (from.Height + 3) / 4 : from.Height;
				if (read.Pitch <= 0 || write.Pitch <= 0) result = D3DERR_INVALIDCALL;
				else for (UINT row = 0; row < rows; ++row)
				{
					std::memcpy(static_cast<char*>(write.pBits) + row * write.Pitch,
						static_cast<const char*>(read.pBits) + row * read.Pitch,
						static_cast<std::size_t>((std::min)(read.Pitch, write.Pitch)));
				}
				const auto unlockWrite = destination->UnlockRect(level);
				const auto unlockRead = source->UnlockRect(level);
				if (FAILED(result)) return result;
				if (FAILED(unlockWrite)) return unlockWrite;
				if (FAILED(unlockRead)) return unlockRead;
			}
			return D3D_OK;
		}

	private:
		IDirect3DDevice9* device_ = nullptr; // Loader keeps the device alive.
		IDirect3DTexture9* source_ = nullptr;
		IDirect3DTexture9* destination_ = nullptr;
		IDirect3DTexture9** output_ = nullptr;
		bool usedFallback_ = false;
	};
}
