// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#include <ScreenGrab.h>
#include <wincodec.h>

#include "common.h"
#include "DeviceResources.h"
#include "PrimarySurface.h"
#include "BackbufferSurface.h"
#include "FrontbufferSurface.h"
#include "FreePIE.h"
#include "Matrices.h"

#define DBG_MAX_PRESENT_LOGS 0

#include <vector>

#include "XWAObject.h"
extern PlayerDataEntry* PlayerDataTable;
const auto mouseLook_Y = (int*)0x9E9624;
const auto mouseLook_X = (int*)0x9E9620;
extern uint32_t *g_playerInHangar;

extern int g_iNaturalConcourseAnimations, g_iHUDOffscreenCommandsRendered;
extern bool g_bIsTrianglePointer, g_bLastTrianglePointer, g_bFixedGUI;
extern bool g_bYawPitchFromMouseOverride;
extern dc_element g_DCElements[];
extern int g_iNumDCElements;
extern DCHUDRegions g_DCHUDRegions;
extern move_region_coords g_DCMoveRegions;
extern char g_sCurrentCockpit[128];
//extern float g_fXWAScale;

extern VertexShaderCBuffer g_VSCBuffer;
extern PixelShaderCBuffer g_PSCBuffer;
extern DCPixelShaderCBuffer g_DCPSCBuffer;
extern float g_fAspectRatio, g_fGlobalScale, g_fBrightness, g_fGUIElemsScale, g_fHUDDepth, g_fFloatingGUIDepth;
extern float g_fCurScreenWidth, g_fCurScreenHeight, g_fCurScreenWidthRcp, g_fCurScreenHeightRcp;
extern D3D11_VIEWPORT g_nonVRViewport;

#include <headers/openvr.h>
const float PI = 3.141592f;
const float RAD_TO_DEG = 180.0f / PI;
extern float g_fPitchMultiplier, g_fYawMultiplier, g_fRollMultiplier;
extern float g_fYawOffset, g_fPitchOffset;
extern float g_fPosXMultiplier, g_fPosYMultiplier, g_fPosZMultiplier;
extern float g_fMinPositionX, g_fMaxPositionX;
extern float g_fMinPositionY, g_fMaxPositionY;
extern float g_fMinPositionZ, g_fMaxPositionZ;
extern Vector3 g_headCenter;
extern bool g_bResetHeadCenter, g_bSteamVRPosFromFreePIE, g_bReshadeEnabled;
extern vr::IVRSystem *g_pHMD;
extern int g_iFreePIESlot;
extern Matrix4 g_fullMatrixLeft, g_fullMatrixRight, g_fullMatrixHead;

// Dynamic Cockpit
// The following is used when the Dynamic Cockpit is enabled to render the HUD separately
//bool g_bHUDVerticesReady = false; // Set to true when the g_HUDVertices array has valid data

// Bloom
extern bool /* g_bDumpBloomBuffers, */ g_bDCManualActivate;
extern BloomConfig g_BloomConfig;
float g_fBloomLayerMult[8] = {
	1.000f, // 0
	1.025f, // 1
	1.030f, // 2
	1.035f, // 3
	1.045f, // 4
	1.055f, // 5
	1.070f, // 6
	1.100f, // 7
};
float g_fBloomSpread[8] = {
	2.0f, // 0
	3.0f, // 1
	4.0f, // 2
	4.0f, // 3
	4.0f, // 4
	4.0f, // 5
	4.0f, // 6
	4.0f, // 7
};
int g_iBloomPasses[8] = {
	1, 1, 1, 1, 1, 1, 1, 1
};

// SSAO
extern SSAOTypeEnum g_SSAO_Type;
extern float g_fSSAOZoomFactor, g_fSSAOWhitePoint, g_fNormWeight, g_fNormalBlurRadius;
extern int g_iSSDODebug, g_iSSAOBlurPasses;
extern bool g_bBlurSSAO, g_bDepthBufferResolved;
extern bool g_bShowSSAODebug, g_bEnableIndirectSSDO, g_bFNEnable;
extern bool g_bDumpSSAOBuffers, g_bEnableSSAOInShader, g_bEnableBentNormalsInShader;
extern Vector4 g_LightVector;

/*
 * Convert a rotation matrix to a normalized quaternion.
 * From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
 */
vr::HmdQuaternionf_t rotationToQuaternion(vr::HmdMatrix34_t m) {
	float tr = m.m[0][0] + m.m[1][1] + m.m[2][2];
	vr::HmdQuaternionf_t q;

	if (tr > 0) {
		float S = sqrt(tr + 1.0f) * 2.0f; // S=4*qw 
		q.w = 0.25f * S;
		q.x = (m.m[2][1] - m.m[1][2]) / S;
		q.y = (m.m[0][2] - m.m[2][0]) / S;
		q.z = (m.m[1][0] - m.m[0][1]) / S;
	}
	else if ((m.m[0][0] > m.m[1][1]) && (m.m[0][0] > m.m[2][2])) {
		float S = sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f; // S=4*qx 
		q.w = (m.m[2][1] - m.m[1][2]) / S;
		q.x = 0.25f * S;
		q.y = (m.m[0][1] + m.m[1][0]) / S;
		q.z = (m.m[0][2] + m.m[2][0]) / S;
	}
	else if (m.m[1][1] > m.m[2][2]) {
		float S = sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f; // S=4*qy
		q.w = (m.m[0][2] - m.m[2][0]) / S;
		q.x = (m.m[0][1] + m.m[1][0]) / S;
		q.y = 0.25f * S;
		q.z = (m.m[1][2] + m.m[2][1]) / S;
	}
	else {
		float S = sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f; // S=4*qz
		q.w = (m.m[1][0] - m.m[0][1]) / S;
		q.x = (m.m[0][2] + m.m[2][0]) / S;
		q.y = (m.m[1][2] + m.m[2][1]) / S;
		q.z = 0.25f * S;
	}
	float Q = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
	q.x /= Q;
	q.y /= Q;
	q.z /= Q;
	q.w /= Q;
	return q;
}

/*
   From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/index.htm
   yaw: left = +90, right = -90
   pitch: up = +90, down = -90
   roll: left = +90, right = -90

   if roll > 90, the axis will swap pitch and roll; but why would anyone do that?
*/
void quatToEuler(vr::HmdQuaternionf_t q, float *yaw, float *roll, float *pitch) {
	float test = q.x*q.y + q.z*q.w;

	if (test > 0.499f) { // singularity at north pole
		*yaw = 2 * atan2(q.x, q.w);
		*pitch = PI / 2.0f;
		*roll = 0;
		return;
	}
	if (test < -0.499f) { // singularity at south pole
		*yaw = -2 * atan2(q.x, q.w);
		*pitch = -PI / 2.0f;
		*roll = 0;
		return;
	}
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	*yaw = atan2(2.0f * q.y*q.w - 2.0f * q.x*q.z, 1.0f - 2.0f * sqy - 2.0f * sqz);
	*pitch = asin(2.0f * test);
	*roll = atan2(2.0f * q.x*q.w - 2.0f * q.y*q.z, 1.0f - 2.0f * sqx - 2.0f * sqz);
}

Matrix3 HmdMatrix34toMatrix3(const vr::HmdMatrix34_t &mat) {
	Matrix3 matrixObj(
		mat.m[0][0], mat.m[1][0], mat.m[2][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2]
	);
	return matrixObj;
}

// TODO: Load the signs from a config file so that it's easier to test this:
Matrix4 ComputeRotationMatrixFromXWAView() {
	// Compute the full rotation
	float yaw   = PlayerDataTable[0].yaw   / 65536.0f * 360.0f;
	float pitch = PlayerDataTable[0].pitch / 65536.0f * 360.0f;
	float roll  = PlayerDataTable[0].roll  / 65536.0f * 360.0f;
	/*while (yaw < 0) yaw += 360.0f;
	while (pitch < 0) pitch += 360.0f;
	while (roll < 0) roll += 360.0f;*/
	Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	rotMatrixFull.identity();
	rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
	rotMatrixPitch.identity(); rotMatrixPitch.rotateX(pitch);
	rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(-roll);
	rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixYaw;
	return rotMatrixFull;
}

void GetSteamVRPositionalData(float *yaw, float *pitch, float *roll, float *x, float *y, float *z, Matrix3 *rotMatrix)
{
	vr::TrackedDeviceIndex_t unDevice = vr::k_unTrackedDeviceIndex_Hmd;
	if (!g_pHMD->IsTrackedDeviceConnected(unDevice)) {
		//log_debug("[DBG] HMD is not connected");
		return;
	}

	vr::VRControllerState_t state;
	if (g_pHMD->GetControllerState(unDevice, &state, sizeof(state)))
	{
		vr::TrackedDevicePose_t trackedDevicePose;
		vr::HmdMatrix34_t poseMatrix;
		vr::HmdQuaternionf_t q;
		vr::ETrackedDeviceClass trackedDeviceClass = vr::VRSystem()->GetTrackedDeviceClass(unDevice);

		vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0, &trackedDevicePose, 1);
		poseMatrix = trackedDevicePose.mDeviceToAbsoluteTracking; // This matrix contains all positional and rotational data.
		q = rotationToQuaternion(trackedDevicePose.mDeviceToAbsoluteTracking);
		quatToEuler(q, yaw, pitch, roll);

		*x = poseMatrix.m[0][3];
		*y = poseMatrix.m[1][3];
		*z = poseMatrix.m[2][3];
		*rotMatrix = HmdMatrix34toMatrix3(poseMatrix);
	}
}

struct MainVertex
{
	float pos[2];
	float tex[2];
	MainVertex() {}
	MainVertex(float x, float y, float tx, float ty) {
		pos[0] = x; pos[1] = y;
		tex[0] = tx; tex[1] = ty;
	}
};

// Barrel Effect
BarrelPixelShaderCBuffer g_BarrelPSCBuffer;
extern float g_fLensK1, g_fLensK2, g_fLensK3;

// Bloom
BloomPixelShaderCBuffer g_BloomPSCBuffer;
SSAOPixelShaderCBuffer g_SSAO_PSCBuffer;
extern bool g_bBloomEnabled, g_bAOEnabled;
extern float g_fBloomAmplifyFactor;

// Main Pixel Shader constant buffer
MainShadersCBuffer g_MSCBuffer;
extern float g_fConcourseScale, g_fConcourseAspectRatio;
extern int g_iDraw2DCounter;
extern bool g_bStartedGUI, g_bPrevStartedGUI, g_bIsScaleableGUIElem, g_bPrevIsScaleableGUIElem, g_bScaleableHUDStarted, g_bDynCockpitEnabled;
extern bool g_bEnableVR, g_bDisableBarrelEffect;
extern bool g_bDumpGUI;
extern int g_iDrawCounter, g_iExecBufCounter, g_iPresentCounter, g_iNonZBufferCounter, g_iDrawCounterAfterHUD;
extern bool g_bTargetCompDrawn, g_bPrevIsFloatingGUI3DObject, g_bIsFloating3DObject;
extern unsigned int g_iFloatingGUIDrawnCounter;

bool g_bRendering3D = false; // Set to true when the system is about to render in 3D

// SteamVR
extern DWORD g_FullScreenWidth, g_FullScreenHeight;
extern Matrix4 g_viewMatrix;
extern VertexShaderMatrixCB g_VSMatrixCB;


extern HeadPos g_HeadPosAnim, g_HeadPos;
void animTickX();
void animTickY();
void animTickZ();

MainVertex g_BarrelEffectVertices[6] = {
	MainVertex(-1, -1, 0, 1),
	MainVertex(1, -1, 1, 1),
	MainVertex(1,  1, 1, 0),

	MainVertex(1,  1, 1, 0),
	MainVertex(-1,  1, 0, 0),
	MainVertex(-1, -1, 0, 1),
};

extern bool g_bStart3DCapture, g_bDo3DCapture;
extern FILE *g_HackFile;
#ifdef DBR_VR
extern bool g_bCapture2DOffscreenBuffer;
#endif

/* SteamVR HMD */
#include <headers/openvr.h>
extern vr::IVRSystem *g_pHMD;
extern vr::IVRCompositor *g_pVRCompositor;
extern bool g_bSteamVREnabled, g_bUseSteamVR;
extern uint32_t g_steamVRWidth, g_steamVRHeight;
extern vr::TrackedDevicePose_t g_rTrackedDevicePose;
void *g_pSurface = NULL;
void WaitGetPoses();

// void capture()
//#ifdef DBR_VR
void PrimarySurface::capture(int time_delay, ComPtr<ID3D11Texture2D> buffer, const wchar_t *filename)
{
	bool bDoCapture = false;
	HRESULT hr = D3D_OK;

	hr = DirectX::SaveWICTextureToFile(this->_deviceResources->_d3dDeviceContext.Get(),
		buffer.Get(), GUID_ContainerFormatJpeg, filename);

	if (SUCCEEDED(hr))
		log_debug("[DBG] CAPTURED (1)!");
	else
		log_debug("[DBG] NOT captured, hr: %d", hr);
}
//#endif

PrimarySurface::PrimarySurface(DeviceResources* deviceResources, bool hasBackbufferAttached)
{
	this->_refCount = 1;
	this->_deviceResources = deviceResources;

	this->_hasBackbufferAttached = hasBackbufferAttached;

	if (this->_hasBackbufferAttached)
	{
		*this->_backbufferSurface.GetAddressOf() = new BackbufferSurface(this->_deviceResources);
		this->_deviceResources->_backbufferSurface = this->_backbufferSurface;
	}

	this->_flipFrames = 0;
}

