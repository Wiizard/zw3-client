#pragma once

#include "Dvar.hpp"

namespace Components
{
	struct rawMouseValue_t
	{
		int current = 0;
		int previous = 0;

		void ResetDelta();
		int GetDelta() const;
		void Update(int value, bool absolute);
	};

	class RawMouse : public Component
	{
	public:
		RawMouse();

		static void IN_MouseMove();

		static void SuspendMouseInput();

		static LRESULT OnLBDown(LPARAM lParam, WPARAM wParam);
		static LRESULT OnLBUp(LPARAM lParam, WPARAM wParam);

		static LRESULT OnRBDown(LPARAM lParam, WPARAM wParam);
		static LRESULT OnRBUp(LPARAM lParam, WPARAM wParam);

		static LRESULT OnMBDown(LPARAM lParam, WPARAM wParam);
		static LRESULT OnMBUp(LPARAM lParam, WPARAM wParam);

		static LRESULT OnXBDown(LPARAM lParam, WPARAM wParam);
		static LRESULT OnXBUp(LPARAM lParam, WPARAM wParam);

	private:
		static Dvar::Var m_rawinput;
		static Dvar::Var m_rawinput_verbose;
		static Dvar::Var r_autopriority;
		static rawMouseValue_t mouseRawX;
		static rawMouseValue_t mouseRawY;
		static std::uint32_t mouseRawEvents;
		static bool inRawInput;
		static bool firstRawInputUpdate;
		static bool firstLegacyInputUpdate;
		static bool isCursorClipped;

		static void IN_ClampMouseMove();
		static void ResetMouseRawEvents();
		static void ReleaseMouseCursor();
		static void ProcessMouseRawEvent(DWORD usButtonFlags, DWORD flagDown, DWORD mouseEvent);
		static bool GetRawInput(LPARAM lParam, RAWINPUT& raw, UINT& dwSize);
		static LRESULT OnRawInput(LPARAM lParam, WPARAM wParam);
		static bool IsMouseInClientBounds();
		static LRESULT OnLegacyMouseEvent(UINT Msg, LPARAM lParam, WPARAM wParam);
		static LRESULT OnKillFocus(LPARAM lParam, WPARAM wParam);
		static LRESULT OnSetFocus(LPARAM lParam, WPARAM wParam);
		static void IN_RawMouseMove();
		static bool ToggleRawInput(bool enable = true);
		static void IN_RawMouse_Init();
		static void IN_Init();
		static void IN_Frame();
		static BOOL IN_ClipCursor();
		static BOOL IN_RecenterMouse();
	};
}
