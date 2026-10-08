#include "STDInclude.hpp"

#include <hidusage.h>

#include "Window.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "LobbyScene.hpp"
#include "Logger.hpp"
#include "RawMouse.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"

namespace Components
{
	Dvar::Var Window::ui_nativeCursor;

	HWND Window::mainWindow = nullptr;
	WNDPROC Window::originalWindowProc = nullptr;
	BOOL Window::cursorVisible = TRUE;
	std::unordered_map<UINT, std::function<Window::WndProcCallback>> Window::wndMessageCallbacks;
	std::vector<std::function<Window::CreateCallback>> Window::createSignals;
	std::vector<std::function<Window::DeviceChangeCallback>> Window::deviceChangeSignals;

	constexpr std::uintptr_t R_InitGraphicsApi_CreateWindowCall = 0x140032873;
	static const std::uint8_t createWindowCall[] = { 0xFF, 0x15, 0xB7, 0xFD, 0x32, 0x00 };

	constexpr std::uintptr_t UI_RefreshViewport_DrawCursorCall = 0x140270E9A;
	constexpr std::uintptr_t UI_DrawHandlePic = 0x140250F40;

	constexpr std::uintptr_t ShowCursorImport = 0x1403626E8;
	constexpr WORD windowIconResource = 1;

	constexpr std::uintptr_t MainWndProc = 0x1402AA9A0;
	static const Utils::Hook::LeaSite wndProcLea = { 0x1402A5E93, { 0x48, 0x8D, 0x05 }, MainWndProc };

	constexpr int displayModeFullscreen = 0;
	constexpr int displayModeNoBorder = 1;
	constexpr int displayModeWindowed = 2;

	static Dvar::Var r_fullscreen;
	static Dvar::Var r_noborder;
	static bool hasMirroredDisplayMode = false;
	static bool mirroredFullscreen = false;
	static bool mirroredNoBorder = false;

	static Utils::Hook createWindowHook;
	static Utils::Hook drawCursorHook;

	static bool isDragActive = false;
	static POINT dragOffset{};
	static DWORD windowThreadId = 0;

	static bool ActivateMainWindow()
	{
		const auto window = Window::GetWindow();

		if (!window || !IsWindow(window))
		{
			return false;
		}

		ShowWindow(window, SW_SHOWNORMAL);

		const auto foreground = GetForegroundWindow();
		const auto currentThread = GetCurrentThreadId();
		DWORD foregroundThread = 0;

		if (foreground)
		{
			foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
		}

		const bool isAttached = foregroundThread && foregroundThread != currentThread && AttachThreadInput(currentThread, foregroundThread, TRUE);

		SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
		BringWindowToTop(window);
		SetForegroundWindow(window);
		SetActiveWindow(window);
		SetFocus(window);

		if (isAttached)
		{
			AttachThreadInput(currentThread, foregroundThread, FALSE);
		}

		return Window::HasFocus();
	}

	static void BeginWindowDrag(HWND window)
	{
		RECT rect{};
		POINT cursor{};

		if (!GetWindowRect(window, &rect) || !GetCursorPos(&cursor))
		{
			return;
		}

		RawMouse::SuspendMouseInput();
		dragOffset = { cursor.x - rect.left, cursor.y - rect.top };
		SetCapture(window);

		isDragActive = GetCapture() == window;
	}

