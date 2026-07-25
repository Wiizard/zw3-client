namespace StartupSplash
{
	constexpr auto* WindowClassName = "ZW3StartupSplash";
	constexpr UINT_PTR AnimationTimerId = 1;
	constexpr UINT_PTR DotAnimationTimerId = 2;
	constexpr auto AnimationTimerInterval = 35;
	constexpr auto DotAnimationInterval = 700;
	constexpr LONG MinWindowWidth = 760;
	constexpr LONG DefaultImageHeight = 380;
	constexpr LONG FooterHeight = 150;
	constexpr int CancelButtonWidth = 118;
	constexpr int CancelButtonHeight = 28;

	std::atomic<HWND> Window = nullptr;
	std::atomic_bool Active = false;
	std::atomic_bool Determinate = false;
	std::atomic<float> Progress = 0.0f;
	std::mutex StatusMutex;
	std::string StatusText = "Launching game, please wait";

	HBITMAP SplashBitmap = nullptr;
	HFONT TitleFont = nullptr;
	HFONT StatusFont = nullptr;
	HFONT PercentageFont = nullptr;
	HFONT ButtonFont = nullptr;
	HBRUSH BackgroundBrush = nullptr;
	LONG ImageWidth = MinWindowWidth;
	LONG ImageHeight = DefaultImageHeight;
	LONG WindowWidth = MinWindowWidth;
	LONG WindowHeight = DefaultImageHeight + FooterHeight;
	int DotCount = 0;
	int ProgressOffset = 0;

	HBITMAP LoadSplashBitmap()
	{
		auto* bitmap = static_cast<HBITMAP>(LoadImageA(nullptr, "zw3/data/images/splash.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
		if (!bitmap)
		{
			bitmap = static_cast<HBITMAP>(LoadImageA(nullptr, "iw4x/images/splash.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
		}

		return bitmap;
	}

	std::string GetStatusText()
	{
		std::lock_guard lock(StatusMutex);
		auto text = StatusText;

		if (!Determinate.load(std::memory_order_relaxed))
		{
			text.append(static_cast<std::size_t>(DotCount), '.');
		}

		return text;
	}

	void SetStatus(const std::string_view status)
	{
		{
			std::lock_guard lock(StatusMutex);
			StatusText.assign(status);
		}

		if (const auto window = Window.load(std::memory_order_acquire); window && IsWindow(window))
		{
			InvalidateRect(window, nullptr, FALSE);
		}
	}

	void SetProgress(const float progress)
	{
		Progress.store(std::clamp(progress, 0.0f, 1.0f), std::memory_order_release);
		Determinate.store(true, std::memory_order_release);

		if (const auto window = Window.load(std::memory_order_acquire); window && IsWindow(window))
		{
			InvalidateRect(window, nullptr, FALSE);
		}
	}

	bool IsActive()
	{
		return Active.load(std::memory_order_acquire);
	}

	RECT GetCancelButtonRect()
	{
		const auto x = (WindowWidth - CancelButtonWidth) / 2;
		const auto y = ImageHeight + 112;
		return { x, y, x + CancelButtonWidth, y + CancelButtonHeight };
	}

	void Destroy()
	{
		Active.store(false, std::memory_order_release);

		const auto hwnd = Window.exchange(nullptr, std::memory_order_acq_rel);
		if (hwnd && IsWindow(hwnd))
		{
			DestroyWindow(hwnd);
		}

		if (TitleFont) DeleteObject(TitleFont);
		if (StatusFont) DeleteObject(StatusFont);
		if (PercentageFont) DeleteObject(PercentageFont);
		if (ButtonFont) DeleteObject(ButtonFont);
		if (SplashBitmap) DeleteObject(SplashBitmap);
		if (BackgroundBrush) DeleteObject(BackgroundBrush);
		TitleFont = nullptr;
		StatusFont = nullptr;
		PercentageFont = nullptr;
		ButtonFont = nullptr;
		SplashBitmap = nullptr;
		BackgroundBrush = nullptr;
	}

	void Paint(HWND hwnd)
	{
		PAINTSTRUCT ps{};
		const auto dc = BeginPaint(hwnd, &ps);
		if (!dc)
		{
			return;
		}

		RECT client{};
		GetClientRect(hwnd, &client);
		const auto bufferDc = CreateCompatibleDC(dc);
		const auto bufferBitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
		const auto oldBufferBitmap = SelectObject(bufferDc, bufferBitmap);
		FillRect(bufferDc, &client, BackgroundBrush);

		if (SplashBitmap)
		{
			const auto imageDc = CreateCompatibleDC(bufferDc);
			const auto oldBitmap = SelectObject(imageDc, SplashBitmap);
			BitBlt(bufferDc, (WindowWidth - ImageWidth) / 2, 0, ImageWidth, ImageHeight, imageDc, 0, 0, SRCCOPY);
			SelectObject(imageDc, oldBitmap);
			DeleteDC(imageDc);
		}

		const RECT footer{ 0, ImageHeight, WindowWidth, WindowHeight };
		const auto footerBrush = CreateSolidBrush(RGB(9, 9, 12));
		FillRect(bufferDc, &footer, footerBrush);
		DeleteObject(footerBrush);

		const auto accentBrush = CreateSolidBrush(RGB(132, 16, 22));
		const RECT accent{ 0, ImageHeight, WindowWidth, ImageHeight + 2 };
		FillRect(bufferDc, &accent, accentBrush);
		DeleteObject(accentBrush);

		SetBkMode(bufferDc, TRANSPARENT);
		SetTextColor(bufferDc, RGB(248, 248, 248));
		const auto oldFont = SelectObject(bufferDc, TitleFont);
		RECT titleRect{ 0, ImageHeight + 13, WindowWidth, ImageHeight + 37 };
		DrawTextA(bufferDc, "Zombie Warfare 3", -1, &titleRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

		SetTextColor(bufferDc, RGB(190, 190, 196));
		SelectObject(bufferDc, StatusFont);
		const auto status = GetStatusText();
		RECT statusRect{ 24, ImageHeight + 39, WindowWidth - 24, ImageHeight + 62 };
		DrawTextA(bufferDc, status.c_str(), -1, &statusRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

		const auto trackWidth = std::min<LONG>(WindowWidth - 128, 560);
		constexpr auto trackHeight = 8;
		constexpr auto chunkWidth = 150;
		const auto trackX = (WindowWidth - trackWidth) / 2;
		const auto trackY = ImageHeight + 72;
		const auto nullPen = static_cast<HPEN>(GetStockObject(NULL_PEN));
		const auto oldPen = SelectObject(bufferDc, nullPen);
		const auto trackBrush = CreateSolidBrush(RGB(42, 42, 46));
		const auto oldBrush = SelectObject(bufferDc, trackBrush);
		RoundRect(bufferDc, trackX, trackY, trackX + trackWidth, trackY + trackHeight, trackHeight, trackHeight);

		const auto clip = CreateRoundRectRgn(trackX, trackY, trackX + trackWidth, trackY + trackHeight, trackHeight, trackHeight);
		SelectClipRgn(bufferDc, clip);
		const auto progressBrush = CreateSolidBrush(RGB(190, 32, 38));
		SelectObject(bufferDc, progressBrush);

		const auto determinate = Determinate.load(std::memory_order_acquire);
		const auto progress = Progress.load(std::memory_order_acquire);
		if (determinate)
		{
			const auto filledWidth = static_cast<LONG>(static_cast<float>(trackWidth) * progress);
			if (filledWidth > 0)
			{
				RoundRect(bufferDc, trackX, trackY, trackX + filledWidth, trackY + trackHeight, trackHeight, trackHeight);
			}
		}
		else
		{
			const auto chunkX = trackX + (ProgressOffset % (trackWidth + chunkWidth)) - chunkWidth;
			RoundRect(bufferDc, chunkX, trackY, chunkX + chunkWidth, trackY + trackHeight, trackHeight, trackHeight);
		}

		SelectClipRgn(bufferDc, nullptr);
		DeleteObject(clip);
		SelectObject(bufferDc, oldBrush);
		SelectObject(bufferDc, oldPen);
		DeleteObject(progressBrush);
		DeleteObject(trackBrush);

		if (determinate)
		{
			SelectObject(bufferDc, PercentageFont);
			SetTextColor(bufferDc, RGB(175, 175, 182));
			const auto percentage = std::format("{}%", static_cast<int>((progress * 100.0f) + 0.5f));
			RECT percentageRect{ 0, ImageHeight + 84, WindowWidth, ImageHeight + 104 };
			DrawTextA(bufferDc, percentage.c_str(), -1, &percentageRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		}

		const auto cancelRect = GetCancelButtonRect();
		const auto buttonBrush = CreateSolidBrush(RGB(24, 24, 28));
		const auto buttonPen = CreatePen(PS_SOLID, 1, RGB(110, 34, 40));
		const auto oldButtonBrush = SelectObject(bufferDc, buttonBrush);
		const auto oldButtonPen = SelectObject(bufferDc, buttonPen);
		RoundRect(bufferDc, cancelRect.left, cancelRect.top, cancelRect.right, cancelRect.bottom, 10, 10);
		SelectObject(bufferDc, ButtonFont);
		SetTextColor(bufferDc, RGB(238, 238, 238));
		auto cancelText = cancelRect;
		DrawTextA(bufferDc, "Cancel", -1, &cancelText, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		SelectObject(bufferDc, oldButtonBrush);
		SelectObject(bufferDc, oldButtonPen);
		DeleteObject(buttonPen);
		DeleteObject(buttonBrush);
		SelectObject(bufferDc, oldFont);

		BitBlt(dc, 0, 0, client.right, client.bottom, bufferDc, 0, 0, SRCCOPY);
		SelectObject(bufferDc, oldBufferBitmap);
		DeleteObject(bufferBitmap);
		DeleteDC(bufferDc);
		EndPaint(hwnd, &ps);
	}

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_TIMER:
			if (wParam == DotAnimationTimerId)
			{
				if (!Determinate.load(std::memory_order_acquire))
				{
					DotCount = (DotCount + 1) % 4;
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				return 0;
			}

			if (wParam == AnimationTimerId)
			{
				const auto trackWidth = std::min<LONG>(WindowWidth - 128, 560);
				ProgressOffset = (ProgressOffset + 5) % (trackWidth + 150);
				InvalidateRect(hwnd, nullptr, FALSE);
				return 0;
			}
			break;

		case WM_LBUTTONUP:
		{
			const POINT point{ static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
			const auto cancelRect = GetCancelButtonRect();
			if (PtInRect(&cancelRect, point))
			{
				ExitProcess(EXIT_SUCCESS);
			}
			return 0;
		}

		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			Paint(hwnd);
			return 0;
		case WM_CLOSE:
			Destroy();
			return 0;
		case WM_DESTROY:
			KillTimer(hwnd, AnimationTimerId);
			KillTimer(hwnd, DotAnimationTimerId);
			return 0;
		}

		return DefWindowProcA(hwnd, message, wParam, lParam);
	}

	void Create()
	{
		if (Window.load(std::memory_order_acquire))
		{
			return;
		}

		const auto instance = GetModuleHandleA(nullptr);
		BackgroundBrush = CreateSolidBrush(RGB(10, 10, 12));
		WNDCLASSA wc{};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = BackgroundBrush;
		wc.lpszClassName = WindowClassName;
		RegisterClassA(&wc);

		SplashBitmap = LoadSplashBitmap();
		BITMAP info{};
		if (SplashBitmap) GetObjectA(SplashBitmap, sizeof(info), &info);
		ImageWidth = info.bmWidth > 0 ? info.bmWidth : MinWindowWidth;
		ImageHeight = info.bmHeight > 0 ? info.bmHeight : DefaultImageHeight;
		WindowWidth = std::max(ImageWidth, MinWindowWidth);
		WindowHeight = ImageHeight + FooterHeight;
		const auto x = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		const auto y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;
		const auto hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WindowClassName, "Call of Duty: Zombie Warfare 3", WS_POPUP,
			x, y, WindowWidth, WindowHeight, nullptr, nullptr, instance, nullptr);

		if (!hwnd)
		{
			Destroy();
			return;
		}

		TitleFont = CreateFontA(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
		StatusFont = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
		PercentageFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
		ButtonFont = CreateFontA(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
		Window.store(hwnd, std::memory_order_release);
		Active.store(true, std::memory_order_release);
		ShowWindow(hwnd, SW_SHOWNORMAL);
		UpdateWindow(hwnd);
		SetTimer(hwnd, AnimationTimerId, AnimationTimerInterval, nullptr);
		SetTimer(hwnd, DotAnimationTimerId, DotAnimationInterval, nullptr);
	}

	void Pump()
	{
		MSG message{};
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}

	void Start()
	{
		Determinate.store(false, std::memory_order_release);
		Progress.store(0.0f, std::memory_order_release);
		DotCount = 0;
		ProgressOffset = 0;
		{
			std::lock_guard lock(StatusMutex);
			StatusText = "Launching game, please wait";
		}

		Components::Scheduler::Once(Create, Components::Scheduler::Pipeline::ASYNC);
		Components::Scheduler::Schedule([]
			{
				Pump();
				return Window.load(std::memory_order_acquire) == nullptr;
			}, Components::Scheduler::Pipeline::ASYNC, 10ms);
	}

	void Stop()
	{
		Components::Scheduler::Once(Destroy, Components::Scheduler::Pipeline::ASYNC);
	}
}

namespace Main
{
	void Initialize()
	{
		std::srand(std::uint32_t(std::time(nullptr)) ^ ~(GetTickCount() * GetCurrentProcessId()));

		Utils::SetEnvironment();
		Steam::Proxy::RunMod();
		Utils::Cryptography::Initialize();

		Components::FileSystem::CleanupZw3Files();
		Components::Loader::Initialize();
	}

	void Uninitialize()
	{
		Components::Loader::Uninitialize();
	}

	int EntryPoint()
	{
		// /GS security cookie must be initialized before any exception-handling
		// constructs are registered in the current module.
		//
		Game::__security_init_cookie();

		// Perform IW4x-specific initialization before transferring control
	// to the original C runtime startup. See DllMain() for context.
	//
		Initialize();

		return Game::__tmainCRTStartup();
	}
}

BOOL APIENTRY DllMain(HINSTANCE /*hinstDLL*/, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		SetProcessDEPPolicy(PROCESS_DEP_ENABLE);

#ifndef DISABLE_BINARY_CHECK
		const auto* binary = reinterpret_cast<const char*>(0x6F9358);
		if (!binary || std::memcmp(binary, BASEGAME_NAME, 14) != 0)
		{
			MessageBoxA(nullptr,
			            "Failed to load game binary.\n"
			            "You did not install the iw4x-rawfiles!\n"
			            "Please use the Zombie Warfare 3 Launcher to run the game. For support, please visit https://zw3.eu",
			            "ERROR",
			            MB_ICONERROR
			);
			return FALSE;
		}
#endif

		Utils::Hook(0x6BAC0F, Main::EntryPoint, HOOK_JUMP).install()->quick();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		// For `DLL_PROCESS_DETACH`, the `lpReserved` parameter is used to
		// determine the context:
		//
		//   - `lpReserved == nullptr` when `FreeLibrary()` is called.
		//   - `lpReserved != nullptr` when the process is being terminated.
		//
		// When `FreeLibrary()` is called, worker threads remain alive. That is,
		// runtime's state is consistent, and executing proper shutdown is
		// acceptable.
		//
		// When process is terminated, worker threads have either exited or been
		// forcefully terminated by the OS, leaving only the shutdown thread.
		// This situation leave runtime in an inconsistent state.
		//
		// Hence, proper cleanup should only be attempted when `FreeLibrary()`
		// is called. Otherwise, the process should rely on the OS to reclaim
		// resources.
		//
		if (lpvReserved != nullptr)
			return TRUE;

		Main::Uninitialize();
	}

	return TRUE;
}
