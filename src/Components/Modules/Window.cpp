
#include "FastFiles.hpp"
#include "RawMouse.hpp"
#include "Renderer.hpp"
#include "Window.hpp"

namespace Components
{
	Dvar::Var Window::NoBorder;
	Dvar::Var Window::NativeCursor;

	HWND Window::MainWindow = nullptr;
	WNDPROC Window::OriginalWindowProc = nullptr;
	BOOL Window::CursorVisible = TRUE;
	std::unordered_map<UINT, Utils::Slot<Window::WndProcCallback>> Window::WndMessageCallbacks;
	Utils::Signal<Window::CreateCallback> Window::CreateSignals;
	Utils::Signal<Window::DeviceChangeCallback> Window::DeviceChangeSignals;
	namespace
	{
		bool WindowDragActive = false;
		POINT WindowDragOffset{};
		DWORD WindowThreadId = 0;

		bool ActivateMainWindow()
		{
			const auto window = Window::GetWindow();
			if (!window || !IsWindow(window)) return false;

			ShowWindow(window, SW_SHOWNORMAL);

			const auto foreground = GetForegroundWindow();
			const auto currentThread = GetCurrentThreadId();
			const auto foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
			const bool attached = foregroundThread && foregroundThread != currentThread
				&& AttachThreadInput(currentThread, foregroundThread, TRUE);

			SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			BringWindowToTop(window);
			SetForegroundWindow(window);
			SetActiveWindow(window);
			SetFocus(window);

			if (attached) AttachThreadInput(currentThread, foregroundThread, FALSE);
			return Window::HasFocus();
		}

		void BeginWindowDrag(HWND window)
		{
			RECT rect{};
			POINT cursor{};
			if (!GetWindowRect(window, &rect) || !GetCursorPos(&cursor)) return;

			RawMouse::SuspendMouseInput();
			WindowDragOffset = { cursor.x - rect.left, cursor.y - rect.top };
			SetCapture(window);
			// SetCapture returns the PREVIOUS capture owner, not success/failure.
			WindowDragActive = GetCapture() == window;
		}

		void UpdateWindowDrag(HWND window)
		{
			if (!WindowDragActive) return;

			POINT cursor{};
			if (!GetCursorPos(&cursor)) return;

			SetWindowPos(window, nullptr,
				cursor.x - WindowDragOffset.x,
				cursor.y - WindowDragOffset.y,
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
		}

		void EndWindowDrag()
		{
			if (!WindowDragActive) return;
			WindowDragActive = false;
			if (GetCapture() == Window::GetWindow()) ReleaseCapture();
		}
	}

	bool Window::IsLoadingScreenMovable()
	{
		const auto* fullscreen = Dvar::Var("r_fullscreen").get<Game::dvar_t*>();
		return MainWindow && fullscreen && !fullscreen->current.enabled
			&& (!FastFiles::MainMenuReady() || !FastFiles::Ready() || Renderer::IsDeviceRecoveryActive());
	}

	bool Window::IsDragging()
	{
		return WindowDragActive;
	}

	void Window::PumpLoadingEvents()
	{
		// Asset uploads can occupy the window-owning thread for seconds. Only
		// service mouse/window messages here; never re-enter commands or loading.
		if (!WindowThreadId || GetCurrentThreadId() != WindowThreadId) return;
		static bool pumping = false;
		static ULONGLONG lastPump = 0;
		const auto now = GetTickCount64();
		if (pumping || now - lastPump < 8 || (!IsLoadingScreenMovable() && !IsDragging())) return;
		lastPump = now;
		pumping = true;
		const auto guard = gsl::finally([] { pumping = false; });
		RawMouse::SuspendMouseInput();
		MSG message{};
		const auto dispatch = [&](UINT first, UINT last)
		{
			for (int count = 0; count < 32 && PeekMessageA(&message, MainWindow, first, last, PM_REMOVE); ++count)
			{
				if (message.message == WM_QUIT)
				{
					PostQuitMessage(static_cast<int>(message.wParam));
					break;
				}
				DispatchMessageA(&message);
			}
		};
		dispatch(WM_MOUSEFIRST, WM_MOUSELAST);
		dispatch(WM_NCMOUSEMOVE, WM_NCMBUTTONDBLCLK);
		dispatch(WM_PAINT, WM_PAINT);
	}

	LRESULT CALLBACK Window::NativeWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		if (Msg == WM_MOUSEACTIVATE && IsLoadingScreenMovable())
		{
			return MA_ACTIVATE;
		}

		if (Msg == WM_NCHITTEST && IsLoadingScreenMovable())
		{
			const auto hit = DefWindowProcA(hWnd, Msg, wParam, lParam);
			// Preserve caption buttons, borders and off-window hit tests.
			return hit == HTCLIENT ? HTCAPTION : hit;
		}

		if ((Msg == WM_LBUTTONDOWN || (Msg == WM_NCLBUTTONDOWN && wParam == HTCAPTION)) && IsLoadingScreenMovable())
		{
			BeginWindowDrag(hWnd);
			return 0;
		}

