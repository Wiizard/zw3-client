#include "STDInclude.hpp"

#ifdef WRITE_LOGS
#include "Components/Modules/Logger.hpp"
#endif

namespace Utils
{
	std::string Stream::Reader::ReadString()
	{
		std::string str;

		while (const char byte = this->ReadByte())
		{
			str.push_back(byte);
		}

		return str;
	}

	const char* Stream::Reader::ReadCString()
	{
		return this->allocator->DuplicateString(this->ReadString());
	}

	char Stream::Reader::ReadByte()
	{
		if ((this->position + 1) <= this->buffer.size())
		{
			return this->buffer[this->position++];
		}

		throw std::runtime_error("Reading past the buffer");
	}

	void* Stream::Reader::Read(std::size_t size, std::size_t count)
	{
		const auto bytes = size * count;

		if ((this->position + bytes) <= this->buffer.size())
		{
			auto* const data = this->allocator->Allocate(bytes);
			std::memcpy(data, this->buffer.data() + this->position, bytes);
			this->position += static_cast<unsigned int>(bytes);

			return data;
		}

		throw std::runtime_error("Reading past the buffer");
	}

	bool Stream::Reader::End() const
	{
		return this->buffer.size() == this->position;
	}

	void Stream::Reader::Seek(unsigned int newPosition)
	{
		if (this->buffer.size() >= newPosition)
		{
			this->position = newPosition;
		}
	}

	void Stream::Reader::SeekRelative(unsigned int distance)
	{
		return this->Seek(distance + this->position);
	}

	void* Stream::Reader::ReadPointer()
	{
		const auto stored = this->Read<std::uint32_t>();
		auto* const pointer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(stored));

		if (!this->HasPointer(pointer))
		{
			this->pointerMap[pointer] = nullptr;
		}

