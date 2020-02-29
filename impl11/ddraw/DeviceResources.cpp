// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#include <ScreenGrab.h>
#include <wincodec.h>

#include "common.h"
#include "DeviceResources.h"

#ifdef _DEBUG
#include "../Debug/MainVertexShader.h"
#include "../Debug/MainPixelShader.h"
#include "../Debug/MainPixelShaderBpp2ColorKey20.h"
#include "../Debug/MainPixelShaderBpp2ColorKey00.h"
#include "../Debug/MainPixelShaderBpp4ColorKey20.h"
#include "../Debug/BarrelPixelShader.h"
#include "../Debug/BasicPixelShader.h"
#include "../Debug/SingleBarrelPixelShader.h"
#include "../Debug/VertexShader.h"
#include "../Debug/PassthroughVertexShader.h"
#include "../Debug/SBSVertexShader.h"
#include "../Debug/PixelShaderTexture.h"
#include "../Debug/PixelShaderDC.h"
#include "../Debug/PixelShaderEmptyDC.h"
#include "../Debug/PixelShaderHUD.h"
#include "../Debug/PixelShaderSolid.h"
#include "../Debug/PixelShaderClearBox.h"
#include "../Debug/BloomHGaussPS.h"
#include "../Debug/BloomVGaussPS.h"
#include "../Debug/BloomCombinePS.h"
#include "../Debug/BloomBufferAddPS.h"
#include "../Debug/ComputeNormalsShader.h"
#include "../Debug/SSAOPixelShader.h"
#include "../Debug/SSAOBlurPixelShader.h"
#include "../Debug/SSAOAddPixelShader.h"
#include "../Debug/SSDODirect.h"
#include "../Debug/SSDODirectBentNormals.h"
#include "../Debug/SSDODirectHDR.h"
#include "../Debug/SSDOIndirect.h"
#include "../Debug/SSDOAdd.h"
#include "../Debug/SSDOAddHDR.h"
#include "../Debug/SSDOAddBentNormals.h"
#include "../Debug/SSDOBlur.h"
#include "../Debug/DeathStarShader.h"
#include "../Debug/HyperEntry.h"
#include "../Debug/HyperExit.h"
#include "../Debug/HyperTunnel.h"
#include "../Debug/HyperCompose.h"
#include "../Debug/HyperZoom.h"
#include "../Debug/LaserPointerVR.h"
#include "../Debug/FXAA.h"
#else
#include "../Release/MainVertexShader.h"
#include "../Release/MainPixelShader.h"
#include "../Release/MainPixelShaderBpp2ColorKey20.h"
#include "../Release/MainPixelShaderBpp2ColorKey00.h"
#include "../Release/MainPixelShaderBpp4ColorKey20.h"
#include "../Release/BarrelPixelShader.h"
#include "../Release/BasicPixelShader.h"
#include "../Release/SingleBarrelPixelShader.h"
#include "../Release/VertexShader.h"
#include "../Release/PassthroughVertexShader.h"
#include "../Release/SBSVertexShader.h"
#include "../Release/PixelShaderTexture.h"
#include "../Release/PixelShaderDC.h"
#include "../Release/PixelShaderEmptyDC.h"
#include "../Release/PixelShaderHUD.h"
#include "../Release/PixelShaderSolid.h"
#include "../Release/PixelShaderClearBox.h"
#include "../Release/BloomHGaussPS.h"
#include "../Release/BloomVGaussPS.h"
#include "../Release/BloomCombinePS.h"
#include "../Release/BloomBufferAddPS.h"
#include "../Release/ComputeNormalsShader.h"
#include "../Release/SSAOPixelShader.h"
#include "../Release/SSAOBlurPixelShader.h"
#include "../Release/SSAOAddPixelShader.h"
#include "../Release/SSDODirect.h"
#include "../Release/SSDODirectBentNormals.h"
#include "../Release/SSDODirectHDR.h"
#include "../Release/SSDOIndirect.h"
#include "../Release/SSDOAdd.h"
#include "../Release/SSDOAddHDR.h"
#include "../Release/SSDOAddBentNormals.h"
#include "../Release/SSDOBlur.h"
#include "../Release/DeathStarShader.h"
#include "../Release/HyperEntry.h"
#include "../Release/HyperExit.h"
#include "../Release/HyperTunnel.h"
#include "../Release/HyperCompose.h"
#include "../Release/HyperZoom.h"
#include "../Release/LaserPointerVR.h"
#include "../Release/FXAA.h"
#endif

#include <WICTextureLoader.h>
#include <headers/openvr.h>
#include <vector>
//#include <assert.h>

void InitOPTnames();
void ClearOPTnames();
void InitCraftMaterials();
void ClearCraftMaterials();

bool g_bWndProcReplaced = false;
bool ReplaceWindowProc(HWND ThisWindow);
extern MainShadersCBuffer g_MSCBuffer;
extern BarrelPixelShaderCBuffer g_BarrelPSCBuffer;
extern float g_fConcourseScale, g_fConcourseAspectRatio, g_fTechLibraryParallax, g_fBrightness;
extern bool /* g_bRendering3D, */ g_bDumpDebug, g_bOverrideAspectRatio, g_bCustomFOVApplied;
extern int g_iPresentCounter;
int g_iDraw2DCounter = 0;
extern bool g_bEnableVR, g_bForceViewportChange;
extern Matrix4 g_fullMatrixLeft, g_fullMatrixRight;
extern VertexShaderMatrixCB g_VSMatrixCB;
// DYNAMIC COCKPIT
extern dc_element g_DCElements[];
extern int g_iNumDCElements;
extern char g_sCurrentCockpit[128];
extern DCHUDRegions g_DCHUDRegions;
extern DCElemSrcBoxes g_DCElemSrcBoxes;

// ACTIVE COCKPIT
extern bool g_bActiveCockpitEnabled;
extern ac_element g_ACElements[];
extern int g_iNumACElements;

extern bool g_bReshadeEnabled, g_bBloomEnabled;

extern float g_fHUDDepth;
extern float g_fCurInGameWidth, g_fCurInGameHeight, g_fCurScreenWidth, g_fCurScreenHeight, g_fCurScreenWidthRcp, g_fCurScreenHeightRcp;

// SteamVR
#include <headers/openvr.h>
extern bool g_bSteamVRInitialized, g_bUseSteamVR, g_bEnableVR;
extern uint32_t g_steamVRWidth, g_steamVRHeight;
DWORD g_FullScreenWidth = 0, g_FullScreenHeight = 0;
bool InitSteamVR();

void LoadFocalLength();

/* The different types of Constant Buffers used in the Vertex Shader: */
typedef enum {
	VS_CONSTANT_BUFFER_NONE,
	VS_CONSTANT_BUFFER_2D,
	VS_CONSTANT_BUFFER_3D,
} VSConstantBufferType;
VSConstantBufferType g_LastVSConstantBufferSet = VS_CONSTANT_BUFFER_NONE;

/* The different types of Constant Buffers used in the Pixel Shader: */
typedef enum {
	PS_CONSTANT_BUFFER_NONE,
	PS_CONSTANT_BUFFER_BARREL,
	PS_CONSTANT_BUFFER_2D,
	PS_CONSTANT_BUFFER_3D,
	PS_CONSTANT_BUFFER_BLOOM,
	PS_CONSTANT_BUFFER_SSAO
} PSConstantBufferType;
PSConstantBufferType g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_NONE;

extern bool g_bDynCockpitEnabled;
extern bool g_bAOEnabled;

FILE *g_DebugFile = NULL;

/* SteamVR HMD */
extern vr::IVRSystem *g_pHMD;
extern vr::IVRCompositor *g_pVRCompositor;
extern bool g_bSteamVREnabled, g_bUseSteamVR;

void log_err(const char *format, ...)
{
	char buf[120];

	if (g_DebugFile == NULL)
		fopen_s(&g_DebugFile, "./vrerror.log", "wt");

	va_list args;
	va_start(args, format);

	vsprintf_s(buf, 120, format, args);
	fprintf(g_DebugFile, buf);

	va_end(args);
}

void close_error_file() {
	if (g_DebugFile != NULL)
		fclose(g_DebugFile);
}

void log_err_desc(char *step, HWND hWnd, HRESULT hr, CD3D11_TEXTURE2D_DESC desc) {
	log_err("step: %s\n", step);
	log_err("hWnd: 0x%x\n", hWnd);
	log_err("TEXTURE2D_DESC:\n");
	log_err("hr: 0x%x, %s\n", hr, _com_error(hr).ErrorMessage());
	log_err("ArraySize: %d\n", desc.ArraySize);
	log_err("Format: %d\n", desc.Format);
	log_err("BindFlags: %d\n", desc.BindFlags);
	log_err("CPUAccessFlags: %d\n", desc.CPUAccessFlags);
	log_err("MipLevels: %d\n", desc.MipLevels);
	log_err("MiscFlags: %d\n", desc.MiscFlags);
	log_err("SampleDesc: %d\n", desc.SampleDesc);
	log_err("Usage: %d\n", desc.Usage);
	log_err("Width: %d\n", desc.Width);
	log_err("Height: %d\n", desc.Height);
	log_err("Sample Count: %d\n", desc.SampleDesc.Count);
	log_err("Sample Quality: %d\n", desc.SampleDesc.Quality);
}

void log_shaderres_view(char *step, HWND hWnd, HRESULT hr, D3D11_SHADER_RESOURCE_VIEW_DESC desc) {
	log_err("step: %s\n", step);
	log_err("hWnd: 0x%x\n", hWnd);
	log_err("SHADER_RESOURCE_VIEW_DESC:\n");
	log_err("hr: 0x%x, %s\n", hr, _com_error(hr).ErrorMessage());
	log_err("Format: %d\n", desc.Format);
	log_err("ViewDimension: %d\n", desc.ViewDimension);
	log_err("MostDetailedMip: %d\n", desc.Texture2D.MostDetailedMip);
	log_err("MipLevels: %d\n", desc.Texture2D.MipLevels);
}

struct MainVertex
{
	float pos[2];
	float tex[2];

	MainVertex()
	{
		this->pos[0] = 0;
		this->pos[1] = 0;
		this->tex[0] = 0;
		this->tex[1] = 0;
	}

	MainVertex(float x, float y, float u, float v)
	{
		this->pos[0] = x;
		this->pos[1] = y;
		this->tex[0] = u;
		this->tex[1] = v;
	}
};

const char *DC_TARGET_COMP_SRC_RESNAME		= "dat,12000,1100,";
const char *DC_LEFT_SENSOR_SRC_RESNAME		= "dat,12000,4500,";
const char *DC_LEFT_SENSOR_2_SRC_RESNAME		= "dat,12000,300,";
const char *DC_RIGHT_SENSOR_SRC_RESNAME		= "dat,12000,4600,";
const char *DC_RIGHT_SENSOR_2_SRC_RESNAME	= "dat,12000,400,";
const char *DC_SHIELDS_SRC_RESNAME			= "dat,12000,4300,";
const char *DC_SOLID_MSG_SRC_RESNAME			= "dat,12000,100,";
const char *DC_BORDER_MSG_SRC_RESNAME		= "dat,12000,200,";
const char *DC_LASER_BOX_SRC_RESNAME			= "dat,12000,2300,";
const char *DC_ION_BOX_SRC_RESNAME			= "dat,12000,2500,";
const char *DC_BEAM_BOX_SRC_RESNAME			= "dat,12000,4400,";
const char *DC_TOP_LEFT_SRC_RESNAME			= "dat,12000,2700,";
const char *DC_TOP_RIGHT_SRC_RESNAME			= "dat,12000,2800,";

std::vector<const char *>g_HUDRegionNames = {
	"LEFT_SENSOR_REGION",		// 0
	"RIGHT_SENSOR_REGION",		// 1
	"SHIELDS_REGION",			// 2
	"BEAM_REGION",				// 3
	"TARGET_AND_LASERS_REGION",	// 4
	"LEFT_TEXT_BOX_REGION",		// 5
	"RIGHT_TEXT_BOX_REGION",		// 6
	"TOP_LEFT_REGION",			// 7
	"TOP_RIGHT_REGION"			// 8
};

std::vector<const char *>g_DCElemSrcNames = {
	"LEFT_SENSOR_SRC",			// 0
	"RIGHT_SENSOR_SRC",			// 1
	"LASER_RECHARGE_SRC",		// 2
	"SHIELD_RECHARGE_SRC",		// 3
	"ENGINE_POWER_SRC",			// 4
	"BEAM_RECHARGE_SRC",			// 5
	"SHIELDS_SRC",				// 6
	"BEAM_LEVEL_SRC"	,			// 7
	"TARGETING_COMPUTER_SRC",	// 8
	"QUAD_LASERS_LEFT_SRC",		// 9
	"QUAD_LASERS_RIGHT_SRC",		// 10
	"LEFT_TEXT_BOX_SRC",			// 11
	"RIGHT_TEXT_BOX_SRC",		// 12
	"SPEED_THROTTLE_SRC",		// 13
	"MISSILES_SRC",				// 14
	"NAME_TIME_SRC",				// 15
	"NUM_SHIPS_SRC",				// 16
	"QUAD_LASERS_BOTH_SRC",		// 17
	"DUAL_LASERS_L_SRC",			// 18
	"DUAL_LASERS_R_SRC",			// 19
	"DUAL_LASERS_BOTH_SRC",		// 20
	"B_WING_LASERS_SRC",			// 21
	"SIX_LASERS_BOTH_SRC",		// 22
	"SIX_LASERS_L_SRC",			// 23
	"SIX_LASERS_R_SRC",			// 24
};

int HUDRegionNameToIndex(char *name) {
	if (name == NULL || name[0] == '\0')
		return -1;
	for (int i = 0; i < (int )g_HUDRegionNames.size(); i++)
		if (_stricmp(name, g_HUDRegionNames[i]) == 0)
			return i;
	return -1;
}

int DCSrcElemNameToIndex(char *name) {
	if (name == NULL || name[0] == '\0')
		return -1;
	for (int i = 0; i < (int )g_DCElemSrcNames.size(); i++)
		if (_stricmp(name, g_DCElemSrcNames[i]) == 0)
			return i;
	return -1;
}

