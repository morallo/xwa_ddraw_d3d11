// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#pragma once
#include "Matrices.h"
#include "../shaders/material_defs.h"
#include "../shaders/shader_common.h"
#include <vector>

// Also found in the Floating_GUI_RESNAME list:
extern const char *DC_TARGET_COMP_SRC_RESNAME;
extern const char *DC_LEFT_SENSOR_SRC_RESNAME;
extern const char *DC_LEFT_SENSOR_2_SRC_RESNAME;
extern const char *DC_RIGHT_SENSOR_SRC_RESNAME;
extern const char *DC_RIGHT_SENSOR_2_SRC_RESNAME;
extern const char *DC_SHIELDS_SRC_RESNAME;
extern const char *DC_SOLID_MSG_SRC_RESNAME;
extern const char *DC_BORDER_MSG_SRC_RESNAME;
extern const char *DC_LASER_BOX_SRC_RESNAME;
extern const char *DC_ION_BOX_SRC_RESNAME;
extern const char *DC_BEAM_BOX_SRC_RESNAME;
extern const char *DC_TOP_LEFT_SRC_RESNAME;
extern const char *DC_TOP_RIGHT_SRC_RESNAME;

typedef struct Box_struct {
	float x0, y0, x1, y1;
} Box;

typedef struct uvfloat4_struct {
	float x0, y0, x1, y1;
} uvfloat4;

typedef struct float3_struct {
	float x, y, z;
} float3;

typedef struct float4_struct {
	float x, y, z, w;
} float4;

// Region names. Used in the erase_region and move_region commands
const int LEFT_RADAR_HUD_BOX_IDX		= 0;
const int RIGHT_RADAR_HUD_BOX_IDX	= 1;
const int SHIELDS_HUD_BOX_IDX		= 2;
const int BEAM_HUD_BOX_IDX			= 3;
const int TARGET_HUD_BOX_IDX			= 4;
const int LEFT_MSG_HUD_BOX_IDX		= 5;
const int RIGHT_MSG_HUD_BOX_IDX		= 6;
const int TOP_LEFT_BOX_IDX			= 7;
const int TOP_RIGHT_BOX_IDX			= 8;
const int MAX_HUD_BOXES				= 9;
extern std::vector<const char *>g_HUDRegionNames;
// Convert a string into a *_HUD_BOX_IDX constant
int HUDRegionNameToIndex(char *name);

class DCHUDRegion {
public:
	Box coords;
	Box uv_erase_coords;
	uvfloat4 erase_coords;
	bool bLimitsComputed;
};

/*
 * This class stores the coordinates for each HUD Region : left radar, right radar, text
 * boxes, etc. It does not store the individual HUD elements within each HUD texture. For
 * that, look at DCElementSourceBox
 */
class DCHUDRegions {
public:
	std::vector<DCHUDRegion> boxes;

	DCHUDRegions();

	void Clear() {
		boxes.clear();
	}

	void ResetLimits() {
		for (unsigned int i = 0; i < boxes.size(); i++) 
			boxes[i].bLimitsComputed = false;
	}
};
const int MAX_DC_REGIONS = 9;

const int LEFT_RADAR_DC_ELEM_SRC_IDX = 0;
const int RIGHT_RADAR_DC_ELEM_SRC_IDX = 1;
const int LASER_RECHARGE_DC_ELEM_SRC_IDX = 2;
const int SHIELD_RECHARGE_DC_ELEM_SRC_IDX = 3;
const int ENGINE_RECHARGE_DC_ELEM_SRC_IDX = 4;
const int BEAM_RECHARGE_DC_ELEM_SRC_IDX = 5;
const int SHIELDS_DC_ELEM_SRC_IDX = 6;
const int BEAM_DC_ELEM_SRC_IDX = 7;
const int TARGET_COMP_DC_ELEM_SRC_IDX = 8;
const int QUAD_LASERS_L_DC_ELEM_SRC_IDX = 9;
const int QUAD_LASERS_R_DC_ELEM_SRC_IDX = 10;
const int LEFT_MSG_DC_ELEM_SRC_IDX = 11;
const int RIGHT_MSG_DC_ELEM_SRC_IDX = 12;
const int SPEED_N_THROTTLE_DC_ELEM_SRC_IDX = 13;
const int MISSILES_DC_ELEM_SRC_IDX = 14;
const int NAME_TIME_DC_ELEM_SRC_IDX = 15;
const int NUM_CRAFTS_DC_ELEM_SRC_IDX = 16;
const int QUAD_LASERS_BOTH_DC_ELEM_SRC_IDX = 17;
const int DUAL_LASERS_L_DC_ELEM_SRC_IDX = 18;
const int DUAL_LASERS_R_DC_ELEM_SRC_IDX = 19;
const int DUAL_LASERS_BOTH_DC_ELEM_SRC_IDX = 20;
const int B_WING_LASERS_DC_ELEM_SRC_IDX = 21;
const int SIX_LASERS_BOTH_DC_ELEM_SRC_IDX = 22;
const int SIX_LASERS_L_DC_ELEM_SRC_IDX = 23;
const int SIX_LASERS_R_DC_ELEM_SRC_IDX = 24;
const int MAX_DC_SRC_ELEMENTS = 25;
extern std::vector<const char *>g_DCElemSrcNames;
// Convert a string into a *_DC_ELEM_SRC_IDX constant
int DCSrcElemNameToIndex(char *name);

class DCElemSrcBox {
public:
	Box uv_coords;
	Box coords;
	bool bComputed;

	DCElemSrcBox() {
		bComputed = false;
	}
};

/*
 * Stores the uv_coords and pixel coords for each individual HUD element. Examples of HUD elems
 * are:
 * Laser recharge rate, Shield recharage rate, Radars, etc.
 */
