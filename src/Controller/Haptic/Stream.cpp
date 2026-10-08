#include "STDInclude.hpp"

#include "Controller/Haptic/Stream.hpp"
#include "Controller/Driver/OutputReport.hpp"

namespace Controller::Haptic
{
	Stream::Stream(const Context& context, DeviceId device, Connection link, Transport::HidDevice& hid)
		: context(context),
		device(device),
		link(link),
		hid(hid)
	{
		this->StartReports();
	}

	void Stream::StartReports()
	{
		this->block.resize(Driver::hapticFramesPerReport);

		const std::chrono::nanoseconds period{ static_cast<std::chrono::nanoseconds::rep>(1000000000ull * Driver::hapticFramesPerReport / Driver::hapticSampleRate) };

		this->reports = std::make_unique<Transport::ReportStream>(this->context, this->device, this->hid, Transport::ReportStream::Cadence{ period, Driver::dsHapticReportSize },
			[this](std::span<std::byte> out)
			{
				return this->Produce(out);
			});
	}

	void Stream::StartAudio()
	{
		this->block.clear();

		this->audio = std::make_unique<Transport::AudioEndpoint>(this->context, this->device, this->hid.Path(),
			[this](std::span<Frame> frames, std::uint32_t rate)
			{
				this->mixer.Render(frames, rate);
			});
	}

	bool Stream::IsRunning()
	{
		if (this->reports != nullptr)
		{
			if (this->reports->IsRunning())
			{
				return true;
			}

			if (this->link != Connection::Usb || !this->reports->HasFailed())
			{
				return false;
			}

			this->context.Report(Severity::Info, Facility::Transport, ErrorCode::OutputRejected, this->device,
				"the controller would not carry the haptic report stream over USB; falling back to its audio endpoint, which sits behind the Windows audio pipeline and lags further behind the game");

			this->reports.reset();

			try
			{
				this->StartAudio();
			}
			catch (const std::exception& exception)
			{
				this->context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, this->device,
					std::format("the controller's audio endpoint could not be started: {}", exception.what()));
			}
		}

		return this->audio != nullptr && this->audio->IsRunning();
	}

	std::optional<std::size_t> Stream::Produce(std::span<std::byte> out) noexcept
	{
		std::fill(this->block.begin(), this->block.end(), Frame{});
		this->mixer.Render(this->block, Driver::hapticSampleRate);

		return Driver::TryEncodeDualSenseHaptics(this->block, this->counter, out);
	}

	std::string Stream::Status() const
	{
		std::string status = "not started";

		if (this->reports != nullptr)
		{
			status = this->reports->Status();
		}
		else if (this->audio != nullptr)
		{
			status = this->audio->Status();
		}

		const std::uint64_t dropped = this->mixer.Dropped();

		if (dropped != 0)
		{
			status += std::format(", {} effect(s) dropped for want of a voice", dropped);
		}

		return status;
	}

	void Stream::PollDiagnostics()
	{
		const std::uint64_t dropped = this->mixer.Dropped();

		if (dropped == this->reportedDrops)
		{
			return;
		}

		this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::OutputRejected, this->device,
			std::format("controller haptics dropped {} effect(s) for want of a voice; effects are being started faster than {} can carry them", dropped - this->reportedDrops, Mixer::voiceCount));

		this->reportedDrops = dropped;
	}
}
