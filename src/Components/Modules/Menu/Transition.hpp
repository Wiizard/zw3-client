#pragma once

#include <algorithm>
#include <cmath>

namespace Components::Menu::BackdropData
{
	struct transport_arc
	{
		float vertices[4][2];
		float alpha;
	};

	// Isolated Kino electric arcs along an ellipse, rather than a repeated
	// fullscreen texture. Keep the original 4:1 aspect ratio and never enlarge
	// the 512x128 source. Motion and intensity are continuous at any frame rate.
	template <typename Draw>
	void transport_arcs(int width, int height, int elapsed, Draw draw)
	{
		if (width <= 0 || height <= 0) return;
		constexpr float tau = 6.28318530718f;
		const float time = std::max(0, elapsed) * 0.001f;
		const float length = std::min({440.0f, width * 0.28f, height * 0.42f});
		for (int i = 0; i < 9; ++i)
		{
			const float phase = i * 2.39996323f;
			const float angle = tau * i / 9 + 0.035f * std::sin(time * 0.7f + phase);
			const float x = width * (0.5f + 0.32f * std::cos(angle));
			const float y = height * (0.5f + 0.30f * std::sin(angle));
			const float tangent = angle + tau * 0.25f + 0.16f * std::sin(time + phase);
			const float dx = std::cos(tangent) * length * 0.5f;
			const float dy = std::sin(tangent) * length * 0.5f;
			const float nx = -std::sin(tangent) * length * 0.125f;
			const float ny = std::cos(tangent) * length * 0.125f;
			const float pulse = 0.5f + 0.5f * std::sin(time * 3.1f + phase);
			draw(transport_arc{{{x - dx - nx, y - dy - ny}, {x + dx - nx, y + dy - ny},
				{x + dx + nx, y + dy + ny}, {x - dx + nx, y - dy + ny}},
				0.28f + 0.5f * pulse * pulse});
		}
	}

	// The cover survives unloading a world. Only a ready scene starts its reveal.
	class teleport_transition
	{
	public:
		static constexpr int close_msec = 450;
		static constexpr int hold_msec = 120;
		static constexpr int open_msec = 900;

		bool active() const { return started_ >= 0; }
		int started_at(int fallback) const { return active() ? started_ : fallback; }
		bool closing() const { return active() && target_ == 1.0f; }
		bool opening() const { return active() && target_ == 0.0f; }

		float amount(int now) const
		{
			if (!active()) return 0.0f;
			const float t = std::clamp(static_cast<float>(now - changed_) / duration_, 0.0f, 1.0f);
			return from_ + (target_ - from_) * t * t * (3.0f - 2.0f * t);
		}

		void close(int now) { change(now, 1.0f, close_msec); }
		void hold(int now) { close(now); from_ = 1.0f; }
		void open(int now) { if (active()) change(now, 0.0f, open_msec); }
		void reset() { started_ = -1; }
		bool covered(int now) const { return closing() && now - changed_ >= duration_ + hold_msec; }
		bool finished(int now) const { return opening() && now - changed_ >= duration_; }

	private:
		void change(int now, float target, int duration)
		{
			from_ = amount(now);
			if (!active()) started_ = now;
			changed_ = now;
			target_ = target;
			duration_ = std::max(1, static_cast<int>(duration * std::abs(target_ - from_)));
		}

		int started_ = -1;
		int changed_ = 0;
		int duration_ = 1;
		float from_ = 0.0f;
		float target_ = 0.0f;
	};
}