PrimarySurface::~PrimarySurface()
{
	if (this->_hasBackbufferAttached)
	{
		for (ULONG ref = this->_backbufferSurface->AddRef(); ref > 1; ref--)
		{
			this->_backbufferSurface->Release();
		}
	}

	if (this->_deviceResources->_primarySurface == this)
	{
		this->_deviceResources->_primarySurface = nullptr;
	}
}

HRESULT PrimarySurface::QueryInterface(
	REFIID riid,
	LPVOID* obp
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tE_NOINTERFACE");
	LogText(str.str());
#endif

	return E_NOINTERFACE;
}

ULONG PrimarySurface::AddRef()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	this->_refCount++;

#if LOGGER
	str.str("");
	str << "\t" << this->_refCount;
	LogText(str.str());
#endif

	return this->_refCount;
}

ULONG PrimarySurface::Release()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	this->_refCount--;

#if LOGGER
	str.str("");
	str << "\t" << this->_refCount;
	LogText(str.str());
#endif

	if (this->_refCount == 0)
	{
		delete this;
		return 0;
	}

	return this->_refCount;
}

HRESULT PrimarySurface::AddAttachedSurface(
	LPDIRECTDRAWSURFACE lpDDSAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::AddOverlayDirtyRect(
	LPRECT lpRect
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DWORD dwFlags,
	LPDDBLTFX lpDDBltFx
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	str << tostr_RECT(lpDestRect);

	if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else
	{
		str << " " << lpDDSrcSurface;
	}

	str << tostr_RECT(lpSrcRect);

	if ((dwFlags & DDBLT_COLORFILL) != 0 && lpDDBltFx != nullptr)
	{
		str << " " << (void*)lpDDBltFx->dwFillColor;
	}

	LogText(str.str());
#endif

	if (dwFlags & DDBLT_COLORFILL)
	{
		if (lpDDBltFx == nullptr)
		{
			return DDERR_INVALIDPARAMS;
		}

		return DD_OK;
	}

	if (lpDDSrcSurface != nullptr)
	{
		if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
		{
			return this->Flip(NULL, 0);
		}
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::BltBatch(
	LPDDBLTBATCH lpDDBltBatch,
	DWORD dwCount,
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::BltFast(
	DWORD dwX,
	DWORD dwY,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DWORD dwTrans
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	str << " " << dwX << " " << dwY;

	if (lpDDSrcSurface == nullptr)
	{
		str << " NULL";
	}
	else if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else
	{
		str << " " << lpDDSrcSurface;
	}

	str << tostr_RECT(lpSrcRect);

	if (dwTrans & DDBLTFAST_SRCCOLORKEY)
	{
		str << " SRCCOLORKEY";
	}

	if (dwTrans & DDBLTFAST_DESTCOLORKEY)
	{
		str << " DESTCOLORKEY";
	}

	if (dwTrans & DDBLTFAST_WAIT)
	{
		str << " WAIT";
	}

	if (dwTrans & DDBLTFAST_DONOTWAIT)
	{
		str << " DONOTWAIT";
	}

	LogText(str.str());
#endif

	if (lpDDSrcSurface != nullptr)
	{
		if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
		{
			return this->Flip(this->_deviceResources->_backbufferSurface, 0);
		}
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::DeleteAttachedSurface(
	DWORD dwFlags,
	LPDIRECTDRAWSURFACE lpDDSAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::EnumAttachedSurfaces(
	LPVOID lpContext,
	LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::EnumOverlayZOrders(
	DWORD dwFlags,
	LPVOID lpContext,
	LPDDENUMSURFACESCALLBACK lpfnCallback
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

/*
 * Applies the barrel distortion effect on the 2D window (Concourse, menus, etc).
 */
void PrimarySurface::barrelEffect2D(int iteration) {
	/*
	We need to avoid resolving the offscreen buffer multiple times. It's probably easier to
	skip method this altogether if we already rendered all this in the first iteration.
	*/
	if (iteration > 0)
		return;

	D3D11_VIEWPORT viewport{};
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	/* Create the VertexBuffer if necessary */
	if (resources->_barrelEffectVertBuffer == nullptr)
	{
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// This is probably an opportunity for an optimization: let's use the same topology everywhere?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// At this point, offscreenBuffer contains the image that would be rendered to the screen by
	// copying to the backbuffer. So, resolve the offscreen buffer into offscreenBuffer2 to use it
	// as input, so that we can render the image twice.
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, DXGI_FORMAT_B8G8R8A8_UNORM);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		if (frame == 0) {
			log_debug("[DBG] [Capture] display Width, Height: %d, %d",
				this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight);
			log_debug("[DBG] [Capture] backBuffer Width, Height: %d, %d",
				this->_deviceResources->_backbufferWidth, this->_deviceResources->_backbufferHeight);
		}
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d-A.jpg", frame++);
		capture(0, this->_deviceResources->_offscreenBuffer2, filename);
		if (frame >= 40)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Render the barrel effect
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_barrelPixelShader);
	
	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());

	resources->InitViewport(&viewport);
	context->Draw(6, 0);

	// Restore the original rendertargetview
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(), this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Applies the barrel distortion effect on the 3D window.
 * Resolves the offscreenBuffer into offscreenBufferAsInput
 * Renders to offscreenBufferPost
 */
void PrimarySurface::barrelEffect3D() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);
	
	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);
	
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Create a new viewport to render the offscreen buffer as a texture
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = (float)0;
	viewport.TopLeftY = (float)0;
	viewport.Width = (float)screen_res_x;
	viewport.Height = (float)screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, DXGI_FORMAT_B8G8R8A8_UNORM);

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_barrelPixelShader);
	
	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	// Clear the depth stencil
	// Maybe I should be using resources->clearDepth instead of 1.0f:
	context->ClearDepthStencilView(resources->_depthStencilViewL, D3D11_CLEAR_DEPTH, 1.0f, 0);
	// Clear the render target
	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
	resources->InitViewport(&viewport);
	context->IASetInputLayout(resources->_mainInputLayout);
	context->Draw(6, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf2-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer2, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf3-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer3, filename);

		frame++;
		if (frame >= 5)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(), 
		this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Applies the barrel distortion effect on the 3D window for SteamVR (so each image is
 * independent and the singleBarrelPixelShader is used.
 * Input: _offscreenBuffer and _offscreenBufferR
 * Output: _offscreenBufferPost and _offscreenBufferPostR
 */
void PrimarySurface::barrelEffectSteamVR() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;
		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	D3D11_VIEWPORT viewport{};
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	viewport.TopLeftX = (float)0;
	viewport.TopLeftY = (float)0;
	viewport.Width = (float)screen_res_x;
	viewport.Height = (float)screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Resolve both images
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, DXGI_FORMAT_B8G8R8A8_UNORM);
	context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
		0, DXGI_FORMAT_B8G8R8A8_UNORM);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		log_debug("[DBG] Capturing buffers");
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInput-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);

		frame++;
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_singleBarrelPixelShader);

	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	// Clear the depth stencil and render target
	//context->ClearDepthStencilView(resources->_depthStencilViewL, D3D11_CLEAR_DEPTH, resources->clearDepth, 0);
	//context->ClearDepthStencilView(resources->_depthStencilViewR, D3D11_CLEAR_DEPTH, resources->clearDepth, 0);
	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
	context->IASetInputLayout(resources->_mainInputLayout);

	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
		//resources->_depthStencilViewL.Get());
	context->Draw(6, 0);

	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
	context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
		//resources->_depthStencilViewR.Get());
	context->Draw(6, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferR, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInputR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferAsInputR, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufPostR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferPostR, filename);

		frame++;
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
}

/*
 * I tried to create a custom mesh for the 2D present; but it introduced perspective
 * distortion (why?)
 */
HRESULT PrimarySurface::buildSteamVRResizeMesh2D() {
	HRESULT hr = S_OK;

	// Top-left
	// Top-right
	// Bot-right
	// Bot-left
	MainVertex vertices[4] =
	{
		MainVertex(-1.4f, -1.3f, 0, 1),
		MainVertex( 0.8f,    -1, 1, 1),
		MainVertex( 0.8f,     1, 1, 0),
		MainVertex(-1.4f,  1.3f, 0, 0),
	};

	D3D11_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(vertices);
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;
	vertexBufferData.pSysMem = vertices;
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;

	return _deviceResources->_d3dDevice->CreateBuffer(&vertexBufferDesc,
		&vertexBufferData, this->_deviceResources->_steamVRPresentVertexBuffer.GetAddressOf());
}

/*
 * When rendering for SteamVR, we're usually rendering at half the width; but the Present is done
 * at full resolution, so we need to resize the offscreenBuffer before presenting it.
 * Input: _offscreenBuffer
 * Output: _steamVRPresentBuffer
 */
void PrimarySurface::resizeForSteamVR(int iteration, bool is_2D) {
	/*
	We need to avoid resolving the offscreen buffer multiple times. It's probably easier to
	skip method this altogether if we already rendered all this in the first iteration.
	*/
	if (iteration > 0)
		return;

	D3D11_VIEWPORT viewport{};
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	/*
	if (resources->_steamVRPresentVertexBuffer == NULL) {
		HRESULT hr = buildSteamVRResizeMesh2D();
		if (FAILED(hr)) {
			log_debug("[DBG] SteamVR resize mesh failed, using default _mainVertexBuffer instead");
			resources->_steamVRPresentVertexBuffer = resources->_mainVertexBuffer;
		}
		else
			log_debug("[DBG] Using custom resize mesh");
	}
	*/

	float screen_res_x = (float)g_FullScreenWidth;
	float screen_res_y = (float)g_FullScreenHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_mainVertexBuffer.GetAddressOf(), &stride, &offset);
	//resources->InitVertexBuffer(resources->_steamVRPresentVertexBuffer.GetAddressOf(), &stride, &offset);
	resources->InitIndexBuffer(resources->_mainIndexBuffer);

	// Set Primitive Topology
	// This is probably an opportunity for an optimization: let's use the same topology everywhere?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// At this point, offscreenBuffer contains the image that would be rendered to the screen by
	// copying to the backbuffer. So, resolve the offscreen buffer into offscreenBufferAsInput to
	// use it as input
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, DXGI_FORMAT_B8G8R8A8_UNORM);

	// Resize the buffer to be presented for SteamVR
	float scale_x = screen_res_x / g_steamVRWidth;
	float scale_y = screen_res_y / g_steamVRHeight;
	float scale = (scale_x + scale_y);
	if (!is_2D)
		scale *= 0.5f;
		//scale *= 0.75f; // HACK: Use this for Trinus PSVR
	
	float newWidth = g_steamVRWidth * scale; // Use this when not running Trinus
	//float newWidth = g_steamVRWidth * scale * 0.5f; // HACK: Use this for Trinus PSVR
	float newHeight = g_steamVRHeight * scale;

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		if (frame == 0) {
			log_debug("[DBG] [Capture] display Width, Height: %d, %d",
				this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight);
			log_debug("[DBG] [Capture] backBuffer Width, Height: %d, %d",
				this->_deviceResources->_backbufferWidth, this->_deviceResources->_backbufferHeight);
			log_debug("[DBG] screen_res_x,y: %0.1f, %0.1f", screen_res_x, screen_res_y);
			log_debug("[DBG] g_steamVRWidth,Height: %d, %d", g_steamVRWidth, g_steamVRHeight);
			log_debug("[DBG] scale, newWidth,Height: %0.3f, (%0.3f, %0.3f)", scale, newWidth, newHeight);
		}
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d-A.jpg", frame++);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	//viewport.TopLeftX = (screen_res_x - g_steamVRWidth) / 2.0f;
	/*
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)g_steamVRWidth;
	viewport.Height = (float)g_steamVRHeight;
	*/

	viewport.TopLeftX = (screen_res_x - newWidth) / 2.0f;
	viewport.TopLeftY = (screen_res_y - newHeight) / 2.0f;
	viewport.Width = (float)newWidth;
	viewport.Height = (float)newHeight;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	resources->InitPSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f);
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Don't use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_basicPixelShader);

	context->ClearDepthStencilView(resources->_depthStencilViewL, D3D11_CLEAR_DEPTH, resources->clearDepth, 0);
	context->ClearRenderTargetView(resources->_renderTargetViewSteamVRResize, bgColor);
	context->OMSetRenderTargets(1, resources->_renderTargetViewSteamVRResize.GetAddressOf(),
		resources->_depthStencilViewL.Get());
	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	context->DrawIndexed(6, 0, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInput-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);

		swprintf_s(filename, 120, L"c:\\temp\\steamVRPresentBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_steamVRPresentBuffer, filename);

		log_debug("[DBG] viewport: (%f,%f)-(%f,%f)", viewport.TopLeftX, viewport.TopLeftY,
			viewport.Width, viewport.Height);

		frame++;
		if (frame >= 0)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
}


/*
 * Applies the bloom effect on the 3D window.
 * pass 0:
 *		Input: an already-resolved _offscreenBufferAsInputReshade
 *		Renders to _reshadeOutput1
 * pass 1:
 *		Input: an already-resolved _reshadeOutput1
 *		Renders to _reshadeOutput2
 */

 /*
  * Applies a single bloom effect pass. Depending on the input:
  * pass = 0: Initial horizontal blur pass from the bloom mask.
  * pass = 1: Vertical pass from internal temporary ping-pong buffer.
  * pass = 2: Horizontal pass from internal temporary ping-pong buffer.
  * pass = 3: Final combine pass (deprecated, used for 32-bit UNORM mode)
  * pass = 4: Linear add between current bloom pass and bloomSum
  * pass = 5: Final combine between bloomSum and offscreenBuffer
  *
  * For pass 0, the input texture must be resolved already to 
  * _offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR
  */
