// Copyright (c) 2014 J�r�my Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019

#pragma once

class RenderStates;

class Direct3DTexture;
class XwaTextureData;

class Direct3DDevice : IDirect3DDevice
{
public:
	Direct3DDevice(DeviceResources* deviceResources);

	virtual ~Direct3DDevice();

	/*** IUnknown methods ***/

	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj);

	STDMETHOD_(ULONG, AddRef)(THIS);

	STDMETHOD_(ULONG, Release)(THIS);

	/*** IDirect3DDevice methods ***/

	STDMETHOD(Initialize)(THIS_ LPDIRECT3D, LPGUID, LPD3DDEVICEDESC);

	STDMETHOD(GetCaps)(THIS_ LPD3DDEVICEDESC, LPD3DDEVICEDESC);

	STDMETHOD(SwapTextureHandles)(THIS_ LPDIRECT3DTEXTURE, LPDIRECT3DTEXTURE);

	STDMETHOD(CreateExecuteBuffer)(THIS_ LPD3DEXECUTEBUFFERDESC, LPDIRECT3DEXECUTEBUFFER*, IUnknown*);

	STDMETHOD(GetStats)(THIS_ LPD3DSTATS);

	HRESULT QuickSetZWriteEnabled(BOOL);

	void GetBoundingBoxUVs(LPD3DINSTRUCTION instruction, UINT curIndex,
		float *minX, float *minY, float *maxX, float *maxY,
		float *minU, float *minV, float *maxU, float *maxV,
		bool debug=false);

	void GetBoundingBoxes(LPD3DINSTRUCTION instruction, UINT curIndex, std::vector<Box>& boxes, bool debug=false);

	bool ComputeCentroid(LPD3DINSTRUCTION instruction, UINT curIndex, Vector3 *Centroid, Vector2 *Centroid2D);

	bool ComputeCentroid2D(LPD3DINSTRUCTION instruction, UINT curIndex, Vector2 * Centroid);

	void AddLaserLights(LPD3DINSTRUCTION instruction, UINT curIndex, Direct3DTexture *texture);

	void AddExplosionLights(LPD3DINSTRUCTION instruction, UINT curIndex, XwaTextureData * texture);

	//void ClearBox(Box box, D3D11_VIEWPORT *viewport, bool fullScreen, float scale, D3DCOLOR clearColor);

	inline ID3D11RenderTargetView *SelectOffscreenBuffer();

	void EnableTransparency();

	void EnableHoloTransparency();

	uint32_t GetWarningLightColor(LPD3DINSTRUCTION instruction, UINT currentIndexLocation, XwaTextureData * lastTextureSelected);

	void UpdateReconstructionConstants();

	STDMETHOD(Execute)(THIS_ LPDIRECT3DEXECUTEBUFFER, LPDIRECT3DVIEWPORT, DWORD);

	STDMETHOD(AddViewport)(THIS_ LPDIRECT3DVIEWPORT);

	STDMETHOD(DeleteViewport)(THIS_ LPDIRECT3DVIEWPORT);

	STDMETHOD(NextViewport)(THIS_ LPDIRECT3DVIEWPORT, LPDIRECT3DVIEWPORT*, DWORD);

	STDMETHOD(Pick)(THIS_ LPDIRECT3DEXECUTEBUFFER, LPDIRECT3DVIEWPORT, DWORD, LPD3DRECT);

	STDMETHOD(GetPickRecords)(THIS_ LPDWORD, LPD3DPICKRECORD);

	STDMETHOD(EnumTextureFormats)(THIS_ LPD3DENUMTEXTUREFORMATSCALLBACK, LPVOID);

	STDMETHOD(CreateMatrix)(THIS_ LPD3DMATRIXHANDLE);

	STDMETHOD(SetMatrix)(THIS_ D3DMATRIXHANDLE, const LPD3DMATRIX);

	STDMETHOD(GetMatrix)(THIS_ D3DMATRIXHANDLE, LPD3DMATRIX);

	STDMETHOD(DeleteMatrix)(THIS_ D3DMATRIXHANDLE);

	STDMETHOD(BeginScene)(THIS);

	STDMETHOD(EndScene)(THIS);

	STDMETHOD(GetDirect3D)(THIS_ LPDIRECT3D*);

	ULONG _refCount;

	DeviceResources* _deviceResources;

	RenderStates* _renderStates;

	DWORD _maxExecuteBufferSize;
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;
};