DeviceResources::DeviceResources()
{
	this->_primarySurface = nullptr;
	this->_depthSurface = nullptr;
	this->_backbufferSurface = nullptr;
	this->_frontbufferSurface = nullptr;
	this->_offscreenSurface = nullptr;

	this->_useAnisotropy = g_config.AnisotropicFilteringEnabled ? TRUE : FALSE;
	this->_useMultisampling = g_config.MultisamplingAntialiasingEnabled ? TRUE : FALSE;

	this->_sampleDesc.Count = 1;
	this->_sampleDesc.Quality = 0;

	this->_backbufferWidth = 0;
	this->_backbufferHeight = 0;
	this->_refreshRate = { 0, 1 };
	this->_are16BppTexturesSupported = false;
	this->_use16BppMainDisplayTexture = false;

	const float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	memcpy(this->clearColor, &color, sizeof(color));

	const float colorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	memcpy(this->clearColorRGBA, &colorRGBA, sizeof(colorRGBA));

	this->clearDepth = 1.0f;

	this->sceneRendered = false;
	this->sceneRenderedEmpty = false;
	this->inScene = false;

	this->_barrelEffectVertBuffer = nullptr;
	this->_HUDVertexBuffer = nullptr;
	this->_clearHUDVertexBuffer = nullptr;
	this->_hyperspaceVertexBuffer = nullptr;
	this->_bHUDVerticesReady = false;

	for (int i = 0; i < MAX_DC_SRC_ELEMENTS; i++)
		this->dc_coverTexture[i] = nullptr;
}

HRESULT DeviceResources::Initialize()
{
	HRESULT hr;

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		//D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	this->_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

	hr = D3D11CreateDevice(nullptr, this->_d3dDriverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
		D3D11_SDK_VERSION, &this->_d3dDevice, &this->_d3dFeatureLevel, &this->_d3dDeviceContext);

	if (FAILED(hr))
	{
		this->_d3dDriverType = D3D_DRIVER_TYPE_WARP;
		hr = D3D11CreateDevice(nullptr, this->_d3dDriverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &this->_d3dDevice, &this->_d3dFeatureLevel, &this->_d3dDeviceContext);
	}

	/*
	if (SUCCEEDED(hr)) {
		log_debug("[DBG] Getting the DX11.1 device");
		hr = this->_d3dDevice->QueryInterface(__uuidof(ID3D11Device1),
			reinterpret_cast<void**>(&this->_d3dDevice1));
		log_debug("[DBG] hr: 0x%x", hr);
	} else {
		this->_d3dDevice1 = NULL;
	}

	if (SUCCEEDED(hr)) {
		log_debug("[DBG] Getting ID3D11DeviceContext1 ");
		hr = this->_d3dDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1),
			reinterpret_cast<void**>(&this->_d3dDeviceContext1));
		log_debug("[DBG] hr: 0x%x", hr);
	} else {
		this->_d3dDeviceContext1 = NULL;
	}
	*/

	if (SUCCEEDED(hr))
	{
		this->CheckMultisamplingSupport();
	}

	if (SUCCEEDED(hr))
	{
		this->_are16BppTexturesSupported =
			this->IsTextureFormatSupported(DXGI_FORMAT_B4G4R4A4_UNORM)
			&& this->IsTextureFormatSupported(DXGI_FORMAT_B5G5R5A1_UNORM)
			&& this->IsTextureFormatSupported(DXGI_FORMAT_B5G6R5_UNORM);

		this->_use16BppMainDisplayTexture = this->_are16BppTexturesSupported && (this->_d3dFeatureLevel >= D3D_FEATURE_LEVEL_10_0);
	}

	if (SUCCEEDED(hr))
	{
		hr = this->LoadMainResources();
	}

	if (SUCCEEDED(hr))
	{
		hr = this->LoadResources();
	}

	if (FAILED(hr))
	{
		static bool messageShown = false;

		if (!messageShown)
		{
			MessageBox(nullptr, _com_error(hr).ErrorMessage(), __FUNCTION__, MB_ICONERROR);
		}

		messageShown = true;
	}

	return hr;
}

void DeviceResources::BuildHUDVertexBuffer(UINT width, UINT height) {
	HRESULT hr;
	D3DCOLOR color = 0xFFFFFFFF; // AABBGGRR
	auto &device = this->_d3dDevice;
	//float depth = g_fHUDDepth;
	// The values for rhw_depth and sz_depth were taken from an actual sample from the X-Wing's front panel
	float rhw_depth = 34.0f;
	float sz_depth  = 0.98f;
	D3DTLVERTEX HUDVertices[6] = { 0 };

	HUDVertices[0].sx  = 0;
	HUDVertices[0].sy  = 0;
	HUDVertices[0].sz  = sz_depth;
	HUDVertices[0].rhw = rhw_depth;
	HUDVertices[0].tu  = 0;
	HUDVertices[0].tv  = 0;
	HUDVertices[0].color = color;

	HUDVertices[1].sx = (float)width;
	HUDVertices[1].sy = 0;
	HUDVertices[1].sz  = sz_depth;
	HUDVertices[1].rhw = rhw_depth;
	HUDVertices[1].tu  = 1;
	HUDVertices[1].tv  = 0;
	HUDVertices[1].color = color;
	
	HUDVertices[2].sx = (float)width;
	HUDVertices[2].sy = (float)height;
	HUDVertices[2].sz  = sz_depth;
	HUDVertices[2].rhw = rhw_depth;
	HUDVertices[2].tu  = 1;
	HUDVertices[2].tv  = 1;
	HUDVertices[2].color = color;
	
	HUDVertices[3].sx = (float)width;
	HUDVertices[3].sy = (float)height;
	HUDVertices[3].sz  = sz_depth;
	HUDVertices[3].rhw = rhw_depth;
	HUDVertices[3].tu  = 1;
	HUDVertices[3].tv  = 1;
	HUDVertices[3].color = color;
	
	HUDVertices[4].sx = 0;
	HUDVertices[4].sy = (float)height;
	HUDVertices[4].sz  = sz_depth;
	HUDVertices[4].rhw = rhw_depth;
	HUDVertices[4].tu  = 0;
	HUDVertices[4].tv  = 1;
	HUDVertices[4].color = color;
	
	HUDVertices[5].sx  = 0;
	HUDVertices[5].sy  = 0;
	HUDVertices[5].sz  = sz_depth;
	HUDVertices[5].rhw = rhw_depth;
	HUDVertices[5].tu  = 0;
	HUDVertices[5].tv  = 0;
	HUDVertices[5].color = color;	

	/* Create the VertexBuffer if necessary */
	if (this->_HUDVertexBuffer != NULL) {
		this->_HUDVertexBuffer->Release();
		this->_HUDVertexBuffer = NULL;
	}

	if (this->_clearHUDVertexBuffer != NULL) {
		this->_clearHUDVertexBuffer->Release();
		this->_clearHUDVertexBuffer = NULL;
	}

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(D3DTLVERTEX) * ARRAYSIZE(HUDVertices);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;
	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = HUDVertices;
	hr = device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &this->_HUDVertexBuffer);
	if (FAILED(hr)) {
		log_debug("[DBG] [DC] Could not create _HUDVertexBuffer");
		this->_bHUDVerticesReady = false;
	}

	// Build the vertex buffer that will be used to clear areas of the offscreen DC buffer:
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.StructureByteStride = 0;
	hr = device->CreateBuffer(&vertexBufferDesc, nullptr, &this->_clearHUDVertexBuffer);
	if (FAILED(hr)) {
		log_debug("[DBG] [DC] Could not create _clearHUDVertexBuffer");
		this->_bHUDVerticesReady = false;
	}

	this->_bHUDVerticesReady = true;
}

void DeviceResources::BuildHyperspaceVertexBuffer(UINT width, UINT height) {
	HRESULT hr;
	D3DCOLOR color = 0xFFFFFFFF; // AABBGGRR
	auto &device = this->_d3dDevice;
	//float depth = g_fHUDDepth;
	// The values for rhw_depth and sz_depth were taken from the skybox
	float rhw_depth = 0.000863f; // this is the inverse of the depth (?)
	float sz_depth = 0.001839f;   // this is the Z-buffer value (?)
	// Why do I even have to bother? Can I just use my *own* vertex shader and do
	// away with all this silliness?
	D3DTLVERTEX HyperVertices[6] = { 0 };

	HyperVertices[0].sx  = 0;
	HyperVertices[0].sy  = 0;
	HyperVertices[0].sz  = sz_depth;
	HyperVertices[0].rhw = rhw_depth;
	HyperVertices[0].tu  = 0;
	HyperVertices[0].tv  = 0;
	HyperVertices[0].color = color;

	HyperVertices[1].sx  = (float)width;
	HyperVertices[1].sy  = 0;
	HyperVertices[1].sz  = sz_depth;
	HyperVertices[1].rhw = rhw_depth;
	HyperVertices[1].tu  = 1;
	HyperVertices[1].tv  = 0;
	HyperVertices[1].color = color;

	HyperVertices[2].sx  = (float)width;
	HyperVertices[2].sy  = (float)height;
	HyperVertices[2].sz  = sz_depth;
	HyperVertices[2].rhw = rhw_depth;
	HyperVertices[2].tu  = 1;
	HyperVertices[2].tv  = 1;
	HyperVertices[2].color = color;

	HyperVertices[3].sx  = (float)width;
	HyperVertices[3].sy  = (float)height;
	HyperVertices[3].sz  = sz_depth;
	HyperVertices[3].rhw = rhw_depth;
	HyperVertices[3].tu  = 1;
	HyperVertices[3].tv  = 1;
	HyperVertices[3].color = color;

	HyperVertices[4].sx  = 0;
	HyperVertices[4].sy  = (float)height;
	HyperVertices[4].sz  = sz_depth;
	HyperVertices[4].rhw = rhw_depth;
	HyperVertices[4].tu  = 0;
	HyperVertices[4].tv  = 1;
	HyperVertices[4].color = color;

	HyperVertices[5].sx  = 0;
	HyperVertices[5].sy  = 0;
	HyperVertices[5].sz  = sz_depth;
	HyperVertices[5].rhw = rhw_depth;
	HyperVertices[5].tu  = 0;
	HyperVertices[5].tv  = 0;
	HyperVertices[5].color = color;

	/* Create the VertexBuffer if necessary */
	if (this->_hyperspaceVertexBuffer != NULL) {
		// Calling Release here was causing a crash when the game was exiting; but
		// only if dynamic_cockpit is disabled! If DC is enabled, Release can be
		// called without problems... why?
		// Why is this not happening with the HUD vertices buffer?
		// I think the problem was that BuildHUDVertexBuffer and BuildHyperspaceVertexBuffer
		// both used d3dDevice, which probably increased the refcount for that object directly
		// without updating the refcount in DirectDraw. Making these variables members and the
		// functions methods in DeviceResources seems to have fixed these problems.
		this->_hyperspaceVertexBuffer->Release();
		this->_hyperspaceVertexBuffer = NULL;
	}

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(D3DTLVERTEX) * ARRAYSIZE(HyperVertices);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;
	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = HyperVertices;
	hr = device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &this->_hyperspaceVertexBuffer);
	if (FAILED(hr)) {
		log_debug("[DBG] [DC] Could not create g_HyperspaceVertexBuffer");
	}
}

inline float lerp(float x, float y, float s) {
	return x + s * (y - x);
}

void DeviceResources::CreateRandomVectorTexture() {
	auto& context = this->_d3dDeviceContext;
	log_debug("[DBG] Creating random vector texture for SSDO");
	const int NUM_SAMPLES = 64;
	float rawData[NUM_SAMPLES * 3];
	D3D11_TEXTURE1D_DESC textureDesc;
	textureDesc.Width = NUM_SAMPLES * 3;
	textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA textureData = { 0 };
	textureData.pSysMem = rawData;
	textureData.SysMemPitch = sizeof(float) * 3 * NUM_SAMPLES;
	textureData.SysMemSlicePitch = 0;

	// Sample kernel
	/*
		std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
	std::default_random_engine generator;
	std::vector<glm::vec3> ssdoKernel;
	for (GLuint i = 0; i < 64; ++i)
	{
		// These samples are [-1..1, -1..1, 0..1], so it looks like a hemisphere
		// I think these samples are used to compute the sssdo and bent normal
		glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);
		GLfloat scale = GLfloat(i) / 64.0;

		// Scale samples s.t. they're more aligned to center of kernel
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		ssdoKernel.push_back(sample);
	}
	*/
	//std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
	int j = 0;
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{
		// These samples are [-1..1, -1..1, 0..1], so it looks like a hemisphere
		// I think these samples are used to compute the sssdo and bent normal
		//glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
		Vector3 sample;
		sample.x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		sample.y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		sample.x = ((float)rand() / RAND_MAX);
		sample.normalize();
		//sample *= randomFloats(generator);

		float scale = (float )i / (float )NUM_SAMPLES;
		// Scale samples s.t. they're more aligned to center of kernel
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		rawData[j++] = sample.x;
		rawData[j++] = sample.y;
		rawData[j++] = sample.z;
	}

	ComPtr<ID3D11Texture1D> texture;
	HRESULT hr = this->_d3dDevice->CreateTexture1D(&textureDesc, &textureData, &texture);
	if (FAILED(hr)) {
		log_debug("[DBG] Failed when calling CreateTexture2D on SSDO kernel, reason: 0x%x",
			this->_d3dDevice->GetDeviceRemovedReason());
		goto out;
	}

out:
	//delete textureData;
	// DEBUG
	hr = DirectX::SaveDDSTextureToFile(context, texture, L"C:\\Temp\\_randomTex.dds");
	log_debug("[DBG] Dumped randomTex to file, hr: 0x%x", hr);
	texture->Release();
	// DEBUG
}

void DeviceResources::DeleteRandomVectorTexture() {
	// TODO
}

void DeviceResources::ClearDynCockpitVector(dc_element DCElements[], int size) {
	for (int i = 0; i < size; i++) {
		if (this->dc_coverTexture[i] != nullptr) {
			log_debug("[DBG] [DC] !!!! Releasing [%d][%s]", i, DCElements[i].coverTextureName);
			this->dc_coverTexture[i]->Release();
			//delete DCElements[i].coverTexture;
			//log_debug("[DBG] [DC] !!!! ref: %d", ref);
			this->dc_coverTexture[i] = nullptr;
		}
		DCElements[i].coverTextureName[0] = 0;
	}
	g_iNumDCElements = 0;
	//DCElements.clear();
}

void DeviceResources::ClearActiveCockpitVector(ac_element ACElements[], int size) {
	for (int i = 0; i < size; i++) {
		int numCoords = ACElements[i].coords.numCoords;
		ACElements[i].name[0] = 0;
		for (int j = 0; j < numCoords; j++) {
			ACElements[i].coords.action[j][0] = 0;
			ACElements[i].coords.action_name[j][0] = 0;
		}
		ACElements[i].coords.numCoords = 0;
	}
	g_iNumACElements = 0;
}

