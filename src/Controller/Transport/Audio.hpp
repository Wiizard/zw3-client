#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Haptic/Effect.hpp"

namespace Controller::Transport
{
	class AudioEndpoint
	{
	public:
		using Source = std::function<void(std::span<Haptic::Frame> frames, std::uint32_t rate)>;

		AudioEndpoint(const Context& context, DeviceId device, const std::wstring& hidPath, Source fill);

		AudioEndpoint(const AudioEndpoint&) = delete;
		AudioEndpoint& operator=(const AudioEndpoint&) = delete;

		bool IsRunning() const noexcept
		{
			return this->isRunning.load(std::memory_order_acquire);
		}

		std::string Status() const;

	private:
		void Run(const std::stop_token& stop, const Context& context, DeviceId device, const std::wstring& hidPath);
		void Note(std::string text) const;

		Source fill;

		std::atomic<bool> isRunning{ false };

		mutable std::mutex statusMutex;
		mutable std::string status = "not started";

		std::jthread thread;
	};
}
