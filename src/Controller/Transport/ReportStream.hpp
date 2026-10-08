#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Transport/Hid.hpp"

namespace Controller::Transport
{
	class ReportStream
	{
	public:
		using Producer = std::function<std::optional<std::size_t>(std::span<std::byte>)>;

		struct Cadence
		{
			std::chrono::nanoseconds period;
			std::size_t capacity;
		};

		ReportStream(const Context& context, DeviceId device, HidDevice& hid, Cadence cadence, Producer produce);

		ReportStream(const ReportStream&) = delete;
		ReportStream& operator=(const ReportStream&) = delete;

		bool IsRunning() const noexcept
		{
			return this->isRunning.load(std::memory_order_acquire);
		}

		bool HasFailed() const noexcept
		{
			return this->hasFailed.load(std::memory_order_acquire);
		}

		std::string Status() const;

	private:
		void Run(const std::stop_token& stop, const Context& context, DeviceId device);
		void Note(std::string text) const;

		HidDevice& hid;
		Producer produce;
		Cadence cadence;

		std::atomic<bool> isRunning{ false };
		std::atomic<bool> hasFailed{ false };

		mutable std::mutex statusMutex;
		mutable std::string status = "not started";

		std::jthread thread;
	};
}
