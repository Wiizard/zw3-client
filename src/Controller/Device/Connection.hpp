#pragma once

#include "Controller/Types.hpp"

#include "Controller/Device/Capability.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Device/Identity.hpp"

namespace Controller
{
	struct XInputBinding
	{
		UserIndex index;
	};

	struct HidBinding
	{
		std::wstring path;
	};

	using TransportBinding = std::variant<std::monostate, XInputBinding, HidBinding>;

	bool IsSameBinding(const TransportBinding& left, const TransportBinding& right) noexcept;

	struct DeviceConnection
	{
		DeviceId id{};
		DeviceIdentity identity{};
		TransportKind transport = TransportKind::Unknown;
		Connection link = Connection::Unknown;
		Capabilities caps{};
		TransportBinding binding{};
	};
}
