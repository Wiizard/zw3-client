#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>

namespace Utils
{
	inline POINT ReadLegacyMousePosition(const LPARAM position)
	{
		// Client coordinates are signed, including releases outside the window.
		return { static_cast<SHORT>(LOWORD(position)), static_cast<SHORT>(HIWORD(position)) };
	}

	inline bool HasRawMouseButtonTransition(const USHORT flags)
	{
		constexpr USHORT buttons = RI_MOUSE_BUTTON_1_DOWN | RI_MOUSE_BUTTON_1_UP
			| RI_MOUSE_BUTTON_2_DOWN | RI_MOUSE_BUTTON_2_UP
			| RI_MOUSE_BUTTON_3_DOWN | RI_MOUSE_BUTTON_3_UP
			| RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP
			| RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;
		return (flags & buttons) != 0;
	}

	template <typename Synchronize, typename Forward>
	inline void ForwardMouseButtonState(const std::uint32_t buttons, const Synchronize& synchronize, const Forward& forward)
	{
		// The native key queue may be drained before the next input frame.
		// Refresh UI hit-testing first; forward one unchanged state mask only.
		synchronize();
		forward(buttons);
	}

	inline std::optional<std::uint32_t> ReadLegacyMouseButtons(const UINT message, const WPARAM state)
	{
		// Only button messages carry an authoritative button-state transition.
		// In particular, a zero-state WM_MOUSEMOVE must not release raw buttons.
		switch (message)
		{
		case WM_LBUTTONDOWN: case WM_LBUTTONUP:
		case WM_RBUTTONDOWN: case WM_RBUTTONUP:
		case WM_MBUTTONDOWN: case WM_MBUTTONUP:
		case WM_XBUTTONDOWN: case WM_XBUTTONUP:
			break;
		default:
			return std::nullopt;
		}

		std::uint32_t buttons = 0;
		if (state & MK_LBUTTON) buttons |= 1;
		if (state & MK_RBUTTON) buttons |= 2;
		if (state & MK_MBUTTON) buttons |= 4;
		if (state & MK_XBUTTON1) buttons |= 8;
		if (state & MK_XBUTTON2) buttons |= 16;
		return buttons; // A final release (zero) is a valid state, not no event.
	}
}