		if (WindowDragActive && (Msg == WM_MOUSEMOVE || Msg == WM_NCMOUSEMOVE))
		{
			UpdateWindowDrag(hWnd);
			return 0;
		}

		if (WindowDragActive && (Msg == WM_LBUTTONUP || Msg == WM_NCLBUTTONUP
			|| Msg == WM_CAPTURECHANGED || Msg == WM_CANCELMODE))
		{
			EndWindowDrag();
			return 0;
		}

		if (Msg == WM_SETCURSOR && (IsLoadingScreenMovable() || IsDragging()))
		{
			SetCursor(LoadCursor(nullptr, IDC_ARROW));
			return TRUE;
		}

		if (Msg == WM_CANCELMODE || Msg == WM_KILLFOCUS || Msg == WM_NCDESTROY)
		{
			EndWindowDrag();
		}

		const auto original = OriginalWindowProc;
		if (Msg == WM_NCDESTROY && hWnd == MainWindow)
		{
			MainWindow = nullptr;
			WindowThreadId = 0;
			OriginalWindowProc = nullptr;
		}
		if (original)
		{
			return CallWindowProcA(original, hWnd, Msg, wParam, lParam);
		}

		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}

	int Window::Width()
	{
		return Window::Width(Window::MainWindow);
	}

	int Window::Height()
	{
		return Window::Height(Window::MainWindow);
	}

	int Window::Width(HWND window)
	{
		RECT rect;
		Window::Dimension(window, &rect);
		return (rect.right - rect.left);
	}

	int Window::Height(HWND window)
	{
		RECT rect;
		Window::Dimension(window, &rect);
		return (rect.bottom - rect.top);
	}

	void Window::Dimension(RECT* rect)
	{
		Window::Dimension(Window::MainWindow, rect);
	}

	void Window::Dimension(HWND window, RECT* rect)
	{
		if (rect)
		{
			ZeroMemory(rect, sizeof(RECT));

			if (window && IsWindow(window))
			{
				GetWindowRect(window, rect);
			}
		}
	}

	bool Window::IsCursorWithin(HWND window)
	{
		if (!window || !IsWindowVisible(window) || IsIconic(window)) return false;
		RECT rect{};
		POINT point{};
		return GetClientRect(window, &rect) && GetCursorPos(&point)
			&& ScreenToClient(window, &point) && PtInRect(&rect, point);
	}

	HWND Window::GetWindow()
	{
		return Window::MainWindow;
	}

	bool Window::HasFocus()
	{
		return MainWindow && IsWindowVisible(MainWindow) && !IsIconic(MainWindow)
			&& GetForegroundWindow() == MainWindow;
	}

	void Window::OnWndMessage(UINT Msg, Utils::Slot<Window::WndProcCallback> callback)
	{
		WndMessageCallbacks.emplace(Msg, callback);
	}

	void Window::OnDeviceChange(Utils::Slot<Window::DeviceChangeCallback> callback)
	{
		DeviceChangeSignals.connect(callback);
	}

	void Window::OnCreate(Utils::Slot<CreateCallback> callback)
	{
		CreateSignals.connect(callback);
	}

	int Window::IsNoBorder()
	{
		return Window::NoBorder.get<bool>();
	}

	__declspec(naked) void Window::StyleHookStub()
	{
		__asm
		{
			call Window::IsNoBorder
			test al, al
			jz setBorder

			mov ebp, WS_VISIBLE | WS_POPUP
			retn

		setBorder:
			mov ebp, WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX
			retn
		}
	}

	void Window::DrawCursorStub(Game::ScreenPlacement* scrPlace, float x, float y, float w, float h, int horzAlign, int vertAlign, const float* color, Game::Material* material)
	{
		if (Window::NativeCursor.get<bool>())
		{
			Window::CursorVisible = TRUE;
		}
		else
		{
			Game::UI_DrawHandlePic(scrPlace, x, y, w, h, horzAlign, vertAlign, color, material);
		}
	}

	int WINAPI Window::ShowCursorHook(BOOL show)
	{
		if (Window::NativeCursor.get<bool>() && Window::HasFocus() && Window::IsCursorWithin(Window::MainWindow))
		{
			static int count = 0;
			(show ? ++count : --count);

			if (count >= 0)
			{
				Window::CursorVisible = TRUE;
			}

			return count;
		}

		return ShowCursor(show);
	}

	HWND WINAPI Window::CreateMainWindow(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
	{
		Window::MainWindow = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
		if (Window::MainWindow)
		{
			WindowThreadId = GetCurrentThreadId();
			WindowDragActive = false;
			Window::OriginalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
				Window::MainWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::NativeWindowProc)));

