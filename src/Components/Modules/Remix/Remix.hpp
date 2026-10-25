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

		// The lobby draws its room as Remix's 3D scene: Begin before the room (which map's settings and lights to use),
		// End after it (submits the lights and marks where the menu UI starts).
		static void BeginMenuScene(const char* mapName);
		static void EndMenuScene(IDirect3DDevice9* device);

		// Runs at the start of every backend frame in Remix mode (the device's BeginScene), before the frame's UI.
		static void OnFrameStart(void (*callback)(IDirect3DDevice9* device));

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
