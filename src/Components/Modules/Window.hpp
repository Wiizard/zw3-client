#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Window : public Component
	{
	public:
		typedef LRESULT(WndProcCallback)(LPARAM lParam, WPARAM wParam);
		typedef void(CreateCallback)();
		typedef void(DeviceChangeCallback)(WPARAM wParam, LPARAM lParam);

		Window();

		static int Width();
		static int Height();
		static int Width(HWND window);
		static int Height(HWND window);
		static void Dimension(RECT* rect);
		static void Dimension(HWND window, RECT* rect);

		static bool IsCursorWithin(HWND window);

		static bool HasFocus();

		static bool IsLoadingScreenMovable();
		static void ApplyDisplayModeDvars();
		static bool IsDragging();

		static void PumpLoadingEvents();

		static HWND GetWindow();

		static void OnWndMessage(UINT Msg, const std::function<WndProcCallback>& callback);
		static void OnDeviceChange(const std::function<DeviceChangeCallback>& callback);

		static void OnCreate(const std::function<CreateCallback>& callback);

	private:
		static BOOL cursorVisible;
		static Dvar::Var ui_nativeCursor;
		static std::unordered_map<UINT, std::function<WndProcCallback>> wndMessageCallbacks;
		static std::vector<std::function<CreateCallback>> createSignals;
		static std::vector<std::function<DeviceChangeCallback>> deviceChangeSignals;

		static HWND mainWindow;
		static WNDPROC originalWindowProc;

		static void ApplyCursor();

		static LRESULT CALLBACK MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK NativeWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

		static int WINAPI ShowCursorHook(BOOL show);
		static void DrawCursorStub(const Game::ScreenPlacement* scrPlace, float x, float y, float w, float h, int horzAlign, int vertAlign, const float* color, Game::Material* material);

		static HWND WINAPI CreateMainWindow(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

		static void EnableDpiAwareness();
	};
}