void DeviceResources::ResetDynamicCockpit() {
	if (g_bDynCockpitEnabled && g_sCurrentCockpit[0] != 0) // Testing the name of the cockpit should prevent multiple resets
	{
		ResetActiveCockpit();
		log_debug("[DBG] [DC] Resetting Dynamic Cockpit");
		// Reset the cockpit name
		g_sCurrentCockpit[0] = 0;
		// Reset the HUD boxes: this will force a re-compute of the boxes and the DC elements
		g_DCHUDRegions.ResetLimits();
		// Reset the Source DC elements so that we know when they get re-computed.
		g_DCElemSrcBoxes.Reset();
		// Reset the active slots in g_DCElements
		for (int i = 0; i < g_iNumDCElements; i++)
		{
			dc_element *elem = &g_DCElements[i];
			if (elem->bActive) {
				if (this->dc_coverTexture[i] != nullptr) {
					//log_debug("[DBG] [DC] Releasing [%d][%s]...", i, elem->coverTextureName);
					this->dc_coverTexture[i]->Release();
					//log_debug("[DBG] [DC] RELEASED");
					this->dc_coverTexture[i] = nullptr;
				}
				elem->bActive = false;
				elem->bNameHasBeenTested = false;
			}
		}
		// Reset the dynamic cockpit vector
		if (g_iNumDCElements > 0) {
			log_debug("[DBG] [DC] Clearing g_DCElements");
			ClearDynCockpitVector(g_DCElements, g_iNumDCElements);
			g_iNumDCElements = 0;
		}
	}
}

/*
 * This function is called from ResetDynamicCockpit
 */
void DeviceResources::ResetActiveCockpit() {
	if (g_bActiveCockpitEnabled)
	{
		log_debug("[DBG] [AC] Resetting Active Cockpit");
		// Reset the active slots in g_ACElements
		for (int i = 0; i < g_iNumACElements; i++)
		{
			ac_element *elem = &g_ACElements[i];
			elem->bActive = false;
			elem->bNameHasBeenTested = false;
		}

		// Reset the active cockpit vector
		if (g_iNumACElements > 0) {
			log_debug("[DBG] [AC] Clearing g_ACElements");
			ClearActiveCockpitVector(g_ACElements, g_iNumACElements);
			g_iNumACElements = 0;
		}
	}
}

