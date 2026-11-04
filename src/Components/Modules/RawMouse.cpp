#include "STDInclude.hpp"

#include <hidusage.h>

#include "RawMouse.hpp"
#include "Events.hpp"
#include "Gamepad.hpp"
#include "LobbyScene.hpp"
#include "Logger.hpp"
#include "Window.hpp"
#include "Remix/Remix.hpp"
#include "Remix/Core.hpp"
#include "Scheduler.hpp"
#include "Utils/LegacyMouseButtons.hpp"
#include "Utils/MouseStartupDiagnostics.hpp"

namespace Components
{
	constexpr int mw_up = 205;
	constexpr int mw_down = 206;

	constexpr std::uintptr_t Sys_Init_INInitCall = 0x1402A5635;
	constexpr std::uintptr_t IN_Init_Engine = 0x1402A2D80;

	constexpr std::uintptr_t IN_FrameCalls[] = { 0x1400F6DE1, 0x1400FD3FA, 0x1401F6B21 };
	constexpr std::uintptr_t IN_Frame_Engine = 0x1402A2B70;

	constexpr std::uintptr_t MainWndProc_RecenterCall = 0x1402AAD48;
	constexpr std::uintptr_t IN_RecenterMouse_Engine = 0x1402A2E70;

	static Utils::Hook hooks[std::size(IN_FrameCalls) + 2];
	static Utils::MouseStartupDiagnostics startupDiagnostics;

	static void SaveMouseStartupDiagnostics(const bool asynchronous)
	{
		if (!startupDiagnostics.IsActive()) return;
		const auto samples = startupDiagnostics.Stop();
		const auto save = [samples]
		{
			// Write once after capture, never on the high-frequency mouse-message path.
			std::ostringstream text;
			text << "mouse startup: one-second samples; state bits raw=1 focus=2 ui=4 loading=8 engineActive=16\n"
				<< "second frames rawMessages maxFrameGapMs maxMessageAgeMs registrations failures warps states\n";
			for (std::size_t i = 0; i < samples.size(); ++i)
			{
				const auto& sample = samples[i];
				if (!sample.frames && !sample.rawMessages && !sample.registrations && !sample.warps) continue;
				text << i << ' ' << sample.frames << ' ' << sample.rawMessages << ' ' << sample.maximumFrameGapMs
					<< ' ' << sample.maximumMessageAgeMs << ' ' << sample.registrations << ' ' << sample.registrationFailures
					<< ' ' << sample.warps << ' ' << sample.states << '\n';
			}
			Remix::WriteDiagnosticLog("", text.str());
		};
		if (asynchronous) Scheduler::Once(save, Scheduler::Pipeline::ASYNC);
		else save();
	}

	static bool CanSynchronizeUiMousePosition()
	{
		return Window::HasFocus() && !LobbyScene::IsTransitionActive()
			&& Game::Key_IsCatcherActive(0, Game::KEYCATCH_UI);
	}

	static void SynchronizeUiMousePosition(const POINT& position)
	{
		// The native CL_MouseEvent routes UI pixels through its normal scaling
		// and focus handling. Zero deltas do not add raw gameplay aim movement.
		Game::CL_MouseEvent(position.x, position.y, 0, 0);
	}

	void rawMouseValue_t::ResetDelta()
	{
		this->previous = this->current;
	}

	int rawMouseValue_t::GetDelta() const
	{
		return this->current - this->previous;
	}

	void rawMouseValue_t::Update(int value, bool absolute)
	{
		if (absolute)
		{
			this->current = 0;
		}

		this->current += value;
	}

	Dvar::Var RawMouse::m_rawinput;
	Dvar::Var RawMouse::m_rawinput_verbose;
	Dvar::Var RawMouse::r_autopriority;

	rawMouseValue_t RawMouse::mouseRawX{ 0, 0 };
	rawMouseValue_t RawMouse::mouseRawY{ 0, 0 };
	std::uint32_t RawMouse::mouseRawEvents = 0;