			// The launcher can still own the foreground window when the game
			// creates its HWND.  Activate it here and once again after the first
			// frame; the deferred retry covers the normal Windows foreground-lock
			// race without stealing focus later during gameplay.
			ActivateMainWindow();
			const auto activationStart = std::chrono::steady_clock::now();
			Scheduler::Schedule([activationStart]
			{
				if (!Window::MainWindow || !IsWindow(Window::MainWindow)
					|| Window::HasFocus()
					|| std::chrono::steady_clock::now() - activationStart >= 5s)
				{
					return true;
				}

				ActivateMainWindow();
				return false;
			}, Scheduler::Pipeline::MAIN, 100ms);
		}
		CreateSignals();

		return Window::MainWindow;
	}

	void Window::ApplyCursor()
	{
		bool isLoading = !FastFiles::Ready() && !IsLoadingScreenMovable() && !IsDragging();
		SetCursor(LoadCursor(nullptr, isLoading ? IDC_APPSTARTING : IDC_ARROW));
	}

	BOOL WINAPI Window::MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		if ((Msg == WM_ACTIVATE && (LOWORD(wParam) == WA_INACTIVE || HIWORD(wParam)))
			|| (Msg == WM_ACTIVATEAPP && !wParam)
			|| (Msg == WM_SIZE && wParam == SIZE_MINIMIZED))
		{
			RawMouse::SuspendMouseInput();
		}

		// Handle raw input device change events.
		//
		// Note that we delegate handling to DeviceChangeSignals(), which interprets
		// the event and performs any necessary updates to the gamepad state.
		//
		if (Msg == WM_INPUT_DEVICE_CHANGE)
		{
			DeviceChangeSignals(wParam, lParam);
		}

		if (const auto cb = WndMessageCallbacks.find(Msg); cb != WndMessageCallbacks.end())
		{
			return cb->second(lParam, wParam);
		}

		return Utils::Hook::Call<BOOL(__stdcall)(HWND, UINT, WPARAM, LPARAM)>(0x4731F0)(hWnd, Msg, wParam, lParam);
	}

	void Window::EnableDpiAwareness()
	{
		const Utils::Library user32{"user32.dll"};

		user32.invokePascal<void>("SetProcessDpiAwarenessContext", DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}

	Window::Window()
	{
		// Borderless window
		Window::NoBorder = Dvar::Register<bool>("r_noborder", true, Game::DVAR_ARCHIVE, "Do not use a border in windowed mode");
		Window::NativeCursor = Dvar::Register<bool>("ui_nativeCursor", false, Game::DVAR_ARCHIVE, "Display native cursor");

		Utils::Hook(0x507643, Window::StyleHookStub, HOOK_CALL).install()->quick();

		// Main window creation
		Utils::Hook::Nop(0x5076AA, 1);
		Utils::Hook(0x5076AB, Window::CreateMainWindow, HOOK_CALL).install()->quick();

		// Mark the cursor as visible
		Utils::Hook(0x48E5D3, Window::DrawCursorStub, HOOK_CALL).install()->quick();

		// Draw the cursor if necessary
		Scheduler::Loop([]
		{
			if (Window::NativeCursor.get<bool>() && Window::HasFocus() && Window::IsCursorWithin(Window::MainWindow))
			{
				int value = 0;
				Window::ApplyCursor();

				if (Window::CursorVisible)
				{
					while ((value = ShowCursor(TRUE)) < 0) {};
					while (value > 0) { value = ShowCursor(FALSE); } // Set display counter to 0
				}
				else
				{
					while ((value = ShowCursor(FALSE)) >= 0) {};
					while (value < -1) { value = ShowCursor(TRUE); } // Set display counter to -1
				}

				Window::CursorVisible = FALSE;
			}
		}, Scheduler::Pipeline::RENDERER);

		// Don't let the game interact with the native cursor
		Utils::Hook::Set(0x6D7348, Window::ShowCursorHook);

		// Use custom message handler
		Utils::Hook::Set(0x64D298, Window::MessageHandler);

		Window::OnWndMessage(WM_SETCURSOR, [](WPARAM lParam, LPARAM wParam)
		{
			if (IsLoadingScreenMovable() || IsDragging())
			{
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
				return TRUE;
			}

			if (!Window::HasFocus() || !Window::IsCursorWithin(Window::MainWindow))
			{
				return static_cast<BOOL>(DefWindowProcA(Window::MainWindow, WM_SETCURSOR, wParam, lParam));
			}
			Window::ApplyCursor();
			return TRUE;
		});

		// Register for raw input device notifications when the window is created.
		//
		// This allows the system to notify us when a gamepad is connected or
		// disconnected, without requiring explicit polling. We request
		// notifications specifically for gamepad-class HID devices.
		//
		Window::OnCreate([]()
			{
				RAWINPUTDEVICE rid{};
				rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
				rid.usUsage = HID_USAGE_GENERIC_GAMEPAD;
				rid.dwFlags = RIDEV_DEVNOTIFY;
				rid.hwndTarget = Window::MainWindow;

				if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
				{
					// Some systems may reject usage-specific registration. In that case,
					// fall back to receiving notifications for all devices within the
					// generic desktop page. We lose precision but still receive updates.
					//
					rid.usUsage = 0x00;
					RegisterRawInputDevices(&rid, 1, sizeof(rid));
				}
			});

		Window::EnableDpiAwareness();
	}
}
