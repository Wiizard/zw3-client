#include "STDInclude.hpp"

namespace Utils
{
	Memory::Allocator Memory::allocator;

	void* Memory::AllocateAlign(std::size_t length, std::size_t alignment)
	{
		void* const data = _aligned_malloc(length, alignment);

		assert(data);

		if (data)
		{
			std::memset(data, 0, length);
		}

		return data;
	}

	void* Memory::Allocate(std::size_t length)
	{
		void* const data = std::calloc(length, 1);

		assert(data);

		return data;
	}

	char* Memory::DuplicateString(const std::string& string)
	{
		char* const copy = AllocateArray<char>(string.size() + 1);
		std::memcpy(copy, string.data(), string.size());

		return copy;
	}

	void Memory::Free(void* data)
	{
		std::free(data);
	}

	void Memory::Free(const void* data)
	{
		Free(const_cast<void*>(data));
	}

	void Memory::FreeAlign(void* data)
	{
		if (data)
		{
			_aligned_free(data);
		}
	}

	void Memory::FreeAlign(const void* data)
	{
		FreeAlign(const_cast<void*>(data));
	}

	bool Memory::IsSet(void* memory, char value, std::size_t length)
	{
		const auto* const bytes = static_cast<const char*>(memory);

		for (std::size_t i = 0; i < length; ++i)
		{
			if (bytes[i] != value)
			{
				return false;
			}
		}

		return true;
	}

	bool Memory::IsBadReadPtr(const void* pointer)
	{
		MEMORY_BASIC_INFORMATION information{};

		if (!VirtualQuery(pointer, &information, sizeof(information)))
		{
			return true;
		}

		constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
			| PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

		if (information.Protect & (PAGE_GUARD | PAGE_NOACCESS))
		{
			return true;
		}

		return (information.Protect & readable) == 0;
	}

	bool Memory::IsBadCodePtr(const void* pointer)
	{
		MEMORY_BASIC_INFORMATION information{};

		if (!VirtualQuery(pointer, &information, sizeof(information)))
		{
			return true;
		}

		constexpr DWORD executable = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

		if (information.Protect & (PAGE_GUARD | PAGE_NOACCESS))
		{
			return true;
		}

		return (information.Protect & executable) == 0;
	}

	Memory::Allocator* Memory::GetAllocator()
	{
		return &allocator;
	}
}