	bool RawMouse::inRawInput = false;
	bool RawMouse::firstRawInputUpdate = true;
	bool RawMouse::firstLegacyInputUpdate = true;
	bool RawMouse::isCursorClipped = false;

	static void ClampMousePos(POINT& point)
	{
		if (!Window::HasFocus() || Window::IsLoadingScreenMovable() || Window::IsDragging())
		{
			return;
		}

		RECT rect;
		if (GetWindowRect(Window::GetWindow(), &rect) != TRUE)
		{
			return;
		}

		bool isClamped = false;

		if (point.x >= rect.left)
		{
			if (point.x >= rect.right)
			{
				point.x = rect.right - 1;
				isClamped = true;
			}
		}
		else
		{
			point.x = rect.left;
			isClamped = true;
		}

		if (point.y >= rect.top)
		{
			if (point.y >= rect.bottom)
			{
				point.y = rect.bottom - 1;
				isClamped = true;
			}
		}
		else
		{
			point.y = rect.top;
			isClamped = true;
		}

		if (isClamped)
		{
			if (startupDiagnostics.IsActive()) startupDiagnostics.Warp(GetTickCount64());
			SetCursorPos(point.x, point.y);
		}
	}

	void RawMouse::IN_ClampMouseMove()
	{
		POINT point;
		GetCursorPos(&point);
		ClampMousePos(point);
	}

	static bool CheckButtonFlag(DWORD flags, DWORD mask)
	{
		return (flags & mask) != 0u;
	}

	void RawMouse::ResetMouseRawEvents()
	{
		mouseRawEvents = 0u;
		mouseRawX.ResetDelta();
		mouseRawY.ResetDelta();
		firstRawInputUpdate = true;
		firstLegacyInputUpdate = true;
	}

	void RawMouse::SuspendMouseInput()
	{
		ToggleRawInput(false);
		ResetMouseRawEvents();
		ReleaseMouseCursor();
	}

	void RawMouse::ReleaseMouseCursor()
	{
		if (!isCursorClipped)
		{
			return;
		}

		ClipCursor(nullptr);
		isCursorClipped = false;
	}

	void RawMouse::ProcessMouseRawEvent(DWORD usButtonFlags, DWORD flagDown, DWORD mouseEvent)
	{
		const std::uint32_t previous = mouseRawEvents;

		if (CheckButtonFlag(usButtonFlags, flagDown))
		{
			if (m_rawinput_verbose.Get<bool>())
			{
				if ((previous & mouseEvent) != 0u)
				{
					Logger::Debug("Pressing button that wasn't released");
				}

				Logger::Debug("Mouse button down: [{}, {}]", mouseEvent, previous);
			}

			mouseRawEvents |= mouseEvent;
		}

		if (CheckButtonFlag(usButtonFlags, flagDown << 1u))
		{
			if ((previous & mouseEvent) == 0u)
			{
				if (m_rawinput_verbose.Get<bool>())
				{
					Logger::Debug("!! Releasing button that wasn't pressed");
				}

				return;
			}

			if (m_rawinput_verbose.Get<bool>())
			{
				Logger::Debug("Mouse button up: [{}, {}]", mouseEvent, previous);
			}

			mouseRawEvents &= ~mouseEvent;
		}
	}

	bool RawMouse::GetRawInput(LPARAM lParam, RAWINPUT& raw, UINT& dwSize)
	{
		const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &dwSize, sizeof(RAWINPUTHEADER));

		if (result == static_cast<UINT>(-1) || raw.header.dwType != RIM_TYPEMOUSE)
		{
			return false;
		}

