#include "STDInclude.hpp"

#include <dbt.h>

#include "Controller/Transport/DeviceNotify.hpp"

namespace Controller::Transport
{
	static constexpr wchar_t windowClass[] = L"iw4x_controller_devnotify";
	static constexpr UINT_PTR rescanTimer = 1;
	static constexpr UINT_PTR retryTimer = 2;
	static constexpr UINT rescanDelayMs = 300;
	static constexpr UINT retryDelayMs = 2000;

	static std::atomic<bool>* PendingFlagOf(HWND window) noexcept
	{
		return reinterpret_cast<std::atomic<bool>*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	}

	static void RaisePendingFlag(HWND window) noexcept
	{
		auto* const flag = PendingFlagOf(window);

		if (flag != nullptr)
		{
			flag->store(true, std::memory_order_release);
		}
	}

	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
	{
		switch (message)
		{
		case WM_DEVICECHANGE:
		{
			const auto* const change = reinterpret_cast<const DEV_BROADCAST_HDR*>(lParam);
			const bool isInterfaceChange = (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)
				&& change != nullptr && change->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE;

			if (isInterfaceChange)
			{
				const UINT_PTR first = SetTimer(window, rescanTimer, rescanDelayMs, nullptr);
				const UINT_PTR retry = SetTimer(window, retryTimer, retryDelayMs, nullptr);

				if (first == 0 || retry == 0)
				{
					RaisePendingFlag(window);
				}
			}

			return TRUE;
		}

		case WM_TIMER:
			if (wParam == rescanTimer || wParam == retryTimer)
			{
				KillTimer(window, wParam);
				RaisePendingFlag(window);
				return 0;
			}

			break;

		case WM_CLOSE:
			DestroyWindow(window);
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}

	DeviceNotifier::DeviceNotifier(const Context& context)
		: thread([this, context](std::stop_token stop)
		{
			this->Run(stop, context);
		})
	{
	}

	void DeviceNotifier::Run(const std::stop_token& stop, const Context& context)
	{
		const HINSTANCE instance = GetModuleHandleW(nullptr);

		WNDCLASSEXW windowClassInfo{};
		windowClassInfo.cbSize = sizeof(windowClassInfo);
		windowClassInfo.lpfnWndProc = &WindowProc;
		windowClassInfo.hInstance = instance;
		windowClassInfo.lpszClassName = windowClass;

		if (RegisterClassExW(&windowClassInfo) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			this->hasFailed.store(true, std::memory_order_release);
			context.Report(Severity::Warning, Facility::Discovery, ErrorCode::TransportFailure, "device-change window class registration failed; discovery will poll");
			return;
		}

		const HWND window = CreateWindowExW(0, windowClass, windowClass, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);

		if (window == nullptr)
		{
			this->hasFailed.store(true, std::memory_order_release);
			context.Report(Severity::Warning, Facility::Discovery, ErrorCode::TransportFailure, "device-change window creation failed; discovery will poll");
			return;
		}

		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&this->isPending));

		const std::stop_callback wake(stop, [window]() noexcept
		{
			PostMessageW(window, WM_CLOSE, 0, 0);
		});

		DEV_BROADCAST_DEVICEINTERFACE_W filter{};
		filter.dbcc_size = sizeof(filter);
		filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

		const HDEVNOTIFY notification = RegisterDeviceNotificationW(window, &filter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);

		if (notification == nullptr)
		{
			this->hasFailed.store(true, std::memory_order_release);
			context.Report(Severity::Warning, Facility::Discovery, ErrorCode::TransportFailure, "device-change registration failed; discovery will poll");
		}

		this->isPending.store(true, std::memory_order_release);

		MSG message;

		while (GetMessageW(&message, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}

		if (!stop.stop_requested())
		{
			this->hasFailed.store(true, std::memory_order_release);
		}

		if (notification != nullptr)
		{
			UnregisterDeviceNotification(notification);
		}
	}
}
