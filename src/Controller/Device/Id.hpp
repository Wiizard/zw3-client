#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	class DeviceId
	{
	public:
		constexpr DeviceId() = default;

		constexpr explicit DeviceId(std::uint32_t value) noexcept : value(value)
		{
		}

		constexpr std::uint32_t Value() const noexcept
		{
			return this->value;
		}

		constexpr explicit operator bool() const noexcept
		{
			return this->value != 0;
		}

		friend constexpr bool operator==(DeviceId, DeviceId) noexcept = default;

	private:
		std::uint32_t value = 0;
	};

	inline constexpr DeviceId noDevice{};

	enum class TransportKind : std::uint8_t
	{
		Unknown,
		XInput,
		RawInput,
		Hid,
	};

	const char* ToString(TransportKind transport) noexcept;

	enum class Connection : std::uint8_t
	{
		Unknown,
		Usb,
		Bluetooth,
		Virtualized,
	};

	const char* ToString(Connection link) noexcept;

	class UserIndex
	{
	public:
		static constexpr std::uint8_t count = 4;

		constexpr explicit UserIndex(std::uint8_t value) noexcept : value(value)
		{
		}

		constexpr std::uint8_t Value() const noexcept
		{
			return this->value;
		}

		friend constexpr bool operator==(UserIndex, UserIndex) noexcept = default;

	private:
		std::uint8_t value;
	};
}
