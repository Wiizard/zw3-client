#pragma once

#include "Controller/Types.hpp"

namespace Controller::Aim
{
	struct Knot
	{
		float input = 0.0f;
		float output = 0.0f;
	};

	inline constexpr std::size_t maxGraphKnots = 32;

	class AimGraph
	{
	public:
		static std::optional<AimGraph> TryMake(std::span<const Knot> source, bool isMonotonicRequired, std::string& why);

		float Evaluate(float input) const noexcept;

	private:
		AimGraph() = default;

		std::array<Knot, maxGraphKnots> knots{};
		std::size_t knotCount = 0;
	};
}