void PrimarySurface::BloomBasicPass(int pass, float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	if (pass < 3) {
		viewport.Width  = screen_res_x / fZoomFactor;
		viewport.Height = screen_res_y / fZoomFactor;
	} else { // The final pass should be performed at full resolution
		viewport.Width  = screen_res_x;
		viewport.Height = screen_res_y;
	}

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	// The input texture must be resolved already to
	// _offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR
	switch (pass) {
		case 0: 	// Horizontal Gaussian Blur
			// Input: _offscreenAsInputReshadeSRV
			// Output _bloomOutput1
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputBloomMaskSRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 1: // Vertical Gaussian Blur
			// Input:  _bloomOutput1
			// Output: _bloomOutput2
			resources->InitPixelShader(resources->_bloomVGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput1SRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom2, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom2.GetAddressOf(), NULL);
			break;
		case 2: // Horizontal Gaussian Blur
			// Input:  _bloomOutput2
			// Output: _bloomOutput1
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 3: // Final pass to combine the bloom texture with the offscreenBuffer
			// Input:  _bloomOutput2, _offscreenBufferAsInput
			// Output: _offscreenBuffer (_bloomOutput1?)
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
			break;
		case 4:
			// Input: _bloomOutput2, _bloomSum
			// Output: _bloomOutput1
			resources->InitPixelShader(resources->_bloomBufferAddPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 5: // Final pass to combine the bloom accumulated texture with the offscreenBuffer
			// Input:  _bloomSum, _offscreenBufferAsInput
			// Output: _offscreenBuffer
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV.GetAddressOf());
			/*
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetView.Get(),
				resources->_renderTargetViewBloom1.Get()
			};
			context->OMSetRenderTargets(2, rtvs, NULL);
			*/
			context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
			break;
	}
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		switch (pass) {
		case 0: 	// Prepass
			// Input: _offscreenAsInputReshadeSRV_R
			// Output _bloomOutput1R
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputBloomMaskSRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 1: // Vertical Gaussian Blur
			// Input:  _bloomOutput1R
			// Output: _bloomOutput2R
			resources->InitPixelShader(resources->_bloomVGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput1SRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom2R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom2R.GetAddressOf(), NULL);
			break;
		case 2: // Horizontal Gaussian Blur
			// Input:  _bloomOutput2R
			// Output: _bloomOutput1R
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 3: // Final pass to combine the bloom texture with the backbuffer
			// Input:  _bloomOutput2R, _offscreenBufferAsInputR
			// Output: _offscreenBufferR
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			break;
		case 4:
			// Input: _bloomOutput2R, _bloomSumR
			// Output: _bloomOutput1R
			resources->InitPixelShader(resources->_bloomBufferAddPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 5: // Final pass to combine the bloom accumulated texture with the offscreenBuffer
			// Input:  _bloomSumR, _offscreenBufferAsInputR
			// Output: _offscreenBufferR
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV_R.GetAddressOf());
			/*
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewR.Get(),
				resources->_renderTargetViewBloom1R.Get()
			};
			context->OMSetRenderTargets(2, rtvs, NULL);
			*/
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			break;
		}

		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width  = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Performs a full bloom blur pass to build a pyramidal bloom. This function
 * calls BloomBasicPass internally several times.
 * Input: Bloom mask (_offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR)
 * Output: 
 *		_offscreenBuffer (with accumulated bloom)
 *		_offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR (blurred and downsampled from this pass)
 */
void PrimarySurface::BloomPyramidLevelPass(int PyramidLevel, int AdditionalPasses, float fZoomFactor, bool debug=false) {
	auto &resources = this->_deviceResources;
	auto &context = resources->_d3dDeviceContext;
	float fPixelScale = g_fBloomSpread[PyramidLevel];
	float fFirstPassZoomFactor = fZoomFactor / 2.0f;

	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp  / fFirstPassZoomFactor;
	g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.amplifyFactor		= 1.0f / fFirstPassZoomFactor;
	g_BloomPSCBuffer.bloomStrength		= g_fBloomLayerMult[PyramidLevel];
	g_BloomPSCBuffer.saturationStrength = g_BloomConfig.fSaturationStrength;
	g_BloomPSCBuffer.uvStepSize			= g_BloomConfig.uvStepSize1;
	//g_BloomPSCBuffer.uvStepSize			= 1.5f;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_offscreenInputBloomMask-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// Initial Horizontal Gaussian Blur from Masked Buffer. input: reshade mask, output: bloom1
	// This pass will downsample the image according to fViewportDivider:
	BloomBasicPass(0, fZoomFactor);
	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom1-pass0-level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput1, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// Second Vertical Gaussian Blur: adjust the pixel size since this image was downsampled in
	// the previous pass:
	g_BloomPSCBuffer.pixelSizeX		= fPixelScale * g_fCurScreenWidthRcp / fZoomFactor;
	g_BloomPSCBuffer.pixelSizeY		= fPixelScale * g_fCurScreenHeightRcp / fZoomFactor;
	// The UVs should now go to half the original range because the image was downsampled:
	g_BloomPSCBuffer.amplifyFactor	= 1.0f / fZoomFactor;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
	// Vertical Gaussian Blur. input: bloom1, output: bloom2
	BloomBasicPass(1, fZoomFactor);
	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom2-pass1-level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput2, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	for (int i = 0; i < AdditionalPasses; i++) {
		// Alternating between 2.0 and 1.5 avoids banding artifacts
		//g_BloomPSCBuffer.uvStepSize = (i % 2 == 0) ? 2.0f : 1.5f;
		//g_BloomPSCBuffer.uvStepSize = 1.5f + (i % 3) * 0.7f;
		g_BloomPSCBuffer.uvStepSize = (i % 2 == 0) ? g_BloomConfig.uvStepSize2 : g_BloomConfig.uvStepSize1;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
		// Horizontal Gaussian Blur. input: bloom2, output: bloom1
		BloomBasicPass(2, fZoomFactor);
		// Vertical Gaussian Blur. input: bloom1, output: bloom2
		BloomBasicPass(1, fZoomFactor);
	}
	// The blur output will *always* be in bloom2, let's copy it to the bloom masks to reuse it for the
	// next pass:
	context->CopyResource(resources->_offscreenBufferAsInputBloomMask, resources->_bloomOutput2);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferAsInputBloomMaskR, resources->_bloomOutput2R);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom2Buffer-Level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput2, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
	
	// Combine. input: offscreenBuffer (will be resolved), bloom2; output: offscreenBuffer/bloom1
	//BloomBasicPass(3, fZoomFactor);

	// Accummulate the bloom buffer, input: bloom2, bloomSum; output: bloom1
	BloomBasicPass(4, fZoomFactor);

	// Copy _bloomOutput1 over _bloomOutputSum
	context->CopyResource(resources->_bloomOutputSum, resources->_bloomOutput1);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_bloomOutputSumR, resources->_bloomOutput1R);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloomOutputSum-Level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutputSum, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// To make this step compatible with the rest of the code, we need to copy the results
	// to offscreenBuffer and offscreenBufferR (in SteamVR mode).
	/*context->CopyResource(resources->_offscreenBuffer, resources->_bloomOutput1);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_bloomOutput1R);*/
}

void PrimarySurface::ClearBox(uvfloat4 box, D3D11_VIEWPORT *viewport, D3DCOLOR clearColor) {
	HRESULT hr;
	auto& resources = _deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = resources->InitBlendState(nullptr, &blendDesc);

	// Set the vertex buffer
	// D3DCOLOR seems to be AARRGGBB
	D3DTLVERTEX vertices[6];
	vertices[0].sx = box.x0; vertices[0].sy = box.y0; vertices[0].sz = 0.98f; vertices[0].rhw = 34.0f; vertices[0].color = clearColor;
	vertices[1].sx = box.x1; vertices[1].sy = box.y0; vertices[1].sz = 0.98f; vertices[1].rhw = 34.0f; vertices[1].color = clearColor;
	vertices[2].sx = box.x1; vertices[2].sy = box.y1; vertices[2].sz = 0.98f; vertices[2].rhw = 34.0f; vertices[2].color = clearColor;

	vertices[3].sx = box.x1; vertices[3].sy = box.y1; vertices[3].sz = 0.98f; vertices[3].rhw = 34.0f; vertices[3].color = clearColor;
	vertices[4].sx = box.x0; vertices[4].sy = box.y1; vertices[4].sz = 0.98f; vertices[4].rhw = 34.0f; vertices[4].color = clearColor;
	vertices[5].sx = box.x0; vertices[5].sy = box.y0; vertices[5].sz = 0.98f; vertices[5].rhw = 34.0f; vertices[5].color = clearColor;

	D3D11_MAPPED_SUBRESOURCE map;
	hr = context->Map(resources->_clearHUDVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (SUCCEEDED(hr)) {
		memcpy(map.pData, vertices, sizeof(D3DTLVERTEX) * 6);
		context->Unmap(resources->_clearHUDVertexBuffer, 0);
	}

	D3D11_DEPTH_STENCIL_DESC zDesc;
	// Temporarily disable ZWrite: we won't need it to display the HUD
	ComPtr<ID3D11DepthStencilState> depthState;
	zDesc.DepthEnable = FALSE;
	zDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	zDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	zDesc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &zDesc);

	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitRasterizerState(resources->_rasterizerState);

	// Change the shaders
	resources->InitVertexShader(resources->_passthroughVertexShader);
	resources->InitPixelShader(resources->_pixelShaderClearBox);
	// Change the render target
	// Set two RTVs: one for the foreground HUD and one for the HUD background
	// Binding 2 RTVs implies that _pixelShaderSolid can output to 2 SV_TARGETs
	ID3D11RenderTargetView *rtvs[2] = {
		resources->_renderTargetViewDynCockpitAsInput.Get(),
		resources->_renderTargetViewDynCockpitAsInputBG.Get() 
	};
	context->OMSetRenderTargets(2, rtvs, NULL);

	// Set the viewport
	resources->InitViewport(viewport);
	// Set the vertex buffer (map the vertices from the box)
	UINT stride = sizeof(D3DTLVERTEX);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_clearHUDVertexBuffer.GetAddressOf(), &stride, &offset);
	// Draw
	context->Draw(6, 0);
}

void PrimarySurface::ClearHUDRegions() {
	D3D11_VIEWPORT viewport = { 0 };
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = (float )_deviceResources->_backbufferWidth;
	viewport.Height   = (float )_deviceResources->_backbufferHeight;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;

	int size = g_iNumDCElements;
	for (int i = 0; i < size; i++) {
		dc_element *dc_elem = &g_DCElements[i];
		if (g_sCurrentCockpit[0] != 0 && !dc_elem->bNameHasBeenTested)
		{
			if (strstr(dc_elem->name, g_sCurrentCockpit) != NULL) {
				dc_elem->bActive = true;
				dc_elem->bNameHasBeenTested = true;
				//log_debug("[DBG] [DC] ACTIVATED: '%s'", dc_elem->name);
			}
		}
		// Only clear HUD regions for active dc_elements
		if (!dc_elem->bActive)
			continue;

		if (dc_elem->num_erase_slots == 0)
			continue;

		for (int j = 0; j < dc_elem->num_erase_slots; j++) {
			int erase_slot = dc_elem->erase_slots[j];
			DCHUDRegion *dcSrcBox = &g_DCHUDRegions.boxes[erase_slot];
			if (dcSrcBox->bLimitsComputed)
				ClearBox(dcSrcBox->erase_coords, &viewport, 0x0);
		}
	}
}

/*
 * Renders the HUD foreground and background and applies the move_region 
 * commands if DC is enabled
 */
