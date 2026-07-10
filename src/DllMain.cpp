namespace StartupSplash
{
	constexpr auto* WindowClassName = "ZW3StartupSplash";
	constexpr UINT_PTR AnimationTimerId = 1;
	constexpr LONG MinWindowWidth = 760;
	constexpr LONG DefaultImageHeight = 380;
	constexpr LONG FooterHeight = 118;

	std::thread Thread;
	std::atomic<HWND> Window = nullptr;
	std::atomic_bool StopRequested = false;

	HBITMAP SplashBitmap = nullptr;
	HFONT StatusFont = nullptr;
	HBRUSH BackgroundBrush = nullptr;
	LONG ImageWidth = MinWindowWidth;
	LONG ImageHeight = DefaultImageHeight;
	LONG WindowWidth = MinWindowWidth;
	LONG WindowHeight = DefaultImageHeight + FooterHeight;
	int DotCount = 0;
	int ProgressOffset = 0;
	int AnimationTick = 0;

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
		std::string text = "Launching game, please wait";
		text.append(DotCount, '.');
		return text;
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
		const auto bufferBitmap = CreateCompatibleBitmap(dc, client.right - client.left, client.bottom - client.top);
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

		RECT footer{ 0, ImageHeight, WindowWidth, WindowHeight };
		const auto footerBrush = CreateSolidBrush(RGB(12, 12, 14));
		FillRect(bufferDc, &footer, footerBrush);
		DeleteObject(footerBrush);

		SetBkMode(bufferDc, TRANSPARENT);
		SetTextColor(bufferDc, RGB(238, 238, 238));
		const auto oldFont = SelectObject(bufferDc, StatusFont);

		auto text = GetStatusText();
		RECT textRect{ 0, ImageHeight + 24, WindowWidth, ImageHeight + 52 };
		DrawTextA(bufferDc, text.data(), -1, &textRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

		SelectObject(bufferDc, oldFont);

		const auto trackWidth = std::min<LONG>(WindowWidth - 128, 560);
		const auto trackHeight = 10;
		const auto trackX = (WindowWidth - trackWidth) / 2;
		const auto trackY = ImageHeight + 72;
		const auto chunkWidth = 150;
		const auto chunkX = trackX + (ProgressOffset % (trackWidth + chunkWidth)) - chunkWidth;

		const auto nullPen = static_cast<HPEN>(GetStockObject(NULL_PEN));
		const auto oldPen = SelectObject(bufferDc, nullPen);

		const auto trackBrush = CreateSolidBrush(RGB(42, 42, 46));
		const auto oldBrush = SelectObject(bufferDc, trackBrush);
		RoundRect(bufferDc, trackX, trackY, trackX + trackWidth, trackY + trackHeight, trackHeight, trackHeight);

		HRGN clip = CreateRoundRectRgn(trackX, trackY, trackX + trackWidth, trackY + trackHeight, trackHeight, trackHeight);
		SelectClipRgn(bufferDc, clip);

		const auto chunkBrush = CreateSolidBrush(RGB(190, 32, 38));
		SelectObject(bufferDc, chunkBrush);
		RoundRect(bufferDc, chunkX, trackY, chunkX + chunkWidth, trackY + trackHeight, trackHeight, trackHeight);

		SelectClipRgn(bufferDc, nullptr);
		DeleteObject(clip);
		SelectObject(bufferDc, oldBrush);
		SelectObject(bufferDc, oldPen);
		DeleteObject(chunkBrush);
		DeleteObject(trackBrush);

		BitBlt(dc, 0, 0, client.right - client.left, client.bottom - client.top, bufferDc, 0, 0, SRCCOPY);
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
			if (wParam == AnimationTimerId)
			{
				++AnimationTick;
				if ((AnimationTick % 14) == 0)
				{
					DotCount = (DotCount + 1) % 4;
				}

				ProgressOffset = (ProgressOffset + 10) % (WindowWidth + 180);
				InvalidateRect(hwnd, nullptr, FALSE);
				return 0;
			}
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT:
			Paint(hwnd);
			return 0;

		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;

		case WM_DESTROY:
			KillTimer(hwnd, AnimationTimerId);
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcA(hwnd, message, wParam, lParam);
	}

	void ThreadProc()
	{
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
		if (SplashBitmap)
		{
			GetObjectA(SplashBitmap, sizeof(info), &info);
		}

		ImageWidth = info.bmWidth > 0 ? info.bmWidth : MinWindowWidth;
		ImageHeight = info.bmHeight > 0 ? info.bmHeight : DefaultImageHeight;
		WindowWidth = std::max(ImageWidth, MinWindowWidth);
		WindowHeight = ImageHeight + FooterHeight;

		const auto x = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		const auto y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;

		auto* hwnd = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, WindowClassName, "Call of Duty: Zombie Warfare 3",
			WS_POPUP, x, y, WindowWidth, WindowHeight, nullptr, nullptr, instance, nullptr);

		if (!hwnd)
		{
			if (SplashBitmap) DeleteObject(SplashBitmap);
			if (BackgroundBrush) DeleteObject(BackgroundBrush);
			return;
		}

		StatusFont = CreateFontA(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

		Window.store(hwnd);
		ShowWindow(hwnd, SW_SHOWNORMAL);
		UpdateWindow(hwnd);
		SetTimer(hwnd, AnimationTimerId, 35, nullptr);

		if (StopRequested.load())
		{
			DestroyWindow(hwnd);
		}

		MSG msg{};
		while (GetMessageA(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		Window.store(nullptr);
		if (StatusFont) DeleteObject(StatusFont);
		if (SplashBitmap) DeleteObject(SplashBitmap);
		if (BackgroundBrush) DeleteObject(BackgroundBrush);
		StatusFont = nullptr;
		SplashBitmap = nullptr;
		BackgroundBrush = nullptr;
	}

	void Start()
	{
		if (std::strstr(GetCommandLineA(), "-dedicated") || std::strstr(GetCommandLineA(), "+set dedicated"))
		{
			return;
		}

		StopRequested.store(false);
		Thread = std::thread(ThreadProc);
	}

	void Stop()
	{
		StopRequested.store(true);

		if (const auto hwnd = Window.load())
		{
			PostMessageA(hwnd, WM_CLOSE, 0, 0);
		}

		if (Thread.joinable())
		{
			Thread.join();
		}
	}
}

namespace Main
{
	void Initialize()
	{
		std::srand(std::uint32_t(std::time(nullptr)) ^ ~(GetTickCount() * GetCurrentProcessId()));

		Utils::SetEnvironment();
		StartupSplash::Start();

		Steam::Proxy::RunMod();
		Components::Loader::Initialize();

		StartupSplash::Stop();
	}

	void Uninitialize()
	{
		StartupSplash::Stop();
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
