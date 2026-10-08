#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Device/Identity.hpp"

namespace Controller::Transport
{
	struct HidAttributes
	{
		VendorId vendor;
		ProductId product;
		std::optional<std::uint16_t> version;
	};

	class HidDevice
	{
	public:
		virtual ~HidDevice() = default;

		virtual Connection Link() const noexcept = 0;
		virtual const std::wstring& Path() const noexcept = 0;
		virtual std::size_t FeatureReportLength() const noexcept = 0;

		virtual std::optional<std::size_t> TryRead(std::span<std::byte> buffer) noexcept = 0;
		virtual std::optional<std::size_t> TryWrite(std::span<const std::byte> buffer) noexcept = 0;
		virtual bool TryGetFeature(std::span<std::byte> buffer) noexcept = 0;
	};

	struct HidEnumerationEntry
	{
		std::wstring path;
		HidAttributes attributes;
		Connection link = Connection::Unknown;
		std::size_t inputReportLength = 0;
	};

	Connection ClassifyLink(std::size_t inputReportLength) noexcept;

	std::vector<HidEnumerationEntry> Enumerate(const Context& context);

	std::unique_ptr<HidDevice> TryOpen(const Context& context, const std::wstring& path);
}
