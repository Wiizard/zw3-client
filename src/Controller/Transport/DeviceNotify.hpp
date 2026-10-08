#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"

namespace Controller::Transport
{
	class DeviceNotifier
	{
	public:
		explicit DeviceNotifier(const Context& context);

		DeviceNotifier(const DeviceNotifier&) = delete;
		DeviceNotifier& operator=(const DeviceNotifier&) = delete;

		bool Consume() noexcept
		{
			return this->isPending.exchange(false, std::memory_order_acquire);
		}

		bool HasFailed() const noexcept
		{
			return this->hasFailed.load(std::memory_order_acquire);
		}

	private:
		void Run(const std::stop_token& stop, const Context& context);

		std::atomic<bool> isPending{ false };
		std::atomic<bool> hasFailed{ false };

		std::jthread thread;
	};
}
