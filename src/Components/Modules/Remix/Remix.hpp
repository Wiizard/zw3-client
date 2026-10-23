#pragma once

#include "Components/Modules/Dvar.hpp"

namespace Components::Remix
{
	class Remix : public Component
	{
	public:
		using Direct3DCreate9_t = IDirect3D9*(WINAPI*)(UINT sdkVersion);

		Remix();

		static bool IsActive();

		// r_rtxRemix as set now; the runtime itself only changes on the next launch
		static bool IsEnabled();

		static IDirect3D9* TryCreateDirect3D(UINT sdkVersion);

		static void BeginUiDrawing(IDirect3DDevice9* device);

		// frames Remix put on screen per second, frame generation's included; -1 until Remix has presented
		static int GetPresentedFps();

	private:
		static Dvar::Var r_rtxRemix;

		static void OnDeviceCreated(IDirect3DDevice9* device);
		static void OnDeviceReset(IDirect3DDevice9* device);
		static void FailBackToStock(const std::string& reason);
		static void SetEnabledAndRestart(bool enabled);
	};
}
