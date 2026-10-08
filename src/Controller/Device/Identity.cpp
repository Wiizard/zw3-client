#include "STDInclude.hpp"

#include "Controller/Device/Identity.hpp"

namespace Controller
{
	const char* ToString(Family family) noexcept
	{
		switch (family)
		{
		case Family::Unknown:
			return "unknown";
		case Family::Xbox:
			return "xbox";
		case Family::DualShock4:
			return "dualshock4";
		case Family::DualSense:
			return "dualsense";
		case Family::DualSenseEdge:
			return "dualsense-edge";
		}

		return "unknown";
	}

	Family Classify(VendorId vendor, ProductId product) noexcept
	{
		if (vendor != vendorSony)
		{
			return Family::Unknown;
		}

		if (product == productDualSenseEdge)
		{
			return Family::DualSenseEdge;
		}

		if (product == productDualSense)
		{
			return Family::DualSense;
		}

		if (product == productDs4Gen1 || product == productDs4Gen2 || product == productDs4Dongle)
		{
			return Family::DualShock4;
		}

		return Family::Unknown;
	}
}
