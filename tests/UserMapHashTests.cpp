#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Utils/UserMapHash.hpp"

#include <tomcrypt.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
	void Require(const bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}

	std::string OldSha256(const std::string& bytes)
	{
		hash_state state{};
		std::array<unsigned char, 32> digest{};
		Require(sha256_init(&state) == CRYPT_OK, "old SHA256 initialization failed");
		Require(sha256_process(&state, reinterpret_cast<const unsigned char*>(bytes.data()),
			static_cast<unsigned long>(bytes.size())) == CRYPT_OK, "old SHA256 update failed");
		Require(sha256_done(&state, digest.data()) == CRYPT_OK, "old SHA256 finalization failed");
		return std::string(reinterpret_cast<const char*>(digest.data()), digest.size());
	}

	// Preserve the existing algorithm's signed-char arithmetic as well as
	// concatenation order. No map checksum/protocol migration is intended.
	std::uint32_t Jenkins(const std::string& bytes)
	{
		std::uint32_t hash = 0;
		for (const char byte : bytes)
		{
			hash += byte;
			hash += hash << 10;
			hash ^= hash >> 6;
		}
		hash += hash << 3;
		hash ^= hash >> 11;
		hash += hash << 15;
		return hash;
	}

	void Write(const std::filesystem::path& path, const std::string& data)
	{
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		stream.write(data.data(), static_cast<std::streamsize>(data.size()));
		stream.close();
		Require(static_cast<bool>(stream), "fixture write failed");
	}

	struct Fixtures
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() /
			("zwnet-usermap-hash-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
		std::vector<std::filesystem::path> paths;
		Fixtures() { Require(std::filesystem::create_directory(root), "fixture directory was not newly created"); }
		~Fixtures()
		{
			std::error_code error;
			for (const auto& path : paths) std::filesystem::remove(path, error);
			std::filesystem::remove(root, error);
		}
		std::filesystem::path Add(const char* name, const std::string& data)
		{
			auto path = root / name;
			paths.push_back(path);
			Write(path, data);
			return path;
		}
	};
}

int main()
{
	try
	{
		Fixtures fixtures;
		Utils::UserMapFileHashes cache;
		std::string large(2 * 1024 * 1024 + 17, '\0');
		for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<char>(i % 251);
		const std::array<std::string, 4> names{ "map.ff", "map_load.ff", "map.iwd", "map.arena" };
		const std::array<std::string, 4> contents{ large, std::string("a\0b\xff", 4), "abc", "{ map test }\n" };
		std::array<std::filesystem::path, 4> maps;
		std::string oldAggregate, newAggregate;
		for (std::size_t i = 0; i < names.size(); ++i)
		{
			maps[i] = fixtures.Add(names[i].c_str(), contents[i]);
			Require(!cache.GetCached(maps[i]), "cold cache started hashing content");
			const auto actual = cache.Get(maps[i]);
			const auto expected = OldSha256(contents[i]);
			Require(actual && *actual == expected, "streamed SHA256 differs from existing libtomcrypt SHA256");
			Require(cache.GetCached(maps[i]) == actual, "prewarmed cache did not return checksum");
			oldAggregate += expected;
			newAggregate += *actual;
		}
		Require(Jenkins(oldAggregate) == Jenkins(newAggregate), "four-file map checksum changed");
		Require(Jenkins(newAggregate) == 407626158u, "four-file ordering or signed-char aggregation changed");
		const auto empty = fixtures.Add("empty.bin", "");
		Require(cache.Get(empty) == OldSha256(""), "empty-file SHA256 changed");

		const auto originalTime = std::filesystem::last_write_time(maps[2]);
		Write(maps[2], "xyz");
		std::filesystem::last_write_time(maps[2], originalTime + std::chrono::seconds(2));
		Require(!cache.GetCached(maps[2]), "same-size timestamp change reused stale checksum");
		Require(cache.Get(maps[2]) == OldSha256("xyz"), "same-size update checksum incorrect");
		Write(maps[2], "longer");
		std::filesystem::last_write_time(maps[2], originalTime + std::chrono::seconds(2));
		Require(!cache.GetCached(maps[2]), "size-only change reused stale checksum");
		Require(cache.Get(maps[2]) == OldSha256("longer"), "size-only update checksum incorrect");

		// A replacement retaining size/mtime still has a different file identity.
		const auto replacement = fixtures.Add("replacement.bin", "change");
		std::filesystem::last_write_time(replacement, std::filesystem::last_write_time(maps[2]));
		Require(MoveFileExW(replacement.c_str(), maps[2].c_str(), MOVEFILE_REPLACE_EXISTING) != 0, "replace fixture failed");
		Require(!cache.GetCached(maps[2]), "replacement reused stale checksum");
		Require(cache.Get(maps[2]) == OldSha256("change"), "replacement checksum incorrect");

		// Even a valid cache entry must not make a file open for writing appear
		// verified. No digest of an incomplete installation is returned/cached.
		const auto writer = CreateFileW(maps[2].c_str(), GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
		Require(writer != INVALID_HANDLE_VALUE, "writer fixture failed");
		const bool rejectedWriter = !cache.Get(maps[2]) && !cache.GetCached(maps[2]);
		CloseHandle(writer);
		Require(rejectedWriter, "actively written file was treated as verified");
		Require(cache.Get(maps[2]) == OldSha256("change"), "temporary read failure poisoned cache");
		Require(!cache.Get(fixtures.root / "missing.bin"), "missing file returned fabricated SHA256");
		Require(!cache.Get(fixtures.root), "directory returned a file checksum");

		Utils::UserMapFileHashes cancelledCache;
		int cancellationChecks = 0;
		Require(!cancelledCache.Get(maps[0], [&] { return ++cancellationChecks >= 3; }), "streaming hash ignored cancellation");
		Require(cancellationChecks == 3 && !cancelledCache.GetCached(maps[0]), "partial cancelled hash was cached");
		Require(cancelledCache.Get(maps[0]) == OldSha256(large), "cancellation poisoned later hashes");
		Require(!cancelledCache.Get(maps[0], [] { return true; }), "cached hash ignored cancellation");

		std::atomic<bool> correct{ true };
		std::vector<std::thread> readers;
		const auto expectedLarge = OldSha256(large);
		for (int i = 0; i < 8; ++i)
			readers.emplace_back([&]
			{
				for (int j = 0; j < 50; ++j)
					if (cache.Get(maps[0]) != expectedLarge) correct.store(false);
			});
		for (auto& reader : readers) reader.join();
		Require(correct.load(), "concurrent cache reads produced incorrect checksum");
		std::cout << "Usermap SHA256 compatibility, cache-only, invalidation, failure, cancellation and concurrency tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
