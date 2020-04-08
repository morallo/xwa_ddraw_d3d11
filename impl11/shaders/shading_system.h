#include "material_defs.h"

// PSShadingSystemCB
cbuffer ConstantBuffer : register(b4)
{
	float3 MainLight;
	uint LightCount;
	// 16 bytes
	float4 MainColor;
	// 32 bytes
	float4 LightVector[MAX_XWA_LIGHTS];
	// 32+128 = 160 bytes
	float4 LightColor[MAX_XWA_LIGHTS];
	// 160+128 = 288 bytes
	float global_spec_intensity, global_glossiness, global_spec_bloom_intensity, global_bloom_glossiness_mult;
	// 304 bytes
	float saturation_boost, lightness_boost, ssdo_enabled;
	uint ss_debug;
	// 320 bytes
	float sso_disable, sqr_attenuation, laser_light_intensity;
	uint num_lasers;
	// 336 bytes
	float4 LightPoint[MAX_CB_POINT_LIGHTS];
	// 8 * 16 = 128
	// 464 bytes
	float4 LightPointColor[MAX_CB_POINT_LIGHTS];
	// 8 * 16 = 128
	// 592 bytes
	float ambient, ss_unused0, ss_unused1, ss_unused2;
	// 608 bytes
};
