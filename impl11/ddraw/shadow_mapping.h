#pragma once

#include "Matrices.h"
#include <vector>
#include "../shaders/shader_common.h"

#define SHADOW_MAP_SIZE 1024

class ShadowMappingData {
public:
	bool bEnabled;
	bool bAnisotropicMapScale;
	bool bAllLightsTagged;
	bool bMultipleSuns;
	bool bUseShadowOBJ; // This should be set to true when the Shadow OBJ is loaded
	bool bOBJrange_override;
	float fOBJrange_override_value;
	int ShadowMapSize;
	D3D11_VIEWPORT ViewPort;
	int NumVertices, NumIndices; // This should be set when the Shadow OBJ is loaded
	float black_level;
	float POV_XY_FACTOR;
	float POV_Z_FACTOR;
	float FOVDistScale;
	float sw_pcf_bias;
	float hw_pcf_bias;
	//float XWA_LIGHT_Y_CONV_SCALE;
	float shadow_map_mult_x;
	float shadow_map_mult_y;
	float shadow_map_mult_z;

	int DepthBias;
	float DepthBiasClamp;
	float SlopeScaledDepthBias;

	ShadowMappingData() {
		this->bEnabled = false;
		this->bAnisotropicMapScale = true;
		this->bAllLightsTagged = false;
		this->bMultipleSuns = false;
		this->bUseShadowOBJ = false;
		this->NumVertices = 0;
		this->NumIndices = 0;
		this->ShadowMapSize = SHADOW_MAP_SIZE;
		// Initialize the Viewport
		this->ViewPort.TopLeftX = 0.0f;
		this->ViewPort.TopLeftY = 0.0f;
		this->ViewPort.Height = (float)this->ShadowMapSize;
		this->ViewPort.Width = (float)this->ShadowMapSize;
		this->ViewPort.MinDepth = D3D11_MIN_DEPTH;
		this->ViewPort.MaxDepth = D3D11_MAX_DEPTH;
		this->black_level = 0.2f;
		this->POV_XY_FACTOR = 24.974f;
		this->POV_Z_FACTOR = 25.0f;
		this->bAnisotropicMapScale = true;
		this->bOBJrange_override = false;
		this->fOBJrange_override_value = 5.0f;
		//this->FOVDistScale = 624.525f;
		this->FOVDistScale = 620.0f; // This one seems a bit better
		this->sw_pcf_bias = -0.03f;
		this->hw_pcf_bias = -0.03f;
		// The following scale factor is used when tagging lights (associating XWA lights
		// with sun textures). I don't have a good explanation for this value; but
		// it's used to compensate the Y coordinate so that the light and the centroid of
		// the sun texture line up better. I'll investigate this in detail later.
		//this->XWA_LIGHT_Y_CONV_SCALE = -62.5f;

		this->shadow_map_mult_x = 1.0f;
		this->shadow_map_mult_y = 1.0f;
		this->shadow_map_mult_z = -1.0f;

		this->DepthBias = 0;
		this->DepthBiasClamp = 0.0f;
		this->SlopeScaledDepthBias = 0.0f;
	}

	void SetSize(int Width, int Height) {
		this->ShadowMapSize = Width;
		this->ViewPort.Width = (float)ShadowMapSize;
		this->ViewPort.Height = (float)ShadowMapSize;
	}
};

// For shadow mapping, maybe we'd like to distinguish between sun-lights and
// planet/background-lights. We'll use this struct to tag lights and fade
// those lights which aren't suns
class XWALightInfo {
public:
	bool bTagged, bIsSun;

public:
	XWALightInfo() {
		this->Reset();
	}

	void Reset() {
		this->bTagged = false;
		this->bIsSun = true;
	}
};

extern ShadowMappingData g_ShadowMapping;
extern bool g_bShadowMapEnable, g_bShadowMapDebug, g_bShadowMappingInvertCameraMatrix, g_bShadowMapEnablePCSS;
extern float g_fShadowMapScale, g_fShadowMapAngleX, g_fShadowMapAngleY, g_fShadowMapDepthTrans;
extern float SHADOW_OBJ_SCALE, SHADOW_OBJ_SCALE_Y, SHADOW_OBJ_SCALE_Z;
extern std::vector<Vector4> g_OBJLimits;
bool g_bShadowMapHardwarePCF = false;
extern XWALightInfo g_XWALightInfo[MAX_XWA_LIGHTS];
extern Vector3 g_SunCentroids[MAX_XWA_LIGHTS];
extern Vector2 g_SunCentroids2D[MAX_XWA_LIGHTS];
extern int g_iNumSunCentroids;
