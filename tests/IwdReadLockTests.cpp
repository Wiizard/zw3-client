#include "Components/Modules/IwdReadLock.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace
{
	struct Stream
	{
		std::recursive_mutex mutex;
		bool atHeader = true;
		static void Lock(void* stream) { static_cast<Stream*>(stream)->mutex.lock(); }
		static void Unlock(void* stream) { static_cast<Stream*>(stream)->mutex.unlock(); }
	};

	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	std::uint32_t ReadLength(bool protectHeader)
	{
		Stream stream;
		std::promise<void> headerStarted;
		auto started = headerStarted.get_future();
		std::atomic_bool streamingAttempted = false;
		auto streaming = std::async(std::launch::async, [&]
		{
			started.wait();
			// Native fread/fseek lock each operation, but another seek can split a header read.
			const bool acquired = stream.mutex.try_lock();
			if (acquired)
			{
				stream.atHeader = false;
				stream.mutex.unlock();
			}
			streamingAttempted = true;
			if (!acquired)
			{
				std::lock_guard lock(stream.mutex);
				stream.atHeader = false;
			}
		});

		std::uint32_t length = 0;
		{
			std::optional<Components::IwdReadLock::FileLock> transaction;
			if (protectHeader) transaction.emplace(&stream, Stream::Lock, Stream::Unlock);
			{
				std::lock_guard lock(stream.mutex);
				stream.atHeader = true;
			}
			headerStarted.set_value();
			while (!streamingAttempted) std::this_thread::yield();
			{
				std::lock_guard lock(stream.mutex);
				length = stream.atHeader ? 11300u : 1651012298u;
			}
		}
		Require(streaming.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
			"streaming must resume after reading the header");
		streaming.get();
		return length;
	}
}

int main()
{
	try
	{
		Require(ReadLength(false) == 1651012298u, "interleaved reads must reproduce the corrupt length");
		for (int iteration = 0; iteration < 50; ++iteration)
			Require(ReadLength(true) == 11300u, "a header transaction must preserve the real script length");

		Stream stream;
		try
		{
			const Components::IwdReadLock::FileLock lock(&stream, Stream::Lock, Stream::Unlock);
			throw std::runtime_error("simulated read failure");
		}
		catch (const std::runtime_error&) {}
		auto unlocked = std::async(std::launch::async, [&]
		{
			if (!stream.mutex.try_lock()) return false;
			stream.mutex.unlock();
			return true;
		});
		Require(unlocked.get(), "failed reads must release the file lock");
		const Components::IwdReadLock::FileLock absent(nullptr, nullptr, nullptr);
		std::cout << "IWD reads: interleaved size corruption reproduced; locked headers, recursive reads and error cleanup passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