void PrimarySurface::DrawHUDVertices() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	D3D11_VIEWPORT viewport;
	HRESULT hr;

	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = resources->InitBlendState(nullptr, &blendDesc);

	// We don't need to clear the current vertex and pixel constant buffers.
	// Since we've just finished rendering 3D, they should contain values that
	// can be reused. So let's just overwrite those values that we need.
	g_VSCBuffer.aspect_ratio      =  g_fAspectRatio;
	g_VSCBuffer.z_override        = -1.0f;
	g_VSCBuffer.sz_override       = -1.0f;
	g_VSCBuffer.mult_z_override   = -1.0f;
	g_VSCBuffer.cockpit_threshold = -1.0f;
	g_VSCBuffer.bPreventTransform =  0.0f;
	g_VSCBuffer.bFullTransform    =  0.0f;
	if (g_bEnableVR) {
		g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
		g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
	}
	else {
		g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
		g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
	}
	//g_VSCBuffer.viewportScale[2] = 1.0f; // scale;
	//g_VSCBuffer.viewportScale[3] = g_fGlobalScale;

	// Reduce the scale for GUI elements, except for the HUD
	g_VSCBuffer.viewportScale[3] = g_fGUIElemsScale;
	// Enable/Disable the fixed GUI
	g_VSCBuffer.bFullTransform = g_bFixedGUI ? 1.0f : 0.0f;
	// Since the HUD is all rendered on a flat surface, we lose vrparams that make the 3D object
	// and text float
	g_VSCBuffer.z_override = g_fFloatingGUIDepth;

	//g_PSCBuffer.brightness        = g_fBrightness;
	g_PSCBuffer.brightness		  = 1.0f; // TODO: Check if g_fBrightness is already applied when the textures are rendered
	g_PSCBuffer.bUseCoverTexture  = 0;
	//g_PSCBuffer.bRenderHUD		  = 1;
	// Add the move_regions commands.
	int numCoords = 0;
	if (g_bDynCockpitEnabled) {
		for (int i = 0; i < g_DCMoveRegions.numCoords; i++) {
			int region_slot = g_DCMoveRegions.region_slot[i];
			// Skip invalid src slots
			if (region_slot < 0)
				continue;
			// Skip regions if their limits haven't been computed
			if (!g_DCHUDRegions.boxes[region_slot].bLimitsComputed)
				continue;
			// Fetch the source uv coords:
			g_DCPSCBuffer.src[numCoords] = g_DCHUDRegions.boxes[region_slot].erase_coords;
			// Fetch the destination uv coords:
			g_DCPSCBuffer.dst[numCoords] = g_DCMoveRegions.dst[i];
			numCoords++;
		}
	}
	g_PSCBuffer.DynCockpitSlots = numCoords;

	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);

	UINT stride = sizeof(D3DTLVERTEX);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_HUDVertexBuffer.GetAddressOf(), &stride, &offset);
	resources->InitInputLayout(resources->_inputLayout);
	if (g_bEnableVR)
		resources->InitVertexShader(resources->_sbsVertexShader);
	else
		// The original code used _vertexShader:
		resources->InitVertexShader(resources->_vertexShader);
	resources->InitPixelShader(resources->_pixelShaderHUD);
	//resources->InitPixelShader(resources->_pixelShaderTexture);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitRasterizerState(resources->_rasterizerState);

	// Temporarily disable ZWrite: we won't need it to display the HUD
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Don't clear the render target, the offscreenBuffer already has the 3D render in it
	// Render the left image
	if (g_bUseSteamVR)
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
	else
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
	// VIEWPORT-LEFT
	if (g_bEnableVR) {
		if (g_bUseSteamVR)
			viewport.Width = (float)resources->_backbufferWidth;
		else
			viewport.Width = (float)resources->_backbufferWidth / 2.0f;
	}
	else // Non-VR path
		viewport.Width = (float)resources->_backbufferWidth;
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);
	// Set the left projection matrix
	g_VSMatrixCB.projEye = g_fullMatrixLeft;
	// The viewMatrix is set at the beginning of the frame
	resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
	// Set the HUD foreground and background textures:
	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputSRVDynCockpit.GetAddressOf());
	context->PSSetShaderResources(1, 1, resources->_offscreenAsInputSRVDynCockpitBG.GetAddressOf());
	// Draw the Left Image
	context->Draw(6, 0);

	if (!g_bEnableVR) // Shortcut for the non-VR path
		return;

	// Render the right image
	if (g_bUseSteamVR)
		context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
	else
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);

	// VIEWPORT-RIGHT
	if (g_bUseSteamVR) {
		viewport.Width = (float)resources->_backbufferWidth;
		viewport.TopLeftX = 0.0f;
	}
	else {
		viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		viewport.TopLeftX = 0.0f + viewport.Width;
	}
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);
	// Set the right projection matrix
	g_VSMatrixCB.projEye = g_fullMatrixRight;
	resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
	// Draw the Right Image
	context->Draw(6, 0);
}

void PrimarySurface::ComputeNormalsPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the constants used by the ComputeNormals shader
	float fPixelScale = 0.5f, fFirstPassZoomFactor = 1.0f;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp  / fFirstPassZoomFactor;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fFirstPassZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width  = screen_res_x / (1.0f * fZoomFactor);
	viewport.Height = screen_res_y / (1.0f * fZoomFactor);

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthAsInput
	// Output _normBuf
	//resources->InitPixelShader(resources->_computeNormalsPS);
	context->PSSetShaderResources(0, 1, resources->_depthBufSRV.GetAddressOf());
	context->ClearRenderTargetView(resources->_renderTargetViewNormBuf, bgColor);
	//ID3D11RenderTargetView *rtvs[2] = {
	//			resources->_renderTargetView.Get(), // DEBUG purposes only, remove later
	//			resources->_renderTargetViewNormBuf.Get()
	//};
	//context->OMSetRenderTargets(2, rtvs, NULL);
	context->OMSetRenderTargets(1, resources->_renderTargetViewNormBuf.GetAddressOf(), NULL);
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		// TODO: Check that this works in SteamVR
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthAsInputR
		// Output _normBufR
		context->PSSetShaderResources(0, 1, resources->_depthBufSRV_R.GetAddressOf());
		context->ClearRenderTargetView(resources->_renderTargetViewNormBufR, bgColor);
		context->OMSetRenderTargets(1, resources->_renderTargetViewNormBufR.GetAddressOf(), NULL);
		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

void PrimarySurface::SmoothNormalsPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the constants used by the ComputeNormals shader
	/*
	float fPixelScale = 0.5f, fFirstPassZoomFactor = 1.0f;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fFirstPassZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
	*/

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width  = screen_res_x / (1.0f * fZoomFactor);
	viewport.Height = screen_res_y / (1.0f * fZoomFactor);

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	float fPixelScale = 2.0f, fFirstPassZoomFactor = fZoomFactor / 2.0f;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp / fZoomFactor;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fZoomFactor;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	g_BloomPSCBuffer.enableSSAO = g_bEnableSSAOInShader;
	g_BloomPSCBuffer.enableBentNormals = g_bEnableBentNormalsInShader;
	//g_BloomPSCBuffer.norm_weight = g_fNormWeight;
	g_BloomPSCBuffer.depth_weight = g_SSAO_PSCBuffer.max_dist;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthAsInput
	// Output _normBuf
	resources->InitPixelShader(resources->_computeNormalsPS);
	// Copy bentBuf <- normBuf temporarily...
	context->CopyResource(resources->_bentBuf, resources->_normBuf);
	ID3D11ShaderResourceView *srvs[2] = {
		resources->_depthBufSRV.Get(),
		resources->_bentBufSRV.Get()
	};
	//ID3D11RenderTargetView *rtvs[2] = {
	//			resources->_renderTargetView.Get(), // DEBUG purposes only, remove later
	//			resources->_renderTargetViewNormBuf.Get()
	//};
	//context->OMSetRenderTargets(2, rtvs, NULL);
	//context->OMSetRenderTargets(1, resources->_renderTargetViewNormBuf.GetAddressOf(), NULL);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL); // DEBUG

	context->PSSetShaderResources(0, 2, srvs);
	context->ClearRenderTargetView(resources->_renderTargetViewNormBuf, bgColor);
	
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		// TODO: Check that this works in SteamVR
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthAsInputR
		// Output _normBufR
		context->PSSetShaderResources(0, 1, resources->_depthBufSRV_R.GetAddressOf());
		context->ClearRenderTargetView(resources->_renderTargetViewNormBufR, bgColor);
		context->OMSetRenderTargets(1, resources->_renderTargetViewNormBufR.GetAddressOf(), NULL);
		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

void PrimarySurface::SSAOPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = screen_res_x / fZoomFactor;
	viewport.Height = screen_res_y / fZoomFactor;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Compute the full rotation
	float yaw = PlayerDataTable[0].yaw / 65536.0f * 360.0f;
	float pitch = PlayerDataTable[0].pitch / 65536.0f * 360.0f;
	float roll = PlayerDataTable[0].roll / 65536.0f * 360.0f;
	Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	rotMatrixFull.identity();
	rotMatrixYaw.identity();   rotMatrixYaw.rotateY(yaw);
	rotMatrixPitch.identity(); rotMatrixPitch.rotateX(pitch);
	rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(-roll);
	rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixYaw;
	PixelShaderMatrixCB matrixCB;
	//rotMatrixFull.invert();
	matrixCB.viewMat = rotMatrixFull;
	resources->InitPSConstantBufferMatrix(resources->_PSMatrixBuffer.GetAddressOf(), &matrixCB);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	// Set the SSAO pixel shader constant buffer
	g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth;
	g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight;
	resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

	// Set the layout
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);


	// SSAO Computation, Left Image
	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
	// Output _ssaoBuf, _bentBuf
	{
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, DXGI_FORMAT_B8G8R8A8_UNORM);
		ID3D11ShaderResourceView *srvs_pass1[4] = {
			resources->_depthBufSRV.Get(),
			resources->_depthBuf2SRV.Get(),
			resources->_normBufSRV.Get(),
			resources->_offscreenAsInputShaderResourceView
		};
		resources->InitPixelShader(resources->_ssaoPS);
		if (!g_bBlurSSAO && g_bShowSSAODebug) {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetView.Get(),
				resources->_renderTargetViewBentBuf.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetView, bgColor);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, bgColor);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 4, srvs_pass1);
			context->Draw(6, 0);
			goto out1;
		}
		else {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewSSAO.Get(),
				resources->_renderTargetViewBentBuf.Get()
			};
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO, bgColor);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, bgColor);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 4, srvs_pass1);
			context->Draw(6, 0);
		}
	}

	// Setup the constant buffers to upscale the buffers
	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	float fPixelScale = fZoomFactor;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	g_BloomPSCBuffer.enableSSAO = g_bEnableSSAOInShader;
	g_BloomPSCBuffer.enableBentNormals = g_bEnableBentNormalsInShader;
	g_BloomPSCBuffer.depth_weight = g_SSAO_PSCBuffer.max_dist;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// SSAO Blur, Left Image
	// input: offscreenAsInput (with a copy of the ssaoBuf), depthBuf, bentBufR (with a copy of bentBuf), normBuf
	// output: ssaoBuf, bentBuf
	if (g_bBlurSSAO) {
		resources->InitPixelShader(resources->_ssaoBlurPS);
		// Copy the SSAO buffer to offscreenBufferAsInput -- we'll use it as temp buffer
		// to blur the SSAO buffer
		context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssaoBuf);
		// Here I'm reusing bentBufR as a temporary buffer for bentBuf, in the SteamVR path I'll do
		// the opposite. This is just to avoid having to make a temporary buffer to blur the bent normals.
		context->CopyResource(resources->_bentBufR, resources->_bentBuf);
		// Clear the destination buffers: the blur will re-populate them
		context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), bgColor);
		context->ClearRenderTargetView(resources->_renderTargetViewBentBuf.Get(), bgColor);
		ID3D11ShaderResourceView *srvs[5] = {
				resources->_offscreenAsInputShaderResourceView.Get(),
				resources->_depthBufSRV.Get(),
				resources->_depthBuf2SRV.Get(),
				resources->_normBufSRV.Get(),
				resources->_bentBufSRV_R.Get(),
		};
		if (g_bShowSSAODebug) {
			context->ClearRenderTargetView(resources->_renderTargetView, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
			context->PSSetShaderResources(0, 5, srvs);
			// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
			//context->PSSetShaderResources(0, 1, resources->_bentBufSRV.GetAddressOf());
			// DEBUG: Enable the following line to display the normals
			//context->PSSetShaderResources(0, 1, resources->_normBufSRV.GetAddressOf());
			context->Draw(6, 0);
			goto out1;
		}
		else {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewSSAO.Get(),
				resources->_renderTargetViewBentBuf.Get()
			};
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 5, srvs);
			context->Draw(6, 0);
		}
	}

	// Final combine, Left Image
	{
		// input: offscreenAsInput (resolved here), bloomMask, ssaoBuf
		// output: offscreenBuf
		// Reset the viewport for the final SSAO combine
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = screen_res_x;
		viewport.Height   = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);
		ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
		context->PSSetShaderResources(0, 4, null_srvs4);
		// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
		// so that it can be used as an SRV
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
		resources->InitPixelShader(resources->_ssaoAddPS);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, DXGI_FORMAT_B8G8R8A8_UNORM);
		ID3D11ShaderResourceView *srvs_pass2[6] = {
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_offscreenAsInputBloomMaskSRV.Get(),
			resources->_ssaoBufSRV.Get(),
			resources->_ssaoMaskSRV.Get(),
			resources->_bentBufSRV.Get(),
			resources->_normBufSRV.Get()
		};
		context->PSSetShaderResources(0, 6, srvs_pass2);
		context->Draw(6, 0);
	}