HRESULT DeviceResources::OnSizeChanged(HWND hWnd, DWORD dwWidth, DWORD dwHeight)
{
	/*
	 * When the concourse is displayed, dwWidth,dwHeight = 640x480 regardless of actual in-game res or screen res.
	 * When 3D content is displayed, dwWidth,dwHeight = in-game resolution (1280x1024, 1600x1200, etc)
	 */
	HRESULT hr;
	char* step = "";
	DXGI_FORMAT oldFormat;

	//log_debug("[DBG] OnSizeChanged called");
	// Generic VR Initialization
	// Replace the game's WndProc
	if (!g_bWndProcReplaced) {
		ReplaceWindowProc(hWnd);
		g_bWndProcReplaced = true;
	}

	//log_debug("[DBG] OnSizeChanged, dwWidth,Height: %d, %d", dwWidth, dwHeight);

	if (g_bUseSteamVR) {
		// dwWidth, dwHeight are the in-game's resolution
		// When using SteamVR, let's override the size with the recommended size
		dwWidth = g_steamVRWidth;
		dwHeight = g_steamVRHeight;
	}

	// Reset the present counter
	g_iPresentCounter = 0;
	// Reset the FOV application flag
	g_bCustomFOVApplied = false;

	DeleteRandomVectorTexture();
	this->_depthStencilViewL.Release();
	this->_depthStencilViewR.Release();
	this->_depthStencilL.Release();
	this->_depthStencilR.Release();
	this->_renderTargetView.Release();
	this->_renderTargetViewPost.Release();
	this->_offscreenAsInputShaderResourceView.Release();
	this->_offscreenBuffer.Release();
	this->_offscreenBufferAsInput.Release();
	this->_offscreenBufferPost.Release();
	this->_shadertoyBuf.Release();
	this->_shadertoyAuxBuf.Release();
	this->_shadertoyRTV.Release();
	this->_shadertoySRV.Release();
	this->_shadertoyAuxSRV.Release();
	if (g_bUseSteamVR) {
		this->_offscreenBufferR.Release();
		this->_offscreenBufferAsInputR.Release();
		this->_offscreenBufferPostR.Release();
		this->_offscreenAsInputShaderResourceViewR.Release();
		this->_renderTargetViewR.Release();
		this->_renderTargetViewPostR.Release();
		this->_steamVRPresentBuffer.Release();
		this->_renderTargetViewSteamVRResize.Release();
		this->_shadertoyBufR.Release();
		this->_shadertoyAuxBufR.Release();
		this->_shadertoyRTV_R.Release();
		this->_shadertoySRV_R.Release();
		this->_shadertoyAuxSRV_R.Release();
	}

	if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
		ResetDynamicCockpit();
		this->_renderTargetViewDynCockpit.Release();
		this->_renderTargetViewDynCockpitBG.Release();
		this->_renderTargetViewDynCockpitAsInput.Release();
		this->_renderTargetViewDynCockpitAsInputBG.Release();
		this->_offscreenBufferDynCockpit.Release();
		this->_offscreenBufferDynCockpitBG.Release();
		this->_offscreenAsInputDynCockpit.Release();
		this->_offscreenAsInputDynCockpitBG.Release();
		this->_offscreenAsInputSRVDynCockpit.Release();
		this->_offscreenAsInputSRVDynCockpitBG.Release();
	}

	if (g_bActiveCockpitEnabled) {
		ClearActiveCockpitVector(g_ACElements, g_iNumACElements);
	}

	// offscreenBufferBloomMask/AsInputBloomMask/SRV/RTVs are used for SSDO (and SSAO?)
	// _bloomOutput1 is used as temp buff during SSDO
	if (g_bReshadeEnabled) {
		this->_offscreenBufferBloomMask.Release();
		this->_offscreenBufferAsInputBloomMask.Release();
		this->_offscreenAsInputBloomMaskSRV.Release();
		this->_renderTargetViewBloomMask.Release();
		this->_bloomOutput1.Release();
		this->_bloomOutput1SRV.Release();

		this->_ssaoMask.Release();
		this->_ssMask.Release();
		this->_normBuf.Release();

		this->_ssaoMaskSRV.Release();
		this->_ssMaskSRV.Release();
		this->_normBufSRV.Release();

		this->_renderTargetViewNormBuf.Release();
		this->_renderTargetViewSSAOMask.Release();
		this->_renderTargetViewSSMask.Release();
		if (g_bUseSteamVR) {
			this->_offscreenBufferBloomMaskR.Release();
			this->_offscreenBufferAsInputBloomMaskR.Release();
			this->_offscreenAsInputBloomMaskSRV_R.Release();
			this->_renderTargetViewBloomMaskR.Release();

			this->_ssaoMaskR.Release();
			this->_ssMaskR.Release();
			this->_normBufR.Release();

			this->_ssaoMaskSRV_R.Release();
			this->_ssMaskSRV_R.Release();
			this->_normBufSRV_R.Release();

			this->_renderTargetViewNormBufR.Release();
			this->_renderTargetViewSSAOMaskR.Release();
			this->_renderTargetViewSSMaskR.Release();
		}
		ClearOPTnames();
		ClearCraftMaterials();
	}

	if (g_bBloomEnabled) {
		//this->_offscreenAsInputBloomMaskSRV.Release();
		//this->_renderTargetViewBloomMask.Release();
		this->_renderTargetViewBloom1.Release();
		this->_renderTargetViewBloom2.Release();
		this->_renderTargetViewBloomSum.Release();
		//this->_bloomOutput1.Release();
		this->_bloomOutput2.Release();
		this->_bloomOutputSum.Release();
		//this->_bloomOutput1SRV.Release();
		this->_bloomOutput2SRV.Release();
		this->_bloomOutputSumSRV.Release();
		if (g_bUseSteamVR) {
			/*this->_offscreenBufferBloomMaskR.Release();
			this->_offscreenBufferAsInputBloomMaskR.Release();
			this->_offscreenAsInputBloomMaskSRV_R.Release();
			this->_renderTargetViewBloomMaskR.Release();*/
			this->_bloomOutput1R.Release();
			this->_bloomOutput2R.Release();
			this->_bloomOutputSumR.Release();
			this->_renderTargetViewBloom1R.Release();
			this->_renderTargetViewBloom2R.Release();
			this->_renderTargetViewBloomSumR.Release();
			this->_bloomOutput1SRV_R.Release();
			this->_bloomOutput2SRV_R.Release();
			this->_bloomOutputSumSRV_R.Release();
		}
	}

	if (g_bAOEnabled) {
		this->_depthBuf.Release();
		this->_depthBufAsInput.Release();
		this->_depthBuf2.Release();
		this->_depthBuf2AsInput.Release();
		
		this->_bentBuf.Release();
		this->_bentBufR.Release(); // bentBufR is used to hold a copy of bentBuf to blur it (and viceversa)
		this->_ssaoBuf.Release();
		this->_ssaoBufR.Release(); // ssaoBufR will be used to store SSDO Indirect (and viceversa)
		this->_renderTargetViewDepthBuf.Release();
		this->_depthBufSRV.Release();
		this->_depthBuf2SRV.Release();
		this->_bentBufSRV.Release();
		this->_bentBufSRV_R.Release();
		this->_renderTargetViewBentBuf.Release();
		this->_renderTargetViewSSAO.Release();
		this->_renderTargetViewSSAO_R.Release();
		this->_ssaoBufSRV.Release();
		this->_ssaoBufSRV_R.Release();
		if (g_bUseSteamVR) {
			this->_depthBufR.Release();
			this->_depthBufAsInputR.Release();
			this->_depthBuf2R.Release();
			this->_depthBuf2AsInputR.Release();
			this->_renderTargetViewDepthBufR.Release();
			this->_depthBufSRV_R.Release();
			this->_depthBuf2SRV_R.Release();
			this->_renderTargetViewBentBufR.Release();
		}
	}

	this->_backBuffer.Release();
	this->_swapChain.Release();

	this->_refreshRate = { 0, 1 };

	if (hWnd != nullptr)
	{
		step = "RenderTarget SwapChain";
		ComPtr<IDXGIDevice> dxgiDevice;
		ComPtr<IDXGIAdapter> dxgiAdapter;
		ComPtr<IDXGIOutput> dxgiOutput;

		hr = this->_d3dDevice.As(&dxgiDevice);

		if (SUCCEEDED(hr))
		{
			hr = dxgiDevice->GetAdapter(&dxgiAdapter);

			if (SUCCEEDED(hr))
			{
				hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
			}
		}

		DXGI_MODE_DESC md{};

		if (SUCCEEDED(hr))
		{
			md.Format = BACKBUFFER_FORMAT;

			hr = dxgiOutput->FindClosestMatchingMode(&md, &md, nullptr);
		}

		if (SUCCEEDED(hr))
		{
			DXGI_SWAP_CHAIN_DESC sd{};
			sd.BufferCount = 2;
			sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
			sd.BufferDesc.Width  = 0;
			sd.BufferDesc.Height = 0;
			sd.BufferDesc.Format = BACKBUFFER_FORMAT;
			sd.BufferDesc.RefreshRate = md.RefreshRate;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = hWnd;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;

			ComPtr<IDXGIFactory> dxgiFactory;
			hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

			if (SUCCEEDED(hr))
			{
				hr = dxgiFactory->CreateSwapChain(this->_d3dDevice, &sd, &this->_swapChain);
			}
			
			if (SUCCEEDED(hr))
			{
				this->_refreshRate = sd.BufferDesc.RefreshRate;
			}

			if (SUCCEEDED(hr))
			{
				DXGI_SWAP_CHAIN_DESC sd{};
				this->_swapChain->GetDesc(&sd);
				g_FullScreenWidth = sd.BufferDesc.Width;
				g_FullScreenHeight = sd.BufferDesc.Height;
				g_fCurScreenWidth = (float)sd.BufferDesc.Width;
				g_fCurScreenHeight = (float)sd.BufferDesc.Height;
				g_fCurScreenWidthRcp  = 1.0f / g_fCurScreenWidth;
				g_fCurScreenHeightRcp = 1.0f / g_fCurScreenHeight;
				//log_debug("[DBG] Fullscreen size: %d, %d", g_FullScreenWidth, g_FullScreenHeight);
			}
		}

		if (SUCCEEDED(hr))
		{
			hr = this->_swapChain->GetBuffer(0, IID_PPV_ARGS(&this->_backBuffer));
		}
	}
	else
	{
		step = "RenderTarget Texture2D";

		CD3D11_TEXTURE2D_DESC backBufferDesc(
			BACKBUFFER_FORMAT,
			dwWidth,
			dwHeight,
			1,
			1,
			D3D11_BIND_RENDER_TARGET,
			D3D11_USAGE_DEFAULT,
			0,
			1,
			0,
			0);

		hr = this->_d3dDevice->CreateTexture2D(&backBufferDesc, nullptr, &this->_backBuffer);
	}

	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC backBufferDesc;
		this->_backBuffer->GetDesc(&backBufferDesc);

		this->_backbufferWidth = backBufferDesc.Width;
		this->_backbufferHeight = backBufferDesc.Height;
		//g_fCurScreenWidth = _backbufferWidth;
		//g_fCurScreenHeight = _backbufferHeight;
		//log_debug("[DBG] Screen size: %f, %f", g_fCurScreenWidth, g_fCurScreenHeight);
	}

	if (SUCCEEDED(hr))
	{
		CD3D11_TEXTURE2D_DESC desc(
			BACKBUFFER_FORMAT,
			this->_backbufferWidth,
			this->_backbufferHeight,
			1,
			1,
			D3D11_BIND_RENDER_TARGET,
			D3D11_USAGE_DEFAULT,
			0,
			this->_sampleDesc.Count,
			this->_sampleDesc.Quality,
			0);
		oldFormat = desc.Format;
		log_debug("[DBG] MSAA Count: %d, Quality: %d", this->_sampleDesc.Count, this->_sampleDesc.Quality);

		step = "_offscreenBuffer";
		hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBuffer);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_err_desc(step, hWnd, hr, desc);
			goto out;
		}

		if (g_bUseSteamVR) {
			step = "_offscreenBufferR";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
		}

		step = "_offscreenBufferPost";
		// offscreenBufferPost should be just like offscreenBuffer because it will be bound as a renderTarget
		hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferPost);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_err_desc(step, hWnd, hr, desc);
			goto out;
		}

		if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
			step = "_offscreenBufferDynCockpit";
			// _offscreenBufferDynCockpit should be just like offscreenBuffer because it will be used as a renderTarget
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferDynCockpit);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_offscreenBufferDynCockpitBG";
			// _offscreenBufferDynCockpit should be just like offscreenBuffer because it will be used as a renderTarget
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferDynCockpitBG);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
		}

		if (g_bUseSteamVR) {
			step = "_offscreenBufferPostR";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferPostR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_steamVRPresentBuffer";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_steamVRPresentBuffer);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
		}

		if (g_bReshadeEnabled) {
			step = "_offscreenBufferBloomMask";
			// _offscreenBufferReshade should be just like offscreenBuffer because it will be used as a renderTarget
			desc.Format = BLOOM_BUFFER_FORMAT;
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferBloomMask);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				step = "_offscreenBufferBloomMaskR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferBloomMaskR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}

			desc.Format = oldFormat;
		}

		if (g_bAOEnabled) {
			desc.Format = AO_DEPTH_BUFFER_FORMAT;
			
			// _depthBuf will be used as a renderTarget
			step = "_depthBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_depthBuf2";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBuf2);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				// _depthBuf should be just like offscreenBuffer because it will be used as a renderTarget
				step = "_depthBufR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBufR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}

				step = "_depthBuf2R";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBuf2R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}

			desc.Format = oldFormat;
		}

		// No MSAA after this point
		// offscreenBufferAsInput must not have MSAA enabled since it will be used as input for the barrel shader.
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		step = "offscreenBufferAsInput";
		hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferAsInput);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_err_desc(step, hWnd, hr, desc);
			goto out;
		}

		if (g_bUseSteamVR) {
			step = "_offscreenBufferAsInputR";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferAsInputR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
		}

		if (g_bReshadeEnabled) {
			desc.Format = BLOOM_BUFFER_FORMAT;
			step = "_offscreenBufferAsInputBloomMask";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferAsInputBloomMask);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				step = "_offscreenBufferAsInputBloomMaskR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenBufferAsInputBloomMaskR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}
			desc.Format = oldFormat;
		}

		{
			// These guys should be the last to be created because they modify the BindFlags to
			// add D3D11_BIND_RENDER_TARGET
			UINT curFlags = desc.BindFlags;
			desc.Format = BLOOM_BUFFER_FORMAT;
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET; // SRV + RTV
			log_err("Added D3D11_BIND_RENDER_TARGET flag\n");
			log_err("Flags: 0x%x\n", desc.BindFlags);

			step = "_bloomOutput1";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutput1);
			if (FAILED(hr)) {
				log_err("Failed to create _bloomOutput1\n");
				log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
			else {
				log_err("Successfully created _bloomOutput1 with combined flags\n");
			}
			desc.Format = oldFormat;

			step = "_shadertoyBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_shadertoyBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_shadertoyAuxBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_shadertoyAuxBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				step = "_shadertoyBufR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_shadertoyBufR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}

				step = "_shadertoyAuxBufR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_shadertoyAuxBufR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}
		}

		if (g_bBloomEnabled) {
			// These guys should be the last to be created because they modify the BindFlags to
			// add D3D11_BIND_RENDER_TARGET
			desc.Format = BLOOM_BUFFER_FORMAT;
			UINT curFlags = desc.BindFlags;
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			log_err("Added D3D11_BIND_RENDER_TARGET flag\n");
			log_err("Flags: 0x%x\n", desc.BindFlags);

			step = "_bloomOutput2";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutput2);
			if (FAILED(hr)) {
				log_err("Failed to create _bloomOutput2\n");
				log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			} else {
				log_err("Successfully created _bloomOutput2 with combined flags\n");
			}

			step = "_bloomOutputSum";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutputSum);
			if (FAILED(hr)) {
				log_err("Failed to create _bloomOutputSum\n");
				log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}
			else {
				log_err("Successfully created _bloomOutputSum with combined flags\n");
			}

			if (g_bUseSteamVR) {
				step = "_bloomOutput1R";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutput1R);
				if (FAILED(hr)) {
					log_err("Failed to create _bloomOutput1R\n");
					log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
				else {
					log_err("Successfully created _bloomOutput1R with combined flags\n");
				}

				step = "_bloomOutput2R";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutput2R);
				if (FAILED(hr)) {
					log_err("Failed to create _bloomOutput2R\n");
					log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
				else {
					log_err("Successfully created _bloomOutput2R with combined flags\n");
				}

				step = "_bloomOutputSumR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bloomOutputSumR);
				if (FAILED(hr)) {
					log_err("Failed to create _bloomOutputSumR\n");
					log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
				else {
					log_err("Successfully created _bloomOutputSumR with combined flags\n");
				}
			}
			
			// Restore the previous bind flags, just in case there is a dependency on these later on
			desc.BindFlags = curFlags;
			// Restore the non-float format
			desc.Format = oldFormat;
		}

		// Create the DC Input Buffers
		if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
			// This guy should be the last one to be created because it modifies the BindFlags
			// _offscreenBufferAsInputDynCockpit should have the same properties as _offscreenBufferAsInput
			UINT curFlags = desc.BindFlags;
			desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			log_err("_offscreenBufferAsInputDynCockpit: Added D3D11_BIND_RENDER_TARGET flag\n");
			log_err("Flags: 0x%x\n", desc.BindFlags);

			step = "_offscreenBufferAsInputDynCockpit";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenAsInputDynCockpit);
			if (FAILED(hr)) {
				log_err("Failed to create _offscreenBufferAsInputDynCockpit, error: 0x%x\n", hr);
				log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			} else {
				log_err("Successfully created _offscreenBufferAsInputDynCockpit with combined flags\n");
			}

			step = "_offscreenBufferAsInputDynCockpitBG";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_offscreenAsInputDynCockpitBG);
			if (FAILED(hr)) {
				log_err("Failed to create _offscreenBufferAsInputDynCockpitBG, error: 0x%x\n", hr);
				log_err("GetDeviceRemovedReason: 0x%x\n", this->_d3dDevice->GetDeviceRemovedReason());
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			} else {
				log_err("Successfully created _offscreenBufferAsInputDynCockpitBG with combined flags\n");
			}

			// Restore the previous bind flags, just in case there is a dependency on these later on
			desc.BindFlags = curFlags;
		}

		// Shading System: Create Non-MSAA SSAO/SS Mask Buffers
		if (g_bReshadeEnabled) {
			UINT oldFlags = desc.BindFlags;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Format = AO_MASK_FORMAT;

			step = "_ssaoMask";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssaoMask);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_ssMask";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssMask);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			desc.Format = AO_DEPTH_BUFFER_FORMAT;
			step = "_normBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_normBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				desc.Format = AO_MASK_FORMAT;
				step = "_ssaoMaskR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssaoMaskR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}

				step = "_ssMaskR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssMaskR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}

				desc.Format = AO_DEPTH_BUFFER_FORMAT;
				step = "_normBufR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_normBufR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}

			desc.Format = oldFormat;
			desc.BindFlags = oldFlags;
		}

		// Create Non-MSAA AO Buffers
		if (g_bAOEnabled) {
			UINT oldFlags = desc.BindFlags;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Format = AO_DEPTH_BUFFER_FORMAT;

			step = "_depthBufAsInput";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBufAsInput);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_depthBuf2AsInput";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBuf2AsInput);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			if (g_bUseSteamVR) {
				step = "_depthBufR";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBufAsInputR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}

				step = "_depthBuf2R";
				hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_depthBuf2AsInputR);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_err_desc(step, hWnd, hr, desc);
					goto out;
				}
			}

			// Add the RTV flag
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			step = "_bentBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bentBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			step = "_bentBufR";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_bentBufR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			desc.Format = HDR_FORMAT;
			step = "_ssaoBuf";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssaoBuf);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			desc.Format = HDR_FORMAT;
			step = "_ssaoBufR";
			hr = this->_d3dDevice->CreateTexture2D(&desc, nullptr, &this->_ssaoBufR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_err_desc(step, hWnd, hr, desc);
				goto out;
			}

			desc.Format = oldFormat;
			desc.BindFlags = oldFlags;
		}

		// Create the shader resource views for both offscreen buffers
		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		// Create a shader resource view for the _offscreenBuffer
		shaderResourceViewDesc.Format = desc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		D3D11_SRV_DIMENSION curDimension = shaderResourceViewDesc.ViewDimension;

		/* SRVs */
		// Create the shader resource view for offscreenBufferAsInput
		step = "offscreenAsInputShaderResourceView";
		hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenBufferAsInput,
			&shaderResourceViewDesc, &this->_offscreenAsInputShaderResourceView);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
			goto out;
		}

		step = "_shadertoySRV";
		hr = this->_d3dDevice->CreateShaderResourceView(this->_shadertoyBuf, &shaderResourceViewDesc, &this->_shadertoySRV);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
			goto out;
		}

		step = "_shadertoyAuxSRV";
		hr = this->_d3dDevice->CreateShaderResourceView(this->_shadertoyAuxBuf, &shaderResourceViewDesc, &this->_shadertoyAuxSRV);
		if (FAILED(hr)) {
			log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
			log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
			goto out;
		}

		if (g_bUseSteamVR) {
			// Create the shader resource view for offscreenBufferAsInputR
			step = "offscreenAsInputShaderResourceViewR";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenBufferAsInputR,
				&shaderResourceViewDesc, &this->_offscreenAsInputShaderResourceViewR);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_shadertoySRV_R";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_shadertoyBufR, &shaderResourceViewDesc, &this->_shadertoySRV_R);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_shadertoyAuxSRV_R";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_shadertoyAuxBufR, &shaderResourceViewDesc, &this->_shadertoyAuxSRV_R);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}
		}

		// Bloom SRVs
		if (g_bReshadeEnabled) 
		{
			shaderResourceViewDesc.Format = BLOOM_BUFFER_FORMAT;
			step = "_offscreenAsInputBloomMaskSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenBufferAsInputBloomMask,
				&shaderResourceViewDesc, &this->_offscreenAsInputBloomMaskSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			if (g_bUseSteamVR)
			{
				step = "_offscreenAsInputBloomSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenBufferAsInputBloomMaskR,
					&shaderResourceViewDesc, &this->_offscreenAsInputBloomMaskSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}
			}
		
			step = "_bloomOutput1SRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutput1,
				&shaderResourceViewDesc, &this->_bloomOutput1SRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}
			shaderResourceViewDesc.Format = oldFormat;
		}

		if (g_bBloomEnabled) {
			shaderResourceViewDesc.Format = BLOOM_BUFFER_FORMAT;
			step = "_bloomOutput2SRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutput2,
				&shaderResourceViewDesc, &this->_bloomOutput2SRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_bloomOutputSumSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutputSum,
				&shaderResourceViewDesc, &this->_bloomOutputSumSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			if (g_bUseSteamVR) {
				shaderResourceViewDesc.Format = BLOOM_BUFFER_FORMAT;
				
				step = "_bloomOutput1SRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutput1R,
					&shaderResourceViewDesc, &this->_bloomOutput1SRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

				step = "_bloomOutput2SRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutput2R,
					&shaderResourceViewDesc, &this->_bloomOutput2SRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

				step = "_bloomOutputSumSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_bloomOutputSumR,
					&shaderResourceViewDesc, &this->_bloomOutputSumSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}
			}
			shaderResourceViewDesc.Format = oldFormat;
		}

		// Shading System SRVs
		if (g_bReshadeEnabled) {
			DXGI_FORMAT oldFormat = shaderResourceViewDesc.Format;
			shaderResourceViewDesc.Format = AO_MASK_FORMAT;
			step = "_ssaoMaskSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_ssaoMask, &shaderResourceViewDesc, &this->_ssaoMaskSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_ssMaskSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_ssMask, &shaderResourceViewDesc, &this->_ssMaskSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			shaderResourceViewDesc.Format = AO_DEPTH_BUFFER_FORMAT;
			step = "_normBufSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_normBuf, &shaderResourceViewDesc, &this->_normBufSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			if (g_bUseSteamVR) {
				shaderResourceViewDesc.Format = AO_MASK_FORMAT;
				step = "_ssaoMaskSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_ssaoMaskR, &shaderResourceViewDesc, &this->_ssaoMaskSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

				step = "_ssMaskSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_ssMaskR, &shaderResourceViewDesc, &this->_ssMaskSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

				shaderResourceViewDesc.Format = AO_DEPTH_BUFFER_FORMAT;
				step = "_normBufSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_normBufR, &shaderResourceViewDesc, &this->_normBufSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}
			}
			shaderResourceViewDesc.Format = oldFormat;
		}

		// AO SRVs
		if (g_bAOEnabled) {
			shaderResourceViewDesc.Format = AO_DEPTH_BUFFER_FORMAT;

			step = "_depthBufSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_depthBufAsInput,
				&shaderResourceViewDesc, &this->_depthBufSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_depthBuf2SRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_depthBuf2AsInput,
				&shaderResourceViewDesc, &this->_depthBuf2SRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_bentBufSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_bentBuf, &shaderResourceViewDesc, &this->_bentBufSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_bentBufSRV_R";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_bentBufR, &shaderResourceViewDesc, &this->_bentBufSRV_R);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}
			
			shaderResourceViewDesc.Format = HDR_FORMAT;
			step = "_ssaoBufSRV";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_ssaoBuf, &shaderResourceViewDesc, &this->_ssaoBufSRV);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			step = "_ssaoBufSRV_R";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_ssaoBufR, &shaderResourceViewDesc, &this->_ssaoBufSRV_R);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			if (g_bUseSteamVR) {
				shaderResourceViewDesc.Format = AO_DEPTH_BUFFER_FORMAT;
				step = "_depthBufSRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_depthBufAsInputR,
					&shaderResourceViewDesc, &this->_depthBufSRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

				step = "_depthBuf2SRV_R";
				hr = this->_d3dDevice->CreateShaderResourceView(this->_depthBuf2AsInputR,
					&shaderResourceViewDesc, &this->_depthBuf2SRV_R);
				if (FAILED(hr)) {
					log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
					log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
					goto out;
				}

			}
			shaderResourceViewDesc.Format = oldFormat;
		}

		if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
			// Create the SRV for _offscreenBufferAsInputDynCockpit
			step = "_offscreenBufferAsInputDynCockpit";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenAsInputDynCockpit,
				&shaderResourceViewDesc, &this->_offscreenAsInputSRVDynCockpit);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}

			// Create the SRV for _offscreenBufferAsInputDynCockpitBG
			step = "_offscreenBufferAsInputDynCockpitBG";
			hr = this->_d3dDevice->CreateShaderResourceView(this->_offscreenAsInputDynCockpitBG,
				&shaderResourceViewDesc, &this->_offscreenAsInputSRVDynCockpitBG);
			if (FAILED(hr)) {
				log_err("dwWidth, Height: %u, %u\n", dwWidth, dwHeight);
				log_shaderres_view(step, hWnd, hr, shaderResourceViewDesc);
				goto out;
			}
		}

		// Build the HUD vertex buffer
		BuildHUDVertexBuffer(_displayWidth, _displayHeight);
		BuildHyperspaceVertexBuffer(_displayWidth, _displayHeight);
		CreateRandomVectorTexture();
		g_fCurInGameWidth = (float)_displayWidth;
		g_fCurInGameHeight = (float)_displayHeight;
	}

	/* RTVs */
	if (SUCCEEDED(hr))
	{
		CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(this->_useMultisampling ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D);
		CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDescNoMSAA(D3D11_RTV_DIMENSION_TEXTURE2D);

		step = "_renderTargetView";
		hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBuffer, &renderTargetViewDesc, &this->_renderTargetView);
		if (FAILED(hr)) goto out;

		step = "_renderTargetViewPost";
		hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferPost, &renderTargetViewDesc, &this->_renderTargetViewPost);
		if (FAILED(hr)) goto out;

		step = "_shadertoyRTV";
		// This RTV writes to a non-MSAA texture
		hr = this->_d3dDevice->CreateRenderTargetView(this->_shadertoyBuf, &renderTargetViewDescNoMSAA, &this->_shadertoyRTV);
		if (FAILED(hr)) {
			log_debug("[DBG] [ST] _shadertoyRTV FAILED");
			goto out;
		}

		if (g_bUseSteamVR) {
			step = "RenderTargetViewR";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferR, &renderTargetViewDesc, &this->_renderTargetViewR);
			if (FAILED(hr)) goto out;

			step = "RenderTargetViewPostR";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferPostR, &renderTargetViewDesc, &this->_renderTargetViewPostR);
			if (FAILED(hr)) goto out;

			step = "renderTargetViewSteamVRResize";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_steamVRPresentBuffer, &renderTargetViewDesc, &this->_renderTargetViewSteamVRResize);
			if (FAILED(hr)) goto out;

			step = "_shadertoyRTV_R";
			// This RTV writes to a non-MSAA texture
			hr = this->_d3dDevice->CreateRenderTargetView(this->_shadertoyBufR, &renderTargetViewDescNoMSAA, &this->_shadertoyRTV_R);
			if (FAILED(hr)) {
				log_debug("[DBG] [ST] _shadertoyRTV_R FAILED");
				goto out;
			}
		}

		if (g_bDynCockpitEnabled || g_bReshadeEnabled) {
			step = "_renderTargetViewDynCockpit";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferDynCockpit, &renderTargetViewDesc, &this->_renderTargetViewDynCockpit);
			if (FAILED(hr)) {
				log_debug("[DBG] [DC] _renderTargetViewDynCockpit FAILED");
				goto out;
			}

			step = "_renderTargetViewDynCockpitBG";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferDynCockpitBG, &renderTargetViewDesc, &this->_renderTargetViewDynCockpitBG);
			if (FAILED(hr)) {
				log_debug("[DBG] [DC] _renderTargetViewDynCockpitBG FAILED");
				goto out;
			}
			
			step = "_renderTargetViewDynCockpitAsInput";
			// This RTV writes to a non-MSAA texture
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenAsInputDynCockpit, &renderTargetViewDescNoMSAA,
				&this->_renderTargetViewDynCockpitAsInput);
			if (FAILED(hr)) {
				log_debug("[DBG] [DC] _renderTargetViewDynCockpitAsInput FAILED");
				goto out;
			}

			step = "_renderTargetViewDynCockpitAsInputBG";
			// This RTV writes to a non-MSAA texture
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenAsInputDynCockpitBG, &renderTargetViewDescNoMSAA,
				&this->_renderTargetViewDynCockpitAsInputBG);
			if (FAILED(hr)) {
				log_debug("[DBG] [DC] _renderTargetViewDynCockpitAsInputBG FAILED");
				goto out;
			}
		}

		if (g_bReshadeEnabled) {
			renderTargetViewDesc.Format = BLOOM_BUFFER_FORMAT;
			CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDescNoMSAA(D3D11_RTV_DIMENSION_TEXTURE2D);
			
			step = "_renderTargetViewBloomMask";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferBloomMask, &renderTargetViewDesc, &this->_renderTargetViewBloomMask);
			if (FAILED(hr)) goto out;

			renderTargetViewDescNoMSAA.Format = AO_MASK_FORMAT;
			step = "_renderTargetViewSSAOMask";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_ssaoMask, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSAOMask);
			if (FAILED(hr)) goto out;

			step = "_renderTargetViewSSMask";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_ssMask, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSMask);
			if (FAILED(hr)) goto out;

			renderTargetViewDescNoMSAA.Format = AO_DEPTH_BUFFER_FORMAT;
			step = "_renderTargetViewNormBuf";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_normBuf, &renderTargetViewDescNoMSAA, &this->_renderTargetViewNormBuf);
			if (FAILED(hr)) goto out;

			if (g_bUseSteamVR) {
				renderTargetViewDesc.Format = BLOOM_BUFFER_FORMAT;
				step = "_renderTargetViewBloomMaskR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_offscreenBufferBloomMaskR, &renderTargetViewDesc, &this->_renderTargetViewBloomMaskR);
				if (FAILED(hr)) goto out;

				renderTargetViewDescNoMSAA.Format = AO_MASK_FORMAT;
				step = "_renderTargetViewSSAOMaskR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_ssaoMaskR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSAOMaskR);
				if (FAILED(hr)) goto out;

				step = "_renderTargetViewSSMaskR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_ssMaskR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSMaskR);
				if (FAILED(hr)) goto out;

				renderTargetViewDescNoMSAA.Format = AO_DEPTH_BUFFER_FORMAT;
				step = "_renderTargetViewNormBufR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_normBufR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewNormBufR);
				if (FAILED(hr)) goto out;
			}

			renderTargetViewDesc.Format = oldFormat;
		}

		// Bloom RTVs
		if (g_bBloomEnabled) {
			renderTargetViewDesc.Format = BLOOM_BUFFER_FORMAT;
						
			// These RTVs render to non-MSAA buffers
			CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDescNoMSAA(D3D11_RTV_DIMENSION_TEXTURE2D);
			step = "_renderTargetViewBloom1";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutput1, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloom1);
			if (FAILED(hr)) goto out;
			
			step = "_renderTargetViewBloom2";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutput2, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloom2);
			if (FAILED(hr)) goto out;

			step = "_renderTargetViewBloomSum";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutputSum, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloomSum);
			if (FAILED(hr)) goto out;

			if (g_bUseSteamVR) {
				step = "_renderTargetViewBloom1R";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutput1R, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloom1R);
				if (FAILED(hr)) goto out;

				step = "_renderTargetViewBloom2R";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutput2R, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloom2R);
				if (FAILED(hr)) goto out;

				step = "_renderTargetViewBloomSumR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_bloomOutputSumR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBloomSumR);
				if (FAILED(hr)) goto out;
			}

			renderTargetViewDesc.Format = oldFormat;
		}

		if (g_bAOEnabled) {
			renderTargetViewDesc.Format = AO_DEPTH_BUFFER_FORMAT;
			CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDescNoMSAA(D3D11_RTV_DIMENSION_TEXTURE2D);
			renderTargetViewDescNoMSAA.Format = AO_DEPTH_BUFFER_FORMAT;

			step = "_renderTargetViewDepthBuf";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_depthBuf, &renderTargetViewDesc, &this->_renderTargetViewDepthBuf);
			if (FAILED(hr)) goto out;

			step = "_renderTargetViewDepthBuf2";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_depthBuf2, &renderTargetViewDesc, &this->_renderTargetViewDepthBuf2);
			if (FAILED(hr)) goto out;

			step = "_renderTargetViewBentBuf";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_bentBuf, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBentBuf);
			if (FAILED(hr)) goto out;

			renderTargetViewDescNoMSAA.Format = HDR_FORMAT;
			step = "_renderTargetViewSSAO";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_ssaoBuf, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSAO);
			if (FAILED(hr)) goto out;

			step = "_renderTargetViewSSAO_R";
			hr = this->_d3dDevice->CreateRenderTargetView(this->_ssaoBufR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewSSAO_R);
			if (FAILED(hr)) goto out;

			if (g_bUseSteamVR) {
				renderTargetViewDescNoMSAA.Format = AO_DEPTH_BUFFER_FORMAT;
				step = "_renderTargetViewDepthBufR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_depthBufR, &renderTargetViewDesc, &this->_renderTargetViewDepthBufR);
				if (FAILED(hr)) goto out;

				step = "_renderTargetViewDepthBuf2R";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_depthBuf2R, &renderTargetViewDesc, &this->_renderTargetViewDepthBuf2R);
				if (FAILED(hr)) goto out;

				/*step = "_renderTargetViewBentBufR";
				hr = this->_d3dDevice->CreateRenderTargetView(this->_bentBufR, &renderTargetViewDescNoMSAA, &this->_renderTargetViewBentBufR);
				if (FAILED(hr)) goto out;*/
			}

			renderTargetViewDesc.Format = oldFormat;
		}
	}

	/* depth stencil */
	if (SUCCEEDED(hr))
	{
		step = "DepthStencil";
		D3D11_TEXTURE2D_DESC depthStencilDesc;
		depthStencilDesc.Width = this->_backbufferWidth;
		depthStencilDesc.Height = this->_backbufferHeight;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.SampleDesc.Count = this->_sampleDesc.Count;
		depthStencilDesc.SampleDesc.Quality = this->_sampleDesc.Quality;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		hr = this->_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &this->_depthStencilL);
		if (SUCCEEDED(hr))
		{
			step = "DepthStencilViewL";
			CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(this->_useMultisampling ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D);
			hr = this->_d3dDevice->CreateDepthStencilView(this->_depthStencilL, &depthStencilViewDesc, &this->_depthStencilViewL);
			if (FAILED(hr)) goto out;
		}

		hr = this->_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, &this->_depthStencilR);
		if (SUCCEEDED(hr))
		{
			step = "DepthStencilViewR";
			CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(this->_useMultisampling ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D);
			hr = this->_d3dDevice->CreateDepthStencilView(this->_depthStencilR, &depthStencilViewDesc, &this->_depthStencilViewR);
			if (FAILED(hr)) goto out;
		}
	}

	/* viewport */
	if (SUCCEEDED(hr))
	{
		step = "Viewport";
		this->_d3dDeviceContext->OMSetRenderTargets(1, this->_renderTargetView.GetAddressOf(), this->_depthStencilViewL.Get());

		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)this->_backbufferWidth;
		viewport.Height = (float)this->_backbufferHeight;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;

		this->InitViewport(&viewport);
	}

	if (SUCCEEDED(hr))
	{
		step = "Texture2D displayWidth x displayHeight";

		this->_mainDisplayTextureBpp = (this->_displayBpp == 2 && this->_use16BppMainDisplayTexture) ? 2 : 4;

		D3D11_TEXTURE2D_DESC textureDesc;
		textureDesc.Width = this->_displayWidth;
		textureDesc.Height = this->_displayHeight;
		textureDesc.Format = this->_mainDisplayTextureBpp == 2 ? DXGI_FORMAT_B5G6R5_UNORM : BACKBUFFER_FORMAT;
		textureDesc.Usage = D3D11_USAGE_DYNAMIC;
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags = 0;
		textureDesc.MipLevels = 1;
		textureDesc.ArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		hr = this->_d3dDevice->CreateTexture2D(&textureDesc, nullptr, &this->_mainDisplayTexture);

		if (SUCCEEDED(hr))
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC textureViewDesc{};
			textureViewDesc.Format = textureDesc.Format;
			textureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			textureViewDesc.Texture2D.MipLevels = 1;
			textureViewDesc.Texture2D.MostDetailedMip = 0;

			hr = this->_d3dDevice->CreateShaderResourceView(this->_mainDisplayTexture, &textureViewDesc, &this->_mainDisplayTextureView);
		}
	}

	this->_displayTempWidth = 0;
	this->_displayTempHeight = 0;
	this->_displayTempBpp = 0;

