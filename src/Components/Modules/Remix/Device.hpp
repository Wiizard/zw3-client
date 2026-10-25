#pragma once

namespace Components::Remix::Device
{
	using DeviceCallback = void (*)(IDirect3DDevice9* device);

	IDirect3D9* WrapDirect3D(IDirect3D9* direct3D, DeviceCallback onCreated, DeviceCallback onReset);

	// Called right after the game's BeginScene (RB_BeginFrame, the start of a backend frame), before anything of the
	// frame is drawn.
	void OnBeginScene(DeviceCallback callback);

	// BeginScene for zw3's own drawing (the INSERT menu at the end of the frame), which must not run the callbacks.
	HRESULT BeginOwnScene(IDirect3DDevice9* device);

	// Colour clears are dropped from now until the next frame starts: zw3 drew the frame's 3D scene itself (the lobby)
	// and the menu's own black clear (SCR_DrawScreenField's R_AddCmdClearScreen) would wipe Remix's image of it.
	void KeepSceneUntilFrameEnd();

	template <typename T>
	T* Unwrap(T* object)
	{
		return object;
	}

	void NoteFrameEvent(const std::string& text);

	void ArmFrameCapture();

	void AdvanceFrameCapture();

	bool TryHashTopLevel(const void* bits, const D3DSURFACE_DESC& desc, std::uint64_t& hash);

	bool TryGetTextureHash(const void* texture, std::uint64_t& hash);

	void BeginEffectBlendRemap(IDirect3DDevice9* device);

	void EndEffectBlendRemap(IDirect3DDevice9* device);

	void BeginOverlayBlendRemap(IDirect3DDevice9* device);

	void EndOverlayBlendRemap(IDirect3DDevice9* device);

	void BeginPremultipliedRemap(IDirect3DDevice9* device);

	void EndPremultipliedRemap(IDirect3DDevice9* device);

	void EnterRenderFrame();

	void LeaveRenderFrame();

	bool IsQueryPollGuarded();
}
