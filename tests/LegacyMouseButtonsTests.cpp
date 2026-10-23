#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Utils/LegacyMouseButtons.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
	void Require(const bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		const UINT down[] = { WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN, WM_XBUTTONDOWN };
		const UINT up[] = { WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP, WM_XBUTTONUP };
		const WPARAM flags[] = { MK_LBUTTON, MK_RBUTTON, MK_MBUTTON, MK_XBUTTON1, MK_XBUTTON2 };
		for (unsigned int i = 0; i < 5; ++i)
		{
			const auto pressed = Utils::ReadLegacyMouseButtons(down[i], flags[i]);
			Require(pressed && *pressed == (1u << i), "legacy press mapping is incorrect");
			const auto released = Utils::ReadLegacyMouseButtons(up[i], 0);
			Require(released.has_value() && *released == 0, "final legacy release was discarded");
			Require(Utils::ReadLegacyMouseButtons(down[i], flags[i]) == pressed, "repeated press changed state mask");
			Require(Utils::ReadLegacyMouseButtons(up[i], 0) == released, "repeated release changed state mask");
		}
		Require(Utils::ReadLegacyMouseButtons(WM_LBUTTONDOWN, MK_LBUTTON | MK_RBUTTON) == 3u,
			"simultaneous button state was lost");
		Require(Utils::ReadLegacyMouseButtons(WM_LBUTTONUP, MK_RBUTTON) == 2u,
			"releasing left also released a held right button");
		Require(Utils::ReadLegacyMouseButtons(WM_RBUTTONUP, MK_SHIFT | MK_CONTROL) == 0u,
			"keyboard modifiers became mouse buttons");
		Require(Utils::ReadLegacyMouseButtons(WM_XBUTTONDOWN, MAKEWPARAM(MK_XBUTTON2, XBUTTON2)) == 16u,
			"XBUTTON message high word corrupted the state");
		Require(!Utils::ReadLegacyMouseButtons(WM_MOUSEMOVE, 0), "zero-state mouse movement released raw input");
		Require(!Utils::ReadLegacyMouseButtons(WM_MOUSEMOVE, MK_LBUTTON), "mouse movement was treated as a button transition");
		Require(!Utils::ReadLegacyMouseButtons(WM_MOUSEWHEEL, 0), "wheel message was treated as a button transition");
		Require(!Utils::ReadLegacyMouseButtons(WM_KILLFOCUS, 0), "focus behavior was replaced by legacy button mapping");
		const auto negative = Utils::ReadLegacyMousePosition(MAKELPARAM(static_cast<WORD>(-123), static_cast<WORD>(-456)));
		Require(negative.x == -123 && negative.y == -456, "negative client coordinates lost their sign");
		const auto positive = Utils::ReadLegacyMousePosition(MAKELPARAM(1920, 1080));
		Require(positive.x == 1920 && positive.y == 1080, "client position was scaled or changed");
		Require(!Utils::HasRawMouseButtonTransition(0) && !Utils::HasRawMouseButtonTransition(RI_MOUSE_WHEEL),
			"raw motion or wheel unnecessarily synchronized button hit-testing");
		const USHORT rawFlags[] = { RI_MOUSE_BUTTON_1_DOWN, RI_MOUSE_BUTTON_1_UP, RI_MOUSE_BUTTON_2_DOWN, RI_MOUSE_BUTTON_2_UP,
			RI_MOUSE_BUTTON_3_DOWN, RI_MOUSE_BUTTON_3_UP, RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP };
		for (const auto flag : rawFlags)
			Require(Utils::HasRawMouseButtonTransition(flag), "raw button transition did not synchronize hit-testing");
		for (const std::uint32_t mask : { 0u, 1u, 3u, 16u })
		{
			std::vector<int> order;
			Utils::ForwardMouseButtonState(mask, [&] { order.push_back(-1); },
				[&](const std::uint32_t forwarded) { order.push_back(static_cast<int>(forwarded)); });
			Require(order.size() == 2 && order[0] == -1 && order[1] == static_cast<int>(mask),
				"button state was queued before UI synchronization, duplicated or changed");
		}
		std::cout << "Mouse button release, client coordinates, transition filtering and UI-before-key ordering tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