out:
	if (FAILED(hr))
	{
		static bool messageShown = false;

		if (!messageShown)
		{
			char text[512];
			close_error_file();
			strcpy_s(text, step);
			strcat_s(text, "\n");
			strcat_s(text, _com_error(hr).ErrorMessage());

			MessageBox(nullptr, text, __FUNCTION__, MB_ICONERROR);
		}

		messageShown = true;
	}

	return hr;
}

HRESULT DeviceResources::LoadMainResources()
{
	HRESULT hr = S_OK;

	if (FAILED(hr = this->_d3dDevice->CreateVertexShader(g_MainVertexShader, sizeof(g_MainVertexShader), nullptr, &_mainVertexShader)))
		return hr;

	const D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (FAILED(hr = this->_d3dDevice->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc), g_MainVertexShader, sizeof(g_MainVertexShader), &_mainInputLayout)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_MainPixelShader, sizeof(g_MainPixelShader), nullptr, &_mainPixelShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BasicPixelShader, sizeof(g_BasicPixelShader), nullptr, &_basicPixelShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BarrelPixelShader, sizeof(g_BarrelPixelShader), nullptr, &_barrelPixelShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_DeathStarShader, sizeof(g_DeathStarShader), nullptr, &_deathStarPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperEntry, sizeof(g_HyperEntry), nullptr, &_hyperEntryPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperExit, sizeof(g_HyperExit), nullptr, &_hyperExitPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperTunnel, sizeof(g_HyperTunnel), nullptr, &_hyperTunnelPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperCompose, sizeof(g_HyperCompose), nullptr, &_hyperComposePS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperZoom, sizeof(g_HyperZoom), nullptr, &_hyperZoomPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_LaserPointerVR, sizeof(g_LaserPointerVR), nullptr, &_laserPointerPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_FXAA, sizeof(g_FXAA), nullptr, &_fxaaPS)))
		return hr;

	if (g_bBloomEnabled) {
		//if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomPrePassPS, sizeof(g_BloomPrePassPS), 	nullptr, &_bloomPrepassPS)))
		//	return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomHGaussPS, sizeof(g_BloomHGaussPS), nullptr, &_bloomHGaussPS)))
			return hr;
		
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomVGaussPS, sizeof(g_BloomVGaussPS), nullptr, &_bloomVGaussPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomCombinePS, sizeof(g_BloomCombinePS), nullptr, &_bloomCombinePS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomBufferAddPS, sizeof(g_BloomBufferAddPS), nullptr, &_bloomBufferAddPS)))
			return hr;
	}

	if (g_bAOEnabled) {
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_ComputeNormalsShader, sizeof(g_ComputeNormalsShader), nullptr, &_computeNormalsPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOPixelShader, sizeof(g_SSAOPixelShader), nullptr, &_ssaoPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOBlurPixelShader, sizeof(g_SSAOBlurPixelShader), nullptr, &_ssaoBlurPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOAddPixelShader, sizeof(g_SSAOAddPixelShader), nullptr, &_ssaoAddPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirect, sizeof(g_SSDODirect), nullptr, &_ssdoDirectPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirectBentNormals, sizeof(g_SSDODirectBentNormals), nullptr, &_ssdoDirectBentNormalsPS)))
			return hr;
		
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirectHDR, sizeof(g_SSDODirectHDR), nullptr, &_ssdoDirectHDRPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOIndirect, sizeof(g_SSDOIndirect), nullptr, &_ssdoIndirectPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAdd, sizeof(g_SSDOAdd), nullptr, &_ssdoAddPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAddHDR, sizeof(g_SSDOAddHDR), nullptr, &_ssdoAddHDRPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAddBentNormals, sizeof(g_SSDOAddBentNormals), nullptr, &_ssdoAddBentNormalsPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOBlur, sizeof(g_SSDOBlur), nullptr, &_ssdoBlurPS)))
			return hr;
	}

	if (this->_d3dFeatureLevel >= D3D_FEATURE_LEVEL_10_0)
	{
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_MainPixelShaderBpp2ColorKey20, sizeof(g_MainPixelShaderBpp2ColorKey20), nullptr, &_mainPixelShaderBpp2ColorKey20)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_MainPixelShaderBpp2ColorKey00, sizeof(g_MainPixelShaderBpp2ColorKey00), nullptr, &_mainPixelShaderBpp2ColorKey00)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_MainPixelShaderBpp4ColorKey20, sizeof(g_MainPixelShaderBpp4ColorKey20), nullptr, &_mainPixelShaderBpp4ColorKey20)))
			return hr;
	}

	D3D11_RASTERIZER_DESC rsDesc;
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FrontCounterClockwise = FALSE;
	rsDesc.DepthBias = 0;
	rsDesc.DepthBiasClamp = 0.0f;
	rsDesc.SlopeScaledDepthBias = 0.0f;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.ScissorEnable = FALSE;
	rsDesc.MultisampleEnable = TRUE;
	rsDesc.AntialiasedLineEnable = FALSE;

	if (FAILED(hr = this->_d3dDevice->CreateRasterizerState(&rsDesc, &this->_mainRasterizerState)))
		return hr;

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = this->_useAnisotropy ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxAnisotropy = this->_useAnisotropy ? this->GetMaxAnisotropy() : 1;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = FLT_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.BorderColor[0] = 0.0f;
	samplerDesc.BorderColor[1] = 0.0f;
	samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 0.0f;

	if (FAILED(hr = this->_d3dDevice->CreateSamplerState(&samplerDesc, &this->_mainSamplerState)))
		return hr;

	D3D11_BLEND_DESC blendDesc;
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

	if (FAILED(hr = this->_d3dDevice->CreateBlendState(&blendDesc, &this->_mainBlendState)))
		return hr;

	D3D11_DEPTH_STENCIL_DESC depthDesc;
	depthDesc.DepthEnable = FALSE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthDesc.StencilEnable = FALSE;

	if (FAILED(hr = this->_d3dDevice->CreateDepthStencilState(&depthDesc, &this->_mainDepthState)))
		return hr;

	MainVertex vertices[4] =
	{
		MainVertex(-1, -1, 0, 1),
		MainVertex( 1, -1, 1, 1),
		MainVertex( 1,  1, 1, 0),
		MainVertex(-1,  1, 0, 0),
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

	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &this->_mainVertexBuffer)))
		return hr;
	this->_steamVRPresentBuffer = NULL;

	WORD indices[6] =
	{
		0, 1, 2,
		0, 2, 3,
	};

	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA indexBufferData;
	indexBufferData.pSysMem = indices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;

	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &this->_mainIndexBuffer)))
		return hr;

	return hr;
}

