#include "STDInclude.hpp"

#include "Controller/Engine/Rumble.hpp"

namespace Controller::Engine
{
	static constexpr float millisecondsPerSecond = 1000.0f;

	static constexpr float deepSharpness = 0.0f;
	static constexpr float crispSharpness = 0.7f;

	static Haptic::Envelope EnvelopeFrom(const Game::RumbleGraph* graph) noexcept
	{
		if (graph == nullptr || graph->knotCount == 0)
		{
			return {};
		}

		const std::size_t count = std::min(static_cast<std::size_t>(graph->knotCount), Haptic::Envelope::maxKnots);

		std::array<Haptic::Envelope::Knot, Haptic::Envelope::maxKnots> knots{};

		for (std::size_t i = 0; i != count; ++i)
		{
			knots[i] = { graph->knots[i][0], graph->knots[i][1] };
		}

		return Haptic::Envelope::From({ knots.data(), count });
	}

	bool TryEffectFromRumble(const Game::RumbleInfo& info, float scale, bool shouldLoop, Haptic::Effect& out) noexcept
	{
		if (!(info.duration > 0.0f))
		{
			return false;
		}

		Haptic::Effect effect;
		effect.deep = EnvelopeFrom(info.lowRumbleGraph);
		effect.crisp = EnvelopeFrom(info.highRumbleGraph);

		if (effect.deep.IsEmpty() && effect.crisp.IsEmpty())
		{
			return false;
		}

		effect.deepSharpness = deepSharpness;
		effect.crispSharpness = crispSharpness;
		effect.intensity = std::clamp(scale, 0.0f, 1.0f);
		effect.duration = Seconds{ info.duration / millisecondsPerSecond };
		effect.shouldLoop = shouldLoop;

		effect.tag = static_cast<std::uint32_t>(info.rumbleNameIndex + 1);

		out = effect;
		return true;
	}
}
