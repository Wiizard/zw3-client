#include "STDInclude.hpp"

#include <hidsdi.h>
#include <setupapi.h>

#include "Controller/Transport/Hid.hpp"

namespace Controller::Transport
{
	static constexpr std::size_t maxReportSize = 128;

	static constexpr DWORD writeTimeoutMs = 250;

	static constexpr std::size_t usbInputReportLength = 64;
	static constexpr std::size_t bluetoothInputReportLength = 78;

	static constexpr std::uint16_t usagePageGenericDesktop = 0x01;
	static constexpr std::uint16_t usageGamePad = 0x05;

	Connection ClassifyLink(std::size_t inputReportLength) noexcept
	{
		if (inputReportLength == bluetoothInputReportLength)
		{
			return Connection::Bluetooth;
		}

		if (inputReportLength == usbInputReportLength)
		{
			return Connection::Usb;
		}

		return Connection::Unknown;
	}

	struct WindowsHidOpening
	{
		HANDLE handle;
		std::wstring path;
		Connection link;
		std::size_t inputLength;
		std::size_t featureLength;
		bool isWritable;
	};

	class WindowsHidDevice : public HidDevice
	{
	public:
		explicit WindowsHidDevice(WindowsHidOpening opening) noexcept
			: handle(opening.handle),
			path(std::move(opening.path)),
			link(opening.link),
			inputLength(std::min(opening.inputLength, maxReportSize)),
			featureLength(std::min(opening.featureLength, maxReportSize)),
			isWritable(opening.isWritable)
		{
			this->overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		}

		~WindowsHidDevice() override
		{
			if (this->isReadPending)
			{
				CancelIoEx(this->handle, &this->overlapped);

				DWORD transferred = 0;
				GetOverlappedResult(this->handle, &this->overlapped, &transferred, TRUE);
			}

			if (this->overlapped.hEvent != nullptr)
			{
				CloseHandle(this->overlapped.hEvent);
			}

			if (this->handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(this->handle);
			}
		}

		WindowsHidDevice(const WindowsHidDevice&) = delete;
		WindowsHidDevice& operator=(const WindowsHidDevice&) = delete;

		Connection Link() const noexcept override
		{
			return this->link;
		}

		const std::wstring& Path() const noexcept override
		{
			return this->path;
		}

		std::size_t FeatureReportLength() const noexcept override
		{
			return this->featureLength;
		}

		std::optional<std::size_t> TryRead(std::span<std::byte> out) noexcept override
		{
			if (this->overlapped.hEvent == nullptr)
			{
				return std::nullopt;
			}

			if (!this->isReadPending && !this->TryIssueRead())
			{
				return std::nullopt;
			}

			DWORD transferred = 0;

			if (!GetOverlappedResult(this->handle, &this->overlapped, &transferred, FALSE))
			{
				if (GetLastError() == ERROR_IO_INCOMPLETE)
				{
					return std::size_t{ 0 };
				}

				this->isReadPending = false;
				return std::nullopt;
			}

			this->isReadPending = false;

			const std::size_t count = std::min(static_cast<std::size_t>(transferred), out.size());
			std::memcpy(out.data(), this->buffer.data(), count);

			this->TryIssueRead();

			return count;
		}

		std::optional<std::size_t> TryWrite(std::span<const std::byte> report) noexcept override
		{
			if (!this->isWritable || report.empty())
			{
				return std::nullopt;
			}

			OVERLAPPED writeOverlapped{};
			writeOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

			if (writeOverlapped.hEvent == nullptr)
			{
				return std::nullopt;
			}

			std::optional<std::size_t> written;
			DWORD transferred = 0;

			const bool isDone = WriteFile(this->handle, report.data(), static_cast<DWORD>(report.size()), &transferred, &writeOverlapped) != FALSE;
			const bool isFinished = isDone || (GetLastError() == ERROR_IO_PENDING && this->TryAwaitWrite(writeOverlapped, transferred));

			if (isFinished)
			{
				written = static_cast<std::size_t>(transferred);
			}

			CloseHandle(writeOverlapped.hEvent);
			return written;
		}

		bool TryGetFeature(std::span<std::byte> report) noexcept override
		{
			if (report.empty() || report.size() < this->featureLength)
			{
				return false;
			}

			return HidD_GetFeature(this->handle, report.data(), static_cast<ULONG>(report.size())) != FALSE;
		}

	private:
		bool TryAwaitWrite(OVERLAPPED& pending, DWORD& transferred) noexcept
		{
			if (WaitForSingleObject(pending.hEvent, writeTimeoutMs) != WAIT_OBJECT_0)
			{
				CancelIoEx(this->handle, &pending);
			}

			return GetOverlappedResult(this->handle, &pending, &transferred, TRUE) != FALSE;
		}

		bool TryIssueRead() noexcept
		{
			assert(!this->isReadPending);

			ResetEvent(this->overlapped.hEvent);
			this->overlapped.Internal = 0;
			this->overlapped.InternalHigh = 0;

			DWORD transferred = 0;

			if (ReadFile(this->handle, this->buffer.data(), static_cast<DWORD>(this->inputLength), &transferred, &this->overlapped))
			{
				this->isReadPending = true;
				return true;
			}

			if (GetLastError() == ERROR_IO_PENDING)
			{
				this->isReadPending = true;
				return true;
			}

			return false;
		}

