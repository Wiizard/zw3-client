#pragma once

namespace Game::Engine
{
	enum ScopedCriticalSectionType
	{
		SCOPED_CRITSECT_NORMAL = 0x0,
		SCOPED_CRITSECT_DISABLED = 0x1,
		SCOPED_CRITSECT_RELEASE = 0x2,
		SCOPED_CRITSECT_TRY = 0x3,
	};

	class ScopedCriticalSection
	{
	public:
		ScopedCriticalSection(CriticalSection section, ScopedCriticalSectionType type);
		~ScopedCriticalSection();

		ScopedCriticalSection(ScopedCriticalSection&&) = delete;
		ScopedCriticalSection(const ScopedCriticalSection&) = delete;
		ScopedCriticalSection& operator=(ScopedCriticalSection&&) = delete;
		ScopedCriticalSection& operator=(const ScopedCriticalSection&) = delete;

		void EnterCritSect();
		void LeaveCritSect();
		[[nodiscard]] bool TryEnterCritSect();

		[[nodiscard]] bool HasOwnership() const;
		[[nodiscard]] bool IsScopedRelease() const;

	private:
		CriticalSection section;
		bool hasOwnership;
		bool isScopedRelease;
	};
}
