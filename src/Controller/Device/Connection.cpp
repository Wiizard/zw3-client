#include "STDInclude.hpp"

#include "Controller/Device/Connection.hpp"

namespace Controller
{
	bool IsSameBinding(const TransportBinding& left, const TransportBinding& right) noexcept
	{
		if (left.index() != right.index())
		{
			return false;
		}

		if (const auto* xinput = std::get_if<XInputBinding>(&left))
		{
			return xinput->index == std::get<XInputBinding>(right).index;
		}

		if (const auto* hid = std::get_if<HidBinding>(&left))
		{
			return hid->path == std::get<HidBinding>(right).path;
		}

		return false;
	}
}