class DCElemSrcBoxes {
public:
	std::vector<DCElemSrcBox> src_boxes;

	void Clear() {
		src_boxes.clear();
	}

	DCElemSrcBoxes();
	void Reset() {
		for (unsigned int i = 0; i < src_boxes.size(); i++)
			src_boxes[i].bComputed = false;
	}
};

enum RenderMainColorKeyType
{
	RENDERMAIN_NO_COLORKEY,
	RENDERMAIN_COLORKEY_20,
	RENDERMAIN_COLORKEY_00,
};

class PrimarySurface;
class DepthSurface;
class BackbufferSurface;
class FrontbufferSurface;
class OffscreenSurface;

/*****************/
// This struct is in the process of moving to the cockpit look hook. It can be removed
// later
typedef struct HeadPosStruct {
	float x, y, z;
} HeadPos;
/*****************/

/* 2D Constant Buffers */
typedef struct MainShadersCBStruct {
	float scale, aspectRatio, parallax, brightness;
	float use_3D, unused0, unused1, unused2;
} MainShadersCBuffer;

typedef struct BarrelPixelShaderCBStruct {
	float k1, k2, k3;
	int unused;
} BarrelPixelShaderCBuffer;

#define BACKBUFFER_FORMAT DXGI_FORMAT_B8G8R8A8_UNORM
//#define BLOOM_BUFFER_FORMAT DXGI_FORMAT_B8G8R8A8_UNORM
#define BLOOM_BUFFER_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define AO_DEPTH_BUFFER_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
//#define AO_DEPTH_BUFFER_FORMAT DXGI_FORMAT_R32G32B32A32_FLOAT
//#define AO_MASK_FORMAT DXGI_FORMAT_R8_UINT
#define AO_MASK_FORMAT DXGI_FORMAT_B8G8R8A8_UNORM
#define HDR_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT

typedef struct BloomConfigStruct {
	float fSaturationStrength, fCockpitStrength, fEngineGlowStrength, fSparksStrength;
	float fLightMapsStrength, fLasersStrength, fHyperStreakStrength, fHyperTunnelStrength;
	float fTurboLasersStrength, fLensFlareStrength, fExplosionsStrength, fSunsStrength;
	float fCockpitSparksStrength, fMissileStrength, fSkydomeLightStrength, fBracketStrength;
	float uvStepSize1, uvStepSize2;
	int iNumPasses;
} BloomConfig;

typedef struct BloomPixelShaderCBStruct {
	float pixelSizeX, pixelSizeY, general_bloom_strength, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	int unused1;
	// 32 bytes
	int unused2;
	float unused3, depth_weight;
	int debug;
	// 48 bytes
} BloomPixelShaderCBuffer;

typedef struct SSAOPixelShaderCBStruct {
	float screenSizeX, screenSizeY, indirect_intensity, bias;
	// 16 bytes
	float intensity, near_sample_radius, black_level;
	int samples;
	// 32 bytes
	int z_division;
	float bentNormalInit, max_dist, power;
	// 48 bytes
	int debug;
	float moire_offset, amplifyFactor;
	int fn_enable;
	// 64 bytes
	float fn_max_xymult, fn_scale, fn_sharpness, nm_intensity_near;
	// 80 bytes
	float far_sample_radius, nm_intensity_far, ambient, amplifyFactor2;
	// 96 bytes
	float x0, y0, x1, y1; // Viewport limits in uv space
	// 112 bytes
	float enable_dist_fade, ssaops_unused1, ssaops_unused2, shadow_epsilon;
	// 128 bytes
	float ssaops_unused3, shadow_step_size, shadow_steps, aspect_ratio;
	// 144 bytes
	float vpScale[4];
	// 160 bytes
	int shadow_enable;
	float shadow_k, Bz_mult, moire_scale;
	// 176 bytes
} SSAOPixelShaderCBuffer;

typedef struct ShadertoyCBStruct {
	float iTime, twirl, bloom_strength, srand;
	// 16 bytes
	float iResolution[2];
	// 0: Non-VR, 1: DirectSBS, 2: SteamVR. 
	// Set to 1 when the Hyperspace Effect is in the HS_POST_HYPER_EXIT_ST state
	// Used in the SunShader too.
	int VRmode; 
	float y_center;
	// 32 bytes
	float x0, y0, x1, y1; // Limits in uv-coords of the viewport
	// 48 bytes
	Matrix4 viewMat; // The view rotation matrix
	// 4*4 = 16 elements, 16 * 4 = 64 bytes
	// 48 + 64 = 112 bytes
	int bDisneyStyle; // Enables the flare when jumping into hyperspace and other details
	int hyperspace_phase; // 1 = HYPER_ENTRY, 2 = HYPER_TUNNEL, 3 = HYPER_EXIT, 4 = POST_HYPER_EXIT (same as HypespacePhaseEnum)
	float tunnel_speed, FOVscale;
	// 128 bytes
	int SunFlareCount;
	float flare_intensity, craft_speed, st_unused1;
	// 144 bytes
	//float SunX, SunY, SunZ, flare_intensity;
	float4 SunCoords[MAX_SUN_FLARES];
	// 208 bytes
	float4 SunColor[MAX_SUN_FLARES];
	// 272 bytes
} ShadertoyCBuffer;

// Let's make this Constant Buffer the same size as the ShadertoyCBuffer
// so that we can reuse the same CB slot -- after all, we can't manipulate
// anything while travelling through hyperspace anyway...
typedef struct LaserPointerCBStruct {
	int TriggerState; // 0 = Not pressed, 1 = Pressed
	float FOVscale, iResolution[2];
	// 16 bytes
	float x0, y0, x1, y1; // Limits in uv-coords of the viewport
	// 32 bytes
	float contOrigin[2], intersection[2];
	// 48 bytes
	int bContOrigin, bIntersection, bHoveringOnActiveElem;
	int DirectSBSEye;
	// 64 bytes
	float v0[2], v1[2]; // DEBUG
	// 80 bytes
	float v2[2], uv[2]; // DEBUG
	// 96
	int bDebugMode;
	float cursor_radius, unused[2];
	// 112 bytes
} LaserPointerCBuffer;

