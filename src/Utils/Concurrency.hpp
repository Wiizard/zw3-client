#pragma once

namespace Utils::Concurrency
{
	template <typename T, typename MutexType = std::mutex>
	class Container
	{
	public:
		template <typename R = void, typename F>
		R Access(F&& accessor) const
		{
			std::lock_guard<MutexType> _{ this->mutex };
			return accessor(this->object);
		}

		template <typename R = void, typename F>
		R Access(F&& accessor)
		{
			std::lock_guard<MutexType> _{ this->mutex };
			return accessor(this->object);
		}

		template <typename R = void, typename F>
		R AccessWithLock(F&& accessor) const
		{
			std::unique_lock<MutexType> lock{ this->mutex };
			return accessor(this->object, lock);
		}

		template <typename R = void, typename F>
		R AccessWithLock(F&& accessor)
		{
			std::unique_lock<MutexType> lock{ this->mutex };
			return accessor(this->object, lock);
		}

		T& GetRaw() { return this->object; }
		const T& GetRaw() const { return this->object; }

	private:
		mutable MutexType mutex{};
		T object{};
	};
}
