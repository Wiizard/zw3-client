#pragma once

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <mutex>

namespace Components::Remix::Device
{
	class RenderFrameLock
	{
		std::recursive_timed_mutex mutex;
		std::atomic<DWORD> owner{0};
		unsigned depth = 0;

		void Acquired()
		{
			++depth;
			owner.store(GetCurrentThreadId());
		}

	public:
		bool OwnsCurrentThread() const { return owner.load() == GetCurrentThreadId(); }

		void lock()
		{
			mutex.lock();
			Acquired();
		}

		bool TryLockFor(std::chrono::milliseconds duration)
		{
			if (!mutex.try_lock_for(duration)) return false;
			Acquired();
			return true;
		}

		void unlock()
		{
			// An inner screen update must not release the outer backend frame.
			if (--depth == 0) owner.store(0);
			mutex.unlock();
		}
	};
}
