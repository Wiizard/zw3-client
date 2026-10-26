#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Components::Menu::BackdropData
{
	inline float smooth_pulse(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	enum class loading_overlay { none, startup_black, map_signal };

	inline loading_overlay loading_overlay_for(bool startup_complete, bool changing_map)
	{
		if (!startup_complete) return loading_overlay::startup_black;
		return changing_map ? loading_overlay::map_signal : loading_overlay::none;
	}

	inline std::uint32_t spinner_glow_bits(std::uint32_t base)
	{
		// Add light without darkening existing UI or replacing its composition alpha.
		constexpr std::uint32_t rgb_blend_mask = 0x7FFu;
		constexpr std::uint32_t write_alpha = 0x10000000u;
		constexpr std::uint32_t src_alpha_add_one = 0x125u;
		return (base & ~(rgb_blend_mask | write_alpha)) | src_alpha_add_one;
	}

	inline constexpr int signal_layer_count = 3;
	inline constexpr int signal_texture_width = 4096;
	inline constexpr int signal_texture_height = 256;

	struct signal_profile
	{
		float centre, companion, energy;
	};
	inline float signal_noise(float u, unsigned int period, std::uint32_t seed)
	{
		const float position = (u - std::floor(u)) * period;
		const auto index = static_cast<unsigned int>(position);
		const auto sample = [period, seed](unsigned int i)
		{
			std::uint32_t value = (i % period) * 374761393u + seed * 668265263u;
			value = (value ^ (value >> 13)) * 1274126177u;
			value ^= value >> 16;
			return static_cast<float>(value & 65535u) / 32767.5f - 1.0f;
		};
		const float t = smooth_pulse(position - index);
		return sample(index) * (1 - t) + sample(index + 1) * t;
	}

	// Periodic profiles let a high-resolution trace scroll without rebuilding
	// hundreds of UI quads. Its endpoints and derivatives meet at the seam.
	inline signal_profile signal_at(float u, int layer)
	{
		constexpr float tau = 6.28318530718f;
		const auto seed = static_cast<std::uint32_t>(layer + 1);
		const float packet = std::pow(0.5f + 0.5f * std::sin(tau * u * 3 + layer * 1.73f), 3.0f);
		const float envelope = (0.045f + 0.955f * packet)
			* (0.65f + 0.35f * (signal_noise(u, 19, seed + 13) * 0.5f + 0.5f));
		// Irregular, densely sampled audio transients separated by quiet runs.
		// A restrained centre beam and finer afterimages replace rolling sine ribbons.
		const float carrier = 0.60f * signal_noise(u, 127, seed)
			+ 0.28f * signal_noise(u, 331, seed + 5)
			+ 0.12f * signal_noise(u, 887, seed + 11);
		return {0.90f * envelope * carrier,
			0.52f * envelope * signal_noise(u, 197, seed + 31),
			0.45f + 0.55f * packet};
	}

	struct signal_strip
	{
		float x, y, width, height, s0, s1, alpha;
		int layer;
	};

	// At most two cropped quads per layer, with no wrapped-UV sampler required.
	// The menu's shared command buffer sees at most six draws plus its veil,
	// independent of resolution or the number of filaments in the texture.
	template <typename Draw>
	void signal_strips(int width, int height, int elapsed, Draw draw)
	{
		if (width <= 0 || height <= 0) return;
		const double time = std::max(0, elapsed) * 0.001;
		constexpr double speeds[] = {0.065, -0.037, 0.091};
		constexpr float strengths[] = {0.95f, 0.32f, 0.18f};
		for (int layer = 0; layer < signal_layer_count; ++layer)
		{
			const double travel = time * speeds[layer] + layer * 0.213;
			const float offset = static_cast<float>(travel - std::floor(travel));
			const float phase = static_cast<float>(time * 2.4 + layer * 1.7);
			const float band = height * (0.125f + 0.020f * std::sin(phase));
			const float top = height * 0.5f - band * 0.5f;
			const float alpha = strengths[layer] * (0.86f + 0.14f * std::sin(phase));
			const float first = width * (1 - offset);
			if (first > 0.001f) draw(signal_strip{0, top, first, band, offset, 1, alpha, layer});
			if (width - first > 0.001f) draw(signal_strip{first, top, width - first, band, 0, offset, alpha, layer});
		}
	}

	class startup_barrier
	{
	public:
		void observe(bool complete)
		{
			if (!complete) reset();
			else complete_frames_ = std::min(complete_frames_ + 1, 2);
		}
		bool pending() const { return !released_ && complete_frames_ == 2; }
		bool begin_frame()
		{
			if (!pending()) return false;
			released_ = true;
			return true;
		}
		bool ready() const { return released_; }
		void reset() { complete_frames_ = 0; released_ = false; }
	private:
		int complete_frames_ = 0;
		bool released_ = false;
	};

	// The cover survives unloading a world. Only a ready scene starts its reveal.
	class backdrop_transition
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
