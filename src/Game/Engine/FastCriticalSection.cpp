#include "STDInclude.hpp"

#include "FastCriticalSection.hpp"

namespace Game::Engine
{
	FastCriticalSectionScopeRead::FastCriticalSectionScopeRead(FastCriticalSection* critSect)
		: critSect(critSect)
	{
		if (this->critSect)
		{
			Sys_LockRead(this->critSect);
		}
	}

	FastCriticalSectionScopeRead::~FastCriticalSectionScopeRead()
	{
		if (this->critSect)
		{
			Sys_UnlockRead(this->critSect);
		}
	}

	FastCriticalSectionScopeWrite::FastCriticalSectionScopeWrite(FastCriticalSection* critSect)
		: critSect(critSect)
	{
		if (this->critSect)
		{
			Sys_LockWrite(this->critSect);
		}
	}

	FastCriticalSectionScopeWrite::~FastCriticalSectionScopeWrite()
	{
		if (this->critSect)
		{
			Sys_UnlockWrite(this->critSect);
		}
	}
}
