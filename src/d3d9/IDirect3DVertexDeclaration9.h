#pragma once

class m_IDirect3DVertexDeclaration9 : public IDirect3DVertexDeclaration9, public AddressLookupTableObject
{
private:
	LPDIRECT3DVERTEXDECLARATION9 ProxyInterface;
	m_IDirect3DDevice9Ex* m_pDeviceEx = nullptr;

public:
	m_IDirect3DVertexDeclaration9(LPDIRECT3DVERTEXDECLARATION9 pDeclaration9, m_IDirect3DDevice9Ex* pDevice) : ProxyInterface(pDeclaration9), m_pDeviceEx(pDevice)
	{
		pDevice->ProxyAddressLookupTable->SaveAddress(this, ProxyInterface);
	}
	~m_IDirect3DVertexDeclaration9() {}

	LPDIRECT3DVERTEXDECLARATION9 GetProxyInterface() { return ProxyInterface; }

	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);

	STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
	STDMETHOD(GetDeclaration)(THIS_ D3DVERTEXELEMENT9* pElement, UINT* pNumElements);
};
