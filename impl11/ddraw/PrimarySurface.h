// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#pragma once

class BackbufferSurface;

class Direct3DTexture;

void InitHeadingMatrix();
Matrix4 GetCurrentHeadingMatrix(Vector4 &Rs, Vector4 &Us, Vector4 &Fs, bool invert, bool debug);
Matrix4 GetCurrentHeadingViewMatrix();

class PrimarySurface : public IDirectDrawSurface
{
public:
	PrimarySurface(DeviceResources* deviceResources, bool hasBackbufferAttached);

	virtual ~PrimarySurface();

	/*** IUnknown methods ***/

	STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR * ppvObj);

	STDMETHOD_(ULONG, AddRef) (THIS);

	STDMETHOD_(ULONG, Release) (THIS);

	/*** IDirectDrawSurface methods ***/

	STDMETHOD(AddAttachedSurface)(THIS_ LPDIRECTDRAWSURFACE);

	STDMETHOD(AddOverlayDirtyRect)(THIS_ LPRECT);

	STDMETHOD(Blt)(THIS_ LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDBLTFX);

	STDMETHOD(BltBatch)(THIS_ LPDDBLTBATCH, DWORD, DWORD);

	STDMETHOD(BltFast)(THIS_ DWORD, DWORD, LPDIRECTDRAWSURFACE, LPRECT, DWORD);

	STDMETHOD(DeleteAttachedSurface)(THIS_ DWORD, LPDIRECTDRAWSURFACE);

	STDMETHOD(EnumAttachedSurfaces)(THIS_ LPVOID, LPDDENUMSURFACESCALLBACK);

	STDMETHOD(EnumOverlayZOrders)(THIS_ DWORD, LPVOID, LPDDENUMSURFACESCALLBACK);

	void barrelEffect2D(int iteration);

	void resizeForSteamVR(int iteration, bool is_2D);

	void barrelEffect3D();

	void barrelEffectSteamVR();

	void BloomBasicPass(int pass, float fZoomFactor);

	void BloomPyramidLevelPass(int PyramidLevel, int AdditionalPasses, float fZoomFactor, bool debug);

	void capture(int time_delay, ComPtr<ID3D11Texture2D> buffer, const wchar_t *filename);

	void ClearBox(uvfloat4 box, D3D11_VIEWPORT * viewport, D3DCOLOR clearColor);

	void ClearHUDRegions();

	void DrawHUDVertices();

	void ComputeNormalsPass(float fZoomFactor);

	//void SmoothNormalsPass(float fZoomFactor);

	void SetLights(float fSSDOEnabled);
	
	void SSAOPass(float fZoomFactor);

	void SSDOPass(float fZoomFactor, float fZoomFactor2);

	void DeferredPass();

	//void InitHeadingMatrix();

	//Matrix4 GetCurrentHeadingMatrix(Vector4 &Rs, Vector4 &Us, Vector4 &Fs, bool invert, bool debug);

	//Matrix4 GetCurrentHeadingViewMatrix();

	void GetCockpitViewMatrix(Matrix4 * result, bool invert);

	void GetCraftViewMatrix(Matrix4 *result);

	void RenderHyperspaceEffect(D3D11_VIEWPORT *lastViewport,
		ID3D11PixelShader *lastPixelShader, Direct3DTexture *lastTextureSelected,
		ID3D11Buffer *lastVertexBuffer, UINT *lastVertexBufStride, UINT *lastVertexBufOffset);

	void RenderFXAA();

	void RenderExternalHUD();

	void RenderSun();

	void ACRunAction(WORD * action);

	void RenderLaserPointer(D3D11_VIEWPORT * lastViewport, ID3D11PixelShader * lastPixelShader, Direct3DTexture * lastTextureSelected, ID3D11Buffer * lastVertexBuffer, UINT * lastVertexBufStride, UINT * lastVertexBufOffset);

	void ProcessFreePIEGamePad(uint32_t axis0, uint32_t axis1, uint32_t buttonsPressed);

	STDMETHOD(Flip)(THIS_ LPDIRECTDRAWSURFACE, DWORD);

	STDMETHOD(GetAttachedSurface)(THIS_ LPDDSCAPS, LPDIRECTDRAWSURFACE FAR *);

	STDMETHOD(GetBltStatus)(THIS_ DWORD);

	STDMETHOD(GetCaps)(THIS_ LPDDSCAPS);

	STDMETHOD(GetClipper)(THIS_ LPDIRECTDRAWCLIPPER FAR*);

	STDMETHOD(GetColorKey)(THIS_ DWORD, LPDDCOLORKEY);

	STDMETHOD(GetDC)(THIS_ HDC FAR *);

	STDMETHOD(GetFlipStatus)(THIS_ DWORD);

	STDMETHOD(GetOverlayPosition)(THIS_ LPLONG, LPLONG);

	STDMETHOD(GetPalette)(THIS_ LPDIRECTDRAWPALETTE FAR*);

	STDMETHOD(GetPixelFormat)(THIS_ LPDDPIXELFORMAT);

	STDMETHOD(GetSurfaceDesc)(THIS_ LPDDSURFACEDESC);

	STDMETHOD(Initialize)(THIS_ LPDIRECTDRAW, LPDDSURFACEDESC);

	STDMETHOD(IsLost)(THIS);

	STDMETHOD(Lock)(THIS_ LPRECT, LPDDSURFACEDESC, DWORD, HANDLE);

	STDMETHOD(ReleaseDC)(THIS_ HDC);

	STDMETHOD(Restore)(THIS);

	STDMETHOD(SetClipper)(THIS_ LPDIRECTDRAWCLIPPER);

	STDMETHOD(SetColorKey)(THIS_ DWORD, LPDDCOLORKEY);

	STDMETHOD(SetOverlayPosition)(THIS_ LONG, LONG);

	STDMETHOD(SetPalette)(THIS_ LPDIRECTDRAWPALETTE);

	STDMETHOD(Unlock)(THIS_ LPVOID);

	STDMETHOD(UpdateOverlay)(THIS_ LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDOVERLAYFX);

	STDMETHOD(UpdateOverlayDisplay)(THIS_ DWORD);

	STDMETHOD(UpdateOverlayZOrder)(THIS_ DWORD, LPDIRECTDRAWSURFACE);

	void RenderText();

	ULONG _refCount;

	DeviceResources* _deviceResources;

	bool _hasBackbufferAttached;
	ComPtr<BackbufferSurface> _backbufferSurface;

	UINT _flipFrames;
};
