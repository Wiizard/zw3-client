#pragma once

#include "Controller/Types.hpp"

#include "Controller/Haptic/Effect.hpp"

namespace Controller::Haptic
{
	class Mixer
	{
	public:
		static constexpr std::size_t voiceCount = 12;

		Mixer() = default;

		Mixer(const Mixer&) = delete;
		Mixer& operator=(const Mixer&) = delete;

		bool TryPlay(const Effect& effect) noexcept;
		void Stop(std::uint32_t tag) noexcept;
		void SetRumble(float lowFrequency, float highFrequency) noexcept;

		void Render(std::span<Frame> out, std::uint32_t rate) noexcept;

		std::uint64_t Dropped() const noexcept
		{
			return this->dropped.load(std::memory_order_relaxed);
		}

	private:
		enum class VoiceState : std::uint8_t
		{
			Free,
			Filling,
			Ready,
			Playing,
		};

		struct Voice
		{
			std::atomic<VoiceState> phase{ VoiceState::Free };
			std::atomic<bool> isStopping{ false };

			Effect effect{};

			float elapsed = 0.0f;
			float deepPhase = 0.0f;
			float crispPhase = 0.0f;
		};

		static bool RenderVoice(Voice& voice, std::span<Frame> out, float step) noexcept;
		void RenderRumble(std::span<Frame> out, float step, float scale) noexcept;

		std::array<Voice, voiceCount> voices{};

		std::atomic<float> rumbleLow{ 0.0f };
		std::atomic<float> rumbleHigh{ 0.0f };

		float rumbleLowLevel = 0.0f;
		float rumbleHighLevel = 0.0f;
		float rumbleLowPhase = 0.0f;
		float rumbleHighPhase = 0.0f;

		std::atomic<std::uint64_t> dropped{ 0 };
	};
}
