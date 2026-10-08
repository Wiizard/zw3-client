#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Connection.hpp"

namespace Controller
{
	class Registry
	{
	public:
		explicit Registry(const Context& context);

		DeviceId Add(DeviceConnection connection);
		bool Remove(DeviceId id);

		void ForEach(const std::function<void(const DeviceConnection&)>& visit) const;

		std::uint64_t Generation() const noexcept
		{
			return this->generation.load();
		}

	private:
		const Context& context;

		mutable std::mutex mutex;
		std::uint32_t nextId = 1;
		std::atomic<std::uint64_t> generation{ 0 };
		std::vector<DeviceConnection> devices;
	};
}
