#include "STDInclude.hpp"

#include "ScopedCriticalSection.hpp"

namespace Game::Engine
{
	ScopedCriticalSection::ScopedCriticalSection(CriticalSection section, ScopedCriticalSectionType type)
		: section(section), hasOwnership(false), isScopedRelease(false)
	{
		if (type == SCOPED_CRITSECT_NORMAL)
		{
			Sys_EnterCriticalSection(this->section);
			this->hasOwnership = true;
			return;
		}

		if (type == SCOPED_CRITSECT_TRY)
		{
			this->hasOwnership = Sys_TryEnterCriticalSection(this->section);
			return;
		}

		if (type == SCOPED_CRITSECT_RELEASE)
		{
			Sys_LeaveCriticalSection(this->section);
			this->isScopedRelease = true;
		}
	}

	ScopedCriticalSection::~ScopedCriticalSection()
	{
		if (this->hasOwnership && !this->isScopedRelease)
		{
			Sys_LeaveCriticalSection(this->section);
			return;
		}

		if (!this->hasOwnership && this->isScopedRelease)
		{
			Sys_EnterCriticalSection(this->section);
		}
	}

	void ScopedCriticalSection::EnterCritSect()
	{
		assert(!this->hasOwnership);

		this->hasOwnership = true;
		Sys_EnterCriticalSection(this->section);
	}

	void ScopedCriticalSection::LeaveCritSect()
	{
		assert(this->hasOwnership);

		this->hasOwnership = false;
		Sys_LeaveCriticalSection(this->section);
	}

	bool ScopedCriticalSection::TryEnterCritSect()
	{
		assert(!this->hasOwnership);

		const bool isEntered = Sys_TryEnterCriticalSection(this->section);
		this->hasOwnership = isEntered;
		return isEntered;
	}

	bool ScopedCriticalSection::HasOwnership() const
	{
		return this->hasOwnership;
	}

	bool ScopedCriticalSection::IsScopedRelease() const
	{
		return this->isScopedRelease;
	}
}