out1:
	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screen_res_x / fZoomFactor;
		viewport.Height = screen_res_y / fZoomFactor;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// SSAO Computation, right eye
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
		// Output _ssaoBuf, _bentBuf
		{
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			ID3D11ShaderResourceView *srvs_pass1[4] = {
				resources->_depthBufSRV_R.Get(),
				resources->_depthBuf2SRV_R.Get(),
				resources->_normBufSRV_R.Get(),
				resources->_offscreenAsInputShaderResourceViewR
			};
			resources->InitPixelShader(resources->_ssaoPS);
			if (!g_bBlurSSAO && g_bShowSSAODebug) {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewR.Get(),
					resources->_renderTargetViewBentBufR.Get(),
				};
				context->ClearRenderTargetView(resources->_renderTargetViewR, bgColor);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, bgColor);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs_pass1);
				context->Draw(6, 0);
				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetViewBentBufR.Get()
				};
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, bgColor);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, bgColor);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs_pass1);
				context->Draw(6, 0);
			}
		}

		// Setup the constant buffers to upscale the buffers
		// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
		// size will be twice as big in the next pass due to the downsample, so we have to compensate
		// with a zoom factor:
		float fPixelScale = fZoomFactor;
		g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp;
		g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp;
		g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
		//g_BloomPSCBuffer.white_point = g_fSSAOWhitePoint;
		g_BloomPSCBuffer.uvStepSize = 1.0f;
		g_BloomPSCBuffer.enableSSAO = g_bEnableSSAOInShader;
		g_BloomPSCBuffer.enableBentNormals = g_bEnableBentNormalsInShader;
		//g_BloomPSCBuffer.norm_weight = g_fNormWeight;
		g_BloomPSCBuffer.depth_weight = g_SSAO_PSCBuffer.max_dist;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

		// SSAO Blur, Right Image
		// input: offscreenAsInputR (with a copy of the ssaoBufR), depthBufR, bentBuf (with a copy of bentBufR), normBufR
		// output: ssaoBufR, bentBufR
		if (g_bBlurSSAO)
		{
			resources->InitPixelShader(resources->_ssaoBlurPS);
			// Copy the SSAO buffer to offscreenBufferAsInput -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_offscreenBufferAsInputR, resources->_ssaoBufR);
			// Here I'm reusing bentBuf as a temporary buffer for bentBufR
			// This is just to avoid having to make a temporary buffer to blur the bent normals.
			context->CopyResource(resources->_bentBuf, resources->_bentBufR);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), bgColor);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBufR.Get(), bgColor);
			ID3D11ShaderResourceView *srvs[5] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(),
					resources->_depthBufSRV_R.Get(),
					resources->_depthBuf2SRV_R.Get(),
					resources->_normBufSRV_R.Get(),
					resources->_bentBufSRV.Get(),
			};
			if (g_bShowSSAODebug) {
				context->ClearRenderTargetView(resources->_renderTargetViewR, bgColor);
				context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
				context->PSSetShaderResources(0, 5, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV_R.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV_R.GetAddressOf());
				context->Draw(6, 0);

				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetViewBentBufR.Get()
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs);
				context->Draw(6, 0);
			}
		}

		// Final combine, Right Image
		{
			// input: offscreenAsInputR (resolved here), bloomMaskR, ssaoBufR
			// output: offscreenBufR
			// Reset the viewport for the final SSAO combine
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = screen_res_x;
			viewport.Height = screen_res_y;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);
			ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
			context->PSSetShaderResources(0, 4, null_srvs4);
			// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
			// so that it can be used as an SRV
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			resources->InitPixelShader(resources->_ssaoAddPS);
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			ID3D11ShaderResourceView *srvs_pass2[6] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(),
				resources->_offscreenAsInputBloomMaskSRV_R.Get(),
				resources->_ssaoBufSRV_R.Get(),
				resources->_ssaoMaskSRV_R.Get(),
				resources->_bentBufSRV_R.Get(),
				resources->_normBufSRV_R.Get()
			};
			context->PSSetShaderResources(0, 6, srvs_pass2);
			context->Draw(6, 0);
		}
	}

out2:
	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

void PrimarySurface::SSDOPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = screen_res_x / fZoomFactor;
	viewport.Height   = screen_res_y / fZoomFactor;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	
	PixelShaderMatrixCB matrixCB;
	//rotMatrixFull.invert();
	matrixCB.viewMat = ComputeRotationMatrixFromXWAView();
	Vector4 light = matrixCB.viewMat * g_LightVector;
	matrixCB.LightVector.x = light.x;
	matrixCB.LightVector.y = light.y;
	matrixCB.LightVector.z = light.z;
	resources->InitPSConstantBufferMatrix(resources->_PSMatrixBuffer.GetAddressOf(), &matrixCB);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	// Set the SSDO pixel shader constant buffer
	g_SSAO_PSCBuffer.screenSizeX   = g_fCurScreenWidth;
	g_SSAO_PSCBuffer.screenSizeY   = g_fCurScreenHeight;
	g_SSAO_PSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	g_SSAO_PSCBuffer.fn_enable     = g_bFNEnable;
	resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

	// Set the layout
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	
	// SSDO Direct Lighting, Left Image
	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
	// Output _ssaoBuf, _bentBuf
	{
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, DXGI_FORMAT_B8G8R8A8_UNORM);
		ID3D11ShaderResourceView *srvs_pass1[5] = {
			resources->_depthBufSRV.Get(),
			resources->_depthBuf2SRV.Get(),
			resources->_normBufSRV.Get(),
			resources->_offscreenAsInputShaderResourceView.Get(),
			NULL, // resources->_diffuseSRV.Get(),
		};
		resources->InitPixelShader(resources->_ssdoDirectPS);
		if (g_bShowSSAODebug && !g_bBlurSSAO && !g_bEnableIndirectSSDO) {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetView.Get(),
				resources->_renderTargetViewBentBuf.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetView, black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, black);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 5, srvs_pass1);
			context->Draw(6, 0);
			goto out1;
		}
		else {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewSSAO.Get(),
				resources->_renderTargetViewBentBuf.Get()
			};
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO, black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, black);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 5, srvs_pass1);
			context->Draw(6, 0);
		}
	}
	
	// Setup the constant buffers to upscale the buffers
	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	float fPixelScale = fZoomFactor;
	g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp;
	g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp;
	g_BloomPSCBuffer.amplifyFactor		= 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize			= 1.0f;
	g_BloomPSCBuffer.enableSSAO			= g_bEnableSSAOInShader;
	g_BloomPSCBuffer.enableBentNormals	= g_bEnableBentNormalsInShader;
	g_BloomPSCBuffer.depth_weight		= g_SSAO_PSCBuffer.max_dist;
	g_BloomPSCBuffer.debug				= g_iSSDODebug;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// SSDO Blur, Left Image
	// input: offscreenAsInput (with a copy of the ssaoBuf), depthBuf, bentBufR (with a copy of bentBuf), normBuf
	// output: ssaoBuf, bentBuf
	if (g_bBlurSSAO) 
		for (int i = 0; i < g_iSSAOBlurPasses; i++) {
			resources->InitPixelShader(resources->_ssdoBlurPS);
			// Copy the SSAO buffer to offscreenBufferAsInput -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssaoBuf);
			// Here I'm reusing bentBufR as a temporary buffer for bentBuf, in the SteamVR path I'll do
			// the opposite. This is just to avoid having to make a temporary buffer to blur the bent normals.
			context->CopyResource(resources->_bentBufR, resources->_bentBuf);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf.Get(), black);
			ID3D11ShaderResourceView *srvs[5] = {
					resources->_offscreenAsInputShaderResourceView.Get(),
					resources->_depthBufSRV.Get(),
					resources->_depthBuf2SRV.Get(),
					resources->_normBufSRV.Get(),
					resources->_bentBufSRV_R.Get(),
			};
			if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1 && !g_bEnableIndirectSSDO) {
				context->ClearRenderTargetView(resources->_renderTargetView, black);
				//context->OMSetRenderTargets(1, resources->_renderTargetViewSSAO.GetAddressOf(), NULL);
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetView.Get(), // resources->_renderTargetViewSSAO.Get(),
					resources->_renderTargetViewBentBuf.Get(),
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV.GetAddressOf());
				context->Draw(6, 0);
				goto out1;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO.Get(),
					resources->_renderTargetViewBentBuf.Get()
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs);
				context->Draw(6, 0);
			}
		}

	// SSDO Indirect Lighting, Left Image

	// Clear the Indirect SSDO buffer -- we have to do this regardless of whether the indirect
	// illumination is computed or not.
	context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthBuf, _depthBuf2, _normBuf, offscreenAsInput (with a copy of _ssaoBuf -- not anymore! Should I fix this?)
	// Output _ssaoBufR
	if (g_bEnableIndirectSSDO)
	{
		resources->InitPixelShader(resources->_ssdoIndirectPS);
		// Set the SSDO pixel shader constant buffer
		//g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth  / fZoomFactor; // Not used in the shader
		//g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight / fZoomFactor; // Not used in the shader
		g_SSAO_PSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
		g_SSAO_PSCBuffer.fn_enable     = g_bFNEnable;
		resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

		// Copy the SSAO buffer to offscreenBufferAsInput -- this is the accumulated
		// color + SSDO buffer
		// Not anymore! The accumulation of color + SSDO is now done in the Add shader.
		// What happens if I just send the regular color buffer instead?
		//context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssaoBuf);
		// Resolve offscreenBuf, we need the original color buffer and it may have been overwritten
		// in the previous steps
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, DXGI_FORMAT_B8G8R8A8_UNORM);
		ID3D11ShaderResourceView *srvs[5] = {
			resources->_depthBufSRV.Get(),  // FG Depth Buffer
			resources->_depthBuf2SRV.Get(), // BG Depth Buffer
			resources->_normBufSRV.Get(),   // Normal Buffer
			resources->_offscreenAsInputShaderResourceView.Get(), // Color Buffer
			resources->_ssaoBufSRV.Get(),   // Direct SSDO from previous pass
		};
		
		// DEBUG
		if (g_bShowSSAODebug && !g_bBlurSSAO && g_bEnableIndirectSSDO) {
			context->ClearRenderTargetView(resources->_renderTargetView, black);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetView.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
		}
		// DEBUG
		else {
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, black);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewSSAO_R.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
		}
		context->PSSetShaderResources(0, 5, srvs);
		context->Draw(6, 0);
		
	}

	// Blur the Indirect SSDO buffer
	if (g_bEnableIndirectSSDO && g_bBlurSSAO) {
		resources->InitPixelShader(resources->_ssdoBlurPS);
		for (int i = 0; i < g_iSSAOBlurPasses; i++) {
			// Copy the SSDO Indirect Buffer (ssaoBufR) to offscreenBufferAsInput -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssaoBufR);
			// Here I'm reusing bentBufR as a temporary buffer for bentBuf, in the SteamVR path I'll do
			// the opposite. This is just to avoid having to make a temporary buffer to blur the bent normals.
			//context->CopyResource(resources->_bentBufR, resources->_bentBuf);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);
			//context->ClearRenderTargetView(resources->_renderTargetViewBentBuf.Get(), bgColor);
			ID3D11ShaderResourceView *srvs[4] = {
					resources->_offscreenAsInputShaderResourceView.Get(), // ssaoBufR
					resources->_depthBufSRV.Get(),
					resources->_depthBuf2SRV.Get(),
					resources->_normBufSRV.Get(),
					//resources->_bentBufSRV_R.Get(),
			};
			if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1) {
				context->ClearRenderTargetView(resources->_renderTargetView, black);
				//context->OMSetRenderTargets(1, resources->_renderTargetViewSSAO_R.GetAddressOf(), NULL);
				ID3D11RenderTargetView *rtvs[2] = {
					//resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetView.Get(), // resources->_renderTargetViewBentBuf.Get(),
					NULL,
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV.GetAddressOf());
				context->Draw(6, 0);
				goto out1;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					NULL,
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs);
				context->Draw(6, 0);
			}
		}
	}

	// Final combine, Left Image
	{
		// input: offscreenAsInput (resolved here), bloomMask, ssaoBuf
		// output: offscreenBuf
		resources->InitPixelShader(resources->_ssdoAddPS);
		// Reset the viewport for the final SSAO combine
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screen_res_x;
		viewport.Height = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);
		//ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
		//context->PSSetShaderResources(0, 4, null_srvs4);
		// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
		// so that it can be used as an SRV
		ID3D11RenderTargetView *rtvs[4] = {
			resources->_renderTargetView.Get(),
			NULL, NULL, NULL
		};
		context->OMSetRenderTargets(4, rtvs, NULL);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, DXGI_FORMAT_B8G8R8A8_UNORM);
		ID3D11ShaderResourceView *srvs_pass2[5] = {
			resources->_offscreenAsInputShaderResourceView.Get(),	// Color buffer
			resources->_offscreenAsInputBloomMaskSRV.Get(),			// Bloom Mask
			resources->_ssaoBufSRV.Get(),							// SSDO Direct
			resources->_ssaoBufSRV_R.Get(),							// SSDO Indirect
			resources->_ssaoMaskSRV.Get(),							// SSAO Mask
			//resources->_bentBufSRV.Get(),
			//resources->_normBufSRV.Get()
		};
		context->PSSetShaderResources(0, 5, srvs_pass2);
		context->Draw(6, 0);
	}