		return pointer;
	}

	void Stream::Reader::MapPointer(void* oldPointer, void* newPointer)
	{
		if (this->HasPointer(oldPointer))
		{
			this->pointerMap[oldPointer] = newPointer;
		}
	}

	bool Stream::Reader::HasPointer(void* pointer) const
	{
		return this->pointerMap.contains(pointer);
	}

	Stream::Stream() : ptrAssertion(false), criticalSectionState(0)
	{
		std::memset(this->blockSize, 0, sizeof(this->blockSize));

#ifdef WRITE_LOGS
		this->structLevel = 0;
		IO::WriteFile("userraw/logs/zb_writes.log", "", false);
#endif
	}

	Stream::Stream(std::size_t size) : Stream()
	{
		this->buffer.reserve(size);
	}

	Stream::~Stream()
	{
		this->buffer.clear();

		if (this->criticalSectionState != 0)
		{
			MessageBoxA(nullptr, String::VA("Invalid critical section state '%i' for stream destruction!", this->criticalSectionState), "WARNING", MB_ICONEXCLAMATION);
		}
	}

	std::size_t Stream::Length() const
	{
		return this->buffer.length();
	}

	std::size_t Stream::Capacity() const
	{
		return this->buffer.capacity();
	}

	void Stream::AssertPointer(const void* pointer, std::size_t length)
	{
		if (!this->ptrAssertion)
		{
			return;
		}

		for (const auto& [entryPointer, entryLength] : this->ptrList)
		{
			const auto entryBase = reinterpret_cast<std::uintptr_t>(entryPointer);
			const auto base = reinterpret_cast<std::uintptr_t>(pointer);

			if (HasIntersection(entryBase, entryLength, base, length))
			{
				MessageBoxA(nullptr, "Duplicate data written!", "ERROR", MB_ICONERROR);
#ifdef _DEBUG
				__debugbreak();
#endif
			}
		}

		this->ptrList.emplace_back(pointer, length);
	}

	char* Stream::Save(const void* str, std::size_t size, std::size_t count)
	{
		return this->Save(this->GetCurrentBlock(), str, size, count);
	}

	char* Stream::Save(Game::XFILE_BLOCK_TYPES stream, const void* str, std::size_t size, std::size_t count)
	{
		const auto bytes = size * count;

		if (stream == Game::XFILE_BLOCK_RUNTIME)
		{
			this->IncreaseBlockSize(stream, static_cast<std::uint32_t>(bytes));
			return this->At();
		}

		const auto* const data = this->Data();

		if (this->IsCriticalSection() && this->Length() + bytes > this->Capacity())
		{
			MessageBoxA(nullptr, String::VA("Potential stream reallocation during critical operation detected! Writing data of the length 0x%zX exceeds the allocated stream size of 0x%zX\n", bytes, this->Capacity()), "ERROR", MB_ICONERROR);
			__debugbreak();
		}

		this->buffer.append(static_cast<const char*>(str), bytes);

		if (this->Data() != data && this->IsCriticalSection())
		{
			MessageBoxA(nullptr, "Stream reallocation during critical operations not permitted!\nPlease increase the initial memory size or reallocate memory during non-critical sections!", "ERROR", MB_ICONERROR);
			__debugbreak();
		}

		this->IncreaseBlockSize(stream, static_cast<std::uint32_t>(bytes));
		this->AssertPointer(str, bytes);

		return this->At() - bytes;
	}

	char* Stream::Save(Game::XFILE_BLOCK_TYPES stream, int value, std::size_t count)
	{
		const auto start = this->Length();

		for (std::size_t i = 0; i < count; ++i)
		{
			this->Save(stream, &value, 4, 1);
		}

		return this->Data() + start;
	}

	char* Stream::SaveString(const std::string& string)
	{
		return this->SaveString(string.data());
	}

	char* Stream::SaveString(const char* string)
	{
		return this->SaveString(string, std::strlen(string));
	}

	char* Stream::SaveString(const char* string, std::size_t len)
	{
		const auto start = this->Length();

		if (string)
		{
			this->Save(string, len);
		}

		this->SaveNull();

		return this->Data() + start;
	}

	char* Stream::SaveText(const std::string& string)
	{
		return this->Save(string.data(), string.length());
	}

	char* Stream::SaveByte(unsigned char byte, std::size_t count)
	{
		const auto start = this->Length();

		for (std::size_t i = 0; i < count; ++i)
		{
			this->Save(&byte, 1);
		}

		return this->Data() + start;
	}

	char* Stream::SaveNull(std::size_t count)
	{
		return this->SaveByte(0, count);
	}

	char* Stream::SaveMax(std::size_t count)
	{
		return this->SaveByte(static_cast<unsigned char>(-1), count);
	}

	void Stream::Align(Stream::Alignment align)
	{
		std::uint32_t size = 2u << align;

		if (!size || (size & (size - 1)))
		{
			return;
		}

		--size;

		const Game::XFILE_BLOCK_TYPES stream = this->GetCurrentBlock();

		if (!this->IsValidBlock(stream))
		{
			return;
		}

		this->blockSize[stream] = ~size & (this->GetBlockSize(stream) + size);
	}

	bool Stream::PushBlock(Game::XFILE_BLOCK_TYPES stream)
	{
		this->streamStack.push_back(stream);
		return this->IsValidBlock(stream);
	}

	bool Stream::PopBlock()
	{
		if (this->streamStack.empty())
		{
			return false;
		}

		this->streamStack.pop_back();
		return true;
	}

	bool Stream::HasBlock()
	{
		return !this->streamStack.empty();
	}

	bool Stream::IsValidBlock(Game::XFILE_BLOCK_TYPES stream)
	{
		return stream < Game::MAX_XFILE_COUNT && stream >= Game::XFILE_BLOCK_TEMP;
	}

	void Stream::IncreaseBlockSize(Game::XFILE_BLOCK_TYPES stream, std::uint32_t size)
	{
		if (this->IsValidBlock(stream))
		{
			this->blockSize[stream] += size;
		}

#ifdef WRITE_LOGS
		const auto* line = String::VA("%*s%u\n", this->structLevel, "", size);

		if (stream == Game::XFILE_BLOCK_RUNTIME)
		{
			line = String::VA("%*s(%u)\n", this->structLevel, "", size);
		}

		IO::WriteFile("userraw/logs/zb_writes.log", line, true);
#endif
	}

	void Stream::IncreaseBlockSize(std::uint32_t size)
	{
		return this->IncreaseBlockSize(this->GetCurrentBlock(), size);
	}

	Game::XFILE_BLOCK_TYPES Stream::GetCurrentBlock()
	{
		if (!this->streamStack.empty())
		{
			return this->streamStack.back();
		}

		return Game::XFILE_BLOCK_INVALID;
	}

	char* Stream::At()
	{
		return this->Data() + this->Length();
	}

	char* Stream::Data()
	{
		return this->buffer.data();
	}

	std::uint32_t Stream::GetBlockSize(Game::XFILE_BLOCK_TYPES stream)
	{
		if (this->IsValidBlock(stream))
		{
			return this->blockSize[stream];
		}

		return 0;
	}

	std::uint32_t Stream::GetPackedOffset()
	{
		const Game::XFILE_BLOCK_TYPES block = this->GetCurrentBlock();

		Offset offset;
		offset.block = static_cast<std::uint32_t>(block);
		offset.offset = this->GetBlockSize(block);
		return offset.GetPackedOffset();
	}

	void Stream::ToBuffer(std::string& outBuffer)
	{
		outBuffer.clear();
		outBuffer.append(this->Data(), this->Length());
	}

	std::string Stream::ToBuffer()
	{
		std::string outBuffer;
		this->ToBuffer(outBuffer);
		return outBuffer;
	}

	void Stream::EnterCriticalSection()
	{
		++this->criticalSectionState;
	}

	void Stream::LeaveCriticalSection()
	{
		--this->criticalSectionState;
	}

	bool Stream::IsCriticalSection() const
	{
		if (this->criticalSectionState < 0)
		{
			MessageBoxA(nullptr, "CriticalSectionState in stream has been overrun!", "ERROR", MB_ICONERROR);
			__debugbreak();
		}

		return this->criticalSectionState != 0;
	}

#ifdef WRITE_LOGS
	void Stream::EnterStruct(const char* structName)
	{
		if (this->structLevel >= 0)
		{
			IO::WriteFile("userraw/logs/zb_writes.log", String::VA("%*s%s\n", this->structLevel++, "", structName), true);
		}
	}

	void Stream::LeaveStruct()
	{
		if (--this->structLevel < 0)
		{
			Components::Logger::Print("Stream::leaveStruct underflow! All following writes will not be logged!\n");
			return;
		}

		IO::WriteFile("userraw/logs/zb_writes.log", String::VA("%*s-----\n", this->structLevel, ""), true);
	}
#endif
}
