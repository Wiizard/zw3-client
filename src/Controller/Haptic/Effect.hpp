#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"

namespace Controller::Haptic
{
	struct Frame
	{
		float left = 0.0f;
		float right = 0.0f;
	};

	enum class Actuator : std::uint8_t
	{
		Left,
		Right,
		Both,
	};

	class Envelope
	{
	public:
		static constexpr std::size_t maxKnots = 16;

		struct Knot
		{
			float at = 0.0f;
			float amplitude = 0.0f;
		};

		constexpr Envelope() = default;

		static Envelope Level(float amplitude) noexcept;
		static Envelope From(std::span<const Knot> source) noexcept;

		float Evaluate(float t) const noexcept;

		bool IsEmpty() const noexcept
		{
			return this->knotCount == 0;
		}

	private:
		std::array<Knot, maxKnots> knots{};
		std::size_t knotCount = 0;
	};

	inline constexpr float minHertz = 40.0f;
	inline constexpr float maxHertz = 320.0f;

	float HertzFor(float sharpness) noexcept;

	struct Effect
	{
		Envelope deep;
		Envelope crisp;

		float deepSharpness = 0.0f;
		float crispSharpness = 1.0f;

		float intensity = 1.0f;

		Seconds duration{ 0.25f };

		bool shouldLoop = false;

		Actuator where = Actuator::Both;

		std::uint32_t tag = 0;
	};

	Effect Transient(float intensity, float sharpness) noexcept;
	Effect Continuous(float intensity, float sharpness, Seconds duration) noexcept;
}
