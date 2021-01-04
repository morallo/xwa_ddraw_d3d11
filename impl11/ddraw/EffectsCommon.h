#pragma once

#include "Matrices.h"
#include "../shaders/material_defs.h"
#include "../shaders/shader_common.h"


#ifndef EFFECTS_CONST
#define EFFECTS_CONST
const int MAX_TEXTURE_NAME = 128;
const float PI = 3.141593f;
const float DEG2RAD = PI / 180.0f;
#endif

// General types and globals

typedef struct uvfloat4_struct {
	float x0, y0, x1, y1;
} uvfloat4;

typedef struct float3_struct {
	float x, y, z;
} float3;

typedef struct float4_struct {
	float x, y, z, w;
} float4;

// Holds the current 3D reconstruction constants, register b6
typedef struct MetricReconstructionCBStruct {
	float mr_aspect_ratio;   // Same as sm_aspect_ratio (g_fCurInGameAspectRatio), remove sm_* later
	float mr_FOVscale;       // Same as sm_FOVscale NOT the same as g_ShadertoyBuffer.FOVscale
	float mr_y_center;       // Same as sm_y_center NOT the same as g_ShadertoyBuffer.y_center
	float mr_z_metric_mult;  // Probably NOT the same as sm_z_factor

	float mr_cur_metric_scale, mr_shadow_OBJ_scale, mr_screen_aspect_ratio, mr_debug_value;

	//float mr_vr_aspect_ratio_comp[2]; // This is used with shaders that work with the postproc vertexbuf, like the reticle shader
	float mr_vr_aspect_ratio, mr_unused0;
	float mv_vr_vertexbuf_aspect_ratio_comp[2]; // This is used to render the HUD
} MetricReconstructionCB;

// Color-Light links
class Direct3DTexture;
typedef struct ColorLightPairStruct {
	Direct3DTexture* color, * light;

	ColorLightPairStruct(Direct3DTexture* color) {
		this->color = color;
		this->light = NULL;
	}
} ColorLightPair;

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
	float far_sample_radius, nm_intensity_far, ssao_unused0, amplifyFactor2;
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
	float flare_intensity;
	float preserveAspectRatioComp[2]; // Used to compensate for the distortion introduced when PreserveAspectRatio = 0 in DDraw.cfg
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
	float cursor_radius, lp_aspect_ratio[2];
	// 112 bytes
} LaserPointerCBuffer;

/* 3D Constant Buffers */
typedef struct VertexShaderCBStruct {
	float viewportScale[4];
	// 16 bytes
	float aspect_ratio;
	uint32_t apply_uv_comp;
	float z_override, sz_override;
	// 32 bytes
	float mult_z_override, bPreventTransform, bFullTransform, scale_override;
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
	float ambient, headlights_angle_cos, HDR_white_point;
	uint32_t HDREnabled;
	// 608 bytes
} PSShadingSystemCB;

// See PixelShaderTextureCommon.h for an explanation of these settings
typedef struct PixelShaderCBStruct {
	float brightness;			// Used to control the brightness of some elements -- mostly for ReShade compatibility
	uint32_t DynCockpitSlots;
	uint32_t bUseCoverTexture;
	uint32_t bInHyperspace;
	// 16 bytes

	uint32_t bIsLaser;
	uint32_t bIsLightTexture;
	uint32_t bIsEngineGlow;
	uint32_t ps_unused1;
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
	uint32_t special_control;
	float fAmbient;
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
	uint32_t noisy_holo; // If set to 1, the hologram shader will be noisy!
	float transparent; // If set to 1, the background will be transparent
	// 448 bytes
} DCPixelShaderCBuffer;

/* 2D Constant Buffers */
typedef struct MainShadersCBStruct {
	float scale, aspectRatio, parallax, brightness;
	float use_3D, inv_scale, unused1, unused2;
} MainShadersCBuffer;

typedef struct BarrelPixelShaderCBStruct {
	float k1, k2, k3;
	int unused;
} BarrelPixelShaderCBuffer;

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

typedef enum {
	GLOBAL_FOV,
	XWAHACKER_FOV,
	XWAHACKER_LARGE_FOV
} FOVtype;

extern FOVtype g_CurrentFOVType;
extern FOVtype g_CurrentFOV;