HRESULT DeviceResources::LoadResources()
{
	HRESULT hr = S_OK;

	if (FAILED(hr = this->_d3dDevice->CreateVertexShader(g_VertexShader, sizeof(g_VertexShader), nullptr, &_vertexShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreateVertexShader(g_SBSVertexShader, sizeof(g_SBSVertexShader), nullptr, &_sbsVertexShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreateVertexShader(g_PassthroughVertexShader, sizeof(g_PassthroughVertexShader), nullptr, &_passthroughVertexShader)))
		return hr;

	const D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (FAILED(hr = this->_d3dDevice->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc), g_VertexShader, sizeof(g_VertexShader), &_inputLayout)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc), g_SBSVertexShader, sizeof(g_SBSVertexShader), &_inputLayout)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderTexture, sizeof(g_PixelShaderTexture), nullptr, &_pixelShaderTexture)))
		return hr;

	if (g_bDynCockpitEnabled) {
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderDC, sizeof(g_PixelShaderDC), nullptr, &_pixelShaderDC)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderEmptyDC, sizeof(g_PixelShaderEmptyDC), nullptr, &_pixelShaderEmptyDC)))
			return hr;
	}

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderHUD, sizeof(g_PixelShaderHUD), nullptr, &_pixelShaderHUD)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderSolid, sizeof(g_PixelShaderSolid), nullptr, &_pixelShaderSolid)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_PixelShaderClearBox, sizeof(g_PixelShaderClearBox), nullptr, &_pixelShaderClearBox)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BarrelPixelShader, sizeof(g_BarrelPixelShader), nullptr, &_barrelPixelShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SingleBarrelPixelShader, sizeof(g_SingleBarrelPixelShader), nullptr, &_singleBarrelPixelShader)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_DeathStarShader, sizeof(g_DeathStarShader), nullptr, &_deathStarPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperEntry, sizeof(g_HyperEntry), nullptr, &_hyperEntryPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperExit, sizeof(g_HyperExit), nullptr, &_hyperExitPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperTunnel, sizeof(g_HyperTunnel), nullptr, &_hyperTunnelPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperCompose, sizeof(g_HyperCompose), nullptr, &_hyperComposePS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_HyperZoom, sizeof(g_HyperZoom), nullptr, &_hyperZoomPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_LaserPointerVR, sizeof(g_LaserPointerVR), nullptr, &_laserPointerPS)))
		return hr;

	if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_FXAA, sizeof(g_FXAA), nullptr, &_fxaaPS)))
		return hr;

	if (g_bBloomEnabled) {
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomHGaussPS, sizeof(g_BloomHGaussPS), nullptr, &_bloomHGaussPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomVGaussPS, sizeof(g_BloomVGaussPS), nullptr, &_bloomVGaussPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomCombinePS, sizeof(g_BloomCombinePS), nullptr, &_bloomCombinePS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_BloomBufferAddPS, sizeof(g_BloomBufferAddPS), nullptr, &_bloomBufferAddPS)))
			return hr;
	}

	if (g_bAOEnabled) {
		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_ComputeNormalsShader, sizeof(g_ComputeNormalsShader), nullptr, &_computeNormalsPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOPixelShader, sizeof(g_SSAOPixelShader), nullptr, &_ssaoPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOBlurPixelShader, sizeof(g_SSAOBlurPixelShader), nullptr, &_ssaoBlurPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSAOAddPixelShader, sizeof(g_SSAOAddPixelShader), nullptr, &_ssaoAddPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirect, sizeof(g_SSDODirect), nullptr, &_ssdoDirectPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirectBentNormals, sizeof(g_SSDODirectBentNormals), nullptr, &_ssdoDirectBentNormalsPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDODirectHDR, sizeof(g_SSDODirectHDR), nullptr, &_ssdoDirectHDRPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOIndirect, sizeof(g_SSDOIndirect), nullptr, &_ssdoIndirectPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAdd, sizeof(g_SSDOAdd), nullptr, &_ssdoAddPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAddHDR, sizeof(g_SSDOAddHDR), nullptr, &_ssdoAddHDRPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOAddBentNormals, sizeof(g_SSDOAddBentNormals), nullptr, &_ssdoAddBentNormalsPS)))
			return hr;

		if (FAILED(hr = this->_d3dDevice->CreatePixelShader(g_SSDOBlur, sizeof(g_SSDOBlur), nullptr, &_ssdoBlurPS)))
			return hr;
	}

	D3D11_RASTERIZER_DESC rsDesc;
	rsDesc.FillMode = g_config.WireframeFillMode ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FrontCounterClockwise = TRUE;
	rsDesc.DepthBias = 0;
	rsDesc.DepthBiasClamp = 0.0f;
	rsDesc.SlopeScaledDepthBias = 0.0f;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.ScissorEnable = FALSE;
	rsDesc.MultisampleEnable = TRUE;
	rsDesc.AntialiasedLineEnable = FALSE;

	if (FAILED(hr = this->_d3dDevice->CreateRasterizerState(&rsDesc, &this->_rasterizerState)))
		return hr;

	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = 0;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.StructureByteStride = 0;

	constantBufferDesc.ByteWidth = 48;
	// This was the original constant buffer. Now it's called _VSConstantBuffer
	static_assert(sizeof(VertexShaderCBuffer) == 48, "sizeof(VertexShaderCBuffer) must be 48");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_VSConstantBuffer)))
		return hr;

	constantBufferDesc.ByteWidth = 192; // 4x4 elems in a matrix = 16 elems. Each elem is a float, so 4 bytes * 16 = 64 bytes per matrix. This is a multiple of 16
	// 192 bytes is 3 matrices
	static_assert(sizeof(VertexShaderMatrixCB) == 192, "sizeof(VertexShaderMatrixCB) must be 192");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_VSMatrixBuffer)))
		return hr;

	// Create the constant buffer for the (3D) textured pixel shader
	constantBufferDesc.ByteWidth = 80;
	static_assert(sizeof(PixelShaderCBuffer) == 80, "sizeof(PixelShaderCBuffer) must be 80");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_PSConstantBuffer)))
		return hr;

	// Create the constant buffer for the (3D) textured pixel shader -- Dynamic Cockpit data
	constantBufferDesc.ByteWidth = 448;
	static_assert(sizeof(DCPixelShaderCBuffer) == 448, "sizeof(DCPixelShaderCBuffer) must be 448");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_PSConstantBufferDC)))
		return hr;

	// Create the constant buffer for the barrel pixel shader
	constantBufferDesc.ByteWidth = 16;
	static_assert(sizeof(BarrelPixelShaderCBuffer) == 16, "sizeof(BarrelPixelShaderCBuffer) must be 16");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_barrelConstantBuffer)))
		return hr;

	// Create the constant buffer for the bloom pixel shader
	constantBufferDesc.ByteWidth = 48;
	static_assert(sizeof(BloomPixelShaderCBuffer) == 48, "sizeof(BloomPixelShaderCBuffer) must be 48");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_bloomConstantBuffer)))
		return hr;

	// Create the Hyperspace (ShaderToy) constant buffer
	constantBufferDesc.ByteWidth = 128;
	static_assert(sizeof(ShadertoyCBuffer) == 128, "sizeof(ShadertoyCBuffer) must be 128");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_hyperspaceConstantBuffer)))
		return hr;

	// Laser Pointer (Active Cockpit) constant buffer
	constantBufferDesc.ByteWidth = 112;
	static_assert(sizeof(LaserPointerCBuffer) == 112, "sizeof(LaserPointerCBuffer) must be 112");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_laserPointerConstantBuffer)))
		return hr;

	// Create the constant buffer for the SSAO pixel shader
	constantBufferDesc.ByteWidth = 176;
	static_assert(sizeof(SSAOPixelShaderCBuffer) == 176, "sizeof(SSAOPixelShaderCBuffer) must be 176");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_ssaoConstantBuffer)))
		return hr;

	constantBufferDesc.ByteWidth = 112;
	static_assert(sizeof(PSShadingSystemCB) == 112, "sizeof(PSShadingSystemCB) must be 112");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_shadingSysBuffer)))
		return hr;

	// Create the constant buffer for the main pixel shader
	constantBufferDesc.ByteWidth = 32;
	static_assert(sizeof(MainShadersCBStruct) == 32, "sizeof(MainShadersCBStruct) must be 32");
	if (FAILED(hr = this->_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &this->_mainShadersConstantBuffer)))
		return hr;

	log_debug("[DBG] [MAT] Initializing OPTnames and Materials");
	InitOPTnames();
	InitCraftMaterials();
	// Apply the external FOV parameters
	// LoadFocalLength(); // This doesn't work: looks like XWA overwrites these params after this call.
	return hr;
}

void DeviceResources::InitInputLayout(ID3D11InputLayout* inputLayout)
{
	static ID3D11InputLayout* currentInputLayout = nullptr;

	if (inputLayout != currentInputLayout)
	{
		currentInputLayout = inputLayout;
		this->_d3dDeviceContext->IASetInputLayout(inputLayout);
	}
}

void DeviceResources::InitVertexShader(ID3D11VertexShader* vertexShader)
{
	static ID3D11VertexShader* currentVertexShader = nullptr;

	if (vertexShader != currentVertexShader)
	{
		currentVertexShader = vertexShader;
		this->_d3dDeviceContext->VSSetShader(vertexShader, nullptr, 0);
	}
}

void DeviceResources::InitPixelShader(ID3D11PixelShader* pixelShader)
{
	static ID3D11PixelShader* currentPixelShader = nullptr;

	if (pixelShader != currentPixelShader)
	{
		currentPixelShader = pixelShader;
		this->_d3dDeviceContext->PSSetShader(pixelShader, nullptr, 0);
	}
}

void DeviceResources::InitTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	D3D_PRIMITIVE_TOPOLOGY currentTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	if (topology != currentTopology)
	{
		currentTopology = topology;
		this->_d3dDeviceContext->IASetPrimitiveTopology(topology);
	}
}

void DeviceResources::InitRasterizerState(ID3D11RasterizerState* state)
{
	static ID3D11RasterizerState* currentState = nullptr;

	if (state != currentState)
	{
		currentState = state;
		this->_d3dDeviceContext->RSSetState(state);
	}
}

HRESULT DeviceResources::InitSamplerState(ID3D11SamplerState** sampler, D3D11_SAMPLER_DESC* desc)
{
	static ID3D11SamplerState** currentSampler = nullptr;
	static D3D11_SAMPLER_DESC currentDesc{};

	if (sampler == nullptr)
	{
		if (memcmp(desc, &currentDesc, sizeof(D3D11_SAMPLER_DESC)) != 0)
		{
			HRESULT hr;
			ComPtr<ID3D11SamplerState> tempSampler;
			if (FAILED(hr = this->_d3dDevice->CreateSamplerState(desc, &tempSampler)))
				return hr;

			currentDesc = *desc;
			currentSampler = tempSampler.GetAddressOf();
			this->_d3dDeviceContext->PSSetSamplers(0, 1, currentSampler);
		}
	}
	else
	{
		if (sampler != currentSampler)
		{
			currentDesc = {};
			currentSampler = sampler;
			this->_d3dDeviceContext->PSSetSamplers(0, 1, currentSampler);
		}
	}

	return S_OK;
}

