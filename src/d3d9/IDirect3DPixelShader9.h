#pragma once

class m_IDirect3DPixelShader9 : public IDirect3DPixelShader9, public AddressLookupTableObject
{
private:
	LPDIRECT3DPIXELSHADER9 ProxyInterface;
	m_IDirect3DDevice9Ex* m_pDeviceEx = nullptr;

public:
	m_IDirect3DPixelShader9(LPDIRECT3DPIXELSHADER9 pShader9, m_IDirect3DDevice9Ex* pDevice) : ProxyInterface(pShader9), m_pDeviceEx(pDevice)
	{
		pDevice->ProxyAddressLookupTable->SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DPixelShader9() {}

	LPDIRECT3DPIXELSHADER9 GetProxyInterface() { return ProxyInterface; }

	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
	STDMETHOD(GetFunction)(THIS_ void* pData, UINT* pSizeOfData);
};