/* 3D Constant Buffers */
typedef struct VertexShaderCBStruct {
	float viewportScale[4];
	// 16 bytes
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	// 32 bytes
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
	// 48 bytes
	//float vsunused0, vsunused1, vsunused2, vsunused3;
	// 64 bytes
} VertexShaderCBuffer;

typedef struct VertexShaderMatrixCBStruct {
	Matrix4 projEye;
	Matrix4 viewMat;
	Matrix4 fullViewMat;
} VertexShaderMatrixCB;

typedef struct PSShadingSystemCBStruct {
	float3 MainLight;
	uint32_t LightCount;
	// 16 bytes
	float4 MainColor;
	// 32 bytes
	float4 LightVector[MAX_XWA_LIGHTS];
	// 32+128 = 160 bytes
	float4 LightColor[MAX_XWA_LIGHTS];
	// 160+128 = 288 bytes
	float spec_intensity, glossiness, spec_bloom_intensity, bloom_glossiness_mult;
	// 304 bytes
	float saturation_boost, lightness_boost, ssdo_enabled;
	uint32_t ss_debug;
	// 320 bytes
	float sso_disable, sqr_attenuation, laser_light_intensity;
	uint32_t num_lasers;
	// 336 bytes
	float4 LightPoint[MAX_CB_POINT_LIGHTS];
	// 8 * 16 = 128
	// 464 bytes
	float4 LightPointColor[MAX_CB_POINT_LIGHTS];
	// 8 * 16 = 128
	// 592 bytes
	float ambient, ss_unused0, ss_unused1, ss_unused2;
	// 608 bytes
} PSShadingSystemCB;

typedef struct PixelShaderCBStruct {
	float brightness;			// Used to control the brightness of some elements -- mostly for ReShade compatibility
	uint32_t DynCockpitSlots;
	uint32_t bUseCoverTexture;
	uint32_t bInHyperspace;
	// 16 bytes
	
	uint32_t bIsLaser;
	uint32_t bIsLightTexture;
	uint32_t bIsEngineGlow;
	uint32_t ps_unused1; // Formerly this was bIsSun
	// 32 bytes

	float fBloomStrength;
	float fPosNormalAlpha;
	float fSSAOMaskVal;
	float fSSAOAlphaMult;
	// 48 bytes

	uint32_t bIsShadeless;
	float fGlossiness, fSpecInt, fNMIntensity;
	// 64 bytes

	float fSpecVal, fDisableDiffuse;
	uint32_t debug;
	float ps_unused2;
	// 80 bytes
} PixelShaderCBuffer;

// Pixel Shader constant buffer for the Dynamic Cockpit
typedef struct DCPixelShaderCBStruct {
	uvfloat4 src[MAX_DC_COORDS_PER_TEXTURE];
	// 4 * MAX_DC_COORDS * 4 = 192
	uvfloat4 dst[MAX_DC_COORDS_PER_TEXTURE];
	// 4 * MAX_DC_COORDS * 4 = 192
	// 384 bytes thus far
	uint32_t bgColor[MAX_DC_COORDS_PER_TEXTURE]; // 32-bit Background colors
	// 4 * MAX_DC_COORDS = 48
	// 432 bytes thus far

	float ct_brightness, dc_brightness;
	float unused[2];
	// 448 bytes
} DCPixelShaderCBuffer;

typedef struct uv_coords_src_dst_struct {
	int src_slot[MAX_DC_COORDS_PER_TEXTURE]; // This src slot references one of the pre-defined DC internal areas
	uvfloat4 dst[MAX_DC_COORDS_PER_TEXTURE];
	uint32_t uBGColor[MAX_DC_COORDS_PER_TEXTURE];
	uint32_t uHGColor[MAX_DC_COORDS_PER_TEXTURE];
	uint32_t uWHColor[MAX_DC_COORDS_PER_TEXTURE];
	int numCoords;
} uv_src_dst_coords;

typedef struct uv_coords_struct {
	uvfloat4 src[MAX_DC_COORDS_PER_TEXTURE];
	int numCoords;
} uv_coords;

const int MAX_TEXTURE_NAME = 128;
typedef struct dc_element_struct {
	uv_src_dst_coords coords;
	int erase_slots[MAX_DC_COORDS_PER_TEXTURE];
	int num_erase_slots;
	char name[MAX_TEXTURE_NAME];
	char coverTextureName[MAX_TEXTURE_NAME];
	//ComPtr<ID3D11ShaderResourceView> coverTexture = nullptr;
	//ID3D11ShaderResourceView *coverTexture = NULL;
	bool bActive, bNameHasBeenTested;
} dc_element;

typedef struct move_region_coords_struct {
	int region_slot[MAX_HUD_BOXES];
	uvfloat4 dst[MAX_HUD_BOXES];
	int numCoords;
} move_region_coords;

// ACTIVE COCKPIT
#define MAX_AC_COORDS_PER_TEXTURE 64
#define MAX_AC_TEXTURES_PER_COCKPIT 16
#define MAX_AC_ACTION_LEN 8 // WORDs (scan codes) used to specify an action
typedef struct ac_uv_coords_struct {
	uvfloat4 area[MAX_AC_COORDS_PER_TEXTURE];
	WORD action[MAX_AC_COORDS_PER_TEXTURE][MAX_AC_ACTION_LEN]; // List of scan codes
	char action_name[MAX_AC_COORDS_PER_TEXTURE][16]; // For debug purposes only, remove later
	int numCoords;
} ac_uv_coords;