HRESULT DeviceResources::InitBlendState(ID3D11BlendState* blend, D3D11_BLEND_DESC* desc)
{
	static ID3D11BlendState* currentBlend = nullptr;
	static D3D11_BLEND_DESC currentDesc{};

	if (blend == nullptr)
	{
		if (memcmp(desc, &currentDesc, sizeof(D3D11_BLEND_DESC)) != 0)
		{
			HRESULT hr;
			ComPtr<ID3D11BlendState> tempBlend;
			if (FAILED(hr = this->_d3dDevice->CreateBlendState(desc, &tempBlend)))
				return hr;

			currentDesc = *desc;
			currentBlend = tempBlend;

			static const FLOAT factors[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			UINT mask = 0xffffffff;
			this->_d3dDeviceContext->OMSetBlendState(currentBlend, factors, mask);
		}
	}
	else
	{
		if (blend != currentBlend)
		{
			currentDesc = {};
			currentBlend = blend;

			static const FLOAT factors[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			UINT mask = 0xffffffff;
			this->_d3dDeviceContext->OMSetBlendState(currentBlend, factors, mask);
		}
	}

	return S_OK;
}

HRESULT DeviceResources::InitDepthStencilState(ID3D11DepthStencilState* depthState, D3D11_DEPTH_STENCIL_DESC* desc)
{
	static ID3D11DepthStencilState* currentDepthState = nullptr;
	static D3D11_DEPTH_STENCIL_DESC currentDesc{};

	if (depthState == nullptr)
	{
		if (memcmp(desc, &currentDesc, sizeof(D3D11_DEPTH_STENCIL_DESC)) != 0)
		{
			HRESULT hr;
			ComPtr<ID3D11DepthStencilState> tempDepthState;
			if (FAILED(hr = this->_d3dDevice->CreateDepthStencilState(desc, &tempDepthState)))
				return hr;

			currentDesc = *desc;
			currentDepthState = tempDepthState;
			this->_d3dDeviceContext->OMSetDepthStencilState(currentDepthState, 0);
		}
	}
	else
	{
		if (depthState != currentDepthState)
		{
			currentDesc = {};
			currentDepthState = depthState;
			this->_d3dDeviceContext->OMSetDepthStencilState(currentDepthState, 0);
		}
	}

	return S_OK;
}

void DeviceResources::InitVertexBuffer(ID3D11Buffer** buffer, UINT* stride, UINT* offset)
{
	static ID3D11Buffer** currentBuffer = nullptr;

	if (buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->IASetVertexBuffers(0, 1, buffer, stride, offset);
	}
}

void DeviceResources::InitIndexBuffer(ID3D11Buffer* buffer)
{
	static ID3D11Buffer* currentBuffer = nullptr;

	if (buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->IASetIndexBuffer(buffer, DXGI_FORMAT_R16_UINT, 0);
	}
}

void DeviceResources::InitViewport(D3D11_VIEWPORT* viewport)
{
	static D3D11_VIEWPORT currentViewport{};

	if (memcmp(viewport, &currentViewport, sizeof(D3D11_VIEWPORT)) != 0)
	{
		currentViewport = *viewport;
		this->_d3dDeviceContext->RSSetViewports(1, viewport);
	}
}

void DeviceResources::InitVSConstantBuffer3D(ID3D11Buffer** buffer, const VertexShaderCBuffer* vsCBuffer)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	static VertexShaderCBuffer currentVSConstants = { 0 };
	static int sizeof_constants = sizeof(VertexShaderCBuffer);

	if (g_LastVSConstantBufferSet == VS_CONSTANT_BUFFER_NONE ||
		g_LastVSConstantBufferSet != VS_CONSTANT_BUFFER_3D ||
		memcmp(vsCBuffer, &currentVSConstants, sizeof_constants) != 0)
	{
		memcpy(&currentVSConstants, vsCBuffer, sizeof_constants);
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, vsCBuffer, 0, 0);
	}

	if (g_LastVSConstantBufferSet == VS_CONSTANT_BUFFER_NONE ||
		g_LastVSConstantBufferSet != VS_CONSTANT_BUFFER_3D ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->VSSetConstantBuffers(0, 1, buffer);
	}
	g_LastVSConstantBufferSet = VS_CONSTANT_BUFFER_3D;
}

void DeviceResources::InitVSConstantBufferMatrix(ID3D11Buffer** buffer, const VertexShaderMatrixCB* vsCBuffer)
{
	this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, vsCBuffer, 0, 0);
	this->_d3dDeviceContext->VSSetConstantBuffers(1, 1, buffer);
}

void DeviceResources::InitPSConstantShadingSystem(ID3D11Buffer** buffer, const PSShadingSystemCB* psCBuffer)
{
	this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psCBuffer, 0, 0);
	this->_d3dDeviceContext->PSSetConstantBuffers(4, 1, buffer);
}

void DeviceResources::InitVSConstantBuffer2D(ID3D11Buffer** buffer, const float parallax,
	const float aspectRatio, const float scale, const float brightness, const float use_3D)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	if (g_LastVSConstantBufferSet == VS_CONSTANT_BUFFER_NONE ||
		g_LastVSConstantBufferSet != VS_CONSTANT_BUFFER_2D ||
		g_MSCBuffer.parallax != parallax ||
		g_MSCBuffer.aspectRatio != aspectRatio ||
		g_MSCBuffer.scale != scale ||
		g_MSCBuffer.brightness != brightness ||
		g_MSCBuffer.use_3D != use_3D)
	{
		g_MSCBuffer.parallax = parallax;
		g_MSCBuffer.aspectRatio = aspectRatio;
		g_MSCBuffer.scale = scale;
		g_MSCBuffer.brightness = brightness;
		g_MSCBuffer.use_3D = use_3D;
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, &g_MSCBuffer, 0, 0);
	}

	if (g_LastVSConstantBufferSet == VS_CONSTANT_BUFFER_NONE ||
		g_LastVSConstantBufferSet != VS_CONSTANT_BUFFER_2D ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->VSSetConstantBuffers(0, 1, buffer);
	}
	g_LastVSConstantBufferSet = VS_CONSTANT_BUFFER_2D;
}

void DeviceResources::InitPSConstantBuffer2D(ID3D11Buffer** buffer, const float parallax,
	const float aspectRatio, const float scale, const float brightness)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_2D ||
		g_MSCBuffer.parallax != parallax ||
		g_MSCBuffer.aspectRatio != aspectRatio ||
		g_MSCBuffer.scale != scale ||
		g_MSCBuffer.brightness != brightness)
	{
		g_MSCBuffer.parallax = parallax;
		g_MSCBuffer.aspectRatio = aspectRatio;
		g_MSCBuffer.scale = scale;
		g_MSCBuffer.brightness = brightness;
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, &g_MSCBuffer, 0, 0);
	}

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_2D ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->PSSetConstantBuffers(0, 1, buffer);
	}
	g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_2D;
}

void DeviceResources::InitPSConstantBufferBarrel(ID3D11Buffer** buffer, const float k1, const float k2, const float k3)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_BARREL ||
		g_BarrelPSCBuffer.k1 != k1 ||
		g_BarrelPSCBuffer.k2 != k2 ||
		g_BarrelPSCBuffer.k3 != k3)
	{
		g_BarrelPSCBuffer.k1 = k1;
		g_BarrelPSCBuffer.k2 = k2;
		g_BarrelPSCBuffer.k3 = k3;
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, &g_BarrelPSCBuffer, 0, 0);
	}

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_BARREL ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->PSSetConstantBuffers(0, 1, buffer);
	}
	g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_BARREL;
}

void DeviceResources::InitPSConstantBufferBloom(ID3D11Buffer** buffer, const BloomPixelShaderCBuffer *psConstants)
{
	static BloomPixelShaderCBuffer currentPSConstants = { 0 };
	static int sizeof_constants = sizeof(BloomPixelShaderCBuffer);

	static ID3D11Buffer** currentBuffer = nullptr;
	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_BLOOM ||
		memcmp(psConstants, &currentPSConstants, sizeof_constants) != 0)
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_BLOOM ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->PSSetConstantBuffers(2, 1, buffer);
	}
	g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_BLOOM;
}

void DeviceResources::InitPSConstantBufferSSAO(ID3D11Buffer** buffer, const SSAOPixelShaderCBuffer *psConstants)
{
	static SSAOPixelShaderCBuffer currentPSConstants = { 0 };
	static int sizeof_constants = sizeof(SSAOPixelShaderCBuffer);

	static ID3D11Buffer** currentBuffer = nullptr;
	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_SSAO ||
		memcmp(psConstants, &currentPSConstants, sizeof_constants) != 0)
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_SSAO ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->PSSetConstantBuffers(3, 1, buffer);
	}
	g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_SSAO;
}

void DeviceResources::InitPSConstantBufferHyperspace(ID3D11Buffer ** buffer, const ShadertoyCBuffer * psConstants)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	static ShadertoyCBuffer currentPSConstants = { 0 };
	static int sizeof_constants = sizeof(ShadertoyCBuffer);

	this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);
	this->_d3dDeviceContext->PSSetConstantBuffers(7, 1, buffer);
}

void DeviceResources::InitPSConstantBufferLaserPointer(ID3D11Buffer ** buffer, const LaserPointerCBuffer * psConstants)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	static LaserPointerCBuffer currentPSConstants = { 0 };
	static int sizeof_constants = sizeof(ShadertoyCBuffer);

	this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);
	this->_d3dDeviceContext->PSSetConstantBuffers(7, 1, buffer);
}

void DeviceResources::InitPSConstantBuffer3D(ID3D11Buffer** buffer, const PixelShaderCBuffer* psConstants)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	static PixelShaderCBuffer currentPSConstants = {0};
	static int sizeof_constants = sizeof(PixelShaderCBuffer);

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_3D ||
		memcmp(psConstants, &currentPSConstants, sizeof_constants) != 0)
	{
		memcpy(&currentPSConstants, psConstants, sizeof_constants);
		this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);
	}

	if (g_LastPSConstantBufferSet == PS_CONSTANT_BUFFER_NONE ||
		g_LastPSConstantBufferSet != PS_CONSTANT_BUFFER_3D ||
		buffer != currentBuffer)
	{
		currentBuffer = buffer;
		this->_d3dDeviceContext->PSSetConstantBuffers(0, 1, buffer);
	}
	g_LastPSConstantBufferSet = PS_CONSTANT_BUFFER_3D;
}

void DeviceResources::InitPSConstantBufferDC(ID3D11Buffer** buffer, const DCPixelShaderCBuffer* psConstants)
{
	static ID3D11Buffer** currentBuffer = nullptr;
	static DCPixelShaderCBuffer currentPSConstants = { 0 };
	static int sizeof_constants = sizeof(DCPixelShaderCBuffer);

	this->_d3dDeviceContext->UpdateSubresource(buffer[0], 0, nullptr, psConstants, 0, 0);
	this->_d3dDeviceContext->PSSetConstantBuffers(1, 1, buffer);
}

