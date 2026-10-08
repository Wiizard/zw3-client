#pragma once

#ifndef WRITE_LOGS
#define SaveLogEnter(x)
#define SaveLogExit()
#else
#define SaveLogEnter(x) builder->GetBuffer()->EnterStruct(x)
#define SaveLogExit() builder->GetBuffer()->LeaveStruct()
#endif

namespace Utils
{
	constexpr auto POINTER = 255;
	constexpr auto FOLLOWING = 254;

	class Stream
	{
	private:
		bool ptrAssertion;
		std::vector<std::pair<const void*, std::size_t>> ptrList;

		int criticalSectionState;
		std::uint32_t blockSize[Game::MAX_XFILE_COUNT];
		std::vector<Game::XFILE_BLOCK_TYPES> streamStack;
		std::string buffer;

	public:
		class Reader
		{
		public:
			Reader(Memory::Allocator* readAllocator, std::string data) : position(0), buffer(std::move(data)), allocator(readAllocator) {}

			std::string ReadString();
			const char* ReadCString();

			char ReadByte();

			void* Read(std::size_t size, std::size_t count = 1);

			template <typename T> T* ReadObject()
			{
				return ReadArray<T>(1);
			}

			template <typename T> T* ReadArrayOnce(std::size_t count = 1)
			{
				const auto marker = static_cast<unsigned char>(this->ReadByte());

				if (marker == POINTER)
				{
					const auto filePosition = this->Read<std::uint32_t>();
					auto* const key = reinterpret_cast<void*>(static_cast<std::uintptr_t>(filePosition));

					if (this->allocator->IsPointerMapped(key))
					{
						return this->allocator->GetPointer<T>(key);
					}

					throw std::runtime_error("Bad data: missing ptr");
				}

				if (marker == FOLLOWING)
				{
					const auto filePosition = this->position;
					auto* const data = this->ReadArray<T>(count);
					this->allocator->MapPointer(reinterpret_cast<void*>(static_cast<std::uintptr_t>(filePosition)), data);

					return data;
				}

				throw std::runtime_error("Bad data");
			}

			template <typename T> T* ReadArray(std::size_t count = 1)
			{
				return static_cast<T*>(this->Read(sizeof(T), count));
			}

			template <typename T> T Read()
			{
				T object;

				for (std::size_t i = 0; i < sizeof(T); ++i)
				{
					reinterpret_cast<char*>(&object)[i] = this->ReadByte();
				}

				return object;
			}

			bool End() const;
			void Seek(unsigned int newPosition);
			void SeekRelative(unsigned int distance);

			void* ReadPointer();
			void MapPointer(void* oldPointer, void* newPointer);
			bool HasPointer(void* pointer) const;

		private:
			unsigned int position;
			std::string buffer;
			std::map<void*, void*> pointerMap;
			Memory::Allocator* allocator;
		};

		enum Alignment
		{
			ALIGN_2,
			ALIGN_4,
			ALIGN_8,
			ALIGN_16,
			ALIGN_32,
			ALIGN_64,
			ALIGN_128,
			ALIGN_256,
			ALIGN_512,
			ALIGN_1024,
			ALIGN_2048,
		};

		Stream();
		Stream(std::size_t size);
		~Stream();

		std::unordered_map<const void*, std::uint32_t> dataPointers;

		[[nodiscard]] std::size_t Length() const;
		[[nodiscard]] std::size_t Capacity() const;

		char* Save(const void* str, std::size_t size, std::size_t count = 1);
		char* Save(Game::XFILE_BLOCK_TYPES stream, const void* str, std::size_t size, std::size_t count);
		char* Save(Game::XFILE_BLOCK_TYPES stream, int value, std::size_t count);

		template <typename T> char* Save(T* object)
		{
			return SaveArray<T>(object, 1);
		}

		template <typename T> char* SaveObject(T value)
		{
			return SaveArray(&value, 1);
		}

		template <typename T> void SaveArrayIfNotExisting(T* data, std::size_t count)
		{
			const auto itr = this->dataPointers.find(data);

			if (itr != this->dataPointers.end())
			{
				this->SaveByte(POINTER);
				this->SaveObject(itr->second);
				return;
			}

			this->SaveByte(FOLLOWING);
			this->dataPointers.insert_or_assign(data, static_cast<std::uint32_t>(this->Length()));
			this->SaveArray(data, count);
		}

		char* Save(int value, std::size_t count = 1)
		{
			const auto start = this->Length();

			for (std::size_t i = 0; i < count; ++i)
			{
				this->Save(&value, 4, 1);
			}

			return this->Data() + start;
		}

		template <typename T> char* SaveArray(T* array, std::size_t count)
		{
			return Save(array, sizeof(T), count);
		}

		char* SaveString(const std::string& string);
		char* SaveString(const char* string);
		char* SaveString(const char* string, std::size_t len);
		char* SaveByte(unsigned char byte, std::size_t count = 1);
		char* SaveNull(std::size_t count = 1);
		char* SaveMax(std::size_t count = 1);

		char* SaveText(const std::string& string);

		void Align(Alignment align);
		bool PushBlock(Game::XFILE_BLOCK_TYPES stream);
		bool PopBlock();
		bool IsValidBlock(Game::XFILE_BLOCK_TYPES stream);
		void IncreaseBlockSize(Game::XFILE_BLOCK_TYPES stream, std::uint32_t size);
		void IncreaseBlockSize(std::uint32_t size);
		Game::XFILE_BLOCK_TYPES GetCurrentBlock();
		std::uint32_t GetBlockSize(Game::XFILE_BLOCK_TYPES stream);
		bool HasBlock();

		std::uint32_t GetPackedOffset();

		char* Data();
		char* At();

		template <typename T> T* Dest()
		{
			return reinterpret_cast<T*>(this->At());
		}

		static void ClearPointer(std::uint32_t* object)
		{
			*object = 0xFFFFFFFF;
		}

		void SetPointerAssertion(bool value)
		{
			this->ptrAssertion = value;
		}

		void AssertPointer(const void* pointer, std::size_t length);

		void ToBuffer(std::string& outBuffer);
		std::string ToBuffer();

		void EnterCriticalSection();
		void LeaveCriticalSection();
		bool IsCriticalSection() const;

#ifdef WRITE_LOGS
		int structLevel;
		void EnterStruct(const char* structName);
		void LeaveStruct();
#endif

		class Offset
		{
		public:
			std::uint32_t offset : 28;
			std::uint32_t block : 4;

			Offset() : offset(0), block(0) {}
			Offset(Game::XFILE_BLOCK_TYPES blockType, std::uint32_t blockOffset) : offset(blockOffset), block(static_cast<std::uint32_t>(blockType)) {}

			std::uint32_t GetPackedOffset() const
			{
				return ((this->block << 28) | this->offset) + 1;
			}

			std::uint32_t GetUnpackedOffset() const
			{
				return (this->GetPackedOffset() - 2) & 0x0FFFFFFF;
			}

			int GetUnpackedBlock() const
			{
				return static_cast<int>((this->GetPackedOffset() - 2) >> 28);
			}
		};
	};

	AssertSize(Stream::Offset, 4);
}
