#pragma once

namespace Utils
{
	template <typename T>
	class Chain
	{
	public:
		class Entry
		{
		private:
			std::shared_ptr<T> object;
			std::shared_ptr<Entry> next;

		public:
			bool HasNext()
			{
				return this->next.use_count() > 0;
			}

			bool IsValid()
			{
				return this->object.use_count() > 0;
			}

			void Set(T value)
			{
				this->object = std::make_shared<T>();
				*this->object.get() = value;
			}

			std::shared_ptr<T> Get()
			{
				return this->object;
			}

			Entry GetNext()
			{
				if (this->HasNext())
				{
					return *this->next.get();
				}

				return Entry();
			}

			std::shared_ptr<Entry> GetNextEntry()
			{
				return this->next;
			}

			void SetNextEntry(std::shared_ptr<Entry> entry)
			{
				this->next = entry;
			}

			T* operator->()
			{
				return this->object.get();
			}

			Entry& operator++()
			{
				*this = this->GetNext();
				return *this;
			}

			Entry operator++(int)
			{
				Entry result = *this;
				this->operator++();
				return result;
			}
		};

	private:
		std::mutex mutex;
		Entry object;

	public:
		void Add(T value)
		{
			std::lock_guard _(this->mutex);

			if (!this->Empty())
			{
				std::shared_ptr<Entry> currentObject = std::make_shared<Entry>();
				*currentObject.get() = this->object;

				this->object = Entry();
				this->object.SetNextEntry(currentObject);
			}

			this->object.Set(value);
		}

		void Remove(std::shared_ptr<T> target)
		{
			std::lock_guard _(this->mutex);

			if (this->Empty())
			{
				return;
			}

			if (this->object.Get().get() == target.get())
			{
				this->object = this->object.GetNext();
				return;
			}

			if (!this->object.HasNext())
			{
				return;
			}

			for (auto entry = this->object; entry.IsValid(); ++entry)
			{
				auto next = entry.GetNext();

				if (next.IsValid() && next.Get().get() == target.get())
				{
					*entry.GetNextEntry().get() = next.GetNext();
				}
			}
		}

		void Remove(Entry entry)
		{
			if (entry.IsValid())
			{
				this->Remove(entry.Get());
			}
		}

		bool Empty()
		{
			return !this->object.IsValid();
		}

		Entry Begin()
		{
			return this->object;
		}

		void Clear()
		{
			this->object = Entry();
		}
	};
}
