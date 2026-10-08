#include "STDInclude.hpp"

#include "Controller/Driver/Decode.hpp"

namespace Controller::Driver
{
	std::uint8_t ReadU8(std::span<const std::byte> data, std::size_t offset) noexcept
	{
		assert(offset < data.size());
		return static_cast<std::uint8_t>(data[offset]);
	}

	std::uint16_t ReadLe16(std::span<const std::byte> data, std::size_t offset) noexcept
	{
		assert(offset + 1 < data.size());

		const auto low = static_cast<std::uint16_t>(data[offset]);
		const auto high = static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8);

		return static_cast<std::uint16_t>(low | high);
	}

	std::int16_t ReadLe16Signed(std::span<const std::byte> data, std::size_t offset) noexcept
	{
		return static_cast<std::int16_t>(ReadLe16(data, offset));
	}

	std::uint32_t ReadLe32(std::span<const std::byte> data, std::size_t offset) noexcept
	{
		assert(offset + 3 < data.size());

		return static_cast<std::uint32_t>(data[offset])
			| (static_cast<std::uint32_t>(data[offset + 1]) << 8)
			| (static_cast<std::uint32_t>(data[offset + 2]) << 16)
			| (static_cast<std::uint32_t>(data[offset + 3]) << 24);
	}

	std::uint32_t Crc32Le(std::uint32_t crc, std::span<const std::byte> data) noexcept
	{
		constexpr std::uint32_t reflectedPolynomial = 0xEDB88320u;

		for (const auto value : data)
		{
			crc ^= static_cast<std::uint8_t>(value);

			for (int bit = 0; bit < 8; ++bit)
			{
				if ((crc & 1u) != 0)
				{
					crc = (crc >> 1) ^ reflectedPolynomial;
				}
				else
				{
					crc >>= 1;
				}
			}
		}

		return crc;
	}

	bool IsPsCrc32Valid(std::uint8_t seed, std::span<const std::byte> data, std::uint32_t expected) noexcept
	{
		const auto seedByte = static_cast<std::byte>(seed);

		std::uint32_t crc = Crc32Le(0xFFFFFFFFu, std::span<const std::byte>(&seedByte, 1));
		crc = ~Crc32Le(crc, data);

		return crc == expected;
	}

	PsTouchPoint DecodeTouchPoint(std::span<const std::byte> point) noexcept
	{
		assert(point.size() >= 4);

		const auto contact = static_cast<std::uint8_t>(point[0]);
		const auto xLow = static_cast<std::uint8_t>(point[1]);
		const auto middle = static_cast<std::uint8_t>(point[2]);
		const auto yHigh = static_cast<std::uint8_t>(point[3]);

		PsTouchPoint decoded{};
		decoded.isActive = (contact & 0x80u) == 0;
		decoded.id = static_cast<std::uint8_t>(contact & 0x7Fu);
		decoded.x = static_cast<std::uint16_t>(xLow | ((middle & 0x0Fu) << 8));
		decoded.y = static_cast<std::uint16_t>((middle >> 4) | (yHigh << 4));
		return decoded;
	}

	void ApplyHat(ButtonSet& buttons, std::uint8_t hat) noexcept
	{
		struct Direction
		{
			int x;
			int y;
		};

		static constexpr Direction directions[8] =
		{
			{ 0, -1 },
			{ 1, -1 },
			{ 1, 0 },
			{ 1, 1 },
			{ 0, 1 },
			{ -1, 1 },
			{ -1, 0 },
			{ -1, -1 },
		};

		if (hat >= 8)
		{
			return;
		}

		const auto direction = directions[hat];
		buttons.Set(Button::DpadUp, direction.y < 0);
		buttons.Set(Button::DpadDown, direction.y > 0);
		buttons.Set(Button::DpadLeft, direction.x < 0);
		buttons.Set(Button::DpadRight, direction.x > 0);
	}

	StickVector NormalizePsStick(std::uint8_t x, std::uint8_t y) noexcept
	{
		float normalizedX = std::clamp((static_cast<float>(x) - 128.0f) / 127.0f, -1.0f, 1.0f);
		float normalizedY = std::clamp((128.0f - static_cast<float>(y)) / 127.0f, -1.0f, 1.0f);

		const float magnitude = std::sqrt(normalizedX * normalizedX + normalizedY * normalizedY);

		if (magnitude > 1.0f)
		{
			normalizedX /= magnitude;
			normalizedY /= magnitude;
		}

		return { normalizedX, normalizedY };
	}
}
