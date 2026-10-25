#pragma once

namespace Components::IwdReadLock
{
	class FileLock
	{
	public:
		using Operation = void(*)(void*);

		FileLock(void* file, Operation lock, Operation unlock) : file(file), unlock(unlock)
		{
			if (file) lock(file);
		}

		~FileLock()
		{
			if (file) unlock(file);
		}

		FileLock(const FileLock&) = delete;
		FileLock& operator=(const FileLock&) = delete;

	private:
		void* file;
		Operation unlock;
	};
}