typedef struct ac_element_struct {
	ac_uv_coords coords;
	//int idx; // "Back pointer" into the g_ACElements array
	char name[MAX_TEXTURE_NAME];
	bool bActive, bNameHasBeenTested;
	short width, height; // DEBUG, remove later
} ac_element;

// SSAO Type
typedef enum {
	SSO_AMBIENT,
	SSO_DIRECTIONAL,
	SSO_BENT_NORMALS,
	SSO_DEFERRED, // New Shading Model
} SSAOTypeEnum;

// In order to blend the background with the hyperspace effect when exiting, we need to extend the
// effect for a few more frames. To do that, we need to track the current state of the effect and
// that's why we need a small state machine:
enum HyperspacePhaseEnum {
	HS_INIT_ST = 0,				// Initial state, we're not even in Hyperspace
	HS_HYPER_ENTER_ST = 1,		// We're entering hyperspace
	HS_HYPER_TUNNEL_ST = 2,		// Traveling through the blue Hyperspace tunnel
	HS_HYPER_EXIT_ST = 3,		// HyperExit streaks are being rendered
	HS_POST_HYPER_EXIT_ST = 4   // HyperExit streaks have finished rendering; but now we're blending with the backround
};
const int MAX_POST_HYPER_EXIT_FRAMES = 10; // I had 20 here up to version 1.1.1. Making this smaller makes the zoom faster

// General types and globals
typedef enum {
	TRACKER_NONE,
	TRACKER_FREEPIE,
	TRACKER_STEAMVR,
	TRACKER_TRACKIR
} TrackerType;

struct MaterialStruct;

extern struct MaterialStruct g_DefaultGlobalMaterial;

// Materials
typedef struct MaterialStruct {
	float Metallic;
	float Intensity;
	float Glossiness;
	float NMIntensity;
	float SpecValue;
	bool IsShadeless;
	Vector3 Light;

	MaterialStruct() {
		Metallic    = g_DefaultGlobalMaterial.Metallic;
		Intensity   = g_DefaultGlobalMaterial.Intensity;
		Glossiness  = g_DefaultGlobalMaterial.Glossiness;
		NMIntensity = g_DefaultGlobalMaterial.NMIntensity;
		SpecValue   = g_DefaultGlobalMaterial.SpecValue;
		IsShadeless = g_DefaultGlobalMaterial.IsShadeless;
		Light		= g_DefaultGlobalMaterial.Light;
	}
} Material;

// Color-Light links
class Direct3DTexture;
typedef struct ColorLightPairStruct {
	Direct3DTexture *color, *light;

	ColorLightPairStruct(Direct3DTexture *color) {
		this->color = color;
		this->light = NULL;
	}
} ColorLightPair;

/*
 * Used to store a list of textures for fast lookup. For instance, all suns must
 * have their associated lights reset after jumping through hyperspace; and all
 * textures with materials can be placed here so that material properties can be
 * re-applied while flying.
 */
/*
typedef struct AuxTextureDataStruct {
	Direct3DTexture *texture;
} AuxTextureData;
*/

/*
class XWALightInfoStruct {
public:
	bool Tested, IsSun;

public:
	XWALightInfoStruct() {
		Tested = false;
		IsSun = false;
	}
};
*/

// S0x07D4FA0
struct XwaGlobalLight
{
	/* 0x0000 */ int PositionX;
	/* 0x0004 */ int PositionY;
	/* 0x0008 */ int PositionZ;
	/* 0x000C */ float DirectionX;
	/* 0x0010 */ float DirectionY;
	/* 0x0014 */ float DirectionZ;
	/* 0x0018 */ float Intensity;
	/* 0x001C */ float XwaGlobalLight_m1C;
	/* 0x0020 */ float ColorR;
	/* 0x0024 */ float ColorB;
	/* 0x0028 */ float ColorG;
	/* 0x002C */ float BlendStartIntensity;
	/* 0x0030 */ float BlendStartColor1C;
	/* 0x0034 */ float BlendStartColorR;
	/* 0x0038 */ float BlendStartColorB;
	/* 0x003C */ float BlendStartColorG;
	/* 0x0040 */ float BlendEndIntensity;
	/* 0x0044 */ float BlendEndColor1C;
	/* 0x0048 */ float BlendEndColorR;
	/* 0x004C */ float BlendEndColorB;
	/* 0x0050 */ float BlendEndColorG;
};

#define MAX_HEAP_ELEMS 32
class VectorColor {
public:
	Vector3 P;
	Vector3 col;
};

class SmallestK {
public:
	// Insert-sort-like algorithm to keep the k smallest elements from a constant flow
	VectorColor _elems[MAX_CB_POINT_LIGHTS];
	int _size;

public:
	SmallestK() {
		clear();
	}

	inline void clear() {
		_size = 0;
	}

	void insert(Vector3 P, Vector3 col);
};

#define MAX_SPEED_PARTICLES 256
extern Vector4 g_SpeedParticles[MAX_SPEED_PARTICLES];

class DeviceResources
{
public:
	DeviceResources();

	HRESULT Initialize();

	HRESULT OnSizeChanged(HWND hWnd, DWORD dwWidth, DWORD dwHeight);

	HRESULT LoadMainResources();

	HRESULT LoadResources();

