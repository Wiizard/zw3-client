#include "STDInclude.hpp"

#include "Controller/Transport/ReportStream.hpp"

namespace Controller::Transport
{
	ReportStream::ReportStream(const Context& context, DeviceId device, HidDevice& hid, Cadence cadence, Producer produce)
		: hid(hid),
		produce(std::move(produce)),
		cadence(cadence),
		thread([this, context, device](std::stop_token stop)
		{
			this->Run(stop, context, device);
		})
	{
	}

	std::string ReportStream::Status() const
	{
		std::lock_guard lock(this->statusMutex);
		return this->status;
	}

	void ReportStream::Note(std::string text) const
	{
		std::lock_guard lock(this->statusMutex);
		this->status = std::move(text);
	}

	void ReportStream::Run(const std::stop_token& stop, const Context& context, DeviceId device)
	{
		if (this->cadence.period <= std::chrono::nanoseconds::zero() || this->cadence.capacity == 0)
		{
			this->Note("a report stream was asked for with no cadence to keep");
			this->hasFailed.store(true, std::memory_order_release);
			return;
		}

		std::vector<std::byte> report(this->cadence.capacity);

		bool hasStarted = false;

		auto due = std::chrono::steady_clock::now();

		while (!stop.stop_requested())
		{
			const auto size = this->produce(report);

			if (!size)
			{
				if (hasStarted)
				{
					this->Note("the report stream ended");
				}
				else
				{
					this->Note("no report could be produced for this device");
				}

				break;
			}

			if (!this->hid.TryWrite(std::span<const std::byte>(report.data(), *size)))
			{
				if (hasStarted)
				{
					this->Note("the device stopped accepting the report stream");
				}
				else
				{
					this->Note("the device refused the report");
					context.Report(Severity::Warning, Facility::Transport, ErrorCode::OutputRejected, device,
						"the controller did not accept the output report this stream is built on, so it cannot be driven this way");
				}

				break;
			}

			if (!hasStarted)
			{
				hasStarted = true;
				this->isRunning.store(true, std::memory_order_release);

				const auto periodUs = std::chrono::duration_cast<std::chrono::microseconds>(this->cadence.period).count();
				this->Note(std::format("streaming reports every {} us", periodUs));
			}

			due += this->cadence.period;

			const auto now = std::chrono::steady_clock::now();

			if (due > now)
			{
				std::this_thread::sleep_until(due);
			}
			else
			{
				due = now;
			}
		}

		this->isRunning.store(false, std::memory_order_release);
		this->hasFailed.store(true, std::memory_order_release);
	}
}