	static void UpdateWindowDrag(HWND window)
	{
		POINT cursor{};

		if (!isDragActive || !GetCursorPos(&cursor))
		{
			return;
		}

		SetWindowPos(window, nullptr, cursor.x - dragOffset.x, cursor.y - dragOffset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}

	static void EndWindowDrag()
	{
		if (!isDragActive)
		{
			return;
		}

		isDragActive = false;

		if (GetCapture() == Window::GetWindow())
		{
			ReleaseCapture();
		}
	}

	int Window::Width()
	{
		return Width(mainWindow);
	}

	int Window::Height()
	{
		return Height(mainWindow);
	}

	int Window::Width(HWND window)
	{
		RECT rect;
		Dimension(window, &rect);
		return (rect.right - rect.left);
	}

	int Window::Height(HWND window)
	{
		RECT rect;
		Dimension(window, &rect);
		return (rect.bottom - rect.top);
	}

	void Window::Dimension(RECT* rect)
	{
		Dimension(mainWindow, rect);
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
		if (!window || !IsWindowVisible(window) || IsIconic(window))
		{
			return false;
		}

		RECT rect{};
		POINT point{};

		return GetClientRect(window, &rect) && GetCursorPos(&point) && ScreenToClient(window, &point) && PtInRect(&rect, point);
	}

	HWND Window::GetWindow()
	{
		return mainWindow;
	}

	bool Window::HasFocus()
	{
		return mainWindow && IsWindowVisible(mainWindow) && !IsIconic(mainWindow) && GetForegroundWindow() == mainWindow;
	}

	void Window::ApplyDisplayModeDvars()
	{
		auto* const r_displayMode = *Game::r_displayMode;

		if (!r_displayMode || !r_fullscreen.IsValid() || !r_noborder.IsValid())
		{
			return;
		}

		const bool isFullscreen = r_fullscreen.Get<bool>();
		const bool isNoBorder = r_noborder.Get<bool>();
		const bool hasMenuChanged = hasMirroredDisplayMode && (isFullscreen != mirroredFullscreen || isNoBorder != mirroredNoBorder);

		if (hasMenuChanged)
		{
			int displayMode = displayModeWindowed;

			if (isFullscreen)
			{
				displayMode = displayModeFullscreen;
			}
			else if (isNoBorder)
			{
				displayMode = displayModeNoBorder;
			}

			Game::Dvar_SetInt(r_displayMode, displayMode);
		}

		const int displayMode = r_displayMode->current.integer;

		mirroredFullscreen = displayMode == displayModeFullscreen;
		mirroredNoBorder = displayMode == displayModeNoBorder || (displayMode == displayModeFullscreen && isNoBorder);
		hasMirroredDisplayMode = true;

		r_fullscreen.Set(mirroredFullscreen);
		r_noborder.Set(mirroredNoBorder);
	}

	bool Window::IsLoadingScreenMovable()
	{
		const auto* const r_displayMode = *Game::r_displayMode;

		if (!mainWindow || !r_displayMode || r_displayMode->current.integer != displayModeWindowed)
		{
			return false;
		}

		return !FastFiles::MainMenuReady() || !FastFiles::Ready() || Renderer::IsDeviceRecoveryActive();
	}

	bool Window::IsDragging()
	{
		return isDragActive;
	}

	static bool IsFrontendMovable()
	{
		const auto* mode = *Game::r_displayMode;
		return Window::IsLoadingScreenMovable() || (mode && mode->current.integer == displayModeWindowed
			&& LobbyScene::IsSceneReady() && !Game::CL_IsCgameInitialized(0)
			&& Game::CL_GetLocalClientConnectionState(0) < Game::CA_CONNECTING);
	}

	void Window::PumpLoadingEvents()
	{
		thread_local std::uint32_t callCount = 0;
		static bool isPumping = false;
		static ULONGLONG lastPump = 0;

		if ((++callCount & 0x1F) != 0 && !isDragActive)
		{
			return;
		}

		if (!windowThreadId || GetCurrentThreadId() != windowThreadId)
		{
			return;
		}

		const auto now = GetTickCount64();

		if (isPumping || now - lastPump < 8 || (!IsLoadingScreenMovable() && !IsDragging()))
		{
			return;
		}

		lastPump = now;
		isPumping = true;

		RawMouse::SuspendMouseInput();

		MSG message{};

		const auto dispatch = [&message](UINT first, UINT last)
		{
			for (int count = 0; count < 32 && PeekMessageA(&message, mainWindow, first, last, PM_REMOVE); ++count)
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

		isPumping = false;
	}

	LRESULT CALLBACK Window::NativeWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		if (isDragActive && !IsFrontendMovable())
		{
			EndWindowDrag();
		}

		if (Msg == WM_MOUSEACTIVATE && IsLoadingScreenMovable())
		{
			return MA_ACTIVATE;
		}

		if (Msg == WM_NCHITTEST && IsLoadingScreenMovable())
		{
			const auto hit = DefWindowProcA(hWnd, Msg, wParam, lParam);

			if (hit == HTCLIENT)
			{
				return HTCAPTION;
			}

			return hit;
		}

		const bool isCaptionPress = Msg == WM_NCLBUTTONDOWN && wParam == HTCAPTION;

		if ((Msg == WM_LBUTTONDOWN && IsLoadingScreenMovable()) || (isCaptionPress && IsFrontendMovable()))
		{
			BeginWindowDrag(hWnd);
			return 0;
		}

		if (isDragActive && (Msg == WM_MOUSEMOVE || Msg == WM_NCMOUSEMOVE))
		{
			UpdateWindowDrag(hWnd);
			return 0;
		}

		if (isDragActive && (Msg == WM_LBUTTONUP || Msg == WM_NCLBUTTONUP || Msg == WM_CAPTURECHANGED || Msg == WM_CANCELMODE))
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

		const auto original = originalWindowProc;

		if (Msg == WM_NCDESTROY && hWnd == mainWindow)
		{
			mainWindow = nullptr;
			windowThreadId = 0;
			originalWindowProc = nullptr;
		}

		if (original)
		{
			return CallWindowProcA(original, hWnd, Msg, wParam, lParam);
		}

		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}

	void Window::OnWndMessage(UINT Msg, const std::function<WndProcCallback>& callback)
	{
		wndMessageCallbacks.emplace(Msg, callback);
	}

	void Window::OnDeviceChange(const std::function<DeviceChangeCallback>& callback)
	{
		deviceChangeSignals.push_back(callback);
	}

	void Window::OnCreate(const std::function<CreateCallback>& callback)
	{
		createSignals.push_back(callback);
	}

	void Window::DrawCursorStub(const Game::ScreenPlacement* scrPlace, float x, float y, float w, float h, int horzAlign, int vertAlign, const float* color, Game::Material* material)
	{
		if (LobbyScene::IsTransitionActive())
		{
			cursorVisible = FALSE;
			return;
		}

		if (ui_nativeCursor.Get<bool>())
		{
			cursorVisible = TRUE;
		}
		else
		{
			Game::UI_DrawHandlePic(scrPlace, x, y, w, h, horzAlign, vertAlign, color, material);
		}
	}

	int WINAPI Window::ShowCursorHook(BOOL show)
	{
		if (LobbyScene::IsTransitionActive())
		{
			static int transitionCount = -1;

			if (show)
			{
				++transitionCount;
			}
			else
			{
				--transitionCount;
			}

			cursorVisible = FALSE;
			return transitionCount;
		}

		if (ui_nativeCursor.Get<bool>() && HasFocus() && IsCursorWithin(mainWindow))
		{
			static int count = 0;

			if (show)
			{
				++count;
			}
			else
			{
				--count;
			}

			if (count >= 0)
			{
				cursorVisible = TRUE;
			}

			return count;
		}

		return ShowCursor(show);
	}

	static void ApplyWindowIcon(HWND window)
	{
		HMODULE module = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

		if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&ApplyWindowIcon), &module))
		{
			return;
		}

		auto* const bigIcon = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(windowIconResource), IMAGE_ICON,
			GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
		auto* const smallIcon = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(windowIconResource), IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

		if (bigIcon)
		{
			SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
			SetClassLongPtrW(window, GCLP_HICON, reinterpret_cast<LONG_PTR>(bigIcon));
		}

		if (smallIcon)
		{
			SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
			SetClassLongPtrW(window, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(smallIcon));
		}
	}

	HWND WINAPI Window::CreateMainWindow(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
	{
		Utils::Hook::Set<void*>(ShowCursorImport, reinterpret_cast<void*>(ShowCursorHook));

		mainWindow = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

		if (mainWindow)
		{
			windowThreadId = GetCurrentThreadId();
			isDragActive = false;
			originalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(mainWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&NativeWindowProc)));

			ApplyWindowIcon(mainWindow);
			ActivateMainWindow();

			const auto activationStart = std::chrono::steady_clock::now();

			Scheduler::Schedule([activationStart]
			{
				const bool isDone = !mainWindow || !IsWindow(mainWindow) || HasFocus() || std::chrono::steady_clock::now() - activationStart >= 5s;

				if (isDone)
				{
					return true;
				}

				ActivateMainWindow();
				return false;
			}, Scheduler::Pipeline::MAIN, 100ms);
		}

		for (const auto& callback : createSignals)
		{
			callback();
		}

		return mainWindow;
	}

	void Window::ApplyCursor()
	{
		if (LobbyScene::IsTransitionActive())
		{
			SetCursor(nullptr);
			return;
		}

		const bool isLoading = !FastFiles::Ready() && !IsLoadingScreenMovable() && !IsDragging();

		if (isLoading)
		{
			SetCursor(LoadCursor(nullptr, IDC_APPSTARTING));
		}
		else
		{
			SetCursor(LoadCursor(nullptr, IDC_ARROW));
		}
	}

	LRESULT CALLBACK Window::MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
	{
		const bool isDeactivated = (Msg == WM_ACTIVATE && (LOWORD(wParam) == WA_INACTIVE || HIWORD(wParam))) || (Msg == WM_ACTIVATEAPP && !wParam);
		const bool isMinimised = Msg == WM_SIZE && wParam == SIZE_MINIMIZED;

		if (isDeactivated || isMinimised)
		{
			RawMouse::SuspendMouseInput();
		}

		if (Msg == WM_INPUT_DEVICE_CHANGE)
		{
			for (const auto& callback : deviceChangeSignals)
			{
				callback(wParam, lParam);
			}
		}

		if (const auto cb = wndMessageCallbacks.find(Msg); cb != wndMessageCallbacks.end())
		{
			return cb->second(lParam, wParam);
		}

		return reinterpret_cast<WNDPROC>(Utils::Hook::Rebase(MainWndProc))(hWnd, Msg, wParam, lParam);
	}

	void Window::EnableDpiAwareness()
	{
		const Utils::Library user32{"user32.dll"};

		user32.InvokePascal<BOOL>("SetProcessDpiAwarenessContext", DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}

	Window::Window()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(R_InitGraphicsApi_CreateWindowCall, createWindowCall, sizeof(createWindowCall))
			&& Utils::Hook::BranchesTo(UI_RefreshViewport_DrawCursorCall, UI_DrawHandlePic, false)
			&& Utils::Hook::IsLeaIntact(wndProcLea);

		if (!isExpected)
		{
			Logger::Error("window: window creation does not read as expected, no window hooks\n");
			return;
		}

		const auto messageHandler = Utils::Hook::Trampoline(Utils::Hook::Rebase(wndProcLea.address), reinterpret_cast<std::uintptr_t>(MessageHandler));
		if (!messageHandler || !Utils::Hook::CanLeaReach(wndProcLea, reinterpret_cast<void*>(messageHandler)))
		{
			Logger::Error("window: no room for the message handler beside the image, no window hooks\n");
			return;
		}

		Utils::Hook::Nop(R_InitGraphicsApi_CreateWindowCall, 1);
		const bool isSeated = createWindowHook.Initialize(R_InitGraphicsApi_CreateWindowCall + 1, reinterpret_cast<void*>(CreateMainWindow), HOOK_CALL)->Install()->IsInstalled()

			&& drawCursorHook.Initialize(UI_RefreshViewport_DrawCursorCall, reinterpret_cast<void*>(DrawCursorStub), HOOK_CALL)->Install()->IsInstalled();

		if (!isSeated)
		{
			createWindowHook.Uninstall();
			drawCursorHook.Uninstall();
			Utils::Hook::Set<std::uint8_t>(R_InitGraphicsApi_CreateWindowCall, createWindowCall[0]);

			Logger::Error("window: could not seat the window hooks\n");
			return;
		}

		Utils::Hook::PointLeaAt(wndProcLea, reinterpret_cast<void*>(messageHandler));

		Events::OnDvarInit([]
		{
			ui_nativeCursor = Dvar::Register("ui_nativeCursor", false, Game::DVAR_ARCHIVE, "Display native cursor");
			r_fullscreen = Dvar::Register("r_fullscreen", false, Game::DVAR_NONE, "Display game full screen");
			r_noborder = Dvar::Register("r_noborder", false, Game::DVAR_NONE, "Do not use a border in windowed mode");
		});

		Scheduler::Loop([]
		{
			if (ui_nativeCursor.Get<bool>() && HasFocus() && IsCursorWithin(mainWindow))
			{
				int value = 0;
				ApplyCursor();

				if (cursorVisible)
				{
					do
					{
						value = ShowCursor(TRUE);
					}
					while (value < 0);

					while (value > 0)
					{
						value = ShowCursor(FALSE);
					}
				}
				else
				{
					do
					{
						value = ShowCursor(FALSE);
					}
					while (value >= 0);

					while (value < -1)
					{
						value = ShowCursor(TRUE);
					}
				}

				cursorVisible = FALSE;
			}
		}, Scheduler::Pipeline::RENDERER);

		OnWndMessage(WM_SETCURSOR, [](LPARAM lParam, WPARAM wParam) -> LRESULT
		{
			if (LobbyScene::IsTransitionActive())
			{
				SetCursor(nullptr);
				return TRUE;
			}

			if (IsLoadingScreenMovable() || IsDragging())
			{
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
				return TRUE;
			}

			if (!HasFocus() || !IsCursorWithin(mainWindow))
			{
				return DefWindowProcA(mainWindow, WM_SETCURSOR, wParam, lParam);
			}

			ApplyCursor();
			return TRUE;
		});

		OnCreate([]
		{
			RAWINPUTDEVICE rid{};
			rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
			rid.usUsage = HID_USAGE_GENERIC_GAMEPAD;
			rid.dwFlags = RIDEV_DEVNOTIFY;
			rid.hwndTarget = mainWindow;

			if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
			{
				rid.usUsage = 0x00;
				RegisterRawInputDevices(&rid, 1, sizeof(rid));
			}
		});

		EnableDpiAwareness();
	}
}