	void InitInputLayout(ID3D11InputLayout* inputLayout);
	void InitVertexShader(ID3D11VertexShader* vertexShader);
	void InitPixelShader(ID3D11PixelShader* pixelShader);
	void InitTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void InitRasterizerState(ID3D11RasterizerState* state);
	void InitPSShaderResourceView(ID3D11ShaderResourceView* texView);
	HRESULT InitSamplerState(ID3D11SamplerState** sampler, D3D11_SAMPLER_DESC* desc);
	HRESULT InitBlendState(ID3D11BlendState* blend, D3D11_BLEND_DESC* desc);
	HRESULT InitDepthStencilState(ID3D11DepthStencilState* depthState, D3D11_DEPTH_STENCIL_DESC* desc);
	void InitVertexBuffer(ID3D11Buffer** buffer, UINT* stride, UINT* offset);
	void InitIndexBuffer(ID3D11Buffer* buffer, bool isFormat32);
	void InitViewport(D3D11_VIEWPORT* viewport);
	void InitVSConstantBuffer3D(ID3D11Buffer** buffer, const VertexShaderCBuffer* vsCBuffer);
	void InitVSConstantBufferMatrix(ID3D11Buffer** buffer, const VertexShaderMatrixCB* vsCBuffer);
	void InitPSConstantShadingSystem(ID3D11Buffer** buffer, const PSShadingSystemCB* psCBuffer);
	void InitVSConstantBuffer2D(ID3D11Buffer** buffer, const float parallax, const float aspectRatio, const float scale, const float brightness, const float use_3D);
	void InitPSConstantBuffer2D(ID3D11Buffer** buffer, const float parallax, const float aspectRatio, const float scale, const float brightness);
	void InitPSConstantBufferBarrel(ID3D11Buffer** buffer, const float k1, const float k2, const float k3);
	void InitPSConstantBufferBloom(ID3D11Buffer ** buffer, const BloomPixelShaderCBuffer * psConstants);
	void InitPSConstantBufferSSAO(ID3D11Buffer ** buffer, const SSAOPixelShaderCBuffer * psConstants);
	void InitPSConstantBuffer3D(ID3D11Buffer** buffer, const PixelShaderCBuffer *psConstants);
	void InitPSConstantBufferDC(ID3D11Buffer** buffer, const DCPixelShaderCBuffer * psConstants);
	void InitPSConstantBufferHyperspace(ID3D11Buffer ** buffer, const ShadertoyCBuffer * psConstants);

	void InitPSConstantBufferLaserPointer(ID3D11Buffer ** buffer, const LaserPointerCBuffer * psConstants);

	void BuildHUDVertexBuffer(UINT width, UINT height);
	void BuildHyperspaceVertexBuffer(UINT width, UINT height);
	void BuildPostProcVertexBuffer();
	void InitSpeedParticlesVB(UINT width, UINT height);
	void BuildSpeedVertexBuffer(UINT width, UINT height);
	void CreateRandomVectorTexture();
	void DeleteRandomVectorTexture();
	void ClearDynCockpitVector(dc_element DCElements[], int size);
	void ClearActiveCockpitVector(ac_element ACElements[], int size);

	void ResetDynamicCockpit();

	void ResetActiveCockpit();

	HRESULT RenderMain(char* buffer, DWORD width, DWORD height, DWORD bpp, RenderMainColorKeyType useColorKey = RENDERMAIN_COLORKEY_20);

	HRESULT RetrieveBackBuffer(char* buffer, DWORD width, DWORD height, DWORD bpp);

	UINT GetMaxAnisotropy();

	void CheckMultisamplingSupport();

	bool IsTextureFormatSupported(DXGI_FORMAT format);

	DWORD _displayWidth;
	DWORD _displayHeight;
	DWORD _displayBpp;
	DWORD _displayTempWidth;
	DWORD _displayTempHeight;
	DWORD _displayTempBpp;

