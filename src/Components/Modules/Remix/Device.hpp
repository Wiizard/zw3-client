#pragma once

namespace Components::Remix::Device
{
	using DeviceCallback = void (*)(IDirect3DDevice9* device);

	IDirect3D9* WrapDirect3D(IDirect3D9* direct3D, DeviceCallback onCreated, DeviceCallback onReset);

	template <typename T>
	T* Unwrap(T* object)
	{
		return object;
	}

	void NoteFrameEvent(const std::string& text);

	bool TryHashTopLevel(const void* bits, const D3DSURFACE_DESC& desc, std::uint64_t& hash);

	bool TryGetTextureHash(const void* texture, std::uint64_t& hash);

	void BeginEffectBlendRemap(IDirect3DDevice9* device);

	void EndEffectBlendRemap(IDirect3DDevice9* device);

	void BeginOverlayBlendRemap(IDirect3DDevice9* device);

	void EndOverlayBlendRemap(IDirect3DDevice9* device);

	void BeginPremultipliedRemap(IDirect3DDevice9* device);

	void EndPremultipliedRemap(IDirect3DDevice9* device);
}
