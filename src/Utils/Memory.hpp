#pragma once

namespace Utils
{
	class Memory
	{
	public:
		class Allocator
		{
		public:
			typedef void(*FreeCallback)(void*);

			Allocator() = default;

			~Allocator()
			{
				this->Clear();
			}

			void Clear()
			{
				std::lock_guard _(this->mutex);

				for (const auto& [memory, callback] : this->referenced)
				{
					if (memory && callback)
					{
						callback(memory);
					}
				}

				this->referenced.clear();

				for (auto* const data : this->pool)
				{
					Memory::Free(data);
				}

				this->pool.clear();
			}

			void Free(void* data)
			{
				std::lock_guard _(this->mutex);

				const auto reference = this->referenced.find(data);

				if (reference != this->referenced.end())
				{
					reference->second(reference->first);
					this->referenced.erase(reference);
				}

				const auto pooled = std::find(this->pool.begin(), this->pool.end(), data);

				if (pooled != this->pool.end())
				{
					Memory::Free(data);
					this->pool.erase(pooled);
				}
			}

			void Free(const void* data)
			{
				this->Free(const_cast<void*>(data));
			}

			void Reference(void* memory, FreeCallback callback)
			{
				std::lock_guard _(this->mutex);

				this->referenced[memory] = callback;
			}

			void* Allocate(std::size_t length)
			{
				std::lock_guard _(this->mutex);

				void* const data = Memory::Allocate(length);
				this->pool.push_back(data);

				return data;
			}

			template <typename T>
			T* Allocate()
			{
				return this->AllocateArray<T>(1);
			}

			template <typename T>
			T* AllocateArray(std::size_t count = 1)
			{
				return static_cast<T*>(this->Allocate(count * sizeof(T)));
			}

			bool IsEmpty() const
			{
				return this->pool.empty() && this->referenced.empty();
			}

			char* DuplicateString(const std::string& string)
			{
				std::lock_guard _(this->mutex);

				char* const data = Memory::DuplicateString(string);
				this->pool.push_back(data);

				return data;
			}

			bool IsPointerMapped(void* pointer) const
			{
				return this->pointerMap.contains(pointer);
			}

			template <typename T>
			T* GetPointer(void* oldPointer)
			{
				const auto mapped = this->pointerMap.find(oldPointer);

				if (mapped == this->pointerMap.end())
				{
					return nullptr;
				}

				return static_cast<T*>(mapped->second);
			}

			void MapPointer(void* oldPointer, void* newPointer)
			{
				this->pointerMap[oldPointer] = newPointer;
			}

		private:
			std::mutex mutex;
			std::vector<void*> pool;
			std::unordered_map<void*, void*> pointerMap;
			std::unordered_map<void*, FreeCallback> referenced;
		};

		static void* AllocateAlign(std::size_t length, std::size_t alignment);
		static void* Allocate(std::size_t length);

		template <typename T>
		static T* Allocate()
		{
			return AllocateArray<T>(1);
		}

		template <typename T>
		static T* AllocateArray(std::size_t count = 1)
		{
			return static_cast<T*>(Allocate(count * sizeof(T)));
		}

		template <typename T>
		static T* Duplicate(T* original)
		{
			T* const data = Allocate<T>();
			std::memcpy(data, original, sizeof(T));

			return data;
		}

		static char* DuplicateString(const std::string& string);

		static void Free(void* data);
		static void Free(const void* data);

		static void FreeAlign(void* data);
		static void FreeAlign(const void* data);

		static bool IsSet(void* memory, char value, std::size_t length);

		static bool IsBadReadPtr(const void* pointer);
		static bool IsBadCodePtr(const void* pointer);

		static Allocator* GetAllocator();

	private:
		static Allocator allocator;
	};
}