	D3D_DRIVER_TYPE _d3dDriverType;
	D3D_FEATURE_LEVEL _d3dFeatureLevel;
	ComPtr<ID3D11Device> _d3dDevice;
	ComPtr<ID3D11DeviceContext> _d3dDeviceContext;
	ComPtr<IDXGISwapChain> _swapChain;
	// Buffers/Textures
	ComPtr<ID3D11Texture2D> _backBuffer;
	ComPtr<ID3D11Texture2D> _offscreenBuffer;
	ComPtr<ID3D11Texture2D> _offscreenBufferR; // When SteamVR is used, _offscreenBuffer becomes the left eye and this one becomes the right eye
	ComPtr<ID3D11Texture2D> _offscreenBufferAsInput;
	ComPtr<ID3D11Texture2D> _offscreenBufferAsInputR; // When SteamVR is used, this is the right eye as input buffer
	// Dynamic Cockpit
	ComPtr<ID3D11Texture2D> _offscreenBufferDynCockpit;    // Used to render the targeting computer dynamically <-- Need to re-check this claim
	ComPtr<ID3D11Texture2D> _offscreenBufferDynCockpitBG;  // Used to render the targeting computer dynamically <-- Need to re-check this claim
	ComPtr<ID3D11Texture2D> _offscreenAsInputDynCockpit;   // HUD elements buffer
	ComPtr<ID3D11Texture2D> _offscreenAsInputDynCockpitBG; // HUD element backgrounds buffer
	ComPtr<ID3D11Texture2D> _DCTextMSAA;				   // "RTV" to render text
	ComPtr<ID3D11Texture2D> _DCTextAsInput;				   // Resolved from DCTextMSAA for use in shaders
	// Barrel effect
	ComPtr<ID3D11Texture2D> _offscreenBufferPost;  // This is the output of the barrel effect
	ComPtr<ID3D11Texture2D> _offscreenBufferPostR; // This is the output of the barrel effect for the right image when using SteamVR
	ComPtr<ID3D11Texture2D> _steamVRPresentBuffer; // This is the buffer that will be presented for SteamVR
	// ShaderToy effects
	ComPtr<ID3D11Texture2D> _shadertoyBufMSAA;
	ComPtr<ID3D11Texture2D> _shadertoyBufMSAA_R;
	ComPtr<ID3D11Texture2D> _shadertoyBuf;      // No MSAA
	ComPtr<ID3D11Texture2D> _shadertoyBufR;     // No MSAA
	ComPtr<ID3D11Texture2D> _shadertoyAuxBuf;   // No MSAA
	ComPtr<ID3D11Texture2D> _shadertoyAuxBufR;  // No MSAA
	// Bloom
	ComPtr<ID3D11Texture2D> _offscreenBufferBloomMask;  // Used to render the bloom mask
	ComPtr<ID3D11Texture2D> _offscreenBufferBloomMaskR; // Used to render the bloom mask to the right image (SteamVR)
	ComPtr<ID3D11Texture2D> _offscreenBufferAsInputBloomMask;  // Used to resolve offscreenBufferBloomMask
	ComPtr<ID3D11Texture2D> _offscreenBufferAsInputBloomMaskR; // Used to resolve offscreenBufferBloomMaskR
	ComPtr<ID3D11Texture2D> _bloomOutput1; // Output from bloom pass 1
	ComPtr<ID3D11Texture2D> _bloomOutput2; // Output from bloom pass 2
	ComPtr<ID3D11Texture2D> _bloomOutputSum; // Bloom accummulator
	ComPtr<ID3D11Texture2D> _bloomOutput1R; // Output from bloom pass 1, right image (SteamVR)
	ComPtr<ID3D11Texture2D> _bloomOutput2R; // Output from bloom pass 2, right image (SteamVR)
	ComPtr<ID3D11Texture2D> _bloomOutputSumR; // Bloom accummulator (SteamVR)
	// Ambient Occlusion
	ComPtr<ID3D11Texture2D> _depthBuf;
	ComPtr<ID3D11Texture2D> _depthBufR;
	ComPtr<ID3D11Texture2D> _depthBufAsInput;
	ComPtr<ID3D11Texture2D> _depthBufAsInputR; // Used in SteamVR mode
	ComPtr<ID3D11Texture2D> _depthBuf2;
	ComPtr<ID3D11Texture2D> _depthBuf2R;
	ComPtr<ID3D11Texture2D> _depthBuf2AsInput;
	ComPtr<ID3D11Texture2D> _depthBuf2AsInputR; // Used in SteamVR mode
	ComPtr<ID3D11Texture2D> _bentBuf;		// No MSAA
	ComPtr<ID3D11Texture2D> _bentBufR;		// No MSAA
	ComPtr<ID3D11Texture2D> _ssaoBuf;		// No MSAA
	ComPtr<ID3D11Texture2D> _ssaoBufR;		// No MSAA
	// Shading System
	ComPtr<ID3D11Texture2D> _normBufMSAA;
	ComPtr<ID3D11Texture2D> _normBufMSAA_R;
	ComPtr<ID3D11Texture2D> _normBuf;		 // No MSAA so that it can be both bound to RTV and SRV
	ComPtr<ID3D11Texture2D> _normBufR;		 // No MSAA
	ComPtr<ID3D11Texture2D> _ssaoMaskMSAA;
	ComPtr<ID3D11Texture2D> _ssaoMaskMSAA_R;
	ComPtr<ID3D11Texture2D> _ssaoMask;		 // No MSAA
	ComPtr<ID3D11Texture2D> _ssaoMaskR;		 // No MSAA
	ComPtr<ID3D11Texture2D> _ssMaskMSAA;	
	ComPtr<ID3D11Texture2D> _ssMaskMSAA_R;
	ComPtr<ID3D11Texture2D> _ssMask;			 // No MSAA
	ComPtr<ID3D11Texture2D> _ssMaskR;		 // No MSAA
	//ComPtr<ID3D11Texture2D> _ssEmissionMask;	  // No MSAA, Screen-Space emission mask
	//ComPtr<ID3D11Texture2D> _ssEmissionMaskR; // No MSAA, Screen-Space emission mask

	// RTVs
	ComPtr<ID3D11RenderTargetView> _renderTargetView;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewR; // When SteamVR is used, _renderTargetView is the left eye, and this one is the right eye
	// Dynamic Cockpit
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDynCockpit; // Used to render the HUD to an offscreen buffer
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDynCockpitBG; // Used to render the HUD to an offscreen buffer
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDynCockpitAsInput; // RTV that writes to _offscreenBufferAsInputDynCockpit directly
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDynCockpitAsInputBG; // RTV that writes to _offscreenBufferAsInputDynCockpitBG directly
	ComPtr<ID3D11RenderTargetView> _DCTextRTV;
	ComPtr<ID3D11RenderTargetView> _DCTextAsInputRTV;
	// Barrel Effect
	ComPtr<ID3D11RenderTargetView> _renderTargetViewPost;  // Used for the barrel effect
	ComPtr<ID3D11RenderTargetView> _renderTargetViewPostR; // Used for the barrel effect (right image) when SteamVR is used.
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSteamVRResize; // Used for the barrel effect
	// ShaderToy
	ComPtr<ID3D11RenderTargetView> _shadertoyRTV;
	ComPtr<ID3D11RenderTargetView> _shadertoyRTV_R;
	// Bloom
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloomMask  = NULL; // Renders to _offscreenBufferBloomMask
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloomMaskR = NULL; // Renders to _offscreenBufferBloomMaskR
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloom1;    // Renders to bloomOutput1
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloom2;    // Renders to bloomOutput2
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloomSum;  // Renders to bloomOutputSum
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloom1R;   // Renders to bloomOutput1R
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloom2R;   // Renders to bloomOutput2R
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBloomSumR; // Renders to bloomOutputSumR
	// Ambient Occlusion
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDepthBuf;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDepthBufR;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDepthBuf2;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewDepthBuf2R;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBentBuf;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewBentBufR;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSAO;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSAO_R;
	// Shading System
	ComPtr<ID3D11RenderTargetView> _renderTargetViewNormBuf;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewNormBufR;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSAOMask;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSAOMaskR;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSMask;
	ComPtr<ID3D11RenderTargetView> _renderTargetViewSSMaskR;
	//ComPtr<ID3D11RenderTargetView> _renderTargetViewEmissionMask;
	//ComPtr<ID3D11RenderTargetView> _renderTargetViewEmissionMaskR;

