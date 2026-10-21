#pragma once

namespace Components::Remix::Menu
{
	void Render(IDirect3DDevice9* device, HWND window);

	void OnDeviceLost();

	bool IsOpen();
}
