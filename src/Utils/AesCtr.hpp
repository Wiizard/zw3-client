#pragma once

#include <bcrypt.h>

#pragma comment(lib, "Bcrypt.lib")

namespace Utils
{
	class AesCtr
	{
	public:
		AesCtr() = default;
		AesCtr(const AesCtr&) = delete;
		AesCtr& operator=(const AesCtr&) = delete;

		~AesCtr() = default;

		bool Start(const std::uint8_t* keyBytes, std::size_t keyLength, const std::uint8_t* iv)
		{
			this->Reset();

			if (this->TryStartBCrypt(keyBytes, keyLength))
			{
				this->isBCrypt = true;

				if (!this->SetIv(iv))
				{
					this->Reset();
					return false;
				}

				this->isReady = true;
				return true;
			}

			register_cipher(&aes_desc);
			const int aes = find_cipher("aes");

			if (aes < 0 || ctr_start(aes, iv, keyBytes, static_cast<int>(keyLength), 0, CTR_COUNTER_LITTLE_ENDIAN, &this->fallback) != CRYPT_OK)
			{
				return false;
			}

			this->isReady = true;
			return true;
		}

		bool IsReady() const
		{
			return this->isReady;
		}

		bool SetIv(const std::uint8_t* iv)
		{
			if (!this->isBCrypt)
			{
				return ctr_setiv(iv, blockSize, &this->fallback) == CRYPT_OK;
			}

			std::memcpy(this->counter.data(), iv, blockSize);
			std::memcpy(this->pad.data(), iv, blockSize);
			this->padUsed = 0;
			return this->EncryptBlocks(this->pad.data(), 1);
		}

		bool Crypt(std::uint8_t* data, std::size_t length)
		{
			if (!this->isBCrypt)
			{
				return ctr_decrypt(data, data, static_cast<unsigned long>(length), &this->fallback) == CRYPT_OK;
			}

			while (length)
			{
				if (this->padUsed < blockSize)
				{
					const auto take = std::min(length, blockSize - this->padUsed);

					for (std::size_t i = 0; i < take; ++i)
					{
						data[i] ^= this->pad[this->padUsed + i];
					}

					this->padUsed += take;
					data += take;
					length -= take;
					continue;
				}

				const auto blocks = std::min(length / blockSize, batchBlocks);

				if (!blocks)
				{
					this->Increment();
					std::memcpy(this->pad.data(), this->counter.data(), blockSize);

					if (!this->EncryptBlocks(this->pad.data(), 1))
					{
						return false;
					}

					this->padUsed = 0;
					continue;
				}

				for (std::size_t i = 0; i < blocks; ++i)
				{
					this->Increment();
					std::memcpy(this->stream.data() + i * blockSize, this->counter.data(), blockSize);
				}

				if (!this->EncryptBlocks(this->stream.data(), blocks))
				{
					return false;
				}

				for (std::size_t i = 0; i < blocks * blockSize; ++i)
				{
					data[i] ^= this->stream[i];
				}

				std::memcpy(this->pad.data(), this->stream.data() + (blocks - 1) * blockSize, blockSize);
				this->padUsed = blockSize;
				data += blocks * blockSize;
				length -= blocks * blockSize;
			}

			return true;
		}

		void Reset()
		{
			if (this->isReady && !this->isBCrypt)
			{
				ctr_done(&this->fallback);
			}

			if (this->key)
			{
				BCryptDestroyKey(this->key);
				this->key = nullptr;
			}

			if (this->algorithm)
			{
				BCryptCloseAlgorithmProvider(this->algorithm, 0);
				this->algorithm = nullptr;
			}

			this->isReady = false;
			this->isBCrypt = false;
		}

	private:
		static constexpr std::size_t blockSize = 16;
		static constexpr std::size_t batchBlocks = 512;

		bool TryStartBCrypt(const std::uint8_t* keyBytes, std::size_t keyLength)
		{
			if (BCryptOpenAlgorithmProvider(&this->algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
			{
				this->algorithm = nullptr;
				return false;
			}

			const bool isEcb = BCryptSetProperty(this->algorithm, BCRYPT_CHAINING_MODE,
				reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)), sizeof(BCRYPT_CHAIN_MODE_ECB), 0) >= 0;

			if (!isEcb || BCryptGenerateSymmetricKey(this->algorithm, &this->key, nullptr, 0,
				const_cast<PUCHAR>(keyBytes), static_cast<ULONG>(keyLength), 0) < 0)
			{
				this->key = nullptr;
				BCryptCloseAlgorithmProvider(this->algorithm, 0);
				this->algorithm = nullptr;
				return false;
			}

			this->stream.resize(batchBlocks * blockSize);
			return true;
		}

		void Increment()
		{
			for (auto& byte : this->counter)
			{
				if (++byte != 0)
				{
					break;
				}
			}
		}

		bool EncryptBlocks(std::uint8_t* blocks, std::size_t count)
		{
			const auto bytes = static_cast<ULONG>(count * blockSize);
			ULONG written = 0;
			return BCryptEncrypt(this->key, blocks, bytes, nullptr, nullptr, 0, blocks, bytes, &written, 0) >= 0 && written == bytes;
		}

		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_KEY_HANDLE key = nullptr;
		std::array<std::uint8_t, blockSize> counter{};
		std::array<std::uint8_t, blockSize> pad{};
		std::size_t padUsed = blockSize;
		std::vector<std::uint8_t> stream;
		symmetric_CTR fallback{};
		bool isBCrypt = false;
		bool isReady = false;
	};
}
