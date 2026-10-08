#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Haptic/Effect.hpp"
#include "Controller/Haptic/Mixer.hpp"
#include "Controller/Transport/Audio.hpp"
#include "Controller/Transport/Hid.hpp"
#include "Controller/Transport/ReportStream.hpp"

namespace Controller::Haptic
{
	class Stream
	{
	public:
		Stream(const Context& context, DeviceId device, Connection link, Transport::HidDevice& hid);

		Stream(const Stream&) = delete;
		Stream& operator=(const Stream&) = delete;

		void PollDiagnostics();

		bool IsRunning();

		bool TryPlay(const Effect& effect) noexcept
		{
			return this->mixer.TryPlay(effect);
		}

		void Stop(std::uint32_t tag) noexcept
		{
			this->mixer.Stop(tag);
		}

		void SetRumble(float lowFrequency, float highFrequency) noexcept
		{
			this->mixer.SetRumble(lowFrequency, highFrequency);
		}

		std::string Status() const;

	private:
		void StartReports();
		void StartAudio();

		std::optional<std::size_t> Produce(std::span<std::byte> out) noexcept;

		const Context& context;
		DeviceId device;
		Connection link;
		Transport::HidDevice& hid;

		std::uint64_t reportedDrops = 0;

		Mixer mixer;

		std::vector<Frame> block;
		std::uint8_t counter = 0;

		std::unique_ptr<Transport::AudioEndpoint> audio;
		std::unique_ptr<Transport::ReportStream> reports;
	};
}