	// SRVs
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputShaderResourceView;
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputShaderResourceViewR; // When SteamVR is enabled, this is the SRV for the right eye
	// Dynamic Cockpit
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputDynCockpitSRV;    // SRV for HUD elements without background
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputDynCockpitBG_SRV; // SRV for HUD element backgrounds
	ComPtr<ID3D11ShaderResourceView> _DCTextSRV;						// SRV for the HUD text
	// Shadertoy
	ComPtr<ID3D11ShaderResourceView> _shadertoySRV;
	ComPtr<ID3D11ShaderResourceView> _shadertoySRV_R;
	ComPtr<ID3D11ShaderResourceView> _shadertoyAuxSRV;
	ComPtr<ID3D11ShaderResourceView> _shadertoyAuxSRV_R;
	// Bloom
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputBloomMaskSRV;
	ComPtr<ID3D11ShaderResourceView> _offscreenAsInputBloomMaskSRV_R;
	ComPtr<ID3D11ShaderResourceView> _bloomOutput1SRV;     // SRV for bloomOutput1
	ComPtr<ID3D11ShaderResourceView> _bloomOutput2SRV;     // SRV for bloomOutput2
	ComPtr<ID3D11ShaderResourceView> _bloomOutputSumSRV;   // SRV for bloomOutputSum
	ComPtr<ID3D11ShaderResourceView> _bloomOutput1SRV_R;   // SRV for bloomOutput1R
	ComPtr<ID3D11ShaderResourceView> _bloomOutput2SRV_R;   // SRV for bloomOutput2R
	ComPtr<ID3D11ShaderResourceView> _bloomOutputSumSRV_R; // SRV for bloomOutputSumR
	// Ambient Occlusion
	ComPtr<ID3D11ShaderResourceView> _depthBufSRV;    // SRV for depthBufAsInput
	ComPtr<ID3D11ShaderResourceView> _depthBufSRV_R;  // SRV for depthBufAsInputR
	ComPtr<ID3D11ShaderResourceView> _depthBuf2SRV;   // SRV for depthBuf2AsInput
	ComPtr<ID3D11ShaderResourceView> _depthBuf2SRV_R; // SRV for depthBuf2AsInputR
	ComPtr<ID3D11ShaderResourceView> _bentBufSRV;     // SRV for bentBuf
	ComPtr<ID3D11ShaderResourceView> _bentBufSRV_R;   // SRV for bentBufR
	ComPtr<ID3D11ShaderResourceView> _ssaoBufSRV;     // SRV for ssaoBuf
	ComPtr<ID3D11ShaderResourceView> _ssaoBufSRV_R;   // SRV for ssaoBuf
	// Shading System
	ComPtr<ID3D11ShaderResourceView> _normBufSRV;     // SRV for normBuf
	ComPtr<ID3D11ShaderResourceView> _normBufSRV_R;   // SRV for normBufR
	ComPtr<ID3D11ShaderResourceView> _ssaoMaskSRV;    // SRV for ssaoMask
	ComPtr<ID3D11ShaderResourceView> _ssaoMaskSRV_R;  // SRV for ssaoMaskR
	ComPtr<ID3D11ShaderResourceView> _ssMaskSRV;      // SRV for ssMask
	ComPtr<ID3D11ShaderResourceView> _ssMaskSRV_R;    // SRV for ssMaskR
	//ComPtr<ID3D11ShaderResourceView> _ssEmissionMaskSRV;    // SRV for ssEmissionMask
	//ComPtr<ID3D11ShaderResourceView> _ssEmissionMaskSRV_R;  // SRV for ssEmissionMaskR

	ComPtr<ID3D11Texture2D> _depthStencilL;
	ComPtr<ID3D11Texture2D> _depthStencilR;
	ComPtr<ID3D11DepthStencilView> _depthStencilViewL;
	ComPtr<ID3D11DepthStencilView> _depthStencilViewR;

	ComPtr<ID2D1Factory> _d2d1Factory;
	ComPtr<IDWriteFactory> _dwriteFactory;
	ComPtr<ID2D1RenderTarget> _d2d1RenderTarget;
	ComPtr<ID2D1RenderTarget> _d2d1OffscreenRenderTarget;
	ComPtr<ID2D1DrawingStateBlock> _d2d1DrawingStateBlock;

