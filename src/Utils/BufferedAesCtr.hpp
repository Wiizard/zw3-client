#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#pragma comment(lib, "Bcrypt.lib")

namespace Utils::Cryptography
{
	// Compatible with LibTomCrypt's full-width little-endian AES-CTR mode.
	// DB_ReadXFile often requests just a few bytes. Retain an entire batch
	// across those reads instead of calling the crypto provider per AES block.
	class BufferedAesCtr
	{
	public:
		static constexpr std::size_t BlockSize = 16;
		static constexpr std::size_t BatchSize = 64 * 1024;

		BufferedAesCtr() = default;
		BufferedAesCtr(const BufferedAesCtr&) = delete;
		BufferedAesCtr& operator=(const BufferedAesCtr&) = delete;

		~BufferedAesCtr()
		{
			this->reset();
		}

		bool initialize(const unsigned char* nonce, const unsigned char* key, const ULONG keySize)
		{
			this->reset();
			if (!nonce || !key || !keySize)
			{
				return false;
			}

			if (BCryptOpenAlgorithmProvider(&this->algorithm_, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0 ||
				BCryptSetProperty(this->algorithm_, BCRYPT_CHAINING_MODE,
					reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)), sizeof(BCRYPT_CHAIN_MODE_ECB), 0) < 0)
			{
				this->reset();
				return false;
			}

			DWORD objectSize = 0;
			DWORD resultSize = 0;
			if (BCryptGetProperty(this->algorithm_, BCRYPT_OBJECT_LENGTH,
				reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &resultSize, 0) < 0 || !objectSize)
			{
				this->reset();
				return false;
			}

			this->keyObject_.resize(objectSize);
			if (BCryptGenerateSymmetricKey(this->algorithm_, &this->key_, this->keyObject_.data(), objectSize,
				const_cast<PUCHAR>(key), keySize, 0) < 0)
			{
				this->reset();
				return false;
			}

			this->counterBlocks_.resize(BatchSize);
			this->keyStream_.resize(BatchSize);
			return this->restart(nonce);
		}

		bool restart(const unsigned char* nonce)
		{
			if (!this->key_ || !nonce) return false;
			std::memcpy(this->counter_.data(), nonce, this->counter_.size());
			this->keyStreamOffset_ = BatchSize;
			return true;
		}

		void reset()
		{
			if (this->key_)
			{
				BCryptDestroyKey(this->key_);
				this->key_ = nullptr;
			}
			if (this->algorithm_)
			{
				BCryptCloseAlgorithmProvider(this->algorithm_, 0);
				this->algorithm_ = nullptr;
			}
			this->keyObject_.clear();
			this->counterBlocks_.clear();
			this->keyStream_.clear();
			this->counter_.fill(0);
			this->keyStreamOffset_ = BatchSize;
		}

		bool decrypt(unsigned char* data, std::size_t size)
		{
			if (!this->key_ || (!data && size))
			{
				return false;
			}

			while (size)
			{
				if (this->keyStreamOffset_ == BatchSize && !this->refill())
				{
					return false;
				}

				const auto count = (std::min)(size, BatchSize - this->keyStreamOffset_);
				const auto* stream = this->keyStream_.data() + this->keyStreamOffset_;
				std::size_t i = 0;
				for (; i + sizeof(std::uint64_t) <= count; i += sizeof(std::uint64_t))
				{
					std::uint64_t input = 0;
					std::uint64_t mask = 0;
					std::memcpy(&input, data + i, sizeof(input));
					std::memcpy(&mask, stream + i, sizeof(mask));
					input ^= mask;
					std::memcpy(data + i, &input, sizeof(input));
				}
				for (; i < count; ++i)
				{
					data[i] ^= stream[i];
				}

				this->keyStreamOffset_ += count;
				data += count;
				size -= count;
			}
			return true;
		}

	private:
		bool refill()
		{
			for (std::size_t offset = 0; offset < BatchSize; offset += BlockSize)
			{
				std::memcpy(this->counterBlocks_.data() + offset, this->counter_.data(), BlockSize);
				for (auto& byte : this->counter_)
				{
					if (++byte != 0) break;
				}
			}

			ULONG written = 0;
			if (BCryptEncrypt(this->key_, this->counterBlocks_.data(), static_cast<ULONG>(BatchSize),
				nullptr, nullptr, 0, this->keyStream_.data(), static_cast<ULONG>(BatchSize), &written, 0) < 0 || written != BatchSize)
			{
				return false;
			}
			this->keyStreamOffset_ = 0;
			return true;
		}

		BCRYPT_ALG_HANDLE algorithm_ = nullptr;
		BCRYPT_KEY_HANDLE key_ = nullptr;
		std::vector<unsigned char> keyObject_;
		std::vector<unsigned char> counterBlocks_;
		std::vector<unsigned char> keyStream_;
		std::array<unsigned char, BlockSize> counter_{};
		std::size_t keyStreamOffset_ = BatchSize;
	};
}
