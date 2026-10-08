#include "STDInclude.hpp"

namespace Utils
{
	NamedMutex::NamedMutex(const std::string& name)
	{
		this->handle = CreateMutexA(nullptr, FALSE, name.data());
	}

	NamedMutex::~NamedMutex()
	{
		if (this->handle)
		{
			CloseHandle(this->handle);
		}
	}

	void NamedMutex::lock() const
	{
		if (this->handle)
		{
			WaitForSingleObject(this->handle, INFINITE);
		}
	}

	bool NamedMutex::try_lock(const std::chrono::milliseconds timeout) const
	{
		if (this->handle)
		{
			return WAIT_OBJECT_0 == WaitForSingleObject(this->handle, static_cast<DWORD>(timeout.count()));
		}

		return false;
	}

	void NamedMutex::unlock() const noexcept
	{
		if (this->handle)
		{
			ReleaseMutex(this->handle);
		}
	}
}