	ComPtr<ID3D11VertexShader> _mainVertexShader;
	ComPtr<ID3D11InputLayout> _mainInputLayout;
	ComPtr<ID3D11PixelShader> _mainPixelShader;
	ComPtr<ID3D11PixelShader> _mainPixelShaderBpp2ColorKey20;
	ComPtr<ID3D11PixelShader> _mainPixelShaderBpp2ColorKey00;
	ComPtr<ID3D11PixelShader> _mainPixelShaderBpp4ColorKey20;
	ComPtr<ID3D11PixelShader> _basicPixelShader;
	ComPtr<ID3D11PixelShader> _barrelPixelShader;
	ComPtr<ID3D11PixelShader> _bloomHGaussPS;
	ComPtr<ID3D11PixelShader> _bloomVGaussPS;
	ComPtr<ID3D11PixelShader> _bloomCombinePS;
	ComPtr<ID3D11PixelShader> _bloomBufferAddPS;
	ComPtr<ID3D11PixelShader> _ssaoPS;
	ComPtr<ID3D11PixelShader> _ssaoBlurPS;
	ComPtr<ID3D11PixelShader> _ssaoAddPS;
	ComPtr<ID3D11PixelShader> _ssdoDirectPS;
	ComPtr<ID3D11PixelShader> _ssdoIndirectPS;
	ComPtr<ID3D11PixelShader> _ssdoAddPS;
	ComPtr<ID3D11PixelShader> _ssdoBlurPS;
	ComPtr<ID3D11PixelShader> _deathStarPS;
	ComPtr<ID3D11PixelShader> _hyperEntryPS;
	ComPtr<ID3D11PixelShader> _hyperExitPS;
	ComPtr<ID3D11PixelShader> _hyperTunnelPS;
	ComPtr<ID3D11PixelShader> _hyperZoomPS;
	ComPtr<ID3D11PixelShader> _hyperComposePS;
	ComPtr<ID3D11PixelShader> _laserPointerPS;
	ComPtr<ID3D11PixelShader> _fxaaPS;
	ComPtr<ID3D11PixelShader> _externalHUDPS;
	ComPtr<ID3D11PixelShader> _sunShaderPS;
	ComPtr<ID3D11PixelShader> _sunFlareShaderPS;
	ComPtr<ID3D11PixelShader> _sunFlareComposeShaderPS;
	
	ComPtr<ID3D11PixelShader> _speedEffectPS;
	ComPtr<ID3D11PixelShader> _speedEffectComposePS;
	ComPtr<ID3D11PixelShader> _singleBarrelPixelShader;
	ComPtr<ID3D11RasterizerState> _mainRasterizerState;
	ComPtr<ID3D11SamplerState> _mainSamplerState;
	ComPtr<ID3D11BlendState> _mainBlendState;
	ComPtr<ID3D11DepthStencilState> _mainDepthState;
	ComPtr<ID3D11Buffer> _mainVertexBuffer;
	ComPtr<ID3D11Buffer> _steamVRPresentVertexBuffer; // Used in SteamVR mode to correct the image presented on the monitor
	ComPtr<ID3D11Buffer> _mainIndexBuffer;
	ComPtr<ID3D11Texture2D> _mainDisplayTexture;
	ComPtr<ID3D11ShaderResourceView> _mainDisplayTextureView;
	ComPtr<ID3D11Texture2D> _mainDisplayTextureTemp;
	ComPtr<ID3D11ShaderResourceView> _mainDisplayTextureViewTemp;

	ComPtr<ID3D11VertexShader> _vertexShader;
	ComPtr<ID3D11VertexShader> _sbsVertexShader;
	ComPtr<ID3D11VertexShader> _passthroughVertexShader;
	ComPtr<ID3D11VertexShader> _speedEffectVS;
	ComPtr<ID3D11InputLayout> _inputLayout;
	ComPtr<ID3D11PixelShader> _pixelShaderTexture;
	ComPtr<ID3D11PixelShader> _pixelShaderDC;
	ComPtr<ID3D11PixelShader> _pixelShaderEmptyDC;
	ComPtr<ID3D11PixelShader> _pixelShaderHUD;
	ComPtr<ID3D11PixelShader> _pixelShaderSolid;
	ComPtr<ID3D11PixelShader> _pixelShaderClearBox;
	ComPtr<ID3D11RasterizerState> _rasterizerState;
	ComPtr<ID3D11Buffer> _VSConstantBuffer;
	ComPtr<ID3D11Buffer> _VSMatrixBuffer;
	ComPtr<ID3D11Buffer> _shadingSysBuffer;
	ComPtr<ID3D11Buffer> _PSConstantBuffer;
	ComPtr<ID3D11Buffer> _PSConstantBufferDC;
	ComPtr<ID3D11Buffer> _barrelConstantBuffer;
	ComPtr<ID3D11Buffer> _bloomConstantBuffer;
	ComPtr<ID3D11Buffer> _ssaoConstantBuffer;
	ComPtr<ID3D11Buffer> _hyperspaceConstantBuffer;
	ComPtr<ID3D11Buffer> _laserPointerConstantBuffer;
	ComPtr<ID3D11Buffer> _mainShadersConstantBuffer;
	
	ComPtr<ID3D11Buffer> _postProcessVertBuffer;
	ComPtr<ID3D11Buffer> _HUDVertexBuffer;
	ComPtr<ID3D11Buffer> _clearHUDVertexBuffer;
	ComPtr<ID3D11Buffer> _hyperspaceVertexBuffer;
	ComPtr<ID3D11Buffer> _speedParticlesVertexBuffer;
	bool _bHUDVerticesReady;

	// Dynamic Cockpit coverTextures:
	ComPtr<ID3D11ShaderResourceView> dc_coverTexture[MAX_DC_SRC_ELEMENTS];

	BOOL _useAnisotropy;
	BOOL _useMultisampling;
	DXGI_SAMPLE_DESC _sampleDesc;
	UINT _backbufferWidth;
	UINT _backbufferHeight;
	DXGI_RATIONAL _refreshRate;
	bool _are16BppTexturesSupported;
	bool _use16BppMainDisplayTexture;
	DWORD _mainDisplayTextureBpp;

	float clearColor[4];
	float clearColorRGBA[4];
	float clearDepth;
	bool sceneRendered;
	bool sceneRenderedEmpty;
	bool inScene;
	bool inSceneBackbufferLocked;

	PrimarySurface* _primarySurface;
	DepthSurface* _depthSurface;
	BackbufferSurface* _backbufferSurface;
	FrontbufferSurface* _frontbufferSurface;
	OffscreenSurface* _offscreenSurface;
};
