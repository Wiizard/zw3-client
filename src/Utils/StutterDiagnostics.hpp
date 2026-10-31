#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>
#include <vector>

namespace Utils::StutterDiagnostics
{
	inline std::int64_t Now()
	{
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	struct Entry
	{
		std::int64_t started = 0;
		std::int64_t duration = 0;
		std::size_t thread = 0;
		std::string_view category;
		std::source_location source;
	};

	struct Capture
	{
		std::int64_t started = 0;
		std::uint64_t overwritten = 0;
		std::vector<Entry> entries;
	};

	class Recorder
	{
	public:
		static constexpr std::size_t capacity = 4096;

		void Start(std::int64_t now)
		{
			const std::lock_guard lock(mutex);
			written = 0;
			started = now;
			token.store(++generation, std::memory_order_release);
		}

		std::uint64_t Token() const { return token.load(std::memory_order_acquire); }

		void Add(std::uint64_t captureToken, Entry entry, std::int64_t threshold)
		{
			if (!captureToken || entry.duration < threshold || Token() != captureToken) return;
			const std::lock_guard lock(mutex);
			if (Token() != captureToken) return;
			entries[written++ % capacity] = entry;
		}

		Capture Stop()
		{
			const std::lock_guard lock(mutex);
			token.store(0, std::memory_order_release);
			Capture result;
			result.started = started;
			result.overwritten = written > capacity ? written - capacity : 0;
			const auto count = written < capacity ? written : capacity;
			const auto first = written - count;
			result.entries.reserve(static_cast<std::size_t>(count));
			for (auto i = first; i < written; ++i) result.entries.push_back(entries[i % capacity]);
			return result;
		}

	private:
		std::atomic<std::uint64_t> token{0};
		std::uint64_t generation = 0;
		std::uint64_t written = 0;
		std::int64_t started = 0;
		std::mutex mutex;
		std::array<Entry, capacity> entries{};
	};

	inline Recorder recorder;

	class Scope
	{
	public:
		explicit Scope(std::string_view category, std::int64_t thresholdMs = 4,
			std::source_location source = std::source_location::current())
			: token(recorder.Token()), threshold(thresholdMs * 1000000)
		{
			if (token)
			{
				entry.started = Now();
				entry.category = category;
				entry.source = source;
			}
		}

		~Scope()
		{
			if (!token) return;
			entry.duration = Now() - entry.started;
			if (entry.duration < threshold) return;
			static thread_local const auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
			entry.thread = threadId;
			recorder.Add(token, entry, threshold);
		}

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		std::uint64_t token;
		std::int64_t threshold;
		Entry entry;
	};
}
