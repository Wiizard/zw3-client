#pragma once

namespace Game::Engine
{
	class FastCriticalSectionScopeRead
	{
	public:
		explicit FastCriticalSectionScopeRead(FastCriticalSection* critSect);
		~FastCriticalSectionScopeRead();

		FastCriticalSectionScopeRead(FastCriticalSectionScopeRead&&) = delete;
		FastCriticalSectionScopeRead(const FastCriticalSectionScopeRead&) = delete;
		FastCriticalSectionScopeRead& operator=(FastCriticalSectionScopeRead&&) = delete;
		FastCriticalSectionScopeRead& operator=(const FastCriticalSectionScopeRead&) = delete;

	private:
		FastCriticalSection* critSect;
	};

	class FastCriticalSectionScopeWrite
	{
	public:
		explicit FastCriticalSectionScopeWrite(FastCriticalSection* critSect);
		~FastCriticalSectionScopeWrite();

		FastCriticalSectionScopeWrite(FastCriticalSectionScopeWrite&&) = delete;
		FastCriticalSectionScopeWrite(const FastCriticalSectionScopeWrite&) = delete;
		FastCriticalSectionScopeWrite& operator=(FastCriticalSectionScopeWrite&&) = delete;
		FastCriticalSectionScopeWrite& operator=(const FastCriticalSectionScopeWrite&) = delete;

	private:
		FastCriticalSection* critSect;
	};
}
