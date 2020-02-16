#include "material_defs.h"

// PSShadingSystemCB
cbuffer ConstantBuffer : register(b4)
{
	float4 LightVector;
	// 16 bytes
	float4 LightVector2;
	// 32 bytes
	float4 LightColor;
	// 48 bytes
	float4 LightColor2;
	// 64 bytes
	float spec_intensity, glossiness, spec_bloom_intensity, bloom_glossiness_mult;
	// 80 bytes
	float saturation_boost, lightness_boost, ssdo_enabled;
	uint ss_debug;
	// 96 bytes
};