		return true;
	}

	LRESULT RawMouse::OnRawInput(LPARAM lParam, WPARAM wParam)
	{
		if (!inRawInput || !Window::HasFocus() || GET_RAWINPUT_CODE_WPARAM(wParam) != RIM_INPUT || LobbyScene::IsTransitionActive())
		{
			ResetMouseRawEvents();
			return DefWindowProcA(Window::GetWindow(), WM_INPUT, wParam, lParam);
		}

		UINT size = sizeof(RAWINPUT);
		static RAWINPUT raw;

		if (!GetRawInput(lParam, raw, size))
		{
			return DefWindowProcA(Window::GetWindow(), WM_INPUT, wParam, lParam);
		}
		if (startupDiagnostics.IsActive())
		{
			// DWORD subtraction also handles the Windows message clock wrapping.
			const DWORD age = GetTickCount() - static_cast<DWORD>(GetMessageTime());
			startupDiagnostics.RawMessage(GetTickCount64(), age);
		}

		const bool isAbsolute = (raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0u;

		mouseRawX.Update(raw.data.mouse.lLastX, isAbsolute);
		mouseRawY.Update(raw.data.mouse.lLastY, isAbsolute);

		if (firstRawInputUpdate)
		{
			mouseRawX.ResetDelta();
			mouseRawY.ResetDelta();
			firstRawInputUpdate = false;
		}

		ProcessMouseRawEvent(raw.data.mouse.usButtonFlags, RI_MOUSE_BUTTON_1_DOWN, 1);
		ProcessMouseRawEvent(raw.data.mouse.usButtonFlags, RI_MOUSE_BUTTON_2_DOWN, 2);
		ProcessMouseRawEvent(raw.data.mouse.usButtonFlags, RI_MOUSE_BUTTON_3_DOWN, 4);
		ProcessMouseRawEvent(raw.data.mouse.usButtonFlags, RI_MOUSE_BUTTON_4_DOWN, 8);
		ProcessMouseRawEvent(raw.data.mouse.usButtonFlags, RI_MOUSE_BUTTON_5_DOWN, 16);

		Utils::ForwardMouseButtonState(mouseRawEvents, []
		{
			if (!Utils::HasRawMouseButtonTransition(raw.data.mouse.usButtonFlags) || !CanSynchronizeUiMousePosition()) return;
			POINT position{};
			if (GetCursorPos(&position) && ScreenToClient(Window::GetWindow(), &position))
				SynchronizeUiMousePosition(position);
		}, [](const std::uint32_t buttons)
		{
			Game::IN_MouseEvent(static_cast<int>(buttons));
		});

		if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
		{
			const SHORT delta = static_cast<SHORT>(raw.data.mouse.usButtonData);

			if (delta > 0)
			{
				Game::Sys_QueEvent(Game::g_wv->sysMsgTime, 1, mw_down, TRUE, 0, nullptr);
				Game::Sys_QueEvent(Game::g_wv->sysMsgTime, 1, mw_down, FALSE, 0, nullptr);
			}

			if (delta < 0)
			{
				Game::Sys_QueEvent(Game::g_wv->sysMsgTime, 1, mw_up, TRUE, 0, nullptr);
				Game::Sys_QueEvent(Game::g_wv->sysMsgTime, 1, mw_up, FALSE, 0, nullptr);
			}
		}

		return DefWindowProcA(Window::GetWindow(), WM_INPUT, wParam, lParam);
	}

	bool RawMouse::IsMouseInClientBounds()
	{
		return Window::HasFocus() && Window::IsCursorWithin(Window::GetWindow());
	}

	LRESULT RawMouse::OnLegacyMouseEvent(UINT Msg, LPARAM lParam, WPARAM wParam)
	{
		if (!Window::HasFocus() || LobbyScene::IsTransitionActive())
		{
			ResetMouseRawEvents();
			return DefWindowProcA(Window::GetWindow(), Msg, wParam, lParam);
		}

		const auto buttons = Utils::ReadLegacyMouseButtons(Msg, wParam);
		if (!buttons) return DefWindowProcA(Window::GetWindow(), Msg, wParam, lParam);
		const auto mouseEvent = *buttons;

		if (m_rawinput.Get<bool>() && inRawInput)
		{
			if (m_rawinput_verbose.Get<bool>())
			{
				Logger::Debug("Window Mouse Message: [{}, {}]", mouseEvent, mouseRawEvents);
			}

			mouseRawEvents = mouseEvent;
		}

		// RDP/injected input may provide only the legacy press/release pair,
		// even with raw input enabled. Always forward its final zero state.
		// IN_MouseEvent already consumes state masks and deduplicates unchanged
		// raw/legacy states; do not synthesize a second UI key/action here.
		Utils::ForwardMouseButtonState(mouseEvent, [lParam]
		{
			if (CanSynchronizeUiMousePosition()) SynchronizeUiMousePosition(Utils::ReadLegacyMousePosition(lParam));
		}, [](const std::uint32_t buttons)
		{
			Game::IN_MouseEvent(static_cast<int>(buttons));
		});

		return DefWindowProcA(Window::GetWindow(), Msg, wParam, lParam);
	}

	LRESULT RawMouse::OnKillFocus([[maybe_unused]] LPARAM lParam, WPARAM)
	{
		SuspendMouseInput();

		Game::Key_ClearStates(0);

		Game::IN_MouseEvent(0);

		if (r_autopriority.Get<bool>())
		{
			SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		}

		return DefWindowProc(Window::GetWindow(), WM_KILLFOCUS, 0, 0);
	}

	LRESULT RawMouse::OnSetFocus([[maybe_unused]] LPARAM lParam, WPARAM)
	{
		ResetMouseRawEvents();

		if (Window::HasFocus() && r_autopriority.Get<bool>())
		{
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		}

		return DefWindowProc(Window::GetWindow(), WM_SETFOCUS, 0, 0);
	}

	void RawMouse::IN_RawMouseMove()
	{
		if (!Window::HasFocus() || LobbyScene::IsTransitionActive())
		{
			SuspendMouseInput();
			return;
		}

		const auto dx = mouseRawX.GetDelta();
		const auto dy = mouseRawY.GetDelta();

		mouseRawX.ResetDelta();
		mouseRawY.ResetDelta();

		POINT point;
		GetCursorPos(&point);
		Game::s_wmv->oldPos = point;
		ScreenToClient(Window::GetWindow(), &point);

		Gamepad::OnMouseMove(point.x, point.y, dx, dy);

		if (!Game::CL_MouseEvent(point.x, point.y, dx, dy))
		{
			ReleaseMouseCursor();
			return;
		}

		RECT rect;
		if (GetWindowRect(Window::GetWindow(), &rect) == TRUE)
		{
			IN_RecenterMouse();
		}
	}

	bool RawMouse::ToggleRawInput(bool enable)
	{
		// raw input has one target window per device in a process: remix's menu reads the mouse through its own input
		// sink window, and registering (or removing) the mouse here takes that away so its menu never sees a click.
		// the game falls back to the legacy cursor mouse, which the sink leaves alone
		if (Remix::Remix::IsActive())
		{
			return false;
		}

		enable = enable && Window::HasFocus() && !Window::IsLoadingScreenMovable() && !Window::IsDragging();

		if (!m_rawinput.Get<bool>())
		{
			if (!inRawInput)
			{
				return false;
			}

			enable = false;
		}
		else
		{
			if (inRawInput == enable)
			{
				return inRawInput;
			}
		}

		const DWORD flags = RIDEV_NOLEGACY;

		RAWINPUTDEVICE rid[1];
		rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;

		if (enable)
		{
			rid[0].dwFlags = flags;
			rid[0].hwndTarget = Window::GetWindow();
		}
		else
		{
			rid[0].dwFlags = RIDEV_REMOVE;
			rid[0].hwndTarget = NULL;
		}

		const bool isRegistered = RegisterRawInputDevices(rid, ARRAYSIZE(rid), sizeof(rid[0])) == TRUE;
		const DWORD registrationError = isRegistered ? ERROR_SUCCESS : GetLastError();
		if (startupDiagnostics.IsActive()) startupDiagnostics.Registration(GetTickCount64(), isRegistered);

		if (!isRegistered)
		{
			Logger::Warning("RawInputDevices: failed: {}\n", registrationError);
		}
		else
		{
			inRawInput = (rid[0].dwFlags & RIDEV_REMOVE) == 0u;

			if (m_rawinput_verbose.Get<bool>())
			{
				if (inRawInput)
				{
					Logger::Debug("Raw Input enabled");
				}
				else
				{
					Logger::Debug("Raw Input disabled");
				}
			}

			ResetMouseRawEvents();
		}

		return true;
	}

	void RawMouse::IN_RawMouse_Init()
	{
		if (Window::GetWindow() && ToggleRawInput(true))
		{
			Logger::Debug("Raw Mouse Init");
		}
	}

	void RawMouse::IN_Init()
	{
		startupDiagnostics.Start(GetTickCount64());
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(IN_Init_Engine))();
		IN_RawMouse_Init();
		ResetMouseRawEvents();

		r_autopriority = Dvar::Var("r_autopriority");
	}

	void RawMouse::IN_Frame()
	{
		if (startupDiagnostics.IsActive())
		{
			using Diagnostics = Utils::MouseStartupDiagnostics;
			const auto now = GetTickCount64();
			const unsigned int state = (inRawInput ? Diagnostics::rawInput : 0u)
				| (Window::HasFocus() ? Diagnostics::focused : 0u)
				| (Game::Key_IsCatcherActive(0, Game::KEYCATCH_UI) ? Diagnostics::ui : 0u)
				| (Window::IsLoadingScreenMovable() ? Diagnostics::loading : 0u)
				| (Game::s_wmv->mouseActive ? Diagnostics::engineMouseActive : 0u);
			startupDiagnostics.Frame(now, state);
			if (startupDiagnostics.IsFinished(now)) SaveMouseStartupDiagnostics(true);
		}
		if (Window::IsLoadingScreenMovable() || Window::IsDragging())
		{
			SuspendMouseInput();
			Window::PumpLoadingEvents();
			return;
		}

		if (Window::HasFocus())
		{
			ToggleRawInput(IsMouseInClientBounds());
		}
		else
		{
			SuspendMouseInput();
		}

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(IN_Frame_Engine))();
	}

	BOOL RawMouse::IN_ClipCursor()
	{
		if (!Window::HasFocus() || Window::IsLoadingScreenMovable() || Window::IsDragging())
		{
			ReleaseMouseCursor();
			return FALSE;
		}

		RECT rect;
		if (!GetClientRect(Window::GetWindow(), &rect))
		{
			return FALSE;
		}

		ClientToScreen(Window::GetWindow(), reinterpret_cast<POINT*>(&rect.left));
		ClientToScreen(Window::GetWindow(), reinterpret_cast<POINT*>(&rect.right));

		const BOOL isClipped = ClipCursor(&rect);

		if (isClipped)
		{
			isCursorClipped = true;
		}

		return isClipped;
	}

	BOOL RawMouse::IN_RecenterMouse()
	{
		if (!IN_ClipCursor())
		{
			return FALSE;
		}

		if (startupDiagnostics.IsActive()) startupDiagnostics.Warp(GetTickCount64());
		return reinterpret_cast<BOOL(*)()>(Utils::Hook::Rebase(IN_RecenterMouse_Engine))();
	}

	void RawMouse::IN_MouseMove()
	{
		if (!Window::HasFocus() || Window::IsLoadingScreenMovable() || Window::IsDragging() || LobbyScene::IsTransitionActive())
		{
			SuspendMouseInput();
			return;
		}

		if (inRawInput)
		{
			IN_RawMouseMove();
			return;
		}

		POINT current;
		static POINT previous;

		GetCursorPos(&current);

		if (firstLegacyInputUpdate)
		{
			previous = current;
			firstLegacyInputUpdate = false;
		}

		const auto* r_displayMode = *Game::r_displayMode;
		if (r_displayMode && r_displayMode->current.integer == 0)
		{
			ClampMousePos(current);
		}

		const int dx = current.x - previous.x;
		const int dy = current.y - previous.y;
		previous = current;

		ScreenToClient(Window::GetWindow(), &current);

		Gamepad::OnMouseMove(current.x, current.y, dx, dy);

		const auto shouldRecenter = Game::CL_MouseEvent(current.x, current.y, dx, dy);

		if (shouldRecenter && (dx || dy))
		{
			RECT rect;
			if (GetWindowRect(Window::GetWindow(), &rect) == TRUE)
			{
				if (!IN_ClipCursor())
				{
					return;
				}

				const int cx = (rect.right + rect.left) / 2;
				const int cy = (rect.top + rect.bottom) / 2;
				if (startupDiagnostics.IsActive()) startupDiagnostics.Warp(GetTickCount64());
				SetCursorPos(cx, cy);

				previous.x = cx;
				previous.y = cy;
			}
		}
		else if (!shouldRecenter)
		{
			ReleaseMouseCursor();
		}
	}

	LRESULT RawMouse::OnLBDown(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_LBUTTONDOWN, lParam, wParam);
	}

	LRESULT RawMouse::OnLBUp(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_LBUTTONUP, lParam, wParam);
	}

	LRESULT RawMouse::OnRBDown(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_RBUTTONDOWN, lParam, wParam);
	}

	LRESULT RawMouse::OnRBUp(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_RBUTTONUP, lParam, wParam);
	}

	LRESULT RawMouse::OnMBDown(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_MBUTTONDOWN, lParam, wParam);
	}

	LRESULT RawMouse::OnMBUp(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_MBUTTONUP, lParam, wParam);
	}

	LRESULT RawMouse::OnXBDown(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_XBUTTONDOWN, lParam, wParam);
	}

	LRESULT RawMouse::OnXBUp(LPARAM lParam, WPARAM wParam)
	{
		return OnLegacyMouseEvent(WM_XBUTTONUP, lParam, wParam);
	}

	RawMouse::RawMouse()
	{
		bool isExpected = Utils::Hook::BranchesTo(Sys_Init_INInitCall, IN_Init_Engine, false)
			&& Utils::Hook::BranchesTo(MainWndProc_RecenterCall, IN_RecenterMouse_Engine, false);

		for (const auto call : IN_FrameCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, IN_Frame_Engine, false);
		}

		if (!isExpected)
		{
			Logger::Error("rawmouse: the input code does not read as expected, no raw input\n");
			return;
		}

		std::size_t hookCount = 0;
		bool isSeated = hooks[hookCount++].Initialize(Sys_Init_INInitCall, reinterpret_cast<void*>(IN_Init), HOOK_CALL)->Install()->IsInstalled();

		for (const auto call : IN_FrameCalls)
		{
			isSeated = hooks[hookCount++].Initialize(call, reinterpret_cast<void*>(IN_Frame), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[hookCount++].Initialize(MainWndProc_RecenterCall, reinterpret_cast<void*>(IN_RecenterMouse), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		assert(hookCount == std::size(hooks));

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("rawmouse: could not seat every hook, no raw input\n");
			return;
		}

		Events::OnDvarInit([]
		{
			m_rawinput = Dvar::Register("m_rawinput", true, Game::DVAR_ARCHIVE, "Use raw mouse input");
			m_rawinput_verbose = Dvar::Register("m_rawinput_verbose", false, Game::DVAR_ARCHIVE, "Show raw mouse input log");
		});

		Window::OnWndMessage(WM_KILLFOCUS, OnKillFocus);
		Window::OnWndMessage(WM_SETFOCUS, OnSetFocus);

		Window::OnWndMessage(WM_LBUTTONDOWN, OnLBDown);
		Window::OnWndMessage(WM_LBUTTONUP, OnLBUp);
		Window::OnWndMessage(WM_RBUTTONDOWN, OnRBDown);
		Window::OnWndMessage(WM_RBUTTONUP, OnRBUp);
		Window::OnWndMessage(WM_MBUTTONDOWN, OnMBDown);
		Window::OnWndMessage(WM_MBUTTONUP, OnMBUp);
		Window::OnWndMessage(WM_XBUTTONDOWN, OnXBDown);
		Window::OnWndMessage(WM_XBUTTONUP, OnXBUp);

		Window::OnWndMessage(WM_INPUT, OnRawInput);
		Window::OnCreate(IN_RawMouse_Init);
		Scheduler::OnShutdown([] { SaveMouseStartupDiagnostics(false); });
	}
}