HRESULT DeviceResources::RenderMain(char* src, DWORD width, DWORD height, DWORD bpp, RenderMainColorKeyType useColorKey)
{
	HRESULT hr = S_OK;
	char* step = "";

	D3D11_MAPPED_SUBRESOURCE displayMap;
	DWORD pitchDelta;

	ID3D11Texture2D* tex = nullptr;
	ID3D11ShaderResourceView** texView = nullptr;

	/*
	if (g_bUseSteamVR) {
		// Process SteamVR events
		vr::VREvent_t event;
		while (g_pHMD->PollNextEvent(&event, sizeof(event)))
		{
			ProcessVREvent(event);
		}
	}
	*/

	if (SUCCEEDED(hr))
	{
		if ((width == this->_displayWidth) && (height == this->_displayHeight) && (bpp == this->_mainDisplayTextureBpp))
		{
			step = "DisplayTexture displayWidth x displayHeight";
			hr = this->_d3dDeviceContext->Map(this->_mainDisplayTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &displayMap);

			if (SUCCEEDED(hr))
			{
				pitchDelta = displayMap.RowPitch - width * bpp;
				tex = this->_mainDisplayTexture;
				texView = this->_mainDisplayTextureView.GetAddressOf();
			}
		}
		else
		{
			step = "DisplayTexture temp";

			if (width != this->_displayTempWidth || height != this->_displayTempHeight || bpp != this->_displayTempBpp)
			{
				D3D11_TEXTURE2D_DESC textureDesc;
				textureDesc.Width = width;
				textureDesc.Height = height;
				textureDesc.Format = (bpp == 2 && this->_use16BppMainDisplayTexture) ? DXGI_FORMAT_B5G6R5_UNORM : BACKBUFFER_FORMAT;
				textureDesc.Usage = D3D11_USAGE_DYNAMIC;
				textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				textureDesc.MiscFlags = 0;
				textureDesc.MipLevels = 1;
				textureDesc.ArraySize = 1;
				textureDesc.SampleDesc.Count = 1;
				textureDesc.SampleDesc.Quality = 0;
				textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

				hr = this->_d3dDevice->CreateTexture2D(&textureDesc, nullptr, &this->_mainDisplayTextureTemp);

				if (SUCCEEDED(hr))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC textureViewDesc{};
					textureViewDesc.Format = textureDesc.Format;
					textureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					textureViewDesc.Texture2D.MipLevels = 1;
					textureViewDesc.Texture2D.MostDetailedMip = 0;

					hr = this->_d3dDevice->CreateShaderResourceView(this->_mainDisplayTextureTemp, &textureViewDesc, &this->_mainDisplayTextureViewTemp);
				}

				if (SUCCEEDED(hr))
				{
					this->_displayTempWidth = width;
					this->_displayTempHeight = height;
					this->_displayTempBpp = bpp;
				}
			}

			if (SUCCEEDED(hr))
			{
				hr = this->_d3dDeviceContext->Map(this->_mainDisplayTextureTemp, 0, D3D11_MAP_WRITE_DISCARD, 0, &displayMap);

				if (SUCCEEDED(hr))
				{
					pitchDelta = displayMap.RowPitch - width * ((bpp == 2 && this->_use16BppMainDisplayTexture) ? 2 : 4);
					tex = this->_mainDisplayTextureTemp;
					texView = this->_mainDisplayTextureViewTemp.GetAddressOf();
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		if (bpp == 2)
		{
			if (this->_use16BppMainDisplayTexture)
			{
				if (pitchDelta == 0)
				{
					memcpy(displayMap.pData, src, width * height * 2);
				}
				else
				{
					unsigned short* srcColors = (unsigned short*)src;
					unsigned short* colors = (unsigned short*)displayMap.pData;

					for (DWORD y = 0; y < height; y++)
					{
						memcpy(colors, srcColors, width * 2);
						srcColors += width;
						colors = (unsigned short*)((char*)(colors + width) + pitchDelta);
					}
				}
			}
			else
			{
				if (useColorKey == RENDERMAIN_COLORKEY_20)
				{
					unsigned short* srcColors = (unsigned short*)src;
					unsigned int* colors = (unsigned int*)displayMap.pData;

					for (DWORD y = 0; y < height; y++)
					{
						for (DWORD x = 0; x < width; x++)
						{
							unsigned short color16 = srcColors[x];

							if (color16 == 0x2000)
							{
								colors[x] = 0xff000000;
							}
							else
							{
								colors[x] = convertColorB5G6R5toB8G8R8X8(color16);
							}
						}

						srcColors += width;
						colors = (unsigned int*)((char*)(colors + width) + pitchDelta);
					}
				}
				else if (useColorKey == RENDERMAIN_COLORKEY_00)
				{
					unsigned short* srcColors = (unsigned short*)src;
					unsigned int* colors = (unsigned int*)displayMap.pData;

					for (DWORD y = 0; y < height; y++)
					{
						for (DWORD x = 0; x < width; x++)
						{
							unsigned short color16 = srcColors[x];

							if (color16 == 0)
							{
								colors[x] = 0xff000000;
							}
							else
							{
								colors[x] = convertColorB5G6R5toB8G8R8X8(color16);
							}
						}

						srcColors += width;
						colors = (unsigned int*)((char*)(colors + width) + pitchDelta);
					}
				}
				else
				{
					unsigned short* srcColors = (unsigned short*)src;
					unsigned int* colors = (unsigned int*)displayMap.pData;

					for (DWORD y = 0; y < height; y++)
					{
						for (DWORD x = 0; x < width; x++)
						{
							unsigned short color16 = srcColors[x];

							colors[x] = convertColorB5G6R5toB8G8R8X8(color16);
						}

						srcColors += width;
						colors = (unsigned int*)((char*)(colors + width) + pitchDelta);
					}
				}
			}
		}
		else
		{
			if (useColorKey && (this->_d3dFeatureLevel < D3D_FEATURE_LEVEL_10_0))
			{
				unsigned int* srcColors = (unsigned int*)src;
				unsigned int* colors = (unsigned int*)displayMap.pData;

				__m128i key = _mm_set1_epi32(0x200000);
				__m128i colorMask = _mm_set1_epi32(0xffffff);

				for (DWORD y = 0; y < height; y++)
				{
					DWORD x = 0;
					for (; x < (width & ~3); x += 4)
					{
						__m128i color = _mm_load_si128((const __m128i*)(srcColors + x));
						__m128i transparent = _mm_cmpeq_epi32(color, key);

						color = _mm_andnot_si128(transparent, color);
						color = _mm_and_si128(color, colorMask);

						transparent = _mm_slli_epi32(transparent, 24);

						color = _mm_or_si128(color, transparent);
						_mm_store_si128((__m128i*)(colors + x), color);
					}

					for (; x < width; x++)
					{
						unsigned int color32 = srcColors[x];

						if (color32 == 0x200000)
						{
							colors[x] = 0xff000000;
						}
						else
						{
							colors[x] = color32 & 0xffffff;
						}
					}

					srcColors += width;
					colors = (unsigned int*)((char*)(colors + width) + pitchDelta);
				}
			}
			else
			{
				if (pitchDelta == 0)
				{
					memcpy(displayMap.pData, src, width * height * 4);
				}
				else
				{
					unsigned int* srcColors = (unsigned int*)src;
					unsigned int* colors = (unsigned int*)displayMap.pData;

					for (DWORD y = 0; y < height; y++)
					{
						memcpy(colors, srcColors, width * 4);
						srcColors += width;
						colors = (unsigned int*)((char*)(colors + width) + pitchDelta);
					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		this->_d3dDeviceContext->Unmap(tex, 0);
	}

	this->InitInputLayout(this->_mainInputLayout);
	this->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	this->InitVertexShader(this->_mainVertexShader);

	if (bpp == 2)
	{
		if (useColorKey && this->_use16BppMainDisplayTexture)
		{
			switch (useColorKey)
			{
			case RENDERMAIN_COLORKEY_20:
				this->InitPixelShader(this->_mainPixelShaderBpp2ColorKey20);
				break;

			case RENDERMAIN_COLORKEY_00:
				this->InitPixelShader(this->_mainPixelShaderBpp2ColorKey00);
				break;

			default:
				this->InitPixelShader(this->_mainPixelShader);
				break;
			}
		}
		else
		{
			this->InitPixelShader(this->_mainPixelShader);
		}
	}
	else
	{
		if (useColorKey && this->_d3dFeatureLevel >= D3D_FEATURE_LEVEL_10_0)
		{
			this->InitPixelShader(this->_mainPixelShaderBpp4ColorKey20);
		}
		else
		{
			this->InitPixelShader(this->_mainPixelShader);
		}
	}

	UINT w;
	UINT h;

	if (g_config.AspectRatioPreserved)
	{
		if (this->_backbufferHeight * width <= this->_backbufferWidth * height)
		{
			w = this->_backbufferHeight * width / height;
			h = this->_backbufferHeight;
		}
		else
		{
			w = this->_backbufferWidth;
			h = this->_backbufferWidth * height / width;
		}
		
		if (!g_bOverrideAspectRatio) { // Only compute the aspect ratio if we didn't read it off of the config file
			g_fConcourseAspectRatio = 2.0f * w / this->_backbufferWidth;
			if (g_fConcourseAspectRatio < 2.0f)
				g_fConcourseAspectRatio = 2.0f;
		}
	}
	else
	{
		w = this->_backbufferWidth;
		h = this->_backbufferHeight;
		
		if (!g_bOverrideAspectRatio) {
			float original_aspect = (float)this->_displayWidth / (float)this->_displayHeight;
			float actual_aspect = (float)this->_backbufferWidth / (float)this->_backbufferHeight;
			g_fConcourseAspectRatio = 2.0f; // * (1.0f + (actual_aspect - original_aspect));
			//g_fConcourseAspectRatio = 2.0f * actual_aspect / original_aspect;
		}
	}
	//if (g_bUseSteamVR) g_fConcourseAspectRatio = 1.0f;

	UINT left = (this->_backbufferWidth - w) / 2;
	UINT top = (this->_backbufferHeight - h) / 2;

	if (g_bEnableVR) { // SteamVR and DirectSBS modes
		InitVSConstantBuffer2D(this->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, g_fConcourseAspectRatio, g_fConcourseScale, g_fBrightness, 1.0f); // Use 3D projection matrices
		InitPSConstantBuffer2D(this->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, g_fConcourseAspectRatio, g_fConcourseScale, g_fBrightness);
	} 
	else {
		InitVSConstantBuffer2D(this->_mainShadersConstantBuffer.GetAddressOf(), 0, 1, 1, g_fBrightness, 0.0f); // Don't 3D projection matrices when VR is disabled
		InitPSConstantBuffer2D(this->_mainShadersConstantBuffer.GetAddressOf(), 0, 1, 1, g_fBrightness);
	}

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = (float)left;
	viewport.TopLeftY = (float)top;
	viewport.Width = (float)w;
	viewport.Height = (float)h;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	this->InitViewport(&viewport);

	if (SUCCEEDED(hr))
	{
		step = "States";
		this->InitRasterizerState(this->_mainRasterizerState);
		this->InitSamplerState(this->_mainSamplerState.GetAddressOf(), nullptr);
		this->InitBlendState(this->_mainBlendState, nullptr);
		this->InitDepthStencilState(this->_mainDepthState, nullptr);
	}

	if (SUCCEEDED(hr))
	{
		step = "Texture2D ShaderResourceView";

		this->_d3dDeviceContext->PSSetShaderResources(0, 1, texView);
	}

	if (SUCCEEDED(hr))
	{
		step = "Draw";
		D3D11_VIEWPORT viewport = { 0 };
		UINT stride = sizeof(MainVertex);
		UINT offset = 0;

		this->InitVertexBuffer(this->_mainVertexBuffer.GetAddressOf(), &stride, &offset);
		this->InitIndexBuffer(this->_mainIndexBuffer);

		float screen_res_x = (float)this->_backbufferWidth;
		float screen_res_y = (float)this->_backbufferHeight;

		if (!g_bEnableVR) {
			// The Concourse and 2D menu are drawn here... maybe the default starfield too?
			this->_d3dDeviceContext->DrawIndexed(6, 0, 0);
			goto out;
		}

		// Let's do SBS rendering here. That'll make it compatible with the Tech Library where
		// both 2D and 3D are mixed.
		// Left viewport
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		if (g_bUseSteamVR)
			viewport.Width = screen_res_x;
		else
			viewport.Width = screen_res_x / 2.0f;
		viewport.Height = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		this->InitViewport(&viewport);
		this->InitVSConstantBuffer2D(_mainShadersConstantBuffer.GetAddressOf(),
			g_fTechLibraryParallax * g_iDraw2DCounter, g_fConcourseAspectRatio, g_fConcourseScale, g_fBrightness, 
			1.0f); // Use 3D projection matrices

		// The Concourse and 2D menu are drawn here... maybe the default starfield too?
		// When SteamVR is not used, the RenderTargets are set in the OnSizeChanged() event above
		g_VSMatrixCB.projEye = g_fullMatrixLeft;
		InitVSConstantBufferMatrix(_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
		if (g_bUseSteamVR)
			_d3dDeviceContext->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilViewL.Get());
		else
			_d3dDeviceContext->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilViewL.Get());
		this->_d3dDeviceContext->DrawIndexed(6, 0, 0);

		// Right viewport
		if (g_bUseSteamVR) {
			viewport.TopLeftX = 0;
			viewport.Width = screen_res_x;
		} else {
			viewport.TopLeftX = screen_res_x / 2.0f;
			viewport.Width = screen_res_x / 2.0f;
		}
		viewport.TopLeftY = 0;
		viewport.Height = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		this->InitViewport(&viewport);
		this->InitVSConstantBuffer2D(this->_mainShadersConstantBuffer.GetAddressOf(),
			g_fTechLibraryParallax * g_iDraw2DCounter, g_fConcourseAspectRatio, g_fConcourseScale, g_fBrightness,
			1.0f); // Use 3D projection matrices
		// The Concourse and 2D menu are drawn here... maybe the default starfield too?
		g_VSMatrixCB.projEye = g_fullMatrixRight;
		InitVSConstantBufferMatrix(_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
		if (g_bUseSteamVR)
			_d3dDeviceContext->OMSetRenderTargets(1, _renderTargetViewR.GetAddressOf(), _depthStencilViewR.Get());
		else
			_d3dDeviceContext->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilViewL.Get());
		this->_d3dDeviceContext->DrawIndexed(6, 0, 0);
	out:
		// Increase the 2D DrawCounter -- this is used in the Tech Library to increase the parallax when the second 2D
		// layer is rendered on top of the 3D floating object.
		g_iDraw2DCounter++;
	}

	if (FAILED(hr))
	{
		static bool messageShown = false;

		if (!messageShown)
		{
			char text[512];
			strcpy_s(text, step);
			strcat_s(text, "\n");
			strcat_s(text, _com_error(hr).ErrorMessage());

			MessageBox(nullptr, text, __FUNCTION__, MB_ICONERROR);
		}

		messageShown = true;
	}

	return hr;
}

HRESULT DeviceResources::RetrieveBackBuffer(char* buffer, DWORD width, DWORD height, DWORD bpp)
{
	HRESULT hr = S_OK;
	char* step = "";

	memset(buffer, 0, width * height * bpp);

	D3D11_TEXTURE2D_DESC textureDescription;
	this->_backBuffer->GetDesc(&textureDescription);

	textureDescription.BindFlags = 0;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.SampleDesc.Quality = 0;

	ComPtr<ID3D11Texture2D> backBuffer;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.CPUAccessFlags = 0;

	step = "Resolve BackBuffer";

	if (SUCCEEDED(hr = this->_d3dDevice->CreateTexture2D(&textureDescription, nullptr, &backBuffer)))
	{
		this->_d3dDeviceContext->ResolveSubresource(backBuffer, D3D11CalcSubresource(0, 0, 1), this->_backBuffer, D3D11CalcSubresource(0, 0, 1), textureDescription.Format);

		step = "Staging Texture2D";

		ComPtr<ID3D11Texture2D> texture;
		textureDescription.Usage = D3D11_USAGE_STAGING;
		textureDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		if (SUCCEEDED(hr = this->_d3dDevice->CreateTexture2D(&textureDescription, nullptr, &texture)))
		{
			this->_d3dDeviceContext->CopyResource(texture, backBuffer);

			step = "Map";

			D3D11_MAPPED_SUBRESOURCE map;
			if (SUCCEEDED(hr = this->_d3dDeviceContext->Map(texture, 0, D3D11_MAP_READ, 0, &map)))
			{
				step = "copy";

				if (bpp == 4 && width == this->_backbufferWidth && height == this->_backbufferHeight && this->_backbufferWidth * 4 == map.RowPitch)
				{
					memcpy(buffer, map.pData, width * height * 4);
				}
				else
				{
					if (this->_backbufferWidth * 4 == map.RowPitch)
					{
						scaleSurface(buffer, width, height, bpp, (char*)map.pData, this->_backbufferWidth, this->_backbufferHeight, 4);
					}
					else
					{
						char* buffer2 = new char[this->_backbufferWidth * this->_backbufferHeight * 4];

						unsigned int* srcColors = (unsigned int*)map.pData;
						unsigned int* colors = (unsigned int*)buffer2;

						for (DWORD y = 0; y < this->_backbufferHeight; y++)
						{
							memcpy(colors, srcColors, this->_backbufferWidth * 4);

							srcColors = (unsigned int*)((char*)srcColors + map.RowPitch);
							colors += this->_backbufferWidth;
						}

						scaleSurface(buffer, width, height, bpp, buffer2, this->_backbufferWidth, this->_backbufferHeight, 4);

						delete[] buffer2;
					}
				}

				this->_d3dDeviceContext->Unmap(texture, 0);
			}
		}
	}

	if (FAILED(hr))
	{
		static bool messageShown = false;

		if (!messageShown)
		{
			char text[512];
			strcpy_s(text, step);
			strcat_s(text, "\n");
			strcat_s(text, _com_error(hr).ErrorMessage());

			MessageBox(nullptr, text, __FUNCTION__, MB_ICONERROR);
		}

		messageShown = true;
	}

	return hr;
}

UINT DeviceResources::GetMaxAnisotropy()
{
	return this->_d3dFeatureLevel >= D3D_FEATURE_LEVEL_9_2 ? D3D11_MAX_MAXANISOTROPY : D3D_FL9_1_DEFAULT_MAX_ANISOTROPY;
}

void DeviceResources::CheckMultisamplingSupport()
{
	this->_sampleDesc.Count = 1;
	this->_sampleDesc.Quality = 0;

	if (!this->_useMultisampling)
	{
		return;
	}

	UINT formatSupport;

	if (FAILED(this->_d3dDevice->CheckFormatSupport(BACKBUFFER_FORMAT, &formatSupport)))
	{
		return;
	}

	bool supported = (formatSupport & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) && (formatSupport & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET);

	if (supported)
	{
		for (UINT i = 2; i <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; i *= 2)
		{
			UINT numQualityLevels = 0;

			HRESULT hr = this->_d3dDevice->CheckMultisampleQualityLevels(BACKBUFFER_FORMAT, i, &numQualityLevels);

			if (SUCCEEDED(hr) && (numQualityLevels > 0))
			{
				this->_sampleDesc.Count = i;
				this->_sampleDesc.Quality = numQualityLevels - 1;
			}
		}
	}

	if (this->_sampleDesc.Count <= 1)
	{
		this->_useMultisampling = FALSE;
	}
}

bool DeviceResources::IsTextureFormatSupported(DXGI_FORMAT format)
{
	UINT formatSupport;

	if (FAILED(this->_d3dDevice->CheckFormatSupport(format, &formatSupport)))
	{
		return false;
	}

	const UINT expected = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_MIP | D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_CPU_LOCKABLE;

	return (formatSupport & expected) == expected;
}
