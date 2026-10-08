#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	class VendorId
	{
	public:
		constexpr explicit VendorId(std::uint16_t value) noexcept : value(value)
		{
		}

		constexpr std::uint16_t Value() const noexcept
		{
			return this->value;
		}

		friend constexpr bool operator==(VendorId, VendorId) noexcept = default;

	private:
		std::uint16_t value;
	};

	class ProductId
	{
	public:
		constexpr explicit ProductId(std::uint16_t value) noexcept : value(value)
		{
		}

		constexpr std::uint16_t Value() const noexcept
		{
			return this->value;
		}

		friend constexpr bool operator==(ProductId, ProductId) noexcept = default;

	private:
		std::uint16_t value;
	};

	enum class Family : std::uint8_t
	{
		Unknown,
		Xbox,
		DualShock4,
		DualSense,
		DualSenseEdge,
	};

	inline constexpr std::size_t familyCount = 5;

	const char* ToString(Family family) noexcept;

	inline constexpr VendorId vendorSony{ 0x054C };
	inline constexpr VendorId vendorMicrosoft{ 0x045E };

	inline constexpr ProductId productDs4Gen1{ 0x05C4 };
	inline constexpr ProductId productDs4Gen2{ 0x09CC };
	inline constexpr ProductId productDs4Dongle{ 0x0BA0 };
	inline constexpr ProductId productDualSense{ 0x0CE6 };
	inline constexpr ProductId productDualSenseEdge{ 0x0DF2 };

	struct DeviceIdentity
	{
		Controller::Family family = Family::Unknown;
		std::optional<VendorId> vendor;
		std::optional<ProductId> product;
		std::optional<std::uint16_t> release;
	};

	Family Classify(VendorId vendor, ProductId product) noexcept;
}
