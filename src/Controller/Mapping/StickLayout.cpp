#include "STDInclude.hpp"

#include "Controller/Mapping/StickLayout.hpp"

namespace Controller::Mapping
{
	static bool IsLayoutName(std::string_view name, std::string_view layout) noexcept
	{
		return name.size() == layout.size() && _strnicmp(name.data(), layout.data(), name.size()) == 0;
	}

	static float Squared(float component, StickVector stick) noexcept
	{
		return std::clamp(stick.Magnitude() * component, -1.0f, 1.0f);
	}

	static VirtualAxis AxisFor(StickLayout layout, Stick which, bool isHorizontal) noexcept
	{
		const bool isSouthpaw = layout == StickLayout::Southpaw || layout == StickLayout::LegacySouthpaw;
		const bool isLegacy = layout == StickLayout::Legacy || layout == StickLayout::LegacySouthpaw;

		const bool isMoveStick = (which == Stick::Left) != isSouthpaw;

		if (!isHorizontal)
		{
			if (isMoveStick)
			{
				return VirtualAxis::Forward;
			}

			return VirtualAxis::Pitch;
		}

		if (isMoveStick)
		{
			if (isLegacy)
			{
				return VirtualAxis::Yaw;
			}

			return VirtualAxis::Side;
		}

		if (isLegacy)
		{
			return VirtualAxis::Side;
		}

		return VirtualAxis::Yaw;
	}

	StickLayout StickLayoutFromName(std::string_view name)
	{
		if (IsLayoutName(name, "thumbstick_legacysouthpaw"))
		{
			return StickLayout::LegacySouthpaw;
		}

		if (IsLayoutName(name, "thumbstick_legacy"))
		{
			return StickLayout::Legacy;
		}

		if (IsLayoutName(name, "thumbstick_southpaw"))
		{
			return StickLayout::Southpaw;
		}

		return StickLayout::Standard;
	}

	ResolvedAxes Resolve(StickLayout layout, StickVector left, StickVector right) noexcept
	{
		ResolvedAxes axes;

		static constexpr Stick sticks[] = { Stick::Left, Stick::Right };
		static constexpr bool orientations[] = { true, false };

		for (const auto stick : sticks)
		{
			StickVector vector = left;

			if (stick == Stick::Right)
			{
				vector = right;
			}

			for (const bool isHorizontal : orientations)
			{
				float component = vector.y;

				if (isHorizontal)
				{
					component = vector.x;
				}

				switch (AxisFor(layout, stick, isHorizontal))
				{
				case VirtualAxis::Side:
					axes.side = Squared(component, vector);
					break;

				case VirtualAxis::Forward:
					axes.forward = Squared(component, vector);
					break;

				case VirtualAxis::Yaw:
					axes.yaw = component;
					break;

				case VirtualAxis::Pitch:
					axes.pitch = component;
					break;
				}
			}
		}

		return axes;
	}
}
