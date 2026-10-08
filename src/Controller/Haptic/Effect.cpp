#include "STDInclude.hpp"

#include "Controller/Haptic/Effect.hpp"

namespace Controller::Haptic
{
	static constexpr float bodyOffset = 0.35f;

	static Envelope Struck() noexcept
	{
		static constexpr Envelope::Knot knots[] =
		{
			{ 0.00f, 0.0f },
			{ 0.04f, 1.0f },
			{ 0.35f, 0.45f },
			{ 1.00f, 0.0f },
		};

		return Envelope::From(knots);
	}

	static Seconds TransientDuration(float sharpness) noexcept
	{
		return Seconds{ std::lerp(0.09f, 0.02f, std::clamp(sharpness, 0.0f, 1.0f)) };
	}

	static bool IsInUnitRange(float value) noexcept
	{
		return value >= 0.0f && value <= 1.0f;
	}

	Envelope Envelope::Level(float amplitude) noexcept
	{
		const float level = std::clamp(amplitude, 0.0f, 1.0f);

		const Knot levelKnots[] = { { 0.0f, level }, { 1.0f, level } };
		return From(levelKnots);
	}

	Envelope Envelope::From(std::span<const Knot> source) noexcept
	{
		Envelope envelope;

		float lastAt = -1.0f;

		for (const auto& knot : source)
		{
			if (envelope.knotCount == maxKnots)
			{
				break;
			}

			if (!IsInUnitRange(knot.at) || !IsInUnitRange(knot.amplitude) || knot.at <= lastAt)
			{
				continue;
			}

			envelope.knots[envelope.knotCount] = knot;
			++envelope.knotCount;
			lastAt = knot.at;
		}

		if (envelope.knotCount < 2)
		{
			envelope.knotCount = 0;
		}

		return envelope;
	}

	float Envelope::Evaluate(float t) const noexcept
	{
		if (this->knotCount == 0)
		{
			return 0.0f;
		}

		const auto& first = this->knots[0];
		const auto& last = this->knots[this->knotCount - 1];

		if (t <= first.at)
		{
			return first.amplitude;
		}

		if (t >= last.at)
		{
			return last.amplitude;
		}

		for (std::size_t i = 1; i < this->knotCount; ++i)
		{
			if (t > this->knots[i].at)
			{
				continue;
			}

			const auto& from = this->knots[i - 1];
			const auto& to = this->knots[i];

			return std::lerp(from.amplitude, to.amplitude, (t - from.at) / (to.at - from.at));
		}

		return last.amplitude;
	}

	float HertzFor(float sharpness) noexcept
	{
		const float t = std::clamp(sharpness, 0.0f, 1.0f);
		return minHertz * std::pow(maxHertz / minHertz, t);
	}

	Effect Transient(float intensity, float sharpness) noexcept
	{
		const float clampedSharpness = std::clamp(sharpness, 0.0f, 1.0f);

		Effect effect;
		effect.deep = Struck();
		effect.crisp = Struck();
		effect.deepSharpness = std::max(0.0f, clampedSharpness - bodyOffset);
		effect.crispSharpness = clampedSharpness;
		effect.intensity = std::clamp(intensity, 0.0f, 1.0f);
		effect.duration = TransientDuration(clampedSharpness);
		return effect;
	}

	Effect Continuous(float intensity, float sharpness, Seconds duration) noexcept
	{
		const float clampedSharpness = std::clamp(sharpness, 0.0f, 1.0f);

		Effect effect;
		effect.deep = Envelope::Level(1.0f);
		effect.crisp = Envelope::Level(1.0f);
		effect.deepSharpness = std::max(0.0f, clampedSharpness - bodyOffset);
		effect.crispSharpness = clampedSharpness;
		effect.intensity = std::clamp(intensity, 0.0f, 1.0f);
		effect.duration = duration;
		return effect;
	}
}