out1:
	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screen_res_x / fZoomFactor;
		viewport.Height = screen_res_y / fZoomFactor;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// SSAO Computation, right eye
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
		// Output _ssaoBuf, _bentBuf
		{
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			ID3D11ShaderResourceView *srvs_pass1[4] = {
				resources->_depthBufSRV_R.Get(),
				resources->_depthBuf2SRV_R.Get(),
				resources->_normBufSRV_R.Get(),
				resources->_offscreenAsInputShaderResourceViewR
			};
			resources->InitPixelShader(resources->_ssaoPS);
			if (!g_bBlurSSAO && g_bShowSSAODebug) {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewR.Get(),
					resources->_renderTargetViewBentBufR.Get(),
				};
				context->ClearRenderTargetView(resources->_renderTargetViewR, black);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, black);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs_pass1);
				context->Draw(6, 0);
				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetViewBentBufR.Get()
				};
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, black);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, black);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs_pass1);
				context->Draw(6, 0);
			}
		}

		// Setup the constant buffers to upscale the buffers
		// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
		// size will be twice as big in the next pass due to the downsample, so we have to compensate
		// with a zoom factor:
		float fPixelScale = fZoomFactor;
		g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp;
		g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp;
		g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
		//g_BloomPSCBuffer.white_point = g_fSSAOWhitePoint;
		g_BloomPSCBuffer.uvStepSize = 1.0f;
		g_BloomPSCBuffer.enableSSAO = g_bEnableSSAOInShader;
		g_BloomPSCBuffer.enableBentNormals = g_bEnableBentNormalsInShader;
		//g_BloomPSCBuffer.norm_weight = g_fNormWeight;
		g_BloomPSCBuffer.depth_weight = g_SSAO_PSCBuffer.max_dist;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

		// SSAO Blur, Right Image
		// input: offscreenAsInputR (with a copy of the ssaoBufR), depthBufR, bentBuf (with a copy of bentBufR), normBufR
		// output: ssaoBufR, bentBufR
		if (g_bBlurSSAO)
		{
			resources->InitPixelShader(resources->_ssaoBlurPS);
			// Copy the SSAO buffer to offscreenBufferAsInput -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_offscreenBufferAsInputR, resources->_ssaoBufR);
			// Here I'm reusing bentBuf as a temporary buffer for bentBufR
			// This is just to avoid having to make a temporary buffer to blur the bent normals.
			context->CopyResource(resources->_bentBuf, resources->_bentBufR);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBufR.Get(), black);
			ID3D11ShaderResourceView *srvs[5] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(),
					resources->_depthBufSRV_R.Get(),
					resources->_depthBuf2SRV_R.Get(),
					resources->_normBufSRV_R.Get(),
					resources->_bentBufSRV.Get(),
			};
			if (g_bShowSSAODebug) {
				context->ClearRenderTargetView(resources->_renderTargetViewR, black);
				context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
				context->PSSetShaderResources(0, 5, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV_R.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV_R.GetAddressOf());
				context->Draw(6, 0);

				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetViewBentBufR.Get()
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs);
				context->Draw(6, 0);
			}
		}
	
		// Final combine, Right Image
		{
			// input: offscreenAsInputR (resolved here), bloomMaskR, ssaoBufR
			// output: offscreenBufR
			// Reset the viewport for the final SSAO combine
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = screen_res_x;
			viewport.Height = screen_res_y;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);
			ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
			context->PSSetShaderResources(0, 4, null_srvs4);
			// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
			// so that it can be used as an SRV
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			resources->InitPixelShader(resources->_ssaoAddPS);
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, DXGI_FORMAT_B8G8R8A8_UNORM);
			ID3D11ShaderResourceView *srvs_pass2[6] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(),
				resources->_offscreenAsInputBloomMaskSRV_R.Get(),
				resources->_ssaoBufSRV_R.Get(),
				resources->_ssaoMaskSRV_R.Get(),
				resources->_bentBufSRV_R.Get(),
				resources->_normBufSRV_R.Get()
			};
			context->PSSetShaderResources(0, 6, srvs_pass2);
			context->Draw(6, 0);
		}
	}

out2:
	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

/* Convenience function to call WaitGetPoses() */
inline void WaitGetPoses() {
	// We need to call WaitGetPoses so that SteamVR gets the focus, otherwise we'll just get
	// error 101 when doing VRCompositor->Submit()
	vr::EVRCompositorError error = g_pVRCompositor->WaitGetPoses(&g_rTrackedDevicePose,
		0, NULL, 0);
}

