#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamRemoteStorage.hpp"

namespace Steam
{
	void* const RemoteStorage::vtable[] =
	{
		reinterpret_cast<void*>(FileWrite),
		reinterpret_cast<void*>(FileRead),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(FileExists),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(GetFileSize),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(GetFileCount),
		reinterpret_cast<void*>(GetFileNameAndSize),
		reinterpret_cast<void*>(GetQuota),
	};

	Interface RemoteStorage::object = { RemoteStorage::vtable };

	Interface* RemoteStorage::Get()
	{
		static_assert(std::size(vtable) == 18);
		return &object;
	}

	bool RemoteStorage::FileWrite([[maybe_unused]] Interface* self, [[maybe_unused]] const char* file, [[maybe_unused]] const void* data, [[maybe_unused]] int size)
	{
		return true;
	}

	int RemoteStorage::FileRead([[maybe_unused]] Interface* self, [[maybe_unused]] const char* file, [[maybe_unused]] void* data, [[maybe_unused]] int size)
	{
		return 0;
	}

	bool RemoteStorage::FileExists([[maybe_unused]] Interface* self, [[maybe_unused]] const char* file)
	{
		return false;
	}

	int RemoteStorage::GetFileSize([[maybe_unused]] Interface* self, [[maybe_unused]] const char* file)
	{
		return 0;
	}

	int RemoteStorage::GetFileCount([[maybe_unused]] Interface* self)
	{
		return 0;
	}

	const char* RemoteStorage::GetFileNameAndSize([[maybe_unused]] Interface* self, [[maybe_unused]] int file, int* size)
	{
		*size = 0;
		return "";
	}

	bool RemoteStorage::GetQuota([[maybe_unused]] Interface* self, int* totalBytes, int* availableBytes)
	{
		*totalBytes = 0x10000000;
		*availableBytes = 0x10000000;
		return false;
	}
}