		HANDLE handle;
		std::wstring path;
		Connection link;
		std::size_t inputLength;
		std::size_t featureLength;
		bool isWritable;

		bool isReadPending = false;
		OVERLAPPED overlapped{};

		std::array<std::byte, maxReportSize> buffer{};
	};

	struct CollectionCaps
	{
		std::size_t inputReportLength;
		std::size_t featureReportLength;
		std::uint16_t usagePage;
		std::uint16_t usage;
	};

	static std::optional<std::wstring> TryGetInterfacePath(HDEVINFO deviceSet, SP_DEVICE_INTERFACE_DATA& deviceInterface)
	{
		DWORD size = 0;
		SetupDiGetDeviceInterfaceDetailW(deviceSet, &deviceInterface, nullptr, 0, &size, nullptr);

		if (size == 0)
		{
			return std::nullopt;
		}

		std::vector<std::uint8_t> storage(size);

		auto* const detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

		if (!SetupDiGetDeviceInterfaceDetailW(deviceSet, &deviceInterface, detail, size, nullptr, nullptr))
		{
			return std::nullopt;
		}

		return std::wstring(detail->DevicePath);
	}

	static HANDLE OpenForQuery(const std::wstring& path) noexcept
	{
		return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	}

	static std::optional<CollectionCaps> TryQueryCaps(HANDLE device) noexcept
	{
		PHIDP_PREPARSED_DATA preparsed = nullptr;

		if (!HidD_GetPreparsedData(device, &preparsed))
		{
			return std::nullopt;
		}

		HIDP_CAPS caps{};
		const bool isRead = HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS;
		HidD_FreePreparsedData(preparsed);

		if (!isRead)
		{
			return std::nullopt;
		}

		return CollectionCaps{ caps.InputReportByteLength, caps.FeatureReportByteLength, caps.UsagePage, caps.Usage };
	}

	std::vector<HidEnumerationEntry> Enumerate(const Context& context)
	{
		std::vector<HidEnumerationEntry> found;

		GUID hidGuid{};
		HidD_GetHidGuid(&hidGuid);

		const HDEVINFO deviceSet = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

		if (deviceSet == INVALID_HANDLE_VALUE)
		{
			context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, "unable to enumerate HID device interfaces");
			return found;
		}

		SP_DEVICE_INTERFACE_DATA deviceInterface{};
		deviceInterface.cbSize = sizeof(deviceInterface);

		for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceSet, nullptr, &hidGuid, i, &deviceInterface); ++i)
		{
			auto path = TryGetInterfacePath(deviceSet, deviceInterface);

			if (!path)
			{
				continue;
			}

			const HANDLE device = OpenForQuery(*path);

			if (device == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			HIDD_ATTRIBUTES attributes{};
			attributes.Size = sizeof(attributes);

			std::optional<CollectionCaps> caps;

			if (HidD_GetAttributes(device, &attributes))
			{
				caps = TryQueryCaps(device);
			}

			CloseHandle(device);

			if (!caps)
			{
				continue;
			}

			if (caps->usagePage != usagePageGenericDesktop || caps->usage != usageGamePad)
			{
				continue;
			}

			const VendorId vendor(attributes.VendorID);
			const ProductId product(attributes.ProductID);

			if (Classify(vendor, product) == Family::Unknown)
			{
				continue;
			}

			found.push_back(HidEnumerationEntry{ std::move(*path), HidAttributes{ vendor, product, attributes.VersionNumber }, ClassifyLink(caps->inputReportLength), caps->inputReportLength });
		}

		SetupDiDestroyDeviceInfoList(deviceSet);
		return found;
	}

	std::unique_ptr<HidDevice> TryOpen(const Context& context, const std::wstring& path)
	{
		bool isWritable = true;

		HANDLE device = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

		if (device == INVALID_HANDLE_VALUE)
		{
			isWritable = false;
			device = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		}

		if (device == INVALID_HANDLE_VALUE)
		{
			context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, "unable to open a HID device interface");
			return nullptr;
		}

		HIDD_ATTRIBUTES attributes{};
		attributes.Size = sizeof(attributes);

		std::optional<CollectionCaps> caps;

		if (HidD_GetAttributes(device, &attributes))
		{
			caps = TryQueryCaps(device);
		}

		if (!caps)
		{
			CloseHandle(device);
			context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, "opened HID device does not answer its capabilities");
			return nullptr;
		}

		const VendorId vendor(attributes.VendorID);
		const ProductId product(attributes.ProductID);
		const Connection link = ClassifyLink(caps->inputReportLength);

		if (Classify(vendor, product) == Family::Unknown || link == Connection::Unknown)
		{
			CloseHandle(device);
			context.Report(Severity::Warning, Facility::Transport, ErrorCode::AmbiguousIdentity, "opened HID device is not a controller we can decode");
			return nullptr;
		}

		auto opened = std::make_unique<WindowsHidDevice>(WindowsHidOpening{ device, path, link, caps->inputReportLength, caps->featureReportLength, isWritable });

		if (!isWritable)
		{
			context.Report(Severity::Info, Facility::Transport, ErrorCode::None, "HID device opened read-only; output reports unavailable");
		}

		return opened;
	}
}
