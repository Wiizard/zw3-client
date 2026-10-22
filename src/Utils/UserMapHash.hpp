#pragma once

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "Bcrypt.lib")

namespace Utils
{
	// The wire checksum remains SHA256(file bytes), in userMapFiles order,
	// followed by the existing Jenkins aggregate. Only file I/O and caching change.
	class UserMapFileHashes
	{
	public:
		std::optional<std::string> Get(const std::filesystem::path& path,
			const std::function<bool()>& isCancelled = {})
		{
			return this->Read(path, false, isCancelled);
		}

		// Main-thread join validation may only consume a prewarmed, unchanged
		// checksum. It must not start hashing or wait for another cache caller.
		std::optional<std::string> GetCached(const std::filesystem::path& path)
		{
			return this->Read(path, true, {});
		}

	private:
		std::optional<std::string> Read(const std::filesystem::path& path, const bool cacheOnly,
			const std::function<bool()>& isCancelled)
		{
			if (isCancelled && isCancelled()) return std::nullopt;
			std::error_code error;
			auto normalized = std::filesystem::absolute(path, error).lexically_normal().wstring();
			if (error) return std::nullopt;
			for (auto& character : normalized) character = static_cast<wchar_t>(std::towlower(character));

			// Deny writers and deletion while hashing, including handles that were
			// already opened for writing. Never hash a partly installed map file.
			File file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
			if (file.handle == INVALID_HANDLE_VALUE || GetFileType(file.handle) != FILE_TYPE_DISK) return std::nullopt;
			const auto before = Stamp::Read(file.handle);
			if (!before) return std::nullopt;

			{
				std::unique_lock lock(this->mutex, std::defer_lock);
				if (cacheOnly)
				{
					if (!lock.try_lock()) return std::nullopt;
				}
				else lock.lock();
				const auto cached = this->cache.find(normalized);
				if (cached != this->cache.end() && cached->second.stamp == *before)
					return cached->second.hash;
			}
			if (cacheOnly) return std::nullopt;

			const auto hash = Hash(file.handle, before->size, isCancelled);
			const auto after = Stamp::Read(file.handle);
			if (!hash || !after || *before != *after || (isCancelled && isCancelled())) return std::nullopt;

			{
				// Hashing and disk I/O never hold this lock. Main-thread cache hits
				// are not blocked behind a large file being read by preflight.
				std::lock_guard lock(this->mutex);
				this->cache.insert_or_assign(std::move(normalized), Cached{ *after, *hash });
			}
			return hash;
		}

		struct File
		{
			explicit File(HANDLE value) : handle(value) {}
			~File() { if (this->handle != INVALID_HANDLE_VALUE) CloseHandle(this->handle); }
			File(const File&) = delete;
			File& operator=(const File&) = delete;
			HANDLE handle;
		};

		struct Stamp
		{
			std::uint64_t size;
			std::uint64_t modified;
			std::uint64_t fileIndex;
			DWORD volume;
			bool operator==(const Stamp&) const = default;

			static std::optional<Stamp> Read(HANDLE file)
			{
				BY_HANDLE_FILE_INFORMATION info{};
				if (!GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					return std::nullopt;
				return Stamp{
					(static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow,
					(static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) | info.ftLastWriteTime.dwLowDateTime,
					(static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow,
					info.dwVolumeSerialNumber
				};
			}
		};

		struct Cached
		{
			Stamp stamp;
			std::string hash;
		};

		static std::optional<std::string> Hash(HANDLE file, const std::uint64_t expectedSize,
			const std::function<bool()>& isCancelled)
		{
			struct HashState
			{
				BCRYPT_ALG_HANDLE algorithm{};
				BCRYPT_HASH_HANDLE hash{};
				std::vector<unsigned char> object;
				~HashState()
				{
					if (this->hash) BCryptDestroyHash(this->hash);
					if (this->algorithm) BCryptCloseAlgorithmProvider(this->algorithm, 0);
				}
			};
			HashState state;
			if (BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
				return std::nullopt;
			DWORD objectLength = 0, digestLength = 0, written = 0;
			if (BCryptGetProperty(state.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength),
				sizeof(objectLength), &written, 0) < 0 || written != sizeof(objectLength)
				|| BCryptGetProperty(state.algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestLength),
					sizeof(digestLength), &written, 0) < 0 || written != sizeof(digestLength) || digestLength != 32)
				return std::nullopt;
			state.object.resize(objectLength);
			if (BCryptCreateHash(state.algorithm, &state.hash, state.object.data(), objectLength, nullptr, 0, 0) < 0)
				return std::nullopt;

			std::vector<unsigned char> buffer(1024 * 1024);
			std::uint64_t total = 0;
			for (;;)
			{
				if (isCancelled && isCancelled()) return std::nullopt;
				DWORD bytesRead = 0;
				if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
					return std::nullopt;
				if (!bytesRead) break;
				if (total > expectedSize || bytesRead > expectedSize - total
					|| BCryptHashData(state.hash, buffer.data(), bytesRead, 0) < 0) return std::nullopt;
				total += bytesRead;
			}
			if (total != expectedSize) return std::nullopt;
			std::array<unsigned char, 32> digest{};
			if (BCryptFinishHash(state.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
				return std::nullopt;
			return std::string(reinterpret_cast<const char*>(digest.data()), digest.size());
		}

		std::mutex mutex;
		std::unordered_map<std::wstring, Cached> cache;
	};
}
