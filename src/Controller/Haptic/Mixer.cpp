#include "STDInclude.hpp"

#include "Controller/Haptic/Mixer.hpp"

namespace Controller::Haptic
{
	static constexpr float twoPi = 6.28318530717958647692f;

	static constexpr float rumbleLowHertz = 60.0f;
	static constexpr float rumbleHighHertz = 180.0f;

	static constexpr float rumbleRampSeconds = 0.004f;

	static void Advance(float& phase, float increment) noexcept
	{
		phase += increment;

		while (phase >= 1.0f)
		{
			phase -= 1.0f;
		}
	}

	static float Approach(float current, float target, float coefficient) noexcept
	{
		return current + (target - current) * coefficient;
	}

	bool Mixer::TryPlay(const Effect& effect) noexcept
	{
		for (auto& voice : this->voices)
		{
			auto expected = VoiceState::Free;

			if (!voice.phase.compare_exchange_strong(expected, VoiceState::Filling, std::memory_order_acquire, std::memory_order_relaxed))
			{
				continue;
			}

			voice.effect = effect;
			voice.isStopping.store(false, std::memory_order_relaxed);

			voice.phase.store(VoiceState::Ready, std::memory_order_release);
			return true;
		}

		this->dropped.fetch_add(1, std::memory_order_relaxed);
		return false;
	}

	void Mixer::Stop(std::uint32_t tag) noexcept
	{
		if (tag == 0)
		{
			return;
		}

		for (auto& voice : this->voices)
		{
			const auto phase = voice.phase.load(std::memory_order_acquire);

			if (phase != VoiceState::Ready && phase != VoiceState::Playing)
			{
				continue;
			}

			if (voice.effect.tag == tag)
			{
				voice.isStopping.store(true, std::memory_order_relaxed);
			}
		}
	}

	void Mixer::SetRumble(float lowFrequency, float highFrequency) noexcept
	{
		this->rumbleLow.store(std::clamp(lowFrequency, 0.0f, 1.0f), std::memory_order_relaxed);
		this->rumbleHigh.store(std::clamp(highFrequency, 0.0f, 1.0f), std::memory_order_relaxed);
	}

	void Mixer::RenderRumble(std::span<Frame> out, float step, float scale) noexcept
	{
		const float targetLow = this->rumbleLow.load(std::memory_order_relaxed) * scale;
		const float targetHigh = this->rumbleHigh.load(std::memory_order_relaxed) * scale;

		const float coefficient = std::min(step / rumbleRampSeconds, 1.0f);

		const float lowIncrement = rumbleLowHertz * step;
		const float highIncrement = rumbleHighHertz * step;

		for (auto& frame : out)
		{
			this->rumbleLowLevel = Approach(this->rumbleLowLevel, targetLow, coefficient);
			this->rumbleHighLevel = Approach(this->rumbleHighLevel, targetHigh, coefficient);

			frame.left += this->rumbleLowLevel * std::sin(this->rumbleLowPhase * twoPi);
			frame.right += this->rumbleHighLevel * std::sin(this->rumbleHighPhase * twoPi);

			Advance(this->rumbleLowPhase, lowIncrement);
			Advance(this->rumbleHighPhase, highIncrement);
		}
	}

	bool Mixer::RenderVoice(Voice& voice, std::span<Frame> out, float step) noexcept
	{
		const auto& effect = voice.effect;

		const float duration = effect.duration.count();

		if (!(duration > 0.0f))
		{
			return true;
		}

		const bool isLooping = effect.shouldLoop && !voice.isStopping.load(std::memory_order_relaxed);

		const float deepIncrement = HertzFor(effect.deepSharpness) * step;
		const float crispIncrement = HertzFor(effect.crispSharpness) * step;

		for (auto& frame : out)
		{
			if (voice.elapsed >= duration)
			{
				if (!isLooping)
				{
					return true;
				}

				voice.elapsed -= duration;
			}

			const float t = voice.elapsed / duration;

			const float deep = effect.deep.Evaluate(t) * effect.intensity * std::sin(voice.deepPhase * twoPi);
			const float crisp = effect.crisp.Evaluate(t) * effect.intensity * std::sin(voice.crispPhase * twoPi);

			switch (effect.where)
			{
			case Actuator::Both:
				frame.left += deep;
				frame.right += crisp;
				break;

			case Actuator::Left:
				frame.left += deep + crisp;
				break;

			case Actuator::Right:
				frame.right += deep + crisp;
				break;
			}

			Advance(voice.deepPhase, deepIncrement);
			Advance(voice.crispPhase, crispIncrement);

			voice.elapsed += step;
		}

		return false;
	}

	void Mixer::Render(std::span<Frame> out, std::uint32_t rate) noexcept
	{
		if (rate == 0)
		{
			return;
		}

		const float step = 1.0f / static_cast<float>(rate);

		std::fill(out.begin(), out.end(), Frame{});

		bool hasEffects = false;

		for (auto& voice : this->voices)
		{
			auto phase = voice.phase.load(std::memory_order_acquire);

			if (phase == VoiceState::Ready)
			{
				voice.elapsed = 0.0f;
				voice.deepPhase = 0.0f;
				voice.crispPhase = 0.0f;

				voice.phase.store(VoiceState::Playing, std::memory_order_relaxed);
				phase = VoiceState::Playing;
			}

			if (phase != VoiceState::Playing)
			{
				continue;
			}

			hasEffects = true;

			if (RenderVoice(voice, out, step))
			{
				voice.phase.store(VoiceState::Free, std::memory_order_release);
			}
		}

		float rumbleScale = 1.0f;

		if (hasEffects)
		{
			rumbleScale = 0.0f;
		}

		this->RenderRumble(out, step, rumbleScale);

		for (auto& frame : out)
		{
			frame.left = std::clamp(frame.left, -1.0f, 1.0f);
			frame.right = std::clamp(frame.right, -1.0f, 1.0f);
		}
	}
}
