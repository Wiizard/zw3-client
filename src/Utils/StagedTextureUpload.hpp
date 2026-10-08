#pragma once

namespace Utils
{
	class StagedTextureUpload
	{
	public:
		struct Shape
		{
			UINT width;
			UINT height;
			UINT levels;
			D3DFORMAT format;
		};

		StagedTextureUpload() = default;
		StagedTextureUpload(const StagedTextureUpload&) = delete;
		StagedTextureUpload& operator=(const StagedTextureUpload&) = delete;

		~StagedTextureUpload()
		{
			if (this->source)
			{
				this->source->Release();
			}

			if (this->destination)
			{
				this->destination->Release();
			}
		}

		bool TryBegin(IDirect3DDevice9* targetDevice, const Shape& shape, IDirect3DTexture9** output)
		{
			if (this->source || !targetDevice || !output)
			{
				return false;
			}

			IDirect3DTexture9* createdDestination = nullptr;

			if (FAILED(targetDevice->CreateTexture(shape.width, shape.height, shape.levels, D3DUSAGE_DYNAMIC, shape.format, D3DPOOL_DEFAULT, &createdDestination, nullptr)))
			{
				return false;
			}

			IDirect3DTexture9* createdSource = nullptr;

			if (FAILED(targetDevice->CreateTexture(shape.width, shape.height, shape.levels, 0, shape.format, D3DPOOL_SYSTEMMEM, &createdSource, nullptr)))
			{
				createdDestination->Release();
				return false;
			}

			this->device = targetDevice;
			this->source = createdSource;
			this->destination = createdDestination;
			this->engineTexture = output;

			this->source->AddRef();
			*output = this->source;

			return true;
		}

		bool IsPending() const
		{
			return this->destination != nullptr;
		}

		HRESULT Finish()
		{
			if (!this->IsPending() || *this->engineTexture != this->source)
			{
				return S_FALSE;
			}

			HRESULT result = this->source->AddDirtyRect(nullptr);

			if (SUCCEEDED(result))
			{
				result = this->device->UpdateTexture(this->source, this->destination);
			}

			if (FAILED(result))
			{
				result = CopyMipChain(this->source, this->destination);
			}

			if (SUCCEEDED(result))
			{
				*this->engineTexture = this->destination;
				this->destination = nullptr;
			}
			else
			{
				*this->engineTexture = nullptr;
			}

			this->source->Release();

			return result;
		}

	private:
		IDirect3DDevice9* device = nullptr;
		IDirect3DTexture9* source = nullptr;
		IDirect3DTexture9* destination = nullptr;
		IDirect3DTexture9** engineTexture = nullptr;

		static HRESULT CopyMipChain(IDirect3DTexture9* from, IDirect3DTexture9* to)
		{
			const DWORD levelCount = from->GetLevelCount();

			if (levelCount != to->GetLevelCount())
			{
				return D3DERR_INVALIDCALL;
			}

			for (UINT level = 0; level < levelCount; ++level)
			{
				D3DSURFACE_DESC fromDesc{};
				D3DSURFACE_DESC toDesc{};

				HRESULT result = from->GetLevelDesc(level, &fromDesc);

				if (FAILED(result))
				{
					return result;
				}

				result = to->GetLevelDesc(level, &toDesc);

				if (FAILED(result))
				{
					return result;
				}

				if (fromDesc.Width != toDesc.Width || fromDesc.Height != toDesc.Height || fromDesc.Format != toDesc.Format)
				{
					return D3DERR_INVALIDCALL;
				}

				D3DLOCKED_RECT read{};
				D3DLOCKED_RECT write{};

				result = from->LockRect(level, &read, nullptr, D3DLOCK_READONLY);

				if (FAILED(result))
				{
					return result;
				}

				result = to->LockRect(level, &write, nullptr, 0);

				if (FAILED(result))
				{
					from->UnlockRect(level);
					return result;
				}

				const bool isCompressed = fromDesc.Format >= D3DFMT_DXT1 && fromDesc.Format <= D3DFMT_DXT5;
				UINT rowCount = fromDesc.Height;

				if (isCompressed)
				{
					rowCount = (fromDesc.Height + 3) / 4;
				}

				if (read.Pitch <= 0 || write.Pitch <= 0)
				{
					result = D3DERR_INVALIDCALL;
				}
				else
				{
					const auto readPitch = static_cast<std::size_t>(read.Pitch);
					const auto writePitch = static_cast<std::size_t>(write.Pitch);
					const std::size_t rowBytes = std::min(readPitch, writePitch);

					for (UINT row = 0; row < rowCount; ++row)
					{
						const auto* const fromRow = static_cast<const std::uint8_t*>(read.pBits) + row * readPitch;
						auto* const toRow = static_cast<std::uint8_t*>(write.pBits) + row * writePitch;

						std::memcpy(toRow, fromRow, rowBytes);
					}
				}

				const HRESULT unlockWrite = to->UnlockRect(level);
				const HRESULT unlockRead = from->UnlockRect(level);

				if (FAILED(result))
				{
					return result;
				}

				if (FAILED(unlockWrite))
				{
					return unlockWrite;
				}

				if (FAILED(unlockRead))
				{
					return unlockRead;
				}
			}

			return D3D_OK;
		}
	};
}
