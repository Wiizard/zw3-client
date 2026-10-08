#include "STDInclude.hpp"

#include "Controller/Device/Id.hpp"

namespace Controller
{
	const char* ToString(TransportKind transport) noexcept
	{
		switch (transport)
		{
		case TransportKind::Unknown:
			return "unknown";
		case TransportKind::XInput:
			return "xinput";
		case TransportKind::RawInput:
			return "raw-input";
		case TransportKind::Hid:
			return "hid";
		}

		return "unknown";
	}

	const char* ToString(Connection link) noexcept
	{
		switch (link)
		{
		case Connection::Unknown:
			return "unknown";
		case Connection::Usb:
			return "usb";
		case Connection::Bluetooth:
			return "bluetooth";
		case Connection::Virtualized:
			return "virtualized";
		}

		return "unknown";
	}
}
