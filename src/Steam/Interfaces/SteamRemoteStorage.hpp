#pragma once

namespace Steam
{
	class RemoteStorage
	{
	public:
		static Interface* Get();

	private:
		static void* const vtable[];
		static Interface object;

		static bool FileWrite(Interface* self, const char* file, const void* data, int size);
		static int FileRead(Interface* self, const char* file, void* data, int size);
		static bool FileExists(Interface* self, const char* file);
		static int GetFileSize(Interface* self, const char* file);
		static int GetFileCount(Interface* self);
		static const char* GetFileNameAndSize(Interface* self, int file, int* size);
		static bool GetQuota(Interface* self, int* totalBytes, int* availableBytes);
	};
}