HRESULT PrimarySurface::Flip(
	LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride,
	DWORD dwFlags
	)
{
	static uint64_t frame, lastFrame = 0;
	static float seconds;

#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;

	if (lpDDSurfaceTargetOverride == nullptr)
	{
		str << " NULL";
	}
	else if (lpDDSurfaceTargetOverride == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else if (lpDDSurfaceTargetOverride == this->_deviceResources->_frontbufferSurface)
	{
		str << " FrontbufferSurface";
	}
	else
	{
		str << " " << lpDDSurfaceTargetOverride;
	}

	if (dwFlags & DDFLIP_WAIT)
	{
		str << " WAIT";
	}

	LogText(str.str());
#endif

	this->_deviceResources->sceneRenderedEmpty = this->_deviceResources->sceneRendered == false;
	this->_deviceResources->sceneRendered = false;
	auto &context = this->_deviceResources->_d3dDeviceContext;

	if (this->_deviceResources->sceneRenderedEmpty && this->_deviceResources->_frontbufferSurface != nullptr && this->_deviceResources->_frontbufferSurface->wasBltFastCalled)
	{
		this->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(this->_deviceResources->_renderTargetView, this->_deviceResources->clearColor);
		if (g_bUseSteamVR) {
			this->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(this->_deviceResources->_renderTargetViewR, this->_deviceResources->clearColor);
			this->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(this->_deviceResources->_renderTargetViewPostR, this->_deviceResources->clearColor);
		}
		/*
		if (g_bDynCockpitEnabled) {
			context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewDynCockpit, this->_deviceResources->clearColor);
			context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewDynCockpitBG, this->_deviceResources->clearColor);
			context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewDynCockpitAsInput, this->_deviceResources->clearColor);
			context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewDynCockpitAsInputBG, this->_deviceResources->clearColor);
		}
		*/
		// Do we need to clear renderTargetViewPost?
		this->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(this->_deviceResources->_renderTargetViewPost, this->_deviceResources->clearColor);
		this->_deviceResources->_d3dDeviceContext->ClearDepthStencilView(this->_deviceResources->_depthStencilViewL, D3D11_CLEAR_DEPTH, this->_deviceResources->clearDepth, 0);
		this->_deviceResources->_d3dDeviceContext->ClearDepthStencilView(this->_deviceResources->_depthStencilViewR, D3D11_CLEAR_DEPTH, this->_deviceResources->clearDepth, 0);
	}

	/* Present 2D content */
	if (lpDDSurfaceTargetOverride != nullptr)
	{
		if (lpDDSurfaceTargetOverride == this->_deviceResources->_frontbufferSurface)
		{
			HRESULT hr;

			if (FAILED(hr = this->_deviceResources->_backbufferSurface->BltFast(0, 0, this->_deviceResources->_frontbufferSurface, nullptr, 0)))
				return hr;

			return this->Flip(this->_deviceResources->_backbufferSurface, 0);
		}

		/* Present 2D content */
		if (lpDDSurfaceTargetOverride == this->_deviceResources->_backbufferSurface)
		{
			if (this->_deviceResources->_frontbufferSurface == nullptr)
			{
				if (FAILED(this->_deviceResources->RenderMain(this->_deviceResources->_backbufferSurface->_buffer, this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight, this->_deviceResources->_displayBpp)))
					return DDERR_GENERIC;
			}
			else
			{
				const unsigned short colorKey = 0x8080;

				int width = 640;
				int height = 480;

				int topBlack = 0;

				if (this->_deviceResources->_displayBpp == 2)
				{
					int w = this->_deviceResources->_displayWidth;
					int x = (this->_deviceResources->_displayWidth - width) / 2;
					int y = (this->_deviceResources->_displayHeight - height) / 2;
					unsigned short* buffer = (unsigned short*)this->_deviceResources->_backbufferSurface->_buffer + y * w + x;

					for (int i = 0; i < height / 2; i++)
					{
						if (buffer[i * w] != colorKey)
							break;

						topBlack++;
					}
				}
				else
				{
					const unsigned int colorKey32 = convertColorB5G6R5toB8G8R8X8(colorKey);

					int w = this->_deviceResources->_displayWidth;
					int x = (this->_deviceResources->_displayWidth - width) / 2;
					int y = (this->_deviceResources->_displayHeight - height) / 2;
					unsigned int* buffer = (unsigned int*)this->_deviceResources->_backbufferSurface->_buffer + y * w + x;

					for (int i = 0; i < height / 2; i++)
					{
						if (buffer[i * w] != colorKey32)
							break;

						topBlack++;
					}
				}

				if (topBlack < height / 2)
				{
					height -= topBlack * 2;
				}

				RECT rc;
				rc.left = (this->_deviceResources->_displayWidth - width) / 2;
				rc.top = (this->_deviceResources->_displayHeight - height) / 2;
				rc.right = rc.left + width;
				rc.bottom = rc.top + height;

				int length = width * height * this->_deviceResources->_displayBpp;
				char* buffer = new char[length];

				copySurface(buffer, width, height, this->_deviceResources->_displayBpp, this->_deviceResources->_backbufferSurface->_buffer, this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight, this->_deviceResources->_displayBpp, 0, 0, &rc, false);

				HRESULT hr = this->_deviceResources->RenderMain(buffer, width, height, this->_deviceResources->_displayBpp);

				delete[] buffer;

				if (FAILED(hr))
					return DDERR_GENERIC;
			}

			HRESULT hr;

			/* Present Concourse, ESC Screen Menu */
			if (this->_deviceResources->_swapChain)
			{
				UINT rate = 25 * this->_deviceResources->_refreshRate.Denominator;
				UINT numerator = this->_deviceResources->_refreshRate.Numerator + this->_flipFrames;
				UINT interval = numerator / rate;
				this->_flipFrames = numerator % rate;
				// DEBUG
				//static bool bDisplayInterval = true;
				// DEBUG

				interval = max(interval, 1);
				// DEBUG
				/*if (bDisplayInterval) {
					log_debug("[DBG] Original interval: %d", interval);
				}*/
				// DEBUG

				hr = DD_OK;

				interval = g_iNaturalConcourseAnimations ? interval : 1;
				if (g_iNaturalConcourseAnimations > 1)
					interval = g_iNaturalConcourseAnimations;
				
				// DEBUG
				/*if (bDisplayInterval) {
					log_debug("[DBG] g_iNaturalConcourseAnimations: %d, Final interval: %d", g_iNaturalConcourseAnimations, interval);
					bDisplayInterval = false;
				}*/
				// DEBUG

				for (UINT i = 0; i < interval; i++)
				{
					// In the original code the offscreenBuffer is simply resolved into the backBuffer.
					// this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0, this->_deviceResources->_offscreenBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);

					if (!g_bDisableBarrelEffect && g_bEnableVR && !g_bUseSteamVR) {
						// Barrel effect enabled for DirectSBS mode
						barrelEffect2D(i);
						this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
							this->_deviceResources->_offscreenBufferPost, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
					}
					else {
						// In SteamVR mode this will display the left image:
						if (g_bUseSteamVR) {
							resizeForSteamVR(0, true);
							this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
								this->_deviceResources->_steamVRPresentBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
						}
						else {
							this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
								this->_deviceResources->_offscreenBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
						}
					}
					
					// Enable capturing 3D objects even when rendering 2D. This happens in the Tech Library
#ifdef DBG_VR
					if (g_bStart3DCapture && !g_bDo3DCapture) {
						g_bDo3DCapture = true;
					}
					else if (g_bStart3DCapture && g_bDo3DCapture) {
						g_bDo3DCapture = false;
						g_bStart3DCapture = false;
						fclose(g_HackFile);
						g_HackFile = NULL;
					}
#endif

					// Reset the 2D draw counter -- that'll help us increase the parallax for the Tech Library
					g_iDraw2DCounter = 0;				
					if (g_bRendering3D) {
						// We're about to switch from 3D to 2D rendering --> This means we're in the Tech Library (?)
						// Let's clear the render target for the next iteration or we'll get multiple images during
						// the animation
						auto &context = this->_deviceResources->_d3dDeviceContext;
						float bgColor[4] = { 0, 0, 0, 0 };
						this->_deviceResources->_d3dDeviceContext->ClearRenderTargetView(
							this->_deviceResources->_renderTargetView, bgColor);
						if (g_bUseSteamVR) {
							context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewR, bgColor);
							context->ClearRenderTargetView(this->_deviceResources->_renderTargetViewSteamVRResize, bgColor);
						}
						//log_debug("[DBG] In Tech Library, external cam: %d", PlayerDataTable->externalCamera);
					}

					if (g_bUseSteamVR) {					
						vr::EVRCompositorError error = vr::VRCompositorError_None;
						vr::Texture_t leftEyeTexture = { this->_deviceResources->_offscreenBuffer.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
						vr::Texture_t rightEyeTexture = { this->_deviceResources->_offscreenBufferR.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
						error = g_pVRCompositor->Submit(vr::Eye_Left, &leftEyeTexture);
						error = g_pVRCompositor->Submit(vr::Eye_Right, &rightEyeTexture);
					}
					
					g_bRendering3D = false;
					// Present 2D
					if (FAILED(hr = this->_deviceResources->_swapChain->Present(g_iNaturalConcourseAnimations, 0)))
					{
						static bool messageShown = false;
						
						if (!messageShown)
						{
							MessageBox(nullptr, _com_error(hr).ErrorMessage(), __FUNCTION__, MB_ICONERROR);
						}

						messageShown = true;

						hr = DDERR_SURFACELOST;
						break;
					}

					if (g_bUseSteamVR) {
						g_pVRCompositor->PostPresentHandoff();
						WaitGetPoses();
					}		
				}
			}
			else
			{
				hr = DD_OK;
			}

			if (this->_deviceResources->_frontbufferSurface != nullptr)
			{
				if (this->_deviceResources->_frontbufferSurface->wasBltFastCalled)
				{
					memcpy(this->_deviceResources->_frontbufferSurface->_buffer2, this->_deviceResources->_frontbufferSurface->_buffer, this->_deviceResources->_frontbufferSurface->_bufferSize);
				}

				this->_deviceResources->_frontbufferSurface->wasBltFastCalled = false;
			}

			return hr;
		}
	}
	/* Present 3D content */
	else
	{
		HRESULT hr;
		auto &resources = this->_deviceResources;
		auto &context = resources->_d3dDeviceContext;

		// This moves the external camera when in the hangar:
		//static __int16 yaw = 0;
		//PlayerDataTable[0].cameraYaw   += 40 * *mouseLook_X;
		//PlayerDataTable[0].cameraPitch += 15 * *mouseLook_Y;
		//yaw += 600;
		//log_debug("[DBG] roll: %0.3f", PlayerDataTable[0].roll / 32768.0f * 180.0f);
		// This looks like it's always 0:
		//log_debug("[DBG] els Lasers: %d, Shields: %d, Beam: %d",
		//	PlayerDataTable[0].elsLasers, PlayerDataTable[0].elsShields, PlayerDataTable[0].elsBeam);

		if (this->_deviceResources->_swapChain)
		{
			hr = DD_OK;

			// Resolve the Bloom mask before the SSAO and Bloom effects.
			if (g_bReshadeEnabled) {
				// Resolve whatever is in the _offscreenBufferReshadeMask into _offscreenBufferAsInputReshadeMask, and
				// do the same for the right (SteamVR) image -- I'll worry about the details later.
				// _offscreenBufferAsInputReshade was previously resolved during Execute() -- right before any GUI is rendered
				context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
					resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
				if (g_bUseSteamVR)
					context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
						resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);

				// DEBUG
				/*if (g_iPresentCounter == 100) {
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask.dds");
				}*/
				// DEBUG
			}

			// AO must (?) be computed before the bloom shader -- or at least output to a different buffer
			if (g_bAOEnabled) {
				if (!g_bDepthBufferResolved) {
					// If an external camera is used, then the depth buf will be resolved here
					//log_debug("[DBG] [AO] Resolving depth Buf in PrimarySurface Flip()");
					// If the depth buffer wasn't resolved during the regular Execute() then resolve it here
					context->ResolveSubresource(resources->_depthBufAsInput, 0, resources->_depthBuf, 0, AO_DEPTH_BUFFER_FORMAT);
					context->ResolveSubresource(resources->_depthBuf2AsInput, 0, resources->_depthBuf2, 0, AO_DEPTH_BUFFER_FORMAT);
					//context->ResolveSubresource(resources->_normBufAsInput, 0, resources->_normBuf, 0, AO_DEPTH_BUFFER_FORMAT);
					if (g_bUseSteamVR) {
						context->ResolveSubresource(resources->_depthBufAsInputR, 0,
							resources->_depthBufR, 0, AO_DEPTH_BUFFER_FORMAT);
						context->ResolveSubresource(resources->_depthBuf2AsInputR, 0,
							resources->_depthBuf2R, 0, AO_DEPTH_BUFFER_FORMAT);
						//context->ResolveSubresource(resources->_normBufAsInputR, 0,
						//	 resources->_normBufR, 0, AO_DEPTH_BUFFER_FORMAT);
					}
				}

				// We need to set the blend state properly for SSAO
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display SSAO
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				/*
				D3D11_SAMPLER_DESC oldSamplerDesc = this->_renderStates->GetSamplerDesc();
				D3D11_SAMPLER_DESC samplerDesc;
				samplerDesc.Filter = resources->_useAnisotropy ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.MaxAnisotropy = resources->_useAnisotropy ? resources->GetMaxAnisotropy() : 1;
				samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.MipLODBias = 0.0f;
				samplerDesc.MinLOD = 0;
				samplerDesc.MaxLOD = FLT_MAX;
				samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
				samplerDesc.BorderColor[0] = 0.0f;
				samplerDesc.BorderColor[1] = 0.0f;
				samplerDesc.BorderColor[2] = 0.0f;
				samplerDesc.BorderColor[3] = 0.0f;
				ComPtr<ID3D11SamplerState> tempSampler;
				hr = resources->_d3dDevice->CreateSamplerState(&samplerDesc, &tempSampler);
				context->PSSetSamplers(1, 1, &tempSampler);*/

				if (g_bDumpSSAOBuffers) {
					DirectX::SaveDDSTextureToFile(context, resources->_normBuf, L"C:\\Temp\\_normBuf.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_depthBuf, L"C:\\Temp\\_depthBuf.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_depthBuf2, L"C:\\Temp\\_depthBuf2.dds");
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_bloomMask.jpg");
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_offscreenBuf.jpg");
					DirectX::SaveWICTextureToFile(context, resources->_ssaoMask, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_ssaoMask.jpg");
					log_debug("[DBG] [AO] Captured debug buffers");
				}

				// Input: depthBufAsInput (already resolved during Execute())
				// Output: normalsBuf
				//SmoothNormalsPass(1.0f);

				// Input: depthBuf, normBuf, randBuf
				// Output: _bloom1
				switch (g_SSAO_Type) {
					case SSO_AMBIENT:
						SSAOPass(g_fSSAOZoomFactor);
						break;
					case SSO_DIRECTIONAL:
						SSDOPass(g_fSSAOZoomFactor);
						break;
				}

				if (g_bDumpSSAOBuffers) {
					//DirectX::SaveDDSTextureToFile(context, resources->_bentBuf, L"C:\\Temp\\_bentBuf.dds");
					DirectX::SaveWICTextureToFile(context, resources->_bentBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_bentBuf.jpg");
					DirectX::SaveWICTextureToFile(context, resources->_ssaoBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_ssaoBuf.jpg");
					DirectX::SaveWICTextureToFile(context, resources->_ssaoBufR, GUID_ContainerFormatJpeg, L"C:\\Temp\\_ssaoBufR.jpg");
				}
			}

			// Apply the Bloom effect
			if (g_bBloomEnabled) {
				// _offscreenBufferAsInputBloomMask is resolved earlier, before the SSAO pass because
				// SSAO uses that mask to prevent applying SSAO on bright areas

				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				int HyperspacePhase = PlayerDataTable->hyperspacePhase;
				bool bHyperStreaks = (HyperspacePhase == 2) || (HyperspacePhase == 3);

				// DEBUG
				//static int CaptureCounter = 1;
				// DEBUG

				// Initialize the accummulator buffer
				float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				context->ClearRenderTargetView(resources->_renderTargetViewBloomSum, bgColor);
				if (g_bUseSteamVR)
					context->ClearRenderTargetView(resources->_renderTargetViewBloomSumR, bgColor);

				// DEBUG
				/* if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
					wchar_t filename[80];

					swprintf_s(filename, 80, L".\\_offscreenBufferBloomMask-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBufferBloomMask-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, filename);


					swprintf_s(filename, 80, L".\\_offscreenBuffer-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBuffer-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBuffer, filename);
				} */
				// DEBUG

				if (bHyperStreaks)
				{
					// Only apply the "short" bloom to the hyperspace streaks (otherwise they
					// tend to get overexposed)
					// 2 = Entering hyperspace
					// 4 = Blue tunnel
					// 3 = Exiting hyperspace
					// Nice hyperspace animation:
					// https://www.youtube.com/watch?v=d5W3afhgOlY
					BloomPyramidLevelPass(1, 2, 2.0f);
					BloomPyramidLevelPass(2, 1, 4.0f);
					BloomPyramidLevelPass(3, 1, 8.0f);
					BloomPyramidLevelPass(4, 1, 16.0f);
				}
				else
				{
					float fScale = 2.0f;
					for (int i = 1; i <= g_BloomConfig.iNumPasses; i++) {
						int AdditionalPasses = g_iBloomPasses[i] - 1;
						// Zoom level 2.0f with only one pass tends to show artifacts unless
						// the spread is set to 1
						BloomPyramidLevelPass(i, AdditionalPasses, fScale);
						fScale *= 2.0f;
					}
				}

				//if (!g_bAOEnabled) {
					// If SSAO is not enabled, then we can merge the bloom buffer with the offscreen buffer
					// here. Otherwise, we'll merge it along with the SSAO buffer later.
					// Add the accumulated bloom with the offscreen buffer
					// Input: _bloomSum, _offscreenBufferAsInput
					// Output: _offscreenBuffer
					BloomBasicPass(5, 1.0f);
				//}

				// DEBUG
				/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
					wchar_t filename[80];

					swprintf_s(filename, 80, L".\\_bloomMask-Final-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_bloomOutput1, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_bloomMask-Final-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_bloomOutput1, filename);

					swprintf_s(filename, 80, L".\\_offscreenBuffer-Final-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBuffer-Final-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBuffer, filename);

					CaptureCounter++;
					g_bDumpBloomBuffers = false;
				}*/
				// DEBUG
			}

			// Apply the HUD *after* we have re-shaded it (if necessary)
			if (g_bDCManualActivate && (g_bDynCockpitEnabled || g_bReshadeEnabled) && 
				g_iHUDOffscreenCommandsRendered && resources->_bHUDVerticesReady) {
				// Clear everything we don't want to display from the HUD
				if (g_bDynCockpitEnabled)
					ClearHUDRegions();
				
				// Display the HUD. This renders to offscreenBuffer/offscreenBufferR
				DrawHUDVertices();
			}

			// In the original code, the offscreenBuffer is resolved to the backBuffer
			//this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0, this->_deviceResources->_offscreenBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);

			// The offscreenBuffer contains the fully-rendered image at this point.
			if (g_bEnableVR) {
				if (g_bUseSteamVR) {
					if (!g_bDisableBarrelEffect) {
						// Do the barrel effect (_offscreenBuffer -> _offscreenBufferPost)
						barrelEffectSteamVR();
						this->_deviceResources->_d3dDeviceContext->CopyResource(this->_deviceResources->_offscreenBuffer,
							this->_deviceResources->_offscreenBufferPost);
					}
					// Resize the buffer to be presented (_offscreenBuffer -> _steamVRPresentBuffer)
					resizeForSteamVR(0, false);
					// Resolve steamVRPresentBuffer to backBuffer so that it gets presented
					this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
						this->_deviceResources->_steamVRPresentBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
				} 
				else { // Direct SBS mode
					if (!g_bDisableBarrelEffect) {
						barrelEffect3D();
						this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
							this->_deviceResources->_offscreenBufferPost, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
					} else
						this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
							this->_deviceResources->_offscreenBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
				}
			} 
			else // Non-VR mode
				this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0,
					this->_deviceResources->_offscreenBuffer, 0, DXGI_FORMAT_B8G8R8A8_UNORM);

			// Let's reset some frame counters and other control variables
			g_iDrawCounter = 0; g_iExecBufCounter = 0;
			g_iNonZBufferCounter = 0; g_iDrawCounterAfterHUD = -1;
			g_iFloatingGUIDrawnCounter = 0;
			g_bTargetCompDrawn = false;
			g_bPrevIsFloatingGUI3DObject = false;
			g_bIsFloating3DObject = false;
			//log_debug("[DBG] g_bStartedGUI at end-of-frame: %d", g_bStartedGUI);
			g_bStartedGUI = false;
			g_bPrevStartedGUI = false;
			g_bIsScaleableGUIElem = false;
			g_bPrevIsScaleableGUIElem = false;
			g_bScaleableHUDStarted = false;
			g_bIsTrianglePointer = false;
			g_bLastTrianglePointer = false;
			g_iHUDOffscreenCommandsRendered = 0;
			// Disable the Dynamic Cockpit whenever we're in external camera mode:
			g_bDCManualActivate = !PlayerDataTable->externalCamera;
			g_bDepthBufferResolved = false;
			//*g_playerInHangar = 0;
			if (g_bDumpSSAOBuffers)
				g_bDumpSSAOBuffers = false;


			if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
				float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				auto &context = resources->_d3dDeviceContext;
				context->ResolveSubresource(_deviceResources->_offscreenAsInputDynCockpit,
					0, _deviceResources->_offscreenBufferDynCockpit, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
				context->ResolveSubresource(_deviceResources->_offscreenAsInputDynCockpitBG,
					0, _deviceResources->_offscreenBufferDynCockpitBG, 0, DXGI_FORMAT_B8G8R8A8_UNORM);

				/*static bool bDump = true;
				if (g_iPresentCounter == 100 && bDump) {
					capture(0, resources->_offscreenAsInputDynCockpit, L"C:\\Temp\\_DC-FG.jpg");
					capture(0, resources->_offscreenAsInputDynCockpitBG, L"C:\\Temp\\_DC-BG.jpg");
					bDump = false;
				}*/
			}

			// Perform the lean left/right etc animations
			animTickX();
			animTickY();
			animTickZ();

			// Enable 6dof
			if (g_bUseSteamVR) {
				float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
				float x = 0.0f, y = 0.0f, z = 0.0f;
				Matrix3 rotMatrix;
				Vector3 pos;
				static Vector4 headCenter(0, 0, 0, 0);
				Vector3 headPos;
				Vector3 headPosFromKeyboard(g_HeadPos.x, g_HeadPos.y, g_HeadPos.z);

				GetSteamVRPositionalData(&yaw, &pitch, &roll, &x, &y, &z, &rotMatrix);
				yaw   *= RAD_TO_DEG * g_fYawMultiplier;
				pitch *= RAD_TO_DEG * g_fPitchMultiplier;
				roll  *= RAD_TO_DEG * g_fRollMultiplier;
				yaw   += g_fYawOffset;
				pitch += g_fPitchOffset;

				// HACK ALERT: I'm reading the positional tracking data from FreePIE when
				// running SteamVR because setting up the PSMoveServiceSteamVRBridge is kind
				// of... tricky; and I'm not going to bother right now since PSMoveService
				// already works very well for me.
				// Read the positional data from FreePIE if the right flag is set
				if (g_bSteamVRPosFromFreePIE) {
					ReadFreePIE(g_iFreePIESlot);
					if (g_bResetHeadCenter) {
						headCenter[0] = g_FreePIEData.x;
						headCenter[1] = g_FreePIEData.y;
						headCenter[2] = g_FreePIEData.z;
						g_bResetHeadCenter = false;
					}
					x = g_FreePIEData.x - headCenter[0];
					y = g_FreePIEData.y - headCenter[1];
					z = g_FreePIEData.z - headCenter[2];
				}
				pos.set(x, y, z);
				headPos = -pos;

				// Compute the full rotation
				Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
				rotMatrixFull.identity();
				rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
				rotMatrixPitch.identity(); rotMatrixPitch.rotateX(-pitch);
				rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(roll);
				rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixYaw;
				//rotMatrixFull.invert();

				// Adding headPosFromKeyboard is only to allow the keys to move the cockpit.
				// g_HeadPos can be removed once positional tracking has been fixed... or 
				// maybe we can leave it there to test things
				headPos[0] = headPos[0] * g_fPosXMultiplier + headPosFromKeyboard[0];
				headPos[1] = headPos[1] * g_fPosYMultiplier + headPosFromKeyboard[1];
				headPos[2] = headPos[2] * g_fPosZMultiplier + headPosFromKeyboard[2];

				// Limits clamping
				if (headPos[0] < g_fMinPositionX) headPos[0] = g_fMinPositionX;
				if (headPos[1] < g_fMinPositionY) headPos[1] = g_fMinPositionY;
				if (headPos[2] < g_fMinPositionZ) headPos[2] = g_fMinPositionZ;

				if (headPos[0] > g_fMaxPositionX) headPos[0] = g_fMaxPositionX;
				if (headPos[1] > g_fMaxPositionY) headPos[1] = g_fMaxPositionY;
				if (headPos[2] > g_fMaxPositionZ) headPos[2] = g_fMaxPositionZ;

				// Transform the absolute head position into a relative position. This is
				// needed because the game will apply the yaw/pitch on its own. So, we need
				// to undo the yaw/pitch transformation by computing the inverse of the
				// rotation matrix. Fortunately, rotation matrices can be inverted with a
				// simple transpose.
				rotMatrix.invert();
				//rotMatrix.transpose();
				headPos = rotMatrix * headPos;

				g_viewMatrix.identity();
				g_viewMatrix.rotateZ(roll);
				g_viewMatrix[12] = headPos[0]; 
				g_viewMatrix[13] = headPos[1];
				g_viewMatrix[14] = headPos[2];
				rotMatrixFull[12] = headPos[0];
				rotMatrixFull[13] = headPos[1];
				rotMatrixFull[14] = headPos[2];
				// viewMat is not a full transform matrix: it's only RotZ + Translation
				// because the cockpit hook already applies the yaw/pitch rotation
				g_VSMatrixCB.viewMat = g_viewMatrix;
				g_VSMatrixCB.fullViewMat = rotMatrixFull;
			}
			else if (g_bEnableVR) { // DirectSBS mode, read the roll and position (?) from FreePIE
				//float pitch, yaw, roll, pitchSign = -1.0f;
				float yaw, pitch, roll;
				static Vector4 headCenter(0, 0, 0, 0);
				Vector4 headPos(0,0,0,1);
				//Vector3 headPosFromKeyboard(0, 0, 0);
				Vector3 headPosFromKeyboard(g_HeadPos.x, g_HeadPos.y, g_HeadPos.z); // Regular keyboard functionality
				//Vector3 headPosFromKeyboard(-g_HeadPos.x, g_HeadPos.y, -g_HeadPos.z);
				
				if (g_bResetHeadCenter) {
					g_HeadPos = { 0 };
					g_HeadPosAnim = { 0 };
				}

				if (ReadFreePIE(g_iFreePIESlot)) {
					if (g_bResetHeadCenter) {
						headCenter[0] = g_FreePIEData.x;
						headCenter[1] = g_FreePIEData.y;
						headCenter[2] = g_FreePIEData.z;
					}
					Vector4 pos(g_FreePIEData.x, g_FreePIEData.y, g_FreePIEData.z, 1.0f);
					yaw    = g_FreePIEData.yaw   * g_fYawMultiplier;
					pitch  = g_FreePIEData.pitch * g_fPitchMultiplier;
					roll   = g_FreePIEData.roll  * g_fRollMultiplier;
					yaw   += g_fYawOffset;
					pitch += g_fPitchOffset;
					headPos = (pos - headCenter);
				} 
				
				if (g_bYawPitchFromMouseOverride) {
					// If FreePIE could not be read, then get the yaw/pitch from the mouse:
					yaw   =  (float)PlayerDataTable[0].cockpitCameraYaw / 32768.0f * 180.0f;
					pitch = -(float)PlayerDataTable[0].cockpitCameraPitch / 32768.0f * 180.0f;
				}

				if (g_bResetHeadCenter)
					g_bResetHeadCenter = false;

				headPos[0] = headPos[0] * g_fPosXMultiplier + headPosFromKeyboard[0]; 
				//headPos[0] = headPos[0] * g_fPosXMultiplier; // HACK to roll-through-keyboard
				headPos[1] = headPos[1] * g_fPosYMultiplier + headPosFromKeyboard[1];
				headPos[2] = headPos[2] * g_fPosZMultiplier + headPosFromKeyboard[2];

				// Limits clamping
				if (headPos[0] < g_fMinPositionX) headPos[0] = g_fMinPositionX;
				if (headPos[1] < g_fMinPositionY) headPos[1] = g_fMinPositionY;
				if (headPos[2] < g_fMinPositionZ) headPos[2] = g_fMinPositionZ;
				//if (-headPos[2] < g_fMinPositionZ) headPos[2] = -g_fMinPositionZ;

				if (headPos[0] > g_fMaxPositionX) headPos[0] = g_fMaxPositionX;
				if (headPos[1] > g_fMaxPositionY) headPos[1] = g_fMaxPositionY;
				if (headPos[2] > g_fMaxPositionZ) headPos[2] = g_fMaxPositionZ;
				//if (-headPos[2] > g_fMaxPositionZ) headPos[2] = -g_fMaxPositionZ;

				Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
				rotMatrixFull.identity();
				//rotMatrixYaw.identity(); rotMatrixYaw.rotateY(g_FreePIEData.yaw);
				rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
				rotMatrixPitch.identity(); rotMatrixPitch.rotateX(pitch);
				rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(roll);

				// For the fixed GUI, yaw has to be like this:
				rotMatrixFull.rotateY(yaw);
				rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixFull;
				// But the matrix to compensate for the translation uses -yaw:
				rotMatrixYaw  = rotMatrixPitch * rotMatrixYaw;
				// Can we avoid computing the matrix inverse?
				rotMatrixYaw.invert();
				headPos = rotMatrixYaw * headPos;

				g_viewMatrix.identity();
				g_viewMatrix.rotateZ(roll); 
				//g_viewMatrix.rotateZ(roll + 30.0f * headPosFromKeyboard[0]); // HACK to enable roll-through keyboard
				g_viewMatrix[12] = headPos[0];
				g_viewMatrix[13] = headPos[1];
				g_viewMatrix[14] = headPos[2];
				rotMatrixFull[12] = headPos[0];
				rotMatrixFull[13] = headPos[1];
				rotMatrixFull[14] = headPos[2];
				// viewMat is not a full transform matrix: it's only RotZ + Translation
				// because the cockpit hook already applies the yaw/pitch rotation
				g_VSMatrixCB.viewMat = g_viewMatrix;
				g_VSMatrixCB.fullViewMat = rotMatrixFull;
			}

#ifdef DBG_VR
			if (g_bStart3DCapture && !g_bDo3DCapture) {
				g_bDo3DCapture = true;
			}
			else if (g_bStart3DCapture && g_bDo3DCapture) {
				g_bDo3DCapture = false;
				g_bStart3DCapture = false;
				fclose(g_HackFile);
				g_HackFile = NULL;
			}
#endif

			if (g_bUseSteamVR) {
				//if (!g_pHMD->GetTimeSinceLastVsync(&seconds, &frame))
				//	log_debug("[DBG] No Vsync info available");

				vr::EVRCompositorError error = vr::VRCompositorError_None;
				vr::Texture_t leftEyeTexture;
				vr::Texture_t rightEyeTexture;

				if (g_bDisableBarrelEffect) {
					leftEyeTexture = { this->_deviceResources->_offscreenBuffer.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
					rightEyeTexture = { this->_deviceResources->_offscreenBufferR.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
				}
				else {
					leftEyeTexture = { this->_deviceResources->_offscreenBufferPost.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
					rightEyeTexture = { this->_deviceResources->_offscreenBufferPostR.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
				}
				error = g_pVRCompositor->Submit(vr::Eye_Left, &leftEyeTexture);
				//if (error) log_debug("[DBG] 3D Present (left) SteamVR error: %d", error);
				error = g_pVRCompositor->Submit(vr::Eye_Right, &rightEyeTexture);
				//if (error) log_debug("[DBG] 3D Present (right) SteamVR error: %d", error);
				//g_pVRCompositor->PostPresentHandoff();
			}

			//float timeRemaining = g_pVRCompositor->GetFrameTimeRemaining();
			//log_debug("[DBG] Time remaining: %0.3f", timeRemaining);
			//if (timeRemaining < 0.005) WaitGetPoses();

			// We're about to show 3D content, so let's set the corresponding flag
			g_bRendering3D = true;
			// Doing Present(1, 0) limits the framerate to 30fps, without it, it can go up to 60; but usually
			// stays around 45 in my system
			//log_debug("[DBG] Present 3D");
			g_iPresentCounter++;
			// This is Jeremy's code:
			//if (FAILED(hr = this->_deviceResources->_swapChain->Present(g_config.VSyncEnabled ? 1 : 0, 0)))
			// For VR, we probably want to disable VSync to get as much frames a possible:
			//if (FAILED(hr = this->_deviceResources->_swapChain->Present(0, 0)))
			if (FAILED(hr = this->_deviceResources->_swapChain->Present(g_config.VSyncEnabled ? 1 : 0, 0)))
			{
				static bool messageShown = false;
				if (!messageShown)
				{
					MessageBox(nullptr, _com_error(hr).ErrorMessage(), __FUNCTION__, MB_ICONERROR);
				}
				messageShown = true;
				hr = DDERR_SURFACELOST;
			}
			if (g_bUseSteamVR) {
				g_pVRCompositor->PostPresentHandoff();
				//g_pHMD->GetTimeSinceLastVsync(&seconds, &frame);
				//if (seconds > 0.008)
				WaitGetPoses();
			}
		}
		else
		{
			hr = DD_OK;
		}

		return hr;
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetAttachedSurface(
	LPDDSCAPS lpDDSCaps,
	LPDIRECTDRAWSURFACE FAR *lplpDDAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	if (lpDDSCaps == nullptr)
	{
#if LOGGER
		str.str("\tDDERR_INVALIDPARAMS");
		LogText(str.str());
#endif

		return DDERR_INVALIDPARAMS;
	}

	if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
	{
		if (!this->_hasBackbufferAttached)
		{
#if LOGGER
			str.str("\tDDERR_INVALIDOBJECT");
			LogText(str.str());
#endif

			return DDERR_INVALIDOBJECT;
		}

		*lplpDDAttachedSurface = this->_backbufferSurface;
		this->_backbufferSurface->AddRef();

#if LOGGER
		str.str("");
		str << "\tBackbufferSurface\t" << *lplpDDAttachedSurface;
		LogText(str.str());
#endif

		return DD_OK;
	}

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetBltStatus(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetCaps(
	LPDDSCAPS lpDDSCaps
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetClipper(
	LPDIRECTDRAWCLIPPER FAR *lplpDDClipper
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetColorKey(
	DWORD dwFlags,
	LPDDCOLORKEY lpDDColorKey
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetDC(
	HDC FAR *lphDC
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetFlipStatus(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetOverlayPosition(
	LPLONG lplX,
	LPLONG lplY
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetPalette(
	LPDIRECTDRAWPALETTE FAR *lplpDDPalette
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetPixelFormat(
	LPDDPIXELFORMAT lpDDPixelFormat
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetSurfaceDesc(
	LPDDSURFACEDESC lpDDSurfaceDesc
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	if (lpDDSurfaceDesc == nullptr)
	{
#if LOGGER
		str.str("\tDDERR_INVALIDPARAMS");
		LogText(str.str());
#endif

		return DDERR_INVALIDPARAMS;
	}

	*lpDDSurfaceDesc = {};
	lpDDSurfaceDesc->dwSize = sizeof(DDSURFACEDESC);
	lpDDSurfaceDesc->dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH;
	lpDDSurfaceDesc->ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
	lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 16;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
	lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x7E0;
	lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x1F;
	lpDDSurfaceDesc->dwHeight = this->_deviceResources->_displayHeight;
	lpDDSurfaceDesc->dwWidth = this->_deviceResources->_displayWidth;
	lpDDSurfaceDesc->lPitch = this->_deviceResources->_displayWidth * 2;

#if LOGGER
	str.str("");
	str << "\t" << tostr_DDSURFACEDESC(lpDDSurfaceDesc);
	LogText(str.str());
#endif

	return DD_OK;
}

HRESULT PrimarySurface::Initialize(
	LPDIRECTDRAW lpDD,
	LPDDSURFACEDESC lpDDSurfaceDesc
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::IsLost()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Lock(
	LPRECT lpDestRect,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	DWORD dwFlags,
	HANDLE hEvent
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::ReleaseDC(
	HDC hDC
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Restore()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetClipper(
	LPDIRECTDRAWCLIPPER lpDDClipper
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetColorKey(
	DWORD dwFlags,
	LPDDCOLORKEY lpDDColorKey
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetOverlayPosition(
	LONG lX,
	LONG lY
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetPalette(
	LPDIRECTDRAWPALETTE lpDDPalette
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Unlock(
	LPVOID lpSurfaceData
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlay(
	LPRECT lpSrcRect,
	LPDIRECTDRAWSURFACE lpDDDestSurface,
	LPRECT lpDestRect,
	DWORD dwFlags,
	LPDDOVERLAYFX lpDDOverlayFx
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlayDisplay(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlayZOrder(
	DWORD dwFlags,
	LPDIRECTDRAWSURFACE lpDDSReference
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}
