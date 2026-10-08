#include "STDInclude.hpp"

#include "Controller/Aim/Graph.hpp"

namespace Controller::Aim
{
	std::optional<AimGraph> AimGraph::TryMake(std::span<const Knot> source, bool isMonotonicRequired, std::string& why)
	{
		if (source.size() < 2)
		{
			why = "an aim graph requires at least two knots";
			return std::nullopt;
		}

		if (source.size() > maxGraphKnots)
		{
			why = "an aim graph has too many knots";
			return std::nullopt;
		}

		bool isNonDecreasing = true;

		for (std::size_t i = 0; i < source.size(); ++i)
		{
			if (!std::isfinite(source[i].input) || !std::isfinite(source[i].output))
			{
				why = "aim graph knot values must be finite";
				return std::nullopt;
			}

			if (source[i].input < 0.0f || source[i].input > 1.0f)
			{
				why = "aim graph knot inputs must lie in [0, 1]";
				return std::nullopt;
			}

			if (i == 0)
			{
				continue;
			}

			if (source[i].input <= source[i - 1].input)
			{
				why = "aim graph knot inputs must be strictly increasing";
				return std::nullopt;
			}

			if (source[i].output < source[i - 1].output)
			{
				isNonDecreasing = false;
			}
		}

		if (isMonotonicRequired && !isNonDecreasing)
		{
			why = "a monotonic aim graph requires non-decreasing outputs";
			return std::nullopt;
		}

		AimGraph graph;

		for (const auto& knot : source)
		{
			graph.knots[graph.knotCount] = knot;
			++graph.knotCount;
		}

		return graph;
	}

	float AimGraph::Evaluate(float input) const noexcept
	{
		assert(this->knotCount >= 2);

		const auto& first = this->knots[0];
		const auto& last = this->knots[this->knotCount - 1];

		if (input <= first.input)
		{
			return first.output;
		}

		if (input >= last.input)
		{
			return last.output;
		}

		for (std::size_t i = 1; i < this->knotCount; ++i)
		{
			if (input > this->knots[i].input)
			{
				continue;
			}

			const auto& from = this->knots[i - 1];
			const auto& to = this->knots[i];

			const float span = to.input - from.input;
			assert(span > 0.0f);

			const float t = (input - from.input) / span;
			return from.output + (to.output - from.output) * t;
		}

		return last.output;
	}
}
